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

class MidiNoteTracker
{
public:

	void onMidiMessage(midi::message_view msg)
	{
		auto header = midi_2_0::decodeHeader(msg);
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

	MidiNoteTracker noteTracker;

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
	}

	void onSetPins() override
	{
		// Check which pins are updated.
		if( pinVoiceCount.isUpdated() )
		{
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

	void onMidiMessage(int pin, const uint8_t* midiMessage, int size) override
	{
		noteTracker.onMidiMessage({ midiMessage, size });

		pinMIDIOut.send(midiMessage, size);
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
