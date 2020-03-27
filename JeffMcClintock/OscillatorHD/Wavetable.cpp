#define _USE_MATH_DEFINES
#define NOMINMAX

#include <math.h>
#include <vector>
#include <fstream>
#include <assert.h>
#include "float.h"
#include "real_fft.h"
#include "OscMipmapPolicy.h"
//include "unicode_conversion.h"
//#include "mp_sdk_stdint.h"
//#include "../../mfc_emulation.h"

#include <windows.h>
#include "Shlobj.h"

using namespace std;

#define FIX_ZERO_CROSSINGS

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM     1
#endif

#if defined(_DEBUG)
int WaveTable::debugSlot = -1;
float* WaveTable::diagnosticOutput = 0;
float* WaveTable::diagnosticProbeOutput = 0;
#endif

struct MYWAVEFORMATEX
{
    uint16_t  wFormatTag;         /* format type */
    uint16_t  nChannels;          /* number of channels (i.e. mono, stereo...) */
    int32_t	  nSamplesPerSec;     /* sample rate */
    int32_t   nAvgBytesPerSec;    /* for buffer estimation */
    uint16_t  nBlockAlign;        /* block size of data */
    uint16_t  wBitsPerSample;     /* number of bits per sample of mono data */
    uint16_t  cbSize;             /* the count in bytes of the size of */
                                    /* extra information (after cbSize) */
};// MYWAVEFORMATEX; //, *PWAVEFORMATEX, NEAR *NPWAVEFORMATEX, FAR *LPWAVEFORMATEX;


void WaveTable::NormalizeWave( vector<float>& wave )
{
	// normalise wave.
	float maximum = 0.0f;
	for( unsigned int i = 0 ; i < wave.size() ; ++i )
	{
		maximum = (std::max)( maximum, fabsf(wave[i]) );
	}
	maximum = (std::max)(maximum, 0.00001f); // prevent divide by zero.

	float scale = 0.5f / maximum;
	for( unsigned int i = 0 ; i < wave.size() ; ++i )
	{
		wave[i] *= scale;
	}
}


float AutoCorrelate( vector<float>& sample, int cycleStart, int autocorrelateto, int correlateCount )
{
    float error = 0.0;
    for (int j = 0; j < correlateCount; ++j)
    {
		float s1, s2;
		int i = cycleStart + j;
		if( i < (int)sample.size() && i >= 0 )
		{
			s1 = sample[i];
		}
		else
		{
			s1 = 0.0f;
		}
		
		i = cycleStart + autocorrelateto + j;
		if( i < (int)sample.size() && i >= 0 )
		{
			s2 = sample[i];
		}
		else
		{
			s2 = 0.0f;
		}

        float diff = s1 - s2;
        error += diff * diff;
	}

	return error;
}
// Golden seach stuff.
#define R 0.61803399
// The golden ratios.
#define C (1.0-R)
#define SHFT2(a,b,c) (a)=(b);(b)=(c);
#define SHFT3(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);

using namespace std;


// - QDSS Windowed Sinc ReSampling subroutine.

// function parameters
// : x      = new sample point location (relative to old indexes)
//            (e.g. every other integer for 0.5x decimation)
// : indat  = original data array
// : alim   = size of data array
// : fmax   = low pass filter cutoff frequency
// : fsr    = sample rate
// : wnwdth = width of windowed Sinc used as the low pass filter

// resamp() returns a filtered new sample point

float resamp( float x, vector<float>& indat, float fmax, float fsr, int wnwdth)
{
    float r_g,r_w,r_a,r_snc,r_y; // some local variables
    r_g = 2 * fmax / fsr;            // Calc gain correction factor
    r_y = 0;
    for( int i = -wnwdth/2 ; i < (wnwdth/2)-1 ; ++ i) // For 1 window width
    {
        int j = (int) (x + i);           // Calc input sample index

        // calculate von Hann Window. Scale and calculate Sinc
        r_w     = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (0.5f + (j - x)/wnwdth) );
        r_a     = 2.0f * (float)M_PI * (j - x) * fmax/fsr;
        r_snc   = 1.0f;

        if( r_a != 0.0f )
        {
            r_snc = sinf(r_a) / r_a;
        }

        if( (j >= 0) && (j < (int) indat.size()) )
        {
            r_y = r_y + r_g * r_w * r_snc * indat[j];
        }
    }
    return r_y;                   // Return new filtered sample
}

// datasize must be power-of-two.
float resampleCyclic( float x, float* indat, int dataSize, float fmax = 0.5f, float fsr = 1.0f, int wnwdth = 20 )
{
    float r_g,r_w,r_a,r_snc,r_y; // some local variables
    r_g = 2 * fmax / fsr;            // Calc gain correction factor
    r_y = 0;
    for( int i = -wnwdth/2 ; i < (wnwdth/2)-1 ; ++ i) // For 1 window width
    {
        int j = (int) (x + i);           // Calc input sample index

        // calculate von Hann Window. Scale and calculate Sinc
        r_w     = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (0.5f + (j - x)/wnwdth) );
        r_a     = 2.0f * (float)M_PI * (j - x) * fmax/fsr;
        r_snc   = 1.0f;

        if( r_a != 0.0f )
        {
            r_snc = sinf(r_a) / r_a;
        }

//        if( (j >= 0) && (j < indat.size()) )
        {
            r_y = r_y + r_g * r_w * r_snc * indat[j & (dataSize-1)];
        }
    }
    return r_y;                   // Return new filtered sample
}


float AutoCorrelateFractional( vector<float>& sample, int cycleStart, float autocorrelateto, int correlateCount )
{
    float error = 0.0;
    for (int j = 0; j < correlateCount; ++j)
    {
		float s1, s2;
		if( cycleStart + j < (int)sample.size() && cycleStart + j >= 0 )
		{
			s1 = sample[cycleStart + j];
		}
		else
		{
			s1 = 0.0f;
		}

        const int sincSize = 20;
        const float filtering = 0.5f;
		s2 = resamp(cycleStart + j + autocorrelateto, sample, filtering, 1.0, sincSize);

        float diff = s1 - s2;
        error += diff * diff;
	}

	return error;
}


struct wave_file_header
{
	char chnk1_name[4];
	int32_t chnk1_size;
	char chnk2_name[4];
	char chnk3_name[4];
	int32_t chnk3_size;
	uint16_t wFormatTag;
	uint16_t nChannels;
	int32_t nSamplesPerSec;
	int32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	char chnk4_name[4];
	int32_t chnk4_size;
};

void WaveTable::GenerateWavetable( int wavetableNumber, int selectedFromSlot, int selectedToSlot, int shape )
{
    assert( wavetableNumber >= 0 && wavetableNumber < waveTableCount );
    assert( selectedFromSlot >= 0 && selectedFromSlot < slotCount );
    assert( selectedToSlot >= 0 && selectedToSlot < slotCount );

	WaveTable* waveTable = this;
	vector<float> harmonics;

	switch( shape )
	{
	case 7: // Noise.
		{
			for( int slot = selectedFromSlot ; slot <= selectedToSlot ; ++slot )
			{
				// White noise
				float* dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + slot) * waveTable->waveSize;
				for( int i = 0 ; i < waveTable->waveSize ; ++i )
				{
					dest[i] = rand() / (float) RAND_MAX - 0.5f;
				}
			}
		}
		break;

	case 8: // Random.
		{
			/*
			// White noise
			float* dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + selectedSlot) * waveTable->waveSize;
			for( int i = 0 ; i < waveTable->waveSize ; ++i )
			{
				dest[i] = rand() / (float) RAND_MAX - 0.5f;
			}
			*/

			// Random harmonics.
			int totalHarmonics = waveTable->waveSize / 2;
			harmonics.resize(totalHarmonics);
			int mask = rand() & 0x3;
			mask = max(mask,1);
			int mask2 = rand() & 0x1;
			float falloff = rand() / (float) RAND_MAX ;
			for( int partial = 1 ; partial < totalHarmonics ; ++partial )
			{
				float level = rand() / (float) RAND_MAX - 0.5f;
				level = level / (partial * falloff); // pinkify it.
				if( ((partial & mask) == 0) == mask2 ) // mix up even/odd harmonics.
				{
					level = 0.0f;
				}
				harmonics[partial] = level;
			}
		}
		break;
	case 9: // Silence.
		{
			harmonics.resize(2); // entry 0 not used.
			harmonics[1] = 0.0f;
		}
		break;
	case 10: // DC.
		{
			for( int slot = selectedFromSlot ; slot <= selectedToSlot ; ++slot )
			{
				float* dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + slot) * waveTable->waveSize;
				for( int i = 0 ; i < waveTable->waveSize ; ++i )
				{
					dest[i] = 1.0f;
				}
			}
		}
		break;
	case 2: // Sine.
		{
			harmonics.resize(2); // entry 0 not used.
			harmonics[1] = 1.0f;

			//harmonics.resize( 3 ); // entry 0 not used.
			//harmonics[1] = 1.0f;
			//harmonics[2] = 1.0f;
		}
		break;
	case 0: // Saw.
	case 4: // Pulse 15%.
	case 5: // Pulse 50%.
	case 6: // Pulse 85%.
		{
			int totalHarmonics = waveTable->waveSize / 2;
			harmonics.resize(totalHarmonics);
			for( int partial = 1 ; partial < totalHarmonics ; ++partial )
			{
				float level = -1.0f / partial;
				harmonics[partial] = level;
			}
		}
		break;
	case 1: // Ramp.
		{
			int totalHarmonics = waveTable->waveSize / 2;
			harmonics.resize(totalHarmonics);
			for( int partial = 1 ; partial < totalHarmonics ; ++partial )
			{
				float level = 1.0f / partial;
				harmonics[partial] = level;
			}
		}
		break;
	case 3: // Triangle.
		{
			int totalHarmonics = waveTable->waveSize / 2;
			harmonics.resize(totalHarmonics);
			for( int partial = 1 ; partial < totalHarmonics ; ++partial )
			{
				float level = 1.0f / (partial * partial);
				if( (partial & 0x01) == 0 )
				{
					level = 0.0f;
				}

				if( (partial>>1) & 1 ) // every 2nd harmonic inverted
				{
					level = -level;
				}

				harmonics[partial] = level;
			}
		}
		break;
	}

	if( harmonics.size() > 0 ) //shape != 7 ) // noise 
	{
		int totalHarmonics = static_cast<int>(harmonics.size());

		/* moved to mipmap
		// windowing function to reduce gibbs phenomena (hamming). Reduced effectivness once mip-mapp truncates series anyhow.
		for( int partial = 1 ; partial < totalHarmonics ; ++partial )
		{
			float window = 0.5f + 0.5f * cosf( (partial - 1.f) * (float)M_PI / (float) totalHarmonics );
			harmonics[partial] *= window;
		}
		*/
		float maximum = 0.0f;
		for( int slot = selectedFromSlot ; slot <= selectedToSlot ; ++slot )
		{
			float* dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + slot) * waveTable->waveSize;
			for( int i = 0 ; i < waveTable->waveSize ; ++i )
			{
				// Sum harmonics.
				float s = 0.0f;
				for( int partial = 1 ; partial < totalHarmonics ; ++partial )
				{
					s += harmonics[partial] * sinf( (float) partial * 2.0f * (float)M_PI * (float) i / (float) waveTable->waveSize );
				}
				dest[i] = s;
				maximum = std::max(maximum,s);
			}

			// Pulse (adds two saws).
			switch( shape )
			{
			case 4: // Pulse 15%.
			case 5: // Pulse 50%.
			case 6: // Pulse 85%.
				{
					vector<float> inverseSaw;
					inverseSaw.resize(waveTable->waveSize);
					for( int i = 0 ; i < waveTable->waveSize ; ++i )
					{
						inverseSaw[i] = dest[i];
					}

					int offset = waveTable->waveSize / 2;
					switch( shape )
					{
					case 4: // Pulse 15%.
						offset = (15 * waveTable->waveSize) / 100;
						break;
					case 5: // Pulse 50%.
						offset = waveTable->waveSize / 2;
						break;
					case 6: // Pulse 85%.
						offset = (85 * waveTable->waveSize) / 100;
						break;
					};

					for( int i = 0 ; i < waveTable->waveSize ; ++i )
					{
						dest[i] -= inverseSaw[(i + offset) % waveTable->waveSize];
					}
				}
				break;
			default:
				break;
			}

			// Normalise.
			if( maximum > 0.000000001f ) // avoid divide by zero on silence.
			{
				float scale = 0.5f / maximum;
				for( int i = 0 ; i < waveTable->waveSize ; ++i )
				{
					dest[i] *= scale;
				}
			}
		}
	}

	
	/* old
	case GW_ANALOG_WAVES:
		{
			float* dest;
			dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount) * waveTable->waveSize;
			// clear out existing.
			for( int i = 0 ; i < waveTable->waveSize * waveTable->slotCount ; ++ i )
			{
				dest[i] = 0.0f;
			}

			int waveNumber = -1;

			// Sine.
			waveNumber++;
			dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + waveNumber) * waveTable->waveSize;
			for( int i = 0 ; i < waveTable->waveSize ; ++i )
			{
				dest[i] = 0.5f * sinf( 2.0f * (float)M_PI * (float) i / (float) waveTable->waveSize );
			}

			// triangle.
			waveNumber++;;
			dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + waveNumber) * waveTable->waveSize;
			for( int i = 0 ; i < waveTable->waveSize ; ++i )
			{
				float s = -2.f * (float) i / (float) waveTable->waveSize + 1.0f;
				if( s > 0.5f )
					s = -s + 1.0f;
				if( s < -0.5f )
					s = -s - 1.0f;

				dest[i] = s;
//_RPT1(_CRT_WARN, "%f\n", s );
			}

			// Square.
			waveNumber++;
			dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + waveNumber) * waveTable->waveSize;
			for( int i = 0 ; i < waveTable->waveSize ; ++i )
			{
				dest[i] = i < (waveTable->waveSize/2) ? 0.5f : -0.5f;
			}

			// Sawtooth.
			waveNumber++;
			dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + waveNumber) * waveTable->waveSize;
			for( int i = 0 ; i < waveTable->waveSize ; ++i )
			{
				dest[i] = -((float) i / (float) waveTable->waveSize - 0.5f);
			}
			invalidateRect();

			// Update patchmem and DSP.
			pinWaveBank.sendPinUpdate();
	}
	break;

	case GW_PULSE_WIDTH_MODULATION:
		{
			float* dest;
			
			for( int waveNumber = 0 ; waveNumber < waveTable->slotCount ; ++waveNumber )
			{
				float pw = (float)waveNumber / (float)waveTable->slotCount;
				int pulsewidth = (waveNumber * waveTable->waveSize) / waveTable->slotCount;
				// Pulse.
				dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + waveNumber) * waveTable->waveSize;
				for( int i = 0 ; i < waveTable->waveSize ; ++i )
				{
					dest[i] = i > pulsewidth ? pw - 1.0f : pw;
				}
			}
		}
		break;

	case GW_SAW_SYNC_SWEEP:
		{
			for( int waveNumber = 0 ; waveNumber < waveTable->slotCount ; ++waveNumber )
			{
				int pulsewidth = waveTable->waveSize - (waveNumber * waveTable->waveSize) / waveTable->slotCount;

				float* dest = waveTable->Wavedata + (wavetableNumber * waveTable->slotCount + waveNumber) * waveTable->waveSize;
				int j = 0;
				for( int i = 0 ; i < waveTable->waveSize ; ++i )
				{
					dest[i] = -((float) j++ / (float) waveTable->waveSize - 0.5f);
					if( j > pulsewidth )
					{
						j = 0;
					}
				}
			}
		}
		break;
	}
*/
}


void CalcMagnitudePhaseSpectrum( float* dest, float* src, int sourceSize )
{
	memcpy( dest, src, sizeof(float) * sourceSize );
	realft( dest - 1, sourceSize, 1 );

	/* no, leave DC-removal to import. For diagnostics, it's useful to be able to play DC through the PSOLA effect.
	// zero-out DC, nyquist values.
	spectrum2[0] = 0.0f;
	spectrum2[1] = 0.0f;
	*/

	// normalise spectrum.
    float scale = 2.0f / (float)sourceSize;
	for( int i = 0 ; i < sourceSize; ++i )
	{
		dest[i] *=  scale;
	}

	// convert to magnitude/phase format.
	for( int s = 2 ; s < sourceSize ; s += 2 )
	{
		float phase = atan2( dest[s], dest[s+1] );
		float magnitude = sqrtf(dest[s] * dest[s] + dest[s+1] * dest[s+1] );
		dest[s] = magnitude;
		dest[s + 1] = phase;
	}

	dest[0] *= 0.5f;	// normalise DC.
	dest[1] = (float) M_PI_2;	// allows DC to be treated correctly.
}

// PLease use CopyAndMipmap2 instead.
void WaveTable::CopyAndMipmap( WaveTable* sourceWavetable, WavetableMipmapPolicy &mipInfo )
{
//    _RPT0(_CRT_WARN, "CopyAndMipmap(). START....\n" );

	assert( sourceWavetable->waveTableCount == 1 ); // only handles a single wavetable.
/*
    // Calculate size of DSP MIP-Mapped wavetable.
    WaveTable newDimensions = *sourceWavetable;
	newDimensions.waveTableCount = 1;
	newDimensions.slotCount = WaveTable::MorphedSlotRatio * (sourceWavetable->slotCount - 1 ) + 1; // add extra slots in-between.

	// Mip-maps require extra memory. Calculate.
	WavetableMipmapPolicy mipInfo;
	mipInfo.initialize(&newDimensions);
*/
	int table = 0;

	const int fftMaxSize = 1024;

	float spectrum1[fftMaxSize + 2];
	float spectrum2[fftMaxSize + 2];
	float spectrum[fftMaxSize + 2]; // morphed spectrum.

	int sourceSize = sourceWavetable->waveSize;

	// zero out unneeded FFT data.
	for( int s = sourceSize ; s < fftMaxSize ; ++s )
	{
		spectrum1[s] = 0.0f;
		spectrum2[s] = 0.0f;
		spectrum[s] = 0.0f;
	}

	// Preload slot zero.
	float* src = sourceWavetable->Wavedata + sourceWavetable->waveSize * ( table * sourceWavetable->slotCount );

	CalcMagnitudePhaseSpectrum( spectrum2, src, sourceSize );

	// Generate in-between 'ghost' slots.
	for( int slot = 0 ; slot < sourceWavetable->slotCount - 1 ; ++slot )
	{
		memcpy( spectrum1, spectrum2, sizeof(spectrum1) );

		// load the upper slot.
		float* src = sourceWavetable->Wavedata + sourceWavetable->waveSize * ( slot + 1 + table * sourceWavetable->slotCount );
		CalcMagnitudePhaseSpectrum( spectrum2, src, sourceSize );

		int morphTo = WaveTable::MorphedSlotRatio;
		if( slot == sourceWavetable->slotCount - 2 ) // on very last slot, do the extra one.
		{
			++morphTo;
		}

		for( int morphSlot = 0 ; morphSlot < morphTo ; ++morphSlot )
		{
			float morph = (float) morphSlot / (float) WaveTable::MorphedSlotRatio;

			int destSlot = slot * WaveTable::MorphedSlotRatio + morphSlot;

			// morph specturm smoothly.
			for( int s = 0 ; s < sourceSize ; s += 2 )
			{
				float magnitude1 = spectrum1[s];
				float phase1 = spectrum1[s+1];
				float magnitude2 = spectrum2[s];
				float phase2 = spectrum2[s+1];

				float magnitude = magnitude1 + morph * ( magnitude2 - magnitude1 );
				float phaseDelta = phase2 - phase1;
				if( phaseDelta > M_PI ) // choose shortest phase wrap
				{
					phaseDelta -= (float) M_PI * 2.0f;
				}
				else
				{
					if( phaseDelta < -M_PI ) // choose shortest phase wrap
					{
						phaseDelta += (float) M_PI * 2.0f;
					}
				}
				float phase = phase1 + morph * phaseDelta;

				spectrum[s] = magnitude * sinf(phase);
				spectrum[s+1] = magnitude * cosf(phase);
			}

			spectrum[0] *= 2.0f; // DC Level needs to be twice other partials.
			spectrum[1] = 0.0f; // remove 'phase' of DC componenet, else ends up as a false nyquist level.

			// for each mip level.
			for( int mip = 0 ; mip < mipInfo.getMipCount() ; ++mip )
			{
//				_RPT1(_CRT_WARN, "%x\n", ( dest- waveTableMipMapped->Wavedata ) / 4 );
				// halve the size of the FFT, removing upper octave.
				// Perform inverse FFT.
				// Copy to dest.
				float waveDownsampled[1024];
				int mipWaveSize = mipInfo.GetWaveSize(mip);
				int binCount = mipInfo.GetFftBinCount(mip); // number of harmonics plus 1 (DC component).
				float scale2 = (float)mipWaveSize * 0.5f;

				// Copy required number of harmonics.
				for( int i = 0 ; i < binCount * 2; ++i )
				{
					waveDownsampled[i] = spectrum[i] * scale2;
				}

				// zero-out unwanted high harmonics.
				for( int i = binCount * 2 ; i < mipWaveSize; ++i )
				{
					waveDownsampled[i] = 0.0f;
				}

				realft2( waveDownsampled, (unsigned int) mipWaveSize, -1 );

				float scale = 2.0f / mipWaveSize;

				float* dest = Wavedata + mipInfo.getSlotOffset( 0, destSlot, mip );

				#ifdef SE_WT_OSC_STORE_HALF_CYCLES // Assume symetrical wave.
					for( int count = 0 ; count < mipWaveSize / 2 ; ++count )
					{
						*dest++ = waveDownsampled[count] * scale;
					}
				#else
					for( int count = 0 ; count < mipWaveSize ; ++count )
					{
						*dest++ = waveDownsampled[count] * scale;
					}
				#endif
			}
		}
	}
//    _RPT0(_CRT_WARN, "CopyAndMipmap(). DONE....\n" );
}

void WaveTable::CopyAndMipmap2(WavetableMipmapPolicy &destMipInfo, int wavetable, float* destSamples)
{
	// New
	{
		const int interpolationSamples = 4;
		float spectrumSource[interpolationSamples][WaveTable::WavetableFileSampleCount];

		// prepare spectrum interpolation array. need prev, current and 2 future.
		for( int slot = -3; slot < slotCount; ++slot )
		{
			// shuffle spectrum.
			for( int i = 0; i < interpolationSamples - 1; ++i )
			{
				memcpy( spectrumSource[i], spectrumSource[i + 1], sizeof( spectrumSource[0] ) );
			}

			// load next slot.
			int loadSlot = (std::min)( (std::max)( slot + 2, 0 ), slotCount - 1);
			float* src = GetSlotPtr( wavetable, loadSlot );
			CalcMagnitudePhaseSpectrum( spectrumSource[interpolationSamples - 1], src, WaveTable::WavetableFileSampleCount );

			if( slot >= 0 && slot < slotCount - 1 )
			{
				int morphs = WaveTable::MorphedSlotRatio;
				if( slot == slotCount - 2 ) // on very last slot, do the extra one.
				{
					++morphs;
				}

				for( int morphSlot = 0; morphSlot < morphs; ++morphSlot )
				{
					float fraction = (float) morphSlot / (float) WaveTable::MorphedSlotRatio;

					int destSlot = slot * WaveTable::MorphedSlotRatio + morphSlot;

					// morph spectrum smoothly. ignore phase.
					float spectrum[WaveTable::WavetableFileSampleCount];
					for( int s = 0; s < WaveTable::WavetableFileSampleCount; s += 2 )
					{
						/*
						// linear.
						float magnitude1 = spectrumSource[1][s];
						float magnitude2 = spectrumSource[2][s];
						float magnitude = magnitude1 + fraction * ( magnitude2 - magnitude1 );
						*/

						// cubic.
						float y0 = spectrumSource[0][s];
						float y1 = spectrumSource[1][s];
						float y2 = spectrumSource[2][s];
						float y3 = spectrumSource[3][s];
						float magnitude = y1 + 0.5f * fraction*( y2 - y0 + fraction*( 2.0f*y0 - 5.0f*y1 + 4.0f*y2 - y3 + fraction*( 3.0f*( y1 - y2 ) + y3 - y0 ) ) );

						spectrum[s] = 0.0f;
						spectrum[s + 1] = magnitude;
					}

					spectrum[0] *= 2.0f; // DC Level needs to be twice other partials.
					spectrum[1] = 0.0f; // remove 'phase' of DC componenet, else ends up as a false nyquist level.

					// for each mip level.
					for( int mip = 0; mip < destMipInfo.getMipCount( ); ++mip )
					{
						//				_RPT1(_CRT_WARN, "%x\n", ( dest- waveTableMipMapped->Wavedata ) / 4 );
						// halve the size of the FFT, removing upper octave.
						// Perform inverse FFT.
						// Copy to dest.
						float waveDownsampled[1024];
						int mipWaveSize = destMipInfo.GetWaveSize( mip );
						int binCount = destMipInfo.GetFftBinCount( mip ); // number of harmonics plus 1 (DC component).
						float scale2 = (float) mipWaveSize * 0.5f;

						// Copy required number of harmonics.
						for( int i = 0; i < binCount * 2; ++i )
						{
							waveDownsampled[i] = spectrum[i] * scale2;
						}

						// zero-out unwanted high harmonics.
						for( int i = binCount * 2; i < mipWaveSize; ++i )
						{
							waveDownsampled[i] = 0.0f;
						}

						realft2( waveDownsampled, (unsigned int) mipWaveSize, -1 );

						float scale = 2.0f / mipWaveSize;

						float* dest = destSamples + destMipInfo.getSlotOffset( wavetable, destSlot, mip );
						//				_RPT4(_CRT_WARN, "CopyAndMipmap(). WT:%d SL:%d MIP:%d offset:%d\n", wavetable, destSlot, mip, destMipInfo.getSlotOffset(wavetable, destSlot, mip));

						for( int count = 0; count < mipWaveSize / 2; ++count )
						{
							*dest++ = waveDownsampled[count] * scale;
						}
					}
				}
			}
		}
	}

//debug.
/*
	for( int s = 0; s < WaveTable::MorphedSlotRatio * slotCount - 1; ++s )
	{
		float* dest = destSamples + destMipInfo.getSlotOffset( 0, s, 0 );
		_RPT1( _CRT_WARN, "%f\n", dest[100] );
	}
*/
	/* old
	return;

    _RPT0(_CRT_WARN, "CopyAndMipmap(). START....\n" );
    WaveTable* sourceWavetable = this;

	assert( sourceWavetable->waveTableCount == 1 ); // only handles a single wavetable.

	int table = 0;

	const int fftMaxSize = 1024;

	float spectrum1[fftMaxSize + 2];
	float spectrum2[fftMaxSize + 2];
	float spectrum[fftMaxSize + 2]; // morphed spectrum.

	int sourceSize = sourceWavetable->waveSize;

	// zero out unneeded FFT data.
	for( int s = sourceSize ; s < fftMaxSize ; ++s )
	{
		spectrum1[s] = 0.0f;
		spectrum2[s] = 0.0f;
		spectrum[s] = 0.0f;
	}

	// Preload slot zero.
	float* src = sourceWavetable->Wavedata + sourceWavetable->waveSize * (table * sourceWavetable->slotCount);

	CalcMagnitudePhaseSpectrum( spectrum2, src, sourceSize );

	// Generate in-between 'ghost' slots.
	for( int slot = 0 ; slot < sourceWavetable->slotCount - 1 ; ++slot )
	{
		memcpy( spectrum1, spectrum2, sizeof(spectrum1) );

		// load the upper slot.
		float* src = sourceWavetable->Wavedata + sourceWavetable->waveSize * ( slot + 1 + table * sourceWavetable->slotCount );
		CalcMagnitudePhaseSpectrum( spectrum2, src, sourceSize );

		int morphTo = WaveTable::MorphedSlotRatio;
		if( slot == sourceWavetable->slotCount - 2 ) // on very last slot, do the extra one.
		{
			++morphTo;
		}

		for( int morphSlot = 0 ; morphSlot < morphTo ; ++morphSlot )
		{
			float morph = (float) morphSlot / (float) WaveTable::MorphedSlotRatio;

			int destSlot = slot * WaveTable::MorphedSlotRatio + morphSlot;

#ifdef FIX_ZERO_CROSSINGS

			// morph spectrum smoothly. ignore phase.
			for( int s = 0 ; s < sourceSize ; s += 2 )
			{
				float magnitude1 = spectrum1[s];
				float magnitude2 = spectrum2[s];
				float magnitude = magnitude1 + morph * ( magnitude2 - magnitude1 );

				spectrum[s] = 0.0f;
				spectrum[s+1] = magnitude;
			}
#else

			// morph spectrum smoothly.
			for( int s = 0 ; s < sourceSize ; s += 2 )
			{
				float magnitude1 = spectrum1[s];
				float phase1 = spectrum1[s+1];
				float magnitude2 = spectrum2[s];
				float phase2 = spectrum2[s+1];

				float magnitude = magnitude1 + morph * ( magnitude2 - magnitude1 );
				float phaseDelta = phase2 - phase1;
				if( phaseDelta > M_PI ) // choose shortest phase wrap
				{
					phaseDelta -= (float) M_PI * 2.0f;
				}
				else
				{
					if( phaseDelta < -M_PI ) // choose shortest phase wrap
					{
						phaseDelta += (float) M_PI * 2.0f;
					}
				}
				float phase = phase1 + morph * phaseDelta;

				spectrum[s] = magnitude * sinf(phase);
				spectrum[s+1] = magnitude * cosf(phase);
			}
#endif

			spectrum[0] *= 2.0f; // DC Level needs to be twice other partials.
			spectrum[1] = 0.0f; // remove 'phase' of DC componenet, else ends up as a false nyquist level.

			// for each mip level.
			for( int mip = 0 ; mip < destMipInfo.getMipCount() ; ++mip )
			{
//				_RPT1(_CRT_WARN, "%x\n", ( dest- waveTableMipMapped->Wavedata ) / 4 );
				// halve the size of the FFT, removing upper octave.
				// Perform inverse FFT.
				// Copy to dest.
				float waveDownsampled[1024];
				int mipWaveSize = destMipInfo.GetWaveSize(mip);
				int binCount = destMipInfo.GetFftBinCount(mip); // number of harmonics plus 1 (DC component).
				float scale2 = (float)mipWaveSize * 0.5f;

				// Copy required number of harmonics.
				for( int i = 0 ; i < binCount * 2; ++i )
				{
					waveDownsampled[i] = spectrum[i] * scale2;
				}

				// zero-out unwanted high harmonics.
				for( int i = binCount * 2 ; i < mipWaveSize; ++i )
				{
					waveDownsampled[i] = 0.0f;
				}

				realft( waveDownsampled - 1, (unsigned int) mipWaveSize, -1 );

				float scale = 2.0f / mipWaveSize;

				float* dest = destSamples + destMipInfo.getSlotOffset(wavetable, destSlot, mip);
//				_RPT4(_CRT_WARN, "CopyAndMipmap(). WT:%d SL:%d MIP:%d offset:%d\n", wavetable, destSlot, mip, destMipInfo.getSlotOffset(wavetable, destSlot, mip));

				#ifdef SE_WT_OSC_STORE_HALF_CYCLES // Assume symetrical wave.
					for( int count = 0 ; count < mipWaveSize / 2 ; ++count )
					{
						*dest++ = waveDownsampled[count] * scale;
					}
				#else
					for( int count = 0 ; count < mipWaveSize ; ++count )
					{
						*dest++ = waveDownsampled[count] * scale;
					}
				#endif
			}

		}
	}
//    _RPT0(_CRT_WARN, "CopyAndMipmap(). DONE....\n" );
	for( int s = 0; s < WaveTable::MorphedSlotRatio * sourceWavetable->slotCount - 1; ++s )
	{
		float* dest = destSamples + destMipInfo.getSlotOffset( 0, s, 0 );
		_RPT1(_CRT_WARN, "%f\n", dest[100]);
	}
	*/

}

// No morph slots, for Oscillator.
void WaveTable::CopyAndMipmap3( WavetableMipmapPolicy &destMipInfo, int wavetable, float* destSamples )
{
	const int maxWaveSize = 2048;
	float spectrum[maxWaveSize + 2];
	assert( waveSize <= maxWaveSize );

	// prepare spectrum interpolation array. need prev, current and 2 future.
	for( int slot = 0; slot < slotCount; ++slot )
	{
		// load slot.
		float* src = GetSlotPtr( wavetable, slot );
		memcpy( spectrum, src, sizeof( float ) * waveSize );
		realft2( spectrum, waveSize, 1 );

		// normalise spectrum.
		float scale = 2.0f / (float)waveSize;
		for( int i = 0; i < waveSize; ++i )
		{
			spectrum[i] *= scale;
		}

//hmmm?		spectrum[0] *= 2.0f; // DC Level needs to be twice other partials.
		spectrum[1] = 0.0f; // remove nyquist level.

		// for each mip level.
		for( int mip = 0; mip < destMipInfo.getMipCount(); ++mip )
		{
			//				_RPT1(_CRT_WARN, "%x\n", ( dest- waveTableMipMapped->Wavedata ) / 4 );
			// halve the size of the FFT, removing upper octave.
			// Perform inverse FFT.
			// Copy to dest.
			float waveDownsampled[maxWaveSize + 2];
			int mipWaveSize = destMipInfo.GetWaveSize( mip );
			int binCount = destMipInfo.GetFftBinCount( mip ) - 1; // number of harmonics plus 1 (DC component).
			float scale2 = (float)mipWaveSize * 0.5f;

			
//TODO. mayby use filter on output instead? so non-linier stuff keeps harmonics under control.
			// Copy required number of harmonics.
			for( int i = 0; i < binCount * 2; ++i )
			{
			/* test no gibbs fix
				// windowing function to reduce gibbs phenomena (hamming).
				float window = 0.5f + 0.5f * cosf( ((i / 2) - 1.f) * (float)M_PI / (float)binCount );
*/
				waveDownsampled[i] = spectrum[i] * scale2; // *window;
			}
			// zero-out unwanted high harmonics.
			for( int i = binCount * 2; i < mipWaveSize; ++i )
			{
				waveDownsampled[i] = 0.0f;
			}

			realft2( waveDownsampled, (unsigned int)mipWaveSize, -1 );

			float scale = 2.0f / mipWaveSize;

			float* dest = destSamples + destMipInfo.getSlotOffset( wavetable, slot, mip );
			//				_RPT4(_CRT_WARN, "CopyAndMipmap(). WT:%d SL:%d MIP:%d offset:%d\n", wavetable, destSlot, mip, destMipInfo.getSlotOffset(wavetable, destSlot, mip));

			_RPT0( _CRT_WARN, "-----------------------\n" );
			float* s = GetSlotPtr( wavetable, slot );
			for( int count = 0; count < mipWaveSize / 2; ++count )
			{
				*dest++ = waveDownsampled[count] * scale;
				_RPT2( _CRT_WARN, "%f, %f\n", s[count], waveDownsampled[count] * scale );
			}
		}
	}
}
