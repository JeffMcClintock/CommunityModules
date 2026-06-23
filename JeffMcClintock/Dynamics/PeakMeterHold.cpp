#include "mp_sdk_audio.h"

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

public:
	PeakMeterHold()
	{
		initializePin( pinTimeS );
		initializePin( pinInput );
		initializePin( pinOutput );
	}

	int32_t open() override
	{
		sampleCount = chunkSize = getSampleRate() / 100; // 10ms chunks.
		return MpBase2::open();
	}

	void subProcess( int sampleFrames )
	{
		sampleCount -= sampleFrames;

		if(sampleCount < 0)
		{
			peaks[delayIndex] = peak;
			peak = -10000.f;
			if(++delayIndex >= peaks.size())
				delayIndex = 0;

			sampleCount = (std::max)( 0, sampleCount + chunkSize);

			const float maxPeak = *std::max_element(peaks.begin(), peaks.end());
			pinOutput.setValue(maxPeak, 0);
		}

		auto input = getBuffer(pinInput);

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

		// TODO sleep when delay line empty

		setSubProcess(&PeakMeterHold::subProcess);
	}
};

namespace
{
	auto r = Register<PeakMeterHold>::withId(L"SE PeakMeterHold");
}
