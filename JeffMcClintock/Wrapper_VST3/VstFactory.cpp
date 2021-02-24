#include "VstFactory.h"
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <assert.h>
#include "mp_sdk_common.h"
#include "unicode_conversion.h"
//#include "AEffectWrapper.h"
//#include "./Vst2Wrapper.h"
#include "xplatform.h"
#include "string_utilities.h"
#include "xp_dynamic_linking.h"

#if !defined(SE_TARGET_WAVES)
#include "FileFinder.h"
//#include "./Vst2WrapperGui.h"
//#include "VST2WrapperController.h"
#include "./VstwrapperfailGui.h"
#include "./AaVstWrapperDiagGui.h"

#if defined(_WIN32)
#include "shlobj.h"
#endif

#endif

#define INFO_PLUGIN_ID "SE: VST3 WRAPPER"
#define L_INFO_PLUGIN_ID L"SE: VST3 WRAPPER"

using namespace std;
using namespace gmpi;
using namespace JmUnicodeConversions;

// IMpShellFactory: Query a plugin's info. Should occur only during a user-initiated re-scan.
int32_t VstFactory::getPluginIdentification(int32_t index, IMpUnknown* iReturnXml)
{
#if !defined(SE_TARGET_WAVES)

	IString* returnString = 0;

	if (MP_OK != iReturnXml->queryInterface(MP_IID_RETURNSTRING, reinterpret_cast<void**>( &returnString)))
	{
		return gmpi::MP_NOSUPPORT;
	}

	if (index == 0) // User-initiated rescan.
	{
		wavesShellLocations.clear();
		ScanVsts();
	}
	else
	{
		if (!scannedPLugins)
		{
			loadPluginInfo();
		}
	}

	if (index >= 0 && index < (int)plugins.size())
	{
		returnString->setData(plugins[index].xmlBrief_.data(), (int32_t)plugins[index].xmlBrief_.size());
		return gmpi::MP_OK;
	}
#endif

	return gmpi::MP_FAIL;
}

int32_t VstFactory::getPluginInformation(const wchar_t* uniqueId, IMpUnknown* iReturnXml)
{
#if !defined(SE_TARGET_WAVES)
	IString* returnXml = 0;

	if (MP_OK != iReturnXml->queryInterface(MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnXml)))
	{
		return gmpi::MP_NOSUPPORT;
	}

	if (!scannedPLugins)
	{
		loadPluginInfo();
	}

	int vstUniqueId = 0;
	if (wcslen(uniqueId) > 11)
	{
		wchar_t* idx;
		vstUniqueId = wcstol(uniqueId + 11, &idx, 16);
	}

	assert(false);
/* TODO !!!
	// load plugin, get full XML.
	AEffectWrapper ae;

	ae.LoadDll(getShellFromId(vstUniqueId), vstUniqueId);
	
	if (!ae.IsLoaded())
	{
		return gmpi::MP_FAIL;
	}


	ae.dispatcher(effOpen);
	std::string xmlFull = XmlFromPlugin(&ae, wrapperVersion);

	returnXml->setData(xmlFull.data(), (int32_t)xmlFull.size());

	return gmpi::MP_OK;
	*/
#endif

	return gmpi::MP_FAIL;
}

#if !defined(SE_TARGET_WAVES)

vector< std::wstring >getSearchPaths()
{
	vector< std::wstring >searchPaths;

#ifdef _WIN32
	// Use standard VST3 folder.
	wchar_t myPath[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, SHGFP_TYPE_CURRENT, myPath);
	std::wstring commonFilesFolder(myPath);

	// "C:\Program Files (x86)\Common Files\VST3
	searchPaths.push_back(commonFilesFolder + L"\\VST3\\");
#else
    searchPaths.push_back(L"/Library/Audio/Plug-ins/VST3/");
    searchPaths.push_back(L"~/Library/Audio/Plug-ins/VST3/");
#endif
    
	return searchPaths;
}
#endif

void VstFactory::LocateWavesShell()
{
#if !defined(SE_TARGET_WAVES)
	// TODO: !!! In the case of multiple WaveShells, use NEWEST one (biggest version number). !!!

	// Time to re-scan VSTs.
	std::wstring excludeMyself = StripFilename(gmpi_dynamic_linking::MP_GetDllFilename());

	auto searchPaths = getSearchPaths();
	for (auto itSp = searchPaths.begin(); itSp != searchPaths.end(); ++itSp)
	{
		ScanForWavesShell(*itSp, excludeMyself); // Will use only first WavesShell.
	}
#endif
}

void VstFactory::ScanVsts()
{
	LocateWavesShell();

	scannedPLugins = true;

#if !defined(SE_TARGET_WAVES)
	// Time to re-scan VSTs.
	plugins.clear();
	duplicates.clear();

	// Always add 'Info' plugin
	{
		ostringstream oss;
		oss << "<PluginList><Plugin id=\"" << INFO_PLUGIN_ID << "\" name=\"Wrapper Info\" category=\"VST3 Plugins\" >";
		oss << "<GUI graphicsApi=\"GmpiGui\"><Pin/></GUI>";
		oss << "</Plugin></PluginList>";

		plugins.push_back({ INFO_PLUGIN_ID, "", oss.str() });
	}

	for (auto& i : wavesShellLocations)
	{
		ScanDll(i.second, i.first.c_str());
	}
	savePluginInfo();

#endif
}

// Search for Waves shell, stopping at first one found. Not recursive.
void VstFactory::ScanForWavesShell(const std::wstring& searchPath, const std::wstring& excludePath)
{
#if !defined(SE_TARGET_WAVES)
//	float bestVersion = 0;

	auto searchMask = searchPath + L"*.vst3";
	for (FileFinder it = toPlatformString(searchMask).c_str(); !it.done(); ++it)
	{
		if (!(*it).isFolder)
		{
			auto fullFilename = searchPath + JmUnicodeConversions::toWstring((*it).filename);
			if (fullFilename.find(L"WaveShell") != std::string::npos && fullFilename.find(L"StudioRack") == std::string::npos && fullFilename.find(excludePath) != 0)
			{
				/*
				auto p1 = fullFilename.find_first_of(L"0123456789");
				auto p2 = fullFilename.find_first_not_of(L"0123456789.", p1);
				float version = stof(fullFilename.substr(p1, p2 - p1));

				if (version > bestVersion)
				{
					bestVersion = version;
					wavesShellLocation = fullFilename;
				}
				*/

				std::string shortFilename = JmUnicodeConversions::toString((*it).filename);
				wavesShellLocations.insert(std::make_pair(shortFilename, fullFilename));
			}
		}
	}
#endif
}

/*
void VstFactory::RecursiveScanVsts(const std::wstring& searchPath, const std::wstring& excludePath)
{
#if !defined(SE_TARGET_WAVES)
	auto searchMask = searchPath + L"*.dll";
	for (FileFinder it = toPlatformString( searchMask ).c_str(); !it.done(); ++it)
	{
		if ((*it).isFolder)
		{
			RecursiveScanVsts(searchPath + JmUnicodeConversions::toWstring((*it).filename), excludePath);
		}
		else
		{
			auto fullFilename = searchPath + JmUnicodeConversions::toWstring((*it).filename);
			if (fullFilename.find(excludePath) != 0)
			{
				ScanDll(fullFilename);
			}
		}
	}
#endif
}
*/

struct backgroundData
{
	VstFactory* factory;
	const std::wstring* full_path;
	const char* shellName;
};

#if !defined(SE_TARGET_WAVES)

void VstFactory::ScanDll(const std::wstring /*platform_string*/& full_path, const char* shellName)
{
	if (full_path.find(L"WaveShell") == std::string::npos)
	{
		return; // Only interested in Wavesshell.
	}


	assert(false); // TODO!!!
#if 0
	AEffectWrapper plugin;
	plugin.LoadDll(full_path);

	if (plugin.IsLoaded())
	{
		plugin.dispatcher(effOpen);

		bool isShellPlugin = plugin.dispatcher(effGetPlugCategory, 0, 0, NULL, 0.0f) == kPlugCategShell;

		if (isShellPlugin)
		{
			//			plugin.dispatcher(effOpen); // Assume we need to open shell?

			vector < pair<int32_t, string > > pluginList;
			int index = 0;
			VstIntPtr uniqueId = 0;
			string name;

			// Waves5 continues with the next plug after the last loaded
			// that's not what we want - workaround: swallow all remaining
			//while (uniqueId = plugin.ShellGetNextPlugin(name)) {}

#if 1 // quick name-only scan.
			do
			{
				uniqueId = plugin.ShellGetNextPlugin(name);

				if (uniqueId != 0)
				{
					AddPluginName(shellName, uniqueId, name);
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
			// Standard VST. Will never happen in Waves.
			AddPlugin(/* dat->full_path, */&plugin);
		}

		//		plugin.dispatcher(effClose);
	}
#endif
}
#endif

void VstFactory::AddPluginName(const char* shellName, std::string uuid, const std::string& name)
{
	// Plugin ID's must be unique. Skip multiple waveshells.
	for (auto& p : plugins)
	{
		if (p.uuid_ == uuid)
		{
			ostringstream oss2;
			oss2 << std::hex << uuid << ":" << p.name_ <<", " << name;

			duplicates.push_back(oss2.str());
			return;
		}
	}

	ostringstream oss;

	oss << "<PluginList>";

	// Wrapper
	oss << "<Plugin id=\"wvVST2WRAP:" << uuid << "\" name=\"" << name << "\" category=\"" << shellName << "/\" >";
	oss << "<Audio/>";
	oss << "<Controller/>";
	oss << "<GUI graphicsApi=\"GmpiGui\" />";
	oss << "</Plugin>";

	oss << "</PluginList>";

	plugins.push_back(pluginInfo(name, uuid, oss.str()));
}

std::string VstFactory::XmlFromPlugin(void/*AEffectWrapper*/* plugin)
{
	assert(false); // TODO!!!
	ostringstream oss;
#if 0
	// Gather parameter names.
	auto numParams = plugin->getNumParams();
	vector<string> paramNames;
	for (int i = 0; i < numParams; ++i)
	{
		paramNames.push_back(plugin->getParameterName(i));
	}


	oss << "<PluginList><Plugin id=\"wvVST2WRAP:" << std::hex << plugin->getUniqueID() << "\" name=\"" << plugin->getName() << "\" category=\"VST2 (Waves)\" >";

	// Parameter to store state.
	oss << "<Parameters>";

	int i = 0;
	for (auto name : paramNames)
	{
		oss << "<Parameter id=\"" << std::dec << i << "\" name=\"" << name << "\" datatype=\"float\" metadata=\",,1,0\"";
		if (wrapperVersion == 2)
			oss << " persistant=\"false\" ";
		oss << " />";
		++i;
	}

	// next-to last parameter stores state from getChunk() / setChunk().
	oss << "<Parameter id=\"" << std::dec << i << "\" name=\"chunk\" ignorePatchChange=\"true\" datatype=\"blob\" />";
	++i;

	// Provide parameter to share pointer to plugin.
	oss << "<Parameter id=\"" << std::dec << i << "\" name=\"effectptr\" ignorePatchChange=\"true\" persistant=\"false\" private=\"true\" datatype=\"blob\" />";

	oss << "</Parameters>";

	// Controller.
	oss << "<Controller/>";

	// Audio.
	oss << "<Audio>";

	// Add Power and Tempo pins.
	oss << R"XML(
		<Pin name = "Power/Bypass" datatype = "bool" default = "1" />
		<Pin name = "Auto Sleep" datatype = "bool" isMinimised="true" />
		<Pin name = "Host BPM" datatype = "float" hostConnect = "Time/BPM" />
		<Pin name = "Host SP" datatype = "float" hostConnect = "Time/SongPosition" />
		<Pin name = "Host Transport" datatype = "bool" hostConnect = "Time/TransportPlaying" />
		<Pin name = "Numerator" datatype = "int" hostConnect = "Time/Timesignature/Numerator" />
		<Pin name = "Denominator" datatype = "int" hostConnect = "Time/Timesignature/Denominator" />
		<Pin name = "Host Bar Start" datatype = "float" hostConnect = "Time/BarStartPosition" />
	)XML";

	auto aeffectPointerParamId = i;
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << aeffectPointerParamId << "\" private=\"true\" />";

	if (plugin->CanDo("receiveVstMidiEvent"))
		oss << "<Pin name=\"MIDI In\" direction=\"in\" datatype=\"midi\" />";

	for (int i = 0; i < plugin->getNumInputs(); ++i)
		oss << "<Pin name=\"Signal In\" datatype=\"float\" rate=\"audio\" />";

	for (int i = 0; i < plugin->getNumOutputs(); ++i)
		oss << "<Pin name=\"Signal Out\" direction=\"out\" datatype=\"float\" rate=\"audio\" />";

	// DSP controls for setting normalised parameter values.
	for (int i = 0; i < plugin->getNumParams(); ++i)
		oss << "<Pin name=\"" << plugin->getParameterName(i) << "\" datatype=\"float\" metadata=\",,1,0\" default=\"-1\"/>";

	oss << "</Audio>";

	// GUI.
	oss << "<GUI graphicsApi=\"GmpiGui\" >";

	// aeffect ptr first.
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << aeffectPointerParamId << "\" private=\"true\" />";

	oss << "</GUI>";

	oss << "</Plugin></PluginList>";
#endif
	return oss.str();
}

void VstFactory::AddPlugin(/*const std::wstring* full_path,*/ AEffectWrapper* plugin)
{
	assert(false); // TODO!!!
#if 0
	// Plugin ID's must be unique. Skip multiple waveshells.
	auto uniqueId = plugin->getUniqueID();
	for (auto& p : plugins)
	{
		if (p.uniqueId_ == uniqueId)
			return;
	}

	plugins.push_back(pluginInfo(/**full_path,*/ plugin->getName(), plugin->getUniqueID(), XmlFromPlugin(plugin)));
#endif
}

// Determine settings file: C:\Users\Jeff\AppData\Local\Plugin\Preferences.xml
std::wstring VstFactory::getSettingFilePath()
{
#if !defined(SE_TARGET_WAVES) && defined(_WIN32)
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

#if !defined(SE_TARGET_WAVES)
std::string VstFactory::getDiagnostics()
{
	std::ostringstream oss;

	if (wavesShellLocations.empty())
	{
		oss << "Can't locate *WaveShell*.*\nSearched:\n";

		auto searchPaths = getSearchPaths();
		for (auto path : searchPaths)
		{
			oss << WStringToUtf8(path) << "\n";
		}
	}
	else
	{
		oss << "WaveShells located:";
		bool first = true;
		for (auto& it : wavesShellLocations)
		{
			if (!first)
			{
				oss << ", ";
			}
			oss << WStringToUtf8(it.second);
			first = false;
		}
		oss << "\n";

		oss << plugins.size() - 1 << " Waves plugins available.\n\n";

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
#endif

void VstFactory::savePluginInfo()
{
#if !defined(SE_TARGET_WAVES) && defined(_WIN32)
	ofstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		for( auto& p : plugins )
		{
			myfile << p.name_ << "\n";
			myfile << p.uuid_ << "\n";
			myfile << p.xmlBrief_ << "\n";
		}
		myfile.close();
	}
#endif
}

void VstFactory::loadPluginInfo()
{
#if !defined(SE_TARGET_WAVES) && defined(_WIN32)
	ifstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		string name;
		string id;
		string xml;
		std::getline(myfile, name);
		while( !myfile.eof() )
		{
			std::getline(myfile, id);
			std::getline(myfile, xml);
			plugins.push_back(pluginInfo(name, id, xml));
			std::getline(myfile, name);
		}
		myfile.close();
		scannedPLugins = true;
		LocateWavesShell();
	}
	else
	{
		ScanVsts();
	}
#endif
}

#if !defined(SE_TARGET_WAVES)

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
#endif // !SE_TARGET_WAVES

//---------------FACTORY --------------------

VstFactory* GetVstFactory()
{
	// Initialize on first use.  Ensures the factory is alive before any other object
	// including other global static objects (allows plugins to auto-register).
	static VstFactory* theFactory = new VstFactory;

	return theFactory;
}

#if !defined(SE_TARGET_WAVES)
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

#endif // !SE_TARGET_WAVES

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

#if !defined(SE_TARGET_WAVES)
std::wstring VstFactory::getShellFromId(const std::string& uuid)
{
	for (auto& p : plugins)
	{
		if (p.uuid_ == uuid)
		{
			auto p1 = p.xmlBrief_.find("category");
			p1 = p.xmlBrief_.find_first_of('\"', p1) + 1;
			auto p2 = p.xmlBrief_.find_first_of("/\"", p1);
			auto shellName = p.xmlBrief_.substr(p1, p2 - p1);

			for (auto& s : wavesShellLocations)
			{
				if (s.first == shellName)
				{
					return s.second;
				}
			}
		}
	}
	return L"";
}
#endif

// Support older version of SE which pass host at construction.
int32_t VstFactory::createInstance(
	const wchar_t* uniqueId,
	int32_t subType,
	gmpi::IMpUnknown* host,
	void** returnInterface
)
{
	if (!scannedPLugins)
	{
		loadPluginInfo();
	}

	*returnInterface = 0; // if we fail for any reason, default return-val to NULL.

#if !defined(SE_TARGET_WAVES)
	if (wcscmp(uniqueId, L_INFO_PLUGIN_ID) == 0)
	{
		if (subType == MP_SUB_TYPE_GUI2)
		{
			auto wp = new AaVstWrapperDiagGui();

			if (host)
			{
				wp->setHost(host);
			}

			*returnInterface = static_cast<gmpi_gui_api::IMpGraphics*>(wp);

			return gmpi::MP_OK;
		}
		return gmpi::MP_FAIL;
	}
#endif

	int vstUniqueId = 0;
	bool useChunkPresets = false;
	bool useGuiPins = true;
	if( wcslen(uniqueId) > 11 )
	{
		wchar_t* idx;
		vstUniqueId = wcstol(uniqueId + 11, &idx, 16);

		useChunkPresets = uniqueId[0] != L'S';
		useGuiPins = uniqueId[0] != L'w';
	}

	for( auto& pluginInfo : plugins )
	{
assert(false); // !!! TODO
#if 0
		if( pluginInfo.uuid_ == vstUniqueId )
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

#if !defined(SE_TARGET_WAVES)
			case MP_SUB_TYPE_CONTROLLER:
			{
				auto wp = new VST2WrapperController(getShellFromId(pluginInfo.uniqueId_), pluginInfo.uniqueId_, useChunkPresets, useGuiPins);

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
				if (wavesShellLocations.empty())
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
#endif
			default:
				return gmpi::MP_FAIL;
				break;
			}
		}
#endif
	}

#if !defined(SE_TARGET_WAVES)
	if (subType == MP_SUB_TYPE_GUI2)
	{
		string err("Error");
		if (wavesShellLocations.empty())
		{
			err = "Can't Locate WavesShell";
		}
		else
		{
			err = "Can't find Waves Plugin:";
			char buffer[50] = "";
			sprintf(buffer, "%X", vstUniqueId);
			err += buffer;
			//err += "\nShell:" + WStringToUtf8(wavesShellLocation);

			bool first = true;
			for (auto& it : wavesShellLocations)
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
#endif

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
