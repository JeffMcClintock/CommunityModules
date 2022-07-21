#include "leegame.h"

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


void lees_game::drawFrame(Graphics& g)
{
	g.Clear(Color::Aqua);
	auto brush = g.CreateSolidColorBrush(Color::Black);
	g.FillCircle (canonball,5,brush);
	g.FillRectangle(105,100,125,110,brush);
	g.FillRectangle(110, 92.5, 120, 100, brush);
	canonball.x = canonball.x + dx;//how fast canonball gos in a horzontal

	
		canonball.y = canonball.y - dy;
	
		dy = dy - 0.08;
	frame_num = frame_num + 1;
}