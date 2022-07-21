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
#include <vector>
#include <memory>
#include "mp_sdk_audio.h"

using namespace gmpi;

class WaveformScroller final : public MpBase2
{
//	BlobOutPin pinBlobOut;
	BlobOutPin pinPMValueOut;
	AudioInPin pinSignal;

//	std::vector< std::unique_ptr<AudioInPin> > pinSignal;
	//std::vector<float*> ins;
	std::vector<float> signalBuffer;
	int blockIndex = 0;

public:
	int recordingBufferSize_ = {};
	static const int recordingBufferHeaderFloats_ = 2;

	WaveformScroller()
	{
		initializePin(pinPMValueOut);
		initializePin(pinSignal);
	}

	int32_t open() override
	{
		const int assumedGuiUpdateRateHz = 20;
		recordingBufferSize_ = host.getSampleRate() / assumedGuiUpdateRateHz;

		initSignalBuffer();

		return MpBase2::open();
	}

	void subProcess(int sampleFrames)
	{
		auto in = getBuffer(pinSignal);

		for (int s = 0; s < sampleFrames; ++s)
		{
			signalBuffer.push_back(*in++);

			if (signalBuffer.size() == recordingBufferSize_ + recordingBufferHeaderFloats_)
			{
				// send to GUI.
				pinPMValueOut.setValueRaw(signalBuffer.size() * sizeof(signalBuffer[0]), signalBuffer.data());
				pinPMValueOut.sendPinUpdate(getBlockPosition() + s);

				initSignalBuffer();
			}
		}
	}

	void initSignalBuffer()
	{
		signalBuffer.clear();
		signalBuffer.push_back(static_cast<float>(blockIndex++));		// samples per channel
		signalBuffer.push_back(1);										// number of channels
	}

	void onSetPins(void) override
	{
		setSubProcess(&WaveformScroller::subProcess);
		setSleep(false);
	}
};

namespace
{
	auto r = Register<WaveformScroller>::withId(L"SE Waveform Scroller");
}
