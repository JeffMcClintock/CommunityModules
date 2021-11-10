#include "VstFactory.h"
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <assert.h>
#include "../se_sdk3/mp_sdk_common.h"
#include "../shared/unicode_conversion.h"
#include "AEffectWrapper.h"
#include "./Wrapper_VST2.h"
#include "../shared/xplatform.h"
#include "../shared/string_utilities.h"

#include "../shared/FileFinder.h"
#include "./Vst2WrapperGui.h"
#include "VST2WrapperController.h"
#include "./VstwrapperfailGui.h"
#include "./AaVstWrapperDiagGui.h"

#if defined(_WIN32)
#include "shlobj.h"
#endif

using namespace std;
using namespace gmpi;
using namespace JmUnicodeConversions;

#define VST_WRAPPER_ID_PREFIX "seVST2WRAP:"
#define VST_DIAG_WRAPPER_ID "seVST2WRAP:DIAG"
#define VST_DIAG_WRAPPER_ID_L L"seVST2WRAP:DIAG"

VstFactory::VstFactory(void) : scannedPlugins(false)
{
}

// IMpShellFactory: Query a plugin's info. Should occur only during a user-initiated re-scan.
int32_t VstFactory::getPluginIdentification(int32_t index, IMpUnknown* iReturnXml)
{
	IString* returnString = 0;

	if (MP_OK != iReturnXml->queryInterface(MP_IID_RETURNSTRING, reinterpret_cast<void**>( &returnString)))
	{
		return gmpi::MP_NOSUPPORT;
	}

	if (index == 0) // User-initiated rescan.
	{
		pluginPaths.clear();
		ScanVsts();
	}
	else
	{
		if (!scannedPlugins)
		{
			loadPluginInfo();
		}
	}

	if (index >= 0 && index < (int)plugins.size())
	{
		returnString->setData(plugins[index].xmlBrief_.data(), (int32_t)plugins[index].xmlBrief_.size());
		return gmpi::MP_OK;
	}

	return gmpi::MP_FAIL;
}

int32_t VstFactory::getPluginInformation(const wchar_t* uniqueId, IMpUnknown* iReturnXml)
{
	IString* returnXml = 0;

	if (MP_OK != iReturnXml->queryInterface(MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnXml)))
	{
		return gmpi::MP_NOSUPPORT;
	}

	if (!scannedPlugins)
	{
		loadPluginInfo();
	}

	int vstUniqueId = 0;
	if (wcslen(uniqueId) > 11)
	{
		wchar_t* idx;
		vstUniqueId = wcstol(uniqueId + 11, &idx, 16);
	}

	pluginInfo* info = {};
	for (auto& p : plugins)
	{
		if (p.uniqueId_ == vstUniqueId)
		{
			info = &p;
			break;
		}
	}

	if (!info)
		return gmpi::MP_FAIL;

	// load plugin, get full XML.
	AEffectWrapper ae;

	ae.LoadDll(Utf8ToWstring(info->fullPath_), info->uniqueId_);
	
	if (!ae.IsLoaded())
	{
		return gmpi::MP_FAIL;
	}

	ae.dispatcher(effOpen);
	std::string xmlFull = XmlFromPlugin(&ae);

	returnXml->setData(xmlFull.data(), (int32_t)xmlFull.size());

	return gmpi::MP_OK;
}

vector< std::wstring >getSearchPaths()
{
	vector< std::wstring >searchPaths;

#ifdef _WIN32
	// Use standard VST2 folders.
	wchar_t myPath[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, SHGFP_TYPE_CURRENT, myPath);
	std::wstring commonFilesFolder(myPath);
	SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, myPath);
	std::wstring programFilesFolder(myPath);

	// "C:\Program Files (x86)\VstPlugins
	searchPaths.push_back(programFilesFolder + L"\\VstPlugins\\");

	// "C:\Program Files (x86)\Common Files\VST2
	searchPaths.push_back(commonFilesFolder + L"\\VST2\\");

	// C:\Program Files (x86)\Common Files\Steinberg\VST2
	searchPaths.push_back(commonFilesFolder + L"\\Steinberg\\VST2\\");

	// "C:\Program Files\Steinberg\VstPlugins"
	searchPaths.push_back(programFilesFolder + L"\\Steinberg\\VstPlugins\\");
    
#else
    
    searchPaths.push_back(L"/Library/Audio/Plug-Ins/VST/");
    
#endif
    
	return searchPaths;
}

void VstFactory::LocateVstPlugins()
{
	// Time to re-scan VSTs.
	std::wstring excludeMyself = StripFilename(gmpi_dynamic_linking::MP_GetDllFilename());

	auto searchPaths = getSearchPaths();
	for (auto itSp = searchPaths.begin(); itSp != searchPaths.end(); ++itSp)
	{
		RecursiveScanVsts(*itSp, excludeMyself);
	}
}

void VstFactory::ScanVsts()
{
	LocateVstPlugins();

	/* not relevant when scanned in background thread
	// SE already inits com Single-Threaded. Applys only to imbedded? HMM why only for scan, what about instansiate?
	auto r = CoInitializeEx(NULL, COINIT_MULTITHREADED + COINIT_SPEED_OVER_MEMORY);
	auto rememberToDeInitCom = r == S_OK || r == S_FALSE;
	*/

	scannedPlugins = true;

	// Time to re-scan VSTs.
	plugins.clear();
	duplicates.clear();

	ostringstream oss;

	oss << "<PluginList><Plugin id=\"" VST_DIAG_WRAPPER_ID "\" name=\"Wrapper Info\" category=\"VST2 Plugins\" >";
	oss << "<GUI graphicsApi=\"GmpiGui\"><Pin/></GUI>";
	oss << "</Plugin></PluginList>";

	plugins.push_back({ "AA: VST WRAPPER2", 0, oss.str(), {} });

	for (auto& i : pluginPaths)
	{
		ScanDll(i.second, i.first.c_str());
	}
	savePluginInfo();
}

void VstFactory::RecursiveScanVsts(const platform_string& searchPath, const platform_string& excludePath)
{
	const auto searchMask = combinePathAndFile(searchPath, { _T("*.*") });
	for (FileFinder it = searchMask.c_str(); !it.done(); ++it)
	{
		if ((*it).isDots())
			continue;

		if ((*it).isFolder)
		{
			RecursiveScanVsts(searchPath + (*it).filename, excludePath);
		}
		else
		{
			if ((*it).filename.find(_T(".dll")) != std::string::npos && (*it).fullPath.find(excludePath) != 0)
			{
				pluginPaths.insert({toString((*it).filename), (*it).fullPath });
			}
		}
	}
}

struct backgroundData
{
	VstFactory* factory;
	const std::wstring* full_path;
	const char* shellName;
};

void VstFactory::ScanDll(const std::wstring /*platform_string*/& full_path, const char* shellName)
{
	const auto fullPath_u = WStringToUtf8(full_path);

	AEffectWrapper plugin;
	plugin.LoadDll(full_path);

	if (plugin.IsLoaded())
	{
		plugin.dispatcher(effOpen);

		bool isShellPlugin = plugin.dispatcher(effGetPlugCategory, 0, 0, NULL, 0.0f) == kPlugCategShell;

		if (isShellPlugin)
		{
			// plugin.dispatcher(effOpen); // Assume we need to open shell?

			vector < pair<int32_t, string > > pluginList;
			int index = 0;
			VstIntPtr uniqueId = 0;
			string name;

#if 1 // quick name-only scan.
			do
			{
				uniqueId = plugin.ShellGetNextPlugin(name);

				if (uniqueId != 0)
				{
					AddPluginName(shellName, uniqueId, name, fullPath_u);
				}
			} while (uniqueId != 0);
#else // full detailed scan
			do
			{
				uniqueId = plugin.ShellGetNextPlugin(name);

				if (uniqueId != 0)
				{
					pluginList.push_back(pair<int32_t, string>(uniqueId, name));
				}
			} while (uniqueId != 0);

			for (auto p : pluginList)
			{
				{
					AEffectWrapper subPlugin;
					subPlugin.InitFromShell(plugin.getDllHandle(), *(dat->full_path), p.first);
					if (subPlugin.IsLoaded())
					{
						subPlugin.dispatcher(effOpen);
						dat->factory->AddPlugin(dat->full_path, &subPlugin);
					}
				}
			}
#endif
		}
		else
		{
			// Standard VST. non-Waves.
			AddPluginName(shellName, plugin.getUniqueID(), plugin.getName(), fullPath_u);
		}
	}
}

void VstFactory::AddPluginName(const char* shellName, VstIntPtr uniqueId, const std::string& name, std::string fullPath)
{
	// Plugin ID's must be unique. Skip multiple waveshells.
	for (auto& p : plugins)
	{
		if (p.uniqueId_ == uniqueId)
		{
			ostringstream oss2;
			oss2 << std::hex << uniqueId << ":" << p.name_ <<", " << name;

			duplicates.push_back(oss2.str());
			return;
		}
	}

	std::string category(shellName);
	if (category.find("WaveShell") == std::string::npos)
	{
		category = "VST2 Plugins";
	}

	ostringstream oss;

	oss << "<PluginList>";

	// Wrapper V3
	oss << "<Plugin id=\"" VST_WRAPPER_ID_PREFIX << std::hex << uniqueId << "\" name=\"" << name << "\" category=\"" << category << "/\" >";
	oss << "<Audio/>";
	oss << "<Controller/>";
	oss << "<GUI graphicsApi=\"GmpiGui\" />";
	oss << "</Plugin>";

	oss << "</PluginList>";

	plugins.push_back({ name, uniqueId, oss.str(), fullPath });
}

std::string VstFactory::XmlFromPlugin(AEffectWrapper* plugin)// , int wrapperVersion)
{
	// Gather parameter names.
	auto numParams = plugin->getNumParams();
	vector<string> paramNames;
	for (int i = 0; i < numParams; ++i)
	{
		paramNames.push_back(plugin->getParameterName(i));
	}

	ostringstream oss;

	oss << "<PluginList><Plugin id=\"" VST_WRAPPER_ID_PREFIX << std::hex << plugin->getUniqueID() << "\" name=\"" << plugin->getName() << "\" category=\"VST2 Plugins\" >";

	// Parameter to store state.
	oss << "<Parameters>";

	int i = 0;
/*
	if (wrapperVersion != 3)
	{
		for (auto name : paramNames)
		{
			oss << "<Parameter id=\"" << std::dec << i << "\" name=\"" << name << "\" datatype=\"float\" metadata=\",,1,0\"";
			if (wrapperVersion == 2)
				oss << " persistant=\"false\" ";
			oss << " />";
			++i;
		}
	}
*/
	// next-to last parameter stores state from getChunk() / setChunk().
//	if (wrapperVersion != 1)
	{
		oss << "<Parameter id=\"" << std::dec << i << "\" name=\"chunk\" ignorePatchChange=\"true\" datatype=\"blob\" />";
		++i;
	}

	// Provide parameter to share pointer to plugin.
	oss << "<Parameter id=\"" << std::dec << i << "\" name=\"controllerptr\" ignorePatchChange=\"true\" persistant=\"false\" private=\"true\" datatype=\"blob\" />";

	oss << "</Parameters>";

	// Controller.
	oss << "<Controller/>";

	// Audio.
	oss << "<Audio>";

//	if (wrapperVersion == 3)
	{
		// Add Power and Tempo pins.
		// was 2nd, replaced with rendermode for cancellation testing.	<Pin name = "Auto Sleep" datatype = "bool" isMinimised="true" />
		oss << R"XML(
			<Pin name = "Power/Bypass" datatype = "bool" default = "1" />
			<Pin name = "RenderMode" datatype = "int" hostConnect = "Processor/OfflineRenderMode" />
			<Pin name = "Host BPM" datatype = "float" hostConnect = "Time/BPM" />
			<Pin name = "Host SP" datatype = "float" hostConnect = "Time/SongPosition" />
			<Pin name = "Host Transport" datatype = "bool" hostConnect = "Time/TransportPlaying" />
			<Pin name = "Numerator" datatype = "int" hostConnect = "Time/Timesignature/Numerator" />
			<Pin name = "Denominator" datatype = "int" hostConnect = "Time/Timesignature/Denominator" />
			<Pin name = "Host Bar Start" datatype = "float" hostConnect = "Time/BarStartPosition" />
		)XML";
	}

	auto controllerPointerParamId = i;
	oss << "<Pin name=\"controllerptr\" datatype=\"blob\" parameterId=\"" << controllerPointerParamId << "\" private=\"true\" />";

	if (plugin->CanDo("receiveVstMidiEvent"))
		oss << "<Pin name=\"MIDI In\" direction=\"in\" datatype=\"midi\" />";

	for (int i = 0; i < plugin->getNumInputs(); ++i)
		oss << "<Pin name=\"Signal In\" datatype=\"float\" rate=\"audio\" />";

	for (int i = 0; i < plugin->getNumOutputs(); ++i)
		oss << "<Pin name=\"Signal Out\" direction=\"out\" datatype=\"float\" rate=\"audio\" />";

//	if (wrapperVersion == 3)
	{
		// DSP controls for setting normalised parameter values.
		for (int i = 0; i < plugin->getNumParams(); ++i)
			oss << "<Pin name=\"" << plugin->getParameterName(i) << "\" datatype=\"float\" metadata=\",,1,0\" default=\"-1\"/>";
	}

	oss << "</Audio>";

	// GUI.
	oss << "<GUI graphicsApi=\"GmpiGui\" >";

	// controllerptr ptr first.
	oss << "<Pin name=\"controllerptr\" datatype=\"blob\" parameterId=\"" << controllerPointerParamId << "\" private=\"true\" />";
/*
	if (wrapperVersion != 3)
	{
		// GUI controls to give feedback on parameter real-world values.
		for (auto name : paramNames)
		{
			oss << "<Pin name=\"" << name << "\" datatype=\"float\"  direction=\"out\" metadata=\",,1,0\" />";
			oss << "<Pin name=\"" << name << "\" datatype=\"string\" direction=\"out\" />";
		}

		// GUI pins to get/set parameters.
		for (int i = 0; i < numParams; ++i)
		{
			oss << "<Pin datatype=\"float\" parameterId=\"" << i << "\" />";
		}
	}
*/
	oss << "</GUI>";

	oss << "</Plugin></PluginList>";

	return oss.str();
}

void VstFactory::AddPlugin(/*const std::wstring* full_path,*/ AEffectWrapper* plugin)
{
#if 0
	// Plugin ID's must be unique. Skip multiple waveshells.
	auto uniqueId = plugin->getUniqueID();
	for (auto& p : plugins)
	{
		if (p.uniqueId_ == uniqueId)
			return;
	}

	for (int wrapperVersion = 1; wrapperVersion < 4; ++wrapperVersion)
	{
		plugins.push_back({ plugin->getName(), plugin->getUniqueID(), XmlFromPlugin(plugin, wrapperVersion), fullpath });
	}
#endif
}

// Determine settings file: C:\Users\Jeff\AppData\Local\Plugin\Preferences.xml
std::wstring VstFactory::getSettingFilePath()
{
#if defined(_WIN32)
	wchar_t mySettingsPath[MAX_PATH];
	SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, &(mySettingsPath[0]) );
	std::wstring meSettingsFile( &(mySettingsPath[0]) );
	meSettingsFile += L"/SeVst2Wrapper";

	// Create folder if not already.
	_wmkdir(meSettingsFile.c_str());

	meSettingsFile += L"/ScannedPlugins.xml";

	return meSettingsFile;
#else
	return wstring(L"");
#endif
}

std::string VstFactory::getDiagnostics()
{
	std::ostringstream oss;

	if (pluginPaths.empty())
	{
		oss << "Can't locate plugins *.*\nSearched:\n";

		auto searchPaths = getSearchPaths();
		for (auto path : searchPaths)
		{
			oss << WStringToUtf8(path) << "\n";
		}
	}
	else
	{
		oss << "Shells located:";
		for (auto& it : pluginPaths)
		{
			oss << "\n " << WStringToUtf8(it.second);
		}
		oss << "\n";

		oss << plugins.size() - 1 << " plugins available.\n\n";

		if (!duplicates.empty())
		{
			oss << "Skiped Duplicates:\n";
			for (auto s : duplicates)
			{
				oss << s << "\n";
			}
		}
	}

//	oss << "Can't open VST Plugin. (not a vst plugin). (";
	return oss.str();
}

void VstFactory::savePluginInfo()
{
#if defined(_WIN32)
	ofstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		for( auto& p : plugins )
		{
			myfile << p.name_ << "\n";
			myfile << p.uniqueId_ << "\n";
			myfile << p.xmlBrief_ << "\n";
			myfile << p.fullPath_ << "\n";
		}
		myfile.close();
	}
#endif
}

void VstFactory::loadPluginInfo()
{
#if defined(_WIN32)
	ifstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		string fullpath;
		string name;
		string id;
		string xml;
		std::getline(myfile, name);
		while( !myfile.eof() )
		{
			std::getline(myfile, id);
			std::getline(myfile, xml);
			std::getline(myfile, fullpath);
			plugins.push_back({ name, strtol(id.c_str(), 0, 10), xml, fullpath });
			std::getline(myfile, name);
		}
		myfile.close();
		scannedPlugins = true;
		LocateVstPlugins();
	}
	else
	{
		ScanVsts();
	}
#endif
}

#ifdef _WIN32

// Define the entry point for the DLL
#ifdef _MANAGED
#pragma managed(push, off)
#endif

// temp. testing with MFC included.
#ifndef __AFXWIN_H__

// store dll instance handle, needed when creating windows and window classes.
HMODULE dllInstanceHandle;

extern "C"
__declspec (dllexport)
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved)
{
	dllInstanceHandle = hModule;
	return TRUE;
}
#endif

#ifdef _MANAGED
#pragma managed(pop)
#endif

#endif // _WIN32

//---------------FACTORY --------------------

VstFactory* GetVstFactory()
{
	static VstFactory* theFactory = 0;

	// Initialize on first use.  Ensures the factory is alive before any other object
	// including other global static objects (allows plugins to auto-register).
	if (theFactory == 0)
	{
		theFactory = new VstFactory;
	}

	return theFactory;
}

//  Factory lives until plugin dll is unloaded.
//  this helper destroys the factory automatically.
class factory_deleter_helper
{
public:
	~factory_deleter_helper()
	{
		delete GetVstFactory();
	}
} grim_reaper2;


#if !defined(_M_X64)
// Export additional symbol without name decoration.
#pragma comment( linker, "/export:MP_GetFactory=_MP_GetFactory@4" ) 
#endif

// This is the DLL's main entry point.  It returns the factory.
extern "C"

#ifdef _WIN32
#define VST_EXPORT __declspec (dllexport)
#else

#if defined (__GNUC__)
#define VST_EXPORT	__attribute__ ((visibility ("default")))
#else
#define VST_EXPORT 
#endif

#endif


VST_EXPORT
int32_t MP_STDCALL MP_GetFactory(void** returnInterface)
{
	// call queryInterface() to keep refcounting in sync
	return GetVstFactory()->queryInterface(MP_IID_UNKNOWN, returnInterface);
}

int32_t VstFactory::queryInterface(const MpGuid& iid, void** returnInterface)
{
	if (iid == MP_IID_SHELLFACTORY || iid == MP_IID_FACTORY2 || iid == MP_IID_FACTORY || iid == MP_IID_UNKNOWN)
	{
		*returnInterface = this;
		addRef();
		return gmpi::MP_OK;
	}

	*returnInterface = 0;
	return MP_NOSUPPORT;
}


// Support older hosts. Not likely.
int32_t VstFactory::createInstance(const wchar_t* uniqueId, int32_t subType,
	gmpi::IMpUnknown* host,
	void** returnInterface)
{
	if (!scannedPlugins)
	{
		loadPluginInfo();
	}

	*returnInterface = 0; // if we fail for any reason, default return-val to NULL.

	if (wcscmp(uniqueId, VST_DIAG_WRAPPER_ID_L) == 0)
	{
		if (subType == MP_SUB_TYPE_GUI2)
		{
			auto wp = new AaVstWrapperDiagGui();

			if (host != nullptr)
			{
				wp->setHost(host);
			}

			*returnInterface = static_cast<gmpi_gui_api::IMpGraphics*>(wp);

			return gmpi::MP_OK;
		}
		return gmpi::MP_FAIL;
	}

	int vstUniqueId = 0;
	bool useChunkPresets = true;
	bool useGuiPins = false;
	if( wcslen(uniqueId) > 11 )
	{
		wchar_t* idx;
		vstUniqueId = wcstol(uniqueId + 11, &idx, 16);
	}

	for( auto& pluginInfo : plugins )
	{
		if( pluginInfo.uniqueId_ == vstUniqueId )
		{
			switch( subType )
			{
			case MP_SUB_TYPE_AUDIO:
			{
				auto wp = new Vst2Wrapper(pluginInfo.uniqueId_, useChunkPresets);

				if( host != nullptr )
				{
					wp->setHost(host);
				}

				*returnInterface = static_cast<void*>( wp );

				return gmpi::MP_OK;
				break;
			}

			case MP_SUB_TYPE_CONTROLLER:
			{
				// auto wp = new VST2WrapperController(getShellFromId(pluginInfo.uniqueId_), pluginInfo.uniqueId_, useChunkPresets, useGuiPins);
				auto wp = new VST2WrapperController(
					Utf8ToWstring(pluginInfo.fullPath_),
					pluginInfo.uniqueId_,
					useChunkPresets,
					useGuiPins
				);

				if( host != nullptr )
				{
					wp->setHost(host);
				}

				*returnInterface = static_cast<void*>( wp );

				return gmpi::MP_OK;
				break;
			}

			case MP_SUB_TYPE_GUI2:
			{
				if (pluginPaths.empty())
				{
					break;
				}

				auto wp = new Vst2WrapperGui(pluginInfo.uniqueId_, useGuiPins);

				if( host != nullptr )
				{
					wp->setHost(host);
				}

				*returnInterface = static_cast<gmpi_gui_api::IMpGraphics*>( wp );

				return gmpi::MP_OK;
				break;
			}
			default:
				return gmpi::MP_FAIL;
				break;
			}
		}
	}

	if (subType == MP_SUB_TYPE_GUI2)
	{
		string err("Error");
		if (pluginPaths.empty())
		{
			err = "Can't Locate Shell";
		}
		else
		{
			err = "Can't find Plugin:";
			char buffer[50] = "";
			sprintf(buffer, "%X", vstUniqueId);
			err += buffer;

			bool first = true;
			for (auto& it : pluginPaths)
			{
				if (!first)
				{
					err += ", ";
				}
				err += WStringToUtf8(it.second);
				first = false;
			}
		}

		auto wp = new VstwrapperfailGui(err);

		if (host != nullptr)
		{
			wp->setHost(host);
		}

		*returnInterface = static_cast<gmpi_gui_api::IMpGraphics*>(wp);

		return gmpi::MP_OK;
	}

	return MP_FAIL;
}

int32_t VstFactory::createInstance2(const wchar_t* uniqueId, int32_t subType,
	void** returnInterface)
{
	return createInstance(uniqueId, subType, nullptr, returnInterface);
}

int32_t VstFactory::getSdkInformation(int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation)
{
	returnSdkVersion = GMPI_SDK_REVISION;

	// use safe string printf if possible.
#if defined(_MSC_VER ) && _MSC_VER >= 1400
#if defined( _DEBUG )
	_snwprintf_s(returnCompilerInformation, maxChars, _TRUNCATE, L"MS Compiler V%d (DEBUG)", (int)_MSC_VER);
#else
	_snwprintf_s(returnCompilerInformation, maxChars, _TRUNCATE, L"MS Compiler V%d", (int)_MSC_VER);
#endif
#else
#if defined( __GXX_ABI_VERSION )
	swprintf(returnCompilerInformation, maxChars, L"GCC Compiler V%d", (int)__GXX_ABI_VERSION);
#else
	wcscpy(returnCompilerInformation, L"Unknown Compiler");
#endif
#endif

	return gmpi::MP_OK;
}
