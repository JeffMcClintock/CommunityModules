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

using namespace gmpi;

class FloatsToColorGui final : public SeGuiInvisibleBase
{
	FloatGuiPin pinR;
	FloatGuiPin pinG;
	FloatGuiPin pinB;
	FloatGuiPin pinA;
	BlobGuiPin pinColor;

	void onSetValueIn()
	{
		const GmpiDrawing::Color c(pinR, pinG, pinB, pinA);

		MpBlob b{ sizeof(GmpiDrawing::Color), &c };

		pinColor = b;
	}

public:
	FloatsToColorGui()
	{
		initializePin(pinR, static_cast<MpGuiBaseMemberPtr2>(&FloatsToColorGui::onSetValueIn));
		initializePin(pinG, static_cast<MpGuiBaseMemberPtr2>(&FloatsToColorGui::onSetValueIn));
		initializePin(pinB, static_cast<MpGuiBaseMemberPtr2>(&FloatsToColorGui::onSetValueIn));
		initializePin(pinA, static_cast<MpGuiBaseMemberPtr2>(&FloatsToColorGui::onSetValueIn));
		initializePin( pinColor );
	}
};

namespace
{
	auto r = Register<FloatsToColorGui>::withId(L"SE Floats to Color");
}
