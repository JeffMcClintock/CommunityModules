#include "./Vst2WrapperGui.h"
#include "../shared/unicode_conversion.h"
#include "it_enum_list.h"

using namespace gmpi;
using namespace JmUnicodeConversions;
using namespace GmpiDrawing;
using namespace gmpi_gui_api;

Vst2WrapperGui::Vst2WrapperGui(VstIntPtr shellPluginId, bool hasGuiParameterPins) :
shellPluginId_(shellPluginId)
, paramCount(-1)
, initialized_(false)
,hasGuiParameterPins_(hasGuiParameterPins)
, vstEffect_(nullptr)
{
}

int32_t Vst2WrapperGui::setPin(int32_t pinId, int32_t voice, int32_t size, const void* data)
{
	if (aeffectPtrPinId == pinId)
	{
		if (size == sizeof(vstEffect_))
		{
			vstEffect_ = *(AEffectWrapper**)data;

			if (vstEffect_ != nullptr)
			{
				paramCount = vstEffect_->getNumParams();

				if (hasGuiParameterPins_)
				{
					pinParamValues.assign(paramCount, 2.0f); // unlikely initial value to trigger change.
				}
			}
		}
	}

	if (hasGuiParameterPins_ && pinId > 0)
	{
		pinId--; // ignore effect ptr pin.

		if (pinId < paramCount * 2)
		{
			// Output pin update.
			if ((pinId & 0x01) == 0)
			{
				auto paramId = pinId >> 1;

				// Which input pin represents that param.
				int inputPinId = 1 + paramCount * 2 + paramId;
				getHost()->pinTransmit(inputPinId, size, data);
			}
		}
		else
		{
			if (initialized_) // prevent spurious messages on open.
			{
				if (vstEffect_ != nullptr)
				{
					// Param update from host on input pin. Relay to output pin.
					int paramId = pinId - paramCount * 2;
					int outputPinId = 1 + paramId * 2;
					getHost()->pinTransmit(outputPinId, size, data);

					// Update textpin.
					auto ws = Utf8ToWstring(vstEffect_->getParameterDisplay(paramId));
					getHost()->pinTransmit(outputPinId + 1, static_cast<int32_t>(ws.size() * sizeof(wchar_t)), ws.data());
				}
			}
		}
	}

	return GuiPinOwner::setPin2(pinId, voice, size, data);
}

int32_t Vst2WrapperGui::initialize()
{
	initialized_ = true;

	auto r = MpGuiBase2::initialize();

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

	return r;
}

int32_t Vst2WrapperGui::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
	// Let host handle right-clicks.
	if( ( flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON ) == 0 )
	{
		return gmpi::MP_UNHANDLED;
	}

	setCapture();
	return gmpi::MP_OK;
}

int32_t Vst2WrapperGui::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
	if( !getCapture() )
	{
		return gmpi::MP_UNHANDLED;
	}

	releaseCapture();

	openVstGui();

	return gmpi::MP_OK;
}

int32_t Vst2WrapperGui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	Graphics dc(drawingContext);

	auto r = getRect();

	// Background Fill.
	auto brush = dc.CreateSolidColorBrush(0xE8E8E8u);
	dc.FillRectangle(r, brush);

	// Outline.
	{
		const float penWidth = 1;

		auto geometry = GetGraphicsFactory().CreatePathGeometry();
		auto sink = geometry.Open();

        GmpiDrawing::Point p(r.top, r.left);

		sink.BeginFigure(p);

		sink.AddLine(GmpiDrawing::Point(r.left, r.top));
		sink.AddLine(GmpiDrawing::Point(r.right - 1, r.top));
		sink.AddLine(GmpiDrawing::Point(r.right - 1, r.bottom - 1));
		sink.AddLine(GmpiDrawing::Point(r.left, r.bottom - 1));

		sink.EndFigure();
		sink.Close();

		brush.SetColor(0x969696u);

		dc.DrawGeometry(geometry, brush, penWidth);
	}

	// Current selection text.
	brush.SetColor(Color(0x000032u));

	std::string txt = "EDIT";

	if (vstEffect_ == nullptr)
	{
		txt = "LOADFAIL";
		brush.SetColor(Color::Red);
	}

	GmpiDrawing::Rect textRect(r);
	textRect.bottom -= border;
	textRect.top += border;
	textRect.left += border;
	textRect.right -= border;

	dc.DrawTextU(txt, getTextFormat(), textRect, brush);

	return gmpi::MP_OK;
}

TextFormat& Vst2WrapperGui::getTextFormat()
{
	if( dtextFormat.isNull() )
	{
		const float fontSize = 14;
		dtextFormat = GetGraphicsFactory().CreateTextFormat(fontSize);
		dtextFormat.SetTextAlignment(TextAlignment::Leading); // Left
		dtextFormat.SetParagraphAlignment(ParagraphAlignment::Center);
	}

	return dtextFormat;
}

int32_t Vst2WrapperGui::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	auto font = getTextFormat();

	auto s = getTextFormat().GetTextExtentU("XXXEDITXXX");

	returnDesiredSize->width = s.width;
	returnDesiredSize->height = s.height;

	return gmpi::MP_OK;
}

int32_t Vst2WrapperGui::populateContextMenu(float x, float y, gmpi::IMpUnknown* contextMenuItemsSink)
{
	gmpi::IMpContextItemSink* sink;
	contextMenuItemsSink->queryInterface(gmpi::MP_IID_CONTEXT_ITEMS_SINK, ( void**) &sink );
	std::string info("Shell: ");
//	info += WStringToUtf8(GetVstFactory()->getShellLocation());

	sink->AddItem(info.c_str(), 0 );

	{
		char buffer[50] = "";
		sprintf(buffer, "%X", (int) shellPluginId_);

		std::string info2("Shell ID: ");
		info2 += buffer;
		sink->AddItem(info2.c_str(), 1);
	}
	return gmpi::MP_OK;
}

int32_t Vst2WrapperGui::onContextMenu(int32_t selection)
{
	return gmpi::MP_OK;
}

void Vst2WrapperGui::openVstGui()
{
	if(vstEffect_ != nullptr )
	{
		vstEffect_->OpenEditor();
	}
}
