#pragma once

#include "mp_sdk_gui2.h"

class AaVstWrapperDiagGui : public gmpi_gui::MpGuiGfxBase
{
public:
	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext ) override;
};