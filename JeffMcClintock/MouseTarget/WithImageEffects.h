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

	virtual int32_t renderImage(GmpiDrawing_API::IMpDeviceContext* drawingContext) = 0;
	virtual int32_t filterImage(Bitmap& bitmap) = 0;

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		if (!pinVisible)
			return gmpi::MP_OK;

		if (!bitmap)
		{
			Graphics g_orig(drawingContext);

			auto r = getRect();

			// access newer API.
			gmpi_sdk::mp_shared_ptr<GmpiDrawing_API::IMpDeviceContextExt> graphics2;
			if (gmpi::MP_NOSUPPORT == drawingContext->queryInterface(GmpiDrawing_API::IMpDeviceContextExt::guid, graphics2.asIMpUnknownPtr()))
				return gmpi::MP_FAIL;

			const float scale = pinHd ? 2.0f : 1.0f;

			GmpiDrawing::BitmapRenderTarget g_mask;
			graphics2->CreateBitmapRenderTarget(SizeL(r.getWidth() * scale, r.getHeight() * scale), true, (GmpiDrawing_API::IMpBitmapRenderTarget**)g_mask.asIMpUnknownPtr());

			g_mask.BeginDraw();

			if (pinHd)
			{
				const auto sm = g_mask.GetTransform() * Matrix3x2::Scale({ scale, scale });
				g_mask.SetTransform(sm);
			}

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
		sourceRect.right = sourceRect.left + bitmapSize.width * scale;
		sourceRect.bottom = sourceRect.top + bitmapSize.height * scale;

		Graphics g(drawingContext);
		g.DrawBitmap(bitmap, getRect(), sourceRect);

		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL arrange(GmpiDrawing_API::MP1_RECT finalRect) override
	{
		rerender();
		return gmpi_gui::MpGuiGfxBase::arrange(finalRect);
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
