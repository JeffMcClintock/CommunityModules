#pragma once

/*
#include "AEffectWrapper.h"
*/

#include <vector>
#include "../shared/xplatform.h"
#include "../shared/xp_dynamic_linking.h"
#include "../shared/xp_critical_section.h"
#include "../se_sdk3/mp_api.h"
#include "./VST3 SDK/aeffectx.h"

using namespace gmpi_dynamic_linking;

class IVST2Host
{
public:
	virtual float MP_STDCALL hSampleRate(void) = 0;
	virtual int MP_STDCALL hGetBlockSize(void) = 0;
	virtual VstTimeInfo* MP_STDCALL hHostGetTime(int mask) = 0;
	virtual void MP_STDCALL hProcessPlugsEvents(struct VstEvents* p_vst_events) = 0;
	virtual void MP_STDCALL hRequestIdle(void){};
	virtual int MP_STDCALL hFileDialog(bool load_or_save, const std::string& extension, std::string& filename) = 0;
	virtual void MP_STDCALL hCancelIdle(void){};
	virtual int MP_STDCALL hHostSetTime(struct VstTimeInfo* /*p_time_info*/)
	{
		return 0;
	}
	virtual void MP_STDCALL hUpdateParam(int /*index*/, float /*value*/) {};
	virtual void MP_STDCALL hUpdateDisplay() {}; // patch change or whatever
	virtual void MP_STDCALL hIOChanged() {};
    virtual void MP_STDCALL onAEffectWrapperDestroyed() {};
    virtual int MP_STDCALL getProcessLevel() {return 2;} // 2: currently in audio thread
};

class DummyHost : public IVST2Host
{
	VstTimeInfo vstTime_;
public:

	virtual float MP_STDCALL hSampleRate(void)
	{
		return 44100;
	}
	virtual int MP_STDCALL hGetBlockSize(void)
	{
		return 512;
	}
	virtual VstTimeInfo* MP_STDCALL hHostGetTime(int /*mask*/)
	{
		memset(&vstTime_, 0, sizeof(vstTime_));
		vstTime_.sampleRate = 44100;
		vstTime_.tempo = 100;
		vstTime_.timeSigDenominator = 4;
		vstTime_.timeSigNumerator = 4;
		vstTime_.flags = kVstTempoValid | kVstTimeSigValid | kVstBarsValid | kVstPpqPosValid;
		return &vstTime_;
	}
	virtual void MP_STDCALL hProcessPlugsEvents(struct VstEvents* /*p_vst_events*/){}
	virtual void MP_STDCALL hRequestIdle(void){}
	virtual int MP_STDCALL hFileDialog(bool /*load_or_save*/, const std::string& /*extension*/, std::string& /*filename*/){ return 0; }
};

class IVstObserver
{
public:
	virtual void hUpdateParam(int index, float value) = 0;
	virtual void hUpdateDisplay() = 0;
};

// References a vst2 plugin instance without ownership.
class IAEffect
{
public:
	virtual VstIntPtr MP_STDCALL dispatcher(int opCode, int index = 0, int value = 0, void* ptr = NULL, float opt = 0.0) = 0;
	virtual void MP_STDCALL setHost(IVST2Host* host) = 0;
	virtual void MP_STDCALL releaseHost() = 0;
	virtual int32_t MP_STDCALL getNumInputs() = 0;
	virtual int32_t MP_STDCALL getNumOutputs() = 0;
	virtual void MP_STDCALL Resume() = 0;
	virtual void MP_STDCALL setParameter(int index, float value) = 0;
	virtual void MP_STDCALL processReplacing(float** inputs, float** outputs, long sampleFrames, VstEvents* events) = 0;
	virtual bool MP_STDCALL IsLoaded() = 0;
};

// Owns a vst2 plugin instance.
class AEffectWrapper : public IAEffect
{

protected:
	AEffect* aeffect_;
	IVST2Host* host_;
	DummyHost dummyHost_;
	gmpi_dynamic_linking::DLL_HANDLE hinstLib;
	std::wstring filename_;
	VstIntPtr currentLoadingVstId_;
	std::string pluginDirectory_;
	bool comWasInit_;
	class NativeWindow_Win32* editorWindow;

public:
	static AEffectWrapper* currentLoadingVst;
	static gmpi_sdk::CriticalSectionXp currentLoadingVstLock;

	AEffectWrapper();
	virtual ~AEffectWrapper();

	virtual VstIntPtr MP_STDCALL dispatcher(int opCode, int index = 0, int value = 0, void* ptr = NULL, float opt = 0.0)
	{
		if (aeffect_)
		{
			return aeffect_->dispatcher(aeffect_, opCode, index, value, ptr, opt);
		}

		return 0;
	}

	virtual void MP_STDCALL setParameter(int index, float value)
	{
		// _RPT2(_CRT_WARN, "AEffectWrapper::setParameter: %3d %f\n", index, value);
		aeffect_->setParameter(aeffect_, index, value);
	}

	inline float getParameter(int index)
	{
		return aeffect_->getParameter(aeffect_, index);
	}

	virtual void MP_STDCALL processReplacing(float** inputs, float** outputs, long sampleFrames, VstEvents* events)
	{
		if (events->numEvents != 0)
		{
			aeffect_->dispatcher(aeffect_, effProcessEvents, 0, 0, events, 0);
		}

		aeffect_->processReplacing(aeffect_, inputs, outputs, (VstInt32)sampleFrames);
	}

	inline long getUniqueID()
	{
		return aeffect_->uniqueID;
	}
	inline long getNumPrograms()
	{
		return aeffect_->numPrograms;
	}
	inline long getNumParams()
	{
		return aeffect_->numParams;
	}
	inline VstInt32 getLatency()
	{
		return aeffect_->initialDelay;
	}

	virtual int32_t MP_STDCALL getNumInputs()
	{
		return aeffect_->numInputs;
	}
	virtual int32_t MP_STDCALL getNumOutputs()
	{
		return aeffect_->numOutputs;
	}

	bool CanDo(const std::string p_querystring)
	{
		// effect returns -1 No, 0 -Don't know, 1 -Yes
		return dispatcher(effCanDo, 0, 0, (void*)p_querystring.c_str(), 0) == 1;
	}

	// get param name as string eg. "Volume"
	std::string getParameterName(int index)
	{
		char temp[500];
		memset(temp, 0, sizeof(temp)); // cope with lack of NULL terminators in some plugins by setting all extra bytes NUll.
		/*auto r =*/ dispatcher(effGetParamName, index, 0, temp);// stuff parameter <index> label (max 8 char + 0) into string

		return std::string(temp);
	}
	std::string getParameterDisplay(int index)
	{
		char temp[500];
		memset(temp, 0, sizeof(temp)); // cope with lack of NULL terminators in some plugins by setting all extra bytes NUll.
		/*auto r =*/ dispatcher(effGetParamDisplay, index, 0, temp);// stuff parameter <index> label (max 8 char + 0) into string

		return std::string(temp);
	}

	VstIntPtr GetVstVersion()
	{
		/*
		0 => for VST 1.0
		2 => for VST 2.0
		2100 => for VST 2.1
		2200 => for VST 2.2
		2300 => for VST 2.3
		*/
		return dispatcher(effGetVstVersion, 0, 0);
	}

	// Allow process thread to take over hosting.
	virtual void MP_STDCALL setHost(IVST2Host* host)
	{
		host_ = host;
	}
	virtual void MP_STDCALL releaseHost()
	{
		host_ = &dummyHost_;
	}
	
	void LoadDll(const std::wstring& load_filename, VstIntPtr shellPluginId = 0);
	void InitFromShell(gmpi_dynamic_linking::DLL_HANDLE dllHandle, const std::wstring& load_filename, VstIntPtr shellPluginId);

	void registerObserver(IVstObserver* pObserver)
	{
		observers.push_back(pObserver);
	}
	void unRegisterObserver(IVstObserver* pObserver)
	{
		auto it = find(observers.begin(), observers.end(), pObserver);
		if( it != observers.end() )
			observers.erase(it);
	}
	//	void LoadDllBackgroundAndResume(IVST2Host* host, const platform_string& load_filename, long shellPluginId = 0);
	virtual bool MP_STDCALL IsLoaded()
	{
		return aeffect_ != 0;
	}
	virtual void MP_STDCALL Resume();

	VstIntPtr ShellGetNextPlugin(std::string& returnName);
	int InstansiatePlugin(long shellPluginId = 0);

	intptr_t callback(int opcode, int index, intptr_t value, void* ptr, float opt);

	std::string getName();

	void OpenEditor();
	void OnCloseEditor();

	gmpi_dynamic_linking::DLL_HANDLE getDllHandle()
	{
		return hinstLib;
	}

	std::vector<IVstObserver*> observers;
};

VstIntPtr VSTCALLBACK vst2_callback(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
