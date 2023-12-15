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
#include "Drawing.h"

using namespace gmpi;
using namespace GmpiDrawing;

class MouseTargetGui final : public gmpi_gui::MpGuiGfxBase
{
	BoolGuiPin pinShow;
	BoolGuiPin pinHover;
	BoolGuiPin pinLeftClick;

public:
	MouseTargetGui()
	{
		initializePin(pinShow);
		initializePin(pinHover);
		initializePin(pinLeftClick);
	}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext ) override
	{
		Graphics g(drawingContext);

		auto brush = g.CreateSolidColorBrush(Color(0.0f,0.0f,1.0f,0.3f));

		if (pinShow)
		{
			g.FillRectangle(getRect(), brush);
		}
		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
	{
		setCapture();
		pinLeftClick = true;
		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
	{
		return hitTest(point);
	}

	int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
	{
		pinLeftClick = false;
		releaseCapture();
		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL setHover(bool isMouseOverMe) override
	{
		pinHover = isMouseOverMe;

		return gmpi::MP_UNHANDLED;
	}
};

namespace
{
	auto r = Register<MouseTargetGui>::withId(L"SE Mouse Target");
}

// Mouse Target to Patch Mem - Bool
class MT2PMB final : public SeGuiInvisibleBase
{
	BoolGuiPin pinToggle;
	BoolGuiPin pinClick;
	BoolGuiPin pinPatchMem;

public:
	MT2PMB()
	{
		initializePin(pinToggle);
		initializePin(pinClick, static_cast<MpGuiBaseMemberPtr2>(&MT2PMB::recalc));
		initializePin(pinPatchMem);
	}

	void recalc()
	{
		if (pinToggle)
		{
			if (pinClick)
			{
				pinPatchMem = !pinPatchMem;
			}
		}
		else
		{
			pinPatchMem = pinClick;
		}
	}
};

namespace
{
	auto r2 = Register<MT2PMB>::withId(L"SE MT2PMB");
}
