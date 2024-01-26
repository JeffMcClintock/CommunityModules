#pragma once
#include "Drawing.h"

class EmmissiveFilter
{
#ifdef _DEBUG
    static const int KERNAL_SIZE = 16;
#else
    static const int KERNAL_SIZE = 48;
#endif

    static float filter[KERNAL_SIZE][KERNAL_SIZE];
    static void initFilterKernal();

public:
    int32_t calcExtraBorderPixels() { return KERNAL_SIZE; }
    int32_t filterImage(GmpiDrawing::Bitmap& bitmap, float intensity);
};
