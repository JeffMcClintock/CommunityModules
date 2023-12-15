#pragma once

#include "../shared/ImageCache.h"
#include "../shared/skinBitmap.h"

class Image4Gui: public gmpi_gui::MpGuiGfxBase, public skinBitmap
{
protected:
	StringGuiPin pinFilename;
	FloatGuiPin pinAnimationPosition;
	IntGuiPin pinFrameCount;

public:
	Image4Gui();

	float getAnimationPos(){
		return pinAnimationPosition;
	}
	void setAnimationPos(float p);
	void onSetFilename();
	void onLoaded();
	void calcDrawAt();

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
	int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;

	// MP_OK = hit, MP_UNHANDLED/MP_FAIL = miss.
	// Default to MP_OK to allow user to select by clicking.
	// point will always be within bounding rect.
	int32_t MP_STDCALL hitTest(GmpiDrawing_API::MP1_POINT point) override
	{
		return skinBitmap::bitmapHitTestLocal(point) ? gmpi::MP_OK : gmpi::MP_UNHANDLED;
	}
};
