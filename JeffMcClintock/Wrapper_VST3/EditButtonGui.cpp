#include "./EditButtonGui.h"
#include "../shared/unicode_conversion.h"
#include "ControllerWrapper.h"
#include "C:\SE\SE15\SDKs\VST3_SDK\pluginterfaces\gui\iplugview.h"
#include "C:\SE\SE15\SDKs\VST3_SDK\public.sdk\samples\vst-hosting\editorhost\source\platform\iwindow.h"
#include "C:\SE\SE15\SDKs\VST3_SDK\public.sdk\samples\vst-hosting\editorhost\source\platform\iplatform.h"
#include "C:\SE\SE15\SDKs\VST3_SDK\pluginterfaces\gui\iplugviewcontentscalesupport.h"

using namespace gmpi;
using namespace JmUnicodeConversions;
using namespace gmpi_gui_api;

using namespace Steinberg;
using namespace Steinberg::Vst::EditorHost;

inline bool operator== (const ViewRect& r1, const ViewRect& r2)
{
	return memcmp (&r1, &r2, sizeof (ViewRect)) == 0;
}

inline bool operator!= (const ViewRect& r1, const ViewRect& r2)
{
	return !(r1 == r2);
}

class WindowController : public IWindowController, public IPlugFrame
{
public:
	WindowController (const IPtr<IPlugView>& plugView);
	~WindowController () noexcept override;

	void onShow (IWindow& w) override;
	void onClose (IWindow& w) override;
	void onResize (IWindow& w, Size newSize) override;
	Size constrainSize (IWindow& w, Size requestedSize) override;
	void onContentScaleFactorChanged (IWindow& window, float newScaleFactor) override;

	// IPlugFrame
	tresult PLUGIN_API resizeView (IPlugView* view, ViewRect* newSize) override;

	void closePlugView ();

private:
	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
	{
		if (FUnknownPrivate::iidEqual (_iid, IPlugFrame::iid) ||
		    FUnknownPrivate::iidEqual (_iid, FUnknown::iid))
		{
			*obj = this;
			addRef ();
			return kResultTrue;
		}
		if (window)
			return window->queryInterface (_iid, obj);
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () override { return 1000; }
	uint32 PLUGIN_API release () override { return 1000; }

	IPtr<IPlugView> plugView;
	IWindow* window {nullptr};
	bool resizeViewRecursionGard {false};
};


EditButtonGui::EditButtonGui() :
 initialized_(false)
{
}

int32_t EditButtonGui::initialize()
{
	initialized_ = true;

	auto r = MpGuiBase2::initialize();

#if 0
	if (vstEffect_ != nullptr && hasGuiParameterPins_)
	{
		int pinIdx = 1;
		int paramCount = vstEffect_->getNumParams();
		for (int i = 0; i < paramCount; ++i)
		{
			float v = vstEffect_->getParameter(i);

			if (pinParamValues[i] != v) // avoid feedback loops due to precision issue in plugin.
			{
				pinParamValues[i] = v;
				getHost()->pinTransmit(pinIdx, sizeof(v), &v);
			}
			++pinIdx;

			auto ws = Utf8ToWstring(vstEffect_->getParameterDisplay(i));
			getHost()->pinTransmit(pinIdx, static_cast<int32_t>(ws.size() * sizeof(wchar_t)), ws.data());
			++pinIdx;
		}
	}
#endif
	return r;
}

int32_t EditButtonGui::setPin(int32_t pinId, int32_t voice, int32_t size, const void* data)
{
	if (controllertPtrPinId == pinId)
	{
		if (size == sizeof(void*))
		{
			pluginProvider_ = *(Steinberg::Vst::PlugProvider**)data;
		}
	}

	return GuiPinOwner::setPin2(pinId, voice, size, data);
}

int32_t EditButtonGui::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
	// Let host handle right-clicks.
	if( ( flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON ) == 0 )
	{
		return gmpi::MP_UNHANDLED;
	}

	setCapture();
	invalidateRect();

	return gmpi::MP_OK;
}

int32_t EditButtonGui::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
	if( !getCapture() )
	{
		return gmpi::MP_UNHANDLED;
	}

	releaseCapture();
	invalidateRect();

	openVstGui();

	return gmpi::MP_OK;
}

int32_t EditButtonGui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	GmpiDrawing::Graphics g(drawingContext);

	auto r = getRect();

	// Background Fill.
	const uint32_t backgroundColor = getCapture() ? 0xE8E8Eff : 0xE8E8E8u;
	auto brush = g.CreateSolidColorBrush(backgroundColor);
	g.FillRectangle(r, brush);

	// Outline.
	brush.SetColor(0x969696u);
	g.DrawRectangle(r, brush);

	// Current selection text.
	brush.SetColor(GmpiDrawing::Color(0x000032u));

	std::string txt = "EDIT";

	if (!pluginProvider_)
	{
		txt = "LOADFAIL";
		brush.SetColor(GmpiDrawing::Color::Red);
	}

	GmpiDrawing::Rect textRect(r);
	textRect.Deflate(border);

	g.DrawTextU(txt, getTextFormat(), textRect, brush);

	return gmpi::MP_OK;
}

GmpiDrawing::TextFormat& EditButtonGui::getTextFormat()
{
	if( dtextFormat.isNull() )
	{
		const float fontSize = 14;
		dtextFormat = GetGraphicsFactory().CreateTextFormat(fontSize);
		dtextFormat.SetTextAlignment(GmpiDrawing::TextAlignment::Leading); // Left
		dtextFormat.SetParagraphAlignment(GmpiDrawing::ParagraphAlignment::Center);
	}

	return dtextFormat;
}

int32_t EditButtonGui::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	auto font = getTextFormat();

	auto s = getTextFormat().GetTextExtentU("XXXEDITXXX");

	returnDesiredSize->width = s.width;
	returnDesiredSize->height = s.height;

	return gmpi::MP_OK;
}

int32_t EditButtonGui::populateContextMenu(float x, float y, gmpi::IMpUnknown* contextMenuItemsSink)
{
	gmpi::IMpContextItemSink* sink;
	contextMenuItemsSink->queryInterface(gmpi::MP_IID_CONTEXT_ITEMS_SINK, ( void**) &sink );
	std::string info("WavesShell: ");
//	info += WStringToUtf8(GetVstFactory()->getWavesShellLocation());

	sink->AddItem(info.c_str(), 0 );

	{
		char buffer[50] = "";
		sprintf(buffer, "%s", shellPluginId_.c_str());

		std::string info2("Shell ID: ");
		info2 += buffer;
		sink->AddItem(info2.c_str(), 1);
	}
	return gmpi::MP_OK;
}

int32_t EditButtonGui::onContextMenu(int32_t selection)
{
	return gmpi::MP_OK;
}

void EditButtonGui::openVstGui()
{
	auto nativeController = owned(pluginProvider_->getController());

	if (!nativeController) // VST not installed?
	{
		return;
	}

	auto view = owned (nativeController->createView(Steinberg::Vst::ViewType::kEditor));
	if (!view)
	{
//		IPlatform::instance ().kill (-1, "EditController does not provide its own editor");
		return;
	}

	ViewRect plugViewSize {};
	auto result = view->getSize (&plugViewSize);
	if (result != kResultTrue)
	{
//		IPlatform::instance ().kill (-1, "Could not get editor view size");
		return;
	}

	auto viewRect = ViewRectToRect (plugViewSize);

	windowController = std::make_shared<WindowController> (view);
	auto window = IPlatform::instance ().createWindow (
	    "Editor", viewRect.size, view->canResize () == kResultTrue, windowController);
	if (!window)
	{
//		IPlatform::instance ().kill (-1, "Could not create window");
		return;
	}

	window->show ();
}


//------------------------------------------------------------------------
WindowController::WindowController (const IPtr<IPlugView>& plugView) : plugView (plugView)
{
}

//------------------------------------------------------------------------
WindowController::~WindowController () noexcept
{
}

//------------------------------------------------------------------------
void WindowController::onShow (IWindow& w)
{
	SMTG_DBPRT1 ("onShow called (%p)\n", (void*)&w);

	window = &w;
	if (!plugView)
		return;

	auto platformWindow = window->getNativePlatformWindow ();
	if (plugView->isPlatformTypeSupported (platformWindow.type) != kResultTrue)
	{
		IPlatform::instance ().kill (-1, std::string ("PlugView does not support platform type:") +
		                                     platformWindow.type);
	}

	plugView->setFrame (this);

	if (plugView->attached (platformWindow.ptr, platformWindow.type) != kResultTrue)
	{
		IPlatform::instance ().kill (-1, "Attaching PlugView failed");
	}
}

//------------------------------------------------------------------------
void WindowController::closePlugView ()
{
	if (plugView)
	{
		plugView->setFrame (nullptr);
		if (plugView->removed () != kResultTrue)
		{
			IPlatform::instance ().kill (-1, "Removing PlugView failed");
		}
		plugView = nullptr;
	}
	window = nullptr;
}

//------------------------------------------------------------------------
void WindowController::onClose (IWindow& w)
{
	SMTG_DBPRT1 ("onClose called (%p)\n", (void*)&w);

	closePlugView ();

	// TODO maybe quit only when the last window is closed
	IPlatform::instance ().quit ();
}

//------------------------------------------------------------------------
void WindowController::onResize (IWindow& w, Size newSize)
{
	SMTG_DBPRT1 ("onResize called (%p)\n", (void*)&w);

	if (plugView)
	{
		ViewRect r {};
		r.right = newSize.width;
		r.bottom = newSize.height;
		ViewRect r2 {};
		if (plugView->getSize (&r2) == kResultTrue && r != r2)
			plugView->onSize (&r);
	}
}

//------------------------------------------------------------------------
Size WindowController::constrainSize (IWindow& w, Size requestedSize)
{
	SMTG_DBPRT1 ("constrainSize called (%p)\n", (void*)&w);

	ViewRect r {};
	r.right = requestedSize.width;
	r.bottom = requestedSize.height;
	if (plugView && plugView->checkSizeConstraint (&r) != kResultTrue)
	{
		plugView->getSize (&r);
	}
	requestedSize.width = r.right - r.left;
	requestedSize.height = r.bottom - r.top;
	return requestedSize;
}

//------------------------------------------------------------------------
void WindowController::onContentScaleFactorChanged (IWindow& w, float newScaleFactor)
{
	SMTG_DBPRT1 ("onContentScaleFactorChanged called (%p)\n", (void*)&w);

	FUnknownPtr<IPlugViewContentScaleSupport> css (plugView);
	if (css)
	{
		css->setContentScaleFactor (newScaleFactor);
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API WindowController::resizeView (IPlugView* view, ViewRect* newSize)
{
	SMTG_DBPRT1 ("resizeView called (%p)\n", (void*)view);

	if (newSize == nullptr || view == nullptr || view != plugView)
		return kInvalidArgument;
	if (!window)
		return kInternalError;
	if (resizeViewRecursionGard)
		return kResultFalse;
	ViewRect r;
	if (plugView->getSize (&r) != kResultTrue)
		return kInternalError;
	if (r == *newSize)
		return kResultTrue;

	resizeViewRecursionGard = true;
	Size size {newSize->right - newSize->left, newSize->bottom - newSize->top};
	window->resize (size);
	resizeViewRecursionGard = false;
	if (plugView->getSize (&r) != kResultTrue)
		return kInternalError;
	if (r != *newSize)
		plugView->onSize (newSize);
	return kResultTrue;
}