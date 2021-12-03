
/* Copyright (c) 2007-2021 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SEM, nor SynthEdit, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SynthEdit Ltd ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SynthEdit Ltd BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "../se_sdk3/mp_sdk_audio.h"
#include "../shared/xp_simd.h"

using namespace gmpi;
#define interpolationExtraSamples 3

// backward compatible for Metafilter
inline void ModulationAnalogCalculate_BC(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
{
	float targetReadOffset = (1.0f - *modulation) * buffer_size;

	// prevent divide by zero. Reverse logic to catch Nan.
	if (!(targetReadOffset >= 2.0f))
	{
		targetReadOffset = 2.0f;
	}

	readOffset += 1.0f - (readOffset / targetReadOffset); // Waves tape delay effect.

	read_offset_int = FastRealToIntTruncateTowardZero(readOffset);
	read_offset_fine = readOffset - (float)read_offset_int;

	if (read_offset_int < 2) // need to leave 1 trailing sample for interpolation.
	{
		read_offset_int = 2;
		read_offset_fine = 0.0f;
	}
	else
	{
		if (read_offset_int > buffer_size - 2)
		{
			read_offset_int = buffer_size - 2;
			read_offset_fine = 0.0f;
		}
	}
	++modulation;
}

inline void ModulationAnalogCalculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
{
	// unnesc 'max'? float targetReadOffset = std::max( 1.0f, (1.0f - *modulation ) * buffer_size );
	float targetReadOffset = *modulation * buffer_size;

	// prevent divide by zero. Reverse logic to catch Nan.
	if (!(targetReadOffset >= 1.0f))
	{
		targetReadOffset = 1.0f;
	}

	readOffset += 1.0f - (readOffset / targetReadOffset); // Waves tape delay effect.

	read_offset_int = FastRealToIntTruncateTowardZero(readOffset);
	read_offset_fine = readOffset - (float)read_offset_int;

	if (read_offset_int <= 0) // indicated index *might* be less than zero. e.g. Could be 0.1 which is valid, or -0.1 which is not.
	{
		if (!(readOffset >= 0.0f)) // reverse logic to catch Nans.
		{
			read_offset_int = 0;
			read_offset_fine = 0.0f;
		}
	}
	else
	{
		if (read_offset_int > buffer_size)
		{
			read_offset_int = buffer_size;
			read_offset_fine = 0.0f;
		}
	}
	++modulation;
};

inline void ModulationDigitalCalculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
{
	readOffset = *modulation * buffer_size;
	read_offset_int = FastRealToIntTruncateTowardZero(readOffset);
	read_offset_fine = readOffset - (float)read_offset_int;

	if (read_offset_int <= 0) // indicated index *might* be less than zero. e.g. Could be 0.1 which is valid, or -0.1 which is not.
	{
		if (!(readOffset >= 0.0f)) // reverse logic to catch Nans.
		{
			read_offset_int = 0;
			read_offset_fine = 0.0f;
		}
	}
	else
	{
		if (read_offset_int >= buffer_size)
		{
			read_offset_int = buffer_size;
			read_offset_fine = 0.0f;
		}
	}
	++modulation;
};

inline void ModulationDigitalCalculate_BC(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
{
	readOffset = (1.0f - *modulation) * buffer_size;
	read_offset_int = FastRealToIntTruncateTowardZero(readOffset);
	read_offset_fine = readOffset - (float)read_offset_int;

	if (read_offset_int < 2) // need to leave 1 trailing sample for interpolation.
	{
		read_offset_int = 2;
		read_offset_fine = 0.0f;
	}
	else
	{
		if (read_offset_int > buffer_size - 2)
		{
			read_offset_int = buffer_size - 2;
			read_offset_fine = 0.0f;
		}
	}
	++modulation;
};

class PolicyModulationAnalogChanging
{
public:
	inline static void CalculateInitial(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
	inline static void Calculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		ModulationAnalogCalculate(modulation, buffer_size, readOffset, read_offset_int, read_offset_fine);
	}

	enum { Active = true };
};

class PolicyModulationAnalogChanging_BC
{
public:
	inline static void CalculateInitial(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
	inline static void Calculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		ModulationAnalogCalculate_BC(modulation, buffer_size, readOffset, read_offset_int, read_offset_fine);
	}

	enum { Active = true };
};


class PolicyModulationDigitalChanging
{
public:
	inline static void CalculateInitial(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
	inline static void Calculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		ModulationDigitalCalculate(modulation, buffer_size, readOffset, read_offset_int, read_offset_fine);
	}
	enum { Active = true };
};

class PolicyModulationDigitalChanging_BC
{
public:
	inline static void CalculateInitial(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
	inline static void Calculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		ModulationDigitalCalculate_BC(modulation, buffer_size, readOffset, read_offset_int, read_offset_fine);
	}
	enum { Active = true };
};

class PolicyModulationFixed
{
public:
	inline static void CalculateInitial(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		ModulationDigitalCalculate(modulation, buffer_size, readOffset, read_offset_int, read_offset_fine);
	}

	inline static void Calculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
	enum { Active = false };
};

class PolicyModulationFixed_BC
{
public:
	inline static void CalculateInitial(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		ModulationDigitalCalculate_BC(modulation, buffer_size, readOffset, read_offset_int, read_offset_fine);
	}

	inline static void Calculate(float*& modulation, int buffer_size, float& readOffset, int& read_offset_int, float& read_offset_fine)
	{
		// do nothing. Hopefully optimizes away to nothing.
	}
	enum { Active = false };
};

class PolicyFeedbackOff
{
public:
	inline static float Calculate(float prev_out, float*& feedback)
	{
		return 0.0f;
	};
};
class PolicyFeedbackModulated
{
public:
	inline static float Calculate(float prev_out, float*& feedback)
	{
		return prev_out * *feedback++;
	};
};

class PolicyInterpolationCubic
{
public:
	inline static float Calculate(int count, float* buffer, int read_offset, float fraction, int padded_buffer_size)
	{
		int index = count - read_offset;

		if (index < 2)
		{
			index += padded_buffer_size;
		}

		// Buffer has 3 extra samples 'off-end' filled with duplicate samples from buffer start. Avoid need to handle wrapping.
		float y0 = buffer[index + 1];
		float y1 = buffer[index + 0];
		float y2 = buffer[index - 1];
		float y3 = buffer[index - 2];

		// Can't read right up to current sample with cubic interpolation, so switch to linear.
		if (read_offset < 1)
		{
			if (read_offset < 0)
				return y1;
			else
				return y1 + fraction * (y2 - y1);
		}

		assert(index >= 0 && index < padded_buffer_size + interpolationExtraSamples);

		return y1 + 0.5f * fraction * (y2 - y0 + fraction * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + fraction * (3.0f * (y1 - y2) + y3 - y0)));
	}
};

class PolicyInterpolationCubic_BC
{
public:
	inline static float Calculate(int count, float* buffer, int read_offset, float fraction, int padded_buffer_size)
	{
		int index = count + read_offset - 1;

		if (index >= padded_buffer_size - interpolationExtraSamples)
		{
			index -= padded_buffer_size - interpolationExtraSamples;
		}

		assert(index >= 0 && index < padded_buffer_size);

		// Buffer has 3 extra samples 'off-end' filled with duplicate samples from buffer start. Avoid need to handle wrapping.
		float y0 = buffer[index + 0];
		float y1 = buffer[index + 1];
		float y2 = buffer[index + 2];
		float y3 = buffer[index + 3];

		return y1 + 0.5f * fraction * (y2 - y0 + fraction * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + fraction * (3.0f * (y1 - y2) + y3 - y0)));
	}
};

// Not used, but here if needed.
class PolicyInterpolationLinear
{
public:
	inline static float Calculate(int count, float* buffer, int read_offset, float read_offset_fine, int buffer_size)
	{
		if (read_offset < 0)
		{
			return buffer[0];
		}

		int index = count - read_offset - 1;

		if (index < 0)
		{
			index += buffer_size;
		}

		assert(index >= 0 && index < buffer_size + interpolationExtraSamples);

		// Buffer has 3 extra samples 'off-end' filled with duplicate samples from buffer start. Avoid need to handle wrapping.
		float y0 = buffer[index + 0];
		float y1 = buffer[index + 1];

		//		return y0 + read_offset_fine * (y1 - y0);
		return y1 + read_offset_fine * (y0 - y1);
	};
};

class PolicyInterpolationNone
{
public:
	inline static float Calculate(int count, float* buffer, int read_offset, float read_offset_fine, int buffer_size)
	{
		int index = count - read_offset;
		if (index < 0)
		{
			index += buffer_size;
		}
		return buffer[index];
	};
};

class DelayAnalog : public MpBase2
{
public:
	DelayAnalog()
	{
		initializePin(pinSignalIn);
		initializePin(pinSignalOut);
		initializePin(pinModulation);
		initializePin(pinFeedback);
		initializePin(pinClear);
		initializePin(pinDelayTimesecs);
		initializePin(pinMode);
	}

	void onSetPins(void) override
	{
		// Check which pins are updated.
		if (pinDelayTimesecs.isUpdated())
		{
			CreateBuffer();
		}
		if (pinClear.isUpdated() && pinClear == true)
		{
			ClearBuffer();
		}

		// Set state of output audio pins.
		pinSignalOut.setStreaming(true);
//		setSleep(pinAllowSleep);

		// Set processing method.
		typedef void (DelayAnalog::* MyProcess_ptr)(int sampleFrames);
#define PROCESS_PTR( modulation, interpolate, feedback ) &DelayAnalog::subProcess< modulation, interpolate, feedback >

		// NOTE: Resonance limits depend on pitch, so when pitch modulated, need to calc resonance too.
		const static MyProcess_ptr ProcessSelection[2][2][2][2] = // Mode, modulate, Interpolate, Feedback.
		{
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationNone,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationNone,    PolicyFeedbackModulated),
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationCubic,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationCubic,    PolicyFeedbackModulated),

			// Analogue mode always active even when modulation static.
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationNone,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationNone,    PolicyFeedbackModulated),
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationCubic,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationAnalogChanging ,    PolicyInterpolationCubic,    PolicyFeedbackModulated),


			PROCESS_PTR(PolicyModulationFixed		   ,    PolicyInterpolationNone,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationFixed		   ,    PolicyInterpolationNone,    PolicyFeedbackModulated),
			PROCESS_PTR(PolicyModulationFixed		   ,    PolicyInterpolationCubic,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationFixed		   ,    PolicyInterpolationCubic,    PolicyFeedbackModulated),

			PROCESS_PTR(PolicyModulationDigitalChanging,    PolicyInterpolationNone,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationDigitalChanging,    PolicyInterpolationNone,    PolicyFeedbackModulated),
			PROCESS_PTR(PolicyModulationDigitalChanging,    PolicyInterpolationCubic,    PolicyFeedbackOff),
			PROCESS_PTR(PolicyModulationDigitalChanging,    PolicyInterpolationCubic,    PolicyFeedbackModulated),
		};

		const int pinInterpolateOutput = 1;
		setSubProcess(static_cast <SubProcess_ptr2> (ProcessSelection[pinMode][pinModulation.isStreaming()][pinInterpolateOutput][pinFeedback.isStreaming() || pinFeedback != 0.0f]));
	}

	template< class ModulationPolicy, class InterpolationPolicy, class FeedbackPolicy >
	void subProcess(int sampleFrames)
	{
		auto signalIn = getBuffer(pinSignalIn);
		auto modulation = getBuffer(pinModulation);
		auto feedback = getBuffer(pinFeedback);
		auto signalOut = getBuffer(pinSignalOut);

		float prev_out = m_prev_out;

		int read_offset;
		float read_offset_fine;
		ModulationPolicy::CalculateInitial(modulation, buffer_size, readOffset, read_offset, read_offset_fine);

		for (int s = sampleFrames; s > 0; --s)
		{
			// Also refer 'Delay2" which addresses small spikes problem.
			ModulationPolicy::Calculate(modulation, buffer_size, readOffset, read_offset, read_offset_fine);

			buffer[count] = *signalIn++ + FeedbackPolicy::Calculate(prev_out, feedback);
			// !! THIS is where you should write the wrapped samples, BEFORE they are (potentially) read below.

			*signalOut++ = prev_out = InterpolationPolicy::Calculate(count, buffer.data(), read_offset, read_offset_fine, padded_buffer_size);
			if (count >= padded_buffer_size)
			{
				// make a copy of same sample at buffer start and end to ease interpolation.
				buffer[count - padded_buffer_size] = buffer[count]; // !! TOO LATE (sometimes, causes small spikes).
				if (count >= padded_buffer_size + interpolationExtraSamples - 1)
				{
					count -= padded_buffer_size;
				}
			}
			++count;
		}

		m_prev_out = prev_out;
		// DON'T WORK, NEVER QUITE REACHES TARGET.
		// If tape delay effect as caught up, switch to fixed modulation policy.
		if (ModulationPolicy::Active == true && !pinModulation.isStreaming())
		{
			float targetReadOffset = (1.0f - *modulation) * buffer_size;
			if (fabsf(readOffset - targetReadOffset) < 0.001f)
			{
				SET_PROCESS2((&DelayAnalog::subProcess<PolicyModulationFixed, InterpolationPolicy, FeedbackPolicy >));
			}
		}
	}

	void CreateBuffer()
	{
		buffer_size = (int)(getSampleRate() * pinDelayTimesecs);

		if (buffer_size < 0)
			buffer_size = 0;

		if (buffer_size > getSampleRate() * 10)	// limit to 10 s sample
			buffer_size = (int)getSampleRate() * 10;

		// Need at least 3 samples at begining for wrapping interpolation samples.
		padded_buffer_size = interpolationExtraSamples + buffer_size;

		// Add some samples off-end to simplify interpolation.
		int allocatedSize = padded_buffer_size + interpolationExtraSamples;

		buffer.assign(allocatedSize, 0.0f);

		// ensure we aren't accessing data outside buffer
		count = 0;
	}

	void ClearBuffer()
	{
		std::fill(buffer.begin(), buffer.end(), 0.0f);
	}

private:
	AudioInPin pinSignalIn;
	AudioInPin pinModulation;
	AudioOutPin pinSignalOut;
	FloatInPin pinDelayTimesecs;
	IntInPin pinMode;
	AudioInPin pinFeedback;
	BoolInPin pinClear;

	std::vector<float> buffer;
	float m_prev_out;
	float readOffset;
	int count;
	int buffer_size;
	int padded_buffer_size;
};


namespace
{
	auto r = Register<DelayAnalog>::withId(L"SE Delay (Analog)");
}
