#pragma once
#include "Drawing.h"

class ShadowFilter
{
public:
    int32_t calcExtraBorderPixels(
        int offsetX,
        int offsetY,
        int blurRadius,
        bool hd
    );

    int32_t filterImage(
        GmpiDrawing::Bitmap& bitmap,
        float intensity,
        int offsetX,
        int offsetY,
        int blurRadius,
        bool hasOuterShadow,
        bool hasInnerShadow,
        bool hd
    );
};
