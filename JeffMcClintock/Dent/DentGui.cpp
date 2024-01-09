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
 	StringGuiPin pinColor;
	FloatGuiPin pinIntensity;

//	const float blurRadius = 10.0f;

public:
	DentGui()
	{
		initializePin( pinCornerRadius, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetCornerRadius) );
		initializePin( pinTopLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopLeft) );
		initializePin( pinTopRight, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopRight) );
		initializePin( pinBottomLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetBottomLeft) );
		initializePin( pinBottomRight, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetBottomRight) );
		initializePin(pinBlurRadius, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopColor));
		initializePin(pinColor, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopColor));
		initializePin(pinIntensity, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopColor));
	}

	// draw a white image on a black background, suitable for blurring
	void drawMask(BitmapRenderTarget& g)
	{
		auto r = getClientRect();

		int width = r.right - r.left;
		int height = r.bottom - r.top;

		int radius = (int)pinCornerRadius;

		radius = (std::min)(radius, width / 2);
		radius = (std::min)(radius, height / 2);

		auto geometry = g.GetFactory().CreatePathGeometry();
		auto sink = geometry.Open();

		// define a corner 
		const float rightAngle = M_PI * 0.5f;
		// top left
		if (pinTopLeft)
		{
			sink.BeginFigure(Point(0, radius), FigureBegin::Filled);
			ArcSegment as(Point(radius, 0), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.BeginFigure(Point(0, 0), FigureBegin::Filled);
		}
		/*
		// tweak needed for radius of 10
		if(radius == 20)
		{
		Corner.Width += 1;
		Corner.Height += 1;
		width -=1; height -= 1;
		}
		*/
		// top right
		if (pinTopRight)
		{
			sink.AddLine(Point(width - radius, 0));
			//		sink.AddArc(Corner, 270, 90);
			ArcSegment as(Point(width, radius), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.AddLine(Point(width, 0));
		}

		// bottom right
		if (pinBottomRight)
		{
			sink.AddLine(Point(width, height - radius));
			//		sink.AddArc(Corner, 0, 90);
			ArcSegment as(Point(width - radius, height), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.AddLine(Point(width, height));
		}

		// bottom left
		if (pinBottomLeft)
		{
			sink.AddLine(Point(radius, height));
			ArcSegment as(Point(0, height - radius), Size(radius, radius), rightAngle);
			sink.AddArc(as);
		}
		else
		{
			sink.AddLine(Point(0, height));
		}

		// end path
		sink.EndFigure();
		sink.Close();

		Point point1(1, 0);
		Point point2(1, height);

		GradientStop gradientStops[]
		{
			{ 0.0f, Color{0xffffffu, 0.01f} },
			{ 0.49f, Color{0xffffffu, 0.0f} },
			{ 0.5f, Color{0x000000u, 0.0f} },
			{ 1.0f, Color{0x000000u, 0.05f} },
		};

		bool outerBlur = true;

		auto brush = g.CreateSolidColorBrush(outerBlur ? Color::White : Color::Black);

		g.Clear(outerBlur ? Color::Black : Color::White);
		g.FillGeometry(geometry, brush);
	}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
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
		g_mask.SetTransform(Matrix3x2::Translation(blurRadius, blurRadius));

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

//			float error = {}; // dithering

			// Copt the image intensity to a linear float format
			for (int y = 0; y < imageSize.height; ++y)
			{
				for (int x = 0; x < imageSize.width; ++x)
				{
					const auto intensity = pinIntensity * se_sdk::FastGamma::sRGB_to_float(*sourcePixels++ & 0xff);

					if (intensity > 0.0f)
					{
						for (int dy = y - blurRadius; dy < y + blurRadius; ++dy)
						{
							for (int dx = x - blurRadius; dx < x + blurRadius; ++dx)
							{
								if (dx >= 0 && dx < imageSize.width && dy >= 0 && dy < imageSize.height)
								{
									linearImageOut[dy * imageSize.width + dx] += intensity;
								}
							}
						}
					}
				}
			}

			// mask off pixels under the mask itself
			if(0)
			{
				int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

				for (int y = 0; y < imageSize.height; ++y)
				{
					for (int x = 0; x < imageSize.width; ++x)
					{
						const auto intensity = se_sdk::FastGamma::sRGB_to_float(*sourcePixels++ & 0xff);

						if (intensity > 0.0f)
						{
							linearImageOut[y * imageSize.width + x] *= 1.0f - intensity;
						}
//						linearImageOut[y * imageSize.width + x] *= intensity;
					}
				}
			}

			// overwrite the bitmap with the blurred image.
			{
				Color tint = Color::FromHexString(pinColor);
				const bool subtractive = tint.r == 0.0f && tint.g == 0.0f && tint.b == 0.0f;

				int32_t* sourcePixels = (int32_t*)pixelsSource.getAddress();

				for (int y = 0; y < imageSize.height; ++y)
				{
					for (int x = 0; x < imageSize.width; ++x)
					{
						const auto intensity = (std::min)(1.0f, linearImageOut[y * imageSize.width + x]);

						Color c = tint;
						if (subtractive) // shadow - subtractive
						{
							c.a = intensity;
						}
						else // glow - additive
						{
							c.a = 0.0f;// -intensity;
							// pre-multiply
							c.r *= intensity;
							c.g *= intensity;
							c.b *= intensity;
						}

						int32_t pixelVal =
							se_sdk::FastGamma::float_to_sRGB(c.r) |
							(se_sdk::FastGamma::float_to_sRGB(c.g) << 8) |
							(se_sdk::FastGamma::float_to_sRGB(c.b) << 16) |
							(se_sdk::FastGamma::fastNormalisedToPixel(c.a) << 24);

						//int32_t colorVal = pixelVal << 24; // | (pixelVal << 8) | (pixelVal << 16) | 0xff000000;
						*sourcePixels++ = pixelVal;
					}
				}
			}
		}

		g_orig.DrawBitmap(g_mask.GetBitmap(), Point(0, 0), r);

		return gmpi::MP_OK;
	}

	Rect getClientRect()
	{
		auto r = getRect();
		r.Deflate(pinBlurRadius);

		return r;
	}
};

namespace
{
	auto r = Register<DentGui>::withId(L"SE Dent");
}
