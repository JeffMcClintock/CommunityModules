#include "./Wrapper_VST2.h"
#include "../se_sdk3/mp_midi.h"
#include "../shared/xplatform.h"
#include <algorithm>

SE_DECLARE_INIT_STATIC_FILE(Vst2Wrapper)

using namespace std;
using namespace gmpi;
using namespace GmpiMidi;

Vst2Wrapper::Vst2Wrapper(VstIntPtr shellPluginId, bool pUseChunkPresets) :
	 shellPluginId_(shellPluginId)
	, vstEffect_(nullptr)
	, useChunkPresets(pUseChunkPresets)
	, OnOffSwitchEnabled(false)
	, bypassMode(true)
	, tailSamples(32)
	, currentVstSubProcess(&Vst2Wrapper::subProcessBypass)
#if defined(DEBUG_VST2_SIGNAL_LEVEL )
	,peakLevel(0)
	,peakCounter(0)
#endif
{
	memset(&vstTime_, 0, sizeof(vstTime_));
	vstTime_.flags = kVstTempoValid | kVstTimeSigValid | kVstBarsValid | kVstPpqPosValid;

	// reasonable defaults for now.
	vstTime_.tempo = 100;
	vstTime_.timeSigDenominator = 4;
	vstTime_.timeSigNumerator = 4;
}

void Vst2Wrapper::onAEffectWrapperDestroyed()
{
    if( vstEffect_ )
    {
        vstEffect_->releaseHost();
        vstEffect_ = 0;
    }
}
Vst2Wrapper::~Vst2Wrapper()
{
	if( vstEffect_ )
	{
        vstEffect_->dispatcher(effMainsChanged, 0, 0); // power off - suspend()
		vstEffect_->releaseHost();
	}
}

int32_t Vst2Wrapper::open()
{
	MpBase2::open();

	// Setup IO.
	gmpi_sdk::mp_shared_ptr<gmpi::IMpPinIterator> it;
	if (MP_OK == getHost()->createPinIterator(it.asIMpUnknownPtr()))
	{
		int r = it->first();
		int idx = 0;
		while (r == MP_OK)
		{
			int32_t direction;
			int32_t datatype;
			int32_t id;
			it->getPinDirection(direction);
			it->getPinId(id);
			it->getPinDatatype(datatype);

			if (direction == gmpi::MP_IN)
			{
				// enum EPlugDataType { DT_ENUM, DT_TEXT, DT_MIDI2, DT_DOUBLE, DT_BOOL, DT_FSAMPLE, DT_FLOAT, DT_VST_PARAM, DT_INT, DT_INT64, DT_BLOB, DT_NONE=-1 };  //plug datatype

				switch (datatype)
				{
					// NEW, Older exports won't include these pins, so only initialize them if pinOnOffSwitch detected (it's the only bool).
				case MP_BOOL:
					OnOffSwitchEnabled = true;
					initializePin(idx++, pinOnOffSwitch);
					r = it->next();

					// This pin added later. Check if it's in XML yet. Only need this if statement until both Grand80 and RHP ported again.
					it->getPinDatatype(datatype);
					if (datatype == MP_BOOL)
					{
						initializePin(idx++, pinAutoSleep);
						r = it->next();
					}
					else if (datatype == MP_INT32)
					{
						initializePin(idx++, pinProcessMode);
						r = it->next();
					}
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
					initializePin(idx, pinMidi);
					break;

					// This does not apply when Parameter pins are on GUI.
				case MP_FLOAT32:
// not VS2012		ParameterPins.push_back(make_unique<FloatInPin>());
					ParameterPins.push_back( std::unique_ptr<FloatInPin>( new FloatInPin() ));

					initializePin(idx, *(ParameterPins.back()));
					break;

				case MP_BLOB:
					initializePin(idx, pinAeffectPointer);
					break;

				default: // MP_AUDIO
// not VS2012		AudioIns.push_back(make_unique<AudioInPin>());
					AudioIns.push_back( std::unique_ptr<AudioInPin>( new AudioInPin() ));
					initializePin(idx, *(AudioIns.back()));
					break;
				}
			}
			else
			{
				//AudioOuts.push_back(make_unique<AudioOutPin>());
				AudioOuts.push_back( std::unique_ptr<AudioOutPin>( new AudioOutPin() ));
				initializePin(idx, *(AudioOuts.back()));
			}
			r = it->next();
			++idx;
		}
	}

	vstTime_.sampleRate = (double)getSampleRate();

	// Preserve FPU state (Waves plugins trash it).
#if _MSC_VER >= 1600 // Not Avail in VS2005.
	unsigned int fpState;
	_controlfp_s(&fpState, 0, 0);
#endif

	for (auto it = AudioOuts.begin(); it != AudioOuts.end(); ++it)
	{
		(*it)->setStreaming(true);
	}

#if _MSC_VER >= 1600 // Not Avail in VS2005.
	unsigned int unused;
#if defined (_WIN32) && !defined(_WIN64)
	_controlfp_s(&unused, fpState, MCW_PC | _MCW_DN);
#else
	_controlfp_s(&unused, fpState, _MCW_DN);
#endif
#endif

	return MP_OK;
}

void Vst2Wrapper::initVst()
{
	bool success = vstEffect_ != 0 && vstEffect_->IsLoaded();
	if (!success)
	{
		vstEffect_ = 0;
		bypassMode = true;
		currentVstSubProcess = &Vst2Wrapper::subProcessBypass;
	}
	else
	{
		vstEffect_->setHost(this);

		vstEffect_->Resume();

		inputBuffers.assign(vstEffect_->getNumInputs(), 0);
		outputBuffers.assign(vstEffect_->getNumOutputs(), 0);

		tailSamples = (std::max)(32, static_cast<int>( vstEffect_->dispatcher(effGetTailSize) + getBlockSize() ));

		for (size_t i = 0; i < ParameterPins.size(); ++i)
		{
			auto value = ParameterPins[i]->getValue();
			if (value > -0.5f) // -1 has special meaning - "ignore" DSP pin, use parameter value from preset.
			{
				vstEffect_->setParameter(static_cast<int>(i), value);
			}
		}
	}
}

void Vst2Wrapper::onMidiMessage(int pin, int timeDelta, const unsigned char* midiMessage, int size)
{
//	int due_time_delta = getBlockPosition();
	assert(timeDelta >= 0);

	timeDelta = max(timeDelta, 0); // should never be nesc, but is safer to do
	int dataSize = size;
	const unsigned char* midiData = midiMessage;

	int msgSize = size;
	// 1.1, complaints system msg needed.		if( !is_system_msg ) // FMHeaven crashes if sent system messages
	// pass to plug. !! NEED TO CHECK IT SUPPORTS EVENTS!!
	VstEvent* vstEvent = 0;
	int b1 = midiData[0];

	if (b1 != MIDI_SystemMessage)
	{
		int chan = b1 & 0x0f;
		bool is_system_msg = (b1 & MIDI_SystemMessage) == MIDI_SystemMessage;
/*
		if (!is_system_msg && midi_channel != -1)
		{
			b1 = b1 & 0xf0; //!jason freesonic only responds to chan 1 (zero), mask off any chan #
		}
*/

		VstMidiEvent* m = new VstMidiEvent;
		m->type = kVstMidiType;
		m->noteLength = 0;	// (in sample frames) of entire note, if available, else 0
		m->noteOffset = 0;	// offset into note from note start if available, else 0
							// m->midiData[0]		   1 thru 3 midi bytes; midiData[3] is reserved (zero)
		m->midiData[1] = (char)0xff;	// if unused must be 0xff
		m->midiData[2] = (char)0xff;
		m->midiData[3] = 0;

		for (int i = 0; i < msgSize; ++i)
		{
			m->midiData[i] = midiData[i];
		}

		m->detune = 0;		// -64 to +63 cents; for scales other than 'well-tempered' ('microtuning')
		m->noteOffVelocity = 0;
		m->reserved1 = 0;		// zero
		m->reserved2 = 0;		// zero
		vstEvent = (VstEvent*)m;
	}
	else // SYSEX
	{
		VstMidiSysexEvent* m = new VstMidiSysexEvent;
		m->type = kVstSysExType;
		m->dumpBytes = msgSize;
		m->resvd1 = m->resvd2 = 0; // zero
		m->sysexDump = (char*)midiData;
		vstEvent = (VstEvent*)m;
	}

	vstEvent->byteSize = 24;		// 24
	vstEvent->deltaFrames = timeDelta; // sample frames related to the current block start sample position
	vstEvent->flags = 0;			// none defined yet
	vstEventList.Add(vstEvent);
}

void Vst2Wrapper::ProcessEvents(int32_t count, const gmpi::MpEvent* events)
{
	assert(count > 0);

#if defined(_DEBUG)
	blockPosExact_ = false;
#endif

	vstEventList.Clear();

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
			processEvent(e); // notify all pins_ values
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

void Vst2Wrapper::subProcessBypass(int32_t count, const gmpi::MpEvent* events)
{
	ProcessEvents(count, events);

	CopyInputToOutput(count);
}

void Vst2Wrapper::subProcessBypassSilence(int32_t count, const gmpi::MpEvent* events)
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

void Vst2Wrapper::subProcessSilence(int32_t count, const gmpi::MpEvent* events)
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
		if (currentVstSubProcess == &Vst2Wrapper::subProcessSilence) // it might already be witched elsewhere by "power" button.
		{
			currentVstSubProcess = &Vst2Wrapper::subProcessPreSleep;
		}
	}
	else
	{
		CopySilenceToOutput(count);
	}
}

// This processes the plugin, while waiting for the 'tail' to die away.
void Vst2Wrapper::subProcessPreSleep(int32_t count, const gmpi::MpEvent* events)
{
	if (silenceCounter < 0 && events == nullptr )
	{
		silenceCounter = getBlockSize();
		currentVstSubProcess = &Vst2Wrapper::subProcessSilence;
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
}

void Vst2Wrapper::subProcess(int32_t count, const gmpi::MpEvent* events)
{
#ifdef CANCELLATION_TEST_ENABLE2
    if (!cancellation_done && vstTime_.samplePos > CANCELLATION_SNAPSHOT_TIMESTAMP) // don't account for oversample, but should be good enough
    {
		cancellation_done = true;
		debugDumpPresetToFile();
    }
#endif

	ProcessEvents(count, events);

	ProcessPlugin(count);

#if defined(DEBUG_VST2_SIGNAL_LEVEL )
	for (size_t i = 0; i < AudioOuts.size(); ++i)
	{
		auto o = getBuffer(*(AudioOuts[i]));
		for (int i = 0; i < count; ++i)
		{
			peakLevel = (std::max)(peakLevel, o[i]);
			peakLevel = (std::max)(peakLevel, -o[i]);
		}
	}

	if (peakCounter-- < 0)
	{
		peakCounter = 100;
		int handle;
		getHost()->getHandle(handle);
		_RPT2(_CRT_WARN, "Child Plugin %d peak %f\n", handle, peakLevel);
		peakLevel = 0;
	}
#endif
}

void Vst2Wrapper::onSetPins(void)
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

	if (pinAeffectPointer.isUpdated() && pinAeffectPointer.getValue().getSize() == sizeof(vstEffect_) )
	{
		assert(vstEffect_ == nullptr);
		vstEffect_ = *(IAEffect**)pinAeffectPointer.getValue().getData();

		initVst();
	}

	if (vstEffect_)
	{
		int32_t handle;
		this->getHost()->getHandle(handle);

		// Check for parameter updates.
		for (size_t i = 0; i < ParameterPins.size(); ++i)
		{
			if (ParameterPins[i]->isUpdated())
			{
				auto value = ParameterPins[i]->getValue();
				if (value > -0.5f) // -1 has special meaning - "ignore" DSP pin, use parameter value from preset.
				{
					vstEffect_->setParameter(static_cast<int>(i), value);
                    _RPT3(0, "   setParameter %9d: %2d -> %f\n", handle, i, value);
				}
			}
		}
	}

	bypassMode = vstEffect_ == 0 || (OnOffSwitchEnabled && pinOnOffSwitch == false);

	bool inputStreaming = false;
	bool inputUpdated = pinOnOffSwitch.isUpdated() | pinAutoSleep.isUpdated();
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

			currentVstSubProcess = &Vst2Wrapper::subProcessBypass;
		}
		else
		{
			if (inputUpdated)
			{
				silenceCounter = getBlockSize();
			}

			currentVstSubProcess = &Vst2Wrapper::subProcessBypassSilence;
		}
	}
	else
	{
//		bool inputIsActive = !OnOffSwitchEnabled || pinAutoSleep == false || inputStreaming;
		const bool inputIsActive = true; // now pinAutoSleep = false

		if (inputIsActive)
		{
			if (!outputStreaming)
			{
				for (auto& outPin : AudioOuts)
				{
					outPin->setStreaming(true);
				}
			}

			currentVstSubProcess = &Vst2Wrapper::subProcess;
		}
		else
		{
			if (inputUpdated)
			{
				silenceCounter = getBlockSize();
				currentVstSubProcess = &Vst2Wrapper::subProcessPreSleep;
			}
		}
	}
}

float Vst2Wrapper::hSampleRate(void)
{
	return getSampleRate();
}

VstTimeInfo* Vst2Wrapper::hHostGetTime(int mask)
{
	if( mask & kVstSmpteValid )
	{
		vstTime_.smpteFrameRate = 0; // 0:24
		double frames_per_sec = 24.0;
		double seconds = vstTime_.samplePos / vstTime_.sampleRate;
		double frames = frames_per_sec * seconds;
		vstTime_.smpteOffset = (int)frames;// / 100.0;
		vstTime_.flags |= kVstSmpteValid;
	}

	if( ( mask & kVstNanosValid ) != 0 )
	{
		vstTime_.nanoSeconds = 1000.0 * vstTime_.samplePos / vstTime_.sampleRate;
		vstTime_.flags |= kVstNanosValid;
	}

	vstTime_.flags |= kVstTransportPlaying;

	if( mask & kVstPpqPosValid )
	{
		if( ( vstTime_.flags & kVstTransportPlaying ) != 0 )
		{
			/* todo
			vstTime_.ppqPos = total_midi_clocks / 24.f;

			if( mask & kVstBarsValid )
			{
				vstTime_.barStartPos = floor(vstTime_.ppqPos);
			}

			double remaining_samples = ( floor(total_midi_clocks) - total_midi_clocks ) / samples_to_clocks;
			vstTime_.samplesToNextClock = (int)remaining_samples;
			assert(vstTime_.samplesToNextClock < 3000);
			*/
			/*
			'next' was supposed to mean 'nearest in time' which can be prior or next
			to the current position.  you can tell from the sign if it's prior or
			after the current position. Cubase SX is always -ve
			*/
		}

		vstTime_.flags |= kVstPpqPosValid;
	}

	return &vstTime_;
}

#ifdef CANCELLATION_TEST_ENABLE2
#include <iostream>
#include <fstream>
#include <iomanip>

void Vst2Wrapper::debugDumpPresetToFile()
{
	const int isPreset = 1; // 0 = all presets.
	void* chunkPtr = nullptr;
	auto chunkSize = vstEffect_->dispatcher(effGetChunk, isPreset, 0, &chunkPtr);

	auto data = (const unsigned char*)chunkPtr;
	auto size = chunkSize;
    int32_t handle = {};
	getHost()->getHandle(handle);

	// see also VST3 ControllerWrapper::debugDumpPresetToFile()
	std::string outputFolder;
	outputFolder = "C:/temp/cancellation/" CANCELLATION_BRANCH "/preset_";
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

///////////////////////////////////////////////
// VstEventsWrapper ////////////////////////////////

VstEventsWrapper::VstEventsWrapper() :
	m_vst_events(NULL)
	, m_max_size(0)
{
	SetSize(10);
}

VstEventsWrapper::~VstEventsWrapper()
{
	Clear();
	free(m_vst_events);
}

void VstEventsWrapper::SetSize(int n)
{
	// allocate enough for VstEvents header structure, plus pointers to 'n' VstEvent structs
	int total_size = sizeof(VstEvents) + sizeof(VstEvent*) * n;
	// allocate new array of events
	VstEvents* new_vst_events = (VstEvents*)malloc(total_size);
	new_vst_events->numEvents = 0;
	new_vst_events->reserved = 0;			// zero
											// copy existing pointers over
	int i = 0;

	if (m_vst_events != NULL)
	{
		new_vst_events->numEvents = m_vst_events->numEvents;

		for (; i < m_vst_events->numEvents; i++)
		{
			new_vst_events->events[i] = m_vst_events->events[i];
		}

		free(m_vst_events);
	}

	// set remaining pointers to zero
	for (; i < n; i++)
	{
		new_vst_events->events[i] = NULL;
	}

	m_vst_events = new_vst_events;
	m_max_size = n;
}

// enhancement: keep old vst events around for re-use
// this class would have to manage allocating !!
void VstEventsWrapper::DoClear()
{
	// set pointers to zero
	for (int i = 0; i < m_vst_events->numEvents; i++)
	{
		delete m_vst_events->events[i];
		m_vst_events->events[i] = NULL;
	}

	m_vst_events->numEvents = 0;
}

void VstEventsWrapper::Add(VstEvent* p_event)
{
	if (m_vst_events->numEvents == m_max_size)
	{
		SetSize(m_max_size + 10);
	}

	assert(m_vst_events->numEvents >= 0 && m_vst_events->numEvents < m_max_size);
	m_vst_events->events[m_vst_events->numEvents++] = p_event;
}
