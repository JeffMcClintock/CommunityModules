#include "../../se_sdk3/mp_sdk_audio.h"
#include "../../shared/xp_simd.h"
#include <math.h>

using namespace gmpi;

class Peak final : public MpBase2
{
	AudioInPin pinInputL;
	AudioInPin pinInputR;
	AudioOutPin pinOutput;

public:
	Peak()
	{
		initializePin(pinInputL);
		initializePin(pinInputR);
		initializePin(pinOutput);
	}

	void subProcess(int sampleFrames)
	{
		auto inputL = getBuffer(pinInputL);
		auto inputR = getBuffer(pinInputR);
		auto output = getBuffer(pinOutput);

		for (int s = sampleFrames; s > 0; --s)
		{
			*output = (std::max)(fabsf(*inputL), fabsf(*inputR));

			++inputL;
			++inputR;
			++output;
		}
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if (pinInputL.isStreaming())
		{
		}

		// Set state of output audio pins.
		pinOutput.setStreaming(pinInputL.isStreaming() || pinInputR.isStreaming());

		// Set processing method.
		setSubProcess(&Peak::subProcess);
	}
};

namespace
{
	auto r = Register<Peak>::withId(L"SE Peak");
}


class HardKnee final : public MpBase2
{
	FloatInPin pinThresh;
	AudioInPin pinInput;
	AudioOutPin pinOutput;

public:
	HardKnee()
	{
		initializePin(pinThresh);
		initializePin(pinInput);
		initializePin(pinOutput);
	}

	void subProcess(int sampleFrames)
	{
		auto input = getBuffer(pinInput);
		auto output = getBuffer(pinOutput);

		for (int s = sampleFrames; s > 0; --s)
		{
			if (*input > pinThresh)
				*output = pinThresh / *input;
			else
				*output = 1.0f;

			++input;
			++output;
		}
	}

	void onSetPins(void) override
	{
		// Set state of output audio pins.
		pinOutput.setStreaming(pinInput.isStreaming());

		// Set processing method.
		setSubProcess(&HardKnee::subProcess);
	}
};

namespace
{
	auto r1 = Register<HardKnee>::withId(L"SE Hard Knee");
}


class PeakHold final : public MpBase2
{
	FloatInPin pinTimeMs;
	AudioInPin pinInput;
	AudioOutPin pinOutput;

	std::vector<float> peaks;
	int delayIndex = 0;

public:
	PeakHold()
	{
		initializePin(pinTimeMs);
		initializePin(pinInput);
		initializePin(pinOutput);
	}

	void subProcess(int sampleFrames)
	{
		auto input = getBuffer(pinInput);
		auto output = getBuffer(pinOutput);

		for (int s = sampleFrames; s > 0; --s)
		{
			float peak = *input;
			auto i = delayIndex;
			for (int n = 1; n < peaks.size(); n <<= 1)
			{
				peaks[i] = peak;
				i -= n;
				if (i < 0) // wrap
				{
					i += static_cast<int>(peaks.size());
				}
				assert(i >= 0 && i < peaks.size());

				peak = (std::min)(peak, peaks[i]);
			}

			if (++delayIndex >= peaks.size())
			{
				delayIndex = 0;
			}

			*output = peak;

			++input;
			++output;
		}
	}

	void onSetPins(void) override
	{
		if (pinInput.isStreaming())
		{
		}
		if (pinTimeMs.isUpdated())
		{
			auto timeSamples = (std::max)(1, static_cast<int>(0.5f + 0.001f * pinTimeMs * getSampleRate()));
			peaks.assign(timeSamples, 1.0f);
		}

		// TODO sleep when delay line empty
		// Set state of output audio pins.
//		pinOutput.setStreaming(pinInput.isStreaming() || pinInputR.isStreaming());

		// Set processing method.
		setSubProcess(&PeakHold::subProcess);
	}
};

namespace
{
	auto r2 = Register<PeakHold>::withId(L"SE Peak Hold");
}

class SmootherGausian final : public MpBase2
{
	FloatInPin pinSmoothingTimeMs;
	AudioInPin pinInput;
	AudioOutPin pinOutput;

	struct tap
	{
		int time;
		float multiplier;
	};
	std::vector<int32_t> delay;
	int taps[3];
	int delayIndex = 0;
	int64_t z1 = 0; //?  0.5 sum of items already in delay?
	int64_t z2 = 0;
	float scaleDown;
	static const int fractionBits = 24;
	int indexWrapper;

public:
	SmootherGausian()
	{
		initializePin(pinSmoothingTimeMs);
		initializePin(pinInput);
		initializePin(pinOutput);
	}

	int32_t MP_STDCALL open(void) override
	{
		return MpBase2::open();
	}

	void subProcess(int sampleFrames)
	{
		auto input = getBuffer(pinInput);
		auto output = getBuffer(pinOutput);

		// averager used fixed-point math for stability ( 40.24 )
		constexpr float scaleUp = static_cast<float>(1 << fractionBits);

		for (int s = sampleFrames; s > 0; --s)
		{
			delay[delayIndex] = FastRealToIntFloor(0.5f + *input * scaleUp);

			int64_t sum =
				  delay[delayIndex]		// implicit first tap
				- delay[(delayIndex + taps[0]) & indexWrapper]
				- delay[(delayIndex + taps[1]) & indexWrapper]
				+ delay[(delayIndex + taps[2]) & indexWrapper];

			// Integrators
			sum -= z2;
			sum += z1 << 1;
			z2 = z1;
			z1 = sum;

			*output = scaleDown * static_cast<float>(sum);

			delayIndex = (delayIndex + 1) & indexWrapper;

			++input;
			++output;
		}
	}

	void onSetPins(void) override
	{
		if (pinSmoothingTimeMs.isUpdated())
		{
			const auto delayLength = pinSmoothingTimeMs * getSampleRate() * 0.001f;
			const int fir1 = (std::max)(1, static_cast<int>(0.5f + delayLength * 0.4));
			const int fir2 = (std::max)(1, static_cast<int>(0.5f + delayLength * 0.6));

			taps[0] = fir1;
			taps[1] = fir2;
			taps[2] = fir1 + fir2; // total smoothing

			constexpr float scaleUp = static_cast<float>(1 << fractionBits);
			scaleDown = 1.0f / (scaleUp * taps[0] * taps[1]);

			int niceDelaySize = 2;
			while (niceDelaySize < taps[2] + 1)
				niceDelaySize <<= 1;

			indexWrapper = niceDelaySize - 1;

			// make use of fact that in a circular buffer, subtracting is equivalent to adding
			taps[0] = niceDelaySize - taps[0];
			taps[1] = niceDelaySize - taps[1];
			taps[2] = niceDelaySize - taps[2];

			delay.assign(niceDelaySize, 0);

			z1 = z2 = delayIndex = 0;
		}

		// TODO sleep when delay line empty
		// Set state of output audio pins.
//		pinOutput.setStreaming(pinInput.isStreaming() || pinInputR.isStreaming());

		// Set processing method.
		setSubProcess(&SmootherGausian::subProcess);
	}
};

namespace
{
	auto r3 = Register<SmootherGausian>::withId(L"SE SmootherG");
}

class Integrator final : public MpBase2
{
	AudioInPin pinInput;
	AudioOutPin pinOutput;

	float sum = 0.0f;

public:
	Integrator()
	{
		initializePin(pinInput);
		initializePin(pinOutput);
	}

	void subProcess(int sampleFrames)
	{
		auto input = getBuffer(pinInput);
		auto output = getBuffer(pinOutput);

		for (int s = sampleFrames; s > 0; --s)
		{
			sum += *input;
			*output = sum;

			++input;
			++output;
		}
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if (pinInput.isStreaming())
		{
		}

		// Set state of output audio pins.
//		pinOutput.setStreaming(pinInputL.isStreaming() || pinInputR.isStreaming());

		// Set processing method.
		setSubProcess(&Integrator::subProcess);
	}
};

namespace
{
	auto r6 = Register<Integrator>::withId(L"SE Integrator");
}

class Ms2Factor final : public MpBase2
{
	AudioInPin pinInput;
	AudioOutPin pinOutput;

public:
	Ms2Factor()
	{
		initializePin(pinInput);
		initializePin(pinOutput);
	}

	void subProcess(int sampleFrames)
	{
		auto input = getBuffer(pinInput);
		auto output = getBuffer(pinOutput);

		constexpr float toVolts = 0.1;
		constexpr float fromVolts = 10.0;

		for (int s = sampleFrames; s > 0; --s)
		{
			*output = static_cast<float>(toVolts * exp(log(0.001)/( 0.001 * fromVolts * *input * getSampleRate())));

			++input;
			++output;
		}
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if (pinInput.isStreaming())
		{
		}

		// Set state of output audio pins.
		pinOutput.setStreaming(pinInput.isStreaming());

		// Set processing method.
		setSubProcess(&Ms2Factor::subProcess);
	}
};

namespace
{
	auto r7 = Register<Ms2Factor>::withId(L"SE ms2factor");
}

class Factor2Ms final : public MpBase2
{
	AudioInPin pinInput;
	AudioOutPin pinOutput;

public:
	Factor2Ms()
	{
		initializePin(pinInput);
		initializePin(pinOutput);
	}

	void subProcess(int sampleFrames)
	{
		auto input = getBuffer(pinInput);
		auto output = getBuffer(pinOutput);

		constexpr float toVolts = 0.1;
		constexpr float fromVolts = 10.0;
		for (int s = sampleFrames; s > 0; --s)
		{
			*output = static_cast<float>(toVolts * log(0.001) / (getSampleRate() * 0.001 * log(fromVolts * *input)));
			++input;
			++output;
		}
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if (pinInput.isStreaming())
		{
		}

		// Set state of output audio pins.
		pinOutput.setStreaming(pinInput.isStreaming());

		// Set processing method.
		setSubProcess(&Factor2Ms::subProcess);
	}
};

namespace
{
	auto r8 = Register<Factor2Ms>::withId(L"SE factor2ms");
}