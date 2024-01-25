/* Copyright (c) 2007-2023 SynthEdit Ltd

Permission to use, copy, modify, and /or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include "mp_sdk_gui2.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "../MouseTarget/WithImageEffects.h"

using namespace gmpi;
using namespace GmpiDrawing;

class ShadowGui final : public WithImageEffects
{
 	FloatGuiPin pinCornerRadius;
 	BoolGuiPin pinTopLeft;
 	BoolGuiPin pinTopRight;
 	BoolGuiPin pinBottomLeft;
 	BoolGuiPin pinBottomRight;
 	IntGuiPin pinBlurRadius;
	IntGuiPin pinOffsetX;
	IntGuiPin pinOffsetY;
	BoolGuiPin pinInnerShadow;
	BoolGuiPin pinOuterShadow;

public:
	ShadowGui()
	{
		initializePin( pinCornerRadius, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender) );
		initializePin( pinTopLeft, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender) );
		initializePin( pinTopRight, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender) );
		initializePin( pinBottomLeft, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender) );
		initializePin( pinBottomRight, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender) );
		initializePin(pinBlurRadius, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender));
		initializePin(pinIntensity, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender));
		initializePin(pinOffsetX, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender));
		initializePin(pinOffsetY, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender));
		initializePin(pinInnerShadow, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender));
		initializePin(pinOuterShadow, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender));
		initializePin(pinVisible, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::redraw));
		initializePin(pinHd, static_cast<MpGuiBaseMemberPtr2>(&ShadowGui::rerender));
	}

	// draw a white image on a black background, suitable for blurring
	int32_t renderImage(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		Graphics g(drawingContext);

		auto r = getRect();

		const int radius = (std::min)(pinCornerRadius.getValue(), (std::min)(r.getWidth(), r.getHeight()) / 2);

		auto geometry = g.GetFactory().CreatePathGeometry();
		auto sink = geometry.Open();

		// define a corner 
		const float rightAngle = M_PI * 0.5f;
		// top left
		if (pinTopLeft)
		{
			sink.BeginFigure(r.getTopLeft() + Size(0, radius), FigureBegin::Filled);
			ArcSegment as(r.getTopLeft() + Size(radius, 0), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.BeginFigure(r.getTopLeft(), FigureBegin::Filled);
		}

		// top right
		if (pinTopRight)
		{
			sink.AddLine(r.getTopRight() - Size(radius, 0));
			ArcSegment as(r.getTopRight() + Size(0, radius), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.AddLine(r.getTopRight());
		}

		// bottom right
		if (pinBottomRight)
		{
			sink.AddLine(r.getBottomRight() - Size(0, radius));
			ArcSegment as(r.getBottomRight() - Size(radius, 0), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.AddLine(r.getBottomRight());
		}

		// bottom left
		if (pinBottomLeft)
		{
			sink.AddLine(r.getBottomLeft() + Size(radius, 0));
			ArcSegment as(r.getBottomLeft() - Size(0, radius), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.AddLine(r.getBottomLeft());
		}

		// end path
		sink.EndFigure();
		sink.Close();

		g.Clear(Color::White);

		auto brush = g.CreateSolidColorBrush(Color::Black);
		g.FillGeometry(geometry, brush);

		return gmpi::MP_OK;
	}

	int32_t filterImage(Bitmap& bitmap) override
	{
		const int blurRadius = pinBlurRadius * (pinHd ? 2 : 1);

		// blur the mask.
		const auto border = calcExtraBorderPixels();
		auto imageSize = bitmap.GetSize();
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
				const auto intensity = se_sdk::FastGamma::sRGB_to_float(*source++ & 0xff);
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

		const int offsetX = pinOffsetX * (pinHd ? 2 : 1);
		const int offsetY = pinOffsetY * (pinHd ? 2 : 1);

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

				linearImageOut_pos[y * imageSize.width + x] = sum;
			}
		}

		{
			const bool subtractive = pinIntensity < 0.0f;
			const float brightness = std::clamp(fabs(pinIntensity.getValue()), 0.0f, 1.0f);
			const float innerEnable = pinInnerShadow.getValue() ? 1.0f : 0.0f;
			const float outerEnable = pinOuterShadow.getValue() ? 1.0f : 0.0f;

			int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

			for (int y = 0; y < imageSize.height; ++y)
			{
				for (int x = 0; x < imageSize.width; ++x)
				{
					const auto intensityA = outerEnable * (std::min)(1.0f, linearImageOut_neg[y * imageSize.width + x]);
					const auto intensityB = innerEnable * (1.0f - (std::min)(1.0f, linearImageOut_pos[y * imageSize.width + x]));

					const auto bitsA = se_sdk::FastGamma::float_to_sRGB(intensityA);
					const auto bitsB = se_sdk::FastGamma::float_to_sRGB(intensityB);

					const auto blend = linearImageOut[y * imageSize.width + x];
					const auto intensity = (intensityA + (intensityB - intensityA) * blend);

					int32_t pixelVal{};
					if (subtractive) // shadow - subtractive
					{
						// black with varying alpha
						pixelVal = se_sdk::FastGamma::fastNormalisedToPixel(brightness * intensity) << 24;
					}
					else // glow - additive
					{
						// varying brightness white with zero alpha
						const auto bits = se_sdk::FastGamma::float_to_sRGB(brightness * intensity);
						pixelVal = bits | (bits << 8) | (bits << 16);
					}

					*sourcePixels++ = pixelVal;
				}
			}
		}
		return gmpi::MP_OK;
	}

	int32_t calcExtraBorderPixels() override
	{
		const int maxOffset = (std::max)(abs(pinOffsetX.getValue()), abs(pinOffsetY.getValue()));
		const int blurRadius = pinBlurRadius * (pinHd ? 2 : 1);
		return maxOffset + blurRadius + 1;
	}

	//Rect getClientRect()
	//{
	//	auto r = getRect();

	//	float boarder = 24.f; // 1 + pinBlurRadius + (std::max)(abs(pinOffsetX.getValue()), abs(pinOffsetY.getValue()));
	//	//if(pinHd)
	//	//	boarder *= 2.0f;

	//	r.Deflate(boarder);

	//	return r;
	//}
};

namespace
{
	auto r = Register<ShadowGui>::withId(L"SE Shadow");
}
