#ifndef DRAWINGTESTGUI_H_INCLUDED
#define DRAWINGTESTGUI_H_INCLUDED

#include "../se_sdk3/mp_sdk_gui2.h"
//#include "../se_sdk3_hosting/MacGuiHost/GraphicsTest/GraphicsClientCodeTest.h"

class DrawingTestGui : public gmpi_gui::MpGuiGfxBase
{
	IntGuiPin pinTestType;
	IntGuiPin pinFontface;
	IntGuiPin pinFontsize;
	StringGuiPin pinText;
	BoolGuiPin pinApplyAlphaCorrection;
	FloatGuiPin pinAdjust;

//	TestClient testClient;

	void MyApplyGammaCorrection(GmpiDrawing::Bitmap& bitmap);

public:
	DrawingTestGui();

	//virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override
	//{
	//	testClient.setHost(host);
	//	return gmpi_gui::MpGuiGfxBase::setHost(host);
	//}

	void refresh();
	void drawGammaTest(GmpiDrawing::Graphics& g);

	void drawMacGraphicsTest(GmpiDrawing::Graphics & g);

	// overrides.
	virtual int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;
	void drawGradient(GmpiDrawing::Graphics & g);
	virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;

	// MP_OK = hit, MP_UNHANDLED/MP_FAIL = miss.
// Default to MP_OK to allow user to select by clicking.
// point will always be within bounding rect.
	virtual int32_t MP_STDCALL hitTest(GmpiDrawing_API::MP1_POINT point) override
	{
		return gmpi::MP_OK;
	}
	int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;

};

#endif


