#include "../shared/FilterBase.h"
#include "../shared/xp_simd.h"

using namespace gmpi;

class Crusher final : public FilterBase
{
	AudioInPin pinSignalIn;
	AudioInPin pinDownsample;
	AudioOutPin pinSignalOut;
	float downsampleCount_ = {};
	float held_ = {};

public:
	Crusher()
	{
		initializePin( pinSignalIn );
		initializePin( pinDownsample );
		initializePin( pinSignalOut );
	}

	void subProcess( int sampleFrames )
	{
		// get pointers to in/output buffers.
		auto signalIn = getBuffer(pinSignalIn);
		auto downsample = getBuffer(pinDownsample);
		auto signalOut = getBuffer(pinSignalOut);

		constexpr auto c = 60.f / 200000.0f;
		const float downsampleRatio = (std::max)(1.0f, 1.0f + (*downsample * *downsample) * getSampleRate() * c);

		for( int s = sampleFrames; s > 0; --s )
		{
			downsampleCount_ -= 1.0f;
			if (downsampleCount_ <= 0.0f)
			{
				*signalOut = held_ = *signalIn;

				downsampleCount_ += downsampleRatio;
			}
			else
			{
				*signalOut = held_;
			}

			++signalIn;
			++signalOut;
		}
	}

	void onSetPins(void) override
	{
		// Set state of output audio pins.
		pinSignalOut.setStreaming(true);

		// Set processing method.
		setSubProcess(&Crusher::subProcess);

		initSettling(); // must be last.
	}

	bool isFilterSettling() override
	{
		return !pinSignalIn.isStreaming() && !pinDownsample.isStreaming();
	}
	AudioOutPin& getOutputPin() override
	{
		return pinSignalOut;
	}
};

namespace
{
	auto r = Register<Crusher>::withId(L"JM Crusher");
}


class Reducer final : public MpBase2
{
	AudioInPin pinSignalIn;
	AudioInPin pinBitsize;
	AudioOutPin pinSignalOut;

public:
	Reducer()
	{
		initializePin(pinSignalIn);
		initializePin(pinBitsize);
		initializePin(pinSignalOut);
	}

	void subProcess(int sampleFrames)
	{
		// get pointers to in/output buffers.
		auto signalIn = getBuffer(pinSignalIn);
		auto bitReduce = getBuffer(pinBitsize);
		auto signalOut = getBuffer(pinSignalOut);

		const float temp = 25.0f * ((1.0f - *bitReduce) * (1.0f - *bitReduce));
		const int bitSize = 1 + FastRealToIntTruncateTowardZero(temp);
		const int steps = 1 << bitSize;
		const float inverseSteps = 1.0f / (float)steps;

		for (int s = sampleFrames; s > 0; --s)
		{
			if (bitSize < 24)
			{
				auto t = *signalIn * (float)steps;
				if (t >= 0.0f)
				{
					t += 0.5f;
				}
				else
				{
					t -= 0.5f;
				}
				const int quantized = FastRealToIntTruncateTowardZero(t);
				*signalOut = (float)quantized * inverseSteps;
			}
			else
			{
				*signalOut = *signalIn;
			}

			++signalIn;
			++signalOut;
		}
	}

	void onSetPins(void) override
	{
		// Set state of output audio pins.
		pinSignalOut.setStreaming(pinSignalIn.isStreaming() || pinBitsize.isStreaming());

		// Set processing method.
		setSubProcess(&Reducer::subProcess);
	}
};

namespace
{
	auto r2 = Register<Reducer>::withId(L"JM Reducer");
}