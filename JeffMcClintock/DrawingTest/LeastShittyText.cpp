#include "LeastShittyText.h"
#include <string>

using namespace GmpiDrawing;

uint8_t ascii_7_px[1][7] =
{
	{   // L
		0b00010000,
		0b00010000,
		0b00010000,
		0b00010000,
		0b00010000,
		0b00010000,
		0b00011111,
	},
};

bool match(char c, const char* letters)
{
	while (*letters)
	{
		if (c == *letters)
			return true;

		letters++;
	}
	return false;
}

void SmallText::DrawTextShitty(Graphics& g, const std::string& text, GmpiDrawing::Rect& rect, GmpiDrawing::Brush brush)
{
	if (!bitmapMem.Get())
	{
		bitmapMem = g.GetFactory().CreateImage(MAX_WIDTH, MAX_WIDTH);
	}

	const int topi = static_cast<int>(0.9f + rect.top); // round up
	const int boti = static_cast<int>(rect.bottom);		// round down
	const int height = boti - topi;
	const float cellWidthf = height * 5.f / 7.f;
	const int width = static_cast<int>(0.5f + cellWidthf);
	const int ptSize = (std::max)(1, static_cast<int>(0.5f + height / 7.f));

	const int left = 0;
	const int right = width - 1;
	const int top = 0;
	const int bottom = height - 1;

	const int leftIn = (std::min)(left + ptSize, right);
	const int rightIn = (std::max)(right - ptSize, left);
	const int topIn = (std::min)(top + ptSize, bottom);
	const int bottomIn = (std::max)(bottom - ptSize, top);

	const int midY = (height - 1) / 2;
	const int midYUp = (std::max)(midY - ptSize, top);
	const int midYDn = (std::min)(midY + ptSize, bottom);

	const int midX = (width - 1) / 2;


	gridPen pen(grid);

	constexpr uint32_t ON = rgBytesToPixel(0xff,0xff,0xff);
	constexpr uint32_t OFF = 0;

	int x = rect.left;
	const int y = rect.top;
	for (int i = 0; i < text.size(); ++i)
	{
		const auto c = text[i];
		pen.clear();

		// paint glyf

		// 1.
		if (match(c, "LEFHIKMNTUVWXZ"))
		{
			pen.start(left, top);
		}
		else if (match(c, "ABPR"))
		{
			pen.start(left, bottom);
		}
		else if (match(c, "CDGOQ0"))
		{
			pen.start(right, topIn);
			pen.lineto(rightIn, top);
		}
		else if (match(c, "J"))
		{
			pen.start(left, bottomIn);
		}
		else if (match(c, "S"))
		{
			pen.start(rightIn, top);
		}
		else if (match(c, "1"))
		{
			auto dx = midX / 2;
			pen.start(midX, bottom);
			pen.lineto(midX, top);
			pen.lineto(midX - dx, top + dx);
		}
		else if (match(c, "2"))
		{
			pen.start(left, topIn);
			pen.lineto(leftIn, top);
			pen.lineto(rightIn, top);
			pen.lineto(right, topIn);
			pen.lineto(right, midYUp);
			pen.lineto(left, bottom);
			pen.lineto(right, bottom);
		}
		else if (match(c, "3"))
		{
			auto dx = midX;
			pen.start(left, top);
			pen.lineto(right, top);
			pen.lineto(right - dx, top + dx);
			pen.lineto(right, top + midX * 2);

			pen.lineto(right, bottomIn);
			pen.lineto(rightIn, bottom);
			pen.lineto(leftIn, bottom);
			pen.lineto(left, bottomIn);
		}
		else if (match(c, "4"))
		{
			auto dx = midX / 2;
			pen.start(right, midY);
			pen.lineto(left, midY);
			pen.lineto(right - dx, top);
			pen.lineto(right - dx, bottom);
		}

		// 2.
		if (match(c, "LEFHIKMN"))
		{
			pen.lineto(left, bottom);
		}
		else if (match(c, "A"))
		{
			// add curve on top left
			pen.lineto(left, topIn);
			pen.lineto(leftIn, top);
		}
		else if (match(c, "BDPR")) // to hard top left
		{
			pen.lineto(left, top);
		}
		else if (match(c, "CGOQ0")) // to hard top left
		{
			pen.lineto(leftIn, top);
			pen.lineto(left, topIn);
		}
		else if (match(c, "TZ")) // to hard top left
		{
			pen.lineto(right, top);
		}
		else if (match(c, "S"))
		{
			pen.lineto(leftIn, top);
			pen.lineto(left, topIn);
			pen.lineto(left, midYUp);
			pen.lineto(leftIn, midY);

			pen.lineto(rightIn, midY);
			pen.lineto(right, midYDn);
			pen.lineto(right, bottomIn);
			pen.lineto(rightIn, bottom);
			pen.lineto(left, bottom);
		}
		else if (match(c, "V"))
		{
			auto dx = midX;
			pen.lineto(left, bottom - dx);
			pen.lineto(midX, bottom);
		}
		else if (match(c, "W"))
		{
			auto dx = midX / 2;
			pen.lineto(left, bottom - dx);
			pen.lineto(left + dx, bottom);
			pen.lineto(midX, bottom - dx);
			pen.lineto(midX, midY);
			pen.lineto(midX, bottom - dx);
			pen.lineto(right - dx, bottom);
			pen.lineto(right, bottom - dx);
		}
		else if (match(c, "X"))
		{
			auto dx = midX / 2;
			pen.lineto(left, top + dx);
			pen.lineto(right, bottom);
			pen.start(right, top);
			pen.lineto(right, top + dx);
			pen.lineto(left, bottom);
		}
		else if (match(c, "Y"))
		{
			pen.start(right, top);
			pen.lineto(midX, bottom);
			pen.start(left, top);
			pen.lineto(midX, bottom);
		}

		// 3.
		if (match(c, "LE"))
		{
			pen.lineto(right, bottom);
		}
		else if (match(c, "ABPR"))
		{
			// add curve on top right
			pen.lineto(rightIn, top);
			pen.lineto(right, topIn);
		}
		else if (match(c, "HMN"))
		{
			pen.start(right, top);
			pen.lineto(right, bottom);
		}
		else if (match(c, "K"))
		{
			pen.start(left, midY);
			auto dx = midY;
			pen.lineto(left + dx, top);

			pen.start(left, midY);
			dx = bottom - midY;
			pen.lineto(left + dx, bottom);
		}
		else if (match(c, "T")) // to hard top left
		{
			pen.start(midX, top);
			pen.lineto(midX, bottom);
		}
		else if (match(c, "Z")) // to hard top left
		{
			auto dy = right / 4;
			pen.lineto(right, top + dy);
			pen.lineto(left, bottom);
			pen.lineto(right, bottom);
		}

		// 4.
		if (match(c, "A"))
		{
			pen.lineto(right, bottom);
		}
		else if (match(c, "BPR"))
		{
			// add curve to right midY
			pen.lineto(right, midYUp);
			pen.lineto(rightIn, midY);
			pen.lineto(left, midY);
		}
		else if (match(c, "D"))
		{
			// add curve to right midY
			pen.lineto(left, bottom);
		}
		else if (match(c, "CGJOQU0"))
		{
			// add curve to right midY
			pen.lineto(left, bottomIn);
			pen.lineto(leftIn, bottom);
			pen.lineto(rightIn, bottom);
			pen.lineto(right, bottomIn);
		}
		else if (match(c, "EF"))
		{
			pen.start(left, top);
			pen.lineto(right, top);
			pen.start(left, midY);
			pen.lineto(rightIn, midY);
		}
		else if (match(c, "H"))
		{
			pen.start(left, midY);
			pen.lineto(right, midY);
		}
		else if (match(c, "M"))
		{
			pen.start(left, top);
			auto dy = midX;
			pen.lineto(midX, dy);

			pen.start(right, top);
			pen.lineto(midX, dy);
		}
		else if (match(c, "N"))
		{
			auto dy = right;
			auto sx = (bottom - dy) / 2;
			pen.start(left, sx);
			pen.lineto(right, sx + dy);
		}

		// 5.
		if (match(c, "A"))
		{
			pen.start(left, midY);
			pen.lineto(right, midY);
		}
		else if (match(c, "B"))
		{
			// add curve to right midY
			pen.lineto(rightIn, midY);
			pen.lineto(right, midYDn);
			pen.lineto(right, bottomIn);
			pen.lineto(rightIn, bottom);
			pen.lineto(left, bottom);
		}
		else if (match(c, "R"))
		{
			// add curve to right midY
			pen.start(right, bottom);
			const int toMid = bottom - midX - 1;
			pen.lineto(right - toMid, midY);
		}
		else if (match(c, "DOQ0"))
		{
			pen.lineto(rightIn, bottom);
			pen.lineto(right, bottomIn);
			pen.lineto(right, topIn);
		}
		else if (match(c, "G"))
		{
			pen.lineto(right, midY);
			pen.lineto(midX, midY);
		}
		else if (match(c, "JUVW"))
		{
			pen.lineto(right, top);
		}

		if (match(c, "Q"))
		{
			pen.start(right, bottom);
			auto dy = (bottom - midY + 1) / 2;
			pen.lineto(right - dy, bottom - dy);
		}


#if 0	
		const std::string feat_topBar{ "357DEFBPRTZ" };
		if (feat_topBar.find_first_of(c) != std::string::npos)
		{
			pen.start(left, top);
			pen.lineto(right, top);
		}

		const std::string feat_midpBar{ "4ABEFHPR" };
		if (feat_midpBar.find_first_of(c) != std::string::npos)
		{
			pen.start(left, midY);
			pen.lineto(rightIn, midY);
		}
		const std::string feat_botBar{ "2BDELZ" };
		if (feat_botBar.find_first_of(c) != std::string::npos)
		{
			pen.start(left, bottom);
			pen.lineto(right, bottom);
		}

		const std::string feat_leftBar{ "ABCDEFGHIKLMNOPQRU[" };
		if (feat_leftBar.find_first_of(c) != std::string::npos)
		{
			pen.start(left, top);
			pen.lineto(left, bottom);
		}

		const std::string feat_RIGHTtBar{ "ADHJMNOQU]" };
		if (feat_RIGHTtBar.find_first_of(c) != std::string::npos)
		{
			pen.start(right, top);
			pen.lineto(right, bottom);
		}

		const std::string feat_curvedTop{ "0289@ACGOQ" };
		if (feat_curvedTop.find_first_of(c) != std::string::npos)
		{
			pen.start(left, topIn);
			pen.lineto(leftIn, top);
			pen.lineto(rightIn, top);
			pen.lineto(right, topIn);
		}

		const std::string feat_curvedBot{ "0368CGJOQU" };
		if (feat_curvedBot.find_first_of(c) != std::string::npos)
		{
			pen.start(left, bottomIn);
			pen.lineto(leftIn, bottom);
			pen.lineto(rightIn, bottom);
			pen.lineto(right, bottomIn);
		}
#endif

		int maxX = 0;

		// Convert to bitmap.
		{ // RIAA for pixels
			auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
			const auto imageSize = bitmapMem.GetSize();
			const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

			uint8_t* sourcePixels = pixelsSource.getAddress();

			// clear bitmap
			int32_t* pixel = (int32_t*)sourcePixels;
			for (auto i = 0; i < totalPixels; ++i)
			{
				pixel[i] = 0;
			}

			for (int py = 0; py < height; ++py)
			{
				for (int px = 0; px < width; ++px)
				{
					const auto col = grid[px][py];
					if (col)
					{
						const auto premutliplyBackground = 255 - col;
						const auto premultipliedForeground = col;
						//					pixel[px + py * MAX_WIDTH] = col | (col << 8) | (col << 16) | 0xFF000000;
						pixel[px + py * MAX_WIDTH] = (premutliplyBackground << 24) | col | (col << 8) | (col << 16);
						//					pixel[px + py * MAX_WIDTH] = col | 0xffffff00;

						maxX = (std::max)(maxX, px);
					}
				}
			}
		}
		g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)width, (float)height), GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);

		x += maxX + 1 + ptSize;
	}
}

void SmallText::DrawTextShitty3(Graphics& g, const std::string& text, GmpiDrawing::Rect& rect, GmpiDrawing::Brush brush)
{
	if (!bitmapMem.Get())
	{
		bitmapMem = g.GetFactory().CreateImage(MAX_WIDTH, MAX_WIDTH);
	}

	const int32_t ON = 0xFFFFFFF;
	const int32_t OFF = 0;

	const int topi = static_cast<int>(0.9f + rect.top); // round up
	const int boti = static_cast<int>(rect.bottom);		// round down

	const int cellHeight = boti - topi;
	const int mid = cellHeight / 2;

	const int pixelSize = (std::max)(1, (cellHeight + 3) / 7);
	const int pixelCutoutSize = cellHeight > 3 ? pixelSize : 0;
	bool pixelSizeIsOdd = (pixelSize & 1) != 0;

	const int char_width = (3 + cellHeight * 5) / 7;

	const int cornerRadius = pixelSize;
	const int spacing = pixelSize;

	const float lineWidth = static_cast<float>(pixelSize); // (std::max)(1, 1 + ((pixelSize - 1) / 2) * 2)); // no even line widths
	const Size cornerSize((float)cornerRadius, (float)cornerRadius);

	// current char origin
	int x = rect.left;
	const int y = rect.top;
	Size o{ static_cast<float>(x), static_cast<float>(topi) };
	if (pixelSizeIsOdd)
	{
		o.width += 0.5f;
		o.height += 0.5f;
	}

	const int midY = (cellHeight - 1) / 2;
	const int midX = (char_width - 1) / 2;

	for (int i = 0; i < text.size(); ++i)
	{
		const auto c = text[i];

		{ // RIAA for pixels
			auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
			const auto imageSize = bitmapMem.GetSize();
			const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

			uint8_t* sourcePixels = pixelsSource.getAddress();

			// clear bitmap
			int32_t* pixel = (int32_t*)sourcePixels;
			for (auto i = 0; i < totalPixels; ++i)
			{
				pixel[i] = 0;
			}

			// paint features.
			const std::string feat_topBar{ "357DEFBPRTZ" };
			if (feat_topBar.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < pixelSize; ++y)
				{
					for (int x = 0; x < char_width; ++x)
					{
						pixel[x + y * MAX_WIDTH] = ON;
					}
				}
			}

			const std::string feat_midpBar{ "4ABEFHPR" };
			if (feat_midpBar.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < pixelSize; ++y)
				{
					auto py = midY - y;
					for (int x = 0; x < char_width - pixelSize; ++x)
					{
						pixel[x + py * MAX_WIDTH] = ON;
					}
				}
			}

			const std::string feat_botBar{ "2BDELZ" };
			if (feat_botBar.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < pixelSize; ++y)
				{
					for (int x = 0; x < MAX_WIDTH; ++x)
					{
						auto py = cellHeight - 1 - y;
						pixel[x + py * MAX_WIDTH] = ON;
					}
				}
			}

			const std::string feat_leftBar{ "ABCDEFGHIKLMNOPQRU[" };
			if (feat_leftBar.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < cellHeight; ++y)
				{
					for (int x = 0; x < pixelSize; ++x)
					{
						pixel[x + y * MAX_WIDTH] = ON;
					}
				}
			}

			const std::string feat_RIGHTtBar{ "ADHJMNOQU]" };
			if (feat_RIGHTtBar.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < cellHeight; ++y)
				{
					for (int x = 0; x < pixelSize; ++x)
					{
						const auto px = char_width - 1 - x;
						pixel[px + y * MAX_WIDTH] = ON;
					}
				}
			}

			const std::string feat_curvedTop{ "0289@ACGOQ" };
			if (feat_curvedTop.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < pixelSize * 2; ++y)
				{
					for (int x = 0; x < char_width; ++x)
					{
						bool lit = x >= pixelSize && x < char_width - pixelSize;
						if (y >= pixelSize)
							lit = !lit;

						pixel[x + y * MAX_WIDTH] = lit ? ON : OFF;
					}
				}
			}

			const std::string feat_curvedBot{ "0368CGJOQU" };
			if (feat_curvedBot.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < pixelSize * 2; ++y)
				{
					const auto py = cellHeight - 1 - y;
					for (int x = 0; x < char_width; ++x)
					{
						bool lit = x >= pixelSize && x < char_width - pixelSize;
						if (y >= pixelSize)
							lit = !lit;

						pixel[x + py * MAX_WIDTH] = lit ? ON : OFF;
					}
				}
			}

			const std::string feat_pTop{ "BPR" };
			if (feat_pTop.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y <= midY - pixelSize; ++y)
				{
					for (int x = 0; x < pixelSize; ++x)
					{
						const auto px = char_width - 1 - x;

						const auto lit = y >= pixelSize;

						pixel[px + y * MAX_WIDTH] = lit ? ON : OFF;
					}
				}
			}
			const std::string feat_Bbottom{ "B" };
			if (feat_Bbottom.find_first_of(c) != std::string::npos)
			{
				for (int y = midY + 1; y < cellHeight; ++y)
				{
					for (int x = 0; x < pixelSize; ++x)
					{
						const auto px = char_width - 1 - x;
						const auto lit = y < cellHeight - pixelSize;
						pixel[px + y * MAX_WIDTH] = lit ? ON : OFF;
					}
				}
			}
			const std::string feat_Dright{ "D" };
			if (feat_Dright.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < pixelSize; ++y)
				{
					for (int x = 0; x < pixelSize; ++x)
					{
						const auto px = char_width - 1 - x;
						pixel[px + y * char_width] = OFF;

						pixel[px + (cellHeight - 1 - y) * MAX_WIDTH] = OFF;
					}
				}
			}

			const std::string feat_gstick{ "G" };
			if (feat_gstick.find_first_of(c) != std::string::npos)
			{
				for (int y = midY; y < cellHeight; ++y)
				{
					for (int x = 0; x < pixelSize; ++x)
					{
						const auto px = char_width - 1 - x;
						const auto lit = true; // y < cellHeight - pixelSize;
						pixel[px + y * MAX_WIDTH] = lit ? ON : OFF;
					}
				}

				for (int y = 0; y < pixelSize; ++y)
				{
					auto py = midY - y;// +pixelSize;
					for (int x = pixelSize * 2; x < char_width; ++x)
					{
						pixel[x + py * MAX_WIDTH] = ON;
					}
				}
			}

			const std::string feat_kangles{ "K" };
			if (feat_kangles.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < cellHeight; ++y)
				{
					for (int x = 0; x < char_width; ++x)
					{
						const auto lit =
							abs(y - midY + pixelSize - x) < pixelSize || // top angle
							abs(midY - y + pixelSize - x) < pixelSize;

						if (lit)
						{
							pixel[x + y * MAX_WIDTH] = ON;
						}
					}
				}
			}
			// perhaps M must always be an odd number of pixels wide?
			const std::string feat_Mangles{ "M" };
			if (feat_Mangles.find_first_of(c) != std::string::npos)
			{
				for (int y = 0; y < cellHeight; ++y)
				{
					for (int x = 0; x < char_width; ++x)
					{
						int k = (midY - pixelSize) - abs(midX - x);
						const auto lit = abs(k - y) < pixelSize;

						if (lit)
						{
							pixel[x + y * MAX_WIDTH] = ON;
						}
					}
				}
			}
		}

		g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));

		o.width += char_width + spacing;
		x += char_width + spacing;
	}
}

void DrawTextShitty2(Graphics& g, const std::string& text, GmpiDrawing::Rect& rect, GmpiDrawing::Brush brush)
{
//	int char_width = (rect.getHeight() * 5) / 7;

	const int topi = static_cast<int>(0.9f + rect.top); // round up
	const int boti = static_cast<int>(rect.bottom);		// round down

	const int cellHeight = boti - topi;
	const int mid = cellHeight / 2;

	const int pixelSize = (std::max)(1, (cellHeight + 3) / 7);
	const int pixelCutoutSize = cellHeight > 3 ? pixelSize : 0;
	bool pixelSizeIsOdd = (pixelSize & 1) != 0;

	const int char_width = (3 + cellHeight * 5) / 7;

	const int cornerRadius = pixelSize;
	const int spacing = pixelSize;

	const float top = static_cast<float>(pixelSize / 2);
	const float bot = static_cast<float>(cellHeight - pixelSize / 2);
	const float left = static_cast<float>(pixelSize / 2);
	const float rigt = static_cast<float>(char_width - pixelSize / 2);

	const Point tl{ left , top };
	const Point ml{ left , (float)mid };
	const Point mr{ rigt , (float)mid };
	const Point bl{ left , bot };
	const Point tr{ rigt, top };
	const Point br{ rigt, bot };

	const Point trl = tr - Size((float)cornerRadius, 0.f);
	const Point trd = tr + Size(0.f, (float)cornerRadius);
	const Point bru = br - Size(0.f, (float)cornerRadius);
	const Point brl = br - Size((float)cornerRadius, 0.f);
	const Point blr = bl + Size((float)cornerRadius, 0.f);
	const Point blu = bl - Size(0.f, (float)cornerRadius);

	const float lineWidth = static_cast<float>(pixelSize); // (std::max)(1, 1 + ((pixelSize - 1) / 2) * 2)); // no even line widths
	const Size cornerSize((float)cornerRadius, (float)cornerRadius);

	// current char origin
	int x = rect.left;
	const int y = rect.top;
	Size o{ static_cast<float>(x), static_cast<float>(topi) };
	if (pixelSizeIsOdd)
	{
		o.width += 0.5f;
		o.height += 0.5f;
	}

	for (int i = 0; i < text.size(); ++i)
	{
		auto geometry = g.GetFactory().CreatePathGeometry();
		auto sink = geometry.Open();
		const auto c = text[i];
		switch (c)
		{
		case ' ':
			break;

		case 'L':
		{
			auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
			{
				{ // RIAA for pixels
					auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
					const auto imageSize = bitmapMem.GetSize();
					const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

					uint8_t* sourcePixels = pixelsSource.getAddress();

					// clear bitmap
					int x = 0;
					int y = 0;
					int32_t* pixel = (int32_t*)sourcePixels;
					for (auto i = 0; i < totalPixels; ++i)
					{
						if (7 == cellHeight)
						{
							const int charIndex = 0;
							if ((ascii_7_px[charIndex][y] >> (char_width - x)) & 1)
							{
								*pixel = 0xffffffff;
							}
							else
							{
								*pixel = 0;
							}
						}
						else
						{
							if (x < pixelSize || y >= cellHeight - pixelSize)
							{
								*pixel = 0xffffffff;
							}
							else
							{
								*pixel = 0;
							}
						}

						// next;
						if (++x == char_width)
						{
							x = 0;
							++y;
						}

						++pixel;
					}
				}

				g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
			}
		}
		//g.DrawLine(tl + o, bl + o, brush, lineWidth);
		//g.DrawLine(bl + o, br + o, brush, lineWidth);
		break;

		case 'O':
			//g.DrawLine(tl + o, bl + o, brush, lineWidth);
			//g.DrawLine(tr + o, br + o, brush, lineWidth);
			//g.DrawLine(tl + o, tr + o, brush, lineWidth);
			//g.DrawLine(bl + o, br + o, brush, lineWidth);
		{
			auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
			{
				{ // RIAA for pixels
					auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
					const auto imageSize = bitmapMem.GetSize();
					const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

					uint8_t* sourcePixels = pixelsSource.getAddress();

					// clear bitmap
					int x = 0;
					int y = 0;
					int32_t* pixel = (int32_t*)sourcePixels;
					for (auto i = 0; i < totalPixels; ++i)
					{
						const bool isVert = x < pixelSize || x >= char_width - pixelSize;
						const bool isHoriz = y < pixelSize || y >= cellHeight - pixelSize;
						// for very small letters, don't cut out corners
						if ((!pixelCutoutSize && (isVert || isHoriz)) || (isVert ^ isHoriz))
						{
							*pixel = 0xffffffff;
						}
						else
						{
							*pixel = 0;
						}

						// next;
						if (++x == char_width)
						{
							x = 0;
							++y;
						}
						++pixel;
					}
				}

				g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
			}
		}
		break;

		case 'D':
#if 0
			sink.BeginFigure(bl + o);
			sink.AddLine(tl + o);
			sink.AddLine(trl + o);
			if (pixelSize > 2)
			{
				sink.AddArc(ArcSegment(trd + o, cornerSize));
			}
			else
			{
				sink.AddLine(trd + o);
			}
			sink.AddLine(bru + o);
			if (pixelSize > 2)
			{
				sink.AddArc(ArcSegment(brl + o, cornerSize));
			}
			else
			{
				sink.AddLine(brl + o);
			}
			sink.AddLine(bl + o);
			sink.EndFigure();
#endif
			{
				auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
				{
					{ // RIAA for pixels
						auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
						const auto imageSize = bitmapMem.GetSize();
						const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

						uint8_t* sourcePixels = pixelsSource.getAddress();

						// clear bitmap
						int x = 0;
						int y = 0;
						int32_t* pixel = (int32_t*)sourcePixels;
						for (auto i = 0; i < totalPixels; ++i)
						{
							const bool isVert = x < pixelSize || x >= char_width - pixelSize;
							const bool isHoriz = y < pixelSize || y >= cellHeight - pixelSize;
							if (x < pixelSize || (isVert ^ isHoriz))
							{
								*pixel = 0xffffffff;
							}
							else
							{
								*pixel = 0;
							}

							// next;
							if (++x == char_width)
							{
								x = 0;
								++y;
							}
							++pixel;
						}
					}

					g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
				}
			}
			break;

		case 'U':
#if 0
			sink.BeginFigure(tr + o);
			sink.AddLine(bru + o);
			sink.AddArc(ArcSegment(brl + o, cornerSize));
			sink.AddLine(blr + o);
			sink.AddArc(ArcSegment(blu + o, cornerSize));
			sink.AddLine(tl + o);
			sink.EndFigure(FigureEnd::Open);
#endif
			{
				auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
				{
					{ // RIAA for pixels
						auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
						const auto imageSize = bitmapMem.GetSize();
						const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

						uint8_t* sourcePixels = pixelsSource.getAddress();

						// clear bitmap
						int x = 0;
						int y = 0;
						int32_t* pixel = (int32_t*)sourcePixels;
						for (auto i = 0; i < totalPixels; ++i)
						{
							const bool isVert = x < pixelSize || x >= char_width - pixelSize;
							const bool isHoriz = /* y < pixelSize || */ y >= cellHeight - pixelSize;
							//if (isVert ^ isHoriz)
							if ((!pixelCutoutSize && (isVert || isHoriz)) || (isVert ^ isHoriz))
							{
								*pixel = 0xffffffff;
							}
							else
							{
								*pixel = 0;
							}

							// next;
							if (++x == char_width)
							{
								x = 0;
								++y;
							}
							++pixel;
						}
					}

					g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
				}
			}
			break;

		case 'S':
#if 0
			g.DrawLine(tl + o, tr + o, brush, lineWidth);
			g.DrawLine(tl + o, ml + o, brush, lineWidth);
			g.DrawLine(ml + o, mr + o, brush, lineWidth);
			g.DrawLine(mr + o, br + o, brush, lineWidth);
			g.DrawLine(bl + o, br + o, brush, lineWidth);
#endif
			{
				auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
				{
					{ // RIAA for pixels
						auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
						const auto imageSize = bitmapMem.GetSize();
						const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

						uint8_t* sourcePixels = pixelsSource.getAddress();

						// clear bitmap
						int x = 0;
						int y = 0;
						int32_t* pixel = (int32_t*)sourcePixels;
						for (auto i = 0; i < totalPixels; ++i)
						{
							const bool isCenterBar = ((y + pixelSize - 1) / pixelSize) == (cellHeight / 2) / pixelSize;
							const bool isHoriz = y < pixelSize || y >= cellHeight - pixelSize || isCenterBar;

							const bool isVert = (((y + pixelSize - 1) / pixelSize) <= (cellHeight / 2) / pixelSize && x < pixelSize) ||
								(((y + pixelSize - 1) / pixelSize) >= (cellHeight / 2) / pixelSize && x >= char_width - pixelSize);

							//							const int middlePixel = ((1 + cellHeight) / 2) / pixelSize;
							//							const bool isMiddle = y / pixelSize == middlePixel;

							//							const bool isVert = y / pixelSize < middlePixel ? x < pixelSize : x >= char_width - pixelSize;
							//							const bool isCenterBar = ( x > pixelSize || x <= char_width - pixelSize) && isMiddle;
														// for very small letters, don't cut out corners
														//if (isVert ^ isHoriz)
							if ((!pixelCutoutSize && (isVert || isHoriz)) || (isVert ^ isHoriz))
							{
								*pixel = 0xffffffff;
							}
							else
							{
								*pixel = 0;
							}

							// next;
							if (++x == char_width)
							{
								x = 0;
								++y;
							}
							++pixel;
						}
					}

					g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
				}
			}

			break;

		case 'N':
			//g.DrawLine(tl + o, bl + o, brush, lineWidth);
			//g.DrawLine(tl + o, br + o, brush, lineWidth);
			//g.DrawLine(tr + o, br + o, brush, lineWidth);

		{
			auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
			{
				{ // RIAA for pixels
					auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
					const auto imageSize = bitmapMem.GetSize();
					const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

					uint8_t* sourcePixels = pixelsSource.getAddress();

					// clear bitmap
					int x = 0;
					int y = 0;
					int32_t* pixel = (int32_t*)sourcePixels;
					for (auto i = 0; i < totalPixels; ++i)
					{
						const bool isVert = x < pixelCutoutSize || x >= char_width - pixelCutoutSize;
						const bool isDiag = x / pixelSize == y / pixelSize;
						//						const bool isHoriz = y < pixelCutoutSize || y >= cellHeight - pixelCutoutSize;
						if (isVert || isDiag) // ^ isHoriz)
						{
							*pixel = 0xffffffff;
						}
						else
						{
							*pixel = 0;
						}

						// next;
						if (++x == char_width)
						{
							x = 0;
							++y;
						}
						++pixel;
					}
				}

				g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
			}
		}
		break;

		case 'E':
		{
			auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
			{
				{ // RIAA for pixels
					auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
					const auto imageSize = bitmapMem.GetSize();
					const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

					uint8_t* sourcePixels = pixelsSource.getAddress();

					// clear bitmap
					int x = 0;
					int y = 0;
					int32_t* pixel = (int32_t*)sourcePixels;
					for (auto i = 0; i < totalPixels; ++i)
					{
						const bool isVert = x < pixelSize;
						const bool isCenterBar = x < char_width - pixelSize && ((y + pixelSize - 1) / pixelSize) == (cellHeight / 2) / pixelSize;
						const bool isHoriz = y < pixelSize || y >= cellHeight - pixelSize || isCenterBar;
						// for very small letters, don't cut out corners
						if (isVert || isHoriz)
						{
							*pixel = 0xffffffff;
						}
						else
						{
							*pixel = 0;
						}

						// next;
						if (++x == char_width)
						{
							x = 0;
							++y;
						}
						++pixel;
					}
				}

				g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
			}
		}
		break;

		default:
#if 0
			g.DrawLine(tl + o, bl + o, brush, lineWidth);
			g.DrawLine(tl + o, tr + o, brush, lineWidth);
			g.DrawLine(ml + o, mr + o, brush, lineWidth);
			g.DrawLine(bl + o, br + o, brush, lineWidth);
#endif
			break;
#if 0
			{
				auto bitmapMem = g.GetFactory().CreateImage(char_width, cellHeight);
				{
					{ // RIAA for pixels
						auto pixelsSource = bitmapMem.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE);
						const auto imageSize = bitmapMem.GetSize();
						const int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

						uint8_t* sourcePixels = pixelsSource.getAddress();

						// clear bitmap
						int x = 0;
						int y = 0;
						int32_t* pixel = (int32_t*)sourcePixels;
						for (auto i = 0; i < totalPixels; ++i)
						{
							bool light = false;

							if (x < 10 && y < 13)
							{
								light = (letters[c - 32][y] >> x) & 1;
							}
							if (light)
							{
								*pixel = 0xffffffff;
							}
							else
							{
								*pixel = 0;
							}

							// next;
							if (++x == char_width)
							{
								x = 0;
								++y;
							}
							++pixel;
						}
					}

					g.DrawBitmap(bitmapMem, Point(x, y), Rect(0.f, 0.f, (float)char_width, (float)cellHeight));
				}
			}
#endif
		}

		sink.Close();
		g.DrawGeometry(geometry, brush, lineWidth);

		o.width += char_width + spacing;
		x += char_width + spacing;
	}
}