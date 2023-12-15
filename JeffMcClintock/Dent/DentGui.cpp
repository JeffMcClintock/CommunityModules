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
 	IntGuiPin pinDepth;
 	StringGuiPin pinBottomColor;

public:
	DentGui()
	{
		initializePin( pinCornerRadius, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetCornerRadius) );
		initializePin( pinTopLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopLeft) );
		initializePin( pinTopRight, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopRight) );
		initializePin( pinBottomLeft, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetBottomLeft) );
		initializePin( pinBottomRight, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetBottomRight) );
		initializePin(pinDepth, static_cast<MpGuiBaseMemberPtr2>(&DentGui::onSetTopColor) );
	}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		Graphics g(drawingContext);

		auto r = getRect();
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

		auto gradientBrush = g.CreateLinearGradientBrush(gradientStops, point1, point2);

		const auto orig = g.GetTransform();

		for (int x = -1 ; x < 2; ++x)
		{
			for (int i = -5; i < 6; ++i)
			{
				g.SetTransform(orig * Matrix3x2::Translation(x, i));
				g.FillGeometry(geometry, gradientBrush);
			}
		}

		g.SetTransform(orig);

		auto fillBrush = g.CreateSolidColorBrush(Color::FromArgb(0xff555555));
		g.FillGeometry(geometry, fillBrush);

		return gmpi::MP_OK;
	}

};

namespace
{
	auto r = Register<DentGui>::withId(L"SE Dent");
}
