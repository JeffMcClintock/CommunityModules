#include "./Oscillator.h"
#include "real_fft.h"
#include "OscMipmaps.h"

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


//#define PRINT_WAVETABLE_STATS 1
void Oscillator::CalcWave(float* spectrum, float* pdest, const WavetableMipmapPolicy& pMipMapPolicy, const char* debug_waveshape_name)
{
#ifdef PRINT_WAVETABLE_STATS
	_RPT1(_CRT_WARN, "\n\n%s\n", debug_waveshape_name);
	_RPT0(_CRT_WARN, "---------------------------\n");
#endif

	for (int mip = 0; mip < pMipMapPolicy.getMipCount(); ++mip)
	{
		int wavesize = pMipMapPolicy.GetWaveSize(mip);
		int activepartials = pMipMapPolicy.GetPartialCount(mip);

		float* dest = pdest + pMipMapPolicy.getSlotOffset(0, 0, mip) + extraInterpolationPreSamples;
		unsigned int fftSize = wavesize;
		bool applyGibbsFix = mip > 2 && mip == pMipMapPolicy.getMipCount() - 1; // lowest mip of sawtooth smooth to reduce overtone when stretched right down.

//#ifdef PRINT_WAVETABLE_STATS
//		_RPT0(_CRT_WARN, "Harm     lev          Gibbs\n");
//		_RPT0(_CRT_WARN, "---------------------------\n");
//#endif
		int componenets = (std::min)(activepartials * 2, wavesize - 2);

#ifdef PRINT_WAVETABLE_STATS
		_RPT3(_CRT_WARN, "MIP %3d partials %3d size %d\n", mip, (componenets / 2), wavesize);
#endif

		int i;
		for (i = 0; i < (componenets + 2); i = i + 2)
		{
			dest[i] = spectrum[i];
			dest[i + 1] = spectrum[i + 1];

			// Gibbs fix intended to make wave smoother for less interpolation noise.
			// Requires extra MIPs to work without attenuating HF.

			if (applyGibbsFix)
			{
				float damping = (float)(i - 2) / (componenets);
				float window = 0.5f + 0.5f * cosf(damping * (float)M_PI);
				dest[i + 1] *= window;
				dest[i] *= window;
			}
		}
		for (; i < (int)fftSize; ++i)
		{
			dest[i] = 0.0f;
		}

		realft(dest - 1, fftSize, -1);

		// Wrap samples off front and back to ease interpolator.
		for (i = -extraInterpolationPreSamples; i < 0; ++i)
		{
			dest[i] = dest[i + fftSize];
		}
		for (i = 0; i < extraInterpolationPostSamples; ++i)
		{
			dest[fftSize + i] = dest[i];
		}
	}
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
	int32_t needInitialize;
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

	int32_t needInit;
	// Init Sync cross-fade curve.
	int totalMemoryBytes = ( 2 + syncCrossFadeSamples ) * sizeof( float );
	int r = getHost()->allocateSharedMemory( L"JM:Oscillator:SyncCurve", (void**)&syncFadeCurve_, -1, totalMemoryBytes, needInit );
	if( needInit != 0 )
	{
		for( int i = 1; i <= syncCrossFadeSamples; ++i )
		{
			syncFadeCurve_[i - 1] = 0.5f + 0.5f * sinf( (float)M_PI * ((float)(i) / (float)(syncCrossFadeSamples + 1) - 0.5f));
		}
		syncFadeCurve_[syncCrossFadeSamples] = 100.0f; // testing not overstepping.
	}

	const int minWaveSz = 256; // little extra oversampling on top 2 octaves.
	const int maxWaveSz = 8192; // biggest wave FFT can handle.
	const int waveOversample = 32;
	const double spectrumFill = 0.5; // how much spectum (minimum) to produce at any MIP level.
	mipMapPolicy.initializeOsc(mipCount, waveOversample, extraInterpolationPreSamples + extraInterpolationPostSamples, minWaveSz, maxWaveSz, spectrumFill);
	mipMapPolicySine.initializeOsc(2, waveOversample, extraInterpolationPreSamples + extraInterpolationPostSamples, 512, maxWaveSz, spectrumFill);

	// Init wavetable memory.

	// Sawtooth
	{
		auto needAllocation = (MP_OK != getHost()->allocateSharedMemory(L"JM:HdOscillator:Saw", (void**)&waveSawtooth, -1, -1, needInit));
		if(needAllocation)
		{
			auto sawToothSpectrum = [](int partial) -> std::tuple<float, float> {
				constexpr float scale = -1.0f / M_PI;
				return { 0.0f, scale / partial };
			};

			const auto mips = MipMapCalculator::CalcMips(getSampleRate(), sawToothSpectrum);
			MipMapCalculator::PrintMips(getSampleRate(), mips);

			r = getHost()->allocateSharedMemory(
				L"JM:HdOscillator:Saw",
				(void**)&waveSawtooth,
				-1,
				static_cast<int32_t>(mips.GetTotalMemoryBytes()),
				needInit
			);

			assert(needInit);
			// TODO. if I put mip level info AND waveform in shared mem, don't need to recalc it for every osc.

			// mips.writeOut(&waveSawtooth);
		}
	}

	totalMemoryBytes = mipMapPolicy.GetTotalMipMapSize() * sizeof(float);
	r = getHost()->allocateSharedMemory(L"JM:Oscillator:Saw", (void**)&waveSawtooth, -1, totalMemoryBytes, needInit);
	if (needInit != 0)
	{
		const int maxSamples = 16384;
		float spectrum[maxSamples + 2];


		// Saw Wave.
		int totalHarmonics = maxSamples / 2;
		spectrum[0] = spectrum[1] = 0.0f; // DC and nyquist level.
		float scale = 2.0f / M_PI; // scale to 5V.
		for (int partial = 1; partial < totalHarmonics; ++partial)
		{
			spectrum[partial * 2] = 0.0f;
			float level = scale * -0.5f / partial;
			spectrum[partial * 2 + 1] = level;

			//const auto test = sawToothSpectrum(partial);
			//assert(std::get<0>(test) == spectrum[partial * 2]);
			//assert(std::get<1>(test) == spectrum[partial * 2 + 1]);
		}

		CalcWave(spectrum, waveSawtooth, mipMapPolicy, "Sawtooth");

		const auto s = mipMapPolicy.PrintMips("Sawtooth");
		_RPT1(_CRT_WARN, "%s\n", s.c_str() );
	}

	r = getHost()->allocateSharedMemory(L"JM:Oscillator:Tri", (void**)&waveTriangle, -1, totalMemoryBytes, needInit);
	if (needInit != 0)
	{
		const int maxSamples = 16384;
		float spectrum[maxSamples + 2];

		// Saw Wave.
		int totalHarmonics = maxSamples / 2;
		spectrum[0] = spectrum[1] = 0.0f; // DC and nyquist level.
		float scale = 4.0f / (float)(M_PI * M_PI); // scale to 5V.
		for (int partial = 1; partial < totalHarmonics; ++partial)
		{
			float level = scale / (partial * partial);
			if ((partial & 0x01) == 0)
			{
				level = 0.0f;
			}

			if ((partial >> 1) & 1) // every 2nd harmonic inverted
			{
				level = -level;
			}
			spectrum[partial * 2] = 0.0f;
			spectrum[partial * 2 + 1] = level;
		}

		CalcWave(spectrum, waveTriangle, mipMapPolicy, "Triangle");
	}

	// Sine
	totalMemoryBytes = mipMapPolicySine.GetTotalMipMapSize() * sizeof(float);
	r = getHost()->allocateSharedMemory(L"JM:Oscillator:Sin", (void**)&waveSine, -1, totalMemoryBytes, needInit);
	if (needInit != 0)
	{
		float spectrum[4];

		// Saw Wave.
		spectrum[0] = spectrum[1] = 0.0f; // DC and nyquist level.
		spectrum[2] = 0.0f;
		spectrum[3] = 0.5f;

		CalcWave(spectrum, waveSine, mipMapPolicySine, "Sine");
	}

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

	WavetableMipmapPolicy* mipPolicy;

	switch (pinWaveform)
	{
	case WS_SINE:
		grains[g].wave = waveSine;
		mipPolicy = &mipMapPolicySine;
		break;
	case WS_TRI:
		grains[g].wave = waveTriangle;
		mipPolicy = &mipMapPolicy;
		break;
	default:
		grains[g].wave = waveSawtooth;
		mipPolicy = &mipMapPolicy;
		break;
	}

	int mip = mipPolicy->CalcMipLevel(increment);
	grains[g].maxIncrement = mipPolicy->GetMaximumIncrement(mip);
	grains[g].minIncrement = mipPolicy->GetMinimumIncrement(mip);
	grains[g].waveSize = mipPolicy->GetWaveSize(mip);
	grains[g].wave += mipPolicy->getSlotOffset(0, 0, mip) + extraInterpolationPreSamples;
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
			++zeroSamplesCounter;
			*signalOut++ = 0.f;
		}
	}
}

namespace
{
	auto r = Register<Oscillator>::withId(L"SE Oscillator");
}