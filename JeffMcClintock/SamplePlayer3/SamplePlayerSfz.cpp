#include <memory>
#include <thread>
#include <mutex>

#include "mp_sdk_audio.h"
#include "mp_midi.h"
#include "unicode_conversion2.h"
#include "sfizz.hpp"

using namespace gmpi;

class SamplePlayerSfz : public MpBase2
{
	MidiInPin pinMIDIIn;
	StringInPin pinFilename;
	IntInPin pinVoiceCount;
	IntInPin pinOversampling;
	AudioOutPin pinOutputLeft;
	AudioOutPin pinOutputRight;

    // synth state. acquire processMutex before accessing
    std::unique_ptr<sfz::Sfizz> _synth;

    // worker and thread sync
    std::mutex _processMutex;

    std::thread _worker;
    std::mutex backgroundMutex;
    std::condition_variable backgroundSignal;
    bool closeBackgroundThread = false;

    gmpi::midi_2_0::MidiConverter2 midiConverter;

public:
	SamplePlayerSfz() :
        midiConverter(
            // provide a lambda to accept converted MIDI 2.0 messages
            [this](const midi::message_view& msg, int offset)
            {
                onMidi2Message(msg, offset);
            }
        )
    {
		initializePin( pinMIDIIn );
//		initializePin( pinChannel );
		initializePin( pinFilename );
		initializePin( pinVoiceCount );
		initializePin( pinOversampling );
		initializePin( pinOutputLeft );
		initializePin( pinOutputRight );
	}

    ~SamplePlayerSfz()
    {
        // ask thread to stop
        {
            std::lock_guard<std::mutex> lk(backgroundMutex);
            closeBackgroundThread = true;
        }
        signalBackgroundThread();

        _worker.join();
    }

    void signalBackgroundThread()
    {
        backgroundSignal.notify_one();
    }

    void doBackgroundWork()
    {
        std::unique_lock<std::mutex> lk(backgroundMutex);
        while (!closeBackgroundThread)
        {
            // Perform time-consuming operations.
            if (pinVoiceCount.getValue() > 0) // skip this until pins are init to proper values.
            {
                _synth->setNumVoices((std::max)(1, pinVoiceCount.getValue()));
                _synth->setOversamplingFactor(pinOversampling);
            }

            // wait until signaled.
            backgroundSignal.wait(lk);
        }
    }

    virtual int32_t MP_STDCALL open() override
    {
        _synth.reset(new sfz::Sfizz);

        _synth->tempo(0, 0.5);
        _synth->timeSignature(0, 4, 4);
        _synth->timePosition(0, 0, 0);
        _synth->playbackState(0, 0);

        _synth->setSampleRate(getSampleRate());
        _synth->setSamplesPerBlock(getBlockSize());

        _worker = std::thread([this]() { doBackgroundWork(); });

        return MpBase2::open();
    }

    void MP_STDCALL process(int32_t sampleFrames, const gmpi::MpEvent* events) override
    {
        std::unique_lock<std::mutex> lock(_processMutex, std::try_to_lock);

        // handle events
#if defined(_DEBUG)
        blockPosExact_ = false;
#endif

        blockPos_ = 0;
        int lblockPos = blockPos_;
        int remain = sampleFrames;
        const MpEvent* next_event = events;

        for (;;)
        {
            if (next_event == 0) // fast version, when no events on list.
            {
                break;
            }

            assert(next_event->timeDelta < sampleFrames); // Event will happen in this block

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
            do {
                preProcessEvent(e); // updates all pins_ values
                pins_set_flag = pins_set_flag || e->eventType == EVENT_PIN_SET || e->eventType == EVENT_PIN_STREAMING_START || e->eventType == EVENT_PIN_STREAMING_STOP;
                e = e->next;
            } while (e != 0 && e->timeDelta == lblockPos);

            // PROCESS EVENT
            e = next_event;
            do {
                if (e->eventType == EVENT_MIDI)
                {
                    if (e->extraData == 0) // short msg
                    {
                        midiConverter.processMidi({ (const unsigned char*)&(e->parm3), e->parm2 }, e->timeDelta);
                    }
                    else
                    {
                        midiConverter.processMidi({ (const unsigned char*)(e->extraData), e->parm2 }, e->timeDelta);
                    }
                }
                else
                {
                    processEvent(e); // notify all pins_ values
                }
                e = e->next;
            } while (e != 0 && e->timeDelta == lblockPos);

            if (pins_set_flag) {
                onSetPins();
            }

            // POST-PROCESS EVENT
            do {
                postProcessEvent(next_event);
                next_event = next_event->next;
            } while (next_event != 0 && next_event->timeDelta == lblockPos);

#if defined(_DEBUG)
            blockPosExact_ = false;
#endif
        }

        // Process audio
        constexpr int numChannels = 2;
        float* outputs[numChannels] = {
            getBuffer(pinOutputLeft),
            getBuffer(pinOutputRight)
        };

        if (!lock.owns_lock()) {
            for (unsigned c = 0; c < numChannels; ++c)
                std::memset(outputs[c], 0, sampleFrames * sizeof(float));
            return;
        }

        _synth->renderBlock(outputs, sampleFrames, numChannels / 2);
    }

    void onMidi2Message(const midi::message_view msg, int delta)
    {
        const auto header = gmpi::midi_2_0::decodeHeader(msg);

        // only 8-byte messages supported.
        if (header.messageType != gmpi::midi_2_0::ChannelVoice64)
            return;

        switch (header.status)
        {
        case gmpi::midi_2_0::NoteOn:
        {
            const auto note = gmpi::midi_2_0::decodeNote(msg);
            _synth->hdNoteOn(delta, note.noteNumber, note.velocity);
        } break;

        case gmpi::midi_2_0::NoteOff:
        {
            const auto note = gmpi::midi_2_0::decodeNote(msg);
            _synth->hdNoteOff(delta, note.noteNumber, note.velocity);
        } break;

        case gmpi::midi_2_0::PolyAfterTouch: {
            const auto aftertouch = gmpi::midi_2_0::decodePolyController(msg);
            _synth->hdPolyAftertouch(delta, aftertouch.noteNumber, aftertouch.value);
        } break;

        case gmpi::midi_2_0::ControlChange: {
            const auto controller = gmpi::midi_2_0::decodeController(msg);
            _synth->hdcc(delta, controller.type, controller.value);
        } break;

        case gmpi::midi_2_0::ChannelPressue: {
            const auto normalized = gmpi::midi_2_0::decodeController(msg).value;
            _synth->hdChannelAftertouch(delta, normalized);
        } break;

        case gmpi::midi_2_0::PitchBend: {
            const auto normalized = gmpi::midi_2_0::decodeController(msg).value;
            _synth->hdPitchWheel(delta, normalized);
        } break;

        //case gmpi::midi_2_0::SystemMessage: {
        //} break;

        default:
            break;
        };
    }

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if( pinVoiceCount.isUpdated() || pinOversampling.isUpdated())
		{
            signalBackgroundThread();
		}
		if( pinFilename.isUpdated() )
		{
            wchar_t fullFilename[500];
		    getHost()->resolveFilename( pinFilename.getValue().c_str(), sizeof(fullFilename)/sizeof(fullFilename[0]), fullFilename );
            std::wstring temp(fullFilename);

           const auto filename = FastUnicode::WStringToUtf8(temp);

            _synth->loadSfzFile(filename);
		}

		// Set state of output audio pins.
		pinOutputLeft.setStreaming(true);
		pinOutputRight.setStreaming(true);

		// Set sleep mode (optional).
		setSleep(false);
	}
};

namespace
{
	auto r = Register<SamplePlayerSfz>::withId(L"My Sample Player - SFZ");
}
