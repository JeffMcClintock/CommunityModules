#ifndef VST2WRAPPER_H_INCLUDED
#define VST2WRAPPER_H_INCLUDED

#include <memory>
#include "../se_sdk3/mp_sdk_audio.h"
#include "../shared/xplatform.h"
#include "AEffectWrapper.h"

class VstEventsWrapper			// a block of events for the current audio block
{
public:
	inline bool IsEmpty() const
	{
		return m_vst_events->numEvents == 0;
	}

	void Add(VstEvent* p_event);
	inline void Clear(void)
	{
		// place most common case (empty) inline for efficiency.
		if (IsEmpty())
			return;

		// call into actual clearing if needed (rarely). Not inline to save code size.
		DoClear();
	}
	void SetSize(int n);
	VstEventsWrapper();
	~VstEventsWrapper();
	inline VstEvents* GetVstEvents(void) const
	{
		return m_vst_events;
	}

private:
	void DoClear(void);

	int m_max_size;
	VstEvents* m_vst_events;
};

class Vst2Wrapper : public MpBase2, public IVST2Host
{
	IAEffect* vstEffect_;
	VstIntPtr shellPluginId_;
	VstEventsWrapper vstEventList;
	VstTimeInfo vstTime_;
	bool useChunkPresets;
	bool OnOffSwitchEnabled;
	bool bypassMode;

	// Silence detection.
	int silenceCounter;
	int tailSamples;
	bool inputStatusChanged;

	typedef void (Vst2Wrapper::* VstSubProcess_ptr)(int32_t count, const gmpi::MpEvent* events);

	VstSubProcess_ptr currentVstSubProcess;

#if defined(DEBUG_VST2_SIGNAL_LEVEL )
	float peakLevel;
	int peakCounter;
#endif
#ifdef CANCELLATION_TEST_ENABLE2
    bool cancellation_done = false;
	void debugDumpPresetToFile();
#endif

public:
	Vst2Wrapper(VstIntPtr shellPluginId, bool pUseChunkPresets);
	~Vst2Wrapper();

	void onMidiMessage(int pin, int timeDelta, const unsigned char* midiMessage, int size);
	void ProcessEvents(int32_t count, const gmpi::MpEvent* events);
	virtual void MP_STDCALL process(int32_t count, const gmpi::MpEvent* events)
	{
		(this->*(currentVstSubProcess))(count, events);
	}
	void subProcess(int32_t count, const gmpi::MpEvent* events);
	void subProcessPreSleep(int32_t count, const gmpi::MpEvent* events);
	void subProcessSilence(int32_t count, const gmpi::MpEvent* events);
	void subProcessBypass(int32_t count, const gmpi::MpEvent* events);
	void subProcessBypassSilence(int32_t count, const gmpi::MpEvent* events);

	virtual int32_t MP_STDCALL open();
	void initVst();
	virtual void onSetPins(void);

	// IVST2Host support.
	virtual float MP_STDCALL hSampleRate(void);
	virtual int MP_STDCALL hGetBlockSize(void)
	{
		return getBlockSize();
	}
	virtual VstTimeInfo* MP_STDCALL hHostGetTime(int mask);
	virtual void MP_STDCALL hProcessPlugsEvents(struct VstEvents* p_vst_events){}
	virtual void MP_STDCALL hRequestIdle(void){}
	virtual int MP_STDCALL hFileDialog(bool load_or_save, const std::string& extension, std::string& filename)
	{
		return 0;
	}
    virtual void MP_STDCALL onAEffectWrapperDestroyed();
    
	int MP_STDCALL getProcessLevel() override
	{
		if (pinProcessMode == 2) // = "Preview" (Offline)
		{
			return kVstProcessLevelOffline; // currently offline processing and thus in user thread
		}
		return kVstProcessLevelRealtime; // 2: currently in audio thread (where process is called)
	} 

private:

	inline void	ProcessPlugin(int count)
	{
		for (size_t i = 0; i < AudioIns.size(); ++i)
		{
			inputBuffers[i] = getBuffer(*(AudioIns[i]));
		}

		for (size_t i = 0; i < AudioOuts.size(); ++i)
		{
			outputBuffers[i] = getBuffer(*(AudioOuts[i]));
		}

		vstEffect_->processReplacing(inputBuffers.data(), outputBuffers.data(), count, vstEventList.GetVstEvents());

		vstTime_.samplePos += count;
	}

	inline void	CopyInputToOutput(int count)
	{
		for (size_t i = 0; i < AudioOuts.size(); ++i)
		{
			// Copy audio input to output.
			auto out = getBuffer(*(AudioOuts[i]));
			if (i < AudioIns.size())
			{
				float* in = getBuffer(*(AudioIns[i]));
				for (int s = count; s > 0; --s)
				{
					*out++ = *in++;
				}
			}
			else
			{
				// if not audio input pins, output silence.
				for (int s = count; s > 0; --s)
				{
					*out++ = 0.0f;
				}
			}
		}
		vstTime_.samplePos += count;
	}

	inline void	CopySilenceToOutput(int count)
	{
		// No significant change to input signal. Output Silence.
		if (silenceCounter > 0)
		{
			// If we havn't already, communicate "static" status from outputs.
			bool outputStreaming = !AudioOuts.empty() && AudioOuts[0]->isStreaming();
			if (outputStreaming)
			{
				for (auto& outPin : AudioOuts)
				{
					outPin->setStreaming(false, 0);
				}
			}

			for (auto& outPin : AudioOuts)
			{
				auto out = getBuffer(*outPin);
				for (int s = count; s > 0; --s)
				{
					*out++ = 0.0f;
				}
			}
			silenceCounter -= count;
		}
		vstTime_.samplePos += count;
	}

	MidiInPin pinMidi;

	std::vector< std::unique_ptr<AudioInPin> > AudioIns;
	std::vector< std::unique_ptr<AudioOutPin> > AudioOuts;
	std::vector< std::unique_ptr<FloatInPin> > ParameterPins;

	std::vector< float* > inputBuffers;
	std::vector< float* > outputBuffers;

	BoolInPin pinOnOffSwitch;
	BoolInPin pinAutoSleep;
	IntInPin pinProcessMode;
	BlobInPin pinAeffectPointer;

	// Musical time
	FloatInPin pinHostBpm;
	FloatInPin pinHostSongPosition;
	IntInPin pinNumerator;
	IntInPin pinDenominator;
	BoolInPin pinHostTransport;
	FloatInPin pinHostBarStart;
};

#endif

