#include "mp_sdk_audio.h"

SE_DECLARE_INIT_STATIC_FILE(PeakMeterHold)

using namespace gmpi;

class PeakMeterHold final : public MpBase2
{
	FloatInPin pinTimeS;
	AudioInPin pinInput;
	FloatOutPin pinOutput;

	float peak{};
	int sampleCount{};
	int chunkSize{};
	std::vector<float> peaks;
	int delayIndex = 0;
	int sleepCount = 0;

public:
	PeakMeterHold()
	{
		initializePin( pinTimeS );
		initializePin( pinInput );
		initializePin( pinOutput );
	}

	int32_t open() override
	{
		chunkSize = getSampleRate() / 100; // 10ms chunks.
		return MpBase2::open();
	}

	void subProcess( int sampleFrames )
	{
		auto input = getBuffer(pinInput);

		sampleCount -= sampleFrames;

		if(sampleCount < 0)
		{
			// cope nicely with very first sample.
			peak = (std::max)(*input, peak);

			peaks[delayIndex] = peak;
			peak = -10000.f;
			if(++delayIndex >= peaks.size())
				delayIndex = 0;

			sampleCount = (std::max)( 0, sampleCount + chunkSize);

			const float maxPeak = *std::max_element(peaks.begin(), peaks.end());
			pinOutput.setValue(10.0f * maxPeak, 0);

			if(sleepCount-- < 0 && !pinInput.isStreaming())
				setSleep(true); // sleep when no input, or constant input level.
		}

		for(int s = sampleFrames; s > 0; --s)
		{
			peak = (std::max)(*input, peak);
			++input;
		}
	}

	void onSetPins() override
	{
		if(pinTimeS.isUpdated())
			peaks.assign(static_cast<int64_t>(pinTimeS * 100.f), -10000.f);

		setSubProcess(&PeakMeterHold::subProcess);
		setSleep(false);
		sleepCount = peaks.size();
	}
};

namespace
{
	auto r = Register<PeakMeterHold>::withId(L"SE PeakMeterHold");
}
