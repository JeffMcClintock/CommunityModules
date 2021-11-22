#include "ControllerWrapper.h"
#include "unicode_conversion.h"
#include "myPluginProvider.h"
#include "./MyViewStream.h"

#if !defined(SE_TARGET_WAVES)
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#endif
#include "pluginterfaces\base\ibstream.h"

using namespace gmpi;
using namespace Steinberg;
using namespace Steinberg::Vst;

class VstComponentHandler : public Steinberg::FObject, public Steinberg::Vst::IComponentHandler
{
public:
	class ControllerWrapper* controller_;

	// IComponentHandler
	Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID id) override;
	Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) override;
	Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID id) override;
	Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override;

	//---Interface---------
	OBJ_METHODS(VstComponentHandler, Steinberg::FObject)
		DEFINE_INTERFACES
		DEF_INTERFACE(Steinberg::Vst::IComponentHandler)
		END_DEFINE_INTERFACES(Steinberg::FObject)
		REFCOUNT_METHODS(Steinberg::FObject)
};

ControllerWrapper::ControllerWrapper(const wchar_t* filename, const std::string& uuid) :
 filename_(filename)
, shellPluginId_(uuid)
, handle_(0)
{
	componentHandler = std::make_unique<VstComponentHandler>();
	componentHandler->controller_ = this;
	plugin = std::make_unique<myPluginProvider>();
}

ControllerWrapper::~ControllerWrapper()
{
#if 0 // wv
    if (processor_component_ptr && processor_vstEffect__ptr)
    {
		// ensure the processor don't try to access the plugin.
		*processor_component_ptr = nullptr;
		*processor_vstEffect__ptr = nullptr;
    }
#endif
	if (windowController)
	{
		windowController->destroyView();
	}

	plugin->terminatePlugin();
}

int32_t ControllerWrapper::setParameter(int32_t parameterHandle, int32_t fieldId, int32_t /*voice*/, const void* data, int32_t size)
{
	// Avoid altering plugin state until we can determin if we are restoring a saved preset, or keeping init preset.
	if(!isOpen)
		return MP_OK;

	if(!inhibitFeedback && fieldId == gmpi::MP_FT_VALUE)
	{
		int32_t moduleHandle = -1;
		int32_t moduleParameterId = -1;
		host_->getParameterModuleAndParamId(parameterHandle, &moduleHandle, &moduleParameterId);

		if(!plugin->controller || !plugin->component)
		{
			return MP_FAIL;
		}

		const auto paramId = 0;
		if(paramId != moduleParameterId)
		{
			return MP_OK;
		}

		if((size_t)(size) < sizeof(int32_t)) // Size of zero implies first-time init, no preset stored yet.
		{
			isSynthEditPresetEmpty = true;
            return MP_OK;
		}
		else
		{
#if 0//defined (_DEBUG) & defined(_WIN32)
			auto effectName = ae->getName();

			_RPT0(_CRT_WARN, "{ ");
			auto d = (unsigned char*)data;
			for(int i = 0; i < 12; ++i)
			for(int i = 0; i < 12; ++i)
			{
				_RPT1(_CRT_WARN, "%02x ", (int)d[i]);
			}
			_RPT2(_CRT_WARN, "}; set: %s H %d\n", effectName.c_str(), moduleHandle);
#endif
			// Load preset chunk.
			const auto controllerStateSize = *((int32_t*)data);
			const auto controllaDataPtr = ((uint8_t*)data) + sizeof(int32_t);
			MyViewStream s(controllaDataPtr, controllerStateSize);
			plugin->controller->setState(&s);

			const auto streamPos = sizeof(int32_t) + controllerStateSize;
			const auto processorStateSize = size - streamPos;
			const auto processorDataPtr = ((uint8_t*)data) + streamPos;
			MyViewStream s2(processorDataPtr, static_cast<int32_t>(processorStateSize));
			plugin->component->setState(&s2);

			MyViewStream s3(processorDataPtr, static_cast<int32_t>(processorStateSize));
			plugin->controller->setComponentState(&s3);
		}
	}

	return MP_OK;
}

int32_t ControllerWrapper::preSaveState()
{
	inhibitFeedback = true;

	if(!plugin->controller || !plugin->component)
	{
		return MP_FAIL;
	}
	{
		// get controller state. usually blank.
		MyBufferStream stream;
		int32_t controllerStateSize = {};
		stream.write(&controllerStateSize, sizeof(controllerStateSize));
		plugin->controller->getState(&stream);

		// update size of data written so far.
		*((int32_t*)stream.buffer_.data()) = static_cast<int32_t>(stream.buffer_.size() - sizeof(int32_t));

		// get processor state.
		plugin->component->getState(&stream);

#if 0 //defined (_DEBUG) & defined(_WIN32)
		_RPT0(_CRT_WARN, "{ ");
		auto d = (unsigned char*)chunkPtr;
		for (int i = 0; i < 12; ++i)
		{
			_RPT1(_CRT_WARN, "%02x ", (int)d[i]);
		}
		_RPT0(_CRT_WARN, "}; get\n");
#endif

#if 0 // ifdef _DEBUG
		// Waves Grand Rhapsody needs to load child plugin preset from the ALG,
		// print preset out in handy format to be pasted into ALG source code. (prints to debug window).
		{
			_RPT0(_CRT_WARN, "{ ");
			const unsigned char* d = (const unsigned char*)chunkPtr;
			for (int i = 0 ; i < chunkSize; ++i)
			{
				if ((i % 20) == 0)
				{
					_RPT0(_CRT_WARN, "\n");
				}
				_RPT1(_CRT_WARN, "0x%02x, ", d[i]);
			}
			_RPT0(_CRT_WARN, "};\n");
		}
#endif

		const int voiceId = 0;
		host_->setParameter(
			host_->getParameterHandle(handle_, chunkParamId),
			MP_FT_VALUE,
			voiceId,
			(char*)stream.buffer_.data(),
			(int32_t) stream.buffer_.size()
		);
	}

	stateDirty = false;
	inhibitFeedback = false;

	return MP_OK;
}

int32_t ControllerWrapper::open()
{
	isOpen = true;

	if (!plugin->controller) // VST not installed?
	{
		return MP_OK;
	}

	plugin->controller->setComponentHandler(componentHandler.get());

	// Pass pointer to 'this' to Process and GUI.
	const int controllerPtrParamId = chunkParamId + 1;
	const int voiceId = 0;
	const auto me = static_cast<IVST3PluginOwner*>(this);
	host_->setParameter(host_->getParameterHandle(handle_, controllerPtrParamId), MP_FT_VALUE, voiceId, &me, sizeof(me));

	{
		// always have to pass initial state from processor to controller.
		{
			assert(plugin->controller && plugin->component);

			// get processor state.
			MyBufferStream stream;
			plugin->component->getState(&stream);

			// pass to controller
			MyViewStream s3(stream.buffer_.data(), static_cast<int32_t>(stream.buffer_.size()));
			plugin->controller->setComponentState(&s3);
		}

		// Test if host has a valid chunk preset.
		isSynthEditPresetEmpty = false;
		host_->updateParameter(host_->getParameterHandle(handle_, chunkParamId), MP_FT_VALUE, voiceId);

		// In the case we have no preset stored yet (because user *just* inserted plugin), Copy init preset to SE patch memory.
		if (isSynthEditPresetEmpty)
		{
			preSaveState();
		}
	}

	return MP_OK;
}

int32_t ControllerWrapper::setHost(gmpi::IMpUnknown* host)
{
	host->queryInterface(MP_IID_CONTROLLER_HOST, reinterpret_cast<void **>( &host_ ));

	if( !host_ )
	{
		return MP_NOSUPPORT; //  host Interfaces not supported
	}

	host_->getHandle(handle_);

	LoadPlugin( JmUnicodeConversions::WStringToUtf8(filename_), shellPluginId_);

	if(plugin && plugin->component)
	{
		Steinberg::Vst::IAudioProcessor* vstEffect = {};
		plugin->component->queryInterface(IAudioProcessor::iid, (void**)&vstEffect);
		if (vstEffect)
		{
			host_->setLatency(vstEffect->getLatencySamples()); // this should be supported on Waves.
			vstEffect->release();
		}
	}
	return plugin->controller != nullptr ? MP_OK : MP_FAIL;
}

int ControllerWrapper::LoadPlugin(std::string path, std::string uuid)
{
	std::string error;
    dll = VST3::Hosting::Module::create(path, error);

	if(!dll)
	{
		// Could not create Module for file
        _RPT1(0, "Failed to load VST3 child plugin. UUID:%s\n", uuid.c_str());
        return gmpi::MP_FAIL;
	}

	const auto classID = VST3::UID::fromString(uuid);
	if(!classID)
	{
        return gmpi::MP_FAIL;
	}

	auto factory = dll->getFactory();
	plugin->setup(factory, *classID);

    return gmpi::MP_OK;
}

void ControllerWrapper::OpenGui()
{
	if (!plugin->controller) // VST not installed?
	{
		return;
	}

	auto view = owned (plugin->controller->createView(Steinberg::Vst::ViewType::kEditor));
	if (!view)
	{
		// EditController does not provide its own editor
		return;
	}

	ViewRect plugViewSize {};
	auto result = view->getSize (&plugViewSize);
	if (result != kResultTrue)
	{
		// Could not get editor view size
		return;
	}

	{
		HDC hdc = ::GetDC(0);
		int lx = GetDeviceCaps(hdc, LOGPIXELSX);
		int ly = GetDeviceCaps(hdc, LOGPIXELSY);
		::ReleaseDC(0, hdc);

		plugViewSize.right = (plugViewSize.right * lx) / 96;
		plugViewSize.bottom = (plugViewSize.bottom * ly) / 96;
	}

	const auto viewRect = ViewRectToRect (plugViewSize);

	windowController = std::make_shared<WindowController> (view);
	auto window = IPlatform::instance ().createWindow (
	    "Editor", viewRect.size, view->canResize () == kResultTrue, windowController);

	if (!window)
	{
		// Could not create window
		return;
	}

	window->show ();
}

bool ControllerWrapper::OnTimer()
{
	if( stateDirty )
	{
		preSaveState();
		
		stateDirty = false;
	}
	return false;
}

tresult VstComponentHandler::beginEdit(ParamID paramId)
{
	const bool value = true;
	const int voiceId = 0;

	controller_->host_->setParameter(
		controller_->host_->getParameterHandle(controller_->handle_, paramId),
		MP_FT_GRAB,
		voiceId,
		(const void*)&value,
		(int32_t) sizeof(value)
	);

	return kResultOk;
}

tresult VstComponentHandler::performEdit (ParamID paramId, ParamValue valueNormalized)
{
	controller_->stateDirty = true;

	// Sync VST param -> SE Param
	const float valueNormalizedF = static_cast<float>(valueNormalized);
	const int voiceId = 0;
	controller_->host_->setParameter(
		controller_->host_->getParameterHandle(controller_->handle_, paramId),
		MP_FT_NORMALIZED,
		voiceId,
		(const void*)&valueNormalizedF,
		(int32_t) sizeof(valueNormalizedF)
	);

	return kResultOk;
}

tresult VstComponentHandler::endEdit (ParamID paramId)
{
	const bool value = true;
	const int voiceId = 0;

	controller_->host_->setParameter(
		controller_->host_->getParameterHandle(controller_->handle_, paramId),
		MP_FT_GRAB,
		voiceId,
		(const void*)&value,
		(int32_t) sizeof(value)
	);

	return kResultOk;
}

tresult VstComponentHandler::restartComponent (int32 /*flags*/)
{
	// TODO!
	return kResultOk;
}


int32_t ControllerWrapper::registerProcessor(Steinberg::Vst::IComponent** component, Steinberg::Vst::IAudioProcessor** vstEffect)
{
	processor_component_ptr = component;
	processor_vstEffect__ptr = vstEffect;

	if (processor_component_ptr && processor_vstEffect__ptr)
    {
        *component = plugin->component.get();
        (*component)->queryInterface(IAudioProcessor::iid, (void**)vstEffect);

        // unusual. processor don't hold references on the plugin. This is becuase it's lifetime must be controlled exclusivly from the ALG.
        if (*vstEffect)
        {
            (*vstEffect)->release();
        }
    }

	return gmpi::MP_OK;
}

