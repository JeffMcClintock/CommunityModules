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
#include "TimerManager.h"

using namespace gmpi;
using namespace GmpiDrawing;

class ColorLagGui final : public SeGuiInvisibleBase, public TimerClient
{
	static constexpr float steps = 5.0f;

 	void onSetIn()
	{
		if (pinIn.rawSize() == sizeof(target))
		{
			prev = target;

			memcpy(&target, pinIn.rawData(), sizeof(target)); // endian matter?

			step = 0.0f;

			StartTimer();
		}
	}

	BlobGuiPin pinIn;
 	BlobGuiPin pinOut;

	Color prev;
	Color target;
	float step = 1.0f;

public:
	ColorLagGui()
	{
		initializePin(pinIn, static_cast<MpGuiBaseMemberPtr2>(&ColorLagGui::onSetIn) );
		initializePin(pinOut);
	}

	bool OnTimer() override
	{
		step += 1.0f;

		Color current;
		current.a = prev.a + (target.a - prev.a) * step / steps;
		current.r = prev.r + (target.r - prev.r) * step / steps;
		current.g = prev.g + (target.g - prev.g) * step / steps;
		current.b = prev.b + (target.b - prev.b) * step / steps;

		MpBlob b{ sizeof(current), &current };
		pinOut = b;

		return step < steps;
	}
};

namespace
{
	auto r = Register<ColorLagGui>::withId(L"SE Color Lag");
}
