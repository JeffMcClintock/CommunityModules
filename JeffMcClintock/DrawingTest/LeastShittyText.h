#include "Drawing.h"

static const int MAX_WIDTH = 32;
typedef uint8_t canvas[MAX_WIDTH][MAX_WIDTH];

class gridPen
{
	canvas& grid;
	int x = 0;
	int y = 0;

public:
	gridPen(canvas& pgrid) : grid(pgrid)
	{
	}

	void clear()
	{
//		std::fill(std::begin(grid), std::end(grid), 0);
		for (int y = 0; y < MAX_WIDTH; ++y)
		{
			for (int x = 0; x < MAX_WIDTH; ++x)
			{
				grid[x][y] = 0;
			}
		}
	}

	void plot(int px, int py, uint8_t c = 255)
	{
		grid[px][py] = (std::max)(c, grid[px][py]);
	}

	void start(int px, int py)
	{
		x = px;
		y = py;
		plot(x, y);
	}

	void lineto(int px, int py)
	{
		while (x != px || y != py)
		{
			const auto dx = (std::max)(-1, (std::min)(1, px - x));
			const auto dy = (std::max)(-1, (std::min)(1, py - y));
			x += dx;
			y += dy;
			plot(x, y);

			if (dx && dy) // diagonal smoothing
			{
				plot(x - dx, y, 80);
				plot(x, y - dy, 80);
			}
		}
	}
};

class SmallText
{
public:
	static const int MAX_WIDTH = 32;
	canvas grid;
	canvas prev;

	GmpiDrawing::Bitmap bitmapMem;

	void DrawTextShitty(GmpiDrawing::Graphics& g, const std::string& text, GmpiDrawing::Rect& rect, GmpiDrawing::Brush brush);
	void DrawTextShitty3(GmpiDrawing::Graphics& g, const std::string& text, GmpiDrawing::Rect& rect, GmpiDrawing::Brush brush);
};