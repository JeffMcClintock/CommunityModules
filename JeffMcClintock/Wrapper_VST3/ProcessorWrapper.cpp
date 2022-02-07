#include "./ProcessorWrapper.h"
#include "../shared/xplatform.h"
#include <algorithm>
#include "./MyViewStream.h"

#if defined(SE_TARGET_WAVES)
#include "../../ug_base.h"
#include "../../dsp_patch_parameter_base.h"
#include "../../dsp_patch_manager.h"
#endif

#ifdef CANCELLATION_TEST_ENABLE2
#include <iostream>
#include <fstream>
#include <iomanip>
#endif

using namespace std;
using namespace gmpi;
using namespace GmpiMidi;

using namespace Steinberg;
using namespace Steinberg::Vst;

ProcessorWrapper::ProcessorWrapper() :
	currentVstSubProcess(&ProcessorWrapper::subProcessNotLoaded)
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
	if (component_)
	{
        component_->setActive(false);
	}
}

void ProcessorWrapper::process(int32_t count, const gmpi::MpEvent* events)
{
	(this->*(currentVstSubProcess))(count, events);
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
				switch(datatype)
				{
				case MP_BOOL:
					initializePin(idx++, pinOnOffSwitch);
					r = it->next();

					// This pin added later. Check if it's in XML yet. Only need this if statement until both Grand80 and RHP ported again.
					it->getPinDatatype(datatype);

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
					initializePin(idx++, pinHostBarStart);
					r = it->next();
					initializePin(idx, pinOfflineRenderMode);
					break;

				case MP_MIDI:
					midiPinsIdxs.push_back(idx);
					break;

				case MP_BLOB:
					initializePin(idx, pinControllerPointer);
					break;

				default: // MP_AUDIO
                    AudioIns.push_back(std::make_unique<AudioInPin>());
					initializePin(idx, *(AudioIns.back()));
					break;
				}
			}
			else
			{
				AudioOuts.push_back(std::make_unique<AudioOutPin>());
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

	for (auto it = AudioOuts.begin(); it != AudioOuts.end(); ++it)
	{
		(*it)->setStreaming(true);
	}

	processData.inputEvents = &vstEventList;
	processData.inputParameterChanges = &parameterEvents;

	return MP_OK;
}

void ProcessorWrapper::initVst()
{
	if (!vstEffect_)
	{
		return;
	}

	processSetup = {
		pinOfflineRenderMode.getValue() == 2 ? kOffline : kRealtime,
		kSample32,
		getBlockSize(),
		getSampleRate()
	};

	if(vstEffect_->setupProcessing(processSetup) != kResultOk)
	{
		return;
	}

	processData.prepare (*component_, 0, processSetup.symbolicSampleSize);

	// init busses
	{
		for (MediaType type = kAudio; type <= kNumMediaTypes; type++) // audio or MIDI
		{
			for (int busDirection = kInput; busDirection <= kOutput; busDirection++)
			{
				auto busCount = component_->getBusCount(type, busDirection);

				if (kAudio == type && busCount > 0)
				{
					std::vector<int>& channelCounts = (busDirection == kInput) ? inputBusses : outputBusses;

					for (int busIndex = 0; busIndex < busCount; ++busIndex)
					{
						BusInfo busInfo{};
						if (component_->getBusInfo(kAudio, busDirection, busIndex, busInfo) == kResultTrue)
						{
							channelCounts.push_back(busInfo.channelCount);
						}
						else
						{
							break;
						}
					}
				}

				for (auto busIndex = 0; busIndex < busCount; ++busIndex)
				{
					component_->activateBus(type, busDirection, busIndex, true);
				}
			}
		}
	}

	component_->setActive(true);

	// init buffers to cope with latency
	latency = vstEffect_ ? vstEffect_->getLatencySamples() : 0;
	const auto bs = getBlockSize();
	const auto latencyBufferSize = ((bs + latency + bs - 1) / bs) * bs; // rounded up to nearest full block
	bypassDelays.resize(AudioIns.size());
	for (auto& delay : bypassDelays)
	{
		delay.resize(latencyBufferSize);
	}

	host.SetLatency(latency);

	currentVstSubProcess = &ProcessorWrapper::subProcess2<ST_PROCESS>;
}

void ProcessorWrapper::addParameterEvent(int clock, int id, float value)
{
	Steinberg::int32 returnIndexUnused = {};
	parameterEvents.addParameterData(id, returnIndexUnused)->addPoint(clock, value, returnIndexUnused);
}

void ProcessorWrapper::onMidiMessage(int pin, int timeDelta, const unsigned char* midiData, int size)
{
	assert(timeDelta >= 0);
	timeDelta = max(timeDelta, 0); // should never be nesc, but is safer to do

	if(pin == parameterAccessPinIndex)
	{
		if(GmpiMidiHdProtocol::isWrappedHdProtocol(midiData, size))
		{
			const auto m2 = (GmpiMidiHdProtocol::Midi2*) midiData;

			const int paramId = m2->key;
			const float normalized = *(float*)&m2->value;
			addParameterEvent(timeDelta, paramId, normalized);

#if 0 // def SE_TARGET_SEM
			// Also send parameter to Controller
			if (controller_)
			{
				controller_->UnsafeAddParameterChangeFromProcessor(paramId, normalized);
			}
#endif
		}
		return;
	}

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

		case GmpiMidi::MIDI_PolyAfterTouch:
            m.type = Event::kPolyPressureEvent;
            m.polyPressure.channel = channel;
            m.polyPressure.noteId = -1;
            m.polyPressure.pitch = midiData[1];
            m.polyPressure.pressure = midiData[2] / 127.0f;
            break;
		};
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

	blockPos_ = 0;
	int lblockPos = blockPos_;
	int remain = count;
	const MpEvent* next_event = events;

	for (;;)
	{
		if (next_event == 0) // fast version, when no events on list.
		{
			break;
		}

		assert(next_event->timeDelta < count); // Event will happen in this block

		int delta_time = next_event->timeDelta - lblockPos;

		if (delta_time > 0) // then process intermediate samples
		{
			eventsComplete_ = false;

			remain -= delta_time;

			eventsComplete_ = true;

			assert(remain != 0); // BELOW NEEDED?, seems non sense. If we are here, there is a event to process. Don't want to exit!
			if (remain == 0) // done
			{
				break;
			}

			lblockPos += delta_time;
		}

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
			if (e->eventType == EVENT_MIDI)
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

void ProcessorWrapper::subProcessNotLoaded(const int32_t count, const gmpi::MpEvent* events)
{
	ProcessEvents(count, events);

	for (auto& outBuffer : AudioOuts)
	{
		auto out = getBuffer(*outBuffer);

		// output silence.
		for (int s = count; s > 0; --s)
		{
			*out++ = 0.0f;
		}
	}

	vstTime_.continousTimeSamples += count;
}

void ProcessorWrapper::onSetPins(void)
{
	// Warning this method is not interleaved with processing like normal, all events are processed before running plugin process block.

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

	if (pinControllerPointer.isUpdated() && pinControllerPointer.getValue().getSize() == sizeof(ControllerWrapper*))
	{
		controller = *(ControllerWrapper**)pinControllerPointer.getValue().getData();

		controller->registerProcessor(&component_, &vstEffect_);

		initVst();
	}

	if (pinOfflineRenderMode.isUpdated() && vstEffect_)
	{
		const int32 newProcessMode = pinOfflineRenderMode.getValue() == 2 ? kOffline : kRealtime;
		if (newProcessMode != processSetup.processMode && vstEffect_)
		{
			// reset processor
			vstEffect_->setProcessing(false); // nesc?
			component_->setActive(false);

			processSetup = {
				newProcessMode,
				kSample32,
				getBlockSize(),
				getSampleRate()
			};

			if (vstEffect_->setupProcessing(processSetup) == kResultOk)
			{
				component_->setActive(true);
				vstEffect_->setProcessing(true);
			}
		}
	}

	if (pinOnOffSwitch.isUpdated())
    {
		targetLevel = pinOnOffSwitch.getValue() ? 1.0f : 0.0f;
    }
}

#ifdef CANCELLATION_TEST_ENABLE2
void ProcessorWrapper::debugDumpPresetToFile()
{
	MyBufferStream stream;

	component_->getState(&stream);

    auto data = (const unsigned char*)stream.buffer_.data();
	auto size = stream.buffer_.size();

	std::string outputFolder;
	outputFolder = "C:/temp/cancellation/" CANCELLATION_BRANCH "/preset_";
    int32_t handle;
    this->getHost()->getHandle(handle);
	outputFolder += std::to_string(handle);
	outputFolder += ".txt";

//		CreateFolderRecursive(Utf8ToWstring(outputFolder));
	std::ofstream presetEscaped;
	presetEscaped.open (outputFolder.c_str());

    {
        const int linelen = 100;
        bool wasOctal = false;

        presetEscaped << '"';
        int totallen = 0;
        for (int i = 0; i < size; ++i)
        {
            if (data[i] == '\"' || data[i] == '\\')
            {
                presetEscaped << '\\' << data[i];
                totallen += 2;
                wasOctal = false;
}
            else if (isalnum(data[i]) || ispunct(data[i]) || data[i] == ' ')
            {

                // Warning C4125 - decimal digit terminates octal escape sequence. i.e. number right after octal sequence.
                if (wasOctal && isdigit(data[i]))
                {
                    presetEscaped << "\"\""; // place two quotes to split string.
                }

                presetEscaped << data[i];
                totallen++;
                wasOctal = false;
            }
            else
            {
                // non-printable. use octal escape sequence.
                presetEscaped << "\\" << std::oct << std::setfill('0') << std::setw(3) << (int)data[i];
                totallen += 4;
                wasOctal = true;
            }
            if (totallen >= linelen || data[i] == '\n')
            {
                presetEscaped << "\"\n"
                                 "\t\t\"";
                totallen = 0;
                wasOctal = false;
            }
        }

        presetEscaped << '"';
    }
}
#endif