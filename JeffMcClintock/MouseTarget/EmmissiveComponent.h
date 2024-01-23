#pragma once
#include "WithImageEffects.h"

class EmmissiveComponent : public WithImageEffects
{
#ifdef _DEBUG
    static const int KERNAL_SIZE = 16;
#else
    static const int KERNAL_SIZE = 48;
#endif

    static float filter[KERNAL_SIZE][KERNAL_SIZE];
    static void initFilterKernal();

public:
    EmmissiveComponent() {}

    int32_t calcExtraBorderPixels() override { return getBorderSize(); }
    int32_t filterImage(Bitmap& bitmap) override;

    static int getBorderSize()
    {
        return KERNAL_SIZE;
    }
};
