#include <math.h>

#include "../se_sdk3/mp_sdk_audio.h"

// New. 0v = 0.001s, 10V = 10s
float VoltageToTime(float v) { return powf(10.0f, ((v) * 0.4f) - 3.0f); }

using namespace gmpi;

#define SCHEME2 1

class Adsr : public MpBase2
{
	BoolInPin pinTrigger;
	BoolInPin pinGate;
	AudioInPin pinAttack;
	AudioInPin pinDecay;
	AudioInPin pinSustain;
	AudioInPin pinRelease;
	AudioInPin pinAttackCurve;
	AudioInPin pinDecayCurve;
	AudioInPin pinReleaseCurve;
	AudioOutPin pinSignalOut;
	FloatInPin pinVoiceReset;

	float level_ = {};
	float curveRate_ = {};
	float target_ = {};
	float CurveTarget_ = {};
	int cur_segment = -1;
	float legalLow = {};

public:
	Adsr()
	{
		initializePin( pinTrigger );
		initializePin( pinGate );
		initializePin( pinAttack );
		initializePin( pinDecay );
		initializePin( pinSustain );
		initializePin( pinRelease );
		initializePin( pinAttackCurve );
		initializePin( pinDecayCurve );
		initializePin( pinReleaseCurve );
		initializePin( pinSignalOut );
		initializePin( pinVoiceReset );
	}

	void subProcess( int sampleFrames )
	{
		float* out = getBuffer(pinSignalOut);

		for (int s = 0; s < sampleFrames; ++s)
		{
#ifndef SCHEME2
			// IDEALLY we could check after increment so level_ NEVER goes out-of-bounds (else glitchyness results)
			// but not switch segment till next sample, mayby a bool.
			if (level_ < legalLow || level_ > 1.0f)
			{
				level_ = target_; // undo overshoot.

				TempBlockPositionSetter x(this, getBlockPosition() + s);
				next_segment();
			}

			assert(level_ > -100.0f && level_ < 100.0f);

			*out++ = level_;
			level_ += curveRate_ * (CurveTarget_ - level_);
		}
		/* hmmm
				// mitigate any unchecked overshoot, which could result in subprocess stepping abbruptly to the end segment.
				if (level_ < legalLow || level_ > 1.0f)
				{
					level_ = target_;
				}
		*/
#else
			level_ += curveRate_ * (CurveTarget_ - level_);
			if (level_ < legalLow || level_ > 1.0f)
			{
				level_ = target_; // undo overshoot.

				TempBlockPositionSetter x(this, getBlockPosition() + s);
				next_segment();
			}

			*out++ = level_;
		}
#endif
	}

	void next_segment() // Called when envelope section ends
	{
		++cur_segment;

		switch (cur_segment)
		{
		case 0:
			calcCurve(pinAttack, pinAttackCurve, 1.0f);
#ifndef SCHEME2

			// provide first step.
			level_ += curveRate_ * (CurveTarget_ - level_);
#endif
			break;

		case 1:
			calcCurve(pinDecay, pinDecayCurve, pinSustain);
			break;

		case 2:
			curveRate_ = CurveTarget_ = 0.0f;
			pinSignalOut.setStreaming(false);
			break;

		case 3:
			calcCurve(pinRelease, pinReleaseCurve, 0.0f);
#ifndef SCHEME2

			// provide first step.
			level_ += curveRate_ * (CurveTarget_ - level_);
#endif
			break;

		default:
			cur_segment = -1;
			level_ = curveRate_ = CurveTarget_ = 0.0f;
			pinSignalOut.setStreaming(false);
			break;
		};
	}

	void calcCurve(float rate, float curveAmmount, float target)
	{
		pinSignalOut.setStreaming(true);

		target_ = target;
		rate = (std::min)(2.0f, rate);

		float deltaY = target - level_;

		deltaY = fabsf(deltaY);

		// By using more or less of the curve we control straight/curved mix.
		float timeConstants = 10.0f * fabsf(curveAmmount);

		// prevent divide-by-zero;
		const float smallNumber = 0.001;
		timeConstants = max(timeConstants, smallNumber); // Prevent divide-by-zero.

		float deltaT = deltaY * getSampleRate() * VoltageToTime(rate * 10.f);
		deltaT = (std::max)(deltaT, 1.0f); // prevent divide-by-zero.

		float stepSize = timeConstants / deltaT;
		if (curveAmmount > 0.0f)
		{
			// first step.
			curveRate_ = 1.0f - expf(-stepSize);
		}
		else
		{
			// 1 before first step.
			curveRate_ = expf(stepSize) - 1.0f;
		}

		// what level does the curve reach after x timeconstants. (We will scale curve to reach 1.0 at this time)
		float valueAtTime1 = 1.0f - expf(-timeConstants);
		CurveTarget_ = deltaY / valueAtTime1; // level curve 'aims' for.

		CurveTarget_ -= deltaY; // relative to end-level.
		assert(CurveTarget_ > 0.0f);

		if (target_ <= level_)
		{
			legalLow = target_;
			CurveTarget_ = -CurveTarget_;
		}
		else
		{
			legalLow = 0.0f;
		}

		if (curveAmmount >= 0) // normal curve.
		{
			// make target relative to segment end level.
			CurveTarget_ = target_ + CurveTarget_;
		}
		else // inverted curve.
		{
			curveRate_ = -curveRate_;

			// make target relative to segment start level.
			CurveTarget_ = level_ - CurveTarget_; // no, not instantaneous level, as subject to modulation, and other weirdness.
		}
	}

	void onSetPins(void) override
	{
		if (&Adsr::subProcess != getSubProcess())
		{
			setSubProcess(&Adsr::subProcess);
			pinSignalOut.setStreaming(false); // first time.
		}

		// Check which pins are updated.
		if( pinTrigger.isUpdated() && pinTrigger.getValue() && pinGate.getValue())
		{
#if 0
			// mitigate any unchecked overshoot, which will result in subprocess stepping abbruptly to the end segment.
			if (level_ < legalLow || level_ > 1.0f)
			{
				level_ = target_;
			}
#endif				

			cur_segment = -1;
			next_segment();
		}

		if( pinGate.isUpdated() && !pinGate.getValue())
		{
			if (cur_segment > -1 && cur_segment < 3)
			{
#if 0
				// mitigate any unchecked overshoot, which will result in subprocess stepping abbruptly to the end segment.
				if (level_ < legalLow || level_ > 1.0f)
				{
					level_ = target_;
				}
#endif				
				cur_segment = 2;
				next_segment();
			}
		}

		if (pinVoiceReset.isUpdated() && pinVoiceReset == 1.0f) // forcedReset
		{
			// _RPT1(_CRT_WARN, "Envelope:: Hard Reset. %d\n", (int) pinVoiceReset  );
			cur_segment = 3;
			next_segment();
		}
	}
};

namespace
{
	auto r = Register<Adsr>::withId(L"SE ADSR4");
}
