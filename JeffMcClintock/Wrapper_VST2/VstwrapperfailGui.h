#ifndef VSTWRAPPERFAILGUI_H_INCLUDED
#define VSTWRAPPERFAILGUI_H_INCLUDED

#include "mp_sdk_gui2.h"

class VstwrapperfailGui : public gmpi_gui::MpGuiGfxBase
{
	std::string errorMsg;
public:
	VstwrapperfailGui(std::string pErrorMsg);

	// overrides.
	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext ) override;
};

#endif


