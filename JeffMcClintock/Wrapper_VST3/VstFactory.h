#pragma once
/*
#include "VstFactory.h"
*/

#include "../se_sdk3/mp_api.h"
#include "../shared/xplatform.h"
#include <map>
#include <vector>
#include <filesystem>

#if !defined(SE_TARGET_WAVES)
#include "public.sdk/source/vst/hosting/module.h"
#endif

#include "public.sdk/source/vst/hosting/hostclasses.h"

using namespace gmpi;

typedef class gmpi::IMpUnknown* (*MP_CreateFunc2)();

class VstFactory* GetVstFactory();

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
#if !defined(SE_TARGET_WAVES)
	std::string getDiagnostics();
#endif

private:
	void ShallowScanVsts();
	void ScanVsts();
	void ScanDll(const std::wstring& load_filename);

	void RecursiveScanVsts(const std::wstring& searchPath, const std::wstring& excludePath);

	std::filesystem::path getSettingFilePath();

	void savePluginInfo();
	void loadPluginInfo();
};

