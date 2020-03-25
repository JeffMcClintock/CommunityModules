#pragma once

#include "xp_simd.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "mp_sdk_audio.h"
#include "OscMipmapPolicy.h"

typedef double phasor_t;

class ModulatedPinPolicy
{
public:
	inline static void IncrementPointer(const float*& samplePointer)
	{
		++samplePointer;
	}
	inline static float* getBuffer(const MpBase2* module, const AudioInPin& pin)
	{
		return module->getBuffer(pin);
	}
	enum { Active = true };
};

class NotModulatedPinPolicy
{
public:
	inline static void IncrementPointer(const float*& samplePointer)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
	inline static float* getBuffer(const MpBase2* module, const AudioInPin& pin)
	{
		return nullptr;
	}
	enum { Active = false };
};

class OscPitchFixed : public NotModulatedPinPolicy
{
public:
	inline static void Calculate(const float* pitchTable, const float* pitch, float& returnIncrement)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
};

class OscPitchChanging : public ModulatedPinPolicy
{
public:
    const static int pitchTableLowVolts = -10; // ~ 1/60 Hz
    const static int pitchTableHiVolts = 11;  // ~ 20kHz.

    inline static float ComputeIncrement2(const float* pitchTable, float pitch)
    {
    //	float index = 12.0f * (pitch * 10.0f - (float)pitchTableLowVolts);
		constexpr float a = 12.0f * 10.0f; // 12 tone, 10V / Oct
		constexpr float b = 12.0f * static_cast<float>(pitchTableLowVolts);
		float index = pitch * a - b;
	    int table_floor = FastRealToIntTruncateTowardZero(index);

	    /*
	    std:min slow compared to direct branching.
	    // not as exact as limiting pitch ( fractional part will be wrong (but harmless)
	    table_floor = std::min( table_floor, (pitchTableHiVolts - pitchTableLowVolts) * 12 );
	    table_floor = std::max( table_floor, 0 );
	    */
	    if (table_floor <= 0) // indicated index *might* be less than zero. e.g. Could be 0.1 which is valid, or -0.1 which is not.
	    {
		    if (!(index >= 0.0f)) // reverse logic to catch Nans.
		    {
			    return pitchTable[0];
		    }
	    }
	    else
	    {
		    constexpr int maxTableIndex = (pitchTableHiVolts - pitchTableLowVolts) * 12;
		    if (table_floor >= maxTableIndex)
		    {
			    return pitchTable[maxTableIndex];
		    }
	    }

	    const float fraction = index - (float)table_floor;

	    // Cubic interpolator.
	    assert(table_floor >= 0 && table_floor <= (pitchTableHiVolts - pitchTableLowVolts) * 12);

		const float y0 = pitchTable[table_floor - 1];
		const float y1 = pitchTable[table_floor + 0];
		const float y2 = pitchTable[table_floor + 1];
		const float y3 = pitchTable[table_floor + 2];

	    return y1 + 0.5f * fraction*(y2 - y0 + fraction*(2.0f*y0 - 5.0f*y1 + 4.0f*y2 - y3 + fraction*(3.0f*(y1 - y2) + y3 - y0)));
    }
    inline static void Calculate(const float* pitchTable, const float* pitch, float& returnIncrement)
	{
		returnIncrement = ComputeIncrement2(pitchTable, *pitch);
	}
};

class phaseModulationFixed : public NotModulatedPinPolicy
{
};

class phaseModulationChanging : public ModulatedPinPolicy
{
};

class WaveReadoutNormal : public NotModulatedPinPolicy
{
public:
	inline static void CalcPW(const float* pulseWidth, float returnPW)
	{
	}
	inline static phasor_t CalcPhaseB(phasor_t count, float pw)
	{
		return 0;
	}
	inline static float ModifySign(float sample)
	{
		return sample;
	}
};

class WaveReadoutInverted : public NotModulatedPinPolicy // For Ramp.
{
public:
	inline static void CalcPW(const float* pulseWidth, float returnPW)
	{
	}
	inline static phasor_t CalcPhaseB(phasor_t count, float pw)
	{
		return 0;
	}
	inline static float ModifySign(float sample)
	{
		return -sample;
	}
};

class WaveReadoutPulse : public ModulatedPinPolicy
{
public:
	inline static void CalcPW(const float* pulseWidth, float& returnPW)
	{
		returnPW = 0.25f - 0.25f * *pulseWidth;

		const float pwlimit = 0.0025f;
		if (returnPW < pwlimit)
			returnPW = pwlimit;
		if (returnPW > 0.5f - pwlimit)
			returnPW = 0.5f - pwlimit;
	}
	inline static phasor_t CalcPhaseB(phasor_t count, float pw)
	{
		return count - static_cast<phasor_t>(pw * 2.0f);
	}
	inline static float ModifySign(float sample)
	{
		return sample;
	}
};

struct Grain
{
	int waveSize;
	phasor_t count;
	float* wave;
	int fadeIncrement;
	int fadeIndex;
	phasor_t minIncrement;
	phasor_t maxIncrement;

	Grain() : count( 0 ), waveSize( 0 ), fadeIndex( 0 ), fadeIncrement(0){};
	inline void stop()
	{
		waveSize = 0;
	}
	inline bool isActive()
	{
		return waveSize != 0;
	}
};

// 2-point.
// Low-quality interpolation
struct LinearInterpolator
{
	inline static float Interpolate(phasor_t count, Grain& grain)
	{
		// Tested float accumulator with double index (no good). Both accumulator and index must be doubles.
		phasor_t index = count * (phasor_t)grain.waveSize;
		int table_floor = FastRealToIntTruncateTowardZero(index);

		float fraction = (float)(index - (phasor_t)table_floor);

		table_floor &= grain.waveSize - 1;

		assert(table_floor >= 0);

		float y1 = grain.wave[table_floor + 0];
		float y2 = grain.wave[table_floor + 1];

		return y1 + fraction * (y2 - y1);
	}
};

// 4-point
struct CubicInterpolator_1
{
	inline static float Interpolate(phasor_t count, Grain& grain)
	{
		// Tested float accumulator with double index (no good). Both accumulator and index must be doubles.
		phasor_t index = count * (phasor_t)grain.waveSize;
		int table_floor = FastRealToIntTruncateTowardZero(index);

		float fraction = (float)(index - (phasor_t)table_floor);

		table_floor &= grain.waveSize - 1;

		assert(table_floor >= 0);

		float y0 = grain.wave[table_floor - 1];
		float y1 = grain.wave[table_floor + 0];
		float y2 = grain.wave[table_floor + 1];
		float y3 = grain.wave[table_floor + 2];

		return y1 + 0.5f * fraction*(y2 - y0 + fraction * (2.0f*y0 - 5.0f*y1 + 4.0f*y2 - y3 + fraction * (3.0f * (y1 - y2) + y3 - y0)));
	}

	inline static float Interpolate(phasor_t count, int waveSize, float* wave)
	{
		// Tested float accumulator with double index (no good). Both accumulator and index must be doubles.
		phasor_t index = count * (phasor_t)waveSize;
		int table_floor = FastRealToIntTruncateTowardZero(index);

		float fraction = static_cast<float>(index - static_cast<phasor_t>(table_floor));

		table_floor &= waveSize - 1;

		assert(table_floor >= 0);

		float y0 = wave[table_floor - 1];
		float y1 = wave[table_floor + 0];
		float y2 = wave[table_floor + 1];
		float y3 = wave[table_floor + 2];

		return y1 + 0.5f * fraction*(y2 - y0 + fraction * (2.0f*y0 - 5.0f*y1 + 4.0f*y2 - y3 + fraction * (3.0f * (y1 - y2) + y3 - y0)));
	}
};

// 4-point
struct CubicInterpolator
{
	inline static float Interpolate(phasor_t count, Grain& grain)
	{
		// Tested float accumulator with double index (no good). Both accumulator and index must be doubles.
		phasor_t index = count * (phasor_t)grain.waveSize;
		int table_floor = FastRealToIntTruncateTowardZero(index);

		float fraction = (float)(index - (phasor_t)table_floor);

		table_floor &= grain.waveSize - 1;

		assert(table_floor >= 0);

		__m128 vFraction2 = _mm_set_ps1(fraction);
		vFraction2 = _mm_mul_ps(vFraction2, vFraction2); // squared.
		__m128 vFraction1 = _mm_set_ps1(fraction);
		__m128 vFraction3 = _mm_mul_ps(vFraction1, vFraction2); // cubed.

		const __m128 v3 = _mm_set_ps(0.5, -1.5, 1.5, -0.5);
		const __m128 v2 = _mm_set_ps(-0.5, 2.0, -2.5, 1.0);
		const __m128 v1 = _mm_set_ps(0.0, 0.5, 0.0, -0.5);

		__m128 waveSamples = _mm_loadu_ps(grain.wave + table_floor - 1);
		__m128 s1 = _mm_mul_ps(waveSamples, _mm_mul_ps(v1, vFraction1));
		__m128 s2 = _mm_mul_ps(waveSamples, _mm_mul_ps(v2, vFraction2));
		__m128 s3 = _mm_mul_ps(waveSamples, _mm_mul_ps(v3, vFraction3));
		__m128 st1 = _mm_add_ps(s1, s2);
		__m128 sum = _mm_add_ps(st1, s3);

		// Horizontal add sum3xFrac.
		const __m128 t = _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
		const __m128 sum2 = _mm_add_ss(t, _mm_shuffle_ps(t, t, 1)); // sum in pos 0.

		const __m128 sum3 = _mm_add_ps(sum2, _mm_shuffle_ps(waveSamples, waveSamples, _MM_SHUFFLE(1, 1, 1, 1))); // + y1
		float returnValue;
		_mm_store_ss(&returnValue, sum3);
		return returnValue;
	}
};

class Oscillator : public MpBase2
{
	const static int MaxGrains = 4;
	const static int extraInterpolationPreSamples = 1;
	const static int extraInterpolationPostSamples = 3;
	const static int syncCrossFadeSamples = 8;

	static const int mipCount = 14;
	static unsigned int randomSeedValue;

	Grain grains[MaxGrains];
	float increment;
	float *pitchTable;
	float* waveData_;
	float* syncFadeCurve_;
	bool syncState;
	float prevSync;
	bool firstTime;
	unsigned int random;
	phasor_t prevPhase;

	// pink noise stuff
	float buf0;
	float buf1;
	float buf2;
	float buf3;
	float buf4;
	float buf5;

	WavetableMipmapPolicy mipMapPolicySine;
	WavetableMipmapPolicy mipMapPolicy; // others
	float* waveSawtooth;
	float* waveTriangle;
	float* waveSine;

	int zeroSamplesCounter = 0;

	AudioInPin pinPitch;
	AudioInPin pinPulseWidth;
	IntInPin pinWaveform;
	AudioOutPin pinSignalOut;
	AudioInPin pinSync;
	AudioInPin pinPhaseMod;
	BoolInPin pinBypass;
	IntInPin pinDcoMode;
	FloatInPin pinVoiceActive;

	bool previousActiveState = false;

public:
	enum EWaveShape{ WS_SINE, WS_SAW, WS_RAMP, WS_TRI, WS_PULSE, WS_WHITE_NOISE, WS_PINK_NOISE};

	Oscillator();
	virtual int32_t MP_STDCALL open();

	void startGrain( phasor_t initCount, float increment, int fadeIncrement = 0);

	const static int maxVolts = 10;
	inline float SampleToVoltage(float s)
	{
		return s * (float)maxVolts;
	}
	inline float SampleToFrequency(float volts)
	{
		return 440.f * powf(2.f, SampleToVoltage(volts) - (float)maxVolts * 0.5f);
	}
	inline float ComputeIncrement(float SampleRate, float pitch)
	{
		return SampleToFrequency(pitch) / SampleRate;
	}

	void sub_process_white_noise( int sampleFrames );
	void sub_process_pink_noise( int sampleFrames );
	void sub_process_silence(int sampleFrames);

	inline void DoSync2(float phase, float sync, float prevSync)
	{
		for (int g2 = 0; g2 < MaxGrains; ++g2)
		{
			if (grains[g2].waveSize != 0 && grains[g2].fadeIncrement != -1) // indicates active grain.
			{
				grains[g2].fadeIncrement = -1;
				grains[g2].fadeIndex = syncCrossFadeSamples - 1;
			}
		}

		float count = increment * sync / (sync - prevSync);
		startGrain(100.0 + count - phase, increment, 1); // 5 is to guard against count going negative when phase is high.
	}

	template< class WaveShapePolicy, class PitchModulationPolicy, class phaseModulationPolicy, class SyncModulationPolicy, class InterpolationPolicy >
	void sub_process_template2(int sampleFrames)
	{
		// get pointers to in/output buffers.
		float* signalOut = getBuffer(pinSignalOut);
		const float* pitch = PitchModulationPolicy::getBuffer(this, pinPitch);
		const float* sync = SyncModulationPolicy::getBuffer(this, pinSync);
		const float* phase = getBuffer(pinPhaseMod); // Need phase available for sync regardless of being modulated or not.
		const float* pulseWidth = WaveShapePolicy::getBuffer(this, pinPulseWidth);

		phasor_t delataPhase = 0;
		float pw = 0;

		for (int s = sampleFrames; s > 0; --s)
		{
			PitchModulationPolicy::Calculate(pitchTable, pitch, increment);

			if (phaseModulationPolicy::Active)
			{
				delataPhase = prevPhase - *phase;
				prevPhase = *phase;
			}

			// trigger sync?
			if (SyncModulationPolicy::Active)
			{
				if ((*sync > 0.0f) != syncState)
				{
					syncState = *sync > 0.0f;
					if (syncState)
					{
						DoSync2(*phase + (float)delataPhase, *sync, prevSync);
					}
				}
				prevSync = *sync;
				++sync;
			}

			float samp = 0.0f;
			for (int g = 0; g < MaxGrains; ++g)
			{
				if (grains[g].waveSize)
				{
					WaveShapePolicy::CalcPW(pulseWidth, pw);

					float grainSample = InterpolationPolicy::Interpolate(grains[g].count, grains[g]);

					if constexpr(WaveShapePolicy::Active) // i.e. is - pulsewave.
					{
						const auto c = WaveShapePolicy::CalcPhaseB(grains[g].count, pw);
						grainSample = InterpolationPolicy::Interpolate(c, grains[g]) - grainSample;
					}

					if (grains[g].fadeIncrement != 0) // This grain being rapid-faded?
					{
						grainSample *= syncFadeCurve_[grains[g].fadeIndex];

						if (grains[g].fadeIncrement > 0)
						{
							if (grains[g].fadeIndex == syncCrossFadeSamples - 1)
							{
								grains[g].fadeIncrement = 0; // indicates steady active grain.
							}
							grains[g].fadeIndex += 1;
						}
						else
						{
							if (grains[g].fadeIndex == 0)
							{
								grains[g].waveSize = 0; // indicates inactive grain.
							}
							grains[g].fadeIndex -= 1;
						}
					}

					samp += WaveShapePolicy::ModifySign(grainSample);

					grains[g].count += increment;

					if (phaseModulationPolicy::Active)
					{
						grains[g].count += delataPhase;
						if (grains[g].count < 0.0f) // wrapped -ve?
						{
							grains[g].count += 10.0f;
						}
					}
				}
			}
			*signalOut = samp;

			// Increment buffer pointers.
			PitchModulationPolicy::IncrementPointer(pitch);
			phaseModulationPolicy::IncrementPointer(phase);
			WaveShapePolicy::IncrementPointer(pulseWidth);
			++signalOut;
		}

		int activeGrains = 0;
		for (int g = 0; g < MaxGrains; ++g)
		{
			if (grains[g].waveSize)
			{
				++activeGrains;
				if (grains[g].count >= 1.0)
				{
					// count wraps at 1.0
					int count_floor = FastRealToIntTruncateTowardZero(grains[g].count);
					grains[g].count -= (phasor_t)count_floor;

					if ((grains[g].maxIncrement < increment || grains[g].minIncrement > increment) && grains[g].fadeIncrement == 0)
					{
						// Fade out old grain.
						grains[g].fadeIncrement = -1;
						grains[g].fadeIndex = syncCrossFadeSamples - 1;

						// Fade in new one at new MIP level.
						startGrain(grains[g].count, (float)increment, 1);
					}
				}
			}
		}

		if (activeGrains == 1)
		{
			ChooseProcessMethod();
		}
	}

	template< class WaveShapePolicy, class PitchModulationPolicy, class phaseModulationPolicy, class InterpolationPolicy >
	void sub_process_template2fast(int sampleFrames)
	{
		// get pointers to in/output buffers.
		float* signalOut = getBuffer(pinSignalOut);
		const float* pitch = PitchModulationPolicy::getBuffer(this, pinPitch);
		const float* phase = phaseModulationPolicy::getBuffer(this, pinPhaseMod);
		const float* pulseWidth = WaveShapePolicy::getBuffer(this, pinPulseWidth);

		phasor_t delataPhase = 0;
		float pw = 0;

		for (int s = sampleFrames; s > 0; --s)
		{
			PitchModulationPolicy::Calculate(pitchTable, pitch, increment);

			if (phaseModulationPolicy::Active)
			{
				delataPhase = prevPhase - *phase;
				prevPhase = *phase;
			}

#ifdef _DEBUG
			assert(grains[0].waveSize != 0);
			for (int g = 1; g < MaxGrains; ++g)
			{
				assert(grains[g].waveSize == 0);
			}
#endif
			float samp;
			{
				const int g = 0;
				assert(grains[g].waveSize);
				{
					WaveShapePolicy::CalcPW(pulseWidth, pw);

					samp = InterpolationPolicy::Interpolate(grains[g].count, grains[g]);

					if (WaveShapePolicy::Active) // i.e. is - pulsewave.
					{
						samp = InterpolationPolicy::Interpolate(WaveShapePolicy::CalcPhaseB(grains[g].count, pw), grains[g]) - samp;
					}

					assert(grains[g].fadeIncrement == 0); // This grain being rapid-faded?

					samp = WaveShapePolicy::ModifySign(samp);

					grains[g].count += increment;

					if (phaseModulationPolicy::Active)
					{
						grains[g].count += delataPhase;
						if (grains[g].count < 0.0f) // wrapped -ve?
						{
							grains[g].count += 10.0f;
						}
					}
				}
			}
			*signalOut = samp;

			// Increment buffer pointers.
			++signalOut;
			PitchModulationPolicy::IncrementPointer(pitch);
			phaseModulationPolicy::IncrementPointer(phase);
			WaveShapePolicy::IncrementPointer(pulseWidth);
		}

		{
			const int g = 0;
			assert(grains[g].waveSize);
			{
				if (grains[g].count >= 1.0)
				{
					// count wraps at 1.0
					int count_floor = FastRealToIntTruncateTowardZero(grains[g].count);

					grains[g].count -= (phasor_t)count_floor;

					if ((grains[g].maxIncrement < increment || grains[g].minIncrement > increment) && grains[g].fadeIncrement == 0)
					{
						// Fade out old grain.
						grains[g].fadeIncrement = -1;
						grains[g].fadeIndex = syncCrossFadeSamples - 1;

						// Fade in new one at new MIP level.
						startGrain(grains[g].count, (float)increment, 1);
						ChooseProcessMethod();
					}
				}
			}
		}
	}

	void recalculatePitch();
	void resetOscillator();
	void doSync(bool newSyncState);
	virtual void onSetPins(void);
	void CalcWave(float* spectrum, float* dest, const WavetableMipmapPolicy& mipMapPolicy, const char* debug_waveshape_name);
	void ChooseProcessMethod();
};

