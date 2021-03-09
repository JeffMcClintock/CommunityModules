#ifndef PROCESSORWRAPPER_H_INCLUDED
#define PROCESSORWRAPPER_H_INCLUDED
#pragma once

#include <memory>
#include "../se_sdk3/mp_sdk_audio.h"
#include "../shared/xplatform.h"
#include "public.sdk\source\vst\vstaudioeffect.h"
#include "pluginterfaces\vst\ivstprocesscontext.h"
#include "pluginterfaces\vst\ivstevents.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "pluginterfaces\vst\ivstaudioprocessor.h"
#include "public.sdk\source\vst\hosting\processdata.h"
#include "base\source\fobject.h"

#if defined( _WIN32 ) && defined( _DEBUG )
//#define DEBUG_VST2_SIGNAL_LEVEL
#endif

struct myEventList : public Steinberg::FObject, public Steinberg::Vst::IEventList
{
	std::vector<Steinberg::Vst::Event> events;

	Steinberg::int32 PLUGIN_API getEventCount() override
	{
		return static_cast<Steinberg::int32>(events.size());
	}
	Steinberg::tresult PLUGIN_API getEvent(Steinberg::int32 index, Steinberg::Vst::Event& e /*out*/) override
	{
		e = events.at(index);
		return Steinberg::kResultOk;
	}
	Steinberg::tresult PLUGIN_API addEvent(Steinberg::Vst::Event& e /*in*/) override
	{
		events.push_back(e);
		return Steinberg::kResultOk;
	}

	//---Interface---------
	OBJ_METHODS (myEventList, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (IEventList)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

class ProcessorWrapper : public MpBase2
{
	Steinberg::Vst::IComponent* component_ = {};
	Steinberg::Vst::IAudioProcessor* vstEffect_ = {};
	Steinberg::Vst::ProcessContext vstTime_;
	Steinberg::Vst::PlugProvider* pluginProvider_ = {};
	myEventList vstEventList;
	Steinberg::Vst::HostProcessData processData;
	Steinberg::Vst::ProcessSetup processSetup;
//	FUnknown hostContext;

	bool useChunkPresets;
	bool OnOffSwitchEnabled;
	bool bypassMode;

	// Silence detection.
	int silenceCounter;
	int tailSamples;
	bool inputStatusChanged;

	typedef void (ProcessorWrapper::* VstSubProcess_ptr)(int32_t count, const gmpi::MpEvent* events);

	VstSubProcess_ptr currentVstSubProcess;

#if defined(DEBUG_VST2_SIGNAL_LEVEL )
	float peakLevel;
	int peakCounter;
#endif

public:
	ProcessorWrapper();
	~ProcessorWrapper();

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
	bool setupBuffers(int numBusses, Steinberg::Vst::AudioBusBuffers* audioBuffers, Steinberg::Vst::BusDirection dir);
	bool setupBuffers(Steinberg::Vst::AudioBusBuffers& audioBuffers);
	virtual void onSetPins(void);
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

		processData.inputEvents = &vstEventList;
		processData.numSamples = count;
		vstEffect_->process(processData);

		vstTime_.projectTimeSamples += count;
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
		vstTime_.projectTimeSamples += count;
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
		vstTime_.projectTimeSamples += count;
	}

	MidiInPin pinMidi;
	MidiInPin pinParameterAccess;

	std::vector< std::unique_ptr<AudioInPin> > AudioIns;
	std::vector< std::unique_ptr<AudioOutPin> > AudioOuts;
	std::vector< std::unique_ptr<FloatInPin> > ParameterPins;

	std::vector< float* > inputBuffers;
	std::vector< float* > outputBuffers;

	BoolInPin pinOnOffSwitch;
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

