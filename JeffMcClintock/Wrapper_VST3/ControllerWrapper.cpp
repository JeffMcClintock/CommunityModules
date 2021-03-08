#include "ControllerWrapper.h"
//#include "Vst2Wrapper.h"
//#include "./EditButtonGui.h"
#include "unicode_conversion.h"
#if !defined(SE_TARGET_WAVES)
//#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#endif
using namespace gmpi;

ControllerWrapper::ControllerWrapper(const wchar_t* filename, const std::string& uuid, bool ppresetsUseChunks, bool phasGuiParameterPins) :
handle_(0)
, filename_(filename)
, shellPluginId_(uuid)
, host_(0)
, stateDirty(false)
, inhibitFeedback(false)
, presetsUseChunks(ppresetsUseChunks)
, hasGuiParameterPins(phasGuiParameterPins)
, isOpen(false)
{
}

int32_t ControllerWrapper::setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size)
{
	// Avoid altering plugin state until we can determin if we are restoring a saved preset, or keeping init preset.
	if(!isOpen)
		return MP_OK;
#if 0 // TODO!!!
	if( inhibitFeedback == false && fieldId == 0 ) // FT_VALUE
	{
		auto ae = pluginInstance.get();

		if( ae != nullptr )
		{
			int32_t moduleHandle = -1;
			int32_t moduleParameterId = -1;
			host_->getParameterModuleAndParamId(parameterHandle, &moduleHandle, &moduleParameterId);

			if (hasGuiParameterPins && moduleParameterId < ae->getNumParams()) // Normal float parameters.
			{
				assert(size == sizeof(float));
				ae->setParameter(moduleParameterId, *(float*)data);
			}
			else
			{
				// Blob pin.
				if (presetsUseChunks)
				{
					int chunkParamId = hasGuiParameterPins ? ae->getNumParams() : 0;
					if (chunkParamId == moduleParameterId ) // ignore aeffect ptr.
					{
						if (size == 0) // Size of zero implies first-time init, no preset stored yet.
						{
							isSynthEditPresetEmpty = true;
						}
						else
						{
#if defined (_DEBUG) & defined(_WIN32)
							auto effectName = ae->getName();

							_RPT0(_CRT_WARN, "{ ");
							auto d = (unsigned char*)data;
							for (int i = 0; i < 12; ++i)
							{
								_RPT1(_CRT_WARN, "%02x ", (int)d[i]);
							}
							_RPT2(_CRT_WARN, "}; set: %s H %d\n", effectName.c_str(), moduleHandle);
#endif
							// Load preset chunk.
							const int isPreset = 1; // 0 = all presets.
							void* chunkPtr = nullptr;
							ae->dispatcher(effSetChunk, isPreset, size, (void*)data);
						}
					}
				}
			}
		}
	}
#endif
	return MP_OK;
}

int32_t ControllerWrapper::preSaveState()
{
#if 0 // TODO!!!
	if (presetsUseChunks)
	{
		inhibitFeedback = true;
		auto ae = pluginInstance.get();

		if (ae != nullptr)
		{
			// Save presets.
			const int isPreset = 1; // 0 = all presets.
			void* chunkPtr = nullptr;
			auto chunkSize = ae->dispatcher(effGetChunk, isPreset, 0, &chunkPtr);

			if (chunkPtr == nullptr) // 'Bomb the beat' returns NULL in demo version (but doesn't return chunk size 0)
			{
				chunkPtr = &chunkPtr; // point at valid memory rather than null.
				chunkSize = 0;
			}
#if defined (_DEBUG) & defined(_WIN32)
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
			int paramId = 0;

			if(hasGuiParameterPins)
				paramId = ae->getNumParams();

			host_->setParameter(host_->getParameterHandle(handle_, paramId), MP_FT_VALUE, voiceId, (char*)chunkPtr, (int32_t) chunkSize);
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

//	auto ae = owned(pluginProvider_->getController());
	auto pluginProviderPtr = pluginProvider_.get();
	auto ae = owned(pluginProvider_->getController());

	// Pass VST plugin address to Process and GUI.
	const int chunkParamId = ae->getParameterCount();
	const int controllerPtrParamId = chunkParamId + 1;
	const int voiceId = 0;
	host_->setParameter(host_->getParameterHandle(handle_, controllerPtrParamId), MP_FT_VALUE, voiceId, &pluginProviderPtr, sizeof(pluginProviderPtr));

//	if (presetsUseChunks)
	{
		//if (hasGuiParameterPins)
		//	chunkParamId = ae->getNumParams();

		// Test if host has a valid chunk preset.
		isSynthEditPresetEmpty = false;
		host_->updateParameter(host_->getParameterHandle(handle_, chunkParamId), MP_FT_VALUE, voiceId);

		// In the case we have no preset stored yet (because user *just* inserted plugin), Copy init preset to SE patch memory.
		if (isSynthEditPresetEmpty)
		{
			preSaveState();
		}

		// Wrapper type 2 has non-persistant float parameters, so always sync from VST to SE plugin when opening.
		//if (hasGuiParameterPins)
		//{
		//	// Sync VST params -> SE Params
		//	for (auto parameterId = ae->getNumParams() - 1; parameterId > 0; --parameterId)
		//	{
		//		float value = ae->getParameter(parameterId);
		//		host_->setParameter(host_->getParameterHandle(handle_, parameterId), MP_FT_VALUE, voiceId, (const void*)&value, (int32_t) sizeof(value));
		//	}
		//}
	}
	//else
	//{
	//	// Sync VST params <- SE Params.
	//	// non-chunk type VST Wrapper will open with a zeroed preset when inserted, but you can select a valid preset which will save OK.
	//	for (auto parameterId = ae->getNumParams() - 1; parameterId > 0; --parameterId)
	//	{
	//		host_->updateParameter(host_->getParameterHandle(handle_, parameterId), MP_FT_VALUE, voiceId);
	//	}
	//}
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

	auto ae = LoadPlugin( JmUnicodeConversions::WStringToUtf8(filename_), shellPluginId_);

	if( ae == nullptr ) // VST not installed.
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
AEffectWrapper* ControllerWrapper::LoadPlugin(std::string path, std::string uuid)
#endif
{

#if defined (SE_TARGET_WAVES)
	AEffectWrapperWaves* pAEffectWrapper = 0;
#else
	AEffectWrapper* pAEffectWrapper = 0;
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
			auto module = VST3::Hosting::Module::create(path, error);
			if(!module)
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

			auto factory = module->getFactory();
			for(auto& classInfo : factory.classInfos())
			{
				if(classInfo.ID() == *classID && classInfo.category() == kVstAudioEffectClass) //kVstComponentControllerClass)//kVstAudioEffectClass)
				{
					pluginProvider_.reset(new Steinberg::Vst::PlugProvider(factory, classInfo, true));
//
//					native_ = pluginProvider_->getController();
//					if(!native_)
//					{
//						//		error = "No EditController found (needed for allowing editor) in file " + path;
//						//		IPlatform::instance ().kill (-1, error);
//						return {};
//					}
////					native_->release(); // plugProvider does an addRef
					return 0;// gmpi::MP_OK;
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
	return pAEffectWrapper;
}

bool ControllerWrapper::OnTimer()
{
	if( stateDirty )
	{
		if (presetsUseChunks)
		{
			preSaveState();
		}
		
		stateDirty = false;
	}
	return false;
}
