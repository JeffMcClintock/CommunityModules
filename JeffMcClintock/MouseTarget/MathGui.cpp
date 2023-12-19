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
#include <numeric>
#include "mp_sdk_gui2.h"

using namespace gmpi;

template<class T>
class MultiplyGui final : public SeGuiInvisibleBase
{
	static constexpr int fixedPinCount = 1;
	MpGuiPin<T> pinResult;
	std::vector< std::unique_ptr< MpGuiPin<T> > > pinIns;

public:
	MultiplyGui()
	{
		initializePin(pinResult);
	}

	int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override
	{
		int plugIndex = pinId - fixedPinCount; // Calc index of autoduplicating pin.

		// Add autoduplicate pins as needed.
		while ((int)pinIns.size() <= plugIndex)
		{
			pinIns.push_back(std::make_unique< MpGuiPin<T> >());
			initializePin(*pinIns.back());
		}

		auto result = MpGuiBase2::setPin(pinId, voice, size, data);

		T sum{1};

		for (auto& it : pinIns)
		{
			sum *= it->getValue();
		};

		pinResult = pinIns.empty() ? T{} : sum;

		return result;
	}
};
namespace
{
	auto r = Register<MultiplyGui<int32_t> >::withId(L"SE MathGui Multiply");
}

template<class T>
class AdderGui final : public SeGuiInvisibleBase
{
	static constexpr int fixedPinCount = 1;
	MpGuiPin<T> pinResult;
	std::vector< std::unique_ptr< MpGuiPin<T> > > pinIns;

public:
	AdderGui()
	{
		initializePin(pinResult);
	}

	int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override
	{
		int plugIndex = pinId - fixedPinCount; // Calc index of autoduplicating pin.

		// Add autoduplicate pins as needed.
		while ((int)pinIns.size() <= plugIndex)
		{
			pinIns.push_back(std::make_unique< MpGuiPin<T> >());
			initializePin(*pinIns.back());
		}

		auto result = MpGuiBase2::setPin(pinId, voice, size, data);

		T sum{};

		for (auto& it : pinIns)
		{
			sum += it->getValue();
		};

		pinResult = sum;

		return result;
	}
};

namespace
{
	auto r2 = Register< AdderGui<int32_t> >::withId(L"SE MathGui Add");
}

class Bin2IntGui final : public SeGuiInvisibleBase
{
	static constexpr int fixedPinCount = 1;
	IntGuiPin pinResult;
	std::vector< std::unique_ptr< MpGuiPin<bool> > > pinIns;

public:
	Bin2IntGui()
	{
		initializePin(pinResult);
	}

	int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override
	{
		const int plugIndex = pinId - fixedPinCount; // Calc index of autoduplicating pin.

		// Add autoduplicate pins as needed.
		while ((int)pinIns.size() <= plugIndex)
		{
			pinIns.push_back(std::make_unique< MpGuiPin<bool> >());
			initializePin(*pinIns.back());
		}

		auto result = MpGuiBase2::setPin(pinId, voice, size, data);

		int32_t sum{};
		int32_t mask{ 1 };
		for (auto& it : pinIns)
		{
			sum |= it->getValue() ? mask : 0;
			mask <<= 1;
		};

		pinResult = sum;

		return result;
	}
};

namespace
{
	auto r3 = Register<Bin2IntGui>::withId(L"SE MathGui Bin2Int");
}