#pragma once
/*
#include "VstFactory.h"
*/

#include "../se_sdk3/mp_api.h"
#include "../shared/xplatform.h"
#include <map>
#include <vector>
#include "AEffectWrapper.h"

// Control all parameters externally via DSP and GUI pins. (usual method is store state from plugin via getChunck/setChunk).
#define VST_WRAPPER_EXTERNAL_CONTROL_STRATEGY
//#define VST_WRAPPER_BLOB_STORAGE_STRATEGY

using namespace gmpi;

typedef class gmpi::IMpUnknown* (*MP_CreateFunc2)();

class VstFactory* GetVstFactory();


// VstFactory - a singleton object.  The plugin registers it's ID with the factory.
class VstFactory : public gmpi::IMpShellFactory
{
	struct pluginInfo
	{
		std::string name_;
		VstIntPtr uniqueId_;
		std::string xmlBrief_;
		std::string fullPath_;
	};

	std::vector< pluginInfo > plugins;
	std::vector< std::string > duplicates;
	bool scannedPlugins;
	std::map< std::string, platform_string > pluginPaths;

public:
	VstFactory(void);
	virtual ~VstFactory(void) {};

	/* IMpUnknown methods */
    int32_t MP_STDCALL queryInterface(const MpGuid& iid, void** returnInterface) override;
	GMPI_REFCOUNT_NO_DELETE

		/* IMpFactory methods */
    int32_t MP_STDCALL createInstance(
			const wchar_t* uniqueId,
			int32_t subType,
			IMpUnknown* host,
			void** returnInterface) override;

    int32_t MP_STDCALL createInstance2(
		const wchar_t* uniqueId,
		int32_t subType,
		void** returnInterface) override;

    int32_t MP_STDCALL getSdkInformation(int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation) override;

	// IMpShellFactory: Query a plugin's info.
    int32_t MP_STDCALL getPluginIdentification(int32_t index, IMpUnknown* iReturnXml) override;	// ID and name only.
    int32_t MP_STDCALL getPluginInformation(const wchar_t* iid, IMpUnknown* iReturnXml) override;		// Full pin details.

	void AddPlugin(/*const std::wstring* full_path,*/ class AEffectWrapper* plugin);
	void AddPluginName(const char* shellName, VstIntPtr uniqueId, const std::string& name, std::string fullPath);
	std::string XmlFromPlugin(AEffectWrapper* plugin);// , int wrapperVersion);

	std::string getDiagnostics();
	std::wstring getShellFromId(intptr_t uniqueId);

private:
	void LocateVstPlugins();
	void ScanVsts();
	void ScanDll(const std::wstring& load_filename, const char* shellName);

	void RecursiveScanVsts(const platform_string& searchPath, const platform_string& excludePath);

	std::wstring getSettingFilePath();

	void savePluginInfo();
	void loadPluginInfo();
};
