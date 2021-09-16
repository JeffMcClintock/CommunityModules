#pragma once
#include "../se_sdk3/mp_sdk_common.h" 
#include "AEffectWrapper.h"
#include "VstFactory.h"
#include "../se_sdk3/TimerManager.h"

class VST2WrapperController : public gmpi::IMpController, public TimerClient, public IVstObserver
{
protected:
	int32_t handle_;
	std::wstring filename_;
	VstIntPtr shellPluginId_;
	IMpControllerHost* host_;
	bool stateDirty;
	bool inhibitFeedback;
	bool presetsUseChunks;
	bool hasGuiParameterPins;
	std::unique_ptr<AEffectWrapper> pluginInstance;
	bool isSynthEditPresetEmpty;
	bool isOpen;

public:
	VST2WrapperController( const std::wstring &filename, VstIntPtr shellPluginId, bool ppresetsUseChunks, bool phasGuiParameterPins);

	int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override;
	int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override;
	int32_t MP_STDCALL preSaveState() override;
	int32_t MP_STDCALL open() override;
	int32_t MP_STDCALL setPinDefault(int32_t pinType, int32_t pinId, const char* defaultValue) override
	{
		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int64_t size, const void* data) override
	{
		return gmpi::MP_OK;
	}
	int32_t MP_STDCALL notifyPin(int32_t pinId, int32_t voice) override
	{
		return gmpi::MP_OK;
	}
	int32_t MP_STDCALL onDelete() override
	{
		return gmpi::MP_OK;
	}

	void hUpdateParam(int index, float value) override;
	void hUpdateDisplay() override; // patch change or whatever
	bool OnTimer() override;
	AEffectWrapper* LoadVst2Plugin(const std::wstring& filename, VstIntPtr shellPluginId);

	GMPI_REFCOUNT;
	GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTROLLER, gmpi::IMpController);
};


