/* Copyright (c) 2007-2025 SynthEdit Ltd

Permission to use, copy, modify, and /or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include "Processor.h"
#include "Core/GmpiMidi.h"

using namespace gmpi;

struct keyInfo
{
//	int MidiKeyNumber = 0;
	float pitch = 0; // semitones
	bool held = false;
	int64_t noteSequence = -1;
};

struct outkeysInfo
{
	int64_t noteSequence = -1;
//	int inputKeyNumber = -1;
};

struct MidiUnison
{
	inline static const int keyCount = 128;
	inline static const int channelCount = 128;

	keyInfo noteIds[channelCount][keyCount];

	std::function<void(midi2::message_view)> sendMidi = [](midi2::message_view) {};

	int64_t noteSequence = 0;
	int unisonCount = 1;

	outkeysInfo outNotes[channelCount][keyCount];

	void onMidiMessage(midi2::message_view msg)
	{
		auto header = midi2::decodeHeader(msg);

		if (header.messageType != gmpi::midi2::ChannelVoice64)
			return;

		bool sendThrough = true;

		switch (header.status)
		{
		case gmpi::midi2::ControlChange:
		{
			const auto controller = gmpi::midi2::decodeController(msg);
//			const auto unified_controller_id = (ControllerType::CC << 24) | controller.type;
		}
		break;

		case gmpi::midi2::NoteOn:
		{
			sendThrough = false;
			const auto note = gmpi::midi2::decodeNote(msg);

			auto& info = noteIds[header.channel][note.noteNumber];

			if (!info.held)
			{
				info.held = true;
				info.noteSequence = noteSequence++;

				// _RPTN(0, "PM Note-on %d\n", note.noteNumber);
				if (gmpi::midi2::attribute_type::Pitch == note.attributeType) // !! this is only for the current note!!! not a permanent tuning change !!! TODO
				{
					//const auto timestamp_oversampled = voiceState->voiceControlContainer_->CalculateOversampledTimestamp(Container(), timestamp);

					info.pitch = note.attributeValue;
					//// _RPTN(0, "      ..pitch = %f\n", semitones);

					//voiceState->SetKeyTune(note.noteNumber, semitones);
					//voiceState->OnKeyTuningChangedA(timestamp_oversampled, note.noteNumber, 0);
				}
				else
				{
					info.pitch = static_cast<float>(note.noteNumber);
				}

				for (int i = 0; i < unisonCount; ++i)
				{
					// find oldest outgoing key.
					int64_t oldest = INT64_MAX;
					int outKey = 0;
					for (int j = 0; j < keyCount; ++j)
					{
						if (outNotes[header.channel][j].noteSequence < oldest)
						{
							outKey = j;
							oldest = outNotes[header.channel][j].noteSequence;
						}
					}

//					outNotes[header.channel][oldestIdx].inputKeyNumber = note.noteNumber;
					outNotes[header.channel][outKey].noteSequence = info.noteSequence;

					const auto out = gmpi::midi2::makeNoteOnMessageWithPitch(
						outKey,
						note.velocity,
						info.pitch,
						header.channel
					);

					sendMidi(out.m);
				}
			}
		}
		break;

		case gmpi::midi2::NoteOff:
		{
			sendThrough = false;
			const auto note = gmpi::midi2::decodeNote(msg);

			auto& info = noteIds[header.channel][note.noteNumber];

			if (info.held)
			{
				info.held = false;

				for (int outKey = 0; outKey < keyCount; ++outKey)
				{
					auto& outNote = outNotes[header.channel][outKey];
					if (outNote.noteSequence == info.noteSequence)
					{
						const auto out = gmpi::midi2::makeNoteOffMessage(
							outKey,
							note.velocity,
							header.channel
						);

						sendMidi(out.m);
					}
				}
			}
		}
		break;
		};

		if (sendThrough)
		{
			sendMidi(msg);
		}
	}
};

struct Unison final : public Processor
{
	MidiInPin pinMIDIIn;
	MidiOutPin pinMIDIOut;
	IntInPin pinVoiceCount;
	FloatInPin pinSpread;
	IntInPin pinNoteLo;
	IntInPin pinNoteHi;
	IntInPin pinVelocityLo;
	IntInPin pinVelocityHi;
	IntInPin pinProgramChange;

	MidiUnison noteTracker;

	Unison()
	{
		init( pinMIDIIn );
		init( pinMIDIOut );
		init( pinVoiceCount );
		init( pinSpread );
		init( pinNoteLo );
		init( pinNoteHi );
		init( pinVelocityLo );
		init( pinVelocityHi );
		init( pinProgramChange );

		noteTracker.sendMidi = [this](midi2::message_view msg)
		{
			pinMIDIOut.send(msg);
		};
	}

	void onSetPins() override
	{
		// Check which pins are updated.
		if( pinVoiceCount.isUpdated() )
		{
			noteTracker.unisonCount = pinVoiceCount;
		}
		if( pinSpread.isUpdated() )
		{
		}
		if( pinNoteLo.isUpdated() )
		{
		}
		if( pinNoteHi.isUpdated() )
		{
		}
		if( pinVelocityLo.isUpdated() )
		{
		}
		if( pinVelocityHi.isUpdated() )
		{
		}
		if( pinProgramChange.isUpdated() )
		{
		}
	}

	void onMidiMessage(int pin, std::span<const uint8_t> midiMessage) override
	{
		noteTracker.onMidiMessage(midiMessage);
	}
};

namespace
{
auto r = Register<Unison>::withXml(R"XML(
<?xml version="1.0" encoding="UTF-8"?>
<PluginList>
    <Plugin id="SE Unison" name="Unison" category="MIDI">
        <Audio>
            <Pin name="MIDI In" datatype="midi"/>
            <Pin name="MIDI Out" datatype="midi" direction="out"/>
            <Pin name="Unison Count" datatype="enum" default="1" metadata="range 1,16"/>
            <Pin name="Spread (cents)" datatype="float" default="5" metadata="range 1,20"/>
            <Pin name="Note Lo" datatype="enum" metadata="range 0,127"/>
            <Pin name="Note Hi" datatype="enum" default="127" metadata="range 0,127"/>
            <Pin name="Velocity Lo" datatype="enum" metadata="range 0,127"/>
            <Pin name="Velocity Hi" datatype="enum" default="127" metadata="range 0,127"/>
            <Pin name="Program Change" datatype="enum" default="1" metadata="Off,On"/>
        </Audio>
    </Plugin>
</PluginList>
)XML");
}
