#include "Drawing.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <variant>
#include <functional>
#include "GUI_3_0.h"

using namespace GmpiDrawing;

void functionalUI::init()
{
	// reference, what we're trying to achieve
#if 0
	Color color(Color::Bisque);
	auto brush = g.CreateSolidColorBrush(color);
	const Point center(100.0f, 100.0f);
	const float radius = 50.0f;
	//	g.FillCircle(center, radius, brush);

		// create circle geometry the hard way.
	auto geometry = g.GetFactory().CreatePathGeometry();
	{
		const float pi = M_PI;
		auto sink = geometry.Open();

		// make a circle from two half-circle arcs
		sink.BeginFigure({ center.x, center.y - radius }, FigureBegin::Filled);
		sink.AddArc({ { center.x, center.y + radius}, { radius, radius }, pi });
		sink.AddArc({ { center.x, center.y - radius }, { radius, radius }, pi });

		sink.EndFigure(FigureEnd::Closed);
		sink.Close();
	}

	g.FillGeometry(geometry, brush);
#endif

	///////////////////////////////////////////////////////////////////////////////////
	// NOTES.
	// need to have history of updates to the graph. could be immutable.

#if 0 // using single-arg function signature, not flexible enough.
	using state_t = std::variant< float, Point, Color, SolidColorBrush, PathGeometry>;
	std::vector< state_t > states;

	states.push_back(Color(Color::Bisque));		// color
	states.push_back(Point(100.0f, 100.0f));	// center
	states.push_back(50.0f);					// radius

	std::vector< std::function<state_t(state_t, Graphics& g)> > nodes;

	// Create brush.
	nodes.push_back(
		[](state_t color, Graphics& g) -> state_t
	{
		return g.CreateSolidColorBrush(std::get<Color>(color));
	}
	);

	// Create geometry.
	nodes.push_back(
		[](state_t s_center, Graphics& g) -> state_t
	{
		auto center = std::get<Point>(s_center);
		float radius = 50.0f; // TODO!!!
		auto geometry = g.GetFactory().CreatePathGeometry();
		const float pi = M_PI;
		auto sink = geometry.Open();

		// make a circle from two half-circle arcs
		sink.BeginFigure({ center.x, center.y - radius }, FigureBegin::Filled);
		sink.AddArc({ { center.x, center.y + radius}, { radius, radius }, pi });
		sink.AddArc({ { center.x, center.y - radius }, { radius, radius }, pi });

		sink.EndFigure(FigureEnd::Closed);
		sink.Close();
		return geometry;
	}
	);

	// Paint geometry with brush.
	nodes.push_back(
		[](state_t s_geometry, Graphics& g) -> state_t
	{
		auto geometry = std::get<PathGeometry>(s_geometry);

		//. TODO!!!
		Color color(Color::Bisque);
		auto brush = g.CreateSolidColorBrush(color);

		g.FillGeometry(geometry, brush);
		return 0.0f;
	}
	);

	// execute graph
	//	auto r = nodes[2](nodes[1](nodes[0](states[0], g), g),g);

	// execute graph at runtime
	auto r = states[0];
	for (auto& method : nodes)
	{
		// not quite right, passes brush to create genometery		r = method(r, g);
	}
#endif
#if 1

	states.push_back(Color(Color::Yellow));	// color
	states.push_back(Point(100.0f, 100.0f));	// center
	states.push_back(20.0f);					// radius
	states.push_back(0.0f);						// brush placeholder
	states.push_back(0.0f);						// geometry placeholder
	states.push_back(0.0f);						// drawing result placeholder

#if 0
	// Create brush.
	nodes.push_back(
		[](std::vector<state_t*> states, Graphics& g) -> state_t
	{
		return g.CreateSolidColorBrush(std::get<Color>(*states[0]));
	}
	);

	// Create geometry.
	nodes.push_back(
		[](std::vector<state_t*> states, Graphics& g) -> state_t
	{
		const auto center = std::get<Point>(*states[0]);
		const auto radius = std::get<float>(*states[1]); // TODO!!!

		auto geometry = g.GetFactory().CreatePathGeometry();
		const float pi = M_PI;
		auto sink = geometry.Open();

		// make a circle from two half-circle arcs
		sink.BeginFigure({ center.x, center.y - radius }, FigureBegin::Filled);
		sink.AddArc({ { center.x, center.y + radius}, { radius, radius }, pi });
		sink.AddArc({ { center.x, center.y - radius }, { radius, radius }, pi });

		sink.EndFigure(FigureEnd::Closed);
		sink.Close();
		return geometry;
	}
	);

	// Paint geometry with brush.
	nodes.push_back(
		[](std::vector<state_t*> states, Graphics& g) -> state_t
	{
		const auto brush = std::get<SolidColorBrush>(*states[0]);
		auto geometry = std::get<PathGeometry>(*states[1]);

		g.FillGeometry(geometry, brush);
		return 0.0f;
	}
	);
#endif
	// create graph of dependencies.


	// 0. brush
	graph.push_back(
		{
			[](std::vector<state_t*> states) -> state_t
			{
//				return g.CreateSolidColorBrush(std::get<Color>(*states[0]));
				return vBrush(std::get<Color>(*states[0]));
			},
			{&states[0]},
			0.0f
		}
	);

	// 1. Mouse down changes center
	graph.push_back(
		{
		[](std::vector<state_t*> states) -> state_t
			{
				const auto mouseDown = std::get<float>(*states[0]);
				const auto mousePosition = std::get<Point>(*states[1]);
				const auto currentCenter = std::get<Point>(*states[2]);

				return mouseDown > 0.5f ? mousePosition : currentCenter; // -> outputs a point
			},
			{&mouseDown, &mousePosition, &states[1]}, // <- Inputs
			0.0f
		}
	);

	// 2. circle geometry
	graph.push_back(
		{
		[](std::vector<state_t*> states) -> state_t
			{
				const auto center = std::get<Point>(*states[0]);
				const auto radius = std::get<float>(*states[1]);

				return vCircleGeometry(center, radius);
			},
			{
				&states[0], //&graph[1].result, // connect calculated point to circle center. !!PRoblem - using wrong object as placeholder is confusing.
				&states[2]
			},
			0.0f
		}
	);

	// 3. wave animation
	graph.push_back(
		{
			[this](std::vector<state_t*> states) -> state_t
			{
				auto frame = std::get<float>(*states[0]);
				return 21.0f + 20.0f * sinf(frame * 0.1f);
			},
			{&frameNumber},
			0.0f
		}
	);

	// 4. update state[1] for next frame from calculated center point.
	graph.push_back(
		{
			[this](std::vector<state_t*> states) -> state_t
			{
				auto input = std::get<Point>(*states[0]);
				//auto output = std::get<Point>(*states[1]);
				//output = input;
/*
				state_t a = Point(0.0f, 0.0f);
				state_t b = a;

				state_t& c = *states[0];
				state_t& d = *states[1];
				d = c;
*/
				*states[1] = input; // copy input to output

				return input;
			},
			{&states[0] /*&graph[1].result */, &states[1]}, // !!PRoblem - using states[0] only as a placeholder
			0.0f
		}
	);

	// AFTER graph contructed (no more reallocs). You can connect from other graph objects.

	// connect wave animation to circle radius
	graph[2].arguments[1] = (&graph[3].result);

	// connect calculated point to circle center
	graph[2].arguments[0] = (&graph[1].result);

	// connect calculated point to circle center
	graph[4].arguments[0] = (&graph[1].result);

	// 0. render
	renderNodes.push_back(
		{
			[](std::vector<state_t*> states, Graphics& g) -> void //state_t
			{
				auto brush = std::get<vBrush>(*states[0]);
				auto geometry = std::get<vCircleGeometry>(*states[1]);

				g.FillGeometry(geometry.get(g), brush.get(g));
//				return 0.0f;
			},
			{&graph[0].result, &graph[2].result}//, // !!! Caution when states come from same container. outer vector might reallocate address of these when pushed.
//			0.0f
		}
	);

#endif
}

void functionalUI::step()
{
	// should advance 'frame number' state, then propagate changes through graph.
	frameNumber = std::get<float>(frameNumber) + 1.0f;

	for (auto& n : graph)
	{
		n();
	}
}

void functionalUI::draw(Graphics& g)
{
	for (auto& n : renderNodes)
	{
		n(g);
	}

	// maybe output of final step should be a 'renderer' that can be called during painting.
	// so the graph's purpose is to reactivly produce a 'renderer' that does the actual drawing for the view.


	// should *not* procress graph, only call renderer/s.
}