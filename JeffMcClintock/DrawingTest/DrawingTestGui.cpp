#include "./DrawingTestGui.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <sstream>

#include "../shared/fast_gamma.h"

using namespace std;
using namespace gmpi;
using namespace gmpi_gui;
using namespace GmpiDrawing;

GMPI_REGISTER_GUI(MP_SUB_TYPE_GUI2, DrawingTestGui, L"SE Drawing Test" );

DrawingTestGui::DrawingTestGui()
{
	initializePin(pinTestType, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinFontface, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinFontsize, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinText, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinApplyAlphaCorrection, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));
	initializePin(pinAdjust, static_cast<MpGuiBaseMemberPtr2>(&DrawingTestGui::refresh));	
}

void DrawingTestGui::refresh()
{
	invalidateRect();
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
		auto pixelsSource = bitmapMem.lockPixels(true);
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

void DrawingTestGui::drawMacGraphicsTest(GmpiDrawing::Graphics& g)
{
//	testClient.OnRender(g.Get());
}

void DrawingTestGui::drawGradient(GmpiDrawing::Graphics& g)
{
	auto textFormat = g.GetFactory().CreateTextFormat();
	auto textBrush = g.CreateSolidColorBrush(Color::Orange);

	const int resolution = 1; // 1 or 10
	const float gamma = 2.2f;
	float foregroundColor[3] = { 1, 1, 1 }; // BGR

	const int count = 256;
	const int height = 40;
	// create bitmap with every intensity.
	auto bitmapMem = GetGraphicsFactory().CreateImage(count, count);
	{
		auto pixelsSource = bitmapMem.lockPixels(true);
		auto imageSize = bitmapMem.GetSize();
		int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

		uint8_t* sourcePixels = pixelsSource.getAddress();

		float x = 0;
		float foreground = 0.0f;
		float alpha = 1.0f;

		for (int x = 0; x < count; ++x)
		{
			for (int y = 0; y < height; ++y)
			{
				int alphaVal = (int)(alpha * 255.0f + 0.5f);

				int pixelVal[3];
				for (int i = 0; i < 3; ++i)
				{
					// apply alpha in lin space.
					//float fg = foregroundColor[i] * foreground;
					//fg *= alpha; // pre-multiply.

								 // then convert to SRGB.
					pixelVal[i] = x; // se_sdk::FastGamma::float_to_sRGB(fg);
				}

				uint8_t* pixel = sourcePixels + ((int) sizeof(uint32_t) * (x + y * (int)(imageSize.width)));

				for (int i = 0; i < 3; ++i)
					pixel[i] = pixelVal[i];

				pixel[3] = alphaVal;
			}
		}
	}

	if (pinApplyAlphaCorrection)
	{
//		bitmapMem.ApplyAlphaCorrection();
		//MyApplyGammaCorrection(bitmapMem);
	}

	g.DrawBitmap(bitmapMem, Point(0,0), Rect(0, 0, count, height));
	g.DrawTextU("Bitmap RGB", textFormat, 0.0f, 0.0f, textBrush);

	// Use brushes to draw every sRGB intensity.
	float x1 = 0;
	float y1 = 50;
	auto brush = g.CreateSolidColorBrush(Color::Transparent());

	for (int x = 0; x < count; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			brush.SetColor(Color::FromBytes(x,x,x));
			g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
		}
	}
	g.DrawTextU("Brush RGB", textFormat, x1, y1, textBrush);

	y1 = 100;

	for (float x = 0; x < count; ++x)
	{
		for (float y = 0; y < height; ++y)
		{
			brush.SetColor(Color(x / 256.0f, x / 256.0f, x / 256.0f));
			g.FillRectangle(x1 + x, y1 + y, x1 + x + 1, y1 + y + 1, brush);
		}
	}
	g.DrawTextU("Brush Linear", textFormat, x1, y1, textBrush);
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

	auto r = getRect();

	if (pinTestType == 2)
	{
		drawGammaTest(g);
		return MP_OK;
	}
	if (pinTestType == 3)
	{
		drawGradient(g);
		return MP_OK;
	}

	if (pinTestType == 4)
	{
		drawMacGraphicsTest(g);
		return MP_OK;
	}
	
	// Background Fill.
	auto brush = g.CreateSolidColorBrush(Color::White);
	g.FillRectangle(r, brush);

	if (pinTestType == 0)
	{
		const char* typefaces[] = { "Segoe UI", "Arial", "Courier New", "Times New Roman" , "MS Sans Serif" };

		float x = 10.5;
		Rect textRect;
		textRect.left = x;

		textRect.top = 200.5;

		const char* fontFace = typefaces[pinFontface];

		TextFormat dtextFormat = g.GetFactory().CreateTextFormat((float)pinFontsize.getValue(), fontFace);

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

		return gmpi::MP_OK;
	}

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
		float x = 0.5f + 45.25f * (float) i;
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

		sink.AddLine(Point(x+w, y));
		sink.AddLine(Point(x+w, y + w));
		sink.AddLine(Point(x, y+w));

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

	// Text Extents.
	{
		const char* words[] = { "cat", "White Noise", "the quick brown fox jumped over the lazy dog" };

		dtextFormat.SetParagraphAlignment(ParagraphAlignment::Near); // Top
		dtextFormat.SetTextAlignment(TextAlignment::Leading); // Left

		float x = 10.5;
		for (int col = 0; col < 2; ++col)
		{
			float penWidth = 1.0f;
			float y = 120.5;
			Rect textRect;
			textRect.top = y;
			textRect.left = x;
			dtextFormat.SetWordWrapping(WordWrapping::NoWrap);
			const float maxWidth = 100.f;
			int32_t clipOPtion{};

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

				textRect.top += 20;
				++penWidth;
			}

			dtextFormat.SetWordWrapping(WordWrapping::Wrap);
			clipOPtion = (int32_t)DrawTextOptions::Clip;
			x += 200;
		}
	}

	// Fonts.
	{
		// Note: Segoe UI is not available on Mac and gets substituted.
		const char* typefaces[] = { "Segoe UI", "Arial", "Courier New", "Times New Roman" };
		float fontSizes[] = { 8, 9, 10, 11, 12, 13, 14, 18 , 34, 72 };

		float x = 10.0f;
		Rect textRect;
		textRect.left = x;
		const float noBlur = 0.5f;
		const float lineWidth = 0.5f;
		const bool snapBaseline = true;

		for (auto fontFace : typefaces)
		{
			textRect.top = 200.0f;

			for (auto dipFontSize : fontSizes)
			{
				auto textFormat = g.GetFactory().CreateTextFormat(dipFontSize, fontFace);
				GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
				textFormat.GetFontMetrics(&fontMetrics);

				float yOffset{};
				if (snapBaseline)
				{
					const float baseLine = textRect.top + fontMetrics.ascent;
					yOffset = floorf(baseLine + 0.5f) - baseLine;
				}

				Size textSize = textFormat.GetTextExtentU(fontFace);
				textRect.bottom = textRect.top + textSize.height;
				textRect.right = ceilf(textRect.left + textSize.width);
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

				/* not relevant, should be same as Descent unless box deliberatly bigger than needed.
				brush.SetColor(Color::OrangeRed);
				y = textRect.bottom;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);
				*/

				// cap-height.
				brush.SetColor(Color::Green);
				y = baseLine - fontMetrics.capHeight;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

				// x-height.
				brush.SetColor(Color::MediumBlue);
				y = baseLine - fontMetrics.xHeight;
				g.DrawLine(Point(lineLeft, y), Point(lineRight, y), brush, lineWidth);

				brush.SetColor(Color::Black);
				Rect snappedRect = textRect;
				snappedRect.Offset(0.0f, yOffset);
				g.DrawTextU(fontFace, textFormat, snappedRect, brush);

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

		// EEEEs
		{
			const auto str = "E";
			const auto fontFace = "Courier New";
			float fontSizes[] = { 8, 8.5, 9, 9.5, 10, 10.5 , 11, 11.5, 12 };

			float starty = 66.f;

			for (auto dipFontSize : fontSizes)
			{
				float x = 280.f;
				float y = starty;

				auto textFormat = g.GetFactory().CreateTextFormat(dipFontSize, fontFace);

				float yOffset{};

				GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
				if (snapBaseline)
				{
					textFormat.GetFontMetrics(&fontMetrics);

					const float OriginalBaseline = fontMetrics.ascent;

					const float OriginalBaselineSnapped = floorf(y + OriginalBaseline + 0.5f);
					brush.SetColor(Color::Green);
					float ybl = OriginalBaselineSnapped;
					g.DrawLine(Point(x - 3, ybl), Point(x - 2, ybl), brush, 0.5);

#if 0 // snap Cap height (will result in different size font, probly will alter text length (unwanted)
					const auto snapY = fontMetrics.capHeight;
					const auto scaleOffset = floorf(snapY + 0.5f) / snapY;
					const float snappedDipFontSize = dipFontSize * scaleOffset;

					const float NewBaseline = OriginalBaseline * scaleOffset;

					// TODO: don't bother if close enough.
					textFormat = g.GetFactory().CreateTextFormat(snappedDipFontSize, fontFace);

					yOffset = OriginalBaseline - NewBaseline;

					textFormat.GetFontMetrics(&fontMetrics);
#endif
//					float baseLIneWillbe = fontMetrics.ascent + fontMetrics.descent + yOffset;
				}

				Size textSize = textFormat.GetTextExtentU(str);


				for (int i = 0; i < 50; ++i)
				{
					textRect.top = y;
					textRect.left = x;
					textRect.bottom = textRect.top + textSize.height;
					textRect.right = ceilf(textRect.left + textSize.width);
					const float lineRight = textRect.right + 2;
					const float lineLeft = textRect.left - 2;

					brush.SetColor(Color::LightGray);
					//				g.FillRectangle(textRect, brush);
					g.DrawLine(Point(textRect.left + 2, y), Point(textRect.right - 2, y), brush, 0.5);

					float snapOffset{};
					if (snapBaseline)
					{
						const float baseLine = textRect.top + fontMetrics.ascent + yOffset;
						snapOffset = floorf(baseLine + 0.5f) - baseLine;
					}

					brush.SetColor(Color::Black);
					Rect snappedRect = textRect;
					snappedRect.Offset(0.0f, yOffset + snapOffset);
					g.DrawTextU(str, textFormat, snappedRect, brush, DrawTextOptions::NoSnap);

					x += 4.f;
					y += 0.05f;
				}

				starty += 10.0f;
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

	return gmpi::MP_OK;
}

int32_t DrawingTestGui::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	returnDesiredSize->width = 544;
	returnDesiredSize->height = 544;

	return gmpi::MP_OK;
}

