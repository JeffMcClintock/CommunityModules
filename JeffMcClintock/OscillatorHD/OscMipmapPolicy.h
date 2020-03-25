#pragma once

/*
#include "OscMipmapPolicy.h"
*/

#include "Wavetable.h"
#include <vector>

struct mipIndex
{
	mipIndex(int ss,int ws, int oversampleRatio, int off2, int ppartials = 0) :
		waveSize(ws)
		, SlotStorage(ss)
		, offset2(off2)
		, partials(ppartials)
	{
//		maximumIncrement = (float)oversampleRatio / (float)waveSize;
//		maximumIncrement = 0.5 / partials;
	}
	int SlotStorage;
	int offset2; // when grouped by mip,wave,slot
	int waveSize;
//	double maximumIncrement; // helps decide when to shift down to a lower mip-level.
	int partials;
};

#define maximumWaveSize 2048	// Largest wave stored.
#define minimumWaveSize 128		// Maintain clarity due to imperfections in interpolator.
#define oversampleRatio 4		// Improves quality of interpolator.

class WavetableMipmapPolicy
{
public:

private:
	std::vector<mipIndex> mips;
	int MasterWaveSize;
	int waveTableCount;			// Total Wavetable slots.
	int slotCount;				// Waveforms per wavetable.
	int totalWaveMemorySize;
	bool storeHalfCycles;

public:
	WavetableMipmapPolicy(void) :
		MasterWaveSize(0)
		, waveTableCount(0)
		, slotCount(0)
		, storeHalfCycles(true)
	{
	}

	int getMipCount() const
	{
		return (int) mips.size();
	}
	int GetWaveSize(int mip) const
	{
		return mips[mip].waveSize;
	}
	int GetFftBinCount( int mip ) const // number of components needed in reverse FFT (including DC Componenet)
	{
		int mostPartials = (std::min)( MasterWaveSize, maximumWaveSize ) / 2; // Number of partials in MIP zero (most harmonics).
		return 1 + ( mostPartials >> mip ); // add one for DC component.
	}

	double GetMaximumIncrement( int mip ) const
	{
		if (mip > 0)
		{
			return 0.5 / mips[mip].partials;
		}
		return 10000;
	}
	double GetMinimumIncrement(int mip) const
	{
		if (mip < (int) mips.size() - 1)
		{
			return 0.5 / mips[mip+1].partials;
		}
		return -1;
	}

	int GetPartialCount(int mip) const
	{
		//		return mips[mip].maximumIncrement;
		return mips[mip].partials;
	}

	int GetTotalMipMapSize() const
	{
		return totalWaveMemorySize;
	}

	void initialize( const WaveTable* wavetable )
	{
		storeHalfCycles = false;
#ifdef SE_WT_OSC_STORE_HALF_CYCLES // Assume symetrical wave.
		storeHalfCycles = true;
#endif
		initialize(wavetable->waveTableCount, wavetable->waveSize, wavetable->slotCount, storeHalfCycles);
	}

	// No maximum wave size. Full cycles.

	void initializeOsc(int mipCount, int oversampling, int extraInterpolationSamples, int minWaveSz, int maxWaveSz, double spectrumFill = 0.5f)
	{
		waveTableCount = slotCount = 1;
		double partials = 0;
		int ipartialsPrev = -1;

		int MipStartIdx = 0;
		totalWaveMemorySize = 0;
		for (int mip = 0; mip < mipCount; )
		{
			int ipartials = (int)partials;
			if (ipartials != ipartialsPrev)
			{
				int wavesize;
				if (ipartials == 0)
				{
					wavesize = 8;
				}
				else
				{
					int ws = (std::min) (maxWaveSz, ipartials * 2 * oversampling);
					ws = (std::max)(ws, minWaveSz);
					wavesize = 1;
					while (wavesize < ws)
					{
						wavesize = wavesize << 1;
					}
				}

				int SlotStorage = wavesize + extraInterpolationSamples;

				mips.push_back(mipIndex(SlotStorage, wavesize, oversampling, MipStartIdx, ipartials));

				MipStartIdx += SlotStorage * waveTableCount * slotCount;
				totalWaveMemorySize += SlotStorage;
				++mip;
			}

			// next lower octave.
			if (ipartials == 0) // silence.
			{
				partials = 1;
			}
			else
			{
				partials /= spectrumFill;
			}
			ipartialsPrev = ipartials;
		}
	}

	void initialize( int pWaveTableCount, int pWaveSize, int pSlotCount, bool pStoreHalfCycles )
	{
		mips.clear();

		waveTableCount = pWaveTableCount;
		MasterWaveSize = pWaveSize;
		slotCount = pSlotCount;
		storeHalfCycles = pStoreHalfCycles;

		int wavesize = (std::min)( MasterWaveSize, maximumWaveSize );

		int partials = wavesize / 2;
		int octave = 0;
		totalWaveMemorySize = 0;
		int MipStartIdx = 0;
		//_RPT0(_CRT_WARN, "MIP   Sz   Partials\n" );
		while( partials > 0 )
		{
			wavesize = (MasterWaveSize >> octave ) * oversampleRatio;
			wavesize = (std::min)( wavesize, maximumWaveSize );
			wavesize = (std::max)( wavesize, minimumWaveSize );
			int MipOversampleRatio = wavesize / (MasterWaveSize >> octave ); // large waves are not oversampled.
			//_RPT3(_CRT_WARN, "%2d   %4d   %4d\n", octave, wavesize, partials );

			int SlotStorage = wavesize;
			if(storeHalfCycles)
			{
				SlotStorage /= 2;
			}

			mips.push_back(mipIndex(SlotStorage, wavesize, MipOversampleRatio, MipStartIdx));

			MipStartIdx += SlotStorage * waveTableCount * slotCount;
			totalWaveMemorySize += SlotStorage;

			// next higher octave.
			partials = partials >> 1;
			octave++;
		}
		//_RPT0(_CRT_WARN, "--------------------\n" );

/* e.g.
MIP   Sz   Partials
 0    512    256
 1    512    128
 2    256     64
 3    128     32
 4     64     16
 5     32      8
 6     32      4
 7     32      2
 8     32      1
--------------------
*/
	}
	/*
	// With PSOLA window, need exactly twice the storage per wave.
	void doubleSizes()
	{
		totalWaveMemorySize = 0;
		for( int i = 0 ; i < (int) mips.size() ; ++i )
		{
			mips[i].offset = totalWaveMemorySize;
			mips[i].waveSize *= 2;
			totalWaveMemorySize += mips[i].waveSize;
		}
	}
	*/

	// with mips
	int TotalMemoryRequired() const
	{
		return sizeof(WaveTable) + (totalWaveMemorySize * waveTableCount * slotCount - 1) * sizeof(float);
	}

	/*
	// without mips.
	inline static int CalcTotalMemoryRequired(int waveTableCount, int slotCount, int samplesPerSlot )
	{
		return sizeof(WaveTable) + (samplesPerSlot * waveTableCount * slotCount - 1) * sizeof(float);
	}
	*/
	int WaveMemoryRequiredSamples() const
	{
		return sizeof(WaveTable) + (totalWaveMemorySize-1);
	}

	int getSlotOffset( int table, int slot, int mip ) const
	{
		// Mips Grouped by wavetable.
		// return totalWaveMemorySize * (slot + table * slotCount) + mips[mip].offset;

		// Wavetable Grouped by Mips.
		return mips[mip].offset2 + mips[mip].SlotStorage * (slot + table * slotCount);
	}

	int CalcMipLevel(float increment) const
	{
		for( int i = 1 ; i < (int) mips.size() ; ++i )
		{
			if (GetMaximumIncrement(i) < increment)
			{
				return i - 1;
			}
		}
		return (int) mips.size() - 1;
	}
	inline int getSlotCount()
	{
		return slotCount;
	}
};

