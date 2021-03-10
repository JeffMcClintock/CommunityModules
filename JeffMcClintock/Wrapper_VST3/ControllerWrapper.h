#pragma once
#include "../se_sdk3/mp_sdk_common.h" 
#include "../se_sdk3/TimerManager.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/base/smartpointer.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "WindowManager.h"

class ControllerWrapper : public gmpi::IMpController, public TimerClient
{
protected:
	std::shared_ptr<VST3::Hosting::Module> dll;
	int32_t handle_;
	std::wstring filename_;
	std::string shellPluginId_;
	gmpi::IMpControllerHost* host_;
	std::shared_ptr<WindowController> windowController;

	bool stateDirty;
	bool inhibitFeedback;
	bool presetsUseChunks;
	bool hasGuiParameterPins;
	bool isSynthEditPresetEmpty;
	bool isOpen;

public:
	Steinberg::IPtr<Steinberg::Vst::PlugProvider> pluginProvider_;

	ControllerWrapper(const wchar_t* filename, const std::string& uuid, bool ppresetsUseChunks, bool phasGuiParameterPins);
	~ControllerWrapper()
	{
		if(windowController)
		{
			windowController->destroyView();
		}
		pluginProvider_ = nullptr; // ensure it's destroyed before dll is automatically unloaded.
	}
	virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override;
	virtual int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override;
	virtual int32_t MP_STDCALL preSaveState() override;
	virtual int32_t MP_STDCALL open() override;
	virtual int32_t MP_STDCALL setPinDefault(int32_t pinType, int32_t pinId, const char* defaultValue) override
	{
		return gmpi::MP_OK;
	}

	virtual int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int64_t size, const void* data) override
	{
		return gmpi::MP_OK;
	}
	virtual int32_t MP_STDCALL notifyPin(int32_t pinId, int32_t voice) override
	{
		return gmpi::MP_OK;
	}
	int32_t MP_STDCALL onDelete() override
	{
		return gmpi::MP_OK;
	}

	virtual bool OnTimer() override;
	int LoadPlugin(std::string path, std::string uuid);
	void OpenGui();

	GMPI_REFCOUNT;
	GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTROLLER, gmpi::IMpController);
};


