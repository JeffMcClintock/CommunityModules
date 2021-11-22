#pragma once

#include <memory>
#include "../se_sdk3/mp_sdk_common.h" 
#include "../se_sdk3/TimerManager.h"
#include "WindowManager.h"
#include "./PeerCommunication.h"

namespace VST3
{
	namespace Hosting
	{
		class Module;
	}
}
class VstComponentHandler;
class myPluginProvider;

class ControllerWrapper : public gmpi::IMpController, public TimerClient, public IVST3PluginOwner
{
protected:
	static const int chunkParamId = 0;

	std::wstring filename_;
	std::string shellPluginId_;
	std::shared_ptr<VST3::Hosting::Module> dll;
	std::shared_ptr<WindowController> windowController;
	std::unique_ptr<VstComponentHandler> componentHandler;

	bool inhibitFeedback = {};
	bool isSynthEditPresetEmpty = {};
	bool isOpen = {};
	// we need a way of informing SE processor when plugin is unloaded.
	// we do so by nulling it's pointers to the VST3s processor.
	Steinberg::Vst::IComponent** processor_component_ptr = {};
	Steinberg::Vst::IAudioProcessor** processor_vstEffect__ptr = {};

public:
	std::unique_ptr<myPluginProvider> plugin;
	int32_t handle_ = -1;
	bool stateDirty = {};
	gmpi::IMpControllerHost* host_ = {};

	ControllerWrapper(const wchar_t* filename, const std::string& uuid);
	~ControllerWrapper();

	virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override;
	virtual int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override;
	virtual int32_t MP_STDCALL preSaveState() override;
	virtual int32_t MP_STDCALL open() override;
	virtual int32_t MP_STDCALL setPinDefault(int32_t, int32_t, const char*) override
	{
		return gmpi::MP_OK;
	}

	virtual int32_t MP_STDCALL setPin(int32_t, int32_t, int64_t, const void*) override
	{
		return gmpi::MP_OK;
	}
	virtual int32_t MP_STDCALL notifyPin(int32_t, int32_t) override
	{
		return gmpi::MP_OK;
	}
	int32_t MP_STDCALL onDelete() override
	{
		return gmpi::MP_OK;
	}
	int32_t LoadPlugin(std::string path, std::string uuid);
	bool OnTimer() override;
	void OpenGui();
	// IVST3PluginOwner methods.
	int32_t MP_STDCALL registerProcessor(Steinberg::Vst::IComponent**, Steinberg::Vst::IAudioProcessor**) override;

	GMPI_REFCOUNT;
	int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
	{
		*returnInterface = nullptr;

		if (iid == gmpi::MP_IID_CONTROLLER || iid == gmpi::MP_IID_UNKNOWN)
		{
			*returnInterface = static_cast<gmpi::IMpController*>(this);
			addRef();
			return gmpi::MP_OK;
		}
#if 0
		if (iid == MP_IID_VST3_CONTROLLER_WRAPPER)
		{
			*returnInterface = static_cast<IVst3ControllerWrapper*>(this);
			addRef();
			return gmpi::MP_OK;
		}
#endif
		if (iid == WV_IID_VST3CONTROLLERHOST)
		{
			*returnInterface = static_cast<IVST3PluginOwner*>(this);
			addRef();
			return gmpi::MP_OK;
		}
		
		return gmpi::MP_NOSUPPORT;
	}
};


