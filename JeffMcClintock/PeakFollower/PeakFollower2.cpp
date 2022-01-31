
/* Copyright (c) 2007-2021 SynthEdit Ltd
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

using namespace gmpi;

class PeakFollower2 final : public MpBase2
{
	AudioInPin pinSignalin;
	AudioInPin pinAttack;
	AudioInPin pinRelease;
	AudioOutPin pinSignalOut;

	double current_level = 0;
	double ga;
	double gr;
	
	// the time constant represents the elapsed time required for the system response to decay to zero if the system had
	// continued to decay at the initial rate.  In an increasing system, the time constant is the time for the system's step response to
	// reach 1 − 1 / e ~= 63.2% of its final (asymptotic) value.
	// 1 : 63.2%
	// 2 : 86.5%
	// 3 : 95.0%
	// 4 : 98.2%

	// This converts ms in volts to 1 time constant. i.e. 0.1 = 1ms
	// for actual ms, n = -1000.0 / (getSampleRate() * volts)
	// reverse: ms = log(0.001) / (getSampleRate() * 0.0001f * log(coef))
	double VoltsToCoef(float volts) const
	{
		const auto n = -100.0 / (getSampleRate() * volts);

		if (!isfinite(n))
		{
			return 0.0f;
		}

		return exp(n);
	}

public:
	PeakFollower2()
	{
		initializePin(pinSignalin);
		initializePin(pinAttack);
		initializePin(pinRelease);
		initializePin(pinSignalOut);
	}

	void subProcess(int sampleFrames)
	{
		auto signalin = getBuffer(pinSignalin);
		auto attack = getBuffer(pinAttack);
		auto decay = getBuffer(pinRelease);
		auto signalOut = getBuffer(pinSignalOut);

		for (int s = sampleFrames; s > 0; --s)
		{
			auto i = *signalin;
			i = i < 0.f ? -i : i; // fabs()

			if(current_level < i)
				current_level = ga*(current_level-i) + i;
			else
				current_level = gr*(current_level-i) + i;

			*signalOut = (float) current_level;

			// Increment buffer pointers.
			++signalin;
			++attack;
			++decay;
			++signalOut;
		}
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if (pinSignalin.isStreaming())
		{
		}
		if (pinAttack.isUpdated())// && !pinAttack.isStreaming())
		{
			ga = VoltsToCoef(pinAttack);
		}
		if (pinRelease.isUpdated())// && !pinDecay.isStreaming())
		{
			gr = VoltsToCoef(pinRelease);
		}

		// Set state of output audio pins.
		pinSignalOut.setStreaming(true);

		// Set processing method.
		setSubProcess(&PeakFollower2::subProcess);
	}
};

namespace
{
	auto r = Register<PeakFollower2>::withId(L"SE Peak Follower2");
}
