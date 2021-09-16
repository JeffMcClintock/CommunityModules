#include "./AaVstWrapperDiagGui.h"
#include "Drawing.h"
#include "VstFactory.h"

using namespace gmpi;
using namespace GmpiDrawing;

AaVstWrapperDiagGui::AaVstWrapperDiagGui()
{
}

int32_t AaVstWrapperDiagGui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext )
{
	Graphics g(drawingContext);

	auto r = getRect();
	ClipDrawingToBounds x(g, r);
	
	auto brush = g.CreateSolidColorBrush(Color::White);
	g.FillRectangle(r, brush);

	auto textFormat = GetGraphicsFactory().CreateTextFormat();
	brush.SetColor(Color::Black);

	g.DrawTextU(GetVstFactory()->getDiagnostics(), textFormat, 0.0f, 0.0f, brush);

	return gmpi::MP_OK;
}

