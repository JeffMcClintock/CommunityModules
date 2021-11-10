#include "VST2WrapperController.h"
#include "Wrapper_VST2.h"

VST2WrapperController::VST2WrapperController( const std::wstring &filename, VstIntPtr shellPluginId, bool ppresetsUseChunks, bool phasGuiParameterPins) :
handle_(0)
, filename_(filename)
, shellPluginId_(shellPluginId)
, host_(0)
, stateDirty(false)
, inhibitFeedback(false)
, presetsUseChunks(ppresetsUseChunks)
, hasGuiParameterPins(phasGuiParameterPins)
, isOpen(false)
{
}

int32_t VST2WrapperController::setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size)
{
	// Avoid altering plugin state until we can determin if we are restoring a saved preset, or keeping init preset.
	if(!isOpen)
		return MP_OK;

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
#if 0//defined (_DEBUG) & defined(_WIN32)
							auto effectName = ae->getName();

							_RPT0(_CRT_WARN, "{ ");
							auto d = (unsigned char*)data;
							for (int i = 0; i < 12; ++i)
							{
								_RPT1(_CRT_WARN, "%02x ", (int)d[i]);
							}
							_RPT2(_CRT_WARN, "}; set: %s H %d\n", effectName.c_str(), moduleHandle);
#endif
							_RPT1(_CRT_WARN, "%s\n\n ", ((char*)data)+26);
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

	return MP_OK;
}

int32_t VST2WrapperController::preSaveState()
{
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
#if 0 // defined (_DEBUG) & defined(_WIN32)
			_RPT0(_CRT_WARN, "{ ");
			auto d = (unsigned char*)chunkPtr;
			for (int i = 0; i < 12; ++i)
			{
				_RPT1(_CRT_WARN, "%02x ", (int)d[i]);
			}
			_RPT0(_CRT_WARN, "}; get\n");
#endif

#if 0 //defined (_DEBUG) & defined(_WIN32)
			// print preset out in handy format to be pasted into ALG source code. (prints to debug window).
			{
				_RPT0(_CRT_WARN, "// VST2\n");
				const unsigned char* d = (const unsigned char*)chunkPtr;
				for (int i = 0 ; i < chunkSize; ++i)
				{
					if ((i % 60) == 0)
					{
						_RPT0(_CRT_WARN, "\n");
					}
					_RPT1(_CRT_WARN, "0x%02x, ", d[i]);
				}
				_RPT0(_CRT_WARN, "\n");
			}
#endif

			const int voiceId = 0;
			int paramId = 0;

			if(hasGuiParameterPins)
				paramId = ae->getNumParams();

			host_->setParameter(host_->getParameterHandle(handle_, paramId), MP_FT_VALUE, voiceId, (char*)chunkPtr, (int32_t) chunkSize);
			// _RPT1(_CRT_WARN, "VST2WrapperController:: Saved State: %d bytes\n", chunkSize);
		}

		stateDirty = false;
		inhibitFeedback = false;
	}

	return MP_OK;
}

int32_t VST2WrapperController::open()
{
	isOpen = true;

	auto ae = pluginInstance.get();

	if (ae == nullptr) // VST not installed?
	{
		return MP_OK;
	}

	// Pass VST2 plugin address to Process and GUI.
	int audioeffectPointerParamId = 0;
	if (presetsUseChunks)
	{
		int paramId = 0;

		if (hasGuiParameterPins)
			paramId = ae->getNumParams();

		audioeffectPointerParamId = paramId + 1;
	}
	else
	{
		audioeffectPointerParamId = ae->getNumParams();
	}

	const int voiceId = 0;
	host_->setParameter(host_->getParameterHandle(handle_, audioeffectPointerParamId), MP_FT_VALUE, voiceId, &ae, sizeof(ae));

	if (presetsUseChunks)
	{
		int chunkParamId = 0;

		if (hasGuiParameterPins)
			chunkParamId = ae->getNumParams();

		// Test if host has a valid chunk preset.
		isSynthEditPresetEmpty = false;
		host_->updateParameter(host_->getParameterHandle(handle_, chunkParamId), MP_FT_VALUE, voiceId);

		// In the case we have no preset stored yet (because user *just* inserted plugin), Copy init preset to SE patch memory.
		if (isSynthEditPresetEmpty)
		{
			preSaveState();
		}

		// Wrapper type 2 has non-persistant float parameters, so always sync from VST to SE plugin when opening.
		if (hasGuiParameterPins)
		{
			// Sync VST params -> SE Params
			for (auto parameterId = ae->getNumParams() - 1; parameterId > 0; --parameterId)
			{
				float value = ae->getParameter(parameterId);
				host_->setParameter(host_->getParameterHandle(handle_, parameterId), MP_FT_VALUE, voiceId, (const void*)&value, (int32_t) sizeof(value));
			}
		}
	}
	else
	{
		// Sync VST params <- SE Params.
		// non-chunk type VST Wrapper will open with a zeroed preset when inserted, but you can select a valid preset which will save OK.
		for (auto parameterId = ae->getNumParams() - 1; parameterId > 0; --parameterId)
		{
			host_->updateParameter(host_->getParameterHandle(handle_, parameterId), MP_FT_VALUE, voiceId);
		}
	}

	return MP_OK;
}

int32_t VST2WrapperController::setHost(gmpi::IMpUnknown* host)
{
	host->queryInterface(MP_IID_CONTROLLER_HOST, reinterpret_cast<void **>( &host_ ));

	if( host_ == 0 )
	{
		return MP_NOSUPPORT; //  throw "host Interfaces not supported";
	}

	host_->getHandle(handle_);

	auto ae = LoadVst2Plugin(filename_, shellPluginId_);

	if( ae == nullptr ) // VST not installed.
	{
		return MP_FAIL;
	}

	// Report latency to SE
	host_->setLatency(ae->getLatency());

	ae->registerObserver(this);

	return MP_OK;
}

AEffectWrapper* VST2WrapperController::LoadVst2Plugin(const std::wstring& filename, VstIntPtr shellPluginId)
{
	AEffectWrapper* pAEffectWrapper = 0;
	
	assert(pluginInstance.get() == nullptr);

	{
		pAEffectWrapper = new AEffectWrapper();
		pAEffectWrapper->LoadDll(filename, shellPluginId);
		if (pAEffectWrapper->IsLoaded())
		{
			pAEffectWrapper->dispatcher(effOpen);

			pluginInstance.reset(pAEffectWrapper);
		}
		else
		{
			delete pAEffectWrapper;
			pAEffectWrapper = 0;
		}
	}

	return pAEffectWrapper;
}

void VST2WrapperController::hUpdateDisplay() // patch change or whatever
{
	stateDirty = true;
	StartTimer(250);
}

void VST2WrapperController::hUpdateParam(int index, float value)
{
	if (presetsUseChunks)
		stateDirty = true;

	// Sync VST param -> SE Param
	const int paramId = index;
	const int voiceId = 0;
	host_->setParameter(host_->getParameterHandle(handle_, paramId), MP_FT_VALUE, voiceId, (const void*)&value, (int32_t) sizeof(value));
}

bool VST2WrapperController::OnTimer()
{
	if( stateDirty )
	{
		if (presetsUseChunks)
		{
			preSaveState();
		}
		
		if(hasGuiParameterPins)
		{
			inhibitFeedback = true;
			auto ae = pluginInstance.get();
			if (ae != nullptr)
			{
				// Save presets.
				const int voiceId = 0;
				for (auto p = ae->getNumParams() - 1; p > 0; --p)
				{
					float value = ae->getParameter(p);
					host_->setParameter(host_->getParameterHandle(handle_, p), MP_FT_VALUE, voiceId, (const void*)&value, (int32_t) sizeof(value));
				}
			}
			inhibitFeedback = false;
		}
		stateDirty = false;
	}
	return false;
}
