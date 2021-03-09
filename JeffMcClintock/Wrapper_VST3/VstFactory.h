#pragma once
/*
#include "VstFactory.h"
*/

#include "../se_sdk3/mp_api.h"
#include "../shared/xplatform.h"
#include <map>
#include <vector>

#if !defined(SE_TARGET_WAVES)
#include "public.sdk/source/vst/hosting/module.h"
#endif

// Control all parameters externally via DSP and GUI pins. (usual method is store state from plugin via getChunck/setChunk).
#define VST_WRAPPER_EXTERNAL_CONTROL_STRATEGY
//#define VST_WRAPPER_BLOB_STORAGE_STRATEGY

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
	bool scannedPLugins = {};
#if !defined(SE_TARGET_WAVES)
//	std::map< std::string, std::wstring > pluginIdMap;
#endif
	static const char* pluginIdPrefix;

public:
	virtual ~VstFactory(void) {};

	/* IMpUnknown methods */
	virtual int32_t MP_STDCALL queryInterface(const MpGuid& iid, void** returnInterface);
	GMPI_REFCOUNT_NO_DELETE

	/* IMpFactory methods */
	virtual int32_t MP_STDCALL createInstance(
		const wchar_t* uniqueId,
		int32_t subType,
		IMpUnknown* host,
		void** returnInterface);

	virtual int32_t MP_STDCALL createInstance2(
		const wchar_t* uniqueId,
		int32_t subType,
		void** returnInterface);

	virtual int32_t MP_STDCALL getSdkInformation(int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation);

	// IMpShellFactory: Query a plugin's info.
	virtual int32_t MP_STDCALL getPluginIdentification(int32_t index, IMpUnknown* iReturnXml) override;	// ID and name only.
	std::string uuidFromWrapperID(const wchar_t* uniqueId);
	virtual int32_t MP_STDCALL getPluginInformation(const wchar_t* iid, IMpUnknown* iReturnXml) override;		// Full pin details.

//	void AddPlugin(VST3::Hosting::PluginFactory& factory, const VST3::Hosting::ClassInfo& info);
	void AddPluginName(const char* category, std::string uuid, const std::string& name, const std::wstring& shellPath);
	std::string XmlFromPlugin(VST3::Hosting::PluginFactory& factory, const VST3::Hosting::ClassInfo& info);
#if !defined(SE_TARGET_WAVES)
/*
	std::wstring getWavesShellLocation()
	{
		std::wstring r;
		bool first = true;
		for (auto& it : pluginIdMap)
		{
			if (!first)
			{
				r += L", ";
			}
			r += it.second;
			first = false;
		}
		return r;
	}
*/
	std::string getDiagnostics();
	std::wstring getShellFromId(const std::string& uuid);
#endif

private:
	void ShallowScanVsts();
	void ScanVsts();
	void ScanDll(const std::wstring& load_filename);
//	void ScanForWavesShell(const std::wstring& searchPath, const std::wstring& excludePath);

	void RecursiveScanVsts(const std::wstring& searchPath, const std::wstring& excludePath);

	std::wstring getSettingFilePath();

	void savePluginInfo();
	void loadPluginInfo();
};