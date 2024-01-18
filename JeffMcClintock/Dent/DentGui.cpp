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

using namespace gmpi;
using namespace GmpiDrawing;

class DentGui final : public gmpi_gui::MpGuiGfxBase
{
 	void onSetCornerRadius()
	{
		// pinCornerRadius changed
	}

 	void onSetTopLeft()
	{
		// pinTopLeft changed
	}

 	void onSetTopRight()
	{
		// pinTopRight changed
	}

 	void onSetBottomLeft()
	{
		// pinBottomLeft changed
	}

 	void onSetBottomRight()
	{
		// pinBottomRight changed
	}

 	void onSetTopColor()
	{
		// pinTopColor changed
	}

 	void onSetBottomColor()
	{
		// pinBottomColor changed
	}

 	FloatGuiPin pinCornerRadius;
 	BoolGuiPin pinTopLeft;
 	BoolGuiPin pinTopRight;
 	BoolGuiPin pinBottomLeft;
 	BoolGuiPin pinBottomRight;
 	IntGuiPin pinBlurRadius;
	FloatGuiPin pinIntensity;
	IntGuiPin pinOffsetX;
	IntGuiPin pinOffsetY;
	BoolGuiPin pinInnerShadow;
	BoolGuiPin pinOuterShadow;
	BoolGuiPin pinVisible;

	void redraw()
	{
		invalidateRect();
	}
public:
	DentGui()
	{
		initializePin( pinCornerRadius, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetCornerRadius) );
		initializePin( pinTopLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopLeft) );
		initializePin( pinTopRight, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopRight) );
		initializePin( pinBottomLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetBottomLeft) );
		initializePin( pinBottomRight, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetBottomRight) );
		initializePin(pinBlurRadius, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopColor));
		initializePin(pinIntensity, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopColor));
		initializePin(pinOffsetX);
		initializePin(pinOffsetY);
		initializePin(pinInnerShadow);
		initializePin(pinOuterShadow);
		initializePin(pinVisible, static_cast<MpGuiBaseMemberPtr2>(&DentGui::redraw));
	}

	// draw a white image on a black background, suitable for blurring
	void drawMask(BitmapRenderTarget& g)
	{
		auto r = getClientRect();

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
	}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		if(!pinVisible)
			return gmpi::MP_OK;

		Graphics g_orig(drawingContext);

		auto r = getRect();
		// Draw the black and white mask.

		// access newer API.
		gmpi_sdk::mp_shared_ptr<GmpiDrawing_API::IMpDeviceContextExt> graphics2;
		if (gmpi::MP_NOSUPPORT == drawingContext->queryInterface(GmpiDrawing_API::IMpDeviceContextExt::guid, graphics2.asIMpUnknownPtr()))
			return MP_FAIL;

		GmpiDrawing::BitmapRenderTarget g_mask;
		graphics2->CreateBitmapRenderTarget(SizeL(r.getWidth(), r.getHeight()), true, (GmpiDrawing_API::IMpBitmapRenderTarget**) g_mask.asIMpUnknownPtr());

		g_mask.BeginDraw();

		const int blurRadius = pinBlurRadius;

		drawMask(g_mask);

		g_mask.EndDraw();

		// blur the mask.
		if(true)
		{
			auto bm = g_mask.GetBitmap();
			auto imageSize = bm.GetSize();
			auto pixelsSource = bm.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_READ | GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
			int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

			int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

			std::vector<float> linearImageOut(imageSize.width * imageSize.height, 0.0f);
			std::vector<float> linearImageOut_temp(linearImageOut.size(), 0.0f);
			std::vector<float> linearImageOut_neg(linearImageOut.size(), 0.0f);
			std::vector<float> linearImageOut_pos(linearImageOut.size(), 0.0f);

			// Copy the image intensity to a linear float format
			for (int y = 0; y < imageSize.height; ++y)
			{
				for (int x = 0; x < imageSize.width; ++x)
				{
					const auto intensity = se_sdk::FastGamma::sRGB_to_float(*sourcePixels++ & 0xff);
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

			const int offsetX = pinOffsetX;
			const int offsetY = pinOffsetY;

			// blur vert
			for (int y = 0; y < imageSize.height; ++y)
			{
				for (int x = 0; x < imageSize.width; ++x)
				{
					float sum{};
					int i{};
					for (int dy = -blurRadius; dy <= blurRadius; ++dy)
					{
						const auto y2 = std::clamp(y + dy - offsetY, 0, (int) imageSize.height - 1);
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

#if 1 // test - just draw the blurred image
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
#if 0
						const auto bits = se_sdk::FastGamma::float_to_sRGB(intensity);

//						*sourcePixels++ = bitsA | (bitsB << 8) /*| (bits << 16)*/ | 0xff000000;
						*sourcePixels++ = bits | (bits << 8) | (bits << 16) | 0xff000000;
#else
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
#endif
					}
				}
			}
		}
#else

			// mask off pixels under the mask itself
			{
				int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

				for (int y = 0; y < imageSize.height; ++y)
				{
					for (int x = 0; x < imageSize.width; ++x)
					{
						const auto intensity = se_sdk::FastGamma::sRGB_to_float(*sourcePixels++ & 0xff);

						linearImageOut[y * imageSize.width + x] *= intensity;
					}
				}
			}

			// overwrite the bitmap with the blurred image.
			{
				const bool subtractive = pinIntensity < 0.0f;
				const float brightness = std::clamp(fabs(pinIntensity.getValue()), 0.0f, 1.0f);

				int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

				for (int y = 0; y < imageSize.height; ++y)
				{
					for (int x = 0; x < imageSize.width; ++x)
					{
						const auto intensity = (std::min)(1.0f, linearImageOut[y * imageSize.width + x]);

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
		}
#endif
		g_orig.DrawBitmap(g_mask.GetBitmap(), Point(0, 0), r);

		return gmpi::MP_OK;
	}

	Rect getClientRect()
	{
		auto r = getRect();
		r.Deflate(1 + pinBlurRadius + (std::max)(abs(pinOffsetX.getValue()), abs(pinOffsetY.getValue())) );

		return r;
	}
};

namespace
{
	auto r = Register<DentGui>::withId(L"SE Dent");
}
