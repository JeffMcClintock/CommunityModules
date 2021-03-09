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
#include "xplatform.h"
#include "string_utilities.h"
#include "xp_dynamic_linking.h"

#if !defined(SE_TARGET_WAVES)
//#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"

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

#define INFO_PLUGIN_ID "SE: VST3 WRAPPER"
#define L_INFO_PLUGIN_ID L"SE: VST3 WRAPPER"

using namespace std;
using namespace gmpi;
using namespace JmUnicodeConversions;

using namespace Steinberg;
using namespace Steinberg::Vst;

const char* VstFactory::pluginIdPrefix = "wvVST3WRAP:";

VstFactory::VstFactory()
{
	Steinberg::gStandardPluginContext = &pluginContext;
}

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
//		pluginIdMap.clear();
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

	if (!scannedPLugins)
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
		std::string reason = "Could not create Module for file:";
		reason += path;
		reason += "\nError: ";
		reason += error;
// Displays message box and quits process.
//		IPlatform::instance ().kill (-1, reason);
		return gmpi::MP_FAIL;
	}

	auto classID = VST3::UID::fromString(uuid);
	if(!classID)
	{
		return gmpi::MP_FAIL;
	}

	auto factory = module->getFactory();
	for (auto& classInfo : factory.classInfos ())
	{
		if (classInfo.ID() == *classID && classInfo.category() == kVstAudioEffectClass)
		{
			const std::string xmlFull = XmlFromPlugin(factory, classInfo);
			returnXml->setData(xmlFull.data(), (int32_t)xmlFull.size());
			return gmpi::MP_OK;
		}
	}
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
    searchPaths.push_back(L"/Library/Audio/Plug-ins/VST3/");
    searchPaths.push_back(L"~/Library/Audio/Plug-ins/VST3/");
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
	scannedPLugins = true;

#if !defined(SE_TARGET_WAVES)
	// Time to re-scan VSTs.
	plugins.clear();
	duplicates.clear();

	// Always add 'Info' plugin
	{
		ostringstream oss;
		oss << "<PluginList><Plugin id=\"" << INFO_PLUGIN_ID << "\" name=\"Wrapper Info\" category=\"VST3 Plugins\" >"
			"<GUI graphicsApi=\"GmpiGui\"><Pin/></GUI>"
			"</Plugin></PluginList>";

		plugins.push_back({ INFO_PLUGIN_ID, "", oss.str() });
	}

	ShallowScanVsts();

	savePluginInfo();

#endif
}

//// Search for Waves shell, stopping at first one found. Not recursive.
//void VstFactory::ScanForWavesShell(const std::wstring& searchPath, const std::wstring& excludePath)
//{
//#if !defined(SE_TARGET_WAVES)
//
//	auto searchMask = searchPath + L"*.vst3";
//	for (FileFinder it = toPlatformString(searchMask).c_str(); !it.done(); ++it)
//	{
//		if (!(*it).isFolder)
//		{
//			auto fullFilename = searchPath + JmUnicodeConversions::toWstring((*it).filename);
////			if (fullFilename.find(L"WaveShell") != std::string::npos && fullFilename.find(L"StudioRack") == std::string::npos && fullFilename.find(excludePath) != 0)
//			{
//				std::string shortFilename = JmUnicodeConversions::toString((*it).filename);
//				pluginIdMap.insert(std::make_pair(shortFilename, fullFilename));
//			}
//		}
//	}
//#endif
//}

void VstFactory::RecursiveScanVsts(const std::wstring& searchPath, const std::wstring& excludePath)
{
#if !defined(SE_TARGET_WAVES)
	if(searchPath.find(excludePath) != std::string::npos)
	{
		return;
	}

	auto searchMask = combinePathAndFile(searchPath, std::wstring(L"*.*"));
	for (FileFinder it = toPlatformString( searchMask ).c_str(); !it.done(); ++it)
	{
		if((*it).isDots())
		{
			continue;
		}

		const auto fullFilename = combinePathAndFile(searchPath, JmUnicodeConversions::toWstring((*it).filename));
		if ((*it).isFolder)
		{
			RecursiveScanVsts(fullFilename, excludePath);
		}
		else
		{
			if ((*it).filename.find(L".vst3") == (*it).filename.size() - 5)
			{
				ScanDll(fullFilename);
			}
		}
	}
#endif
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

	std::string error;
	auto module = VST3::Hosting::Module::create(path, error);
	if (!module)
	{
		std::string reason = "Could not create Module for file:";
		reason += path;
		reason += "\nError: ";
		reason += error;
// Displays message box and quits process.
//		IPlatform::instance ().kill (-1, reason);
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

		category = path.c_str() + lastSlash;
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

	plugins.push_back({ name, uuid, oss.str(), shellPath });
}

std::string VstFactory::XmlFromPlugin(VST3::Hosting::PluginFactory& factory, const VST3::Hosting::ClassInfo& classInfo)
{
	ostringstream oss;

	auto plugProvider = owned (new Steinberg::Vst::PlugProvider (factory, classInfo, true));

	auto editController = owned(plugProvider->getController());
	if (!editController)
	{
//		error = "No EditController found (needed for allowing editor) in file " + path;
//		IPlatform::instance ().kill (-1, error);
		return {};
	}
//	editController->release (); // plugProvider does an addRef

/* not sure what this is
	// set optional component handler on edit controller
	if (flags & kSetComponentHandler)
	{
		SMTG_DBPRT0 ("setComponentHandler is used\n");
		editController->setComponentHandler (&gComponentHandler);
	}
*/

	// TODO!!!: Hide and handle MIDI CC dummy parameters

	// Gather parameter names.
	vector<string> paramNames;
	const auto parameterCount = editController->getParameterCount();
	for(int i = 0; i < parameterCount; ++i)
	{
		Steinberg::Vst::ParameterInfo info{};
		editController->getParameterInfo(i, info);

		paramNames.push_back(WStringToUtf8(info.title));
	}

	oss << "<PluginList><Plugin id=\"" << pluginIdPrefix << classInfo.ID().toString() << "\" name=\"" << classInfo.name() << "\" category=\"VST3 (Waves)\" >";

	// Parameter to store state.
	oss << "<Parameters>";

	int i = 0;
	for (auto name : paramNames)
	{
		oss << "<Parameter id=\"" << std::dec << i << "\" name=\"" << name << "\" datatype=\"float\" metadata=\",,1,0\" />";
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

	// instansiate Processor
//	IAudioProcessor* audioEffect{};
		auto component = owned(plugProvider->getComponent());
		if(!component)
		{
			//		error = "No EditController found (needed for allowing editor) in file " + path;
			//		IPlatform::instance ().kill (-1, error);
			return {};
		}
#if 0

		const int32 numInputs = component->getBusCount (kAudio, kInput);
		const int32 numOutputs = component->getBusCount (kAudio, kOutput);
		const int32 numMidiInputs = component->getBusCount (kEvent, kInput);

//		component->queryInterface(IAudioProcessor::iid, (void**)&audioEffect);
//		component->release();
#endif

	int numInputs{};
	int numOutputs{};
	int numMidiInputs{};

	BusInfo busInfo = {};
	const int busIndex{};
	if (component->getBusInfo (kAudio, kInput, busIndex, busInfo) == kResultTrue)
	{
		numInputs = busInfo.channelCount;
/*
		auto busName = VST3::StringConvert::convert (busInfo.name);

		if (busName.empty ())
		{
//			addErrorMessage (testResult, printf ("Bus %d has no name!!!", busIndex));
			return false;
		}
*/

/*
		addMessage (
			testResult,
			printf ("     %s[%d]: \"%s\" (%s-%s) ", busDirection == kInput ? "In " : "Out",
				    busIndex, busName.data (), busInfo.busType == kMain ? "Main" : "Aux",
				    busInfo.kDefaultActive ? "Default Active" : "Default Inactive"));
*/
	}

	if(component->getBusInfo(kAudio, kOutput, busIndex, busInfo) == kResultTrue)
	{
		numOutputs = busInfo.channelCount;
	}

	if(component->getBusInfo(kEvent, kInput, busIndex, busInfo) == kResultTrue)
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
	)XML";

	auto aeffectPointerParamId = i;
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << aeffectPointerParamId << "\" private=\"true\" />";

	if(numMidiInputs)
	{
		oss << "<Pin name=\"MIDI In\" direction=\"in\" datatype=\"midi\" />";
	}

	// Direct parameter access via MIDI
	oss << "<Pin name=\"Parameter Access\" direction=\"in\" datatype=\"midi\" />";

	for (int i = 0; i < numInputs; ++i)
		oss << "<Pin name=\"Signal In\" datatype=\"float\" rate=\"audio\" />";

	for (int i = 0; i < numOutputs; ++i)
		oss << "<Pin name=\"Signal Out\" direction=\"out\" datatype=\"float\" rate=\"audio\" />";

/* old way
	// DSP controls for setting normalised parameter values.
	for (int i = 0; i < plugin->getNumParams(); ++i)
		oss << "<Pin name=\"" << plugin->getParameterName(i) << "\" datatype=\"float\" metadata=\",,1,0\" default=\"-1\"/>";
*/
	oss << "</Audio>";

	// GUI.
	oss << "<GUI graphicsApi=\"GmpiGui\" >";

	// aeffect ptr first.
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << aeffectPointerParamId << "\" private=\"true\" />";

	oss << "</GUI>";

	oss << "</Plugin></PluginList>";

	return oss.str();
}

// Determine settings file: C:\Users\Jeff\AppData\Local\SeVst3Wrapper\ScannedPlugins.xml
std::wstring VstFactory::getSettingFilePath()
{
#if !defined(SE_TARGET_WAVES) && defined(_WIN32)
	wchar_t mySettingsPath[MAX_PATH];
	SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, &(mySettingsPath[0]) );
	std::wstring meSettingsFile( &(mySettingsPath[0]) );
	meSettingsFile += L"\\SeVst3Wrapper";

	// Create folder if not already.
	_wmkdir(meSettingsFile.c_str());

	meSettingsFile += L"\\ScannedPlugins.xml";

	return meSettingsFile;
#else
	return {};
#endif
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
#if !defined(SE_TARGET_WAVES) && defined(_WIN32)
	ofstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		for( auto& p : plugins )
		{
			myfile << p.name_ << "\n";
			myfile << p.uuid_ << "\n";
			myfile << p.xmlBrief_ << "\n";
			myfile << WStringToUtf8(p.shellPath_) << "\n";
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
		string name, id, xml, path;
		std::getline(myfile, name);
		while( !myfile.eof() )
		{
			std::getline(myfile, id);
			std::getline(myfile, xml);
			std::getline(myfile, path);
			plugins.push_back({ name, id, xml, Utf8ToWstring(path) });

			// next plugin.
			std::getline(myfile, name);
		}
		myfile.close();
		scannedPLugins = true;
		ShallowScanVsts();
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

	const auto vstUniqueId = uuidFromWrapperID(uniqueId);
	const bool useChunkPresets = false;
	const bool useGuiPins = true;

	for( auto& pluginInfo : plugins )
	{
		if( pluginInfo.uuid_ == vstUniqueId )
		{
			switch( subType )
			{
			case MP_SUB_TYPE_AUDIO:
			{
				auto wp = new ProcessorWrapper();// pluginInfo.uuid_, useChunkPresets);

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
				auto wp = new ControllerWrapper(pluginInfo.shellPath_.c_str(), vstUniqueId, useChunkPresets, useGuiPins);

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
		//if (pluginIdMap.empty())
		//{
		//	err = "Can't Locate WavesShell";
		//}
		//else
		{
			err = "Can't find Waves Plugin:" + vstUniqueId;
			err += "\n";

			//bool first = true;
			//for (auto& it : pluginIdMap)
			//{
			//	if (!first)
			//	{
			//		err += ", ";
			//	}
			//	err += WStringToUtf8(it.second);
			//	first = false;
			//}
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
