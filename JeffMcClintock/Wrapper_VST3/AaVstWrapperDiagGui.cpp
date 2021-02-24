#include "./AaVstWrapperDiagGui.h"
#include "Drawing.h"
#include "VstFactory.h"

using namespace gmpi;
using namespace GmpiDrawing;

int32_t AaVstWrapperDiagGui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext )
{
	Graphics g(drawingContext);

	auto r = getRect();
	ClipDrawingToBounds x(g, r);

	auto brush = g.CreateSolidColorBrush(Color::White);
	g.FillRectangle(r, brush);

	auto textFormat = GetGraphicsFactory().CreateTextFormat();
	brush.SetColor(Color::Black);

	g.DrawTextU(GetVstFactory()->getDiagnostics(), textFormat, r, brush);

	return gmpi::MP_OK;
}

