#include "ControllerWrapper.h"
#include "unicode_conversion.h"
#if !defined(SE_TARGET_WAVES)
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#endif
#include "pluginterfaces\base\ibstream.h"

using namespace gmpi;
using namespace Steinberg;
using namespace Steinberg::Vst;

// for writting.
class MyBufferStream : public Steinberg::FObject, public IBStream
{
public:
	MyBufferStream() {}
	virtual ~MyBufferStream() {}

	//---from IBStream------------------
	tresult PLUGIN_API read (void* buffer, int32 numBytes, int32* numBytesRead = nullptr) SMTG_OVERRIDE
	{
		return 0;
	}
	tresult PLUGIN_API write(void* buffer, int32 numBytes, int32* numBytesWritten = nullptr) SMTG_OVERRIDE
	{
		if(numBytesWritten)
		{
			*numBytesWritten = numBytes;
		}

		writePos_ += numBytes;
		buffer_.insert(buffer_.end(), (uint8_t*)buffer, ((uint8_t*)buffer) + numBytes);
		return 0;
	}
	tresult PLUGIN_API seek(int64 pos, int32 mode, int64* result = nullptr) SMTG_OVERRIDE
	{
		return 0;
	}
	tresult PLUGIN_API tell(int64* pos) SMTG_OVERRIDE
	{
		return 0;
	}

	std::vector<uint8_t> buffer_;
	int writePos_ = {};

	//---Interface---------
	OBJ_METHODS (MyBufferStream, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (IBStream)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

// for reading.
class MyViewStream : public Steinberg::FObject, public IBStream
{
public:
	MyViewStream(uint8_t* buffer, int32_t size) : buffer_(buffer), size_(size) {}
	virtual ~MyViewStream() {}

	//---from IBStream------------------
	tresult PLUGIN_API read (void* buffer, int32 numBytes, int32* numBytesRead = nullptr) SMTG_OVERRIDE
	{
		const auto remaining = size_ - readPos_;
		numBytes = (std::min)((int)numBytes, remaining);
		if(numBytesRead)
		{
			*numBytesRead = numBytes;
		}
		memcpy(buffer, buffer_ + readPos_, numBytes);
		readPos_ += numBytes;

		return 0;
	}
	tresult PLUGIN_API write(void* buffer, int32 numBytes, int32* numBytesWritten = nullptr) SMTG_OVERRIDE
	{
		return 0;
	}
	tresult PLUGIN_API seek(int64 pos, int32 mode, int64* result = nullptr) SMTG_OVERRIDE
	{
		switch(mode)
		{
		case kIBSeekSet:
			readPos_ = (std::min)((int)pos, size_);
			break;

		case kIBSeekCur:
			{
				const auto remaining = size_ - readPos_;
				pos = (std::min)((int)pos, remaining);
				readPos_ += pos;
			}
			break;

		case kIBSeekEnd:
			readPos_ = size_;
			break;

		default:
			return 1;
			break;
		}

		return 0;
	}

	tresult PLUGIN_API tell(int64* pos) SMTG_OVERRIDE
	{
		if(pos)
		{
			*pos = readPos_;
		}
		return 0;
	}

	const uint8_t* buffer_ = {};
	int readPos_ = {};
	int size_ = {};

	//---Interface---------
	OBJ_METHODS (MyBufferStream, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (IBStream)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

ControllerWrapper::ControllerWrapper(const wchar_t* filename, const std::string& uuid) :
handle_(0)
, filename_(filename)
, shellPluginId_(uuid)
, host_(0)
, stateDirty(false)
, inhibitFeedback(false)
, isOpen(false)
{
	componentHandler.controller_ = this;
}

int32_t ControllerWrapper::setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size)
{
	// Avoid altering plugin state until we can determin if we are restoring a saved preset, or keeping init preset.
	if(!isOpen)
		return MP_OK;

	if(!inhibitFeedback && fieldId == 0) // FT_VALUE
	{
		int32_t moduleHandle = -1;
		int32_t moduleParameterId = -1;
		host_->getParameterModuleAndParamId(parameterHandle, &moduleHandle, &moduleParameterId);

		auto controller = owned(pluginProvider_->getController());
		auto component = owned(pluginProvider_->getComponent());

		if(!controller || !component)
		{
			return MP_FAIL;
		}

		const auto chunkParamId = controller->getParameterCount(); // todo cache this.!!!
		if(chunkParamId != moduleParameterId)
		{
			return MP_OK;
		}

		if(size < sizeof(int32_t)) // Size of zero implies first-time init, no preset stored yet.
		{
			isSynthEditPresetEmpty = true;
		}
		else
		{
#if 0//defined (_DEBUG) & defined(_WIN32)
			auto effectName = ae->getName();

			_RPT0(_CRT_WARN, "{ ");
			auto d = (unsigned char*)data;
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
			controller->setState(&s);

			const auto streamPos = sizeof(int32_t) + controllerStateSize;
			const auto processorStateSize = size - streamPos;
			const auto processorDataPtr = ((uint8_t*)data) + streamPos;
			MyViewStream s2(processorDataPtr, static_cast<int32_t>(processorStateSize));
			component->setState(&s2);

			MyViewStream s3(processorDataPtr, static_cast<int32_t>(processorStateSize));
			controller->setComponentState(&s3);
		}
	}

	return MP_OK;
}

int32_t ControllerWrapper::preSaveState()
{
#if 1 // TODO!!!
	{
		inhibitFeedback = true;
		auto controller = owned(pluginProvider_->getController());
		auto component = owned(pluginProvider_->getComponent());

		if(!controller || !component)
		{
			return MP_FAIL;
		}
		{
			// get controller state. usually blank.
			MyBufferStream stream;
			int32_t controllerStateSize = {};
			stream.write(&controllerStateSize, sizeof(controllerStateSize));
			controller->getState(&stream);

			// update size of data written so far.
			*((int32_t*)stream.buffer_.data()) = static_cast<int32_t>(stream.buffer_.size() - sizeof(int32_t));

			// get processor state.
			component->getState(&stream);

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
			auto paramId = controller->getParameterCount();

			host_->setParameter(host_->getParameterHandle(handle_, paramId), MP_FT_VALUE, voiceId, (char*)stream.buffer_.data(), (int32_t) stream.buffer_.size());
			// _RPT1(_CRT_WARN, "ControllerWrapper:: Saved State: %d bytes\n", chunkSize);
		}

		stateDirty = false;
		inhibitFeedback = false;
	}
#endif
	return MP_OK;
}

int32_t ControllerWrapper::open()
{
#if 1 // TODO!!!
	isOpen = true;

	if (!pluginProvider_) // VST not installed?
	{
		return MP_OK;
	}

	auto component = owned(pluginProvider_->getComponent()); // getting component causes instansiation of entire plugin.
	auto controller = owned(pluginProvider_->getController());

	controller->setComponentHandler(&componentHandler);

	// Pass pointer to 'this' to Process and GUI.
	const int chunkParamId = controller->getParameterCount();
	const int controllerPtrParamId = chunkParamId + 1;
	const int voiceId = 0;
	const auto me = this;
	host_->setParameter(host_->getParameterHandle(handle_, controllerPtrParamId), MP_FT_VALUE, voiceId, &me, sizeof(me));

	{
		// always have to pass initial state from processor to controller.
		{
			assert(controller && component);

			// get processor state.
			MyBufferStream stream;
			component->getState(&stream);

			// pass to controller
			MyViewStream s3(stream.buffer_.data(), static_cast<int32_t>(stream.buffer_.size()));
			controller->setComponentState(&s3);
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
#endif
	return MP_OK;
}

int32_t ControllerWrapper::setHost(gmpi::IMpUnknown* host)
{
	host->queryInterface(MP_IID_CONTROLLER_HOST, reinterpret_cast<void **>( &host_ ));

	if( host_ == 0 )
	{
		return MP_NOSUPPORT; //  throw "host Interfaces not supported";
	}

	host_->getHandle(handle_);

	assert(!pluginProvider_); // don't call twice.

	/*auto ae =*/ LoadPlugin( JmUnicodeConversions::WStringToUtf8(filename_), shellPluginId_);

	if( pluginProvider_ == nullptr ) // VST not installed.
	{
		return MP_FAIL;
	}
/* TODO
	// Report latency to SE
	host_->setLatency(ae->getLatency());

	ae->registerObserver(this);
*/
	return MP_OK;
}

#if defined (SE_TARGET_WAVES)
AEffectWrapperWaves* ControllerWrapper::LoadVst2Plugin(intptr_t instance, const wvFM::WCStPath& filename, VstIntPtr shellPluginId)
#else
int ControllerWrapper::LoadPlugin(std::string path, std::string uuid)
#endif
{

#if defined (SE_TARGET_WAVES)
	AEffectWrapperWaves* pAEffectWrapper = 0;
#else
//	AEffectWrapper* pAEffectWrapper = 0;
#endif
	
//	assert(pluginInstance.get() == nullptr);

	{
#if defined (SE_TARGET_WAVES)
		pAEffectWrapper = new AEffectWrapperWaves();
		gmpi_dynamic_linking::DLL_HANDLE dllHandle = 0;

		bool bIsFirstPluginInstance = true;

		for (auto it = pluginDllHandles.begin(); it != pluginDllHandles.end(); ++it)
		{
			if ((*it).first.handle == handle)
			{
				bIsFirstPluginInstance = false;
				++(*it).first.refCounter;
				dllHandle = (*it).second;
				break;
			}
		}
#endif

#if !defined (_WAVES_PROCESS_TARGET ) && defined (SE_TARGET_WAVES)
		pAEffectWrapper->LoadDll(filename, dllHandle, shellPluginId);
#elif !defined (SE_TARGET_WAVES) // instead of all this shit, have a seperate class for waves vs SE
		//pAEffectWrapper = new AEffectWrapper();
		//pAEffectWrapper->LoadDll(filename, shellPluginId);
		{
			std::string error;
			dll = VST3::Hosting::Module::create(path, error);
			if(!dll)
			{
				std::string reason = "Could not create Module for file:";
				reason += path;
				reason += "\nError: ";
				reason += error;
				// Displays message box and quits process.
				//		IPlatform::instance ().kill (-1, reason);
				return 0;// gmpi::MP_FAIL;
			}

			auto classID = VST3::UID::fromString(uuid);
			if(!classID)
			{
				return 0;//gmpi::MP_FAIL;
			}

			auto factory = dll->getFactory();
			for(auto& classInfo : factory.classInfos())
			{
				if(classInfo.ID() == *classID && classInfo.category() == kVstAudioEffectClass) //kVstComponentControllerClass)//kVstAudioEffectClass)
				{
					pluginProvider_.reset(new Steinberg::Vst::PlugProvider(factory, classInfo, false));// true));
					return 0;
				}
			}

		}
#endif

#if 0 //TODO
		if (pAEffectWrapper->IsLoaded())
		{
			pAEffectWrapper->dispatcher(effOpen);
#if defined (SE_TARGET_WAVES)
			pluginInstances.push_back(std::pair< intptr_t, AEffectWrapperWaves* >(instance, pAEffectWrapper));

			if (bIsFirstPluginInstance)
			{
				sPluginHandle pluginHandle;
				pluginHandle.handle = handle;
				pluginHandle.refCounter = 1;
				pluginDllHandles.push_back(std::pair< sPluginHandle, gmpi_dynamic_linking::DLL_HANDLE >(pluginHandle, dllHandle));

			}
#else
			pluginInstance.reset(pAEffectWrapper);
#endif

		}
		else
		{
			delete pAEffectWrapper;
			pAEffectWrapper = 0;
		}
#endif
	}
	return 0;// pAEffectWrapper;
}

void ControllerWrapper::OpenGui()
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

bool ControllerWrapper::OnTimer()
{
	if( stateDirty )
	{
//		if (presetsUseChunks)
		{
			preSaveState();
		}
		
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

tresult VstComponentHandler::restartComponent (int32 flags)
{
	return kResultOk;
}


