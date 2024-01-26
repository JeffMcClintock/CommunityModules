#include "ShadowFilter.h"
#include "../shared/fast_gamma.h"

#define _USE_MATH_DEFINES
#include <math.h>

using namespace se_sdk;
using namespace GmpiDrawing;

int32_t ShadowFilter::calcExtraBorderPixels(int offsetX, int offsetY, int blurRadius, bool hd)
{
	const int maxOffset = (std::max)(abs(offsetX), abs(offsetY));
	blurRadius = blurRadius * (hd ? 2 : 1);
	return maxOffset + blurRadius + 1;
}

template<bool hasOuter, bool hasInner, bool subtractive>
void mergeImages(
	int32_t* sourcePixels,
	std::vector<float>& linearImageOut_neg,
	std::vector<float>& linearImageOut_pos,
	std::vector<float>& linearImageOut, float brightness
)
{
	for (int i = 0; i < linearImageOut.size(); ++i)
	{
		const auto blend = linearImageOut[i];
		float intensity{};
		if constexpr (hasOuter && hasInner)
		{
			const auto intensityA = (std::min)(1.0f, linearImageOut_neg[i]);
			const auto intensityB = (std::min)(1.0f, linearImageOut_pos[i]);
			intensity = (intensityA + (intensityB - intensityA) * blend);
		}
		else if constexpr (hasOuter)
		{
			const auto intensityA = (std::min)(1.0f, linearImageOut_neg[i]);
			intensity = intensityA * (1.0f - blend);
		}
		else if constexpr (hasInner)
		{
			const auto intensityB = (std::min)(1.0f, linearImageOut_pos[i]);
			intensity = intensityB * blend;
		}

		intensity = std::clamp(fabs(intensity * brightness), 0.0f, 1.0f);

		int32_t pixelVal{};
		if constexpr (subtractive) // shadow - subtractive
		{
			// black with varying alpha
			pixelVal = intensity == 0.0f ? 0 : se_sdk::FastGamma::fastNormalisedToPixel(intensity) << 24;
		}
		else // glow - additive
		{
			// varying brightness white with zero alpha
			const auto bits = intensity == 0.0f ? 0 : se_sdk::FastGamma::float_to_sRGB(intensity);
			pixelVal = bits | (bits << 8) | (bits << 16);
		}

		*sourcePixels++ = pixelVal;
	}
}

int32_t ShadowFilter::filterImage(Bitmap& bitmap, float intensity, int offsetX, int offsetY, int blurRadius, bool hasOuterShadow, bool hasInnerShadow, bool hd)
{
	// blur the mask.
	const auto border = calcExtraBorderPixels(offsetX, offsetY, blurRadius, hd);
//	_RPT1(_CRT_WARN, "blurring with border %d\n", border);

	const auto imageSize = bitmap.GetSize();

	blurRadius *= hd ? 2 : 1; // modify *after* we calced the border.

	auto pixelsSource = bitmap.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_READ | GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
	int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

	int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

	std::vector<float> linearImageOut(imageSize.width * imageSize.height, 0.0f);
	std::vector<float> linearImageOut_temp(linearImageOut.size(), 0.0f);
	std::vector<float> linearImageOut_neg(linearImageOut.size(), 0.0f);
	std::vector<float> linearImageOut_pos(linearImageOut.size(), 0.0f);

	// Copy the image intensity to a linear float format
	for (int y = border; y < imageSize.height - border; ++y)
	{
		int32_t* source = sourcePixels + y * imageSize.width + border;
		for (int x = border; x < imageSize.width - border; ++x)
		{
			const auto intensity = 1.0f - se_sdk::FastGamma::sRGB_to_float(*source++ & 0xff);
			linearImageOut[y * imageSize.width + x] = 1.0f - intensity;
		}
	}

	// Calculate the gausian blur filter
	std::vector<float> filter;
	{
		float sum{};
		for (int i = 0; i < blurRadius * 2 + 1; ++i)
		{
			const float dist = abs(blurRadius - i);
			const float f = exp(-dist * dist / (2.0f * blurRadius * blurRadius));
			sum += f;
			filter.push_back(f);
		}
		for (auto& f : filter)
		{
			f /= sum;
		}
	}

	offsetX *= hd ? 2 : 1;
	offsetY *= hd ? 2 : 1;

	if (hasOuterShadow)
	{
		// blur vert
		for (int y = 0; y < imageSize.height; ++y)
		{
			for (int x = 0; x < imageSize.width; ++x)
			{
				float sum{};
				int i{};
				for (int dy = -blurRadius; dy <= blurRadius; ++dy)
				{
					const auto y2 = std::clamp(y + dy - offsetY, 0, (int)imageSize.height - 1);
					sum += linearImageOut[y2 * imageSize.width + x] * filter[i++];
				}

				linearImageOut_temp[y * imageSize.width + x] = sum;
			}
		}

		// blur horizontal. linearImageOut2 -> linearImageOut
		for (int y = 0; y < imageSize.height; ++y)
		{
			for (int x = 0; x < imageSize.width; ++x)
			{
				float sum{};
				int i{};
				for (int dx = -blurRadius; dx <= blurRadius; ++dx)
				{
					const auto x2 = std::clamp(x + dx - offsetX, 0, (int)imageSize.width - 1);
					sum += linearImageOut_temp[y * imageSize.width + x2] * filter[i++];
				}

				linearImageOut_neg[y * imageSize.width + x] = sum;
			}
		}
	}

	if (hasInnerShadow)
	{
		// blur vert, opposite direction
		for (int y = 0; y < imageSize.height; ++y)
		{
			for (int x = 0; x < imageSize.width; ++x)
			{
				float sum{};
				int i{};
				for (int dy = -blurRadius; dy <= blurRadius; ++dy)
				{
					const auto y2 = std::clamp(y + dy + offsetY, 0, (int)imageSize.height - 1);
					sum += linearImageOut[y2 * imageSize.width + x] * filter[i++];
				}

				linearImageOut_temp[y * imageSize.width + x] = sum;
			}
		}

		// blur horizontal. opposit direction
		for (int y = 0; y < imageSize.height; ++y)
		{
			for (int x = 0; x < imageSize.width; ++x)
			{
				float sum{};
				int i{};
				for (int dx = -blurRadius; dx <= blurRadius; ++dx)
				{
					const auto x2 = std::clamp(x + dx + offsetX, 0, (int)imageSize.width - 1);
					sum += linearImageOut_temp[y * imageSize.width + x2] * filter[i++];
				}

				linearImageOut_pos[y * imageSize.width + x] = (1.0f - sum);
			}
		}
	}

	{
		const bool subtractive = intensity < 0.0f;
//		const float brightness = std::clamp(fabs(intensity), 0.0f, 1.0f);
		const float brightness = intensity;
		int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

		if (hasInnerShadow && hasOuterShadow)
		{
			// both
			if (subtractive)
				mergeImages<true, true, true>(sourcePixels, linearImageOut_neg, linearImageOut_pos, linearImageOut, brightness);
			else
				mergeImages<true, true, false>(sourcePixels, linearImageOut_neg, linearImageOut_pos, linearImageOut, brightness);
		}
		else if (hasInnerShadow)
		{
			// only innner
			if (subtractive)
				mergeImages<false, true, true>(sourcePixels, linearImageOut_neg, linearImageOut_pos, linearImageOut, brightness);
			else
				mergeImages<false, true, false>(sourcePixels, linearImageOut_neg, linearImageOut_pos, linearImageOut, brightness);
		}
		else if (hasOuterShadow)
		{
			// only outer
			if (subtractive)
				mergeImages<true, false, true>(sourcePixels, linearImageOut_neg, linearImageOut_pos, linearImageOut, brightness);
			else
				mergeImages<true, false, false>(sourcePixels, linearImageOut_neg, linearImageOut_pos, linearImageOut, brightness);
		}
	}

	return gmpi::MP_OK;
}