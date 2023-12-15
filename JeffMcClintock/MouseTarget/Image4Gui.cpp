#include "./Image4Gui.h"
#include "../shared/unicode_conversion.h"

using namespace gmpi;
using namespace gmpi_gui;
using namespace GmpiDrawing;

GMPI_REGISTER_GUI(MP_SUB_TYPE_GUI2, Image4Gui, L"Image4");
SE_DECLARE_INIT_STATIC_FILE(Image4_Gui);

Image4Gui::Image4Gui()
{
	initializePin(pinFilename, static_cast<MpGuiBaseMemberPtr2>(&Image4Gui::onSetFilename));
	initializePin(pinAnimationPosition, static_cast<MpGuiBaseMemberPtr2>( &Image4Gui::calcDrawAt ));
	initializePin(pinFrameCount);
}

void Image4Gui::onSetFilename()
{
	auto imageFile = JmUnicodeConversions::WStringToUtf8(pinFilename);
//	Load(imageFile);
	if (MP_OK == skinBitmap::Load(getHost(), getGuiHost(), imageFile.c_str()))
	{
		onLoaded();

		getGuiHost()->invalidateMeasure();
		calcDrawAt();
		//reDraw();
		invalidateRect();
	}
}

int32_t Image4Gui::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	GmpiDrawing::Graphics dc2(drawingContext);

	renderBitmap(dc2, SizeU(0, 0));

	return gmpi::MP_OK;
}

void Image4Gui::setAnimationPos(float p)
{
	pinAnimationPosition = p;
}

void Image4Gui::calcDrawAt()
{
	if (skinBitmap::calcDrawAt(getAnimationPos()))
		invalidateRect();
}

void Image4Gui::onLoaded()
{
	if (bitmapMetadata_)
	{
		int fc = bitmapMetadata_->getFrameCount();
		pinFrameCount = fc;
	}
}

int32_t Image4Gui::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	if( !bitmap_.isNull() )
	{
		*returnDesiredSize = bitmapMetadata_->getPaddedFrameSize();
	}
	else
	{
		returnDesiredSize->width = 10;
		returnDesiredSize->height = 10;
	}

	return gmpi::MP_OK;
}
