#include "./VstwrapperfailGui.h"
#include "Drawing.h"

using namespace gmpi;
using namespace GmpiDrawing;

//GMPI_REGISTER_GUI(MP_SUB_TYPE_GUI2, VstwrapperfailGui, L"SE VSTWrapperFail" );

VstwrapperfailGui::VstwrapperfailGui(std::string pErrorMsg) : errorMsg(pErrorMsg)
{
}

int32_t VstwrapperfailGui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext )
{
	Graphics g(drawingContext);

	const auto r = getRect();
	ClipDrawingToBounds x(g, r);

	auto textFormat = GetGraphicsFactory().CreateTextFormat();
	auto brush = g.CreateSolidColorBrush(Color::Red);

	g.DrawTextU(errorMsg, textFormat,r,brush);

	return gmpi::MP_OK;
}

