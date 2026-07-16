#pragma once
/*
#include "VstFactory.h"
*/

#include "../se_sdk3/mp_api.h"
#include "../shared/xplatform.h"
#include <map>
#include <mutex>
#include <vector>
#include <filesystem>

#include "public.sdk/source/vst/hosting/module.h"

#include "public.sdk/source/vst/hosting/hostclasses.h"

using namespace gmpi;

typedef class gmpi::IMpUnknown* (*MP_CreateFunc2)();

class VstFactory* GetVstFactory();
class ProcessorWrapper;
class ControllerWrapper;

// VstFactory - a singleton object. The plugin registers it's ID with the factory.
class VstFactory : public gmpi::IMpShellFactory
{
	struct pluginInfo
	{
		std::string uuid_;
		std::string name_;
		std::string xmlBrief_;
		std::wstring shellPath_;
	};

	std::vector< pluginInfo > plugins;
	std::vector< std::string > duplicates;
	bool scannedPlugins = {};
	static const char* pluginIdPrefix;
	Steinberg::Vst::HostApplication pluginContext;

public:
	virtual ~VstFactory(void) {}

	/* IMpUnknown methods */
	virtual int32_t MP_STDCALL queryInterface(const MpGuid& iid, void** returnInterface) override;
	GMPI_REFCOUNT_NO_DELETE

	/* IMpFactory methods */
	virtual int32_t MP_STDCALL createInstance(
		const wchar_t* uniqueId,
		int32_t subType,
		IMpUnknown* host,
		void** returnInterface) override;

	virtual int32_t MP_STDCALL createInstance2(
		const wchar_t* uniqueId,
		int32_t subType,
		void** returnInterface) override;

	virtual int32_t MP_STDCALL getSdkInformation(int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation) override;

	// IMpShellFactory: Query a plugin's info.
	virtual int32_t MP_STDCALL getPluginIdentification(int32_t index, IMpUnknown* iReturnXml) override;	// ID and name only.
	std::string uuidFromWrapperID(const wchar_t* uniqueId);
	virtual int32_t MP_STDCALL getPluginInformation(const wchar_t* iid, IMpUnknown* iReturnXml) override;		// Full pin details.

	void AddPluginName(const char* category, std::string uuid, const std::string& name, const std::wstring& shellPath);
	std::string XmlFromPlugin(VST3::Hosting::PluginFactory& factory, const VST3::UID& classId);
	std::string getDiagnostics();

	// Registry connecting the two halves (Processor/Controller) of each plugin instance.
	// Both halves share the same host-assigned handle.
	void registerWrapper(int32_t handle, ControllerWrapper* controller);
	ControllerWrapper* registerWrapper(int32_t handle, ProcessorWrapper* processor); // returns the controller, which registered already.
	void unregisterWrapper(int32_t handle, ControllerWrapper* controller);
	void unregisterWrapper(int32_t handle, ProcessorWrapper* processor);
	ControllerWrapper* getController(int32_t handle);

private:
	struct wrapperPair
	{
		ProcessorWrapper* processor{};
		ControllerWrapper* controller{};
	};
	std::mutex registryMutex;
	std::map<int32_t, wrapperPair> registry;

	void ShallowScanVsts();
	void ScanVsts();
	void ScanDll(const std::wstring& load_filename);

	void RecursiveScanVsts(const std::wstring& searchPath, const std::wstring& excludePath);

	std::filesystem::path getSettingFilePath();

	void savePluginInfo();
	void loadPluginInfo();
};

