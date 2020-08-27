#include "./Oscillator.h"
#include "real_fft.h"
#include "SharedObject.h"

using namespace gmpi;

Oscillator::Oscillator() :
 pitchTable(0)
, waveData_(0)
, syncState(false)
, prevSync(0.0f)
, firstTime(true)
, random(rand())
, prevPhase(0.0f) // randomish but reproducible start.
, buf0( 0 )
, buf1( 0 )
, buf2( 0 )
, buf3( 0 )
, buf4( 0 )
, buf5( 0 )
, increment( 0 )
{
	// Register pins.
	initializePin( pinPitch );
	initializePin( pinPulseWidth );
	initializePin( pinWaveform );
	initializePin( pinSync );
	initializePin( pinPhaseMod );
	// dummy pin 5.
	initializePin(6, pinBypass);
	initializePin(pinDcoMode);
	initializePin(pinSignalOut);
	initializePin(pinVoiceActive);
}

int32_t Oscillator::open()
{
	// 20kHz is about 10.5 Volts. 1Hz is about -3.7 volts. 0.01Hz = -10V
	// -4 -> 11 Volts should cover most posibilities. 15V Range. 12 entries per volt = 180 entries.
	const int extraEntriesAtStart = 1; // for interpolator.
	const int extraEntriesAtEnd = 3; // for interpolator.
    const int pitchTableSize = extraEntriesAtStart + extraEntriesAtEnd + (OscPitchChanging::pitchTableHiVolts - OscPitchChanging::pitchTableLowVolts) * 12;
	const float oneSemitone = 1.0f / 12.0f;
	int size = (pitchTableSize)* sizeof( float );
	int32_t needInitialize = 0;
	getHost()->allocateSharedMemory( L"JM:Oscillator:Pitch", (void**)&pitchTable, getSampleRate(), size, needInitialize );

	if( needInitialize )
	{
        float overSampleRate = 1.0f / getSampleRate();
		for( int i = 0; i < pitchTableSize; ++i )
		{
            float pitch = (OscPitchChanging::pitchTableLowVolts + (i - extraEntriesAtStart) * oneSemitone) * 0.1f;
			float hz = SampleToFrequency( pitch );

			pitchTable[i] = hz * overSampleRate;
		}
	}

	pitchTable += extraEntriesAtStart; // Shift apparent start of table to entry #1, so we can access table[-1] without hassle.

	// TODO release mem after period of inactivity.
	waveTriangle = SharedObjectManager< std::vector<MipMapCalculator::WavetableMip> >::getOrCreateSharedMemory(
		getSampleRate(),
		WS_TRI,
		[](float sampleRate) -> std::shared_ptr<std::vector<MipMapCalculator::WavetableMip>>
		{
			auto TriangleSpectrum = [](int partial) -> std::tuple<float, float>
			{
				constexpr float scale = 4.0f / (float)(M_PI * M_PI); // scale to 5V.

				if(partial == 0)
				{
					return { 0.0f, 0.0f }; // DC and nyquist
				}
				else
				{
					if((partial & 0x01) == 0)
					{
						return { 0.0f, 0.0f };
					}

					float level = scale / (partial * partial);
					if((partial >> 1) & 1) // every 2nd harmonic inverted
					{
						level = -level;
					}
					return { 0.0f, level };
				}
			};

			const auto mips = MipMapCalculator::CalcMips(sampleRate, TriangleSpectrum);
			MipMapCalculator::PrintMips(sampleRate, mips, "Triangle");
			return MipMapCalculator::generateWavetable(mips, TriangleSpectrum);;
		}
	);

	waveSawtooth = SharedObjectManager< std::vector<MipMapCalculator::WavetableMip> >::getOrCreateSharedMemory(
		getSampleRate(),
		WS_SAW,
		[](float sampleRate) -> std::shared_ptr<std::vector<MipMapCalculator::WavetableMip>>
		{
			auto sawToothSpectrum = [](int partial) -> std::tuple<float, float>
			{
				constexpr float scale = -1.0f / M_PI;

				if(partial == 0)
				{
					return { 0.0f, 0.0f }; // DC and nyquist
				}
				else
				{
					return { 0.0f, scale / partial };
				}
			};

			const auto mips = MipMapCalculator::CalcMips(sampleRate, sawToothSpectrum);
			MipMapCalculator::PrintMips(sampleRate, mips, "SawTooth");
			return MipMapCalculator::generateWavetable(mips, sawToothSpectrum);
		}
		);

	waveSine = SharedObjectManager< std::vector<MipMapCalculator::WavetableMip> >::getOrCreateSharedMemory(
		getSampleRate(),
		WS_SINE,
		[](float sampleRate) -> std::shared_ptr<std::vector<MipMapCalculator::WavetableMip>>
		{
			auto sineSpectrum = [](int partial) -> std::tuple<float, float>
			{
				if(partial == 1)
				{
					return { 0.0f, 0.5f };
				}
				else
				{
					return { 0.0f, 0.0f }; // DC and nyquist and all other harmonics.
				}
			};

			const auto mips = MipMapCalculator::CalcMips(sampleRate, sineSpectrum);
			MipMapCalculator::PrintMips(sampleRate, mips, "Sine");
			return MipMapCalculator::generateWavetable(mips, sineSpectrum);
		}
	);


	SET_PROCESS2(&Oscillator::sub_process_silence);

/*
	{
		std::array<float, 200> testSample;
		for (int i = 0; i < testSample.size(); ++i)
		{
			testSample[i] = (float)i;
		}

		// test interpolation code.
		double phasor = 0.0;
		double phasorInc = 0.1;
		static float total = 0.0f;
		for (int i = 10; i < 120; ++i)
		{
			total += CubicInterpolator::Interpolate(phasor, 128, testSample.data());
			phasor += phasorInc;
		}
	}
*/

	return MpBase2::open();
}

typedef void (Oscillator::* OscProcess_ptr)(int sampleFrames);

#define TPB( interpolation, wave, pitch, phase, sync ) (&Oscillator::sub_process_template2< wave, pitch, phase, sync, interpolation > )

const OscProcess_ptr ProcessSelection2[2][3][2][2][2] =
{
	// LOW QUALITY
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationFixed, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationChanging, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationFixed, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationChanging, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationFixed, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationChanging, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(LinearInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	// HIGH QUALITY INTERPOLATION
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationFixed, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationChanging, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationFixed, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationChanging, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationFixed, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationChanging, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationFixed, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationChanging, NotModulatedPinPolicy),
	TPB(CubicInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),
};

#define TPC( interpolation, wave, pitch, phase, unused2 ) (&Oscillator::sub_process_template2fast< wave, pitch, phase, interpolation > )

const OscProcess_ptr ProcessSelection2fast[2][3][2][2] =
{
	// LOW QUALITY
	TPC(LinearInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationFixed, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationChanging, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPC(LinearInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPC(LinearInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPC(LinearInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	// HIGH QUALITY INTERPOLATION
	TPC(CubicInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationFixed, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutNormal, OscPitchFixed, phaseModulationChanging, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutNormal, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPC(CubicInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutInverted, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutInverted, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),

	TPC(CubicInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationFixed, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutPulse, OscPitchFixed   , phaseModulationChanging, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationFixed, ModulatedPinPolicy),
	TPC(CubicInterpolator, WaveReadoutPulse, OscPitchChanging, phaseModulationChanging, ModulatedPinPolicy),
};

void Oscillator::recalculatePitch() 
{
	float pitch = pinPitch;
	OscPitchChanging::Calculate(pitchTable, &pitch, increment);
}

void Oscillator::resetOscillator()
{
	float increment;
	float pitch = pinPitch;
	OscPitchChanging::Calculate(pitchTable, &pitch, increment);

	for (int g = 0; g < MaxGrains; ++g)
	{
		grains[g].waveSize = 0;
	}

	startGrain(0.0, increment);
}

void Oscillator::doSync(bool newSyncState)
{
	syncState = newSyncState;

	if (syncState)
	{
		prevSync = -1; // prevent numeric junk.
		DoSync2(pinPhaseMod, pinSync, -1.0f);
		ChooseProcessMethod();
	}
}

void Oscillator::onSetPins(void)
{
	if (pinWaveform.isUpdated())
		resetOscillator();

	auto pitchUpdated = pinPitch.isUpdated() && !pinPitch.isStreaming();

	if (pitchUpdated || pinBypass.isUpdated())
	{
		if (!pinBypass)
			recalculatePitch();
	}

	// Any update on pinTrigger is a trigger. Actual pin value is garbage.
	if (pinVoiceActive.isUpdated())
	{
		bool newActiveState = pinVoiceActive > 0.0f;

		if (newActiveState && !previousActiveState && pinDcoMode != 0)
		{
			resetOscillator();
			ChooseProcessMethod();
		}

		previousActiveState = newActiveState;
	}

	if (pinSync.isUpdated() && !pinSync.isStreaming())
	{
		bool newSyncState = pinSync > 0.0f;

		if (newSyncState != syncState)
		{
			doSync(newSyncState);
		}
	}

	ChooseProcessMethod();

	pinSignalOut.setStreaming(!pinBypass);

	if(pinBypass.isUpdated() && !pinBypass)
		zeroSamplesCounter = 0;
}

void Oscillator::ChooseProcessMethod()
{
	if (pinBypass)
	{
		SET_PROCESS2(&Oscillator::sub_process_silence);
	}
	else
	{
		int wavetype = -1;

		switch (pinWaveform)
		{
		case WS_WHITE_NOISE:
			setSubProcess(&Oscillator::sub_process_white_noise);
			break;

		case WS_PINK_NOISE:
			setSubProcess(&Oscillator::sub_process_pink_noise);
			break;

		case WS_SINE:
		case WS_SAW:
		case WS_TRI:
			wavetype = 0;	// normal
			break;

		case WS_RAMP:
			wavetype = 1;	// inverted
			break;

		case WS_PULSE:
			wavetype = 2;	// Pulse
			break;

		default:
			break;
		}

		if (wavetype > -1)
		{
			// Fast mode can be used if only one grain active at a steady level.
			 bool fastMode = grains[0].waveSize != 0 && grains[0].fadeIncrement == 0 && !pinSync.isStreaming();
			for (int g = 1; g < MaxGrains; ++g)
			{
				fastMode &= grains[g].waveSize == 0;
			}

			const int interpolation = 1;
			if (fastMode)
			{
				setSubProcess(static_cast <SubProcess_ptr2> (ProcessSelection2fast[interpolation][wavetype][pinPitch.isStreaming()][pinPhaseMod.isStreaming()]));
			}
			else
			{
				setSubProcess(static_cast <SubProcess_ptr2> (ProcessSelection2[interpolation][wavetype][pinPitch.isStreaming()][pinPhaseMod.isStreaming()][pinSync.isStreaming()]));
			}
		}
	}
}

void Oscillator::startGrain( phasor_t initCount, float increment, int fadeIncrement)
{
	assert(initCount >= 0.0 && initCount < 200.0); // negative phases seem OK EDIT: Nope. results in negative table index on lookup sample.

	// Only matters for one-shot, wrap count to range 0 - 1
	auto count_floor = FastRealToIntTruncateTowardZero(initCount);
	initCount -= (phasor_t)count_floor;

	// start a fresh grain.
	for (int g = MaxGrains - 1; g > 0; --g)
	{
		grains[g] = grains[g-1];
	}

	const int g = 0;
	grains[g].count = initCount; // Make grain phase fractionally correct.
	grains[g].fadeIncrement = fadeIncrement;
	if( fadeIncrement == 1 )
	{
		grains[g].fadeIndex = -1;
	}

	grains[g].fadeIndex += grains[g].fadeIncrement;

	std::vector<MipMapCalculator::WavetableMip>* mipMap;

	switch (pinWaveform)
	{
	case WS_SINE:
		mipMap = waveSine.get();
		break;
	case WS_TRI:
		mipMap = waveTriangle.get();
		break;
	default:
		mipMap = waveSawtooth.get();
		break;
	}

	const auto mip = CalcMipLevel(*mipMap, increment);

	grains[g].wave = mip->GetWave(); // TODO arrrange layout of mip same as grain.
	grains[g].maxIncrement = mip->maximumIncrement;
	grains[g].minIncrement = mip->minimumIncrement;
	grains[g].waveSize = mip->GetWaveSize();

	grains[g].PrintState();
}

void Oscillator::sub_process_white_noise( int sampleFrames )
{
	float* signalOut = getBuffer( pinSignalOut );

	unsigned int itemp;
	unsigned int idum = random;
	const unsigned int jflone = 0x3f800000; // see 'numerical recipies in c' pg 285
	const unsigned int jflmsk = 0x007fffff;

	for( int s = sampleFrames; s > 0; --s )
	{
		idum = idum * 1664525L + 1013904223L;// use mask to quickly convert integer to float between -0.5 and 0.5
		itemp = jflone | (jflmsk & idum);
		*signalOut++ = (*(float*)&itemp) - 1.5f;
	}

	random = idum; // store new random number
}

void Oscillator::sub_process_pink_noise( int sampleFrames )
{
	float* signalOut = getBuffer( pinSignalOut );

	unsigned int itemp;
	unsigned int idum = random;
	const unsigned int jflone = 0x3f800000; // see 'numerical recipies in c' pg 285
	const unsigned int jflmsk = 0x007fffff;

	for( int s = sampleFrames; s > 0; --s )
	{
		idum = idum * 1664525L + 1013904223L;
		// use mask to quickly convert integer to float between -0.5 and 0.5
		itemp = jflone | (jflmsk & idum);
		float white = (*(float*)&itemp) - 1.5f;

		// filtering white noise.
		buf0 = 0.997f * buf0 + 0.029591f * white;
		buf1 = 0.985f * buf1 + 0.032534f * white;
		buf2 = 0.950f * buf2 + 0.048056f * white;
		buf3 = 0.850f * buf3 + 0.090579f * white;
		buf4 = 0.620f * buf4 + 0.108990f * white;
		buf5 = 0.250f * buf5 + 0.255784f * white;
		*signalOut++ = buf0 + buf1 + buf2 + buf3 + buf4 + buf5;
	}

	random = idum; // store new random number
}

void Oscillator::sub_process_silence(int sampleFrames)
{
	if (zeroSamplesCounter > getBlockSize())
	{
		SET_PROCESS2(&Oscillator::subProcessNothing);
	}
	else
	{
		auto signalOut = getBuffer(pinSignalOut);

		for (int s = sampleFrames; s > 0; --s)
		{
			*signalOut++ = 0.f;
		}

		zeroSamplesCounter += sampleFrames;
	}
}

namespace
{
	auto r = Register<Oscillator>::withId(L"SE Oscillator");
}