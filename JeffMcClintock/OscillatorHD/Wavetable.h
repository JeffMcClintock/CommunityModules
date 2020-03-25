#pragma once

/*
#include "Wavetable.h"
*/

//#include "mp_sdk_stdint.h"
#include <algorithm>
#include <string>
#include <vector>
//#include "xplatform.h"
#include "math.h"

#define NUM_WAVETABLE_OSCS 2
#define SE_WT_OSC_STORE_HALF_CYCLES

typedef wchar_t* _TCHAR;

struct Genes
{
	double referenceDb;
	double retainDb; // = 50.0f; // Whitener - how much spectrum to keep. throws away anything less than this below peak shape.
	double WhiteningFilter; // = 50.0f; // Whitener - how much spectrum to keep. throws away anything less than this below peak shape.
};


// An collection of Wavetables.
struct WaveTable
{
	static const int FactoryWavetableCount = 64;
	static const int UserWavetableCount = 20;
	static const int WavetableFileSlotCount = 64;
	static const int WavetableFileSampleCount = 512;

#if defined(_DEBUG)
    static const int MorphedSlotRatio = 8;		// lighter memory.
	static const int MaximumTables = 64;
	static int debugSlot;
	static float* diagnosticOutput;
	static float* diagnosticProbeOutput;
#else
    static const int MorphedSlotRatio = 8;		// For every imported slot, generate 7 in-between slots via FFT morph.
    static const int MaximumTables = 64;
#endif

	int32_t waveTableCount;	// Total Wavetable slots.
	int32_t slotCount;		// Waveforms per wavetable.
	int32_t waveSize;		// Samples in each single waveform.
	float Wavedata[1];		// Actual size depends on number of slots etc.

	float* GetSlotPtr( int table, int slot )
	{
		table = (std::max)( (std::min)( table, waveTableCount - 1 ), 0 );
		return Wavedata + ( table * slotCount + slot ) * waveSize;
	}

	static int CalcMemoryRequired( int tableCount, int slotCount, int waveSize )
	{
		return sizeof( WaveTable ) + sizeof(float) * waveSize * slotCount * tableCount;
	}

    void SetSize( int numWaveTables, int numWaveSlots, int numWaveSamples )
    {
        waveTableCount = numWaveTables;
        slotCount = numWaveSlots;
        waveSize = numWaveSamples;
    }

	static void NormalizeWave( std::vector<float>& wave );
	void GenerateWavetable( int wavetableNumber, int selectedFromSlot, int selectedToSlot, int shape );
	void CopyAndMipmap( WaveTable* sourceWavetable, class WavetableMipmapPolicy &mipinfo );
	void CopyAndMipmap2(WavetableMipmapPolicy &destMipInfo, int wavetable, float* destSamples);
	void CopyAndMipmap3( WavetableMipmapPolicy &destMipInfo, int wavetable, float* destSamples );
};

