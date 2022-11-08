#include "leegame.h"
#include <stdlib.h>
#include <time.h>       /* time */

using namespace GmpiDrawing;
uint16_t tank_sprite[1][7] =
{
	{   // L
		0b0000000000000000,
		0b0000000000000000,
		0b0000000000000000,
		0b0000001111000000,
		0b0001111111111000,
		0b0001111111111000,
		0b0000011001100000,
	},

};

lees_game::lees_game()
{
//	for (int x = 0; x < 300; x = x + 1)
	//	terrain[x][27] = true;


	srand(time(NULL));
	{
//		for (int y = 26; y > 0; y = y - 1)
		int y = 200;
		{
			for (int x = 0; x < 300; x = x + 1)
			{
//				if (terrain[x][y + 1])
				{
					if (x > 75 && x < 140 )


					{
						int r = rand() % 4;
						if (r == 0)
						{
							y = y - 3;
						}
						if (r == 2)
						{
							y = y - 2;
						}
						if (r == 3)
						{
							y = y + 1;
						}
						if (r == 1)
						{
							y = y - 2;
						}
					}
					else
						if (x > 160 && x < 225)


						{
							int r = rand() % 4;
							if (r == 0)
							{
								y = y + 3;
							}
							if (r == 2)
							{
								y = y + 2;
							}
							if (r == 3)
							{
								y = y - 1;
							}
							if (r == 1)
							{
								y = y + 2;
							}
						}
						else
					{
						int r = rand() % 2;
						if (r == 0)
						{
							y = y + 1;
						}
						if (r == 1)
						{
							y = y - 1;
						}


					}

						if (y > 260)
							y = 259;
						for (int y2 = y; y2 < 300; y2 = y2 + 1)
							terrain[x][y2] = true;
					
				}
			}
		}
	}
}

void lees_game::onClick(GmpiDrawing_API::MP1_POINT point)
{ 
	if (point.y > 1 && point.y < 20)
	{
		canonball.x = 105;
		canonball.y = 574;
		dy = 9.5;
		dx = 2;
	}
	
	else
	{
		canonball.y = point.y;
		canonball.x = point.x;
		dy = 2;
	}
	
}

void lees_game::drawFrame(Graphics& g)
{


	auto brush = g.CreateSolidColorBrush(Color::Chocolate);
//	g.Clear(Color::Aqua);
	// draw all the brown blocks
	for (int y = 0; y < 300; y = y + 1)
	
	{
		for (int x = 0; x < 300; x = x + 1)
		{
			if (terrain[x][y] == true)
			{

				g.FillRectangle(x * 2, y * 2, x * 2 + 2, y * 2 + 2, brush);
			}
		}
	}

	brush.SetColor(Color::Green);
	//
	for (int y = 0; y < 30; y = y + 1)
		
	{
		for (int x = 0; x < 30; x = x + 1)
		{
			if (terrain[x][y] == false)
			{
				if (terrain[x][y + 1] == true)
				{
					g.FillRectangle(x * 2, y * 2, x * 2 + 2, y * 2 + 2, brush);
				}
				
			}
		}
	}

	

	brush.SetColor(Color::Black);

	g.FillCircle (canonball,5,brush);
	g.FillRectangle(105, 500,125, 490,brush);
	g.FillRectangle(110, 485.5, 120, 490, brush);

	g.FillRectangle(155, 500, 175, 490, brush);
	g.FillRectangle(160, 485.5, 170, 490, brush);
	canonball.x = canonball.x + dx;//how fast canonball gos in a horzontal

	
		canonball.y = canonball.y - dy;
	
		dy = dy - 0.08;
	frame_num = frame_num + 1;
}