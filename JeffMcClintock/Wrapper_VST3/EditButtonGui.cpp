#include "./EditButtonGui.h"
#include "../shared/unicode_conversion.h"
#include "ControllerWrapper.h"
#include "myPluginProvider.h"
#include "VstFactory.h"

using namespace gmpi;
using namespace JmUnicodeConversions;
using namespace gmpi_gui_api;

int32_t EditButtonGui::initialize()
{
	// obtain our other half (the Controller) from the factory. We share the same host-assigned handle.
	controller_ = GetVstFactory()->getController(getHandle());

	return MpGuiBase2::initialize();
}

int32_t EditButtonGui::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
	// Let host handle right-clicks.
	if( ( flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON ) == 0 )
	{
		return gmpi::MP_UNHANDLED;
	}

	if (!controller_)
	{
		return gmpi::MP_UNHANDLED;
	}

	setCapture();
	invalidateRect();

	return gmpi::MP_OK;
}

int32_t EditButtonGui::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
{
	if( !getCapture() )
	{
		return gmpi::MP_UNHANDLED;
	}

	releaseCapture();
	invalidateRect();

	controller_->OpenGui();

	return gmpi::MP_OK;
}

int32_t EditButtonGui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	GmpiDrawing::Graphics g(drawingContext);

	auto r = getRect();

	// Background Fill.
	const uint32_t backgroundColor = getCapture() ? 0xE8E8Eff : 0xE8E8E8u;
	auto brush = g.CreateSolidColorBrush(backgroundColor);
	g.FillRectangle(r, brush);

	// Outline.
	brush.SetColor(0x969696u);
	g.DrawRectangle(r, brush);

	// Current selection text.
	brush.SetColor(GmpiDrawing::Color(0x000032u));

	std::string txt = "EDIT";

	if (!controller_ || !controller_->plugin->controller)
	{
		txt = "LOADFAIL";
		brush.SetColor(GmpiDrawing::Color::Red);
	}

	GmpiDrawing::Rect textRect(r);
	textRect.Deflate(border);

	g.DrawTextU(txt, getTextFormat(), textRect, brush);

	return gmpi::MP_OK;
}

GmpiDrawing::TextFormat& EditButtonGui::getTextFormat()
{
	if( dtextFormat.isNull() )
	{
		const float fontSize = 14;
		dtextFormat = GetGraphicsFactory().CreateTextFormat(fontSize);
		dtextFormat.SetTextAlignment(GmpiDrawing::TextAlignment::Leading); // Left
		dtextFormat.SetParagraphAlignment(GmpiDrawing::ParagraphAlignment::Center);
	}

	return dtextFormat;
}

int32_t EditButtonGui::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	auto font = getTextFormat();

	auto s = getTextFormat().GetTextExtentU("XXXEDITXXX");

	returnDesiredSize->width = s.width;
	returnDesiredSize->height = s.height;

	return gmpi::MP_OK;
}

