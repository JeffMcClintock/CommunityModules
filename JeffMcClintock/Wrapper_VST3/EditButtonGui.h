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

public:
	// overrides.
	int32_t MP_STDCALL initialize() override;

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
	int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;
	int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;

	GmpiDrawing::TextFormat& getTextFormat();
};


