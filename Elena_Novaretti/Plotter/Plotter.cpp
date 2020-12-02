


#include "mp_sdk_audio.h"
#include "mp_sdk_gui2.h"

using namespace gmpi;
using namespace gmpi_gui;
using namespace GmpiDrawing;


#include "Plotter.h"

namespace
{
	auto r = gmpi::Register<Plotter>::withId(L"Plotter");
	auto s = gmpi::Register<PlotterGUI>::withId(L"Plotter");
}



// ---------------------------------- DSP PART -----------------------------------

Plotter::Plotter()
{
	initializePin(pinIn);
	initializePin(pinMSec);
	initializePin(pinRng);

	setSubProcess(&Plotter::subProcess);
	setSleep(false);
}

void Plotter::subProcess(int frames)
{
	int s, cnt = sampleCnt, interval = intervalSamples;
	float *strm_in = getBuffer(pinIn);
	float y, invrng = invRange, min = msgToGUI.min, max=msgToGUI.max;
	const float rate = getSampleRate();

	for (s = 0; s < frames; s++)
	{
		y = *(strm_in++) * invrng;

		if (y < min) min = y;
		if (y > max) max = y;

		if (++cnt == interval)
		{
			msgToGUI.min = min;
			msgToGUI.max = max;
			getHost()->sendMessageToGui(0, sizeof(Msg), &msgToGUI);
			min = 1E30;
			max = -1E30;

			cnt = 0;
		}
	}

	msgToGUI.min = min;
	msgToGUI.max = max;
	sampleCnt = cnt;
}

void Plotter::onSetPins()
{
	if (pinMSec.isUpdated())
	{
		float msec = pinMSec;
		if (msec < 10.f) msec = 10.f;
		intervalSamples = (int)(getSampleRate() * msec * .001f);
		sampleCnt = 0;
	}

	if (pinRng.isUpdated())
	{
		invRange = pinRng * .1f;
		if (invRange < 0.00001f) invRange = 0.00001f;
		invRange = 1.f / invRange;
	}

}





// -----------------------------------GUI PART -----------------------------------



PlotterGUI::PlotterGUI()
{
	initializePin(pinHold, static_cast<MpGuiBaseMemberPtr2>(&PlotterGUI::onSetHold));
	initializePin(pinBgARGB1, static_cast<MpGuiBaseMemberPtr2>(&PlotterGUI::onSetColors));
	initializePin(pinBgARGB2, static_cast<MpGuiBaseMemberPtr2>(&PlotterGUI::onSetColors));
	initializePin(pinPlotARGB, static_cast<MpGuiBaseMemberPtr2>(&PlotterGUI::onSetColors));
	initializePin(pinGridARGB, static_cast<MpGuiBaseMemberPtr2>(&PlotterGUI::onSetColors));
}

inline uint32_t PlotterGUI::argb2Native(float a, float r, float g, float b)
{
	const uint32_t ai = (uint32_t)(a*255.f);

	return((ai << 24) |
		(((se_sdk::FastGamma::float_to_sRGB(r) * ai) & 0xFF00) << 8) |
		((se_sdk::FastGamma::float_to_sRGB(g) * ai) & 0xFF00) |
		((se_sdk::FastGamma::float_to_sRGB(b) * ai) >> 8)
		);
}

Color PlotterGUI::HexStringToColor(const std::wstring &s)
{
	constexpr float inv255 = 1.0f / 255.0f;

	wchar_t* stopString;
	uint32_t hex = wcstoul(s.c_str(), &stopString, 16);
	return Color::FromArgb(hex);
}


int32_t PlotterGUI::receiveMessageFromAudio(int32_t id, int32_t size, const void *data)
{
	plotYMin = ((Msg *)data)->min;
	plotYMax = ((Msg *)data)->max;

	updatePlot = true;
	invalidateRect();

	return gmpi::MP_UNHANDLED;
}

void PlotterGUI::onSetHold()
{
	holdFrame = pinHold;
}

void PlotterGUI::onSetColors()
{
	
	if (!pinBgARGB1.getValue().empty()) colorBg1 = HexStringToColor(pinBgARGB1); else colorBg1 = colorBg1Def;
	if (!pinBgARGB2.getValue().empty()) colorBg2 = HexStringToColor(pinBgARGB2); else colorBg2 = colorBg2Def;
	if (!pinPlotARGB.getValue().empty()) colorPlot = HexStringToColor(pinPlotARGB); else colorPlot = colorPlotDef;
	if (!pinGridARGB.getValue().empty()) colorGrid = HexStringToColor(pinGridARGB); else colorGrid = colorGridDef;

	backgroundBM.setNull();
	invalidateRect();
}




int32_t PlotterGUI::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{

	Graphics g(drawingContext);


	// Avoid drawing parts if they have zero alpha

	bool draw_bg = (colorBg1.a > 0) || (colorBg2.a > 0),
	     draw_grid = (colorGrid.a > 0),
		 draw_shape = (colorPlot.a > 0);

	Rect rect = getRect();

	float width = rect.getWidth(), height = rect.getHeight();
	int iwidth = (int)width, iheight = (int)height;


	if ((iwidth != dispWidth) || (iheight != dispHeight))
	{
		dispWidth = iwidth;
		dispHeight = iheight;
		plotBM.setNull();
		backgroundBM.setNull();
	}


	
	// Create and draw background bitmap (bg + grid)

	if (draw_bg||draw_grid)
	{
		if (backgroundBM.isNull())
		{
			auto dc = g.CreateCompatibleRenderTarget({ width, height });

			dc.BeginDraw();
			dc.Clear(Color::FromArgb(0)); // Clear previous gfx or a mess happens !

			// Draw background

			if(draw_bg)
				dc.FillRectangle({ 0,0,width - 1,height - 1 }, g.CreateLinearGradientBrush(colorBg1, colorBg2, { 0,0 }, { 0,height - 1 }));

			// Draw grid

			if (draw_grid)
			{
				float c, y_scrn;

				auto br_grid = g.CreateSolidColorBrush(colorGrid);

				for (c = 0.f; c < 21.f; c++)
				{
					y_scrn = c * 0.05f * height - 1;
					dc.DrawLine({ 0,y_scrn }, { width-1,y_scrn }, br_grid);
				}
			} 			 		  			

			dc.EndDraw();
			backgroundBM = dc.GetBitmap();
		}

		g.DrawBitmap(backgroundBM, { 0,0 }, rect);
	}



	// Create/draw plot

	if (draw_shape)
	{
		if (plotBM.isNull()) // Create plot bitmap
			plotBM = GetGraphicsFactory().CreateImage(iwidth, iheight); // Already erased

		if (updatePlot && (!holdFrame))
		{
			auto lock = plotBM.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_READ | GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE); // These are NOT optional !!!

			if (!lock.isNull())
			{
				uint32_t *pixels = (uint32_t *)lock.getAddress();

				if (pixels)
				{
					const int32_t pfmt = lock.getPixelFormat();

					const uint32_t color_shape = argb2Native(colorPlot.a, colorPlot.r, colorPlot.g, colorPlot.b);
					uint32_t color_aa;
						
					const float hh = height * .5f;
					float ymin_f = hh - plotYMin * hh,
						  ymax_f = hh - plotYMax * hh;

					// Make sure ymin and ymax do not end up outside the visible area !

					if (ymin_f >= height) ymin_f = height - 1.f;
					else if (ymin_f < 0) ymin_f = 0.f;
					if (ymax_f < 0.f) ymax_f = 0.f;
					else if (ymax_f >= height) ymax_f = height - 1.f;

					int ymin = (int)ymin_f, ymax = (int)ymax_f;
					int x, y, row;

					// Scroll bitmap in advance by one pixel leftwise

					// HERE evidently the blurring effect originates (only in the exported AU,
					// neither in Panel nor in Structure view) ... why ??

					for (y = row = 0; y < iheight; y++, row += iwidth)
						for (x = 0; x < iwidth - 1; x++)
							pixels[row + x] = pixels[row + x + 1];
				
					// Draw current plot line - no rendering target here sadly, shall be done manually
					
					for (y = 0, row = iwidth - 1; y < iheight; y++, row += iwidth)
						pixels[row] = ((y < ymax) || (y > ymin)) ? 0 : color_shape;

					// AA the extremities

					color_aa = argb2Native(colorPlot.a * (1.f - ymax_f + (float)ymax), colorPlot.r, colorPlot.g, colorPlot.b);

					if (ymax > 0) pixels[(ymax - 1)*iwidth + iwidth - 1] = color_aa;

					color_aa = argb2Native(colorPlot.a * (ymin_f - (float)ymin), colorPlot.r, colorPlot.g, colorPlot.b);

					if (ymin < iheight - 1) pixels[(ymin + 1)*iwidth + iwidth - 1] = color_aa;
				}
			}
		}

		if(!plotBM.isNull()) g.DrawBitmap(plotBM, { 0,0 }, rect);
	}
	
	updatePlot = false;
	   
	return gmpi::MP_OK;
}




	

