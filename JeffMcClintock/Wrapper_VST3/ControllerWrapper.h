#pragma once
#include "../se_sdk3/mp_sdk_common.h" 
#include "../se_sdk3/TimerManager.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/base/smartpointer.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "WindowManager.h"

class VstComponentHandler : public Steinberg::FObject, public Steinberg::Vst::IComponentHandler
{
public:
	class ControllerWrapper* controller_;

	// IComponentHandler
	tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID id) override;
	tresult PLUGIN_API performEdit (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) override;
	tresult PLUGIN_API endEdit (Steinberg::Vst::ParamID id) override;
	tresult PLUGIN_API restartComponent (int32 flags) override;

	//---Interface---------
	OBJ_METHODS (VstComponentHandler, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Steinberg::Vst::IComponentHandler)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

class ControllerWrapper : public gmpi::IMpController, public TimerClient
{
protected:
	std::shared_ptr<VST3::Hosting::Module> dll;
	std::wstring filename_;
	std::string shellPluginId_;
	std::shared_ptr<WindowController> windowController;
	VstComponentHandler componentHandler;

	bool inhibitFeedback;
	bool isSynthEditPresetEmpty;
	bool isOpen;

public:
	int32_t handle_;
	bool stateDirty;
	gmpi::IMpControllerHost* host_ = {};
	Steinberg::IPtr<Steinberg::Vst::PlugProvider> pluginProvider_;

	ControllerWrapper(const wchar_t* filename, const std::string& uuid);
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


