#ifndef PROCESSORWRAPPER_H_INCLUDED
#define PROCESSORWRAPPER_H_INCLUDED
#pragma once

#include <memory>
#include <unordered_map>
#include "../se_sdk3/mp_sdk_audio.h"
#include "../shared/xplatform.h"
#include "public.sdk\source\vst\vstaudioeffect.h"
#include "pluginterfaces\vst\ivstprocesscontext.h"
#include "pluginterfaces\vst\ivstevents.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "pluginterfaces\vst\ivstaudioprocessor.h"
#include "public.sdk\source\vst\hosting\processdata.h"
#include "base\source\fobject.h"
#include "pluginterfaces\vst\ivstparameterchanges.h"
#include "../se_sdk3/mp_sdk_audio.h"
#include "mp_midi.h"

#if defined( _WIN32 ) && defined( _DEBUG )
//#define DEBUG_VST2_SIGNAL_LEVEL
#endif

struct myParamValueQueue : public Steinberg::FObject, public Steinberg::Vst::IParamValueQueue
{
	myParamValueQueue(Steinberg::Vst::ParamID id) : paramId(id) {}

	Steinberg::Vst::ParamID paramId = {};
	std::vector< std::pair<Steinberg::int32, Steinberg::Vst::ParamValue> > events;

	Steinberg::Vst::ParamID PLUGIN_API getParameterId () override
	{
		return paramId;
	}

	/** Returns count of points in the queue. */
	Steinberg::int32 PLUGIN_API getPointCount () override
	{
		return static_cast<Steinberg::int32>(events.size());
	}

	Steinberg::tresult PLUGIN_API getPoint (Steinberg::int32 index, Steinberg::int32& sampleOffset /*out*/, Steinberg::Vst::ParamValue& value /*out*/) override
	{
		sampleOffset = events[index].first;
		value = events[index].second;
		return 0;
	}

	/** Adds a new value at the end of the queue, its index is returned. */
	Steinberg::tresult PLUGIN_API addPoint (Steinberg::int32 sampleOffset, Steinberg::Vst::ParamValue value, Steinberg::int32& index /*out*/) override
	{
		index = static_cast<Steinberg::int32>(events.size());
		events.push_back({ sampleOffset, value });
		return 0;
	}

	//---Interface---------
	OBJ_METHODS (myParamValueQueue, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Steinberg::Vst::IParamValueQueue)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

struct myParameterChanges : public Steinberg::FObject, public Steinberg::Vst::IParameterChanges
{
	std::vector<myParamValueQueue> queues;

	Steinberg::int32 PLUGIN_API getParameterCount() override
	{
		return static_cast<Steinberg::int32>(queues.size());
	}

	Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(Steinberg::int32 index) override
	{
		return &queues[index];
	}

	Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID& id, Steinberg::int32& index /*out*/) override
	{
		index = 0;

		for(int i = 0; i < queues.size(); ++i)
		{
			if(queues[i].getParameterId() == id)
			{
				index = i;
				return &queues[i];
			}
		}

		index = static_cast<Steinberg::int32>(queues.size());
		queues.push_back(id);
		return &queues.back();
	}

	void clear()
	{
		queues.clear();
	}

	//---Interface---------
	OBJ_METHODS (myParameterChanges, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Steinberg::Vst::IParameterChanges)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

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


using namespace gmpi;

class Vst3ParamSet : public MpBase2
{
	FloatInPin pinFloatIn;
	IntInPin pinParamIdx;
	MidiOutPin pinParameterAccessOut;

public:
	Vst3ParamSet()
	{
		initializePin( pinFloatIn );
		initializePin( pinParamIdx );
		initializePin( pinParameterAccessOut );
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if( pinFloatIn.isUpdated() )
		{
			// Send MIDI HD-Protocol Note Expression message. Not tested.
			const int channel = 0;
			GmpiMidiHdProtocol::Midi2 msg;
			const int32_t intControllerVal20 = 0x0FFF & (int32_t)(pinFloatIn.getValue() * (float)0x0FFF);
			GmpiMidiHdProtocol::setMidiMessage(msg, GmpiMidi::MIDI_ControlChange, intControllerVal20, 0xFF, pinParamIdx.getValue());
			pinParameterAccessOut.send(msg.data(), msg.size());
		}
	}
};

class ProcessorWrapper : public MpBase2
{
	Steinberg::IPtr<Steinberg::Vst::IComponent> component_;
	Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> vstEffect_;
	Steinberg::Vst::ProcessContext vstTime_;
	myEventList vstEventList;
	myParameterChanges parameterEvents;
	Steinberg::Vst::HostProcessData processData;
	Steinberg::Vst::ProcessSetup processSetup;
	class ControllerWrapper* controller_ = {};

	int inputChannelCount = {};
	int outputChannelCount = {};

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
		for (int i = 0; i < inputChannelCount; ++i)
		{
			processData.setChannelBuffer(
				Steinberg::Vst::kInput,
				0,
				i,
	            getBuffer(*(AudioIns[i]))
			);
		}

		for (int i = 0; i < outputChannelCount; ++i)
		{
			processData.setChannelBuffer(
				Steinberg::Vst::kOutput,
				0,
				i,
	            getBuffer(*(AudioOuts[i]))
			);
		}

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

	void addParameterEvent(int clock, int index, float value);

	MidiInPin pinMidi;
	MidiInPin pinParameterAccess;

	std::vector< std::unique_ptr<AudioInPin> > AudioIns;
	std::vector< std::unique_ptr<AudioOutPin> > AudioOuts;
	std::vector< std::unique_ptr<FloatInPin> > ParameterPins;

	BoolInPin pinOnOffSwitch;
	BlobInPin pinAeffectPointer;

	// Musical time
	FloatInPin pinHostBpm;
	FloatInPin pinHostSongPosition;
	IntInPin pinNumerator;
	IntInPin pinDenominator;
	BoolInPin pinHostTransport;
	FloatInPin pinHostBarStart;

	int firstParameterPinIndex = {};
	int parameterAccessPinIndex = {};
};

#endif

