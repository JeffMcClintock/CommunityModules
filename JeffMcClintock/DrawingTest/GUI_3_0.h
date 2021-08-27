#include "Drawing.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <variant>
#include <functional>

class vBrush
{
	GmpiDrawing::SolidColorBrush brush;
	GmpiDrawing::Color color;

public:
	vBrush(GmpiDrawing::Color c) : color(c)
	{}

	GmpiDrawing::SolidColorBrush& get(GmpiDrawing::Graphics& g)
	{
		// TODO cache it based on factory.
		brush = g.CreateSolidColorBrush(color);
		return brush;
	}
};

class vCircleGeometry
{
	GmpiDrawing::PathGeometry geometry;

	GmpiDrawing::Point center;
	float radius;

public:
	vCircleGeometry(GmpiDrawing::Point c, float r) : center(c), radius(r)
	{}

	GmpiDrawing::PathGeometry& get(GmpiDrawing::Graphics& g)
	{
		// TODO cache it based on factory.
		geometry = g.GetFactory().CreatePathGeometry();
		const float pi = M_PI;
		auto sink = geometry.Open();

		// make a circle from two half-circle arcs
		sink.BeginFigure({ center.x, center.y - radius }, GmpiDrawing::FigureBegin::Filled);
		sink.AddArc({ { center.x, center.y + radius}, { radius, radius }, pi });
		sink.AddArc({ { center.x, center.y - radius }, { radius, radius }, pi });

		sink.EndFigure(GmpiDrawing::FigureEnd::Closed);
		sink.Close();
		return geometry;
	}
};

class functionalUI
{
    using state_t = std::variant< float, GmpiDrawing::Point, GmpiDrawing::Color, vCircleGeometry>;//, vBrush>;// GmpiDrawing::SolidColorBrush, GmpiDrawing::PathGeometry > ;

	struct node
	{
		std::function < state_t(std::vector<state_t*>) > function;
		std::vector<state_t*> arguments;
		state_t result; // maybe could support multiple outputs: std::vector<state_t> results;

		void operator()()
		{
			result = function(arguments);
		}
	};

	struct renderNode
	{
//		std::function < state_t(std::vector<state_t*>, GmpiDrawing::Graphics& g) > function;
		std::function < void(std::vector<state_t*>, GmpiDrawing::Graphics& g) > function;
		std::vector<state_t*> arguments;
//		state_t result;

		void operator()(GmpiDrawing::Graphics& g)
		{
			/*result = */ function(arguments, g);
		}
	};

	std::vector<node> graph;
	std::vector<renderNode> renderNodes;

public:
	std::vector< state_t > states;
	state_t mousePosition = GmpiDrawing::Point();
	state_t frameNumber = 0.0f;
	state_t mouseDown = 0.0f;

	void init();
	void draw(GmpiDrawing::Graphics& g);
	void step();
};
