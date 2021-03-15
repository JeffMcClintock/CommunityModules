#pragma once
#include "mp_sdk_gui2.h"

class VstwrapperfailGui : public gmpi_gui::MpGuiGfxBase
{
	std::string errorMsg;
public:
	VstwrapperfailGui(std::string pErrorMsg);

	// overrides.
	virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext ) override;
};
