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

class WithImageEffects : public gmpi_gui::MpGuiGfxBase
{
protected:
	Bitmap bitmap; // caches the current rendered image.

public:
	FloatGuiPin pinIntensity;
	BoolGuiPin pinVisible;
	BoolGuiPin pinHd;

	// override these
	virtual int32_t renderImage(GmpiDrawing_API::IMpDeviceContext* drawingContext) = 0;
	virtual int32_t filterImage(Bitmap& bitmap) = 0;
	virtual int32_t calcExtraBorderPixels() {return 0;}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		if (!pinVisible)
			return gmpi::MP_OK;

		const auto borderPixels = calcExtraBorderPixels();
		if (!bitmap)
		{
			Graphics g_orig(drawingContext);

			auto r = getRect();

			// access newer API.
			gmpi_sdk::mp_shared_ptr<GmpiDrawing_API::IMpDeviceContextExt> graphics2;
			if (gmpi::MP_NOSUPPORT == drawingContext->queryInterface(GmpiDrawing_API::IMpDeviceContextExt::guid, graphics2.asIMpUnknownPtr()))
				return gmpi::MP_FAIL;

			const float scale = pinHd ? 2.0f : 1.0f;

			SizeL totalSize{
				2 * borderPixels + static_cast<int32_t>(r.getWidth() * scale),
				2 * borderPixels + static_cast<int32_t>(r.getHeight() * scale)
			};

			GmpiDrawing::BitmapRenderTarget g_mask;
			graphics2->CreateBitmapRenderTarget(totalSize, true, (GmpiDrawing_API::IMpBitmapRenderTarget**)g_mask.asIMpUnknownPtr());

			g_mask.BeginDraw();

			const auto sm = g_mask.GetTransform() * Matrix3x2::Scale({ scale, scale }) * Matrix3x2::Translation({ static_cast<float>(borderPixels), static_cast<float>(borderPixels)});
			g_mask.SetTransform(sm);

			renderImage(g_mask.Get());
			g_mask.EndDraw();

			bitmap = g_mask.GetBitmap();
			filterImage(bitmap);
		}
		if (!bitmap)
			return gmpi::MP_FAIL;

		const auto scale = pinHd ? 2.0f : 1.0f;
		const auto bitmapSize = bitmap.GetSizeF();
		Rect sourceRect{};
		sourceRect.right  = bitmapSize.width;
		sourceRect.bottom = bitmapSize.height;

		const float scaledBorder = borderPixels / scale;
		Rect destRect{ getRect() };

		destRect.left -= scaledBorder;
		destRect.top -= scaledBorder;
		destRect.right += scaledBorder;
		destRect.bottom += scaledBorder;

		Graphics g(drawingContext);
		g.DrawBitmap(bitmap, destRect, sourceRect);

		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL arrange(GmpiDrawing_API::MP1_RECT finalRect) override
	{
		const Rect final(finalRect);
		const auto prevRect = getRect();
		if (prevRect.getWidth() != final.getWidth() || prevRect.getHeight() != final.getHeight())
		{
			rerender();
		}
		return gmpi_gui::MpGuiGfxBase::arrange(finalRect);
	}

	int32_t MP_STDCALL getClipArea(GmpiDrawing_API::MP1_RECT* returnRect) override
	{
		auto extraDips = calcExtraBorderPixels() / (pinHd ? 2 : 1);

		auto r = getRect();
		r.Inflate(extraDips);

		*returnRect = r;
		return gmpi::MP_OK;
	}

	void redraw()
	{
		invalidateRect();
	}
	void rerender()
	{
		bitmap.setNull();
		invalidateRect();
	}
};
