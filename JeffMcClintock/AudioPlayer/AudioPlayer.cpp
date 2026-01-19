/* Copyright (c) 2007-2023 SynthEdit Ltd

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

#include <iostream>
#include "Core/Processor.h"
#include "../Extensions/EmbeddedFile.h"
#include "../Extensions/EmbeddedFileHelper.h"
#include "../shared/xp_simd.h"
#include "../shared/interpolation.h"

#include "audio/choc_AudioFileFormat.h"
#include "audio/choc_AudioFileFormat_MP3.h"
#include "audio/choc_AudioFileFormat_WAV.h"
#include "audio/choc_AudioFileFormat_Ogg.h"
#include "audio/choc_AudioFileFormat_FLAC.h"

using namespace gmpi;

class AudioPlayer final : public Processor
{
	StringInPin pinFileName;
	BoolInPin pinGate;
	AudioOutPin pinLeftOut;
	AudioOutPin pinRightOut;

	// chock playback
	std::unique_ptr<choc::audio::AudioFileFormat> audioFile;
	std::unique_ptr<choc::audio::AudioFileReader> audioFileReader;
	uint64_t frameIndex = 0;

	// buffered lookahead. Stereo. One reading, one writting
	std::vector<float> sourceBuffers[2][2];
	int currentPlayBuffer = 0;
	static constexpr int overlap = 6;

	double readPosition = 0.0;
	double readIncrement = 1.0;
	std::vector<float> coefs;

public:
	void subProcess( int sampleFrames )
	{
		auto leftOut = getBuffer(pinLeftOut);
		auto rightOut = getBuffer(pinRightOut);

		for( int s = sampleFrames; s > 0; --s )
		{
			int readIndex = FastRealToIntTruncateTowardZero(readPosition);
			const double frac = readPosition - readIndex;

			if (readPosition >= sourceBuffers[currentPlayBuffer][0].size() - overlap)
			{
				SwitchBuffers();
				readIndex -= sourceBuffers[currentPlayBuffer][0].size() - overlap * 2;
			}

			const float* src_l = sourceBuffers[currentPlayBuffer][0].data() + readIndex;
			const float* src_r = sourceBuffers[currentPlayBuffer][1].data() + readIndex;

			*leftOut = interpolate_sinc(src_l, frac, coefs);
			*rightOut = interpolate_sinc(src_r, frac, coefs);

			++leftOut;
			++rightOut;

			readPosition += readIncrement;
		}
	}

	void SwitchBuffers()
	{
		auto currentWriteBuffer = currentPlayBuffer;
		currentPlayBuffer = (currentPlayBuffer + 1) & 1; // alternates 0,1,0,1 ...

		// copy unused tail of previous buffer to start of next buffer
		for (int chan = 0; chan < 2; ++chan)
		{
			auto& src = sourceBuffers[currentPlayBuffer][chan];
			auto& dst = sourceBuffers[currentWriteBuffer][chan];
			std::copy(src.end() - overlap * 2, src.end(), dst.begin());
		}

		float* chans[2] =
		{
			sourceBuffers[currentWriteBuffer][0].data() + overlap * 2,
			sourceBuffers[currentWriteBuffer][1].data() + overlap * 2
		};

		// read fresh samples into buffers
		auto sampleFrames = static_cast<int>(sourceBuffers[0][0].size()) - overlap * 2;
		auto lbufferview = choc::buffer::createChannelArrayView<float>(chans, 2, sampleFrames);
		audioFileReader->readFrames(frameIndex, lbufferview);

		frameIndex += sampleFrames;
		readPosition -= sampleFrames;
	}

	ReturnCode open(IUnknown* phost) override
	{
		coefs = calcSincInterpolatorCoefs();
		return Processor::open(phost);
	}

	void onSetPins() override
	{
		if (pinFileName.isUpdated())
		{
			audioFileReader = {};
			audioFile = {};

			synthedit::EmbeddedFileHostWrapper fileHost;
			fileHost.Init(host.get());

			const auto fullFilename = fileHost.resolveFilename(pinFileName);

#ifdef _WIN32
			{
				auto msg = "AudioPlayer:" + fullFilename;
				std::cout << msg << std::endl;
			}
#endif

			// determine file type
			std::string fileextension("wav");
			if (auto dotPos = fullFilename.find_last_of('.'); dotPos != std::string::npos)
			{
				fileextension = fullFilename.substr(dotPos + 1);

				std::transform(fileextension.begin(), fileextension.end(), fileextension.begin(), ::tolower);
			}

			constexpr bool doesNotSupportWritting = false;
			if (fileextension == "wav")
			{
				audioFile = std::make_unique<choc::audio::WAVAudioFileFormat<doesNotSupportWritting>>();
			}
			else if (fileextension == "mp3")
			{
				audioFile = std::make_unique<choc::audio::MP3AudioFileFormat>();
			}
			else if (fileextension == "flac")
			{
				audioFile = std::make_unique<choc::audio::FLACAudioFileFormat<doesNotSupportWritting>>();
			}
			else if (fileextension == "ogg")
			{
				audioFile = std::make_unique<choc::audio::OggAudioFileFormat<doesNotSupportWritting>>();
			}

			if (audioFile)
			{
				audioFileReader = audioFile->createReader(fullFilename);

				if (audioFileReader)
				{
					frameIndex = 0;
					auto& props = audioFileReader->getProperties();

					const int lookaheadMs = 100;
					const int lookaheadSamples = lookaheadMs * props.sampleRate / 1000;

					for (auto& a : sourceBuffers)
					{
						for (auto& b : a)
						{
							b.assign(lookaheadSamples, 0.0f);
						}
					}

					readPosition = sourceBuffers[currentPlayBuffer][0].size(); // trigger a buffer refill.
					readIncrement = props.sampleRate / (double) host->getSampleRate();
					SwitchBuffers();
				}
				else
				{
#ifdef _WIN32
					_RPT1(0, "AudioPlayer: Failed to open file '%s'\n", fullFilename.c_str());
#endif
				}
			}
			else
			{
#ifdef _WIN32
				_RPT0(0, "AudioPlayer: Unsupported file type.\n");
#endif
			}
		}

		// Set state of output audio pins.
		const auto isPlaying = (bool) audioFileReader;
		pinLeftOut.setStreaming(isPlaying);
		pinRightOut.setStreaming(isPlaying);

		// Set processing method.
		setSubProcess(isPlaying ? &AudioPlayer::subProcess : &AudioPlayer::subProcessSilence);
	}

	void subProcessSilence(int sampleFrames)
	{
		auto leftOut = getBuffer(pinLeftOut);
		auto rightOut = getBuffer(pinRightOut);

		std::fill(leftOut, leftOut + sampleFrames, 0.0f);
		std::fill(rightOut, rightOut + sampleFrames, 0.0f);
	}
};

namespace
{
	auto r = Register<AudioPlayer>::withXml(
R"XML(
<?xml version="1.0" encoding="UTF-8"?>
<PluginList>
    <Plugin id="SE Audio Player" name="Audio Player" category="Input-Output">
        <Audio>
            <Pin name="File Name" datatype="string" isFilename="true" metadata="wav"/>
            <Pin name="Gate" datatype="bool" default="1"/>
            <Pin name="Left Out" datatype="float" rate="audio" direction="out"/>
            <Pin name="Right Out" datatype="float" rate="audio" direction="out"/>
        </Audio>
    </Plugin>
</PluginList>
)XML");
}
