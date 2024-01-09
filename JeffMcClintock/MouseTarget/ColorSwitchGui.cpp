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

class ColorSwitchGui final : public SeGuiInvisibleBase
{
	static constexpr int fixedPinCount = 2;

	void onSetResult()
	{
		const int selectionPin = pinSelect;
		if (selectionPin >= 0 && selectionPin < (int)pinIns.size())
		{
			pinOut = pinIns[selectionPin]->getValue();
		}
	}

 	IntGuiPin pinSelect;
	MpGuiPin<int64_t> pinOut;
	std::vector< std::unique_ptr< MpGuiPin<int64_t> > > pinIns;

public:
	ColorSwitchGui()
	{
		initializePin(pinOut);
		initializePin(pinSelect, static_cast<MpGuiBaseMemberPtr2>(&ColorSwitchGui::onSetResult));
	}

	int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override
	{
		int plugIndex = pinId - fixedPinCount; // Calc index of autoduplicating pin.

		// Add autoduplicate pins as needed.
		while ((int)pinIns.size() <= plugIndex)
		{
			pinIns.push_back(std::make_unique< MpGuiPin<int64_t> >());
			initializePin(*pinIns.back());
		}

		auto result = MpGuiBase2::setPin(pinId, voice, size, data);

		onSetResult();

		return result;
	}
};

namespace
{
	auto r = Register<ColorSwitchGui>::withId(L"SE Color Switch");
}

class BlobSwitchGui final : public SeGuiInvisibleBase
{
	static constexpr int fixedPinCount = 2;

	void onSetResult()
	{
		const int selectionPin = pinSelect;
		if (selectionPin >= 0 && selectionPin < (int)pinIns.size())
		{
			pinOut = pinIns[selectionPin]->getValue();
		}
	}

	IntGuiPin pinSelect;
	BlobGuiPin pinOut;
	std::vector< std::unique_ptr< BlobGuiPin > > pinIns;

public:
	BlobSwitchGui()
	{
		initializePin(pinOut);
		initializePin(pinSelect, static_cast<MpGuiBaseMemberPtr2>(&BlobSwitchGui::onSetResult));
	}

	int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override
	{
		int plugIndex = pinId - fixedPinCount; // Calc index of autoduplicating pin.

		// Add autoduplicate pins as needed.
		while ((int)pinIns.size() <= plugIndex)
		{
			pinIns.push_back(std::make_unique< BlobGuiPin >());
			initializePin(*pinIns.back());
		}

		auto result = MpGuiBase2::setPin(pinId, voice, size, data);

		onSetResult();

		return result;
	}
};

namespace
{
	auto r2 = Register<BlobSwitchGui>::withId(L"SE Blob Switch");
}