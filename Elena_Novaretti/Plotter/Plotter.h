


typedef struct { float min, max; } Msg;

class Plotter : public MpBase2
{
public:
	Plotter();
	void subProcess(int);
	void onSetPins();
	
private:
	AudioInPin pinIn;
	FloatInPin pinMSec;
	FloatInPin pinRng;

	float invRange = 1.f;

	int intervalSamples = 1000,
		sampleCnt = 0;

	Msg msgToGUI = {1E30f,-1E30f};

};


class PlotterGUI : public gmpi_gui::MpGuiGfxBase
{
public:

	PlotterGUI();
	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext*) override;
	virtual int32_t MP_STDCALL receiveMessageFromAudio(int32_t id, int32_t size, const void *data) override;

protected:
	BoolGuiPin pinHold;
	StringGuiPin pinBgARGB1;
	StringGuiPin pinBgARGB2;
	StringGuiPin pinPlotARGB;
	StringGuiPin pinGridARGB;

	inline uint32_t argb2Native(float a, float r, float g, float b);
	Color HexStringToColor(const std::wstring &s);

	void onSetHold();
	void onSetColors();

	// Work vars

	int dispWidth = 0, dispHeight = 0;
	Bitmap backgroundBM = nullptr,
		   plotBM = nullptr;

	// Default colors

	Color colorBg1Def = Color::FromArgb(0xFFD0E0FF),
		  colorBg2Def = Color::FromArgb(0xFF2060A0),
		  colorPlotDef = Color::FromArgb(0x80000000),
		  colorGridDef = Color::FromArgb(0x40000000);


	Color colorBg1 = colorBg1Def,
		  colorBg2 = colorBg2Def,
		  colorPlot = colorPlotDef,
		  colorGrid = colorGridDef;

	bool holdFrame = false, updatePlot = false;
	float plotYMin = 0.f, plotYMax = 0.f;


};

