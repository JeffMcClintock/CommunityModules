
/* Copyright (c) 2007-2021 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SEM, nor SynthEdit, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SynthEdit Ltd ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SynthEdit Ltd BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "mp_sdk_gui2.h"
#include "Drawing.h"
#include <list>
#include <vector>

using namespace gmpi;
using namespace GmpiDrawing;

struct captureChunk
{
	captureChunk(int nothing) : signal(0), signal_peak(0) {}
	float* signal;
	float* signal_peak;
};

inline void SimplifyGraph(const std::vector<Point>& in, std::vector<Point>& out)
{
	out.clear();

	if (in.size() < 2)
	{
		out = in;
		return;
	}

	const float tollerance = 0.3f;

	float slope{};
	bool first = true;
	Point prev{};

	for (int i = 0; i < in.size(); ++i)
	{
		if (first)
		{
			prev = in[i];
			out.push_back(prev);

			assert(i != in.size() - 1); // should never be last one?

//			if (i != in.size() - 1) // last one?
			{
				slope = (in[i + 1].y - prev.y) / (in[i + 1].x - prev.x);
				++i; // next one can be assumed to fit the prediction (so skip it).
				first = false;
			}
		}
		else
		{
			const float predictedY = prev.y + slope * (in[i].x - prev.x);
			const float err = in[i].y - predictedY;

			if (err > tollerance || err < -tollerance)
			{
				i -= 2; // insert prev in 'out', then recalc slope
				first = true;
			}
		}
	}

	if (out.back() != in.back())
	{
		out.push_back(in.back());
	}
}

inline PathGeometry DataToGraph(Graphics& g, const std::vector<Point>& inData)
{
	const float tollerance = 0.3f;

	auto geometry = g.GetFactory().CreatePathGeometry();
	auto sink = geometry.Open();
	bool first = true;
	for (const auto& p : inData)
	{
		if (first)
		{
			sink.BeginFigure(p);
			first = false;
		}
		else
		{
			sink.AddLine(p);
		}
	}

	sink.EndFigure(FigureEnd::Open);
	sink.Close();

	return geometry;
}

class SignalLoggerGui final : public gmpi_gui::MpGuiGfxBase
{
	const int maxRetainedChunks = 100;

	struct TraceChunk
	{
		int chunkIndex;
		int channelCount;
		std::vector<float> data;
	};

	std::list<TraceChunk> chunks;

	std::vector<Point> temp1; // to save reallocating every frame, we keep these arround.
	std::vector<Point> temp2;

	void onSetBlob()
	{
		if (pinValueIn.rawSize() > 4)
		{
			const float* data = (const float*)pinValueIn.rawData();

			const auto chunkIndex = static_cast<int>(data[0]);
			const auto channelCount = static_cast<int>(data[1]);

			if (chunkIndex == 0) // restarted audio?
				chunks.clear();

			chunks.push_back({ chunkIndex, channelCount, {} });
			const float* chunkData = data + 2;
			const size_t chunkSize = (pinValueIn.rawSize() / sizeof(float)) - 2;
			chunks.back().data.assign(chunkData, chunkData + chunkSize);
		}

		if (maxRetainedChunks < chunks.size())
		{
			chunks.pop_front();
		}

		invalidateRect();
	}

	BlobGuiPin	pinValueIn;

	FloatGuiPin pinZoom_X;
	FloatGuiPin pinZoom_Y;
	FloatGuiPin pinPan_X;
	FloatGuiPin pinPan_Y;

public:
	SignalLoggerGui()
	{
		initializePin(pinValueIn, static_cast<MpGuiBaseMemberPtr2>(&SignalLoggerGui::onSetBlob));
		initializePin(pinZoom_X);
		initializePin(pinZoom_Y);
		initializePin(pinPan_X);
		initializePin(pinPan_Y);
	}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		Graphics g(drawingContext);
		ClipDrawingToBounds x(g, getRect());

		g.Clear(Color::Black);

		auto textFormat = GetGraphicsFactory().CreateTextFormat();
		auto brush = g.CreateSolidColorBrush(Color::Lime);

		auto r = getRect();

		if (chunks.empty())
			return gmpi::MP_OK;

		float height = r.bottom - r.top;
		float width = r.right - r.left;

		float mid_y = floorf(0.5f + height * 0.5f);

		// calc transform to screen
		auto transform = Matrix3x2::Scale(-pinZoom_X, -height * 0.46f * pinZoom_Y, { 0,0 });
		transform = transform * Matrix3x2::Translation(width, mid_y);

		// draw grid
		{
			auto screen_mid_right = transform.TransformPoint({ 0, 0 });
			screen_mid_right.x = width;
			Point screen_mid_left{ 0, screen_mid_right.y };
			brush.SetColor(Color::Gray);
			g.DrawLine(screen_mid_left, screen_mid_right, brush);
		}

		const int traceCount = chunks.back().channelCount;
		const int framesPerChunk = chunks.back().data.size() / traceCount;

		Color traceColors[] =
		{
			Color::Lime,
			Color::Yellow,
			Color::Red,
			Color::LightCyan,
		};

		for (int trace = 0; trace < traceCount; ++trace)
		{
			temp1.clear();
			temp2.clear();

			Point p{};
			Point p_screen{};

			for (auto it = chunks.rbegin(); it != chunks.rend(); ++it)
			{
				auto& chunk = *it;

				for (int i = 0 ; i < framesPerChunk; ++i)
				{
					//chunk.data.size() - traceCount + trace;
					//int index = i * traceCount + trace;
					p.y = chunk.data[chunk.data.size() - traceCount * (i+1) + trace];

					if (isnan(p.y) || isinf(p.y))
					{
						p.y = 100.0; // Nans should stick out.
					}

					p_screen = transform.TransformPoint(p);

					temp1.push_back(p_screen);

					p.x = p.x + 1.0f;

					if (p_screen.x < 0)
						break;
				}

				if (p_screen.x < 0)
					break;
			}

			SimplifyGraph(temp1, temp2);

			const float penWidth = 1;
			auto traceGeometry = DataToGraph(g, temp2);

			brush.SetColor(traceColors[trace % std::size(traceColors)]);
			g.DrawGeometry(traceGeometry, brush, penWidth);
		}

		return gmpi::MP_OK;
	}

	void Reset() {}
};

namespace
{
	auto r = Register<SignalLoggerGui>::withId(L"SE Signal Logger");
}
