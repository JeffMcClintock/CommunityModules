#include "EmmissiveFilter.h"
#include "../shared/fast_gamma.h"

#define _USE_MATH_DEFINES
#include <math.h>

using namespace se_sdk;
using namespace GmpiDrawing;

float EmmissiveFilter::filter[KERNAL_SIZE][KERNAL_SIZE] = {};

struct rgba
{
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;
};

struct rgb_f
{
	float b;
	float g;
	float r;
};

rgb_f operator+(const rgb_f& L, const rgb_f& R) { return rgb_f{ L.b + R.b, L.g + R.g, L.r + R.r }; }
rgb_f operator-(const rgb_f& L, const rgb_f& R) { return rgb_f{ L.b - R.b, L.g - R.g, L.r - R.r }; }
rgb_f operator*(const rgb_f& L, float s) { return rgb_f{ L.b * s, L.g * s, L.r * s }; }

inline rgb_f fastSrgb3ToColor(const rgba& pixel)
{
	return rgb_f
	{
		se_sdk::FastGamma::sRGB_to_float(pixel.b),
		se_sdk::FastGamma::sRGB_to_float(pixel.g),
		se_sdk::FastGamma::sRGB_to_float(pixel.r)
	};
}

float perceptialBrightness(rgb_f p)
{
	return 0.2126f * p.r + 0.7152f * p.g + 0.0722f * p.b;
}

float naeiveBrightness(rgb_f p)
{
	constexpr float one_over_3 = 1.0f / 3.0f;

	return (p.r + p.g + p.b) * one_over_3;
}

// with 'over-bright' calculation.
inline rgba fastColorToSrgb3(const rgb_f& graphPixel)
{
	float components[3] = { graphPixel.b, graphPixel.g, graphPixel.r };

	// 'spill' overbright components into other components
	float spill = {};
	for (auto& c : components)
	{
		if (c > 1.0f)
		{
			spill += c - 1.0f;
			c = 1.f;
		}
	}
	if (spill)
	{
		for (auto& c : components)
		{
			c = (std::min)(1.0f, c + spill * 0.5f);
		}
	}

	rgba pixelVal
	{
		se_sdk::FastGamma::float_to_sRGB(components[0]),
		se_sdk::FastGamma::float_to_sRGB(components[1]),
		se_sdk::FastGamma::float_to_sRGB(components[2])
	};

	return pixelVal;
}

void EmmissiveFilter::initFilterKernal()
{
	// TODO: Might need to compensate for DPI on distance calulation.

	// calculate emmision of a single pixel.
	constexpr auto KERNAL_SIZE_inv = 1.0f / (KERNAL_SIZE - 1);
	constexpr float falloffRate = 1.5f; // 1.0 slowish, 1.5 about right. also affects brightness.
	constexpr float edgeSmoothing = 5.f; // 100.0 sharp, 1.0 none

	for (int y = 0; y < KERNAL_SIZE; ++y)
	{
		for (int x = 0; x < KERNAL_SIZE; ++x)
		{
			const float distance = 1.0f + falloffRate * sqrtf((float)(x * x + y * y));
			float intensity = 1.0f / (distance * distance);
			// feather 'corners' from 90% radius to zero at radius
			const float normalizedRadius = distance * KERNAL_SIZE_inv;
			intensity = intensity * std::clamp((1.0f - normalizedRadius) * edgeSmoothing, 0.f, 1.f);
			filter[x][y] = intensity;
		}
	}
}

#if 0
	// convert back to screen color space.
	{
		rgb_f ditherError = {};

		juce::Image::BitmapData bmd_dest(*image, juce::Image::BitmapData::ReadWriteMode::writeOnly);
		auto destPixels = reinterpret_cast<rgba*>(bmd_dest.data);
		for (int i = 0; i < totalPixels; ++i)
		{
            auto& p = destPixels[i];
			auto corrected = pixels_linear[i] + ditherError;
			p = fastColorToSrgb3(corrected);

            // mac can't handle alpha > pixel value. do next best thing.
#ifndef _WIN32
            const auto maxPixel = (std::max)(p.r, (std::max)(p.g, p.b));
            if(maxPixel)
                p.a = maxPixel;
#endif
            
			// only worth dithering dim pixels
			if (naeiveBrightness(corrected) < 0.3f)
			{
				ditherError = corrected - fastSrgb3ToColor(destPixels[i]);
			}
			else
			{
				ditherError = {};
			}

#if 0 //def _DEBUG
			const auto actual = fastSrgb3ToColor(destPixels[i]);
			_RPTN(0, "ideal = [%f, %f, %f]\n", pixels_linear[i].r, pixels_linear[i].g, pixels_linear[i].b);
			_RPTN(0, "actul = [%f, %f, %f]\n", actual.r, actual.g, actual.b);
//			_RPTN(0, "error = [%f, %f, %f]\n", ditherError.r, ditherError.g, ditherError.b);
			_RPTN(0, "error = %f\n", ditherError);
#endif
		}
	}

//	_RPT0(0, "\n");

	return image;
}
#endif

int32_t EmmissiveFilter::filterImage(Bitmap& image, float intensity)
{
	if (filter[0][0] == 0.f)
	{
		initFilterKernal();
	}

	// Apply glow filter.
	const auto width = image.GetSize().width;
	const auto height = image.GetSize().height;
	const auto border = calcExtraBorderPixels();
	const auto totalSourcePixels = (width - 2 * border) * (height - 2 * border);
	const auto totalDestPixels = width * height;

	std::vector<rgb_f> pixels_linear(totalDestPixels, rgb_f{});
	std::vector<rgb_f> pixels_emmisive(totalSourcePixels, rgb_f{});

	auto pixelsSource = image.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_READ | GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
	
	if (pixelsSource.isNull())
		return gmpi::MP_FAIL;

	{
		const auto imageSize = image.GetSize();
		const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

		auto sourcePixels = reinterpret_cast<rgba*>(pixelsSource.getAddress());

		// Convert and copy render to linear brightness buffer.
#if 0 // pixel brightness determins ammount of glow
		constexpr float threshold = 0.5f;
		constexpr float invthreshold = 1.0f / threshold;

		for (int y = border; y < height - border; ++y)
		{
			const int dest_y = y - border;
			int i = y * width + border;
			int j = (y - border) * (width - 2 * border);
			for (int x = border; x < width - border; ++x)
			{
				const int dest_index = y * width + x;
				assert(dest_index < pixels_linear.size());
				{
					const auto r = FastGamma::sRGB_to_float(sourcePixels[i].r);
					const auto g = FastGamma::sRGB_to_float(sourcePixels[i].g);
					const auto b = FastGamma::sRGB_to_float(sourcePixels[i].b);

					// copy only emmissive (bright) pixels to emissive buffer
					pixels_emmisive[j].r = (std::max)(0.f, r - threshold) * invthreshold;	// take brightness
					pixels_emmisive[j].g = (std::max)(0.f, g - threshold) * invthreshold;
					pixels_emmisive[j].b = (std::max)(0.f, b - threshold) * invthreshold;

					pixels_linear[dest_index].r = (std::min)(threshold, r);
					pixels_linear[dest_index].g = (std::min)(threshold, g);
					pixels_linear[dest_index].b = (std::min)(threshold, b);
				}

				++i;
				++j;
			}
		}
#else // all pixels glow, intensity pin determins how much

		for (int y = border; y < height - border; ++y)
		{
			const int dest_y = y - border;
			int i = y * width + border;
			int j = (y - border) * (width - 2 * border);
			for (int x = border; x < width - border; ++x)
			{
				const auto r = FastGamma::sRGB_to_float(sourcePixels[i].r);
				const auto g = FastGamma::sRGB_to_float(sourcePixels[i].g);
				const auto b = FastGamma::sRGB_to_float(sourcePixels[i].b);

				//const int dest_index = y * width + x;
				//pixels_linear[dest_index].r = 0;//r;
				//pixels_linear[dest_index].g = 0;//g;
				//pixels_linear[dest_index].b = 0;//b;

				// pixels to emissive buffer
				pixels_emmisive[j].r = r * intensity;
				pixels_emmisive[j].g = g * intensity;
				pixels_emmisive[j].b = b * intensity;

				++i;
				++j;
			}
		}
#endif
	}

	// convolve emmisive pixels
	const auto sourceWidth = width - 2 * border;
	const auto sourceHeight = height - 2 * border;

	for (int sourceY = 0; sourceY < sourceHeight; ++sourceY)
	{
		for (int sourceX = 0; sourceX < sourceWidth; ++sourceX)
		{
			auto sourcePixel = pixels_emmisive[sourceX + sourceY * sourceWidth];
			
			// skip blank pixels
			if(sourcePixel.r == 0.0f && sourcePixel.g == 0.0f && sourcePixel.b == 0.0f)
				continue;

			for (int off_y = -KERNAL_SIZE + 1; off_y < KERNAL_SIZE; ++off_y)
			{
				for (int off_x = -KERNAL_SIZE + 1; off_x < KERNAL_SIZE; ++off_x)
				{
					const auto x = sourceX + border + off_x;
					const auto y = sourceY + border + off_y;

					assert(x >= 0 && x < width && y >= 0 && y < height);

					const auto& intensity = filter[abs(off_x)][abs(off_y)];

					auto& destPixel = pixels_linear[x + y * width];

					destPixel = destPixel + sourcePixel * intensity;
				}
			}
		}
	}

#ifdef _DEBUG
	// test dots at top-left, bottom-right
	pixels_linear[0] = rgb_f{ 1, 1, 1 };
	pixels_linear.back() = rgb_f{1, 1, 1};
#endif

	// convert back to screen color space. retaining alpha on the 'solid' parts.
	{
		auto destPixels = reinterpret_cast<rgba*>(pixelsSource.getAddress());
		for (int i = 0; i < totalDestPixels; ++i)
		{
//			const auto a = destPixels[i].a;
			destPixels[i] = fastColorToSrgb3(pixels_linear[i]);
//			destPixels[i].a = a;
		}
	}

	return gmpi::MP_OK;
}
