#include "../se_sdk3/mp_sdk_audio.h"

using namespace gmpi;

class RjLpFilter3 : public MpBase2
{
	AudioInPin pinSignal;
	AudioInPin pinPitch;
	AudioInPin pinResonance;
	AudioInPin pinZing;
	IntInPin pinType;
	IntInPin pinFilterTap;
	IntInPin pinQTap;
	IntInPin pinEmulate;
	IntInPin pinVersion;
	IntInPin pinBassBoost;
	AudioOutPin pinOutput;

public:
	RjLpFilter3()
	{
		initializePin( pinSignal );
		initializePin( pinPitch );
		initializePin( pinResonance );
		initializePin( pinZing );
		initializePin( pinType );
		initializePin( pinFilterTap );
		initializePin( pinQTap );
		initializePin( pinEmulate );
		initializePin( pinVersion );
		initializePin( pinBassBoost );
		initializePin( pinOutput );
	}

	void subProcess( int sampleFrames )
	{
		// get pointers to in/output buffers.
		auto signal = getBuffer(pinSignal);
		auto pitch = getBuffer(pinPitch);
		auto resonance = getBuffer(pinResonance);
		auto zing = getBuffer(pinZing);
		auto output = getBuffer(pinOutput);

		for( int s = sampleFrames; s > 0; --s )
		{
			// TODO: Signal processing goes here.

			// Increment buffer pointers.
			++signal;
			++pitch;
			++resonance;
			++zing;
			++output;
		}
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if( pinSignal.isStreaming() )
		{
		}
		if( pinPitch.isStreaming() )
		{
		}
		if( pinResonance.isStreaming() )
		{
		}
		if( pinZing.isStreaming() )
		{
		}
		if( pinType.isUpdated() )
		{
		}
		if( pinFilterTap.isUpdated() )
		{
		}
		if( pinQTap.isUpdated() )
		{
		}
		if( pinEmulate.isUpdated() )
		{
		}
		if( pinVersion.isUpdated() )
		{
		}
		if( pinBassBoost.isUpdated() )
		{
		}

		// Set state of output audio pins.
		pinOutput.setStreaming(true);

		// Set processing method.
		setSubProcess(&RjLpFilter3::subProcess);

		// Set sleep mode (optional).
		// setSleep(false);
	}
};

namespace
{
	auto r = Register<RjLpFilter3>::withId(L"SE RJ LP Filter 3");
}
