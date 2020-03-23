#include "../se_sdk3/mp_sdk_audio.h"

using namespace gmpi;

class Oscillator : public MpBase2
{
	AudioInPin pinPitch;
	IntInPin pinWaveform;
	AudioInPin pinPulseWidth;
	AudioInPin pinSync;
	AudioInPin pinPhaseMod;
	AudioInPin pinPMDepthdmy;
	AudioOutPin pinAudioOut;
	IntInPin pinResetMode;
	FloatInPin pinVoiceActive;

public:
	Oscillator()
	{
		initializePin( pinPitch );
		initializePin( pinWaveform );
		initializePin( pinPulseWidth );
		initializePin( pinPhaseMod );
		initializePin( pinSync );
		initializePin( pinPMDepthdmy );
		initializePin( pinResetMode );
		initializePin( pinAudioOut );
		initializePin( pinVoiceActive );
	}

	void subProcess( int sampleFrames )
	{
		// get pointers to in/output buffers.
		auto pitch = getBuffer(pinPitch);
		auto pulseWidth = getBuffer(pinPulseWidth);
		auto sync = getBuffer(pinSync);
		auto phaseMod = getBuffer(pinPhaseMod);
		auto audioOut = getBuffer(pinAudioOut);
		auto pMDepthdmy = getBuffer(pinPMDepthdmy);

		for( int s = sampleFrames; s > 0; --s )
		{
			// TODO: Signal processing goes here.

			// Increment buffer pointers.
			++pitch;
			++pulseWidth;
			++sync;
			++phaseMod;
			++audioOut;
			++pMDepthdmy;
		}
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if( pinPitch.isStreaming() )
		{
		}
		if( pinPulseWidth.isStreaming() )
		{
		}
		if( pinWaveform.isUpdated() )
		{
		}
		if( pinSync.isStreaming() )
		{
		}
		if( pinPhaseMod.isStreaming() )
		{
		}
		if( pinPMDepthdmy.isStreaming() )
		{
		}
		if( pinResetMode.isUpdated() )
		{
		}
		if( pinVoiceActive.isUpdated() )
		{
		}

		// Set state of output audio pins.
		pinAudioOut.setStreaming(true);

		// Set processing method.
		setSubProcess(&Oscillator::subProcess);

		// Set sleep mode (optional).
		// setSleep(false);
	}
};

namespace
{
	auto r = Register<Oscillator>::withId(L"SE Oscillator");
}
