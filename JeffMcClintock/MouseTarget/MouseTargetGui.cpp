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
	FloatGuiPin pinDragH;
	FloatGuiPin pinDragV;

	GmpiDrawing::Point prevPoint{};

public:
	MouseTargetGui()
	{
		initializePin(pinShow);
		initializePin(pinHover);
		initializePin(pinLeftClick);
		initializePin(pinDragH);
		initializePin(pinDragV);
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
		prevPoint = point;
		pinLeftClick = true;
		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
	{
		if (getCapture())
		{
			pinDragH = pinDragH + point.x - prevPoint.x;
			pinDragV = pinDragV + prevPoint.y - point.y;
		}
		prevPoint = point;

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
	BoolGuiPin pinPatchMem2;

public:
	MT2PMB()
	{
		initializePin(pinToggle);
		initializePin(pinClick, static_cast<MpGuiBaseMemberPtr2>(&MT2PMB::recalc));
		initializePin(pinPatchMem, static_cast<MpGuiBaseMemberPtr2>(&MT2PMB::passThrough));
		initializePin(pinPatchMem2);
	}

	void recalc()
	{
		if (pinToggle)
		{
			if (pinClick)
			{
//				pinPatchMem = !pinPatchMem;
				pinPatchMem2 = !pinPatchMem;
			}
		}
		else
		{
//			pinPatchMem = pinClick;
			pinPatchMem2 = pinClick;
		}
	}
	void passThrough()
	{
		pinPatchMem2 = pinPatchMem;
	}
};

namespace
{
	auto r2 = Register<MT2PMB>::withId(L"SE MT2PMB");
}


// Mouse Target to Patch Mem - Float
class MT2PMF final : public SeGuiInvisibleBase
{
	FloatGuiPin pinDrag;
	FloatGuiPin pinPatchMem;
	FloatGuiPin pinPatchMem2;
	float lastDrag = 0.0f; // TODO calc per-frame, make pinDrag a delta that needs no mutable memory to recal the prev val.

public:
	MT2PMF()
	{
		initializePin(pinDrag, static_cast<MpGuiBaseMemberPtr2>(&MT2PMF::recalc));
		initializePin(pinPatchMem, static_cast<MpGuiBaseMemberPtr2>(&MT2PMF::passThrough));
		initializePin(pinPatchMem2);
	}

	void recalc()
	{
		const float newNormalized = pinPatchMem + 0.005f * (pinDrag - lastDrag);
		//		pinPatchMem = std::clamp(newNormalized, 0.0f, 1.0f);
		pinPatchMem2 = std::clamp(newNormalized, 0.0f, 1.0f);
		lastDrag = pinDrag;
	}
	void passThrough()
	{
		pinPatchMem2 = pinPatchMem;
	}
};

namespace
{
	auto r4 = Register<MT2PMF>::withId(L"SE MT2PMF");
}

