#pragma once

#include <memory>
#include <unordered_map>
#include "../se_sdk3/mp_sdk_audio.h"
#include "../se_sdk3/mp_midi.h"
#include "../shared/xplatform.h"
#include "base/source/fobject.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#if defined(SE_TARGET_WAVES)
#include "cancellation.h"
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

		// Fix for Waves Child Plugins, presets parameters in order of ID.
		for(auto it = queues.begin(); it != queues.end() ; ++it)
		{
			auto& queue = *it;
			if(queue.getParameterId() == id)
			{
				return &queue;
			}

			if(queue.getParameterId() > id)
			{
				it = queues.insert(it, id);
				return &(*it);
			}  
			++index;
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

class ProcessorWrapper : public MpBase2
{
	Steinberg::Vst::IComponent* component_ = {};
	Steinberg::Vst::IAudioProcessor* vstEffect_ = {};
	Steinberg::Vst::ProcessContext vstTime_;
	myEventList vstEventList;
	myParameterChanges parameterEvents;
	Steinberg::Vst::HostProcessData processData;
	Steinberg::Vst::ProcessSetup processSetup;

	std::vector<int> inputBusses; // bus/chans
	std::vector<int> outputBusses; // bus/chans

	bool bypassMode;
	std::vector<std::vector<float>> bypassDelays;
	int bypassBufferPos = 0;
	int latency = 0;

	// Silence detection.
	int silenceCounter;
	int tailSamples;
	bool inputStatusChanged;

	typedef void (ProcessorWrapper::* VstSubProcess_ptr)(int32_t count, const gmpi::MpEvent* events);

	VstSubProcess_ptr currentVstSubProcess;
#ifdef CANCELLATION_TEST_ENABLE2
    bool cancellation_done = false;
	void debugDumpPresetToFile();
#endif

public:
	ProcessorWrapper();
	~ProcessorWrapper();

	void onMidiMessage(int pin, int timeDelta, const unsigned char* midiMessage, int size);
	void ProcessEvents(int32_t count, const gmpi::MpEvent* events);
	void MP_STDCALL process(int32_t count, const gmpi::MpEvent* events) override
	{
		//(this->*(currentVstSubProcess))(count, events);
		subProcess2(count, events);
	}

	void subProcess2(const int32_t count, const gmpi::MpEvent* events)
	{
		ProcessEvents(count, events);

		const int bypassDelaysize = static_cast<int>(bypassDelays[0].size());

		// add input to bypass latency buffers
		{
			for (size_t i = 0; i < AudioIns.size(); ++i)
			{
				const float* in = getBuffer(*(AudioIns[i]));
				float* dest = bypassDelays[i].data() + bypassBufferPos;

				int todo = count;
				int c = (std::min)(todo, bypassDelaysize - bypassBufferPos);
				while (todo)
				{
					for (int s = 0; s < c; ++s)
					{
						*dest++ = *in++;
					}
					dest = bypassDelays[i].data(); // wrap back arround.
					todo -= c;
					c = todo;
				}
			}
		}

		// process plugin
		{
			for (int bus = 0; bus < inputBusses.size(); ++bus)
			{
				for (int i = 0; i < inputBusses[bus]; ++i)
				{
					processData.setChannelBuffer(
						Steinberg::Vst::kInput,
						bus,
						i,
						getBuffer(*(AudioIns[i]))
					);
				}
			}

			for (int bus = 0; bus < outputBusses.size(); ++bus)
			{
				for (int i = 0; i < outputBusses[bus]; ++i)
				{
					processData.setChannelBuffer(
						Steinberg::Vst::kOutput,
						bus,
						i,
						getBuffer(*(AudioOuts[i]))
					);
				}
			}
			myParameterChanges outputParameterChanges;
			myEventList outputEvents;

			processData.outputParameterChanges = &outputParameterChanges; // todo
			processData.outputEvents = &outputEvents;
			processData.numSamples = count;

			vstEffect_->process(processData);
		}


		// Copy buffered audio input to output.
		for (size_t i = 0; i < AudioOuts.size(); ++i)
		{
			auto out = getBuffer(*(AudioOuts[i]));

			if (i < AudioIns.size())
			{
				int bypassBufferReadPos = (bypassDelaysize + bypassBufferPos - latency) % bypassDelaysize;

				const float* source = bypassDelays[i].data() + bypassBufferReadPos;
				int todo = count;
				while (todo)
				{
					const int c = (std::min)(todo, bypassDelaysize - bypassBufferReadPos);
					for (int s = 0 ; s < c ; ++s)
					{
						*out++ = *source++;
					}
					source = bypassDelays[i].data(); // wrap back arround.
					bypassBufferReadPos = 0;
					todo -= c;
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

#if 0
		if (fadeLevel != targetLevel)
		{
			ProcessPlugin(count);
			CopyInputOverOutput(count);

			// fade-up complete?
			if (fadeLevel == targetLevel && targetLevel == 1.0f)
			{
				currentVstSubProcess = &ProcessorWrapper::subProcess;
			}
		}
		else
		{
			if (fadeLevel == 1.0f)
			{
				ProcessPlugin(count);
			}
			else
			{
				CopyInputToOutput(count);
			}
		}
#endif
		bypassBufferPos = (bypassBufferPos + count) % bypassDelaysize;

		vstTime_.continousTimeSamples += count;
	}

	void subProcess(int32_t count, const gmpi::MpEvent* events);
	void subProcessBypass(int32_t count, const gmpi::MpEvent* events);
	void DoBypass(int32_t count);

	int32_t MP_STDCALL open() override;
	void initVst();
	void onSetPins(void) override;
private:
	void copyInputToBypassBuffers(int32_t count);

	inline void	ProcessPlugin(int count)
	{
#ifdef CANCELLATION_TEST_ENABLE2
        if (!cancellation_done && vstTime_.continousTimeSamples > CANCELLATION_SNAPSHOT_TIMESTAMP) // don't account for oversample, but should be good enough
        {
			cancellation_done = true;
			debugDumpPresetToFile();
        }
#endif

		for (int bus = 0; bus < inputBusses.size(); ++bus)
		{
			for (int i = 0; i < inputBusses[bus]; ++i)
			{
				processData.setChannelBuffer(
					Steinberg::Vst::kInput,
					bus,
					i,
					getBuffer(*(AudioIns[i]))
				);
			}
		}

		for (int bus = 0; bus < outputBusses.size(); ++bus)
		{
			for (int i = 0; i < outputBusses[bus]; ++i)
			{
				processData.setChannelBuffer(
					Steinberg::Vst::kOutput,
					bus,
					i,
					getBuffer(*(AudioOuts[i]))
				);
			}
		}
		myParameterChanges outputParameterChanges;
		myEventList outputEvents;

		processData.outputParameterChanges = &outputParameterChanges; // todo
		processData.outputEvents = &outputEvents;
		processData.numSamples = count;

		vstEffect_->process(processData);

		vstTime_.continousTimeSamples += count;
	}

	void CopyInputToOutput(int count);

	inline void	CopyInputOverOutput(int count)
	{
		float fade = {};
		constexpr float fadeTimeS = 0.02f;
		const float fadeInc = copysignf(1.f / (getSampleRate() * fadeTimeS), targetLevel - fadeLevel);

		for (size_t i = 0; i < AudioOuts.size(); ++i)
		{
			fade = fadeLevel;

			// Copy audio input to output.
			auto out = getBuffer(*(AudioOuts[i]));
			if (i < AudioIns.size())
			{
				float* in = getBuffer(*(AudioIns[i]));
				for (int s = count; s > 0; --s)
				{
					fade = std::clamp(fade + fadeInc, 0.0f, 1.0f);
					*out = *in + fade * (*out - *in);

					++out;
					++in;
				}
			}
			else
			{
				// if not audio input pins, output silence.
				for (int s = count; s > 0; --s)
				{
					fade = std::clamp(fade + fadeInc, 0.0f, 1.0f);
					*out = 0.0f + fade * (*out - 0.0f);
					++out;
				}
			}
		}

		fadeLevel = fade;
	}

	void addParameterEvent(int clock, int index, float value);

	MidiInPin pinMidi;
	MidiInPin pinParameterAccess;

	std::vector< std::unique_ptr<AudioInPin> > AudioIns;
	std::vector< std::unique_ptr<AudioOutPin> > AudioOuts;

	BoolInPin pinOnOffSwitch;
	BlobInPin pinControllerPointer;

	// Musical time
	FloatInPin pinHostBpm;
	FloatInPin pinHostSongPosition;
	IntInPin pinNumerator;
	IntInPin pinDenominator;
	BoolInPin pinHostTransport;
	FloatInPin pinHostBarStart;
	IntInPin pinOfflineRenderMode;

	int firstParameterPinIndex = {};
	int parameterAccessPinIndex = {};

	float fadeLevel = 1.0f;
	float targetLevel = 1.0f;
};

class Vst3ParamSet : public MpBase2
{
	bool initialUpdateDone = false;

	FloatInPin pinFloatIn;
	IntInPin pinParamIdx;
	MidiOutPin pinParameterAccessOut;

public:
	Vst3ParamSet()
	{
		initializePin(pinFloatIn);
		initializePin(pinParamIdx);
		initializePin(pinParameterAccessOut);
	}

	void sendPinValueAsMidi()
	{
		const int paramId = pinParamIdx.getValue();
        assert(paramId < 256);

		// Send MIDI HD-Protocol Note Expression message. key is paramId, value is normalised cast to int32.
		GmpiMidiHdProtocol::Midi2 msg;
		GmpiMidiHdProtocol::setMidiMessage(msg, GmpiMidi::MIDI_ControlChange, 0, paramId, 0);

		const float normalized = pinFloatIn.getValue();
        msg.value = *(int32_t*) &normalized;

		pinParameterAccessOut.send(msg.data(), msg.size(), getBlockPosition());// + paramId);
	}

	void onSetPins(void) override
	{
		if (pinFloatIn.isUpdated())
		{
			sendPinValueAsMidi();
		}
	}
};
