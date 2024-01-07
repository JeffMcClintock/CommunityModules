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

class DentGuiB final : public gmpi_gui::MpGuiGfxBase
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
	DentGuiB()
	{
		initializePin( pinCornerRadius, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetCornerRadius) );
		initializePin( pinTopLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetTopLeft) );
		initializePin( pinTopRight, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetTopRight) );
		initializePin( pinBottomLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetBottomLeft) );
		initializePin( pinBottomRight, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetBottomRight) );
		initializePin(pinBlurRadius, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetTopColor));
		initializePin(pinColor, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetTopColor));
		initializePin(pinIntensity, static_cast<MpGuiBaseMemberPtr2>(&DentGuiB::onSetTopColor));
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
		auto bitmapMem = GetGraphicsFactory().CreateImage(r.getWidth(), r.getHeight());

		{
			auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
			auto imageSize = bitmapMem.GetSize();
			int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

			uint8_t* sourcePixels = pixelsSource.getAddress();

			Point center(r.getWidth() / 2, r.getHeight() / 2);
			float radius = (std::min)(center.x, center.y) * 0.8f;
			float planeDepth = radius * 2.0f;

			constexpr float sqrrt_one_third = 0.57735026918962576450914878050196f;
			const float light_normal[3]{ -sqrrt_one_third, -sqrrt_one_third, sqrrt_one_third };
			const float ambient_intensity = 0.3f;
			const float directional_intensity = 1.0f - ambient_intensity;

			for (float y = 0; y < r.getHeight(); y++)
			{
				float alpha = 1.0f;
				for (float x = 0; x < r.getWidth(); x++)
				{
					float r = sqrt((x - center.x) * (x - center.x) + (y - center.y) * (y - center.y));

					float normal[3];

					float z{};
					if (r > radius)
					{
						z = planeDepth;

						// normal points at camera.
						normal[0] =0.0f;
						normal[1] =0.0f;
						normal[2] =1.0f;
					}
					else
					{
						z = planeDepth - sqrt(radius * radius - r * r);

						normal[0] = (x - center.x) / radius;
						normal[1] = (y - center.y) / radius;
						normal[2] = sqrt(radius * radius - r * r) / radius;
					}

					// compute dot-product of light-normal and surface-normal
					float dot = normal[0] * light_normal[0] + normal[1] * light_normal[1] + normal[2] * light_normal[2];

					// Add some scattering with noise.
					dot += (rand() % 1000) / 1000.0f * 0.05f - 0.025f;

					float intensity = directional_intensity * dot + ambient_intensity;

					auto pixelVal = se_sdk::FastGamma::float_to_sRGB(std::clamp(intensity, 0.0f, 1.0f));
					sourcePixels[0] = pixelVal;
					sourcePixels[1] = pixelVal;
					sourcePixels[2] = pixelVal;
					sourcePixels[3] = 255;

					sourcePixels += 4;
				}
			}
		}

		g_orig.DrawBitmap(bitmapMem, Point(0, 0), r);

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
	auto r = Register<DentGuiB>::withId(L"SE DentB");
}
