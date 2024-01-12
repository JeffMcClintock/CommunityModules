/* Copyright (c) 2007-2023 SynthEdit Ltd

Permission to use, copy, modify, and /or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include "mp_sdk_gui2.h"
#include "unicode_conversion.h"
#include "Drawing.h"
#include "./tinyXml2/tinyxml2.h"

using namespace gmpi;
using namespace tinyxml2;
using namespace GmpiDrawing;

class SvgTocppGui final : public gmpi_gui::MpGuiGfxBase
{
	StringGuiPin pinSvgFilename;
	StringGuiPin pinCppSourceCodeOut;

	SizeL svgSize{};
	tinyxml2::XMLDocument doc;

	void onSetTextVal()
	{
		auto imageFileName = JmUnicodeConversions::WStringToUtf8(pinSvgFilename);

		doc.LoadFile(imageFileName.c_str());

		if (doc.Error())
			return;

		auto svgE = doc.FirstChildElement("svg");

		if (!svgE)
			return;

		svgSize = {};
		svgE->QueryIntAttribute("width", &svgSize.width);
		svgE->QueryIntAttribute("height", &svgSize.height);
	}

	int32_t OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
	{
		GmpiDrawing::Graphics g(drawingContext);

		auto svgE = doc.FirstChildElement("svg");

		if (!svgE)
			return gmpi::MP_OK;

		auto fillBrush = g.CreateSolidColorBrush(Color::Black);
		auto strokeBrush = g.CreateSolidColorBrush(Color::Black);

		for (auto node = svgE->FirstChildElement(); node; node = node->NextSiblingElement())
		{
			auto c = node->ToElement();

			if (!c)
				continue;

			Color fillColor;
			Color strokeColor;
			fillColor.a = strokeColor.a = 0.0f; // default to not drawn

			auto fill(c->Attribute("fill"));
			if (fill && fill[0] == '#')
			{
				std::string fillstr(fill + 1);
				fillColor = Color::FromHexStringU(fillstr);
			}
			fillBrush.SetColor(fillColor);

			auto stroke(c->Attribute("stroke"));
			if (stroke && stroke[0] == '#')
			{
				std::string strokestr(stroke + 1);
				strokeColor = Color::FromHexStringU(strokestr);
			}
			strokeBrush.SetColor(strokeColor);

			if (strcmp(c->Name(), "rect") == 0)
			{
				RoundedRect r{};

				//				Rect r{};
				r.rect.left = c->FloatAttribute("x");
				r.rect.top = c->FloatAttribute("y");
				r.rect.right = r.rect.left + c->FloatAttribute("width");
				r.rect.bottom = r.rect.top + c->FloatAttribute("height");

				//				Size radius{};
				r.radiusX = c->FloatAttribute("rx");
				r.radiusY = c->FloatAttribute("ry", r.radiusX);

				if (fillColor.a > 0)
				{
					g.FillRoundedRectangle(r, fillBrush);
				}

				if (strokeColor.a > 0)
				{
					g.DrawRoundedRectangle(r, strokeBrush);
				}
			}
			else if (strcmp(c->Name(), "path") == 0)
			{
				auto d = c->Attribute("d");
				if (d)
				{
					// split the string at spaces and non-numbers into strokes
					struct strokeToken
					{
						char strokeType;
						std::vector<float> args;
					};

					std::vector<strokeToken> tokens;
					{
						for (auto p = d; *p; ++p)
						{
							if (isalpha(*p))
							{
								tokens.push_back({ *p });
							}
							else if (isdigit(*p) || *p == '-' || *p == '.')
							{
								if (!tokens.empty())
								{
									auto& token = tokens.back();
									char* pEnd{};
									token.args.push_back(strtod(p, &pEnd));
									p = pEnd - 1;
								}
							}
						}
					}

					// render the strokes
					PathGeometry fillPath;
					GeometrySink fillSink;
					if (fillColor.a > 0)
					{
						fillPath = g.GetFactory().CreatePathGeometry();
						fillSink = fillPath.Open();
					}
					PathGeometry strokePath;
					GeometrySink strokeSink;
					if (strokeColor.a > 0)
					{
						strokePath = g.GetFactory().CreatePathGeometry();
						strokeSink = strokePath.Open();
					}

					Point last{};
					bool inFigure{};

					for (const auto& t : tokens)
					{
						switch (t.strokeType)
						{
						case 'M': // Move-to
						{
							if (t.args.size() != 2)
								break;

							if (inFigure)
							{
								if(strokeSink)
									strokeSink.EndFigure(FigureEnd::Open);
								if(fillSink)
									fillSink.EndFigure(FigureEnd::Closed);
							}

							last = { t.args[0], t.args[1] };

							if (strokeSink)
							{
								strokeSink.BeginFigure(last, FigureBegin::Hollow);
							}
							if (fillSink)
							{
								fillSink.BeginFigure(last, FigureBegin::Filled);
							}
						}
						break;

						case 'L': // Line-to
						{
							for (int i = 0; i < t.args.size(); i += 2)
							{
								last = { t.args[i], t.args[i + 1] };

								if (strokeSink)
									strokeSink.AddLine(last);
								if (fillSink)
									fillSink.AddLine(last);
							}
						}
						break;

						case 'Q': // Quadratic Bezier
						{
							if(t.args.size() != 4)
								break;

							Point pt1{t.args[0], t.args[1]};
							last = {t.args[2], t.args[3]};

							if (strokeSink)
								strokeSink.AddQuadraticBezier({ pt1, last });
							if (fillSink)
								fillSink.AddQuadraticBezier({ pt1, last });
						}
						break;

							/* todo
						case 'A': // Arc
						{
							if(t.args.size() != 7)
								break;

							Point pt1{t.args[0], t.args[1]};
							Point pt2{t.args[2], t.args[3]};
							float radius = t.args[4];
							bool largeArc = t.args[5] != 0;
							bool sweep = t.args[6] != 0;

							sink.AddArc({ pt1, pt2 }, radius, largeArc, sweep);
						}
							*/

						case 'H': // Horizontal Line
						{
							if(t.args.size() != 1)
								break;

							last.x = t.args[0];

							if (strokeSink)
								strokeSink.AddLine(last);
							if (fillSink)
								fillSink.AddLine(last);
						}
						break;

						case 'V': // Vertical Line
						{
							if(t.args.size() != 1)
								break;

							last.y = t.args[0];
							if (strokeSink)
								strokeSink.AddLine(last);
							if (fillSink)
								fillSink.AddLine(last);
						}
						break;

						case 'C': // Cubic Bezier
						{
							if(t.args.size() != 6)
								break;

							Point pt1{t.args[0], t.args[1]};
							Point pt2{t.args[2], t.args[3]};
							last = {t.args[4], t.args[5]};

							if (strokeSink)
								strokeSink.AddBezier({ pt1, pt2, last });
							if (fillSink)
								fillSink.AddBezier({ pt1, pt2, last });
						}
						break;

						case 'Z': // Close
						{
							if (strokeSink)
								strokeSink.EndFigure();
							if (fillSink)
								fillSink.EndFigure();

							inFigure = false;
						}
						break;

						default:
							assert(false); // TODO
							break;
						}
					}

					if (strokeSink)
						strokeSink.Close();
					if (fillSink)
						fillSink.Close();

					if (fillSink)
					{
						g.FillGeometry(fillPath, fillBrush);
					}

					if (strokeSink)
					{
						g.DrawGeometry(strokePath, strokeBrush);
					}						
				}
			}
			else if (strcmp(c->Name(), "circle") == 0)
			{
				Point center{};
				center.x = c->FloatAttribute("cx");
				center.y = c->FloatAttribute("cy");

				float radius = c->FloatAttribute("r");

				if (fillColor.a > 0)
				{
					g.FillCircle(center, radius, fillBrush);
				}

				if (strokeColor.a > 0)
				{
					g.DrawCircle(center, radius, strokeBrush);
				}
			}
			else if (strcmp(c->Name(), "ellipse") == 0)
			{
				Point center{};
				center.x = c->FloatAttribute("cx");
				center.y = c->FloatAttribute("cy");

				Size radius{};
				radius.width = c->FloatAttribute("rx");
				radius.height = c->FloatAttribute("ry");

				if (fillColor.a > 0)
				{
					g.FillEllipse({ center, radius.width, radius.height }, fillBrush);
				}

				if (strokeColor.a > 0)
				{
					g.DrawEllipse({ center, radius.width, radius.height }, strokeBrush);
				}
			}
			else if (strcmp(c->Name(), "line") == 0)
			{
				Point start{};
				start.x = c->FloatAttribute("x1");
				start.y = c->FloatAttribute("y1");

				Point end{};
				end.x = c->FloatAttribute("x2");
				end.y = c->FloatAttribute("y2");

				g.DrawLine(start, end, strokeBrush);
			}
			else if (strcmp(c->Name(), "polyline") == 0)
			{
				std::string points(c->Attribute("points"));
				if (!points.empty())
				{
					std::vector<Point> points2;
					auto p = points.c_str();
					while (*p)
					{
						Point pt{};
						pt.x = atof(p);
						while (*p && *p != ',')
							++p;
						if (*p)
							++p;
						pt.y = atof(p);
						while (*p && *p != ' ')
							++p;
						if (*p)
							++p;
						points2.push_back(pt);
					}

					g.DrawLines(points2.data(), points.size(), strokeBrush);
				}
			}
		}
		return gmpi::MP_OK;
	}

public:
	SvgTocppGui()
	{
		initializePin(pinSvgFilename, static_cast<MpGuiBaseMemberPtr2>(&SvgTocppGui::onSetTextVal));
		initializePin(pinCppSourceCodeOut);
	}
};

namespace
{
	auto r = Register<SvgTocppGui>::withId(L"SE SVG to C++");
}
