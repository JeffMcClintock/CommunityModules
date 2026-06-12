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

// SFZ sample-player (sfizz engine) with 8 independent stereo outputs.
// Regions are routed to outputs with the SFZ 'output' opcode (output=0..7).
// This is the GMPI port of the legacy 'SamplePlayer3' module, extended to multiple outputs.

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cstring>

#include "Core/Processor.h"
#include "Core/GmpiMidi.h"
#include "../Extensions/EmbeddedFileHelper.h"

#include "sfizz.hpp"

using namespace gmpi;

class SamplePlayerSfzMulti final : public Processor
{
	static constexpr int kNumStereoOutputs = 8;                 // 8 stereo pairs
	static constexpr int kNumChannels      = kNumStereoOutputs * 2; // 16 audio channels

	// ---- pins. Declaration order sets the pin index, and must match the XML below. ----
	BoolInPin   pinPower;
	MidiInPin   pinMIDIIn;
	StringInPin pinFilename;
	IntInPin    pinVoiceCount;
	AudioOutPin pinOutputs[kNumChannels]; // interleaved stereo pairs: [L0,R0, L1,R1, ... L7,R7]
	IntInPin    pinProcessMode;
	BoolOutPin  pinLoaded;

	// ---- sfizz engine. Hold _processMutex before touching it from the audio thread. ----
	std::unique_ptr<sfz::Sfizz> _synth;

	// ---- background (file-loading) worker, keeps slow SFZ loads off the audio thread. ----
	std::mutex _processMutex;
	std::thread _worker;
	std::atomic<bool> closeBackgroundThread{ false };
	bool firstTime = true;

	std::string loadFilename;
	std::atomic<bool> loadNewZfzFlag{ false };
	std::atomic<bool> isLoading{ false };

	// true while the engine must not be played (powered-off, or a reload is in progress). Audio thread only.
	bool isBypassed = false;

public:
	~SamplePlayerSfzMulti()
	{
		closeBackgroundThread = true;
		if (_worker.joinable())
			_worker.join();
	}

	// Background thread: applies the voice count and performs (slow) SFZ file loads.
	void doBackgroundWork()
	{
		while (!closeBackgroundThread)
		{
			updateSfizz();
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}
	}

	void updateSfizz()
	{
		_synth->setNumVoices((std::max)(1, pinVoiceCount.getValue()));

		const bool shouldReload = loadNewZfzFlag.exchange(false) || _synth->shouldReloadFile();
		if (shouldReload)
		{
			std::unique_lock<std::mutex> lock(_processMutex);
			_synth->loadSfzFile(loadFilename);
#ifdef _DEBUG
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif
			isLoading = false;
		}
	}

	ReturnCode open(api::IUnknown* phost) override
	{
		const auto rc = Processor::open(phost);
		if (rc != ReturnCode::Ok)
			return rc;

		_synth = std::make_unique<sfz::Sfizz>();

		_synth->tempo(0, 0.5);
		_synth->timeSignature(0, 4, 4);
		_synth->timePosition(0, 0, 0);
		_synth->playbackState(0, 0);
		_synth->setSampleRate(host->getSampleRate());
		_synth->setSamplesPerBlock(host->getBlockSize());

		return rc;
	}

	// We override process() (rather than the usual subProcess) so sfizz renders the entire block in
	// one call, with MIDI applied sample-accurately via its per-event delay. The event loop below
	// mirrors Processor::process(), minus the audio sub-blocks.
	void process(int32_t count, const api::Event* events) override
	{
		// Take the engine lock for the whole block. If the loader thread holds it (reload in
		// progress) we output silence and drop MIDI rather than touch a half-loaded engine.
		std::unique_lock<std::mutex> lock(_processMutex, std::try_to_lock);
		isBypassed = !pinPower.getValue() || !lock.owns_lock();

		// Report 'loaded' once any asynchronous load has finished.
		if (!pinLoaded.getValue() && !isLoading)
			pinLoaded.setValue(true, 0);

		blockPos_ = 0;
#if defined(_DEBUG)
		blockPosExact_ = false;
#endif
		auto next_event = events;
		while (next_event)
		{
			blockPos_ = next_event->timeDelta;
#if defined(_DEBUG)
			blockPosExact_ = true;
#endif
			// PRE-PROCESS all events sharing this timestamp.
			bool pinsSet = false;
			auto e = next_event;
			do {
				preProcessEvent(e);
				pinsSet = pinsSet
					|| e->eventType == api::EventType::PinSet
					|| e->eventType == api::EventType::PinStreamingStart
					|| e->eventType == api::EventType::PinStreamingStop;
				e = e->next;
			} while (e && e->timeDelta == blockPos_);

			// PROCESS (MIDI -> onMidiMessage(), pin updates -> pin values).
			e = next_event;
			do {
				processEvent(e);
				e = e->next;
			} while (e && e->timeDelta == blockPos_);

			if (pinsSet)
				onSetPins();

			// POST-PROCESS (clears the per-sample 'updated' flags).
			do {
				postProcessEvent(next_event);
				next_event = next_event->next;
			} while (next_event && next_event->timeDelta == blockPos_);
#if defined(_DEBUG)
			blockPosExact_ = false;
#endif
		}

		// ---- render the whole block ----
		float* outputs[kNumChannels];
		for (int i = 0; i < kNumChannels; ++i)
			outputs[i] = pinOutputs[i].begin();

		if (isBypassed)
		{
			for (int c = 0; c < kNumChannels; ++c)
				std::memset(outputs[c], 0, count * sizeof(float));
		}
		else
		{
			_synth->renderBlock(outputs, count, kNumStereoOutputs);
		}
	}

	// Called by the base class for each MIDI event; blockPos_ is the event's sample offset.
	void onMidiMessage(int /*pin*/, std::span<const uint8_t> midiMessage) override
	{
		if (isBypassed) // don't feed the engine while it is reloading (crash) or powered off.
			return;

		const auto header = midi2::decodeHeader(midiMessage);

		// only 8-byte MIDI 2.0 channel-voice messages are supported.
		if (header.messageType != midi2::ChannelVoice64)
			return;

		const int delta = getBlockPosition();

		switch (header.status)
		{
		case midi2::NoteOn:
		{
			const auto note = midi2::decodeNote(midiMessage);
			// slightly increase velocity to compensate for crude rounding-down in sfizz.
			constexpr float preventRoundingDown = 0.01f / 127.0f;
			const float velocity = (std::min)(1.0f, note.velocity + preventRoundingDown);
			_synth->hdNoteOn(delta, note.noteNumber, velocity);
		} break;

		case midi2::NoteOff:
		{
			const auto note = midi2::decodeNote(midiMessage);
			constexpr float preventRoundingDown = 0.01f / 127.0f;
			const float velocity = (std::min)(1.0f, note.velocity + preventRoundingDown);
			_synth->hdNoteOff(delta, note.noteNumber, velocity);
		} break;

		case midi2::PolyAfterTouch:
		{
			const auto aftertouch = midi2::decodePolyController(midiMessage);
			_synth->hdPolyAftertouch(delta, aftertouch.noteNumber, aftertouch.value);
		} break;

		case midi2::ControlChange:
		{
			const auto controller = midi2::decodeController(midiMessage);
			_synth->hdcc(delta, controller.type, controller.value);
		} break;

		case midi2::ChannelPressue:
		{
			_synth->hdChannelAftertouch(delta, midi2::decodeController(midiMessage).value);
		} break;

		case midi2::PitchBend:
		{
			_synth->hdPitchWheel(delta, midi2::decodeController(midiMessage).value);
		} break;

		default:
			break;
		}
	}

	void onSetPins() override
	{
		if (pinFilename.isUpdated())
		{
			synthedit::EmbeddedFileHostWrapper fileHost;
			fileHost.Init(host.get());
			loadFilename = fileHost.resolveFilename(pinFilename.getValue());

			if (pinProcessMode == 2) // Offline render: load synchronously.
			{
				_synth->loadSfzFile(loadFilename);
			}
			else
			{
				isLoading = true;
				pinLoaded = false;
				loadNewZfzFlag.store(true);
			}
		}

		if (firstTime)
		{
			if (pinProcessMode == 2) // Offline
				_synth->setNumVoices((std::max)(1, pinVoiceCount.getValue()));

			_worker = std::thread([this]() { doBackgroundWork(); });
			firstTime = false;
		}

		// Output pins stream whenever powered.
		const bool power = pinPower.getValue();
		for (auto& pin : pinOutputs)
			pin.setStreaming(power);
	}
};

namespace
{
auto r = Register<SamplePlayerSfzMulti>::withXml(
R"XML(
<?xml version="1.0" encoding="UTF-8"?>
<PluginList>
  <Plugin id="SE:SFZ Multi" name="Sample Player - SFZ Multi" category="Waveform" helpUrl="https://synthedit.com/help/sample-player-sfz">
    <Audio>
      <Pin name="Power" datatype="bool" default="1"/>
      <Pin name="MIDI In" datatype="midi"/>
      <Pin name="Filename" datatype="string" isFilename="true" metadata="sfz"/>
      <Pin name="Voice Count" datatype="int" default="64"/>
      <Pin name="Out 1 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 1 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 2 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 2 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 3 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 3 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 4 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 4 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 5 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 5 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 6 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 6 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 7 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 7 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 8 L" datatype="float" rate="audio" direction="out"/>
      <Pin name="Out 8 R" datatype="float" rate="audio" direction="out"/>
      <Pin name="Process Mode" datatype="int" private="true" hostConnect="Processor/OfflineRenderMode"/>
      <Pin name="Loaded" datatype="bool" direction="out"/>
    </Audio>
  </Plugin>
</PluginList>
)XML");
}
