/* Copyright (c) 2007-2022 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SEM, nor SynthEdit, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SynthEdit Ltd ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SynthEdit Ltd BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "mp_sdk_audio.h"
#include "../shared/xp_simd.h"
#include "../shared/interpolation.h"

#include "audio/choc_AudioFileFormat.h"
#include "audio/choc_AudioFileFormat_MP3.h"
#include "audio/choc_AudioFileFormat_WAV.h"
#include "audio/choc_AudioFileFormat_Ogg.h"
#include "audio/choc_AudioFileFormat_FLAC.h"

using namespace gmpi;

class AudioPlayer final : public MpBase2
{
	BoolInPin pinGate;
	AudioOutPin pinLeftOut;
	AudioOutPin pinRightOut;
	StringInPin pinFileName;

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
	AudioPlayer()
	{
		initializePin( pinFileName );
		initializePin( pinGate );
		initializePin( pinLeftOut );
		initializePin( pinRightOut );
	}

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

#if 0
			*leftOut = interpolate_linear(src_l, frac);
			*rightOut = interpolate_linear(src_r, frac);
#else
			*leftOut = interpolate_sinc(src_l, frac, coefs);
			*rightOut = interpolate_sinc(src_r, frac, coefs);
#endif
			// Increment buffer pointers.
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
		auto sampleFrames = sourceBuffers[0][0].size() - overlap * 2;
		auto lbufferview = choc::buffer::createChannelArrayView<float>(chans, 2, sampleFrames);
		audioFileReader->readFrames(frameIndex, lbufferview);

		frameIndex += sampleFrames;
		readPosition -= sampleFrames;
	}

	int32_t open() override
	{
		coefs = calcSincInterpolatorCoefs();

		return MpBase2::open();
	}

	void onSetPins() override
	{
		if (pinFileName.isUpdated())
		{
			const auto fullFilename = host.resolveFilename(pinFileName);

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

			audioFileReader = audioFile->createReader(fullFilename);

			if (!audioFileReader)
			{
				pinLeftOut.setStreaming(false);
				pinRightOut.setStreaming(false);

				// Set processing method.
				setSubProcess(&AudioPlayer::subProcessSilence);
				return;
			}

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
			readIncrement = (double)props.sampleRate / (double)getSampleRate();
			SwitchBuffers();
		}

		// Set state of output audio pins.
		pinLeftOut.setStreaming(true);
		pinRightOut.setStreaming(true);

		// Set processing method.
		setSubProcess(&AudioPlayer::subProcess);
	}

	void subProcessSilence(int sampleFrames)
	{
		auto leftOut = getBuffer(pinLeftOut);
		auto rightOut = getBuffer(pinRightOut);

		std::fill(leftOut, leftOut + sampleFrames, 0.0f);
		std::fill(rightOut, leftOut + sampleFrames, 0.0f);
	}
};

namespace
{
	auto r = Register<AudioPlayer>::withId(L"SE Audio Player");
}
