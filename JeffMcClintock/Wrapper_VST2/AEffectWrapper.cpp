#include "AEffectWrapper.h"
#include "../shared/string_utilities.h"
#include "../shared/unicode_conversion.h"
#include "../shared/xplatform.h"

#if defined(_WIN32)
#include "NativeWindow_Win32.h"
#endif

using namespace std;
using namespace gmpi_sdk;
using namespace JmUnicodeConversions;

typedef	AEffect* VST_PLUGIN_MAIN(audioMasterCallback audioMaster);

AEffectWrapper* AEffectWrapper::currentLoadingVst = 0;
CriticalSectionXp AEffectWrapper::currentLoadingVstLock;

// There must be only 1 of these, belonging to App
//	This is the link from the plugin back to the container
// I redirect the query to my VST_Wrapper class
//long VSTCALLBACK vst2_callback(AEffect* effect, long opcode, long index, long value, void* ptr, float opt)
VstIntPtr VSTCALLBACK vst2_callback(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
	AEffectWrapper* wrapper = 0;

	// effect == 0, may be in process of loading (plugin pointer not set yet ).
	if (effect)
	{
		// not 64-bit wrapper = (AEffectWrapper*)effect->resvd1;
		wrapper = *reinterpret_cast<AEffectWrapper**>( &( effect->resvd1 ) );
	}

	if( wrapper == 0 )
	{
		wrapper = AEffectWrapper::currentLoadingVst;
	}

	return wrapper->callback(opcode, index, value, ptr, opt);
}

// plugin calling host.
intptr_t AEffectWrapper::callback(int opcode, int index, intptr_t value, void* ptr, float opt)
{
//#define DEBUG_CALLBACK
#ifdef DEBUG_CALLBACK
//		if( opcode != audioMasterPinConnected && opcode != audioMasterGetTime )
			_RPT0(_CRT_WARN, "VST_Wrapper::callback( " );
#endif

	intptr_t ret = 0;

	switch (opcode)
	{
		case audioMasterAutomate:		// index, value, returns 0
									//		_RPT2(_CRT_WARN, "audioMasterAutomate) %d,%.3f\n", index, opt );
									// check that plugin is updating it's internal state correctly
									//_RPT2(_CRT_WARN, "PLUGIN> %f  disp %s\n", getParameter(index), getParameterDisplay(index) );
									// feedback to plugin
									//no		Request Idle(); // seems nesc for blueline plugins thread safe- yea checked in se_app
									// Update vst parameter
		for( auto o : observers )
			o->hUpdateParam(index, opt);

		ret = 0;
		break;

	case audioMasterVersion:		// vst version,  (for example 2200 for VST 2.2) (0 for older)
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterVersion)");
#endif
		ret = 2300;
		break;

	case audioMasterCurrentId:		// returns the unique id of a plug that's currently
									// loading
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterCurrentId)");
#endif
		ret = currentLoadingVstId_;
		break;

	case audioMasterIdle:			// call application idle routine (this will
									// call effEditIdle for all open editors too)
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterIdle)");
#endif
//		RequestIdle(); // !!thread safe???
		ret = 0;
		break;
/*deprecated
	case audioMasterPinConnected:	// inquire if an input or output is beeing connected;
									// index enumerates input or output counting from zero,
									// value is 0 for input and != 0 otherwise. note: the
									// return value is 0 for <true> such that older versions
									// will always return true.
									// can overload debug window..		_RPT2(_CRT_WARN, "audioMasterPinConnected) %d %d", value, index );
		ret = 0;
		break;

	case audioMasterWantMidi:	// plug want's midi, <value> is a filter which is currently ignored
		_RPT0(_CRT_WARN, "audioMasterWantMidi)");
		ret = 1;
		break;
		*/
	case audioMasterGetTime:		// returns const VstTimeInfo* (or 0 if not supported)
									// <value> should contain a mask indicating which fields are required
									// (see valid masks above), as some items may require extensive
									// conversions
									//		_RPT0(_CRT_WARN, "audioMasterGetTime)" );
		ret = (intptr_t)host_->hHostGetTime((int) value);
		break;

	case audioMasterProcessEvents:		// VstEvents* in <ptr>
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterProcessEvents)");
#endif
		host_->hProcessPlugsEvents((VstEvents*)ptr);
		ret = 0;
		break;
		/*deprecated

	case audioMasterSetTime:				// VstTimenfo* in <ptr>, filter in <value>, not supported
	{
		_RPT0(_CRT_WARN, "audioMasterSetTime)");
		VstTimeInfo* ti = (VstTimeInfo*)ptr;
		host_->hHostSetTime(ti);
		ret = 1;
	}
	break;

	case audioMasterTempoAt:				// returns tempo (in bpm * 10000) at sample frame location passed in <value>
		_RPT0(_CRT_WARN, "audioMasterTempoAt)");
		//		ret = 0;
		ret = 1200000;
		break;

		// parameters
	case audioMasterGetNumAutomatableParameters:
		_RPT0(_CRT_WARN, "audioMasterGetNumAutomatableParameters)");
		ret = 0;
		break;

	case audioMasterGetParameterQuantization:	// returns the integer value for +1.0 representation,
												// or 1 if full single float precision is maintained
												// in automation. parameter index in <value> (-1: all, any)
		_RPT0(_CRT_WARN, "audioMasterGetParameterQuantization)");
		ret = 1;
		break;
*/
		// connections, configuration
	case audioMasterIOChanged:				// numInputs and/or numOutputs has changed
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterIOChanged)");
#endif
		host_->hIOChanged();
		ret = 0;
		break;
		/*deprecated

	case audioMasterNeedIdle:				// plug needs idle calls (outside its editor window)
		_RPT0(_CRT_WARN, "audioMasterNeedIdle)");
		//m_effect_idle_enabled = true;
		//RequestIdle(); // !!thread safe???
					   / *
					   > There are two kind of idle functions: one is the editor idle, and the
					   other
					   > is the effect idle.
					   > You obtain calls to your editor derived class idle() member function only
					   > when your editor is open (seems natural, isn't it?). You can base your
					   > graphical behaviour on that function (for example: blinking indicators),
					   but
					   > when the editor gets closed, the idle() function is not called anymore.
					   >
					   > On the other hand, the effect derived class has its own idle function
					   called
					   > fxIdle(). You have to ask the host for repeatedly calling fxIdle() by
					   > calling needIdle(). The VST documentation states that you have to call
					   > needIdle() from within your resume() member function.
					   >
					   > fxIdle() gets called even if the editor is closed, but you have to be
					   > careful because fxIdle() calls, on Macintosh, happen in interrupt time so
					   > you can't allocate memory, open files, update the display, ...
					   > If you have to execute a sort of cyclic work even when the editor is
					   closed,
					   > you have to place your code in fxIdle() but all you can do from there is
					   > read and write memory (get/set variable values).
					   > You can "connect" the effect fxIdle() behaviours to the editor idle() ones
					   > through some state flags set by fxIdle() and read (and reset) by idle(),
					   > obtaining (simulating) a real ever-working idle() function.
					   >
					   > Alfonso De Prisco
					   * /
		ret = 0;
		break;
		*/
	case audioMasterSizeWindow:				// index: width, value: height
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterSizeWindow)");
#endif
		ret = 0;
		break;

	case audioMasterGetSampleRate:
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetSampleRate)");
#endif
		ret = (int) host_->hSampleRate();
		break;

	case audioMasterGetBlockSize:
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetBlockSize)");
#endif
		ret = host_->hGetBlockSize();
		break;

	case audioMasterGetInputLatency:
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetInputLatency)");
#endif
		ret = 0;
		break;

	case audioMasterGetOutputLatency:
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetOutputLatency)");
#endif
		ret = 0;
		break;
		/*deprecated

	case audioMasterGetPreviousPlug:			// input pin in <value> (-1: first to come), returns cEffect*
		_RPT0(_CRT_WARN, "audioMasterGetPreviousPlug)");
		ret = 0;
		break;

	case audioMasterGetNextPlug:				// output pin in <value> (-1: first to come), returns cEffect*
		_RPT0(_CRT_WARN, "audioMasterGetNextPlug)");
		ret = 0;
		break;

		// realtime info
	case audioMasterWillReplaceOrAccumulate:	// returns: 0: not supported, 1: replace, 2: accumulate
		_RPT0(_CRT_WARN, "audioMasterWillReplaceOrAccumulate)");
		ret = 1;
		break;
		*/
	case audioMasterGetCurrentProcessLevel:	// returns: 0: not supported,
											// 1: currently in user thread (gui)
											// 2: currently in audio thread (where process is called)
											// 3: currently in 'sequencer' thread (midi, timer etc)
											// 4: currently offline processing and thus in user thread
											// other: not defined, but probably pre-empting user thread.
//		_RPT0(_CRT_WARN, "audioMasterGetCurrentProcessLevel)");
//		ret = 2;
		ret = host_->getProcessLevel();
		break;

	case audioMasterGetAutomationState:		// returns 0: not supported, 1: off, 2:read, 3:write, 4:read/write
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetAutomationState)");
#endif
		ret = 1;
		break;

	case audioMasterOfflineStart:
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterOfflineStart)");
#endif
		ret = 0;
		break;

	case audioMasterOfflineRead:				// ptr points to offline structure, see below. return 0: error, 1 ok
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterOfflineRead)");
#endif
		ret = 0;
		break;

	case audioMasterOfflineWrite:			// same as read
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterOfflineWrite)");
#endif
		ret = 0;
		break;

	case audioMasterOfflineGetCurrentPass:
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterOfflineGetCurrentPass)");
#endif
		ret = 0;
		break;

	case audioMasterOfflineGetCurrentMetaPass:
		// other
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterOfflineGetCurrentMetaPass)");
#endif
		ret = 0;
		break;

/* deprecated in 2.4
	case audioMasterSetOutputSampleRate:		// for variable i/o, sample rate in <opt>
		_RPT0(_CRT_WARN, "audioMasterSetOutputSampleRate)");
		ret = 0;
		break;
	case audioMasterGetSpeakerArrangement:	// (long)input in <value>, output in <ptr>
		_RPT0(_CRT_WARN, "audioMasterGetSpeakerArrangement)");
		ret = 0;
		break;
*/
	case audioMasterGetVendorString:			// fills <ptr> with a string identifying the vendor (max 64 char)
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetVendorString)");
#endif
		strcpy((char*)ptr, "SynthEdit Ltd");
		ret = 0;
		break;

	case audioMasterGetProductString:		// fills <ptr> with a string with product name (max 64 char)
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetProductString)");
#endif
		strcpy((char*)ptr, "SynthEditVST2");
		ret = 1;
		break;

	case audioMasterGetVendorVersion:		// returns vendor-specific version
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetVendorVersion)");
#endif
		ret = 1;
		break;

	case audioMasterVendorSpecific:			// no definition, vendor specific handling
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterVendorSpecific)");
#endif
		ret = 0;
		break;
		/*deprecated

	case audioMasterSetIcon:					// void* in <ptr>, format not defined yet
		_RPT0(_CRT_WARN, "audioMasterSetIcon)");
		ret = 0;
		break;
*/
	case audioMasterCanDo:					// string in ptr, see below. 1='cando', 0='don't know' (default), -1='No'
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterCanDo)");
#endif
		{

			char* canDoString = (char*)ptr;

			ret = 
				!strcmp(canDoString, "sendVstEvents") ||
				!strcmp(canDoString, "sendVstMidiEvent") ||
				!strcmp(canDoString, "sendVstTimeInfo") ||
				!strcmp(canDoString, "receiveVstEvents") ||
				!strcmp(canDoString, "receiveVstMidiEvent") ||
				//			!strcmp(canDoString, "reportConnectionChanges") ||
				!strcmp(canDoString, "acceptIOChanges") ||
				//			!strcmp(canDoString, "sizeWindow") ||
				//			!strcmp(canDoString, "offline") ||
				//			!strcmp(canDoString, "startStopProcess") ||
				!strcmp(canDoString, "shellCategory");
		}
		break;

	case audioMasterGetLanguage:				// see enum
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetLanguage)");
#endif
		ret = kVstLangEnglish;
		break;
		/*deprecated

	case audioMasterOpenWindow:				// returns platform specific ptr
		_RPT0(_CRT_WARN, "audioMasterOpenWindow)");
		ret = 0;
		break;

	case audioMasterCloseWindow:				// close window, platform specific handle in <ptr>
		_RPT0(_CRT_WARN, "audioMasterCloseWindow)");
		ret = 0;
		break;
*/
	case audioMasterGetDirectory:			// get plug directory, FSSpec on MAC, else char*
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterGetDirectory)");
#endif
		{
			ret = (intptr_t)pluginDirectory_.c_str();
		}
		break;

	case audioMasterUpdateDisplay:			// something has changed, update 'multi-fx' display
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterUpdateDisplay)");
#endif
		host_->hUpdateDisplay();

		for( auto o : observers )
			o->hUpdateDisplay();

		ret = 1;
		break;

		//---from here VST 2.1 extension opcodes------------------------------------------------------
	case audioMasterBeginEdit:               // begin of automation session (when mouse down), parameter index in <index>
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterBeginEdit)");
#endif
		break;

	case audioMasterEndEdit:                 // end of automation session (when mouse up),     parameter index in <index>
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterEndEdit)");
#endif
		break;

	case audioMasterOpenFileSelector:		// open a fileselector window with VstFileSelect* in <ptr>
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterOpenFileSelector)");
#endif
		{
			//VstFileSelect* fs = (VstFileSelect*)ptr;
			std::string ext;
			std::string fname;
			host_->hFileDialog(true, ext, fname);
//			strcpy(fname, (char*)ptr, 200);
		}
		break;

		//---from here VST 2.2 extension opcodes------------------------------------------------------
	case audioMasterCloseFileSelector:		// close a fileselector operation with VstFileSelect* in <ptr>: Must be always called after an open !
#ifdef DEBUG_CALLBACK
		_RPT0(_CRT_WARN, "audioMasterCloseFileSelector)");
#endif
		break;
		/*deprecated

	case audioMasterEditFile:				// open an editor for audio (defined by XML text in ptr)
		_RPT0(_CRT_WARN, "audioMasterEditFile)");
		ret = 0;
		break;

	case audioMasterGetChunkFile:			// get the native path of currently loading bank or project
											// (called from writeChunk) void* in <ptr> (char[2048], or sizeof(FSSpec))
		_RPT0(_CRT_WARN, "audioMasterGetChunkFile)");
		ret = 0;
		break;

		//---from here VST 2.3 extension opcodes------------------------------------------------------
	case audioMasterGetInputSpeakerArrangement:	// result a VstSpeakerArrangement in ret
		_RPT0(_CRT_WARN, "audioMasterGetInputSpeakerArrangement)");
		ret = 0;
		break;
*/
	default:
#ifdef DEBUG_CALLBACK
		_RPT1(_CRT_WARN, "Unsupported : %d ) !!", (int)opcode);
#endif
		ret = 0;
		break;
	}

#ifdef DEBUG_CALLBACK
	_RPT0(_CRT_WARN, "\n");
#endif

	return ret;
}

AEffectWrapper::AEffectWrapper() :
	hinstLib(0)
	, currentLoadingVstId_(0)
	, host_(&dummyHost_)
	, comWasInit_(false)
	, editorWindow(0)
	, aeffect_(0)
{
}


AEffectWrapper::~AEffectWrapper()
{
//	dispatcher(effMainsChanged, 0, 0); // stop processing - suspend()
//	_RPT1(_CRT_WARN, "VST2: Close and unload dll. thread = %d\n", GetCurrentThreadId() );

// Print parameter settings, for comparison with prototype.
#if 0 // defined(_DEBUG)
	if (aeffect_)
	{
		_RPT0(_CRT_WARN, "-----------------------\n");
		_RPT1(_CRT_WARN, "%s\n", getName().c_str());
		for (int i = 0; i < getNumParams(); ++i)
		{
			_RPT4(_CRT_WARN, "%3d,%s,%f,%s\n", i, getParameterName(i).c_str(), getParameter(i), getParameterDisplay(i).c_str());
		}
		_RPT0(_CRT_WARN, "-----------------------\n");
	}
#endif

#if defined(_WIN32)
    if( editorWindow != nullptr )
	{
		editorWindow->Close();
	}
#else
    host_->onAEffectWrapperDestroyed();
#endif
	dispatcher(effClose);
#if defined(_WIN32)
    if( comWasInit_ )
	{
		auto r = CoInitialize(0); // Test if COM STILL initialised.
		if( r == S_FALSE )
		{
			// as-expected. UnInit.
			CoUninitialize();
		}
		else // else leave COM initialised to compensate for buggy plugin.
		{
			_RPT0(_CRT_WARN, "~AEffectWrapper(): COM Uninitialized by VST Plugin!\n");
		}

		comWasInit_ = false; // we're even.
	}
#endif
}

void AEffectWrapper::InitFromShell(gmpi_dynamic_linking::DLL_HANDLE dllHandle, const std::wstring& load_filename, VstIntPtr shellPluginId)
{
	VST_PLUGIN_MAIN* Plugin_Entrypoint;
	MP_DllSymbol(dllHandle, "VSTPluginMain", (void**)&Plugin_Entrypoint);

	if (Plugin_Entrypoint == 0)
	{
		MP_DllSymbol(dllHandle, "VstPluginMain()", (void**)&Plugin_Entrypoint);

		if (Plugin_Entrypoint == 0)
		{
			MP_DllSymbol(dllHandle, "main", (void**)&Plugin_Entrypoint);
		}
	}

	// If the function address is valid, call the function.
	if (Plugin_Entrypoint != 0)
	{
		aeffect_ = 0;
		{
			AutoCriticalSection cs(currentLoadingVstLock); // Serialize multi-threaded instansiations accessing static variable.
			currentLoadingVst = this;
			currentLoadingVstId_ = shellPluginId;
			filename_ = load_filename;
			pluginDirectory_ = JmUnicodeConversions::WStringToUtf8(StripFilename(filename_));

			// call plugin main() function, will instansiate a AudioEffectX object and rtn ptr
			aeffect_ = (Plugin_Entrypoint)(&vst2_callback);

			if (aeffect_ && aeffect_->magic == kEffectMagic)
			{
				*reinterpret_cast<AEffectWrapper**>(&(aeffect_->resvd1)) = this; // provide hook back to this object.
				currentLoadingVst = 0;
				return;
			}

			aeffect_ = 0; // not a VST plugin.
			currentLoadingVst = 0;
			currentLoadingVstId_ = 0;
			filename_.clear();
			pluginDirectory_.clear();
		}

		//if (aeffect_)
		//{
		//	return;
		//}
		//else
		{
			/*
			m_filename = _T("");
			//				err_msg.Format(_T("Can't open VST Plugin. (not a vst plugin). (%s)."), filename);
			std::wostringstream oss;
			oss << L"Can't open VST Plugin. (not a vst plugin). (" << filename << L").";
			err_msg = oss.str();
			*/
		}
	}
	else
	{
		/*
		//			err_msg.Format(_T("Can't open VST Plugin. (Plugin Entrypoint call failed). (%s)."), filename);
		std::wostringstream oss;
		oss << L"Can't open VST Plugin. (Plugin Entrypoint call failed). (" << filename << L").";
		err_msg = oss.str();
		*/
	}
}

void AEffectWrapper::LoadDll(const std::wstring& load_filename, VstIntPtr shellPluginId)
{
	aeffect_ = 0; // indicates "not loaded".
    
#if defined(_WIN32)
	//	_RPT1(_CRT_WARN, "VST2: load dll. thread = %d\n", GetCurrentThreadId());
	// Nasty hack!!!
	// As of 12-08-2015 Waves shell plugins incorrectly unitialise COM, which screws up WPF BADLY (SynthEdit).
	if( load_filename.find(L"WaveShell" ) != string::npos )
	{
		auto r =  CoInitialize(0); // Test if COM initialised.
		comWasInit_ = r != S_OK;
		if( r == S_OK || r == S_FALSE ) // Undo my initiliase.
		{
			CoUninitialize();
		}
	}
#endif
    
    VST_PLUGIN_MAIN* Plugin_Entrypoint = 0;

    if (hinstLib == 0)
	{
        MP_DllLoad(&hinstLib, load_filename.c_str());
    }
    // If the handle is valid, try to get the function address.
    if (hinstLib != 0)
    {
		{
			typedef	bool VST_DLL_INIT();
			VST_DLL_INIT* dll_init = {};
			MP_DllSymbol(hinstLib, "InitDll", (void**)&dll_init);
			if (dll_init)
			{
				dll_init();
			}
		}

        MP_DllSymbol(hinstLib, "VSTPluginMain", (void**) &Plugin_Entrypoint);
        if (Plugin_Entrypoint == 0)
        {
            MP_DllSymbol(hinstLib, "VstPluginMain()", (void**) &Plugin_Entrypoint);
            if (Plugin_Entrypoint == 0)
            {
// seems dangerous                MP_DllSymbol(hinstLib, "main", (void**) &Plugin_Entrypoint);
            }
        }

        // If the function address is valid, call the function.
        if (Plugin_Entrypoint != 0)
        {
            {
                AutoCriticalSection cs(currentLoadingVstLock);// Serialize multi-threaded instansiations accessing static variable.
                currentLoadingVst = this;
                currentLoadingVstId_ = shellPluginId;
                filename_ = load_filename;
                pluginDirectory_ = JmUnicodeConversions::WStringToUtf8(StripFilename(filename_));

                // call plugin main() function, will instansiate a AudioEffectX object and rtn ptr
                aeffect_ = (Plugin_Entrypoint)( &vst2_callback );

				if (aeffect_ && aeffect_->magic == kEffectMagic)
                {
                    *reinterpret_cast<AEffectWrapper**>( &( aeffect_->resvd1 ) ) = this; // provide hook back to this object.
                }
				else
				{
					aeffect_ = 0; // not a VST plugin.
					filename_.clear();
					pluginDirectory_.clear();
				}

				currentLoadingVst = 0;
				currentLoadingVstId_ = 0;
				return;
            }

            //if (aeffect_)
            //{
            //	return;
            //}
            //else
            {
                /*
                m_filename = _T("");
                //				err_msg.Format(_T("Can't open VST Plugin. (not a vst plugin). (%s)."), filename);
                std::wostringstream oss;
                oss << L"Can't open VST Plugin. (not a vst plugin). (" << filename << L").";
                err_msg = oss.str();
                */
            }
        }
        else
        {
            /*
            //			err_msg.Format(_T("Can't open VST Plugin. (Plugin Entrypoint call failed). (%s)."), filename);
            std::wostringstream oss;
            oss << L"Can't open VST Plugin. (Plugin Entrypoint call failed). (" << filename << L").";
            err_msg = oss.str();
             */
        }
    }
    else
    {
        /*
        // some problem
        DWORD err_code = GetLastError();
        LPVOID lpMsgBuf;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            err_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (LPTSTR)&lpMsgBuf,
            0,
            NULL
            );
        //		err_msg.Format(_T("Can't load VST Plugin(%s). (%s)."), lpMsgBuf, load_filename);
        std::wostringstream oss;
        oss << L"Can't load VST Plugin(" << lpMsgBuf << L"). (." << filename << L").";
        err_msg = oss.str();
        LocalFree(lpMsgBuf);
        */
    }
}

void AEffectWrapper::Resume()
{
	// set rate and block size both before and after open, same as cubase V5
	dispatcher(effSetSampleRate, 0, 0, 0, host_->hSampleRate() );
	dispatcher(effSetBlockSize, 0, host_->hGetBlockSize());
	dispatcher(effMainsChanged, 0, 1); // power on - resume()
	dispatcher(effSetSampleRate, 0, 0, 0, host_->hSampleRate());
	dispatcher(effSetBlockSize, 0, host_->hGetBlockSize());
}

int AEffectWrapper::InstansiatePlugin(long shellPluginId)
{
	currentLoadingVstId_ = shellPluginId;

	return 0;
}

VstIntPtr AEffectWrapper::ShellGetNextPlugin(string& returnName)
{
	char nameBuffer[500];
	memset(nameBuffer, 0, sizeof(nameBuffer)); // cope with lack of NULL terminators in some plugins by setting all extra bytes NUll.

	auto returnUniqueId = dispatcher(effShellGetNextPlugin, 0, 0, nameBuffer, 0.0f);
	/*
	if (returnUniqueId == 0 || nameBuffer[0] == '\0')
	{
		returnUniqueId = 0;
		return 0;
	}
	*/

	returnName = nameBuffer;

	return returnUniqueId;
}

std::string AEffectWrapper::getName()
{
	char name[500]; // larger than required to prevent problems
	memset(name, 0, sizeof(name)); // cope with lack of NULL terminators in some plugins by setting all extra bytes NUll.

	if (GetVstVersion() > 1)
	{
		dispatcher(effGetEffectName, 0, 0, name);
	}

	std::string n(name);

	if (n.empty())
	{
		n = StripPath(JmUnicodeConversions::WStringToUtf8(filename_));
		n = StripExtension(n);
	}

	return n;
}

void AEffectWrapper::OpenEditor()
{
#if defined(_WIN32)
    if( !IsLoaded() )
	{
		return;
	}

	if( editorWindow == nullptr )
	{
		editorWindow = new NativeWindow_Win32(this);
		editorWindow->Open();

		dispatcher(effEditOpen, 0, 0, editorWindow->hwnd_);
	}
	else
	{
		editorWindow->ToFront();
	}
#endif
}

void AEffectWrapper::OnCloseEditor()
{
	if( editorWindow != 0 )
	{
		dispatcher(effEditClose);
		editorWindow = 0;
	}
}
