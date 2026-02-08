#include "VstFactory.h"
#include <filesystem>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <assert.h>
#include "mp_sdk_common.h"
#include "unicode_conversion.h"
#include "xplatform.h"
#include "string_utilities.h"
#include "xp_dynamic_linking.h"

#include "myPluginProvider.h"

#if !defined(SE_TARGET_WAVES)
#include "FileFinder.h"
#include "./EditButtonGui.h"
#include "ControllerWrapper.h"
#include "ProcessorWrapper.h"
#include "./VstwrapperfailGui.h"
#include "./AaVstWrapperDiagGui.h"

#if defined(_WIN32)
#include "shlobj.h"
#endif

#endif

#define L_INFO_PLUGIN_ID L"SE: VST3 WRAPPER"
#define INFO_PLUGIN_ID "SE: VST3 WRAPPER"
#define PARAM_SET_PLUGIN_ID "SE: VST3 Param Set"
#define L_PARAM_SET_PLUGIN_ID L"SE: VST3 Param Set"

using namespace std;
using namespace gmpi;
using namespace JmUnicodeConversions;

using namespace Steinberg;
using namespace Steinberg::Vst;

const char* VstFactory::pluginIdPrefix = "wvVST3WRAP:";

// IMpShellFactory: Query a plugin's info. Should occur only during a user-initiated re-scan.
int32_t VstFactory::getPluginIdentification(int32_t index, IMpUnknown* iReturnXml)
{
#if !defined(SE_TARGET_WAVES)

	gmpi::IString* returnString = 0;

	if (MP_OK != iReturnXml->queryInterface(MP_IID_RETURNSTRING, reinterpret_cast<void**>( &returnString)))
	{
		return gmpi::MP_NOSUPPORT;
	}

	if (index == 0) // User-initiated rescan.
	{
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
#endif

	return gmpi::MP_FAIL;
}

std::string VstFactory::uuidFromWrapperID(const wchar_t* uniqueId)
{
	constexpr int prefixLength = 11;
	assert(strlen(pluginIdPrefix) >= prefixLength);

	if(wcslen(uniqueId) <= prefixLength)
	{
		return {};
	}

	return WStringToUtf8( uniqueId + prefixLength );
}

int32_t VstFactory::getPluginInformation(const wchar_t* uniqueId, IMpUnknown* iReturnXml)
{
#if !defined(SE_TARGET_WAVES)
	gmpi::IString* returnXml{};

	if (MP_OK != iReturnXml->queryInterface(MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnXml)))
	{
		return gmpi::MP_NOSUPPORT;
	}

	if (!scannedPlugins)
	{
		loadPluginInfo();
	}

	const auto uuid = uuidFromWrapperID(uniqueId);

	std::string path;
	for (auto& p : plugins)
	{
		if(p.uuid_ == uuid)
		{
			path = WStringToUtf8(p.shellPath_);
			break;
		}
	}

	std::string error;
	auto module = VST3::Hosting::Module::create (path, error);
	if (!module)
	{
		// Could not create Module for file
		return gmpi::MP_FAIL;
	}

	auto classID = VST3::UID::fromString(uuid);
	if(!classID)
	{
		return gmpi::MP_FAIL;
	}

	auto factory = module->getFactory();
	const std::string xmlFull = XmlFromPlugin(factory, *classID);
	returnXml->setData(xmlFull.data(), (int32_t)xmlFull.size());
	return xmlFull.empty() ? gmpi::MP_FAIL : gmpi::MP_OK;
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
	searchPaths.push_back(commonFilesFolder + L"\\VST3");
#else
    searchPaths.push_back(L"/Library/Audio/Plug-Ins/VST3");
    searchPaths.push_back(L"~/Library/Audio/Plug-Ins/VST3");
#endif
    
	return searchPaths;
}
#endif

void VstFactory::ShallowScanVsts()
{
#if !defined(SE_TARGET_WAVES)
	std::wstring excludeMyself = StripFilename(gmpi_dynamic_linking::MP_GetDllFilename());

	auto searchPaths = getSearchPaths();
	for (auto itSp = searchPaths.begin(); itSp != searchPaths.end(); ++itSp)
	{
		RecursiveScanVsts(*itSp, excludeMyself);
	}
#endif
}

void VstFactory::ScanVsts()
{
	scannedPlugins = true;

#if !defined(SE_TARGET_WAVES)
	// Time to re-scan VSTs.
	plugins.clear();
	duplicates.clear();

	// Always add 'Info' plugin. xml must be continuous line, no line breaks.
	{
		ostringstream oss;
		oss << "<PluginList><Plugin id=\"" << INFO_PLUGIN_ID << "\" name=\"Wrapper Info\" category=\"VST3 Plugins\" >"
			"<GUI graphicsApi=\"GmpiGui\"><Pin/></GUI>"
			"</Plugin></PluginList>";

		plugins.push_back({ INFO_PLUGIN_ID, "Info", oss.str(), L"" });
	}
	{
		const auto xml =
R"xml(
<PluginList>
<Plugin id="SE: VST3 Param Set" name="VST3 Param Set" category="VST3 Plugins">
<Audio>
<Pin name="Signal In" datatype="float" />
<Pin name="Param Idx" datatype="int" default="0" />
<Pin name="ParamBuss" datatype="midi" direction="out" />
</Audio>
</Plugin>
</PluginList>
)xml"; 
		plugins.push_back({ PARAM_SET_PLUGIN_ID, "Param Set", xml, L"" });
	}

	ShallowScanVsts();

	savePluginInfo();

#endif
}

void VstFactory::RecursiveScanVsts(const std::wstring& searchPath, const std::wstring& excludePath)
{
	if(searchPath.find(excludePath) != std::string::npos)
		return;

	for (auto& p : std::filesystem::directory_iterator(searchPath))
	{
		auto path = p.path();

		if (path.extension().string() == ".vst3")
		{
			if (p.is_directory()) // handle bundles.
			{
#ifdef _WIN32
				path = path / "Contents" / "x86_64-win";

				// scan fist file in there.
				for (auto& exe_path : std::filesystem::directory_iterator(path))
				{
					//if (exe_path. .is_dots())
					//	continue;

					ScanDll(exe_path.path().wstring());
					break;
				}
#else
                ScanDll(path.wstring());
#endif
            }
#ifdef _WIN32
			else // handle standalone dlls on Windows.
			{
				ScanDll(path.wstring());
			}
#endif
		}
		else
		{
			if (p.is_directory())
			{
				RecursiveScanVsts(path.wstring(), excludePath);
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

#if !defined(SE_TARGET_WAVES)

void VstFactory::ScanDll(const std::wstring /*platform_string*/& full_path)
{
	const auto path = WStringToUtf8(full_path);

	VST3::Hosting::Module::Ptr module = {};
	try
	{
		std::string error;
		module = VST3::Hosting::Module::create(path, error);
		if (!module)
		{
			// Could not create Module for file
			return;
		}

		const char* category = "VST3 Plugins";
		const bool isWavesShell = path.find("WaveShell") != std::string::npos;
		if(isWavesShell)
		{
			auto lastSlash = path.find_last_of('/');
			if(lastSlash == std::string::npos)
			{
				lastSlash = path.find_last_of('\\');
			}
			if(lastSlash == std::string::npos)
			{
				lastSlash = 0;
			}

			category = path.c_str() + lastSlash + 1;
		}

		auto factory = module->getFactory ();
		for (auto& classInfo : factory.classInfos ())
		{
			if (classInfo.category () == kVstAudioEffectClass)
			{
				AddPluginName(category, classInfo.ID().toString(), classInfo.name(), full_path); // a quick scan of name-only.
			}
		}
	}
	catch (...) // PACE protected plugins will throw if dongle not present.
	{
		return;
	}
}
#endif

// For shallow scan, just record name and id.
void VstFactory::AddPluginName(const char* category, std::string uuid, const std::string& name, const std::wstring& shellPath)
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
	oss <<
		"<PluginList>"
			"<Plugin id=\"" << pluginIdPrefix << uuid << "\" name=\"" << name << "\" category=\"" << category << "/\" >"
			"<Audio/>"
			"<Controller/>"
			"<GUI graphicsApi=\"GmpiGui\" />"
			"</Plugin>"
		"</PluginList>";

	plugins.push_back({ uuid, name, oss.str(), shellPath });
}

std::string VstFactory::XmlFromPlugin(VST3::Hosting::PluginFactory& factory, const VST3::UID& classId)
{
	myPluginProvider plugProvider;
	plugProvider.setup(factory, classId);

	if (!plugProvider.controller)
	{
		//No EditController found (needed for allowing editor)
		return {};
	}

	// TODO!!!: Hide and handle MIDI CC dummy parameters
#if 0
	// Gather parameter names.
	vector<string> paramNames;
	const auto parameterCount = plugProvider.controller->getParameterCount();
	for(int i = 0; i < parameterCount; ++i)
	{
		Steinberg::Vst::ParameterInfo info{};
		plugProvider.controller->getParameterInfo(i, info);

		paramNames.push_back(WStringToUtf8(info.title));
	}
#endif

	auto classIdString = classId.toString();

	std::string name;
	for (auto& p : plugins)
	{
		if (p.uuid_ == classIdString)
		{
			name = p.name_;
			break;
		}
	}

	ostringstream oss;
	oss << "<PluginList><Plugin id=\"" << pluginIdPrefix << classIdString << "\" name=\"" << name << "\" category=\"VST3 (Waves)\" >";

	// Parameter to store state.
	oss << "<Parameters>";

	int i = 0;
#if 0
	for (auto name : paramNames)
	{
		oss << "<Parameter id=\"" << std::dec << i << "\" name=\"" << name << "\" datatype=\"float\" metadata=\",,1,0\" />";
		++i;
	}
#endif

	// next-to last parameter stores state from getChunk() / setChunk().
	oss << "<Parameter id=\"" << std::dec << i << "\" name=\"chunk\" ignorePatchChange=\"true\" datatype=\"blob\" />";
	++i;

	// Provide parameter to share pointer to plugin.
	oss << "<Parameter id=\"" << std::dec << i << "\" name=\"effectptr\" ignorePatchChange=\"true\" persistant=\"false\" private=\"true\" datatype=\"blob\" />";

	oss << "</Parameters>";

	// Controller.
	oss << "<Controller/>";

	// instansiate Processor
	if(!plugProvider.component)
	{
		// No EditController found (needed for allowing editor)
		return {};
	}

	int numInputs{};
	int numOutputs{};
	int numMidiInputs{};

	BusInfo busInfo = {};
	const int busIndex{};
	if (plugProvider.component->getBusInfo (kAudio, kInput, busIndex, busInfo) == kResultTrue)
	{
		numInputs = busInfo.channelCount;
	}

	if(plugProvider.component->getBusInfo(kAudio, kOutput, busIndex, busInfo) == kResultTrue)
	{
		numOutputs = busInfo.channelCount;
	}

	if(plugProvider.component->getBusInfo(kEvent, kInput, busIndex, busInfo) == kResultTrue)
	{
		numMidiInputs = busInfo.channelCount;
	}

	// Audio.
	oss << "<Audio>";

	// Add Power and Tempo pins.
	// TODO !!! revise these, need auto sleep? it was a bit buggy i think
//		<Pin name = "Auto Sleep" datatype = "bool" isMinimised="true" />

	oss << R"XML(
		<Pin name = "Power/Bypass" datatype = "bool" default = "1" />
		<Pin name = "Host BPM" datatype = "float" hostConnect = "Time/BPM" />
		<Pin name = "Host SP" datatype = "float" hostConnect = "Time/SongPosition" />
		<Pin name = "Host Transport" datatype = "bool" hostConnect = "Time/TransportPlaying" />
		<Pin name = "Numerator" datatype = "int" hostConnect = "Time/Timesignature/Numerator" />
		<Pin name = "Denominator" datatype = "int" hostConnect = "Time/Timesignature/Denominator" />
		<Pin name = "Host Bar Start" datatype = "float" hostConnect = "Time/BarStartPosition" />
		<Pin name = "" datatype = "int" hostConnect = "Processor/OfflineRenderMode" />
	)XML";

	auto controllerPointerParamId = i;
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << controllerPointerParamId << "\" private=\"true\" />";

	if(numMidiInputs)
	{
		oss << "<Pin name=\"MIDI In\" direction=\"in\" datatype=\"midi\" />";
	}

	// Direct parameter access via MIDI
	oss << "<Pin name=\"ParamBuss\" direction=\"in\" datatype=\"midi\" />";

	for (int i = 0; i < numInputs; ++i)
		oss << "<Pin name=\"Signal In\" datatype=\"float\" rate=\"audio\" />";

	for (int i = 0; i < numOutputs; ++i)
		oss << "<Pin name=\"Signal Out\" direction=\"out\" datatype=\"float\" rate=\"audio\" />";

#if 0
	// DSP pins for receiving normalised parameter values from GUI.
	for(int i = 0; i < parameterCount; ++i)
	{
		oss << "<Pin datatype=\"float\" parameterId=\"" << i << "\" />";
	}
#endif

	oss << "</Audio>";

	// GUI.
	oss << "<GUI graphicsApi=\"GmpiGui\" >";

	// aeffect ptr first.
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << controllerPointerParamId << "\" private=\"true\" />";

	oss << "</GUI>";

	oss << "</Plugin></PluginList>";

	return oss.str();
}

// Determine settings file: C:\Users\Jeff\AppData\Local\SeVst3Wrapper\ScannedPlugins.xml
std::filesystem::path VstFactory::getSettingFilePath()
{
	std::filesystem::path settingsDir;

#if defined(_WIN32)
	wchar_t appDataPath[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataPath);
	settingsDir = std::filesystem::path(appDataPath);
#else // macOS
	const char* home = getenv("HOME");
	if (!home)
		return {};
	settingsDir = std::filesystem::path(home) / "Library" / "Application Support";
#endif

	settingsDir /= "SeVst3Wrapper";

	// Create folder if not already.
	std::filesystem::create_directories(settingsDir);

	return settingsDir / "ScannedPlugins.xml";
}

#if !defined(SE_TARGET_WAVES)
std::string VstFactory::getDiagnostics()
{
	std::ostringstream oss;
/* todo
	if (pluginIdMap.empty())
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
		for (auto& it : pluginIdMap)
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
*/
//	oss << "Can't open VST Plugin. (not a vst plugin). (";
	return oss.str();
}
#endif

void VstFactory::savePluginInfo()
{
	ofstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		for( auto& p : plugins )
		{
			auto xml = p.xmlBrief_;
			// need to strip out newlines for single-line storage.
			xml.erase(std::remove(xml.begin(), xml.end(), '\n'), xml.end());

			myfile << p.name_ << "\n";
			myfile << p.uuid_ << "\n";
			myfile << xml << "\n";
			myfile << WStringToUtf8(p.shellPath_) << "\n";
		}
		myfile.close();
	}
}

void VstFactory::loadPluginInfo()
{
#if !defined(SE_TARGET_WAVES)
	ifstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		string name, id, xml, path;
		std::getline(myfile, name);
		while( !myfile.eof() )
		{
			std::getline(myfile, id);
			std::getline(myfile, xml);
			std::getline(myfile, path);
			plugins.push_back({ id, name, xml, Utf8ToWstring(path) });

			// next plugin.
			std::getline(myfile, name);
		}
		myfile.close();
		scannedPlugins = true;
// no, otherwise loading only one VST3 ends up scanning them all:		ShallowScanVsts();
	}
	else
	{
		ScanVsts();
	}
#endif
}

#if !defined(SE_TARGET_WAVES) // TODO!!! try to replace this, so the wrapper is universal, mayby have runtime detection of context (in a wavesplugin of not)

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

// Support older version of SE which pass host at construction.
int32_t VstFactory::createInstance(
	const wchar_t* uniqueId,
	int32_t subType,
	gmpi::IMpUnknown* host,
	void** returnInterface
)
{
	if (!scannedPlugins)
	{
		loadPluginInfo();
	}

	*returnInterface = nullptr; // if we fail for any reason, default return-val to NULL.

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

	if (wcscmp(uniqueId, L_PARAM_SET_PLUGIN_ID) == 0)
	{
		if (subType == MP_SUB_TYPE_AUDIO)
		{
			auto wp = new Vst3ParamSet();

			if (host)
			{
				wp->setHost(host);
			}

			*returnInterface = static_cast<gmpi::IMpPlugin2*>(wp);

			return gmpi::MP_OK;
		}
		return gmpi::MP_FAIL;
	}
	const auto vstUniqueId = uuidFromWrapperID(uniqueId);
//	const bool useGuiPins = true;

	for( auto& pluginInfo : plugins )
	{
		if( pluginInfo.uuid_ == vstUniqueId )
		{
			switch( subType )
			{
			case MP_SUB_TYPE_AUDIO:
			{
				auto wp = new ProcessorWrapper();

				if( host != nullptr )
				{
					wp->setHost(host);
				}

				*returnInterface = static_cast<void*>( wp );

				return gmpi::MP_OK;
			}
			break;

#if !defined(SE_TARGET_WAVES)
			case MP_SUB_TYPE_CONTROLLER:
			{
				const auto uniqueIdU = WStringToUtf8(uniqueId); // TODO minimize all this string conversions !!!!
				auto wp = new ControllerWrapper(pluginInfo.shellPath_.c_str(), vstUniqueId);

				if( host != nullptr )
				{
					wp->setHost(host);
				}

				*returnInterface = static_cast<void*>( wp );

				return gmpi::MP_OK;
			}
			break;
#endif

			case MP_SUB_TYPE_GUI2:
			{
				auto wp = new EditButtonGui();

				if(host)
				{
					wp->setHost(host);
				}

				*returnInterface = static_cast<gmpi_gui_api::IMpGraphics*>( wp );

				return gmpi::MP_OK;
			}
			break;

			default:
				return gmpi::MP_FAIL;
				break;
			}
		}
	}

#if !defined(SE_TARGET_WAVES)
	if (subType == MP_SUB_TYPE_GUI2)
	{
		string err("Error");
		{
			err = "Can't find VST3 Plugin:" + vstUniqueId;
			err += "\n";
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
