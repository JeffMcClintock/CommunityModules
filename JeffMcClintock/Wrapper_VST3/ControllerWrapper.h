#pragma once
#include "../se_sdk3/mp_sdk_common.h" 
#include "../se_sdk3/TimerManager.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/base/smartpointer.h"
#include "public.sdk/source/vst/hosting\hostclasses.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/hosting\module.h"
#include "pluginterfaces/vst/ivstcomponent.h"
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
#if 0 // older?
struct myPluginProvider
{
	IPtr<Steinberg::Vst::IComponent> component;
	IPtr<Steinberg::Vst::IEditController> controller;
	Steinberg::Vst::HostApplication pluginContext;

	~myPluginProvider()
	{
		terminatePlugin();
	}

	void terminatePlugin()
	{
		disconnectComponents ();

		bool controllerIsComponent = false;
		if (component)
		{
			controllerIsComponent = FUnknownPtr<Steinberg::Vst::IEditController> (component).getInterface () != nullptr;
			component->terminate ();
		}

		if (controller && controllerIsComponent == false)
			controller->terminate ();

		component = nullptr;
		controller = nullptr;
	}

	bool connectComponents()
	{
		FUnknownPtr<Steinberg::Vst::IConnectionPoint> compICP (component);
		FUnknownPtr<Steinberg::Vst::IConnectionPoint> contrICP (controller);

		return compICP
			&& contrICP
			&& kResultOk == compICP->connect(contrICP)
			&& kResultOk == contrICP->connect(compICP);
	}

	bool disconnectComponents()
	{
		FUnknownPtr<Steinberg::Vst::IConnectionPoint> compICP (component);
		FUnknownPtr<Steinberg::Vst::IConnectionPoint> contrICP (controller);

		return compICP
			&& contrICP
			&& kResultOk == compICP->disconnect(contrICP)
			&& kResultOk == contrICP->disconnect(compICP);
	}

	bool setup(VST3::Hosting::PluginFactory& factory, VST3::Hosting::ClassInfo classInfo)
	{
		bool res = false;

		//---create Plug-in here!--------------
		// create its component part
		component = factory.createInstance<Steinberg::Vst::IComponent> (classInfo.ID ());
		if (component)
		{
			// initialize the component with our context
			res = (component->initialize (&pluginContext) == kResultOk);

			// try to create the controller part from the component
			// (for Plug-ins which did not succeed to separate component from controller)
			if (component->queryInterface (Steinberg::Vst::IEditController::iid, (void**)&controller) != kResultTrue)
			{
				TUID controllerCID;

				// ask for the associated controller class ID
				if (component->getControllerClassId (controllerCID) == kResultTrue)
				{
					// create its controller part created from the factory
					controller = factory.createInstance<Steinberg::Vst::IEditController> (VST3::UID (controllerCID));
					if (controller)
					{
						// initialize the component with our context
						res = (controller->initialize (&pluginContext) == kResultOk);
					}
				}
			}
		}

		if(res)
		{
			connectComponents();
		}

		return res;
	}
};
#endif

class ControllerWrapper : public gmpi::IMpController, public TimerClient
{
protected:
	std::shared_ptr<VST3::Hosting::Module> dll;
	std::wstring filename_;
	std::string shellPluginId_;
	std::shared_ptr<WindowController> windowController;
	VstComponentHandler componentHandler;

	bool inhibitFeedback = {};
	bool isSynthEditPresetEmpty = {};
	bool isOpen = {};

public:
	std::unique_ptr<class myPluginProvider> plugin;
	int32_t handle_ = -1;
	bool stateDirty = {};
	gmpi::IMpControllerHost* host_ = {};

	ControllerWrapper(const wchar_t* filename, const std::string& uuid);
	~ControllerWrapper();

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


