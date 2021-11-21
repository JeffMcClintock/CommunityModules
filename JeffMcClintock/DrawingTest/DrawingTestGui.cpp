#include "./DrawingTestGui.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <sstream>
extern "C"
{
#include "./colorspace.h"
}

void updateVFD(float linearImage[100][100]);
void updateVFD2(float linearImage[100][100]);
void blurVFD(float linearImageIn[100][100], float linearImageOut[100][100]);
void blurVFD2(float linearImageIn[100][100], float linearImageOut[100][100]);
void blurVFD3(float linearImageIn[100][100], float linearImageOut[100][100]);
void drawVFD(GmpiDrawing::Graphics& g, GmpiDrawing::Rect rect, float linearImageIn[100][100]);


using namespace std;
using namespace gmpi;
using namespace gmpi_gui;
using namespace GmpiDrawing;

GMPI_REGISTER_GUI(MP_SUB_TYPE_GUI2, DrawingTestGui, L"SE Drawing Test" );

DrawingTestGui::DrawingTestGui()
{
	initializePin(pinTestType, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::onSetTestType));
	initializePin(pinListItems);
	initializePin(pinFontface, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinFontsize, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinText, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinApplyAlphaCorrection, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinAdjust, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));

#ifdef _WIN32
    functionalUI.init();
#endif
	StartTimer();
}

template<typename T>
auto parentFolder(T path)
{
	T ret;

	auto p = path.find_last_of('\\');
	if(p == std::string::npos)
	{
		 p = path.find_last_of('/');
	}

	if(p != std::string::npos)
	{
		ret = path.substr(0, p);
	}

	return ret;
}

void DrawingTestGui::onSetTestType()
{
	if (pinTestType.getValue() == 9)
	{
#ifdef _WIN32
		functionalUI.step();
#endif
	}
	refresh();
}

void DrawingTestGui::refresh()
{
	pinListItems =
		"Text (Classic),"
		"Alpha blending,"
//		"Gamma,"
		"Greyscale=3,"			// 3
//		"MacTest,"
		"Text (Fixed)=5,"
		"Text Vert Align,"
		"Additive,"
		"Color Gradients,"
//		"GUI 3.0,"				// 9
		"Lines=10,"
		"Test Font"
		"Textformat2 (boxsize)=13"
		;
	invalidateRect();

#if 0
	// Get the path to this SEM.
	// e.g.
	// "C:\Program Files\Common Files\SynthEdit\modules\DrawingTest.sem" (SynthEdit)
	// "C:\Program Files\Common Files\VST3\Drawing Test\DrawingTest.sem" (VST3)
	const std::wstring mypath = gmpi_dynamic_linking::MP_GetDllFilename();

	std::wstring semFolder = parentFolder(mypath);
#if _WIN32
	std::wstring pluginFolder = semFolder;
#else
	std::wstring pluginFolder = parentFolder(semFolder); // on mac SEMS are in "plugins" sub-folder.
#endif

	// Get the path to the plugins resources.
	std::string aResource = FindResourceU("background", "Image"); // e.g. "C:\Program Files\Common Files\VST3\Drawing Test\PD303__background.png"
	std::string resourceFolder = parentFolder(aResource); // e.g. (VST3) "C:\Program Files\Common Files\VST3\Drawing Test"


	// Getting the Old API
	IMpUserInterfaceHost* legacyHost = {};
	getHost()->queryInterface(MP_IID_UI_HOST, (void**) &legacyHost);

	legacyHost->resolveFilename(L"", 3, L""); // etc
#endif
}

void DrawingTestGui::MyApplyGammaCorrection(Bitmap& bitmap)
{
	auto pixelsSource = bitmap.lockPixels(true);
	auto imageSize = bitmap.GetSize();
	int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

	uint8_t* sourcePixels = pixelsSource.getAddress();
	const float gamma = 2.2f;
	for (int i = 0; i < totalPixels; ++i)
	{
		int alpha = sourcePixels[3];

		if (alpha != 0 && alpha != 255)
		{
			float bitmapAlpha = alpha / 255.0f;

			// Calc pixel lumination (linear).
			float components[3];
			float foreground = 0.0f;
			for (int c = 0; c < 3; ++c)
			{
				float pixel = sourcePixels[c] / 255.0f;
				pixel /= bitmapAlpha; // un-premultiply
				pixel = powf(pixel, gamma);
				components[c] = pixel;
				//						foreground += pixel;
				//						foreground = (std::max)(foreground, pixel);
			}
			//					foreground /= 3.0f; // average pixels.
			//					foreground = 0.2126 * components[2] + 0.7152 * components[1] + 0.0722 * components[0]; // Luminance.
			foreground = 0.3333f * components[2] + 0.3333f * components[1] + 0.3333f * components[0]; // Average. Much the same as Luminance, better on Blue.
/*
			if (pinApplyAlphaCorrection)
			{
//				foreground = (std::min)((std::min)(components[2], components[1]), components[0]);
				foreground = (std::max)((std::max)(components[2], components[1]), components[0]);
			}
*/

			float blackAlpha = 1.0f - powf(1.0f - bitmapAlpha, 1.0 / gamma);
			float whiteAlpha = powf(bitmapAlpha, 1.0f / gamma);

			float mix = powf(foreground, 1.0f / gamma);

			float bitmapAlphaCorrected = blackAlpha * (1.0f - mix) + whiteAlpha * mix;

			for (int c = 0; c < 3; ++c)
			{
				float pixel = components[c];

				// Alpha is calculated on average forground intensity, need to tweak components that are brighter than average to prevent themgetting too dim.
				//float IntensityError = pixel / foreground;
				//pixel *= IntensityError;

				pixel = powf(pixel, 1.0f / gamma); // linear -> sRGB space.
				pixel *= bitmapAlphaCorrected; // premultiply
				sourcePixels[c] = (std::min)(255, (int)(pixel * 255.0f + 0.5f));
			}

			int alphaVal = (int)(bitmapAlphaCorrected * 255.0f + 0.5f);
			sourcePixels[3] = alphaVal;
		}
		sourcePixels += sizeof(uint32_t);
	}
}

void DrawingTestGui::drawGammaTest(GmpiDrawing::Graphics& g)
{
	const int resolution = 1; // 1 or 10
	const float gamma = 2.2f;
	float foregroundColor[3] = { 1, 1, 1 }; // BGR

	// create bitmap with every intensity vs every alpha.
	auto bitmapMem = GetGraphicsFactory().CreateImage(100 + resolution, 100 + resolution);
	{
		auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
		auto imageSize = bitmapMem.GetSize();
		int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

		uint8_t* sourcePixels = pixelsSource.getAddress();

		float x = 0;
		float foreground = 0.0f;

		for (float x = 0 ; x <= 100 ; x += resolution)
		{
			float alpha = 1.0f;
			for (float y = 0; y <= 100; y += resolution)
			{
				int alphaVal = (int)(alpha * 255.0f + 0.5f);

				int pixelVal[3];
				for(int i = 0 ; i < 3; ++i)
				{
					// apply alpha in lin space.
					float fg = foregroundColor[i] * foreground;
					fg *= alpha; // pre-multiply.

					// then convert to SRGB.
					pixelVal[i] = se_sdk::FastGamma::float_to_sRGB(fg);
				}

				// Fill in square with calulated color.
				for (int xi = (int)x; xi < (int)x + resolution; ++xi)
				{
					for (int yi = (int)y; yi < (int)y + resolution; ++yi)
					{
						uint8_t* pixel = sourcePixels + ((int) sizeof(uint32_t) * (xi + yi * (int)(imageSize.width)));

						for (int i = 0; i < 3; ++i)
							pixel[i] = pixelVal[i];

						pixel[3] = alphaVal;
					}
				}

				alpha -= resolution / 100.0f;
			}
			foreground += resolution / 100.0f;
		}
	}

	if (pinApplyAlphaCorrection)
	{
		bitmapMem.ApplyAlphaCorrection();
		//MyApplyGammaCorrection(bitmapMem);
	}

	int x1 = 0;
	int y1 = 0;
	auto brush = g.CreateSolidColorBrush(Color::Transparent());
	// 8 backgrounds. down left, then down right. Black to White.
	for (float background = 0.0f; background < 1.05f; background += 1.0f / 7.0f)
	{
		// Ideal Gamma test rectangle.
		float foreground = 0.0f;
		for (float x = 0; x <= 100; x += resolution)
		{
			float alpha = 1.0f;
			for (float y = 0; y <= 100; y += resolution)
			{
				float gammaCorrectForeground[3];
				for (int i = 0; i < 3; ++i)
				{
					float blend = foregroundColor[i] * foreground * alpha + (1.0f - alpha) * background;
					gammaCorrectForeground[i] = blend; // se_sdk::FastGamma::linearToSrgb(blend);
				}

				brush.SetColor(Color(gammaCorrectForeground[2], gammaCorrectForeground[1], gammaCorrectForeground[0], 1.0f));
				g.FillRectangle(x1 + x, y1 + y, x1 + x + resolution, y1 + y + resolution, brush);

				alpha -= resolution / 100.0f;
			}
			foreground += resolution / 100.0f;
		}

		// Actual blend
		float gammaCorrectBackground = background; // se_sdk::FastGamma::linearToSrgb(background);  // powf(background, 1.0f / gamma);
		brush.SetColor(Color(gammaCorrectBackground, gammaCorrectBackground, gammaCorrectBackground, 1.0f));
		g.FillRectangle(Rect((float)x1 + 115.f, (float)y1, (float)x1 + 215.f + (float)resolution, (float)y1 + 100.f + (float)resolution), brush);
		g.DrawBitmap(bitmapMem, Point(x1 + 115.f, (float)y1), Rect(0.f, 0.f, 100.f + resolution, 100.f + resolution));
		
		y1 += 115;
		if (y1 > 430)
		{
			y1 = 0;
			x1 += 250;
		}
	}
}

void DrawingTestGui::drawAdditiveTest(GmpiDrawing::Graphics& g)
{
	// Using premultiplication to make a purely additive light source.
	// Alpha = 0 (no reduction on background), color fades from center.

	const int resolution = 1; // 1 or 10

	Color forground = Color::White;

	if(!pinText.getValue().empty())
	{
		forground = Color::FromHexString(pinText.getValue());
	}
	const float foregroundColor[3] = { forground.b, forground.g, forground.r }; // BGR

	// create bitmap with every intensity vs every alpha.
	auto bitmapMem = GetGraphicsFactory().CreateImage(100 + resolution, 100 + resolution);
	{
		Point center{0.5f * (100 + resolution), 0.5f * (100 + resolution)};

		auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
		const auto imageSize = bitmapMem.GetSize();
		uint8_t* sourcePixels = pixelsSource.getAddress();

		for (float x = 0 ; x <= 100 ; x += resolution)
		{
			for (float y = 0; y <= 100; y += resolution)
			{
				const auto distanceV = Point(x, y) - center;
				const auto distance = sqrtf(distanceV.width * distanceV.width + distanceV.height * distanceV.height);
				const auto brightness = distance / center.x;
				float alpha = 1.0f - sqrtf(sqrtf((std::max)(0.f,brightness)));

				int alphaVal = (std::max)(0,(std::min)(255, (int)(alpha * 255.0f + 0.5f)));

				int pixelVal[3];
				for(int i = 0 ; i < 3; ++i)
				{
					// apply alpha in lin space.
					float fg = foregroundColor[i];
					fg *= alpha; // pre-multiply.

					// then convert to SRGB.
					pixelVal[i] = se_sdk::FastGamma::float_to_sRGB(fg);
				}

				// Fill in square with calculated color.
				for (int xi = (int)x; xi < (int)x + resolution; ++xi)
				{
					for (int yi = (int)y; yi < (int)y + resolution; ++yi)
					{
						uint8_t* pixel = sourcePixels + ((int) sizeof(uint32_t) * (xi + yi * (int)(imageSize.width)));

						for (int i = 0; i < 3; ++i)
							pixel[i] = pixelVal[i];

						pixel[3] = (std::max)(0, (int)(y - 50.0f));
					}
				}
			}
		}
	}

	g.DrawBitmap(bitmapMem, Point(0.f, 0.f), Rect(0.f, 0.f, bitmapMem.GetSizeF().width, bitmapMem.GetSizeF().height));
}

void DrawingTestGui::drawMacGraphicsTest(GmpiDrawing::Graphics& g)
{
//	testClient.OnRender(g.Get());
}

void RGB2LAB(uint8_t R, uint8_t G, uint8_t B, float *l, float *a, float *b) {
    float RGB[3], XYZ[3];

    RGB[0] = R * 0.003922f;
    RGB[1] = G * 0.003922f;
    RGB[2] = B * 0.003922f;

    RGB[0] = (RGB[0] > 0.04045f) ? powf(((RGB[0] + 0.055f)/1.055f), 2.4f) : RGB[0] / 12.92f;
    RGB[1] = (RGB[1] > 0.04045f) ? powf(((RGB[1] + 0.055f)/1.055f), 2.4f) : RGB[1] / 12.92f;
    RGB[2] = (RGB[2] > 0.04045f) ? powf(((RGB[2] + 0.055f)/1.055f), 2.4f) : RGB[2] / 12.92f;

    XYZ[0] = 0.412424f  * RGB[0] + 0.357579f * RGB[1] + 0.180464f  * RGB[2];
    XYZ[1] = 0.212656f  * RGB[0] + 0.715158f * RGB[1] + 0.0721856f * RGB[2];
    XYZ[2] = 0.0193324f * RGB[0] + 0.119193f * RGB[1] + 0.950444f  * RGB[2];

    *l = 116 * ( ( XYZ[1] / 1.000000f) > 0.008856f ? powf(XYZ[1] / 1.000000f, 0.333333f) : 7.787f * XYZ[1] / 1.000000f + 0.137931f) - 16;
    *a = 500 * ( ((XYZ[0] / 0.950467f) > 0.008856f ? powf(XYZ[0] / 0.950467f, 0.333333f) : 7.787f * XYZ[0] / 0.950467f + 0.137931f) - ((XYZ[1] / 1.000000f) > 0.008856f ? powf(XYZ[1] / 1.000000f, 0.333333f) : 7.787f * XYZ[1] / 1.000000f + 0.137931f) );
    *b = 200 * ( ((XYZ[1] / 1.000000f) > 0.008856f ? powf(XYZ[1] / 1.000000f, 0.333333f) : 7.787f * XYZ[1] / 1.000000f + 0.137931f) - ((XYZ[2] / 1.088969f) > 0.008856f ? powf(XYZ[2] / 1.088969f, 0.333333f) : 7.787f * XYZ[2] / 1.088969f + 0.137931f) );
}

void LAB2RGB(float L, float A, float B, uint8_t *r, uint8_t *g, uint8_t *b) {
    float XYZ[3], RGB[3];

    XYZ[1] = (L + 16 ) / 116;
    XYZ[0] = A / 500 + XYZ[1];
    XYZ[2] = XYZ[1] - B / 200;

    XYZ[1] = (XYZ[1]*XYZ[1]*XYZ[1] > 0.008856f) ? XYZ[1]*XYZ[1]*XYZ[1] : (XYZ[1] - (16.0f / 116)) / 7.787f;
    XYZ[0] = (XYZ[0]*XYZ[0]*XYZ[0] > 0.008856f) ? XYZ[0]*XYZ[0]*XYZ[0] : (XYZ[0] - (16.0f / 116)) / 7.787f;
    XYZ[2] = (XYZ[2]*XYZ[2]*XYZ[2] > 0.008856f) ? XYZ[2]*XYZ[2]*XYZ[2] : (XYZ[2] - (16.0f / 116)) / 7.787f;

    RGB[0] = 0.950467f * XYZ[0] *  3.2406f + 1.000000f * XYZ[1] * -1.5372f + 1.088969f * XYZ[2] * -0.4986f;
    RGB[1] = 0.950467f * XYZ[0] * -0.9689f + 1.000000f * XYZ[1] *  1.8758f + 1.088969f * XYZ[2] *  0.0415f;
    RGB[2] = 0.950467f * XYZ[0] *  0.0557f + 1.000000f * XYZ[1] * -0.2040f + 1.088969f * XYZ[2] *  1.0570f;

    *r = static_cast<uint8_t>(255 * ( (RGB[0] > 0.0031308f) ? 1.055f * (pow(RGB[0], (1/2.4f)) - 0.055f) : RGB[0] * 12.92f ));
    *g = static_cast<uint8_t>(255 * ( (RGB[1] > 0.0031308f) ? 1.055f * (pow(RGB[1], (1/2.4f)) - 0.055f) : RGB[1] * 12.92f ));
    *b = static_cast<uint8_t>(255 * ( (RGB[2] > 0.0031308f) ? 1.055f * (pow(RGB[2], (1/2.4f)) - 0.055f) : RGB[2] * 12.92f ));
}

void lab2rgb(float L, float A, float B, uint8_t *pr, uint8_t *pg, uint8_t *pb)
{
	auto y = (L + 16) / 116;
	auto x = A / 500 + y;
	auto z = y - B / 200;

	x = 0.95047f * ((x * x * x > 0.008856f) ? x * x * x : (x - 16.0f / 116.0f) / 7.787f);
	y = 1.00000f * ((y * y * y > 0.008856f) ? y * y * y : (y - 16.0f / 116.0f) / 7.787f);
	z = 1.08883f * ((z * z * z > 0.008856f) ? z * z * z : (z - 16.0f / 116.0f) / 7.787f);

	auto r = x * 3.2406 + y * -1.5372 + z * -0.4986;
	auto g = x * -0.9689 + y * 1.8758 + z * 0.0415;
	auto b = x * 0.0557 + y * -0.2040 + z * 1.0570;

	r = (r > 0.0031308) ? (1.055 * pow(r, 1.0 / 2.4) - 0.055) : 12.92 * r;
	g = (g > 0.0031308) ? (1.055 * pow(g, 1.0 / 2.4) - 0.055) : 12.92 * g;
	b = (b > 0.0031308) ? (1.055 * pow(b, 1.0 / 2.4) - 0.055) : 12.92 * b;

	*pr = static_cast<uint8_t>(max(0.0, min(1.0, r)) * 255.0);
	*pg = static_cast<uint8_t>(max(0.0, min(1.0, g)) * 255.0);
	*pb = static_cast<uint8_t>(max(0.0, min(1.0, b)) * 255.0);
}

auto lab2rgb_lin(float L, float A, float B)
{
	auto y = (L + 16) / 116;
	auto x = A / 500 + y;
	auto z = y - B / 200;

	x = 0.95047f * ((x * x * x > 0.008856f) ? x * x * x : (x - 16.0f / 116.0f) / 7.787f);
	y = 1.00000f * ((y * y * y > 0.008856f) ? y * y * y : (y - 16.0f / 116.0f) / 7.787f);
	z = 1.08883f * ((z * z * z > 0.008856f) ? z * z * z : (z - 16.0f / 116.0f) / 7.787f);

	auto r = x * 3.2406f + y * -1.5372f + z * -0.4986f;
	auto g = x * -0.9689f + y * 1.8758f + z * 0.0415f;
	auto b = x * 0.0557f + y * -0.2040f + z * 1.0570f;

	return Color(r, g, b);
}

/* might be handy, check for integer->float screw-ups like 1/3
function rgb2lab(rgb){
  var r = rgb[0] / 255,
      g = rgb[1] / 255,
      b = rgb[2] / 255,
      x, y, z;

  r = (r > 0.04045) ? Math.pow((r + 0.055) / 1.055, 2.4) : r / 12.92;
  g = (g > 0.04045) ? Math.pow((g + 0.055) / 1.055, 2.4) : g / 12.92;
  b = (b > 0.04045) ? Math.pow((b + 0.055) / 1.055, 2.4) : b / 12.92;

  x = (r * 0.4124 + g * 0.3576 + b * 0.1805) / 0.95047;
  y = (r * 0.2126 + g * 0.7152 + b * 0.0722) / 1.00000;
  z = (r * 0.0193 + g * 0.1192 + b * 0.9505) / 1.08883;

  x = (x > 0.008856) ? Math.pow(x, 1/3) : (7.787 * x) + 16/116;
  y = (y > 0.008856) ? Math.pow(y, 1/3) : (7.787 * y) + 16/116;
  z = (z > 0.008856) ? Math.pow(z, 1/3) : (7.787 * z) + 16/116;

  return [(116 * y) - 16, 500 * (x - y), 200 * (y - z)]
}
*/

void DrawingTestGui::brushTransparency(GmpiDrawing::Graphics& g)
{
	auto textFormat = g.GetFactory().CreateTextFormat();
	textFormat.SetImprovedVerticalBaselineSnapping();
	auto textBrush = g.CreateSolidColorBrush(Color::Lime);

	// background checkerboard.
	{
		auto brush = g.CreateSolidColorBrush(Color::Black);
		g.FillRectangle(8, 8, 256 + 8, 256 + 8, brush);

		brush.SetColor(Color::White);

		for(int x = 8; x < 256 + 8; x += 8)
		{
			for(int y = 8; y < 256 + 8; y += 8)
			{
				if((y & 8) == (x & 8))
				{
					g.FillRectangle(x, y, x + 8, y + 8, brush);
				}
			}
		}
	}

	float x1 = 8;
	float y1 = 16;
	const int count = 256;
	const int height = 32;
	// create bitmap with every transparency.
	auto bitmapMem = GetGraphicsFactory().CreateImage(count, height);
	{
		auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
		const auto imageSize = bitmapMem.GetSize();
		uint8_t* sourcePixels = pixelsSource.getAddress();

		for(int x = 0; x < count; ++x)
		{
			const auto pixelAlpha = x;
			const float alphaf = x / (float)(count-1);

			for(int y = 0; y < height; ++y)
			{
				const float intensity = static_cast<float>(y >= height/2);
				const float premultiplied = intensity * alphaf;
				const auto pixelValRgba = se_sdk::FastGamma::float_to_sRGB(premultiplied);

				uint8_t* pixel = sourcePixels + sizeof(uint32_t) * (x + y * static_cast<size_t>(imageSize.width));

				for(int i = 0; i < 3; ++i)
				{
					pixel[i] = pixelValRgba;
				}
				pixel[3] = pixelAlpha;
			}
		}
	}

	g.DrawBitmap(bitmapMem, Point(x1, y1), Rect(0, 0, count, height), GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
	g.DrawTextU("Bitmap", textFormat, x1, y1, textBrush);
	// outline
	g.DrawRectangle({x1, y1, x1+count, y1+height}, textBrush);

	// Use brushes to draw every sRGB intensity.
	x1 = 8;
	y1 = 64;
	auto brush = g.CreateSolidColorBrush(Color::Transparent());

	for(int x = 0; x < count; ++x)
	{
		const float alpha = x / (float)(count - 1);
		for(int y = 0; y < height; ++y)
		{
			const float brightness = y < height / 2 ? 0.0f : 1.0f;
			brush.SetColor(Color(brightness, brightness, brightness, alpha));
			g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
		}
	}
	g.DrawTextU("Brush", textFormat, x1, y1, textBrush);
	// outline
	g.DrawRectangle({x1, y1, x1+count, y1+height}, textBrush);
}

void DrawingTestGui::drawGradient(GmpiDrawing::Graphics& g)
{
	auto textFormat = g.GetFactory().CreateTextFormat();
	textFormat.SetImprovedVerticalBaselineSnapping();

	auto textBrush = g.CreateSolidColorBrush(Color::Orange);

	const int resolution = 1; // 1 or 10
	const float gamma = 2.2f;
	float foregroundColor[3] = { 1, 1, 1 }; // BGR

	const int count = 256;
	const int height = 40;
	const int rightCol = 280;

	// create bitmap with every sRGB intensity.
	auto bitmapMem = GetGraphicsFactory().CreateImage(count, count);
	{
		auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
		auto imageSize = bitmapMem.GetSize();
		int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

		uint8_t* sourcePixels = pixelsSource.getAddress();

		float x = 0;
		float foreground = 0.0f;

		for (int x = 0; x < count; ++x)
		{
			for (int y = 0; y < height; ++y)
			{
				int pixelVal[3];
				for (int i = 0; i < 3; ++i)
				{
					pixelVal[i] = x;
				}

				uint8_t* pixel = sourcePixels + ((int) sizeof(uint32_t) * (x + y * (int)(imageSize.width)));

				for (int i = 0; i < 3; ++i)
					pixel[i] = pixelVal[i];

				pixel[3] = 0xff;
			}
		}
	}

	g.DrawBitmap(bitmapMem, Point(0, 0), Rect(0, 0, count, height));
	g.DrawBitmap(bitmapMem, Rect(rightCol, 0, rightCol + count, height), Rect(0, 0, count/2, height)); // 'dark' half stretched x2 on right

	g.DrawTextU("sRGB (Bitmap)", textFormat, 0.0f, 0.0f, textBrush);

	// Use brushes to draw every sRGB intensity.
	float x1 = 0;
	float y1 = 50;
	auto brush = g.CreateSolidColorBrush(Color::Transparent());

	// full width on left
	for (int x = 0; x < count; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			brush.SetColor(Color::FromBytes(x,x,x));
			g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
		}
	}
	g.DrawTextU("sRGB (Brush)", textFormat, x1, y1, textBrush);
	// dark half on right
	x1 = rightCol;
	for (int x = 0; x < count; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			const int byteVal = x / 2;
			brush.SetColor(Color::FromBytes(byteVal, byteVal, byteVal));
			g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
		}
	}

	y1 += 50;
	x1 = 0;
	{
		Rect r(0, 0, count, height);
		r.Offset(x1, y1);

		for (float x = 0; x < count; ++x)
		{
			uint8_t sRGB[3];

			lab2rgb(100.0f * x / 255.0f, 0.0f, 0.0f, &sRGB[0], &sRGB[1], &sRGB[2]);

			brush.SetColor(Color::FromBytes(sRGB[0], sRGB[1], sRGB[2]));
			for (float y = 0; y < height; ++y)
			{
				g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
			}
		}

		g.DrawTextU("LAB  (brush)", textFormat, x1, y1, textBrush);
	}

	// dark-half
	x1 = rightCol;
	{
		Rect r(0, 0, count, height);
		r.Offset(x1, y1);

		for (float x = 0; x < count; ++x)
		{
			uint8_t sRGB[3];

			lab2rgb(50.0f * x / 255.0f, 0.0f, 0.0f, &sRGB[0], &sRGB[1], &sRGB[2]);

			brush.SetColor(Color::FromBytes(sRGB[0], sRGB[1], sRGB[2]));
			for (float y = 0; y < height; ++y)
			{
				g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
			}
		}

//		g.DrawTextU("LAB  (brush)", textFormat, x1, y1, textBrush);
	}

	//////////////////////////////////////////////////////////////////////
	x1 = 0;
	y1 += 50;
	{
		Rect r(0, 0, count, height);
		r.Offset(x1, y1);

		for (float y = 0; y < height; ++y)
		{
			GmpiDrawing_API::MP1_POINT p1 = { 0.0f, 0.0f };
			GmpiDrawing_API::MP1_POINT p2 = { static_cast<float>(count), 0.0f };

			std::vector<GradientStop> stops;
			const int numStops = 8;// (std::max)(2, (int)y / 4);
			for(int i = 0; i < numStops; ++i)
			{
				const float L = 100.0f * i / (float)(numStops - 1);
				stops.push_back({i / (float)(numStops - 1), lab2rgb_lin(L, 0.0f, 0.0f)});
			}

			auto gbrush = g.CreateLinearGradientBrush(g.CreateGradientStopCollection(stops), p1, p2);

			g.FillRectangle(x1, y1 + y, x1 + count, y1 + y + 1, gbrush);
		}

		g.DrawTextU("LAB (Piecewise Gradient)", textFormat, x1, y1, textBrush);
	}

	x1 = rightCol;
	{
		Rect r(0, 0, count, height);
		r.Offset(x1, y1);

		for (float y = 0; y < height; ++y)
		{
			GmpiDrawing_API::MP1_POINT p1 = { x1, 0.0f };
			GmpiDrawing_API::MP1_POINT p2 = { x1 + static_cast<float>(count), 0.0f };

			std::vector<GradientStop> stops;
			const int numStops = 8;
			for (int i = 0; i < numStops; ++i)
			{
				const float distance = i / (float)(numStops - 1);
				const float L = 50.0f * distance;
				stops.push_back({ distance, lab2rgb_lin(L, 0.0f, 0.0f) });
			}

			auto gbrush = g.CreateLinearGradientBrush(g.CreateGradientStopCollection(stops), p1, p2);

			g.FillRectangle(x1, y1 + y, x1 + count, y1 + y + 1, gbrush);
		}

//		g.DrawTextU("LAB (Piecewise Brush)", textFormat, x1, y1, textBrush);
	}

	//////////////////////////////////////////////////////////////////////

	x1 = 0;
	y1 += 50;

	for (float x = 0; x < count; ++x)
	{
		brush.SetColor(Color(x / 256.0f, x / 256.0f, x / 256.0f));
		for (float y = 0; y < height; ++y)
		{
			g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
		}
	}
	g.DrawTextU("Brush Linear", textFormat, x1, y1, textBrush);

	x1 = rightCol;
	for (float x = 0; x < count; ++x)
	{
		const float intensity = x / 512.0f;
		brush.SetColor(Color(intensity, intensity, intensity));
		for (float y = 0; y < height; ++y)
		{
			g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
		}
	}

	//////////////////////////////////////////////////////////////////////
	x1 = 0;
	y1 += 50;
	{
		Rect r(0, 0, count, height);
		r.Offset(x1, y1);

		auto brushFill = g.CreateLinearGradientBrush(Color::Black, Color::White, { 0.0f, 0.0f }, { static_cast<float>(count), 0.0f });
		g.FillRectangle(r, brushFill);
		g.DrawTextU("GradientBrush", textFormat, x1, y1, textBrush);
	}
	x1 = rightCol;
	{
		Rect r(0, 0, count, height);
		r.Offset(x1, y1);

		const float intensity = 0.5f;
		auto brushFill = g.CreateLinearGradientBrush(Color::Black, Color(intensity, intensity, intensity), { x1, 0.0f }, { x1 + static_cast<float>(count), 0.0f });
		g.FillRectangle(r, brushFill);
//		g.DrawTextU("GradientBrush", textFormat, x1, y1, textBrush);
	}

	//////////////////////////////////////////////////////////////////////
	x1 = 0;
	y1 += 50;
	{
		float err = 0.0f;
		for (float x = 0; x < count; x += 0.5f)
		{
			for (float y = 0; y < height; y += 0.5f)
			{
				const float ideal = x / 255.0f;
				err += ideal;

				float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

				if(err >= r)
				{
					brush.SetColor(Color::White);
					err -= 1.0f;
				}
				else
				{
					brush.SetColor(Color::Black);
				}
				g.FillRectangle(x1 + x, y1 + y, x1 + x + 0.5f, y1 + y + 0.5f, brush);
			}
		}
		g.DrawTextU("Dither", textFormat, x1, y1, textBrush);
	}

	x1 = rightCol;
	{
		float err = 0.0f;
		for (float x = 0; x < count; x += 0.5f)
		{
			for (float y = 0; y < height; y += 0.5f)
			{
				const float ideal = 0.5f * x / 255.0f;
				err += ideal;

				float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

				if (err >= r)
				{
					brush.SetColor(Color::White);
					err -= 1.0f;
				}
				else
				{
					brush.SetColor(Color(0.f, 0.f, 0.f)); // test mac Mini drawing grey
				}
				g.FillRectangle(x1 + x, y1 + y, x1 + x + 0.5f, y1 + y + 0.5f, brush);
			}
		}
//		g.DrawTextU("Dither", textFormat, x1, y1, textBrush);
	}
}

void DrawingTestGui::drawLines(GmpiDrawing::Graphics& g)
{
	Point p1(10.0, 100.0);
	Point p2(100.0, 100.0);

	gmpi_sdk::MpString fullUri;
	getHost()->RegisterResourceUri("background", "Image", &fullUri);
	auto bitmap = g.GetFactory().LoadImageU(fullUri.c_str());
	
	Brush brushes[] =
	{
		g.CreateSolidColorBrush(Color::Orange),
		g.CreateLinearGradientBrush(Color::Red, Color::Lime, p2, p1),
		g.CreateBitmapBrush(bitmap),
	};

	const float widths[] =
	{
		0.5f,
		1.0f,
		4.0f,
		10.0f,
	};

	GmpiDrawing::StrokeStyle strokeStyles[] =
	{
		g.GetFactory().CreateStrokeStyle(CapStyle::Flat),
		g.GetFactory().CreateStrokeStyle(CapStyle::Round),
		g.GetFactory().CreateStrokeStyle(CapStyle::Square),
		g.GetFactory().CreateStrokeStyle(CapStyle::Triangle),
	};

	int brush = 0;
	int width = 0;
	int style = 0;
	for (float y = 10.0f; y < 110.0f; y += widths[width] + 4.0f)
	{
		g.DrawLine({ 10.0, y }, { 100.0, y }, brushes[brush], widths[width], strokeStyles[style]);

		if (++brush == std::size(brushes))
		{
			brush = 0;
			if (++width == std::size(widths))
			{
				width = 0;
			}
		}
		if (++style == std::size(strokeStyles))
		{
			style = 0;
		}
	}

	// dashed lines
	auto blackBrush = g.CreateSolidColorBrush(Color::Black);

	GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES dashedLineProps{};
	dashedLineProps.miterLimit = 1.0f;
	dashedLineProps.dashStyle = GmpiDrawing_API::MP1_DASH_STYLE_CUSTOM;

	float dashes[] = { 1.0f, 2.0f, 3.0f, 4.0f };
	GmpiDrawing::StrokeStyle strokeStyles2[] =
	{
		// custom dashed line
		g.GetFactory().CreateStrokeStyle(
			dashedLineProps, dashes, static_cast<int32_t>(std::size(dashes))
		),

		g.GetFactory().CreateStrokeStyle(
			{
				GmpiDrawing_API::MP1_CAP_STYLE_FLAT,	// start
				GmpiDrawing_API::MP1_CAP_STYLE_FLAT,	// end
				GmpiDrawing_API::MP1_CAP_STYLE_FLAT,	// cap

				GmpiDrawing_API::MP1_LINE_JOIN_MITER,
				1.0f,									// mitre limit
				GmpiDrawing_API::MP1_DASH_STYLE_DASH,
				0.0f,									// dash offset
				GmpiDrawing_API::MP1_STROKE_TRANSFORM_TYPE_NORMAL
			}
			, 0, 0
		),

		g.GetFactory().CreateStrokeStyle(
			{
				GmpiDrawing_API::MP1_CAP_STYLE_FLAT,	// start
				GmpiDrawing_API::MP1_CAP_STYLE_FLAT,	// end
				GmpiDrawing_API::MP1_CAP_STYLE_ROUND,	// cap

				GmpiDrawing_API::MP1_LINE_JOIN_MITER,
				1.0f,									// mitre limit
				GmpiDrawing_API::MP1_DASH_STYLE_DOT,
				0.0f,									// dash offset
				GmpiDrawing_API::MP1_STROKE_TRANSFORM_TYPE_NORMAL
			}
			, 0, 0
		),

		g.GetFactory().CreateStrokeStyle(
			{
				GmpiDrawing_API::MP1_CAP_STYLE_FLAT,	// start
				GmpiDrawing_API::MP1_CAP_STYLE_FLAT,	// end
				GmpiDrawing_API::MP1_CAP_STYLE_TRIANGLE,	// cap

				GmpiDrawing_API::MP1_LINE_JOIN_MITER,
				1.0f,									// mitre limit
				GmpiDrawing_API::MP1_DASH_STYLE_DASH_DOT,
				0.0f,									// dash offset
				GmpiDrawing_API::MP1_STROKE_TRANSFORM_TYPE_NORMAL
			}
			, 0, 0
		),
		g.GetFactory().CreateStrokeStyle(
			{
				GmpiDrawing_API::MP1_CAP_STYLE_SQUARE,	// start
				GmpiDrawing_API::MP1_CAP_STYLE_SQUARE,	// end
				GmpiDrawing_API::MP1_CAP_STYLE_SQUARE,	// cap

				GmpiDrawing_API::MP1_LINE_JOIN_MITER,
				1.0f,									// mitre limit
				GmpiDrawing_API::MP1_DASH_STYLE_DASH_DOT_DOT,
				0.0f,									// dash offset
				GmpiDrawing_API::MP1_STROKE_TRANSFORM_TYPE_NORMAL
			}
			, 0, 0
		),

	};

	float y = 120.0f;
	for (auto& strokeStyle : strokeStyles2)
	{
		g.DrawLine({ 10.0, y }, { 100.0, y }, blackBrush, 2.0f, strokeStyle);
		y += 4.0f;
	}
}

void DrawingTestGui::drawPerceptualColorPicker(GmpiDrawing::Graphics& g)
{
	// create bitmap with every intensity vs every alpha.
	int resolution = 1;
	auto bitmapMem = g.GetFactory().CreateImage(100 + resolution, 100 + resolution);
	{
		auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
		auto imageSize = bitmapMem.GetSize();
		int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

		uint8_t* sourcePixels = pixelsSource.getAddress();

		float x = 0;
		float foreground = 0.0f;

		for (float x = 0; x <= 100; x += resolution)
		{
			float alpha = 1.0f;
			for (float y = 0; y <= 100; y += resolution)
			{
				// make an CIELUV CIELCh color
				const auto L = pinAdjust.getValue();// 80;								// 0 - > 100
				const auto C = 200 - 2.f * y;// y * 2.f;				// 0 -> 200
				const auto h = x * 360.f / 100.f;	// 0 -> 360

				// convert to CIELUV
//					auto L = L;
				const auto h2 = h * M_PI / 180;
				const auto U = cos(h2) * C;
				const auto V = sin(h2) * C;

				double rx, ry, rz;
				Luv2Xyz(&rx, &ry, &rz, L, U, V);

				// XYZ to sRGB
				double r, g, b;
				Xyz2Rgb(&r, &g, &b, rx, ry, rz);

				uint8_t sRGB[3] = {};

				if (r > 1.f
					|| g > 1.f
					|| b > 1.f)
				{
					sRGB[0] = sRGB[1] = sRGB[2] = 0;
				}
				else
				{
					sRGB[2] = static_cast<uint8_t>(max(0.0, min(1.0, r)) * 255.0);
					sRGB[1] = static_cast<uint8_t>(max(0.0, min(1.0, g)) * 255.0);
					sRGB[0] = static_cast<uint8_t>(max(0.0, min(1.0, b)) * 255.0);
				}

#if 0
				// convert to LAB
				// https://github.com/berendeanicolae/ColorSpace/blob/master/src/Conversion.cpp
				{
					const auto h2 = h * M_PI / 180;

//					auto l = l;
					const auto A = cos(h2) * C;
					const auto B = sin(h2) * C;
					// convert to x y z
					// http://www.easyrgb.com/en/math.php
					{
						auto CIE_L = L;
						auto CIE_u = L;
						auto CIE_v = L;

						auto var_Y = (CIE_L + 16) / 116;
						if (var_Y ^ 3 > 0.008856)
							var_Y = var_Y ^ 3;
						else
							var_Y = (var_Y - 16 / 116) / 7.787;

						auto ref_U = (4 * Reference_X) / (Reference_X + (15 * Reference_Y) + (3 * Reference_Z));
						auto ref_V = (9 * Reference_Y) / (Reference_X + (15 * Reference_Y) + (3 * Reference_Z));

						auto var_U = CIE_u / (13 * CIE_L) + ref_U;
						auto var_V = CIE_v / (13 * CIE_L) + ref_V;

						auto Y = var_Y * 100;
						auto X = -(9 * Y * var_U) / ((var_U - 4) * var_V - var_U * var_V);
						auto Z = (9 * Y - (15 * var_V * Y) - (var_V * X)) / (3 * var_V);
					}
					// convert to sRGB
//					lab2rgb(l, a, b, &sRGB[0], &sRGB[1], &sRGB[2]);
					{
						// xyz
						auto y = (L + 16) / 116;
						auto x = A / 500 + y;
						auto z = y - B / 200;

						x = 0.95047f * ((x * x * x > 0.008856f) ? x * x * x : (x - 16.0f / 116.0f) / 7.787f);
						y = 1.00000f * ((y * y * y > 0.008856f) ? y * y * y : (y - 16.0f / 116.0f) / 7.787f);
						z = 1.08883f * ((z * z * z > 0.008856f) ? z * z * z : (z - 16.0f / 116.0f) / 7.787f);

						// linear RGB
						auto r = x * 3.2406 + y * -1.5372 + z * -0.4986;
						auto g = x * -0.9689 + y * 1.8758 + z * 0.0415;
						auto b = x * 0.0557 + y * -0.2040 + z * 1.0570;

						// sRGB
						r = (r > 0.0031308) ? (1.055 * pow(r, 1.0 / 2.4) - 0.055) : 12.92 * r;
						g = (g > 0.0031308) ? (1.055 * pow(g, 1.0 / 2.4) - 0.055) : 12.92 * g;
						b = (b > 0.0031308) ? (1.055 * pow(b, 1.0 / 2.4) - 0.055) : 12.92 * b;

						if (r > 1.f
							|| g > 1.f
							|| b > 1.f)
						{
							sRGB[0] = sRGB[1] = sRGB[2] = 0;
						}
						else
						{
							sRGB[0] = static_cast<uint8_t>(max(0.0, min(1.0, r)) * 255.0);
							sRGB[1] = static_cast<uint8_t>(max(0.0, min(1.0, g)) * 255.0);
							sRGB[2] = static_cast<uint8_t>(max(0.0, min(1.0, b)) * 255.0);
						}
					}
				}
#endif

				int alphaVal = (int)(alpha * 255.0f + 0.5f);

				// Fill in square with calulated color.
				for (int xi = (int)x; xi < (int)x + resolution; ++xi)
				{
					for (int yi = (int)y; yi < (int)y + resolution; ++yi)
					{
						uint8_t* pixel = sourcePixels + ((int)sizeof(uint32_t) * (xi + yi * (int)(imageSize.width)));

						for (int i = 0; i < 3; ++i)
							pixel[i] = sRGB[i];// pixelVal[i];

//						pixel[3] = alphaVal;
						pixel[3] = 0xff;
					}
				}

				alpha -= resolution / 100.0f;
			}
			foreground += resolution / 100.0f;
		}
	}

	g.DrawBitmap(bitmapMem, { 10, 10 }, Rect(0.f, 0.f, 100.f + resolution, 100.f + resolution));

}

void DrawingTestGui::drawGradient2(GmpiDrawing::Graphics& g)
{
	const auto clipRect = g.GetAxisAlignedClip();
//	_RPT4(_CRT_WARN, "clipRect[ %f %f %f %f]\n", clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);

	auto textFormat = g.GetFactory().CreateTextFormat();
	textFormat.SetImprovedVerticalBaselineSnapping();

	auto textBrush = g.CreateSolidColorBrush(Color::Orange);

	const int resolution = 1; // 1 or 10
	const float gamma = 2.2f;
	float foregroundColor[3] = { 1, 1, 1 }; // BGR

	const auto rect = getRect();
	const int width = 64;
	const int height = 64;

	// Use brushes to draw every sRGB intensity.
	float x1 = 12;
	float y1 = 12;
	auto brush = g.CreateSolidColorBrush(Color::Transparent());

	// Linear gradients
	for(int i = 0 ; i < 8 ; ++i)
	{
		Rect r(0, 0, width, height);
		r.Offset(x1, y1);

		Point p1, p2;
		switch(i)
		{
		case 0:
			p1 = r.getTopLeft();
			p2 = r.getTopRight();
			break;
		case 1:
			p1 = r.getTopRight();
			p2 = r.getBottomRight();
			break;
		case 2:
			p1 = r.getBottomRight();
			p2 = r.getBottomLeft();
			break;
		case 3:
			p1 = r.getBottomLeft();
			p2 = r.getTopLeft();
			break;
		case 4:
			p1 = r.getTopLeft();
			p2 = r.getBottomRight();
			break;
		case 5:
			p1 = r.getTopRight();
			p2 = r.getBottomLeft();
			break;
		case 6:
			p1 = r.getBottomRight();
			p2 = r.getTopLeft();
			break;
		case 7:
			p1 = r.getBottomLeft();
			p2 = r.getTopRight();
			break;
		}

		auto brushFill = g.CreateLinearGradientBrush(Color::Red, Color::Lime, p1, p2);
		g.FillRectangle(r, brushFill);

		x1 += width + 12;

		if(x1 + width >= rect.getWidth())
		{
			x1 = 12;
			y1 += height + 12;
		}
	}

	// newline
	x1 = 12;
	y1 += height + 12;

	// linear gradient with off-end behaviour
	{
		Point p1(x1 + width + 12 + width/3, y1);
		Point p2(p1.x + width/3, y1);

		auto brushFill = g.CreateLinearGradientBrush(Color::LightBlue, Color::White, p1, p2);

		for(int i = 0; i < 3; ++i)
		{
			Rect r(0, 0, width, height);
			r.Offset(x1, y1);

			RoundedRect roundRect(r, 5.0f, 5.0f);
//			g.FillRectangle(r, brushFill);
			g.FillRoundedRectangle(roundRect, brushFill);

			x1 += width + 12;
		}
	}

	// radial gradients.
	
	// newline
	x1 = 12;
	y1 += height + 12;

	for(int i = 0 ;i < 5; ++i)
	{
		Rect r(0, 0, width, height);
		r.Offset(x1, y1);
		//auto brushFill = g.CreateLinearGradientBrush(Color::Red, Color::Lime, p1, p2);
		//g.FillRectangle(r, brushFill);

		RadialGradientBrushProperties props{
			{200.0, 200.0}, // center
			{0.0, 0.0},		// gradientOriginOffset
			200.0f,			// radiusX
			200.0f			// radiusY
		};

		switch(i)
		{
		case 0:
			props.center = r.getTopLeft();
			props.radiusX = props.radiusY = 10.0f;
			break;

		case 1:
			props.center.x = (r.left + r.right) * 0.5f;
			props.center.y = r.top;
			props.radiusX = props.radiusY = 20.0f;
			break;

		case 2:
			props.center.x = (r.left + r.right) * 0.5f;
			props.center.y = (r.top + r.bottom) * 0.5f;
			props.radiusX = props.radiusY = 30.0f;
			break;

			// circular, offset center
		case 3:
			props.center.x = (r.left + r.right) * 0.5f;
			props.center.y = (r.top + r.bottom) * 0.5f;
			props.radiusX = props.radiusY = 30.0f;
			props.gradientOriginOffset = Point(10.0f, 10.0f);
			break;

			// circular, offset center outside circle
		case 4:
			props.center.x = (r.left + r.right) * 0.5f;
			props.center.y = (r.top + r.bottom) * 0.5f;
			props.radiusX = props.radiusY = 30.0f;
			props.gradientOriginOffset = Point(width / 2, width / 2);
			break;

		};

		GradientStop gradientStops[] = {
			{0.0f, Color::Blue   },
			{1.0f, Color::Orange }
		};

		auto gradientStopCollection = g.CreateGradientStopCollection(gradientStops);

		auto brushFill = g.CreateRadialGradientBrush({ props }, {}, gradientStopCollection);
		if(!brushFill.isNull())
		{
			g.FillRectangle(r, brushFill);
		}
		else
		{
//			g.FillRectangle(r, fallbackBrush); // falback to plain solid color brush.
		}


		x1 += width + 12;

		if(x1 + width >= rect.getWidth())
		{
			x1 = 12;
			y1 += height + 12;
		}

	}
}

/*
#include <d2d1.h>

int test()
{
	ID2D1RenderTarget* pRT;

	GmpiDrawing_API::IMpDeviceContext* pDC;
	GmpiDrawing::Graphics g;


	// DRAW A RECTANGLE

	// Direct-2D
	{
		D2D1::ColorF black(D2D1::ColorF::Black);

		ID2D1SolidColorBrush* pBlackBrush;

		pRT->CreateSolidColorBrush(
			black,
			&pBlackBrush);

		pRT->DrawRectangle(
			D2D1::RectF(0, 0, 10, 10),
			pBlackBrush);
	}

	// GMPI Drawing API
	{
		GmpiDrawing::Color black(GmpiDrawing::Color::Black);

		GmpiDrawing_API::IMpSolidColorBrush* pBlackBrush;

		pDC->CreateSolidColorBrush(
			&black,
			&pBlackBrush);

		GmpiDrawing_API::MP1_RECT rc{ 0, 0, 10, 10 };
		pDC->DrawRectangle(
			&rc,
			pBlackBrush);
	}

	// GMPI Drawing (wrappers)
	{
		GmpiDrawing::Color black(GmpiDrawing::Color::Black);

		auto BlackBrush = g.CreateSolidColorBrush(black);

		g.DrawRectangle(
			GmpiDrawing::Rect(0, 0, 10, 10),
			BlackBrush);
	}

}
*/

int32_t DrawingTestGui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	GmpiDrawing::Graphics g(drawingContext);

	switch(pinTestType.getValue())
	{
	case 0:
		drawTextTest(g);
		break;
	case 1:
		brushTransparency(g);
		break;
	case 2:
		drawGammaTest(g);
		break;
	case 3:
		drawGradient(g);
		break;
	case 4:
		drawMacGraphicsTest(g);
		break;
	case 5:
		drawTextTestFIXED(g, false);
		break;
	case 6:
	{
		drawTextVertAlign(g);
		auto bmRenderTarget = g.CreateCompatibleRenderTarget(Size(60, 60));
		bmRenderTarget.BeginDraw();
		bmRenderTarget.Clear(Color::Aquamarine);
		auto bitmap = bmRenderTarget.GetBitmap();
		bmRenderTarget.EndDraw();

		auto pixels = bitmap.lockPixels();
	}
		break;
	case 7:
		drawAdditiveTest(g);
		break;

	case 8:
		drawGradient2(g);
		break;

	case 9:
#ifdef _WIN32
		functionalUI.draw(g);
#endif
		break;

	case 10:
		drawLines(g);
		break;

	case 11:
		drawSpecificFont(g);
		break;

	case 12:
		drawPerceptualColorPicker(g);
		break;

	case 13:
//		drawVFD(g, getRect(), linearImageBlurred);
		drawTextTestFIXED(g, true);
		break;
	}

	return MP_OK;
}

bool DrawingTestGui::OnTimer()
{
	switch (pinTestType.getValue())
	{
	case 9:
#ifdef _WIN32
		functionalUI.step();
#endif
		refresh();
		break;

	case 13:
		/*
		updateVFD(linearImage);
//		updateVFD2(linearImage); // 1 pixel per source dot
		blurVFD2(linearImage, linearImageBlurred);
//		blurVFD3(linearImage, linearImageBlurred);
		refresh();
		*/
		break;
	}

	return true;
}

int32_t MP_STDCALL DrawingTestGui::onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
#ifdef _WIN32
	functionalUI.mousePosition = point;
#endif
	return MP_OK;
}

int32_t MP_STDCALL DrawingTestGui::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
#ifdef _WIN32
	functionalUI.mouseDown = 1.0f;
#endif
    setCapture();
	return MP_OK;
}

int32_t MP_STDCALL DrawingTestGui::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
#ifdef _WIN32
	functionalUI.mouseDown = 0.0f;

#endif
    releaseCapture();
	return MP_OK;
}


// EEEEs
void DrawingTestGui::drawTextVertAlign(GmpiDrawing::Graphics& g)
{
	auto factory = g.GetFactory();

	const auto str = "E";
//	const auto fontFace = "Courier New";
	const auto fontFace = "Arial";
	//const auto fontFace = "Times New Roman";
	const std::vector <std::string> fontStack = {fontFace};

	Rect textRect;
	const float noBlur = 0.5f;
	const float lineWidth = 0.5f;
	const bool snapBaseline = true;
	auto brush = g.CreateSolidColorBrush(Color::White);
	auto r = getRect();
	g.FillRectangle(r, brush);

	float starty = 20.f;

	for(float bodyHeight = 6.0f; bodyHeight < 24.0f; bodyHeight += 0.5)
	{
		float x = 32.f;
		float y = starty;
		float yinc = 0.01f;

		auto textFormat = factory.CreateTextFormat2(bodyHeight, fontStack);

		Size textSize = textFormat.GetTextExtentU(str);
		GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
		textFormat.GetFontMetrics(&fontMetrics);

		for(int i = 0; i < 100; ++i)
		{
			textRect.top = y;
			textRect.left = x;
			textRect.bottom = textRect.top + textSize.height;
			textRect.right = ceilf(textRect.left + textSize.width);
			const float lineRight = textRect.right + 2;
			const float lineLeft = textRect.left - 2;

			brush.SetColor(Color::LightGray);
			g.DrawLine(Point(textRect.left + 1, y), Point(textRect.left + 3, y), brush, 0.5);

			brush.SetColor(Color::Black);

			Rect snappedRect = textRect;
			if((i % 10) == 9)
			{
				g.DrawTextU("L", textFormat, snappedRect, brush, DrawTextOptions::NoSnap);
			}
			else
			{
				g.DrawTextU(str, textFormat, snappedRect, brush, DrawTextOptions::NoSnap);
			}

			if(true)
			{
				float predictedBaseLine = textRect.top + fontMetrics.ascent;
				
				// snap to pixel.
				const float offsetMin = -0.25f;
//				const float offsetMax = -0.125f;
				const float offsetMax = -0.25f;
//				const float offsetMax = -0.0f;
				const float offset = bodyHeight < 10.0f ? offsetMin : offsetMax;

				predictedBaseLine += offset;

				const float scale = 0.5f;
				predictedBaseLine = floorf(predictedBaseLine / scale) * scale;

				brush.SetColor(Color::Lime);
				g.DrawLine(Point(textRect.left, predictedBaseLine + 0.25f), Point(textRect.left + 1, predictedBaseLine + 0.25f), brush, 0.5);
			}

			x += 4.f;
			y += yinc;
		}

		starty += floor(bodyHeight * 0.8f) + 2.0f;
	}

	DrawAlignmentCrossHairs(g);
}

void DrawingTestGui::DrawAlignmentCrossHairs(GmpiDrawing::Graphics& g)
{
	auto brush = g.CreateSolidColorBrush(Color::Black);
	auto r = getRect();
	// Alignment cross hairs.
	float crossSize = 6.0f;
	float x1 = 8.5;
	float y1 = 8.5;
	g.DrawLine(x1 - crossSize, y1, x1 + crossSize, y1, brush); // criss
	g.DrawLine(x1, y1 - crossSize, x1, y1 + crossSize, brush); // cross

	x1 = r.right - x1;
	y1 = r.bottom - y1;
	brush.SetColor(Color::Black);
	g.DrawLine(x1 - crossSize, y1, x1 + crossSize, y1, brush); // criss
	g.DrawLine(x1, y1 - crossSize, x1, y1 + crossSize, brush); // cross
}

// Draw using text body height (not point size). Should result in perfect cross-platform comptibility, even with font-fallback to differnt fonts ("Segoe UI").
void DrawingTestGui::drawTextTestFIXED(GmpiDrawing::Graphics& g, bool useFixedBoundingbox)
{
	auto r = getRect();

	// Background Fill.
	auto fallbackBrush = g.CreateSolidColorBrush(Color::White);
/*
	RadialGradientBrushProperties props{
		{200.0, 200.0}, // center
		{0.0, 0.0},		// gradientOriginOffset
		200.0f,			// radiusX
		200.0f			// radiusY
	};

	GradientStop gradientStops[] = {
		{0.0f, Color::White   },
		{1.0f, Color::Black }
	};

	auto gradientStopCollection = g.CreateGradientStopCollection(gradientStops);

	auto brushFill = g.CreateRadialGradientBrush({ props }, {}, gradientStopCollection);
	if(!brushFill.isNull())
	{
		g.FillRectangle(r, brushFill);
	}
	else
	{
		g.FillRectangle(r, fallbackBrush); // falback to plain solid color brush.
	}
	*/
		g.FillRectangle(r, fallbackBrush); // falback to plain solid color brush.

#if 0
//	if (pinTestType == 0)
	{
		const char* typefaces[] = { "Segoe UI", "Arial", "Courier New", "Times New Roman" , "MS Sans Serif" };

		float x = 10.5;
		Rect textRect;
		textRect.left = x;

		textRect.top = 200.5;

		const char* fontFace = typefaces[pinFontface];

		TextFormat dtextFormat = g.GetFactory().CreateTextFormat2((float)pinFontsize.getValue(), fontFace);

		dtextFormat.SetTextAlignment(TextAlignment::Leading); // Left
		dtextFormat.SetParagraphAlignment(ParagraphAlignment::Far);
		dtextFormat.SetWordWrapping(WordWrapping::NoWrap); // prevent word wrapping into two lines that don't fit box.

		string text = fontFace;
		if (!pinText.getValue().empty())
		{
			text = pinText;
		}

		GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
		dtextFormat.GetFontMetrics(&fontMetrics);
//		_RPT1(_CRT_WARN, "fontMetrics.ascent    %f\n", fontMetrics.ascent);
//		_RPT1(_CRT_WARN, "fontMetrics.descent   %f\n", fontMetrics.descent);
//		_RPT1(_CRT_WARN, "fontMetrics.capHeight %f\n", fontMetrics.capHeight);
//		_RPT1(_CRT_WARN, "fontMetrics.xHeight   %f\n", fontMetrics.xHeight);
//		_RPT1(_CRT_WARN, "fontMetrics.lineGap   %f\n", fontMetrics.lineGap);

		auto textSize = dtextFormat.GetTextExtentU(text);
		textRect.bottom = textRect.top + textSize.height;
		textRect.right = textRect.left + textSize.width;

		// Textbox verical size is ascent + descent.
//		textRect.bottom = textRect.top + fontMetrics.descent + fontMetrics.ascent;

		brush.SetColor(Color(0.8f, 0.8f, 0.8f));
		g.FillRectangle(textRect, brush);

		auto baseline = textRect.bottom - fontMetrics.descent; // for bottom-aligned text.
		const float lineWidth = 0.5f;
		const float lineLeft = textRect.left - 2;
		const float lineRight = textRect.right + 2;

		// Ascent (light-blue)
		// Ascent is the distance from the top of font character alignment box to the English baseline.
		brush.SetColor(Color::LightBlue);
		float y = baseline - fontMetrics.ascent;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// cap-height. (green)
		brush.SetColor(Color::Green);
		y = baseline - fontMetrics.capHeight;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// x-height. (blue)
		brush.SetColor(Color::MediumBlue);
		y = baseline - fontMetrics.xHeight;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// Base-line. (orange)
		brush.SetColor(Color::Coral);
		y = baseline;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// Descent/Bottom (light-blue)
		brush.SetColor(Color::AliceBlue);
		y = textRect.bottom;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// Line-gap
		brush.SetColor(Color::Yellow);
		y = textRect.bottom + fontMetrics.lineGap;
		g.DrawLine(Point((textRect.left + textRect.right) * 0.5f, y), Point(textRect.right + 3, y), brush);

		// Text
		brush.SetColor(Color::Black);
		g.DrawTextU(text, dtextFormat, textRect, brush);

		return;
	}
#endif
	{
		// Create font.
		int font_size_ = 12;
		std::string text("Cat");
		float dipFontSize = (font_size_ * 72.f) / 96.f; // Points to DIPs conversion. https://social.msdn.microsoft.com/forums/vstudio/en-US/dfbadc0b-2415-4f92-af91-11c78df435b3/hwndhost-gdi-vs-directwrite-font-size

		 // Default font face.
		TextFormat dtextFormat = useFixedBoundingbox ?
			g.GetFactory().CreateTextFormat2(dipFontSize)
			: g.GetFactory().CreateTextFormat(dipFontSize);

		fallbackBrush.SetColor(Color(0, 0, 0));

		// Paths.
		float y = 20.5;
		float w = 40;
		int i = 0;

		for (int i = 0; i < 9; ++i)
		{
			// Lines draw 'nice' at co-ord x.5
			float x = 0.5f + 45.25f * (float)i;
			float penWidth = 1;

			Rect textRect;
			textRect.bottom = y + w;
			textRect.top = y;
			textRect.left = x;
			textRect.right = x + w;

			auto geometry = g.GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			Point p(x, y);

			sink.BeginFigure(p);

			sink.AddLine(Point(x + w, y));
			sink.AddLine(Point(x + w, y + w));
			sink.AddLine(Point(x, y + w));

			switch (i & 1)
			{
			case 0:
				sink.EndFigure();
				break;
			case 1:
				sink.EndFigure(FigureEnd::Open);
				break;
			}

			sink.Close();

			g.DrawGeometry(geometry, fallbackBrush, penWidth);

			if (i != 0) // first one show default.
			{
				// Text.
				switch (i % 3)
				{
				case 0:
					dtextFormat.SetTextAlignment(TextAlignment::Leading);
					break;
				case 1:
					dtextFormat.SetTextAlignment(TextAlignment::Center);
					break;
				case 2:
					dtextFormat.SetTextAlignment(TextAlignment::Trailing);
					++penWidth;
					break;
				}

				switch ((i / 3) % 3)
				{
				case 0:
					dtextFormat.SetParagraphAlignment(ParagraphAlignment::Near); // Top
					break;
				case 1:
					dtextFormat.SetParagraphAlignment(ParagraphAlignment::Center); // Middle
					break;
				case 2:
					dtextFormat.SetParagraphAlignment(ParagraphAlignment::Far); // Bottom
					break;
				}
			}

			g.DrawTextU(text, dtextFormat, textRect, fallbackBrush);
		}
	}

	// Word Wrapping.
	{
		const char* words[] = { /*"cat", "White Noise",*/ "the quick brown fox jumped over the lazy dog" };

		//TextFormat dtextFormat = g.GetFactory().CreateTextFormat2();
		// Default font face, Default Size.
		TextFormat dtextFormat = useFixedBoundingbox ?
			g.GetFactory().CreateTextFormat2()
			: g.GetFactory().CreateTextFormat();

		dtextFormat.SetParagraphAlignment(ParagraphAlignment::Near); // Top
		dtextFormat.SetTextAlignment(TextAlignment::Leading); // Left

		float x = 10.5;

		for (int col = 0; col < 5; ++col)
		{
			float penWidth = 1.0f;
			float y = 130.5;
			Rect headingRect(x, y - 16, x+100, y);
			Rect textRect;
			textRect.top = y;
			textRect.left = x;
			const float maxWidth = 100.f;
			const char* desc = "";

			int32_t clipOPtion{}; // Use clip OR wrap, but not both. Clip does nothing on wrapped text.
			switch (col)
			{
			case 0: // test defaults
				desc = "Default";
				break;
			case 1:
				desc = "Clip";
				clipOPtion = (int32_t)DrawTextOptions::Clip;
				dtextFormat.SetWordWrapping(WordWrapping::NoWrap);
				break;
			case 2:
				desc = "Wrap";
				clipOPtion = (int32_t)DrawTextOptions::None;
				dtextFormat.SetWordWrapping(WordWrapping::Wrap);
				break;
			case 3:
				desc = "Clip/Wrap";
				clipOPtion = (int32_t)DrawTextOptions::Clip;
				dtextFormat.SetWordWrapping(WordWrapping::Wrap);
				break;
			case 4:
				desc = "No Clip/Wrap";
				clipOPtion = (int32_t)DrawTextOptions::None;
				dtextFormat.SetWordWrapping(WordWrapping::NoWrap);
				break;
			}

			for (auto w : words)
			{
				auto textSize = dtextFormat.GetTextExtentU(w);
				textRect.bottom = textRect.top + ceilf(textSize.height);
				textRect.right = textRect.left + ceilf(textSize.width);

				if (textRect.getWidth() > maxWidth)
				{
					textRect.right = (std::min)(textRect.right, textRect.left + 100.f);
					textRect.bottom += textRect.getHeight() * 3.0f; // Note text metrics do not include linegap, so 2x text height is not enough space for two rows of text.
				}

				fallbackBrush.SetColor(Color(0.0f, 0.0f, 1.0f));
				Rect boxRect(textRect);
				boxRect.Inflate(ceilf(penWidth * 0.5f)); // Box must always end up on whole number plus 0.5 pixel boundaries.

				g.DrawRectangle(boxRect, fallbackBrush, penWidth);
				g.DrawTextU(w, dtextFormat, textRect, fallbackBrush, clipOPtion);

				g.DrawTextU(desc, dtextFormat, headingRect, fallbackBrush); // heading

				textRect.top += 20;
				++penWidth;
			}

			dtextFormat.SetWordWrapping(WordWrapping::Wrap);
			clipOPtion = (int32_t)DrawTextOptions::None;
			const float xstep = 106.0f;
			x += xstep;
			headingRect.Offset(xstep, 0);
		}
	}
	// Fonts.
	{
		auto factory = g.GetFactory();
		// Note: Segoe UI is not available on Mac and gets substituted.
		const char* typefaces[] = { "Segoe UI", "Arial", "Courier New", "Times New Roman" };
		float fontSizes[] = { 8, 9, 10, 11, 12, 13, 14, 18 , 34, 72 };

		float x = 10.0f;
		Rect textRect;
		textRect.left = x;
		const float noBlur = 0.5f;
		const float lineWidth = 0.5f;
		const bool snapBaseline = false;

		for (auto fontFace : typefaces)
		{
			textRect.top = 200.0f;

			for (auto dipFontSize : fontSizes)
			{
				//auto textFormat = factory.CreateTextFormat2(dipFontSize, fontFace);
				TextFormat textFormat = useFixedBoundingbox ?
					g.GetFactory().CreateTextFormat2(dipFontSize, fontFace)
					: g.GetFactory().CreateTextFormat(dipFontSize, fontFace);

				GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
				textFormat.GetFontMetrics(&fontMetrics);

				Size textSize = textFormat.GetTextExtentU(fontFace);

				//if(dipFontSize == 72.0f)
				//{
				//	_RPT3(_CRT_WARN, "%s, BODYHEIGHT %f, BOUNDHEIGHT %f\n", fontFace, fontMetrics.bodyHeight(), textSize.height);
				//}

				textRect.bottom = textRect.top + textSize.height;
				textRect.right = ceilf(textRect.left + textSize.width);

				if (snapBaseline)
				{
					const float baseLine = textRect.top + fontMetrics.ascent;
					const float yOffset = floorf(baseLine + 0.5f) - baseLine;
					textRect.Offset(0.0f, yOffset);
				}

				const float lineRight = textRect.right + 2;
				const float lineLeft = textRect.left - 2;

				fallbackBrush.SetColor(Color::LightGray);
				g.FillRectangle(textRect, fallbackBrush);

				// Metrics measure from TOP of box not bottom.
				const float baseLine = textRect.top + fontMetrics.ascent;

				// Descent
				fallbackBrush.SetColor(Color::LightBlue);
				float y = textRect.top + fontMetrics.ascent + fontMetrics.descent;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), fallbackBrush, lineWidth);

				// Baseline
				fallbackBrush.SetColor(Color::Coral);
				y = baseLine;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), fallbackBrush, lineWidth);

				// x-height.
				fallbackBrush.SetColor(Color::MediumBlue);
				y = baseLine - fontMetrics.xHeight;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), fallbackBrush, lineWidth);

				// cap-height.
				fallbackBrush.SetColor(Color::Green);
				y = baseLine - fontMetrics.capHeight;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), fallbackBrush, lineWidth);

				// Ascent. (should be same as top, unless in legacy mode).
				fallbackBrush.SetColor(Color::Azure);
				y = baseLine - fontMetrics.ascent;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), fallbackBrush, lineWidth);

				fallbackBrush.SetColor(Color::Black);

				// bracket left end of box
				g.DrawLine(Point(textRect.left, textRect.top), Point(textRect.left, textRect.bottom), fallbackBrush, lineWidth);
				g.DrawLine(Point(textRect.left, textRect.top), Point(textRect.left + 20, textRect.top), fallbackBrush, lineWidth);
				g.DrawLine(Point(textRect.left, textRect.bottom), Point(textRect.left + 20, textRect.bottom), fallbackBrush, lineWidth);

				g.DrawTextU(fontFace, textFormat, textRect, fallbackBrush);

				textRect.top += ceilf(dipFontSize * 1.4f);

				if ((fontFace[0] == 'T' || fontFace[0] == 'C') && dipFontSize == 9 )
				{
					//auto textFormatSmall = g.GetFactory().CreateTextFormat2(8);
					TextFormat textFormatSmall = useFixedBoundingbox ?
						g.GetFactory().CreateTextFormat2(8)
						: g.GetFactory().CreateTextFormat(8);

					Rect textRect2 (0,0,1000,20);
					if (fontFace[0] == 'C')
					{
						textRect2.Offset(0, 9);
					}

					const char* metricNames[] = {
						"asc",
						"des",
						"lineGap",
						"capHeight",
						"xHeight",
						"ulPos",
						"ulThc",
						"stPos",
						"stThc" };

					float* metric = (float*)&fontMetrics;

					std::stringstream s;
					s << std::string(fontFace) << " " << dipFontSize << "px: ";

					for (int i = 0; i < 6; ++i)
					{
						s << metricNames[i] << " "  << *metric++ << ", ";
					}
			
//					s << "topgap: " << textSize.height - (fontMetrics.ascent + fontMetrics.descent);
					s << "box: (" << textRect.getWidth() << ", " << textRect.getHeight() <<")";

					g.DrawTextU(s.str(), textFormatSmall, textRect2, fallbackBrush);
				}
			}

			textRect.left += 120;
		}

		// Text Weight
		{
				float x = 10.0f;
				for (int fontWeight = 100; fontWeight < 800 /*1000*/; fontWeight += 100)
				{
					// Arial draws top two font weights lower than the rest.
					// Verdana, Times New Roman, and Trebuchet MS draws only two distinct weights.
					//auto textFormat = g.GetFactory().CreateTextFormat2(28.0f, "Arial", (GmpiDrawing::FontWeight) fontWeight);
					TextFormat textFormat = useFixedBoundingbox ?
						g.GetFactory().CreateTextFormat2(28.0f, "Arial", (GmpiDrawing::FontWeight)fontWeight)
						: g.GetFactory().CreateTextFormat(28.0f, "Arial", (GmpiDrawing::FontWeight)fontWeight);


					Rect textRect2 (x, 64, x + 23, 110);

					fallbackBrush.SetColor(Color::PapayaWhip);
					g.FillRectangle(textRect2, fallbackBrush);

					fallbackBrush.SetColor(Color::Black);
					g.DrawTextU("A", textFormat, textRect2, fallbackBrush);
					x += 24;
				}
		}

		DrawAlignmentCrossHairs(g);

		// Scale
		{
			fallbackBrush.SetColor(Color::Lime);
			for (float y = 0.5; y < r.getHeight(); y += 8)
			{
				g.DrawLine(0, y, 2, y, fallbackBrush);
			}
		}

#if 0
		// determin relationship between font-size and bounding box.
		for (auto fontFace : typefaces)
		{
			textRect.top = 200.5;

			_RPT1(_CRT_WARN, "%s----------------------------\n", fontFace);

			for (int fontSize = 10; fontSize < 50; ++fontSize)
			{
				dtextFormat = nullptr;
				getGuiHost()->CreateTextFormat2(
					fontFace,
					NULL,
					gmpi_gui::Font::W_Regular,
					gmpi_gui::Font::S_Normal,
					gmpi_gui::Font::ST_Normal,
					fontSize,
					0,							// locale.
					&dtextFormat.get()
					);

				Size textSize;
				dtextFormat.GetTextExtentU("1Mj|", (int32_t)4, textSize);

				_RPT2(_CRT_WARN, "%d, %f\n", fontSize, textSize.height);
			}

			textRect.left += 120;
		}
#endif
	}

	// Figure test
	{
		auto geometry = g.GetFactory().CreatePathGeometry();
		auto sink = geometry.Open();

		float centerX = 200.0f;
		float centerY = 200.0f;
		float radiusX = 100.0f;
		float radiusY = 150.0f;
		float startAngle = 0.0f;
		float endAngle = (float) M_PI * 2.0f;
		float stepSize = (endAngle - startAngle) * 0.1f;
		bool first = true;
		for (float a = startAngle; a < endAngle; a += stepSize)
		{
			float x = centerX + sinf(a) * radiusX;
			float y = centerY + cosf(a) * radiusY;
			Point p(x, y);

			if (first)
			{
				first = false;
				sink.BeginFigure(p);
			}
			else
			{
				sink.AddLine(Point(x, y));

			}
		}

		if ( (endAngle - startAngle) == 0.0 ) // TODO do detect full circle
		{
			sink.EndFigure();
		}
		else
		{
			sink.EndFigure(FigureEnd::Open);
		}

		sink.Close();

		float penWidth = 1;
		g.DrawGeometry(geometry, fallbackBrush, penWidth);
	}
}

// draw the string specified in 'pinText' in the font spcified by 'pinFontsize' and 'pinFontface'
void DrawingTestGui::drawSpecificFont(GmpiDrawing::Graphics& g)
{
	auto brush = g.CreateSolidColorBrush(Color::White);
	TextFormat dtextFormat = g.GetFactory().CreateTextFormat2((float)pinFontsize.getValue(), typefaces[pinFontface]);
	dtextFormat.SetTextAlignment(TextAlignment::Leading); // Left
	dtextFormat.SetParagraphAlignment(ParagraphAlignment::Far);
	dtextFormat.SetWordWrapping(WordWrapping::NoWrap); // prevent word wrapping into two lines that don't fit box.

	const auto textRect = getRect();
	g.DrawTextU(pinText, dtextFormat, textRect, brush);
}

void DrawingTestGui::drawTextTest(GmpiDrawing::Graphics& g)
{
	auto r = getRect();

	// Background Fill.
	auto brush = g.CreateSolidColorBrush(Color::White);
	g.FillRectangle(r, brush);
#if 0
//	if (pinTestType == 0)
	{

		float x = 10.5;
		Rect textRect;
		textRect.left = x;

		textRect.top = 200.5;

		const char* fontFace = typefaces[pinFontface];

		TextFormat dtextFormat = g.GetFactory().CreateTextFormat2((float)pinFontsize.getValue(), fontFace);

		dtextFormat.SetTextAlignment(TextAlignment::Leading); // Left
		dtextFormat.SetParagraphAlignment(ParagraphAlignment::Far);
		dtextFormat.SetWordWrapping(WordWrapping::NoWrap); // prevent word wrapping into two lines that don't fit box.

		string text = fontFace;
		if (!pinText.getValue().empty())
		{
			text = pinText;
		}

		GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
		dtextFormat.GetFontMetrics(&fontMetrics);
//		_RPT1(_CRT_WARN, "fontMetrics.ascent    %f\n", fontMetrics.ascent);
//		_RPT1(_CRT_WARN, "fontMetrics.descent   %f\n", fontMetrics.descent);
//		_RPT1(_CRT_WARN, "fontMetrics.capHeight %f\n", fontMetrics.capHeight);
//		_RPT1(_CRT_WARN, "fontMetrics.xHeight   %f\n", fontMetrics.xHeight);
//		_RPT1(_CRT_WARN, "fontMetrics.lineGap   %f\n", fontMetrics.lineGap);

		auto textSize = dtextFormat.GetTextExtentU(text);
		textRect.bottom = textRect.top + textSize.height;
		textRect.right = textRect.left + textSize.width;

		// Textbox verical size is ascent + descent.
//		textRect.bottom = textRect.top + fontMetrics.descent + fontMetrics.ascent;

		brush.SetColor(Color(0.8f, 0.8f, 0.8f));
		g.FillRectangle(textRect, brush);

		auto baseline = textRect.bottom - fontMetrics.descent; // for bottom-aligned text.
		const float lineWidth = 0.5f;
		const float lineLeft = textRect.left - 2;
		const float lineRight = textRect.right + 2;

		// Ascent (light-blue)
		// Ascent is the distance from the top of font character alignment box to the English baseline.
		brush.SetColor(Color::LightBlue);
		float y = baseline - fontMetrics.ascent;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// cap-height. (green)
		brush.SetColor(Color::Green);
		y = baseline - fontMetrics.capHeight;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// x-height. (blue)
		brush.SetColor(Color::MediumBlue);
		y = baseline - fontMetrics.xHeight;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// Base-line. (orange)
		brush.SetColor(Color::Coral);
		y = baseline;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// Descent/Bottom (light-blue)
		brush.SetColor(Color::AliceBlue);
		y = textRect.bottom;
		g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

		// Line-gap
		brush.SetColor(Color::Yellow);
		y = textRect.bottom + fontMetrics.lineGap;
		g.DrawLine(Point((textRect.left + textRect.right) * 0.5f, y), Point(textRect.right + 3, y), brush);

		// Text
		brush.SetColor(Color::Black);
		g.DrawTextU(text, dtextFormat, textRect, brush);

		return;
	}
#endif
	{
		// Create font.
		int font_size_ = 12;
		std::string text("Cat");
		float dipFontSize = (font_size_ * 72.f) / 96.f; // Points to DIPs conversion. https://social.msdn.microsoft.com/forums/vstudio/en-US/dfbadc0b-2415-4f92-af91-11c78df435b3/hwndhost-gdi-vs-directwrite-font-size

		TextFormat dtextFormat = g.GetFactory().CreateTextFormat(dipFontSize); // Default font face.

		brush.SetColor(Color(0, 0, 0));

		// Paths.
		float y = 20.5;
		float w = 40;
		int i = 0;

		for (int i = 0; i < 9; ++i)
		{
			// Lines draw 'nice' at co-ord x.5
			float x = 0.5f + 45.25f * (float)i;
			float penWidth = 1;

			Rect textRect;
			textRect.bottom = y + w;
			textRect.top = y;
			textRect.left = x;
			textRect.right = x + w;

			auto geometry = g.GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			Point p(x, y);

			sink.BeginFigure(p);

			sink.AddLine(Point(x + w, y));
			sink.AddLine(Point(x + w, y + w));
			sink.AddLine(Point(x, y + w));

			switch (i & 1)
			{
			case 0:
				sink.EndFigure();
				break;
			case 1:
				sink.EndFigure(FigureEnd::Open);
				break;
			}

			sink.Close();

			g.DrawGeometry(geometry, brush, penWidth);

			if (i != 0) // first one show default.
			{
				// Text.
				switch (i % 3)
				{
				case 0:
					dtextFormat.SetTextAlignment(TextAlignment::Leading);
					break;
				case 1:
					dtextFormat.SetTextAlignment(TextAlignment::Center);
					break;
				case 2:
					dtextFormat.SetTextAlignment(TextAlignment::Trailing);
					++penWidth;
					break;
				}

				switch ((i / 3) % 3)
				{
				case 0:
					dtextFormat.SetParagraphAlignment(ParagraphAlignment::Near); // Top
					break;
				case 1:
					dtextFormat.SetParagraphAlignment(ParagraphAlignment::Center); // Middle
					break;
				case 2:
					dtextFormat.SetParagraphAlignment(ParagraphAlignment::Far); // Bottom
					break;
				}
			}

			g.DrawTextU(text, dtextFormat, textRect, brush);
		}
	}

	// Word Wrapping.
	{
		const char* words[] = { /*"cat", "White Noise",*/ "the quick brown fox jumped over the lazy dog" };

		TextFormat dtextFormat = g.GetFactory().CreateTextFormat(); // Default font face, Default Size.
		dtextFormat.SetParagraphAlignment(ParagraphAlignment::Near); // Top
		dtextFormat.SetTextAlignment(TextAlignment::Leading); // Left

		float x = 10.5;

		for (int col = 0; col < 4; ++col)
		{
			float penWidth = 1.0f;
			float y = 130.5;
			Rect headingRect(x, y - 16, x+100, y);
			Rect textRect;
			textRect.top = y;
			textRect.left = x;
			const float maxWidth = 100.f;
			const char* desc = "";

			int32_t clipOPtion{}; // Use clip OR wrap, but not both. Clip does nothing on wrapped text.
			switch (col)
			{
			case 0: // test defaults
				desc = "Default";
				break;
			case 1:
				desc = "Clip";
				clipOPtion = (int32_t)DrawTextOptions::Clip;
				dtextFormat.SetWordWrapping(WordWrapping::NoWrap);
				break;
			case 2:
				desc = "Wrap";
				clipOPtion = (int32_t)DrawTextOptions::None;
				dtextFormat.SetWordWrapping(WordWrapping::Wrap);
				break;
			case 3:
				desc = "No Clip/Wrap";
				clipOPtion = (int32_t)DrawTextOptions::None;
				dtextFormat.SetWordWrapping(WordWrapping::NoWrap);
				break;
			}

			for (auto w : words)
			{
				auto textSize = dtextFormat.GetTextExtentU(w);
				textRect.bottom = textRect.top + ceilf(textSize.height);
				textRect.right = textRect.left + ceilf(textSize.width);

				if (textRect.getWidth() > maxWidth)
				{
					textRect.right = (std::min)(textRect.right, textRect.left + 100.f);
					textRect.bottom = textRect.top + textRect.getHeight() * 2.0f;
				}

				brush.SetColor(Color(0.0f, 0.0f, 1.0f));
				Rect boxRect(textRect);
				boxRect.Inflate(ceilf(penWidth * 0.5f)); // Box must always end up on whole number plus 0.5 pixel boundaries.

				g.DrawRectangle(boxRect, brush, penWidth);
				g.DrawTextU(w, dtextFormat, textRect, brush, clipOPtion);

				g.DrawTextU(desc, dtextFormat, headingRect, brush); // heading

				textRect.top += 20;
				++penWidth;
			}

			dtextFormat.SetWordWrapping(WordWrapping::Wrap);
			clipOPtion = (int32_t)DrawTextOptions::None;
			x += 120;
			headingRect.Offset(120, 0);
		}
	}
	// Fonts.
	{
		// Note: Segoe UI is not available on Mac and gets substituted.
		const char* typefacesl[] = { "Segoe UI", "Arial", "Courier New", "Times New Roman" };
		float fontSizes[] = { 8, 9, 10, 11, 12, 13, 14, 18 , 34, 72 };

		float x = 10.0f;
		Rect textRect;
		textRect.left = x;
		const float noBlur = 0.5f;
		const float lineWidth = 0.5f;
		const bool snapBaseline = true;

		for (auto fontFace : typefacesl)
		{
			textRect.top = 200.0f;

			for (auto dipFontSize : fontSizes)
			{
				auto textFormat = g.GetFactory().CreateTextFormat(dipFontSize, fontFace);
				GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
				textFormat.GetFontMetrics(&fontMetrics);

				Size textSize = textFormat.GetTextExtentU(fontFace);
				textRect.bottom = textRect.top + textSize.height;
				textRect.right = ceilf(textRect.left + textSize.width);

				if (snapBaseline)
				{
					const float baseLine = textRect.top + fontMetrics.ascent;
					const float yOffset = floorf(baseLine + 0.5f) - baseLine;
					textRect.Offset(0.0f, yOffset);
				}

				const float lineRight = textRect.right + 2;
				const float lineLeft = textRect.left - 2;

				brush.SetColor(Color::LightGray);
				g.FillRectangle(textRect, brush);

				// Metrics measure from TOP of box not bottom.
				const float baseLine = textRect.top + fontMetrics.ascent;

				// Descent
				brush.SetColor(Color::LightBlue);
				float y = textRect.top + fontMetrics.ascent + fontMetrics.descent;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

				// Baseline
				brush.SetColor(Color::Coral);
				y = baseLine;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

				// x-height.
				brush.SetColor(Color::MediumBlue);
				y = baseLine - fontMetrics.xHeight;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

				// cap-height.
				brush.SetColor(Color::Green);
				y = baseLine - fontMetrics.capHeight;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

				// Ascent. (should be same as top, unless in legacy mode).
				brush.SetColor(Color::Azure);
				y = baseLine - fontMetrics.ascent;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

				brush.SetColor(Color::Black);
				g.DrawTextU(fontFace, textFormat, textRect, brush);

				textRect.top += ceilf(dipFontSize * 1.4f);

				if ((fontFace[0] == 'T' || fontFace[0] == 'C') && dipFontSize == 9 )
				{
					auto textFormatSmall = g.GetFactory().CreateTextFormat(8);
					Rect textRect2 (0,0,1000,20);
					if (fontFace[0] == 'C')
					{
						textRect2.Offset(0, 9);
					}

					const char* metricNames[] = {
						"asc",
						"des",
						"lineGap",
						"capHeight",
						"xHeight",
						"ulPos",
						"ulThc",
						"stPos",
						"stThc" };

					float* metric = (float*)&fontMetrics;

					std::stringstream s;
					s << std::string(fontFace) << " " << dipFontSize << "px: ";

					for (int i = 0; i < 6; ++i)
					{
						s << metricNames[i] << " "  << *metric++ << ", ";
					}
			
//					s << "topgap: " << textSize.height - (fontMetrics.ascent + fontMetrics.descent);
					s << "box: (" << textRect.getWidth() << ", " << textRect.getHeight() <<")";

					g.DrawTextU(s.str(), textFormatSmall, textRect2, brush);
				}
			}

			textRect.left += 120;
		}

		// Text Weight
		{
				float x = 10.0f;
				for (int fontWeight = 100; fontWeight < 800 /*1000*/; fontWeight += 100)
				{
					// Arial draws top two font weights lower than the rest.
					// Verdana, Times New Roman, and Trebuchet MS draws only two distinct weights.
					auto textFormat = g.GetFactory().CreateTextFormat(28, "Arial", (GmpiDrawing_API::MP1_FONT_WEIGHT) fontWeight);
					Rect textRect2 (x, 64, x + 23, 110);

					brush.SetColor(Color::PapayaWhip);
					g.FillRectangle(textRect2, brush);

					brush.SetColor(Color::Black);
					g.DrawTextU("A", textFormat, textRect2, brush);
					x += 24;
				}
		}

		// Alignment cross hairs.
		{
			float crossSize = 6.0f;
			float x1 = 8.5;
			float y1 = 8.5;
			g.DrawLine(x1 - crossSize, y1, x1 + crossSize, y1, brush); // criss
			g.DrawLine(x1, y1 - crossSize, x1, y1 + crossSize, brush); // cross

			x1 = r.right - x1;
			y1 = r.bottom - y1;
			brush.SetColor(Color::Black);
			g.DrawLine(x1 - crossSize, y1, x1 + crossSize, y1, brush); // criss
			g.DrawLine(x1, y1 - crossSize, x1, y1 + crossSize, brush); // cross
		}

		// Scale
		{
			brush.SetColor(Color::Lime);
			for (float y = 0.5; y < r.getHeight(); y += 8)
			{
				g.DrawLine(0, y, 2, y, brush);
			}
		}

#if 0
		// determin relationship between font-size and bounding box.
		for (auto fontFace : typefaces)
		{
			textRect.top = 200.5;

			_RPT1(_CRT_WARN, "%s----------------------------\n", fontFace);

			for (int fontSize = 10; fontSize < 50; ++fontSize)
			{
				dtextFormat = nullptr;
				getGuiHost()->CreateTextFormat(
					fontFace,
					NULL,
					gmpi_gui::Font::W_Regular,
					gmpi_gui::Font::S_Normal,
					gmpi_gui::Font::ST_Normal,
					fontSize,
					0,							// locale.
					&dtextFormat.get()
					);

				Size textSize;
				dtextFormat.GetTextExtentU("1Mj|", (int32_t)4, textSize);

				_RPT2(_CRT_WARN, "%d, %f\n", fontSize, textSize.height);
			}

			textRect.left += 120;
		}
#endif
	}

	// Figure test
	{
		auto geometry = g.GetFactory().CreatePathGeometry();
		auto sink = geometry.Open();

		float centerX = 200.0f;
		float centerY = 200.0f;
		float radiusX = 100.0f;
		float radiusY = 150.0f;
		float startAngle = 0.0f;
		float endAngle = (float) M_PI * 2.0f;
		float stepSize = (endAngle - startAngle) * 0.1f;
		bool first = true;
		for (float a = startAngle; a < endAngle; a += stepSize)
		{
			float x = centerX + sinf(a) * radiusX;
			float y = centerY + cosf(a) * radiusY;
			Point p(x, y);

			if (first)
			{
				first = false;
				sink.BeginFigure(p);
			}
			else
			{
				sink.AddLine(Point(x, y));

			}
		}

		if ( (endAngle - startAngle) == 0.0 ) // TODO do detect full circle
		{
			sink.EndFigure();
		}
		else
		{
			sink.EndFigure(FigureEnd::Open);
		}

		sink.Close();

		float penWidth = 1;
		g.DrawGeometry(geometry, brush, penWidth);
	}
}

int32_t DrawingTestGui::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	returnDesiredSize->width = 544;
	returnDesiredSize->height = 544;

	return gmpi::MP_OK;
}

