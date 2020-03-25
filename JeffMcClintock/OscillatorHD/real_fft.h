/*
#include "real_fft.h"
*/
#ifndef REAL_FFT_H_INCLUDED
#define REAL_FFT_H_INCLUDED

void realft(float data[], unsigned int n, int isign);
void realft2(float data[], unsigned int n, int isign); // normal zero-based indexes.

class SineTables
{
	float Forward1024[26];
	float Reverse1024[26];

	SineTables();
	void InitTable(float* table, int size, int sign );

public:
	static SineTables* Instance();
	float* GetTable( int fftSize, int fftSign );
};

#endif