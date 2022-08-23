#ifndef DRAWINGTESTGUI_H_INCLUDED
#define DRAWINGTESTGUI_H_INCLUDED

#include "../se_sdk3/mp_sdk_gui2.h"
//#include "../se_sdk3_hosting/MacGuiHost/GraphicsTest/GraphicsClientCodeTest.h"
#ifdef _WIN32
#include "GUI_3_0.h"
#endif
#include "../se_sdk3/TimerManager.h"
#include "LeastShittyText.h"
#include "leegame.h"

class DrawingTestGui : public gmpi_gui::MpGuiGfxBase, public TimerClient
{
	IntGuiPin pinTestType;
	IntGuiPin pinFontface;
	IntGuiPin pinFontsize;
	StringGuiPin pinText;
	BoolGuiPin pinApplyAlphaCorrection;
	FloatGuiPin pinAdjust;
	StringGuiPin pinListItems;
//	TestClient testClient;

	void MyApplyGammaCorrection(GmpiDrawing::Bitmap& bitmap);
	const char* typefaces[6] =
	{
		"Segoe UI",
		"Arial",
		"Courier New",
		"Times New Roman",
		"MS Sans Serif",
		"Verdana",
	};

#ifdef _WIN32
	functionalUI functionalUI;
#endif
	float linearImage[100][100] = {};
	float linearImageBlurred[100][100] = {};

	SmallText smallTextDrawer;

	lees_game gameobject;

public:
	DrawingTestGui();

	//virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override
	//{
	//	testClient.setHost(host);
	//	return gmpi_gui::MpGuiGfxBase::setHost(host);
	//}

	void onSetTestType();

	void refresh();
	void drawGammaTest(GmpiDrawing::Graphics& g);

	void drawAdditiveTest(GmpiDrawing::Graphics& g);

	void drawMacGraphicsTest(GmpiDrawing::Graphics & g);

	// overrides.
	virtual int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;
	void AlphaBlending(GmpiDrawing::Graphics& g);
	void drawGradient(GmpiDrawing::Graphics & g);
	void drawLines(GmpiDrawing::Graphics& g);
	void drawPerceptualColorPicker(GmpiDrawing::Graphics& g);
	void drawGradient2(GmpiDrawing::Graphics& g);
	virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;

	void drawTextVertAlign(GmpiDrawing::Graphics& g);

	void DrawAlignmentCrossHairs(GmpiDrawing::Graphics& g);

	void drawShittyText(GmpiDrawing::Graphics& g);

	void drawTextTestFIXED(GmpiDrawing::Graphics& g, bool useFixedBoundingbox);

	void drawTextTest(GmpiDrawing::Graphics& g);
	void drawSpecificFont(GmpiDrawing::Graphics& g);

	// MP_OK = hit, MP_UNHANDLED/MP_FAIL = miss.
// Default to MP_OK to allow user to select by clicking.
// point will always be within bounding rect.
	int32_t MP_STDCALL hitTest(GmpiDrawing_API::MP1_POINT point) override
	{
		return gmpi::MP_OK;
	}

	bool OnTimer() override;

	int32_t MP_STDCALL onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
};

#endif


