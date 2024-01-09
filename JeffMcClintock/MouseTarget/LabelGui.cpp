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
#include "Drawing.h"

using namespace gmpi;
using namespace GmpiDrawing;

class LabelGui final : public gmpi_gui::MpGuiGfxBase
{
	StringGuiPin pinText;
	BlobGuiPin pinColor;
	std::wstring decodedString;
	TextFormat textFormat;
	bool containsSymbols = {}; // support for Sego MDL2 Assets font.

	// Sego MDL2 Assets font.
	bool requiresMDL2Font(wchar_t charCode) const
	{
		return (charCode >= 0xE700 && charCode < 0xF900);
	}
	void redraw()
	{
		invalidateRect();
	}

	void update()
	{
		invalidateRect();

		containsSymbols = false;

		decodedString = pinText;

		// parse unicode characters on html format. e.g. &#9824; of &#x2660; (hex)
		size_t p = 0;
		while (true)
		{
			p = decodedString.find('&', p);
			if (p == std::string::npos)
				break;

			auto p2 = decodedString.find(';', p);
			if (p2 == std::string::npos)
				break;

			auto len = p2 - p;
			if (len < 2 || len > 7 || decodedString[p + 1] != '#')
			{
				p++;
				continue;
			}

			char16_t charCode{};
			if (decodedString[p + 2] == 'x') // hex
			{
				charCode = static_cast<char16_t>(std::stoi(decodedString.substr(p + 3, p2 - p - 3), nullptr, 16));
			}
			else // decimal
			{
				charCode = static_cast<char16_t>(std::stoi(decodedString.substr(p + 2, p2 - p - 2), nullptr, 10));
			}
			// replace with unicode character.
			decodedString.replace(p, p2 - p + 1, 1, charCode);
			p++;
		}

		for (auto& charCode : decodedString)
		{
			if (requiresMDL2Font(charCode))
			{
				containsSymbols = true;
				break;
			}
		}
	}

public:
	LabelGui()
	{
		initializePin(pinText, static_cast<MpGuiBaseMemberPtr2>(&LabelGui::update));
		initializePin(pinColor, static_cast<MpGuiBaseMemberPtr2>(&LabelGui::redraw));
	}

	int32_t MP_STDCALL arrange(GmpiDrawing_API::MP1_RECT finalRect) override
	{
		textFormat.setNull();
		return gmpi_gui::MpGuiGfxBase::arrange(finalRect);
	}

	int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext ) override
	{
		Graphics g(drawingContext);

		if (!textFormat)
		{
			const auto size = getRect().getSize();
			Factory::FontStack fontStack;
			if (containsSymbols)
			{
				fontStack.fontFamilies_.push_back("Segoe MDL2 Assets");
			}
			textFormat = GetGraphicsFactory().CreateTextFormat2(size.height, fontStack);
			textFormat.SetTextAlignment(TextAlignment::Center);
		}

		Color color;
		if (pinColor.rawSize() == sizeof(color))
		{
			memcpy(&color, pinColor.rawData(), sizeof(color)); // endian matter?
		}

		auto brush = g.CreateSolidColorBrush(color);

		g.DrawTextW(decodedString, textFormat, getRect(), brush);

		return gmpi::MP_OK;
	}
};

namespace
{
	auto r = Register<LabelGui>::withId(L"SE Dumb Label");
}
