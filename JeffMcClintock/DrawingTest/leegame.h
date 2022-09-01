#include "Drawing.h"

static const int MAX_WIDTH2 = 32;
typedef uint8_t canvas2[MAX_WIDTH2][MAX_WIDTH2];

class gridPen2
{
	canvas2& grid;
	int x = 0;
	int y = 0;

public:
	gridPen2(canvas2& pgrid) : grid(pgrid)
	{
	}

	void clear()
	{
//		std::fill(std::begin(grid), std::end(grid), 0);
		for (int y = 0; y < MAX_WIDTH2; ++y)
		{
			for (int x = 0; x < MAX_WIDTH2; ++x)
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

			if (dx && dy) // diagonal
			{
				plot(x - dx, y, 80);
				plot(x, y - dy, 80);
			}
		}
	}
};



class lees_game
{
public:
	static const int MAX_WIDTH2 = 32;
//	canvas2 grid;
//	canvas2 prev;
	lees_game();

	bool terrain[300][300] = {};

	GmpiDrawing::Point canonball = { 30, 485 };
	GmpiDrawing::Point tank_sprite = { 100, 100 };
	int frame_num = 0;
	GmpiDrawing::Bitmap bitmapMem;
	float dy = 2;//how fast a canonball gos up
	float dx = 4;
	void drawFrame(GmpiDrawing::Graphics& g);
};