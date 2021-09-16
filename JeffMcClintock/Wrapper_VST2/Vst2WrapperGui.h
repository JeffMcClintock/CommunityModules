#ifndef TEXTENTRY4GUI_H_INCLUDED
#define TEXTENTRY4GUI_H_INCLUDED

#include "mp_sdk_gui2.h"
#include "../shared/ImageMetadata.h"
//#include "../shared/xplatform.h"
#include "VstFactory.h"
#include "TimerManager.h"
#include "Drawing.h"

class Vst2WrapperGui : public gmpi_gui::MpGuiGfxBase
{
	static const int border = 2;
	GmpiDrawing::TextFormat dtextFormat;

	VstIntPtr shellPluginId_;

	std::vector<float> pinParamValues;
	int paramCount;
	bool initialized_;
	bool hasGuiParameterPins_;
	AEffectWrapper* vstEffect_;
	static const int aeffectPtrPinId = 0;

public:
	Vst2WrapperGui(VstIntPtr shellPluginId, bool hasGuiParameterPins);

	// overrides.
	virtual int32_t MP_STDCALL initialize() override;

	virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
	virtual int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;
	virtual int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	virtual int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;

	virtual int32_t MP_STDCALL populateContextMenu(float x, float y, gmpi::IMpUnknown* contextMenuItemsSink) override;
	virtual int32_t MP_STDCALL onContextMenu(int32_t selection) override;

	virtual int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override;

//	virtual bool OnTimer() override;

	//virtual void hUpdateParam(int index, float value) override
	//{
	//	hUpdateDisplay();
	//}
	//virtual void hUpdateDisplay() override;

	GmpiDrawing::TextFormat& getTextFormat();
	void openVstGui();
};

#endif


