#pragma once

#include "mp_sdk_gui2.h"
#include "Drawing.h"

class EditButtonGui : public gmpi_gui::MpGuiGfxBase
{
	static const int border = 2;
	GmpiDrawing::TextFormat dtextFormat;

	std::string shellPluginId_;
	std::string filename_;

	class ControllerWrapper* controller_ = {};
	static const int controllertPtrPinId = 0;

public:
	// overrides.
	int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override;

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
	int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;
	int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;

	int32_t MP_STDCALL populateContextMenu(float x, float y, gmpi::IMpUnknown* contextMenuItemsSink) override;
	int32_t MP_STDCALL onContextMenu(int32_t selection) override;

	GmpiDrawing::TextFormat& getTextFormat();
};


