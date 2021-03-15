#include "./ProcessorWrapper.h"
#include "../se_sdk3/mp_midi.h"
#include "../shared/xplatform.h"
#include <algorithm>
#include "ControllerWrapper.h"

#if defined(SE_TARGET_WAVES)
#include "../../ug_base.h"
#include "../../dsp_patch_parameter_base.h"
#include "../../dsp_patch_manager.h"
#endif

using namespace std;
using namespace gmpi;
using namespace GmpiMidi;

using namespace Steinberg;
using namespace Steinberg::Vst;

ProcessorWrapper::ProcessorWrapper() :
	bypassMode(true)
	, tailSamples(32)
	, currentVstSubProcess(&ProcessorWrapper::subProcessBypass)
{
	memset(&vstTime_, 0, sizeof(vstTime_));
	vstTime_.state =
		ProcessContext::kTempoValid |
		ProcessContext::kTimeSigValid |
		ProcessContext::kBarPositionValid |
		ProcessContext::kContTimeValid;

	// reasonable defaults for now.
	vstTime_.tempo = 100;
	vstTime_.timeSigDenominator = 4;
	vstTime_.timeSigNumerator = 4;

	processData.processContext = &vstTime_;
}

ProcessorWrapper::~ProcessorWrapper()
{
	if( component_ )
	{
		component_->setActive(false);
	}
}

int32_t ProcessorWrapper::open()
{
	MpBase2::open();

	// Setup IO.
	gmpi_sdk::mp_shared_ptr<gmpi::IMpPinIterator> it;
	if(MP_OK == getHost()->createPinIterator(it.asIMpUnknownPtr()))
	{
		std::vector<int> midiPinsIdxs;

		int r = it->first();
		int idx = 0;
		while(r == MP_OK)
		{
			int32_t direction;
			int32_t datatype;
			int32_t id;
			it->getPinDirection(direction);
			it->getPinId(id);
			it->getPinDatatype(datatype);

			if(direction == gmpi::MP_IN)
			{
				// enum EPlugDataType { DT_ENUM, DT_TEXT, DT_MIDI2, DT_DOUBLE, DT_BOOL, DT_FSAMPLE, DT_FLOAT, DT_VST_PARAM, DT_INT, DT_INT64, DT_BLOB, DT_NONE=-1 };  //plug datatype

				switch(datatype)
				{
				case MP_BOOL:
					initializePin(idx++, pinOnOffSwitch);
					r = it->next();

					// This pin added later. Check if it's in XML yet. Only need this if statement until both Grand80 and RHP ported again.
					it->getPinDatatype(datatype);
					//if (datatype == MP_BOOL)
					//{
					//	initializePin(idx++, pinAutoSleep);
					//	r = it->next();
					//}

					initializePin(idx++, pinHostBpm);
					r = it->next();
					initializePin(idx++, pinHostSongPosition);
					r = it->next();
					initializePin(idx++, pinHostTransport);
					r = it->next();
					initializePin(idx++, pinNumerator);
					r = it->next();
					initializePin(idx++, pinDenominator);
					r = it->next();
					initializePin(idx, pinHostBarStart);
					break;

				case MP_MIDI:
					midiPinsIdxs.push_back(idx);
					break;

					// This does not apply when Parameter pins are on GUI.
				case MP_FLOAT32:
					// not VS2012		ParameterPins.push_back(make_unique<FloatInPin>());
					ParameterPins.push_back(std::unique_ptr<FloatInPin>(new FloatInPin()));

					initializePin(idx, *(ParameterPins.back()));

					if(!firstParameterPinIndex)
					{
						firstParameterPinIndex = idx;
					}

					break;

				case MP_BLOB:
					initializePin(idx, pinAeffectPointer);
					break;

				default: // MP_AUDIO
// not VS2012		AudioIns.push_back(make_unique<AudioInPin>());
					AudioIns.push_back(std::unique_ptr<AudioInPin>(new AudioInPin()));
					initializePin(idx, *(AudioIns.back()));
					break;
				}
			}
			else
			{
				//AudioOuts.push_back(make_unique<AudioOutPin>());
				AudioOuts.push_back(std::unique_ptr<AudioOutPin>(new AudioOutPin()));
				initializePin(idx, *(AudioOuts.back()));
			}
			r = it->next();
			++idx;
		}

		if(midiPinsIdxs.size() == 2)
		{
			initializePin(midiPinsIdxs.front(), pinMidi);
		}
		parameterAccessPinIndex = midiPinsIdxs.back();
		initializePin(parameterAccessPinIndex, pinParameterAccess);
	}

	vstTime_.sampleRate = (double)getSampleRate();

	// Preserve FPU state (Waves plugins trash it).
#if _MSC_VER >= 1600 // Not Avail in VS2005.
	unsigned int fpState;
	_controlfp_s(&fpState, 0, 0);
#endif

#if defined(SE_TARGET_WAVES)
	{
		int32_t handle;
		getHost()->getHandle(handle);

		auto ug = dynamic_cast< ug_base*>( getHost() );
		auto synthRuntime_ = ug->AudioMaster()->Application();
		vstEffect_ = synthRuntime_->WavesGetChildPlugin(shellPluginId_, handle);
		initVst();
	}
#endif

	for (auto it = AudioOuts.begin(); it != AudioOuts.end(); ++it)
	{
		(*it)->setStreaming(true);
	}

	unsigned int unused;
#if defined (_WIN32) && !defined(_WIN64)
	_controlfp_s(&unused, fpState, MCW_PC | _MCW_DN);
#else
	_controlfp_s(&unused, fpState, _MCW_DN);
#endif

	processData.inputEvents = &vstEventList;
	processData.inputParameterChanges = &parameterEvents;
	return MP_OK;
}

void ProcessorWrapper::initVst()
{
	bypassMode = true;
	currentVstSubProcess = &ProcessorWrapper::subProcessBypass;
	vstEffect_ = {};
	component_ = {};

	if (!controller_ || !controller_->plugin.controller)
	{
		return;
	}

	{
		component_ = controller_->plugin.component;

		if (!component_)
		{
			return;
		}

		{
			Steinberg::Vst::IAudioProcessor* vstEffect = {};
			component_->queryInterface(IAudioProcessor::iid, (void**)&vstEffect);
			vstEffect_.reset(vstEffect);
		}

		if (!vstEffect_)
		{
			component_ = nullptr;
			return;
		}

		processSetup = {
			kRealtime,
			kSample32,
			getBlockSize(),
			getSampleRate()
		};

		if(vstEffect_->setupProcessing(processSetup) != kResultOk)
		{
			return;
		}

		processData.prepare (*component_, 0, processSetup.symbolicSampleSize);

		{
			BusDirection dir = kInput;
			int32 numBusses = component_->getBusCount (kAudio, dir);
			if(!setupBuffers(numBusses, processData.inputs, dir))
				return;// false;

			processData.numInputs = numBusses;
		}
		{
			BusDirection dir = kOutput;
			int32 numBusses = component_->getBusCount (kAudio, dir);
			if(!setupBuffers(numBusses, processData.outputs, dir))
				return;// false;

			processData.numOutputs = numBusses;
		}

		component_->setActive(true);

		bypassMode = false;
		currentVstSubProcess = &ProcessorWrapper::subProcess;
	}
}

bool ProcessorWrapper::setupBuffers (int numBusses, AudioBusBuffers* audioBuffers, BusDirection dir)
{
	if (((numBusses > 0) && !audioBuffers) || !component_)
		return false;
	for (int32 busIndex = 0; busIndex < numBusses; busIndex++) // buses
	{
		BusInfo busInfo;
		if (component_->getBusInfo (kAudio, dir, busIndex, busInfo) == kResultTrue)
		{
			if (!setupBuffers (audioBuffers[busIndex]))
				return false;

			if(dir == kInput)
			{
				inputChannelCount = busInfo.channelCount;
			}
			else
			{
				outputChannelCount = busInfo.channelCount;
			}
/* todo
			if ((busInfo.flags & BusInfo::kDefaultActive) != 0)
			{
				for (int32 chIdx = 0; chIdx < busInfo.channelCount; chIdx++) // channels per bus
					audioBuffers[busIndex].silenceFlags |=
					    (TestDefaults::instance().channelIsSilent << chIdx);
			}
*/
		}
		else
			return false;
	}
	return true;
}

bool ProcessorWrapper::setupBuffers (AudioBusBuffers& audioBuffers)
{
	//if (processSetup.symbolicSampleSize != processData.symbolicSampleSize)
	//	return false;

	audioBuffers.silenceFlags = 0;
	for (int32 chIdx = 0; chIdx < audioBuffers.numChannels; chIdx++)
	{
//		if (processSetup.symbolicSampleSize == kSample32)
		{
			if (audioBuffers.channelBuffers32)
			{
				audioBuffers.channelBuffers32[chIdx] =
				    new Sample32[processSetup.maxSamplesPerBlock];
				if (audioBuffers.channelBuffers32[chIdx])
					memset (audioBuffers.channelBuffers32[chIdx], 0,
					        processSetup.maxSamplesPerBlock * sizeof (Sample32));
				else
					return false;
			}
			else
				return false;
		}
		//else if (processSetup.symbolicSampleSize == kSample64)
		//{
		//	if (audioBuffers.channelBuffers64)
		//	{
		//		audioBuffers.channelBuffers64[chIdx] =
		//		    new Sample64[processSetup.maxSamplesPerBlock];
		//		if (audioBuffers.channelBuffers64[chIdx])
		//			memset (audioBuffers.channelBuffers64[chIdx], 0,
		//			        processSetup.maxSamplesPerBlock * sizeof (Sample64));
		//		else
		//			return false;
		//	}
		//	else
		//		return false;
		//}
		//else
		//	return false;
	}
	return true;
}

void ProcessorWrapper::addParameterEvent(int clock, int id, float value)
{
	Steinberg::int32 returnIndexUnused = {};
	parameterEvents.addParameterData(id, returnIndexUnused)->addPoint(clock, value, returnIndexUnused);
}

void ProcessorWrapper::onMidiMessage(int pin, int timeDelta, const unsigned char* midiData, int size)
{
//	int due_time_delta = getBlockPosition();
	assert(timeDelta >= 0);

	timeDelta = max(timeDelta, 0); // should never be nesc, but is safer to do

	if(pin == parameterAccessPinIndex)
	{
		if(GmpiMidiHdProtocol::isWrappedHdProtocol(midiData, size))
		{
			int channelGroup{};
			int keyNumber{};
			int val_12b{};
			int val_20b{};
			int status{};
			int midiChannel{};

			GmpiMidiHdProtocol::DecodeHdMessage(midiData, size, status, midiChannel, channelGroup, keyNumber, val_12b, val_20b);

			constexpr float recip = 1.0f / (float)0x0FFF;
			const float normalised = (float)val_20b * recip;

			addParameterEvent(timeDelta, val_12b, normalised);
		}
		return;
	}

	// 1.1, complaints system msg needed.		if( !is_system_msg ) // FMHeaven crashes if sent system messages
	// pass to plug. !! NEED TO CHECK IT SUPPORTS EVENTS!!
	const int b1 = midiData[0];
	const int status = midiData[0] & 0xf0;
	const int channel = midiData[0] & 0x0f;

	const int unusedType = 666;

	Steinberg::Vst::Event m = {};
	m.type = unusedType;

	if (b1 != MIDI_SystemMessage)
	{
		const int chan = b1 & 0x0f;
		const bool is_system_msg = (b1 & MIDI_SystemMessage) == MIDI_SystemMessage;
/*
		if (!is_system_msg && midi_channel != -1)
		{
			b1 = b1 & 0xf0; //!jason freesonic only responds to chan 1 (zero), mask off any chan #
		}
*/

		switch(status)
		{
		case GmpiMidi::MIDI_NoteOff:
			m.type = Event::kNoteOffEvent;
			m.noteOff.channel = channel;
			m.noteOff.noteId = -1;
			m.noteOff.pitch = midiData[1];
			m.noteOff.velocity = midiData[2] / 127.0f;
			break;

		case GmpiMidi::MIDI_NoteOn:
			m.type = Event::kNoteOnEvent;
			m.noteOn.channel = channel;
			m.noteOn.noteId = -1;
			m.noteOn.pitch = midiData[1];
			m.noteOn.velocity = midiData[2] / 127.0f;
			break;

			//etc....
		};

		/*
		m.type = kVstMidiType;
		m.noteLength = 0;	// (in sample frames) of entire note, if available, else 0
		m.noteOffset = 0;	// offset into note from note start if available, else 0
							// m.midiData[0]		   1 thru 3 midi bytes; midiData[3] is reserved (zero)
		m.midiData[1] = (char)0xff;	// if unused must be 0xff
		m.midiData[2] = (char)0xff;
		m.midiData[3] = 0;

		for (int i = 0; i < msgSize; ++i)
		{
			m.midiData[i] = midiData[i];
		}

		m.detune = 0;		// -64 to +63 cents; for scales other than 'well-tempered' ('microtuning')
		m.noteOffVelocity = 0;
		m.reserved1 = 0;		// zero
		m.reserved2 = 0;		// zero
		vstEvent = (VstEvent*)m;
		*/
	}
	else // SYSEX
	{
		m.type = Event::kDataEvent;
		m.data.type = DataEvent::kMidiSysEx;
		m.data.bytes = (uint8*)midiData;
		m.data.size = size;
	}

	if(m.type != unusedType)
	{
		m.sampleOffset = timeDelta;
		vstEventList.events.push_back(m);
	}
}

void ProcessorWrapper::ProcessEvents(int32_t count, const gmpi::MpEvent* events)
{
	assert(count > 0);

#if defined(_DEBUG)
	blockPosExact_ = false;
#endif

	vstEventList.events.clear();
	parameterEvents.clear();

	blockPos_ = 0;
	int lblockPos = blockPos_;
	int remain = count;
	const MpEvent* next_event = events;

	for (;;)
	{
		if (next_event == 0) // fast version, when no events on list.
		{
			//			(this->*(curSubProcess_))(remain);
			break;
		}

		assert(next_event->timeDelta < count); // Event will happen in this block

		int delta_time = next_event->timeDelta - lblockPos;

		if (delta_time > 0) // then process intermediate samples
		{
			eventsComplete_ = false;

			//			(this->*(curSubProcess_))(delta_time);
			remain -= delta_time;

			eventsComplete_ = true;

			assert(remain != 0); // BELOW NEEDED?, seems non sense. If we are here, there is a event to process. Don't want to exit!
			if (remain == 0) // done
			{
				break;
			}

			lblockPos += delta_time;
		}

		//int cur_timeStamp = next_event->timeDelta;
#if defined(_DEBUG)
		blockPosExact_ = true;
#endif
		assert(lblockPos == next_event->timeDelta);

		// PRE-PROCESS EVENT
		bool pins_set_flag = false;
		const MpEvent* e = next_event;
		do
		{
			preProcessEvent(e); // updates all pins_ values
			pins_set_flag = pins_set_flag || e->eventType == EVENT_PIN_SET || e->eventType == EVENT_PIN_STREAMING_START || e->eventType == EVENT_PIN_STREAMING_STOP;
			e = e->next;
		} while (e != 0 && e->timeDelta == lblockPos);

		// PROCESS EVENT
		e = next_event;
		do
		{
			if(e->eventType == EVENT_PIN_SET && e->parm1 >= firstParameterPinIndex)
			{
				addParameterEvent(
					e->timeDelta,
					e->parm1 - firstParameterPinIndex,
					*(float*)(&e->parm3)
				);
			}
			else if (e->eventType == EVENT_MIDI)
			{
				if (e->extraData == 0) // short msg
				{
					onMidiMessage(e->parm1 // pin
						, e->timeDelta, (const unsigned char*)&(e->parm3), e->parm2); // midi bytes (short msg)
				}
				else
				{
					onMidiMessage(e->parm1 // pin
						, e->timeDelta, (const unsigned char*)e->extraData, e->parm2); // midi bytes (sysex)
				}
			}
			else
			{
				processEvent(e); // notify all pins_ values
			}
			e = e->next;
		} while (e != 0 && e->timeDelta == lblockPos);

		if (pins_set_flag)
		{
			onSetPins();
		}

		// POST-PROCESS EVENT
		do
		{
			postProcessEvent(next_event);
			next_event = next_event->next;
		} while (next_event != 0 && next_event->timeDelta == lblockPos);

#if defined(_DEBUG)
		blockPosExact_ = false;
#endif
	}
}

void ProcessorWrapper::subProcessBypass(int32_t count, const gmpi::MpEvent* events)
{
	ProcessEvents(count, events);

	CopyInputToOutput(count);
}

void ProcessorWrapper::subProcessBypassSilence(int32_t count, const gmpi::MpEvent* events)
{
	if (silenceCounter <= 0 && events == nullptr)
	{
		getHost()->sleep();
		return;
	}

	inputStatusChanged = false;

	ProcessEvents(count, events);

	// If any input signal changes, set output to run, else stop.
	bool outputStreaming = !AudioOuts.empty() && AudioOuts[0]->isStreaming();
	if (outputStreaming != inputStatusChanged)
	{
		for (auto& outPin : AudioOuts)
		{
			outPin->setStreaming(inputStatusChanged, 0);
		}
	}

	if (inputStatusChanged)
	{
		for (auto& outPin : AudioOuts)
		{
			outPin->setStreaming(inputStatusChanged, 0);
		}

		// If the input changed in any way, need to feed entire input block to output.
		CopyInputToOutput(count);
	}
	else
	{
		// No significant change to input signal. Output Silence.
		CopySilenceToOutput(count);
	}
}

void ProcessorWrapper::subProcessSilence(int32_t count, const gmpi::MpEvent* events)
{
	if (silenceCounter <= 0 && events == nullptr)
	{
		getHost()->sleep();
		return;
	}

	inputStatusChanged = false;

	ProcessEvents(count, events);

	// If any input signal changes, set output to run, else stop.
	bool outputStreaming = !AudioOuts.empty() && AudioOuts[0]->isStreaming();
	if (outputStreaming != inputStatusChanged)
	{
		for (auto& outPin : AudioOuts)
		{
			outPin->setStreaming(inputStatusChanged, 0);
		}
	}

	if (inputStatusChanged)
	{
		// If the input changed in any way, need to process block to output.
		ProcessPlugin(count);

		// return to monitoring output tail.
		silenceCounter = getBlockSize();
		if (currentVstSubProcess == &ProcessorWrapper::subProcessSilence) // it might already be witched elsewhere by "power" button.
		{
			currentVstSubProcess = &ProcessorWrapper::subProcessPreSleep;
		}
	}
	else
	{
		CopySilenceToOutput(count);
	}
}

// This processes the plugin, while waiting for the 'tail' to die away.
void ProcessorWrapper::subProcessPreSleep(int32_t count, const gmpi::MpEvent* events)
{
	if (silenceCounter < 0 && events == nullptr )
	{
		silenceCounter = getBlockSize();
		currentVstSubProcess = &ProcessorWrapper::subProcessSilence;
		(this->*(currentVstSubProcess))(count, events);
		return;
	}

	silenceCounter -= count;

	inputStatusChanged = false;

	ProcessEvents(count, events);

	if (inputStatusChanged)
	{
		silenceCounter = tailSamples;
	}

	ProcessPlugin(count);

	/*
	// Silence detection.
	const float INSIGNIFICANT_SIGNAL_LEVEL = 0.000001f;

	for (size_t i = 0; i < AudioOuts.size(); ++i)
	{
		auto o = outputBuffers[i];
		for (int i = 0; i < count; ++i)
		{
			if (*o > INSIGNIFICANT_SIGNAL_LEVEL || *o < -INSIGNIFICANT_SIGNAL_LEVEL)
			{
				silenceCounter = tailSamples;
				return;
			}

			++o;
		}
	}
	*/
}

void ProcessorWrapper::subProcess(int32_t count, const gmpi::MpEvent* events)
{
	ProcessEvents(count, events);

	ProcessPlugin(count);
}

void ProcessorWrapper::onSetPins(void)
{
	if (pinHostBpm.isUpdated())
	{
		vstTime_.tempo = pinHostBpm;
	}
	if (pinNumerator.isUpdated())
	{
		vstTime_.timeSigNumerator = pinNumerator;
	}
	if (pinDenominator.isUpdated())
	{
		vstTime_.timeSigDenominator = pinDenominator;
	}

#if !defined(SE_TARGET_WAVES)
	if (pinAeffectPointer.isUpdated() && pinAeffectPointer.getValue().getSize() == sizeof(controller_) )
	{
		controller_ = *(ControllerWrapper**)pinAeffectPointer.getValue().getData();

		initVst();
	}
#endif

	bypassMode = vstEffect_ == 0 || !pinOnOffSwitch;

	bool inputStreaming = false;
	bool inputUpdated = pinOnOffSwitch.isUpdated();// | pinAutoSleep.isUpdated();
	for (auto& p : AudioIns)
	{
		inputStreaming |= p->isStreaming();
		inputUpdated |= p->isUpdated();
	}
	inputStatusChanged |= inputUpdated;

	bool outputStreaming = !AudioOuts.empty() && AudioOuts[0]->isStreaming();

	if (bypassMode)
	{
		if (inputStreaming)
		{
			if (!outputStreaming)
			{
				for (auto& outPin : AudioOuts)
				{
					outPin->setStreaming(true);
				}
			}

			currentVstSubProcess = &ProcessorWrapper::subProcessBypass;
		}
		else
		{
			if (inputUpdated)
			{
				silenceCounter = getBlockSize();
			}

			currentVstSubProcess = &ProcessorWrapper::subProcessBypassSilence;
		}
	}
	else
	{
		if (true)
		{
			if (!outputStreaming)
			{
				for (auto& outPin : AudioOuts)
				{
					outPin->setStreaming(true);
				}
			}

			currentVstSubProcess = &ProcessorWrapper::subProcess;
		}
		else
		{
			if (inputUpdated)
			{
				silenceCounter = getBlockSize();
				currentVstSubProcess = &ProcessorWrapper::subProcessPreSleep;
			}
		}
	}
}
