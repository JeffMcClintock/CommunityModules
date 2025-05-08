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
#include "cachedBlur.h"


#define _USE_MATH_DEFINES
#include <math.h>
#include "cachedBlur.h"

using namespace gmpi;
using namespace GmpiDrawing;

class FastBlurDemoGui final : public gmpi_gui::MpGuiGfxBase
{
 	IntGuiPin pinBlurRadius;
	StringGuiPin pinColor;

	cachedBlur blur;

	void rerender()
	{
		blur.invalidate();
		invalidateRect(nullptr);
	}

public:
	FastBlurDemoGui()
	{
		initializePin(pinBlurRadius, static_cast<MpGuiBaseMemberPtr2>(&FastBlurDemoGui::rerender) );
		initializePin(pinColor, static_cast<MpGuiBaseMemberPtr2>(&FastBlurDemoGui::rerender) );
	}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		Graphics g(drawingContext);

		auto bounds = getRect();

		// draw the object blurred
		blur.draw(
			g
			, bounds
			, [](Graphics& g)
			{
				auto brush = g.CreateSolidColorBrush(Color::White); // always draw the mask in white. Change the final color via blur.tint
				g.DrawCircle({ 50, 50 }, 40, brush, 5.0f);
			}
		);

		// draw the same object sharp, over the top of the blur.
		auto brush = g.CreateSolidColorBrush(Color::White);
		g.DrawCircle({ 50, 50 }, 40, brush, 5.0f);

		// draw the value as text
		auto textFormat = g.GetFactory().CreateTextFormat(22);
		g.DrawTextU("BLUR", textFormat, bounds, brush);

		return gmpi::MP_OK;
	}
};

namespace
{
	auto r = Register<FastBlurDemoGui>::withId(L"SE FastBlurDemo");
}
