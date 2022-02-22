#pragma once
#include <stdint.h>
#include <algorithm>
#include <assert.h>

/*
#include "../se_sdk3/mp_midi.h"

using namespace GmpiMidi;
using namespace GmpiMidiHdProtocol;

*/

#if defined(__GNUC__)
#pragma pack(push,1)
#else
#pragma pack(push)
#pragma pack(1)
#endif

namespace gmpi
{
namespace midi
{
	namespace utils
	{
		// converts a 14-bit bipolar controler to a normalized value (between 0.0 and 1.0)
		inline float bipoler14bitToNormalized(uint8_t msb, uint8_t lsb)
		{
			constexpr int centerInt = 0x2000;
			constexpr float scale = 0.5f / (centerInt - 1); // -0.5 -> +0.5

			const int controllerInt = (static_cast<int>(lsb) + (static_cast<int>(msb) << 7)) - centerInt;
			// bender range is 0 -> 8192 (center) -> 16383
			// which is lopsided (8192 values negative, 8191 positive). So we scale by the smaller number, then clamp any out-of-range value.
			return (std::max)(0.0f, 0.5f + controllerInt * scale);
		}

		// convert a float between 0.0 -> 1.0 to a unsigned int 0 -> 0xffffffff
		inline uint32_t floatToU32(float v)
		{
			// tricky becuase you can't convert a float to u32 without truncating it. (only to signed 32)

			// to avoid overflow, only converts the most significant 30 bits, then shifts, smearing 2 lsbs.
			auto rawValue = static_cast<uint32_t>(v * (static_cast<float>((1 << 30) - 1)));
			rawValue = (std::min)(rawValue, 0x3fffffffu);
			return (rawValue << 2) | (rawValue & 3); // shift and duplicate 2 lsbs
		}

		// convert a unsigned 8.24 integer fraction to a float. (for note pitch)
		inline float u8_24_ToFloat(const uint8_t* m)
		{
			// trickyness is we discard lowest bits to avoid overflow to negative.
			constexpr float normalizedToSemitone = 1.0f / static_cast<float>(1 << 16);
			return normalizedToSemitone * float((m[0] << 16) | (m[1] << 8) | m[2]);
		}

		// convert a unsigned 0.32 integer fraction to a float. (for controllers)
		inline float u0_32_ToFloat(const uint8_t* m)
		{
			// trickyness is: we discard lowest 8 bits to avoid overflow to negative.
			constexpr float f = 1.0f / (static_cast<float>(1 << 24) - 1);
			return f * float((m[0] << 16) | (m[1] << 8) | m[2]);
		}
	}

	class message_view {
		const uint8_t* ptr_;
		std::size_t len_;

	public:
		message_view(const uint8_t* ptr, std::size_t len) noexcept
			: ptr_{ ptr }, len_{ len }
		{}

		const uint8_t& operator[](int i) noexcept {
			return ptr_[i];
		}

		uint8_t const& operator[](int i) const noexcept {
			return ptr_[i];
		}

		std::size_t size() const noexcept {
			return len_;
		}

		const uint8_t* begin() noexcept {
			return ptr_;
		}

		const uint8_t* end() noexcept {
			return ptr_ + len_;
		}
	};
} // namespace midi

namespace midi_1_0
{
	namespace status_type
	{
		enum
		{
			NoteOff = 0x8,
			NoteOn = 0x9,
			PolyAfterTouch = 0xA,
			ControlChange = 0xB,
			ProgramChange = 0xC,
			ChannelPressue = 0xD,
			PitchBend = 0xE,
		};
	}

	struct headerInfo
	{
		uint8_t status;
		uint8_t channel;
	};

	inline headerInfo decodeHeader(gmpi::midi::message_view msg)
	{
		assert(msg.size() > 1);

		uint8_t status = msg[0] >> 4;
		if (status_type::NoteOn == status && 0 == msg[2])
		{
			status = status_type::NoteOff;
		}

		return
		{
			status,
			static_cast<uint8_t>(msg[0] & 0x0f)
		};
	}

	struct noteInfo
	{
		uint8_t noteNumber;
		float velocity;
	};

	inline noteInfo decodeNote(gmpi::midi::message_view msg)
	{
		assert(msg.size() == 3);

		constexpr float recip = 1.0f / (float)0xff;
		return
		{
			msg[1],
			msg[2] * recip
		};
	}

	inline float decodeBender(gmpi::midi::message_view msg)
	{
		return midi::utils::bipoler14bitToNormalized(msg[2], msg[1]);
	}
	inline float decodeController(uint8_t v)
	{
		constexpr float recip = 1.0f / 127.0f;
		return recip * v;
	}
} // namespace midi_1_0

} // namespace gmpi

namespace GmpiMidi
{
	enum MidiStatus //: unsigned char
	{
		MIDI_NoteOff		= 0x80,
		MIDI_NoteOn			= 0x90,
		MIDI_PolyAfterTouch	= 0xA0,
		MIDI_ControlChange	= 0xB0,
		MIDI_ProgramChange	= 0xC0,
		MIDI_ChannelPressue	= 0xD0,
		MIDI_PitchBend		= 0xE0,
		MIDI_SystemMessage	= 0xF0,
		MIDI_SystemMessageEnd = 0xF7,
	};

	enum MidiSysexTimeMessages
	{
		MIDI_Universal_Realtime = 0x7F,
		MIDI_Universal_NonRealtime = 0x7E,
		MIDI_Sub_Id_Tuning_Standard = 0x08,
	};

	enum MidiLimits
	{
		MIDI_KeyCount = 128,
		MIDI_ControllerCount = 128,
		MIDI_7bit_MinimumValue = 0,
		MIDI_7bit_MaximimumValue = 127,
	};

	enum MidiChannels
	{
		MIDI_ChannelCount = 16,
		MIDI_Channel_MinimumValue = 0,
		MIDI_Channel_MaximimumValue = 15,
	};

	enum Controller
	{
		CC_HighResolutionVelocityPrefix = 88,
		CC_AllSoundOff					= 120,
		CC_ResetAllControllers			= 121,
		CC_AllNotesOff					= 123,
	};
}

namespace gmpi
{
namespace midi_2_0
{
	namespace attribute_type
	{
		enum
		{
			None = 0,
			ManufacturerSpecific = 1,
			ProfileSpecific = 2,
			Pitch = 3,
		};
	}

	enum MessageType //: unsigned char
	{
		Utility = 0x0, // 32 bits Utility Messages 
		System = 0x1, // 32 bits System Real Time and System Common Messages (except System Exclusive)
		ChannelVoice32 = 0x2, // 32 bits MIDI 1.0 Channel Voice Messages
		Data64 = 0x3, // 64 bits Data Messages (including System Exclusive)
		ChannelVoice64 = 0x4, // 64 bits MIDI 2.0 Channel Voice Messages
		Data128 = 0x5, // 128 bits Data Messages
		Reserved = 0x6, // 32 bits Reserved for future definition by MMA/AME
	};

	enum Status //: unsigned char
	{
		PolyControlChange = 0x00,
		RPN = 0x02,
		NRPN = 0x03,
		PolyBender = 0x06,

		// classics
		NoteOff = 0x8,
		NoteOn = 0x9,
		PolyAfterTouch = 0xA,
		ControlChange = 0xB,
		ProgramChange = 0xC,
		ChannelPressue = 0xD,
		PitchBend = 0xE,

		PolyNoteManagement = 0xFF,
	};

	enum PerNoteControllers
	{
		PolyModulation = 1, // Modulation
		PolyBreath = 2, // Breath
		PolyPitch = 3, // Absolute Pitch 7.25 – Section 4.2.14.2, see also Poly Bender
		// 4–6 , // Reserved
		PolyVolume = 7, // Volume
		PolyBalance = 8, // Balance
		// 9 , // Reserved
		PolyPan = 10, // Pan
		PolyExpression = 11, // Expression
		// 12–69 , // Reserved
		PolySoundController1 = 70, // Sound Controller 1 Sound Variation
		PolySoundController2 = 71, // Sound Controller 2 Timbre/Harmonic Intensity
		PolySoundController3 = 72, // Sound Controller 3 Release Time
		PolySoundController4 = 73, // Sound Controller 4 Attack Time
		PolySoundController5 = 74, // Sound Controller 5 Brightness
		PolySoundController6 = 75, // Sound Controller 6 Decay Time MMA RP-021 [MMA04]
		PolySoundController7 = 76, //  Sound Controller 7 Vibrato Rate
		PolySoundController8 = 77, //  Sound Controller 8 Vibrato Depth
		PolySoundController9 = 78, //  Sound Controller 9 Vibrato Delay
		PolySoundController10 = 79, //  Sound Controller 10 Undefined
		// 80–90, //  Reserved
		PolyEffects1Depth = 91, //  Effects 1 Depth Reverb Send Level MMA RP-023 [MMA05]
		PolyEffects2Depth = 92, //  Effects 2 Depth (formerly Tremolo Depth)
		PolyEffects3Depth = 93, //  Effects 3 Depth Chorus Send Level MMA RP-023 [MMA05]
		PolyEffects4Depth = 94, //  Effects 4 Depth (formerly Celeste [Detune] Depth)
		PolyEffects5Depth = 95, //  Effects 5 Depth (formerly Phaser Depth)
	};

	namespace RpnTypes
	{
		enum {
			PitchBendSensitivity
		};
	}

	inline bool isMidi2Message(const uint8_t* m, int size)
	{
		// MIDI 2.0 messages are always at least 4 bytes
		// and first 4-bits are message-type of which only values less than 5 are valid.
		return size >= 4 && (m[0] >> 4) <= Data128;
	}

	inline bool isMidi2Message(gmpi::midi::message_view msg)
	{
		return isMidi2Message(msg.begin(), static_cast<int>(std::size(msg)));
	}

	
	struct headerInfo
	{
		// The most significant 4 bits of every message contain the Message Type (MT).
		uint8_t messageType;
		uint8_t group;
		uint8_t channel;
		uint8_t status;
	};

	inline headerInfo decodeHeader(const uint8_t* m, [[maybe_unused]] int size)
	{
		assert(size > 1);

		return
		{
			static_cast<uint8_t>(m[0] >> 4),
			static_cast<uint8_t>(m[0] & 0x0f),
			static_cast<uint8_t>(m[1] & 0x0f),
			static_cast<uint8_t>(m[1] >> 4)
		};
	}

	struct noteInfo
	{
		uint8_t noteNumber;
		uint8_t attributeType;
		float velocity;
		float attributeValue;
	};

	inline noteInfo decodeNote(const uint8_t* m, [[maybe_unused]] int size)
	{
		assert(size == 8);

		constexpr float recip = 1.0f / (float)0xffff;
		constexpr float recip2 = 1.0f / (float)(1 << 9);
		return
		{
			m[2],
			m[3],
			recip * ((m[4] << 8) | m[5]),
			recip2 * ((m[6] << 8) | m[7]),
		};
	}

	struct polyController
	{
		uint8_t noteNumber;
		uint8_t type;
		float value;
	};

	inline polyController decodeController(const uint8_t* m, [[maybe_unused]] int size)
	{
		assert(size == 8);

		return
		{
			m[2],
			m[3],
			gmpi::midi::utils::u0_32_ToFloat(m + 4),
		};
	}

	struct RpnInfo
	{
		uint16_t rpn;
		uint32_t value;
	};

	inline RpnInfo decodeRpn(const uint8_t* m, [[maybe_unused]] int size)
	{
		assert(size == 8);

		return
		{
			static_cast<uint16_t>((m[2] << 8) | m[3]),
			static_cast<uint32_t>((m[4] << 24) | (m[5] << 16) | (m[6] << 8) | m[7])
		};
	}


	struct rawMessage64
	{
		uint8_t m[8];
	};

	inline rawMessage64 makeController(uint8_t controller, float value, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		const auto rawValue = gmpi::midi::utils::floatToU32(value);
		const uint8_t reserved{};

		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::ControlChange << 4) | (channel & 0x0f)),
			reserved,
			controller,
			static_cast<uint8_t>((rawValue >> 24)),
			static_cast<uint8_t>((rawValue >> 16) & 0xff),
			static_cast<uint8_t>((rawValue >> 8) & 0xff),
			static_cast<uint8_t>((rawValue >> 0) & 0xff)
		};
	}

	inline rawMessage64 makeRpnRaw(uint16_t rpn, uint32_t rawValue, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::RPN << 4) | (channel & 0x0f)),
			static_cast<uint8_t>(rpn >> 8),
			static_cast<uint8_t>(rpn & 0xff),
			static_cast<uint8_t>((rawValue >> 24)),
			static_cast<uint8_t>((rawValue >> 16) & 0xff),
			static_cast<uint8_t>((rawValue >> 8) & 0xff),
			static_cast<uint8_t>((rawValue >> 0) & 0xff)
		};
	}
	
	inline rawMessage64 makePolyController(uint8_t noteNumber, uint8_t controller, float value, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		const auto rawValue = gmpi::midi::utils::floatToU32(value);

		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::PolyControlChange << 4) | (channel & 0x0f)),
			noteNumber,
			controller,
			static_cast<uint8_t>((rawValue >> 24)),
			static_cast<uint8_t>((rawValue >> 16) & 0xff),
			static_cast<uint8_t>((rawValue >> 8) & 0xff),
			static_cast<uint8_t>((rawValue >> 0) & 0xff)
		};
	}

	inline rawMessage64 makePolyBender(uint8_t noteNumber, float normalized, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		////		float normalized = (std::min)(1.0f, (std::max)(0.0f, (0.5f * value / bendrange) + 0.5f));
		//const int32_t rawValue24 = static_cast<int32_t>(normalized * (float)(1 << 24));
		//const uint32_t rawValue = rawValue24 << 8;
		const auto rawValue = gmpi::midi::utils::floatToU32(normalized);

		const uint8_t reserved{};

		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::PolyBender << 4) | (channel & 0x0f)),
			noteNumber,
			reserved,
			static_cast<uint8_t>((rawValue >> 24)),
			static_cast<uint8_t>((rawValue >> 16) & 0xff),
			static_cast<uint8_t>((rawValue >> 8) & 0xff),
			static_cast<uint8_t>((rawValue >> 0) & 0xff)
		};
	}

	inline rawMessage64 makePolyPressure(uint8_t noteNumber, float normalized, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		//const int32_t rawValue24 = static_cast<int32_t>(normalized * (float)(1 << 24));
		//const uint32_t rawValue = rawValue24 << 8;
		const auto rawValue = gmpi::midi::utils::floatToU32(normalized);

		const uint8_t reserved{};

		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::PolyAfterTouch << 4) | (channel & 0x0f)),
			noteNumber,
			reserved,
			static_cast<uint8_t>((rawValue >> 24)),
			static_cast<uint8_t>((rawValue >> 16) & 0xff),
			static_cast<uint8_t>((rawValue >> 8) & 0xff),
			static_cast<uint8_t>((rawValue >> 0) & 0xff)
		};
	}

	inline rawMessage64 makeNotePitchMessage(uint8_t noteNumber, float value, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		// 7 bits: Pitch in semitones, based on default Note Number equal temperament scale.
		// 25 bits: Fractional Pitch above Note Number (i.e., fraction of one semitone).
		constexpr float semitoneToNormalized = static_cast<float>(1 << 24);

		const int32_t rawValue = static_cast<uint32_t>(value * semitoneToNormalized);
		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::PolyControlChange << 4) | (channel & 0x0f)),
			noteNumber,
			PolyPitch,
			static_cast<uint8_t>((rawValue >> 24)),
			static_cast<uint8_t>((rawValue >> 16) & 0xff),
			static_cast<uint8_t>((rawValue >> 8) & 0xff),
			static_cast<uint8_t>((rawValue >> 0) & 0xff)
		};
	}

	inline float decodeNotePitch(const uint8_t* m, [[maybe_unused]] int size)
	{
		assert(size == 8);

		return gmpi::midi::utils::u8_24_ToFloat(m + 4);
		//// trickyness is we discard lowest bits to avoid overflow to negative.
		//constexpr float normalizedToSemitone = 1.0f / static_cast<float>(1 << 16);
		//return normalizedToSemitone * float((m[4] << 16) | (m[5] << 8) | m[6]);
	}


	class NoteMapper
	{
	protected:
		struct NoteInfo
		{
			int32_t noteId;		// VST3 note ID
			int16_t pitch;		// in MIDI semitones. hmm how do they handle microtuning?
			uint8_t MidiKeyNumber;
			bool held;
		};

		NoteInfo noteIds[128] = {};
		int noteIdsRoundRobin = 0;

	public:
		NoteMapper()
		{
			for (int i = 0; i < std::size(noteIds); ++i)
				noteIds[i].MidiKeyNumber = static_cast<uint8_t>(i);
		}

		NoteInfo* findNote(int noteId)
		{
			for (int i = 0; i < 128; ++i)
			{
				if (noteIds[i].noteId == noteId)
				{
					return &noteIds[i];
				}
			}

			return {};
		}

		NoteInfo& allocateNote(int noteId, int preferredNoteNumber)
		{
			int res = -1;
			for (int i = 0; i < 128; ++i)
			{
				if (noteIds[i].noteId == noteId)
				{
					res = i;
					break;
				}
			}
#if 1 // set 0 to enable pure round-robin allocation (better for flushing out bugs).
			if (res == -1 && !noteIds[preferredNoteNumber].held)
			{
				res = preferredNoteNumber;
			}
#endif
			if (res == -1)
			{
				for (int i = 0; i < 128; ++i)
				{
					noteIdsRoundRobin = (noteIdsRoundRobin + 1) & 0x7f;

					if (!noteIds[noteIdsRoundRobin].held)
					{
						break;
					}
				}
				res = noteIdsRoundRobin;
			}

			assert(res >= 0 && res < 128);

			noteIds[res].held = true;
			noteIds[res].noteId = noteId;
			return noteIds[res];
		}
	};

	inline rawMessage64 makeNoteOnMessageWithPitch(uint8_t noteNumber, float velocity, float pitch, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		constexpr float scale = static_cast<float>(1 << 16) - 1;
		const int32_t VelocityRaw = static_cast<int32_t>(velocity * scale);

		constexpr float scale2 = static_cast<float>(1 << 9); //  Pitch is a Q7.9 fixed-point unsigned integer that specifies a pitch in semitones
		const int32_t PitchRaw = static_cast<int32_t>(pitch * scale2);

		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::NoteOn << 4) | (channel & 0x0f)),
			noteNumber,
			gmpi::midi_2_0::attribute_type::Pitch,
			static_cast<uint8_t>(VelocityRaw >> 8),
			static_cast<uint8_t>(VelocityRaw & 0xff),
			static_cast<uint8_t>(PitchRaw >> 8),
			static_cast<uint8_t>(PitchRaw & 0xff),
		};
	}

	inline rawMessage64 makeNoteOnMessage(uint8_t noteNumber, float velocity, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		constexpr float scale = static_cast<float>(1 << 16) - 1;

		const int32_t rawValue = static_cast<int32_t>(velocity * scale);
		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::NoteOn << 4) | (channel & 0x0f)),
			noteNumber,
			gmpi::midi_2_0::attribute_type::None,
			static_cast<uint8_t>(rawValue >> 8),
			static_cast<uint8_t>(rawValue & 0xff),
			0,
			0
		};
	}

	inline rawMessage64 makeNoteOffMessage(uint8_t noteNumber, float velocity, uint8_t channel = 0, uint8_t channelGroup = 0)
	{
		constexpr float scale = static_cast<float>(1 << 16) - 1;
		const int32_t rawValue = static_cast<int32_t>(velocity * scale);

		return rawMessage64
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice64 << 4) | (channelGroup & 0x0f)),
			static_cast<uint8_t>((gmpi::midi_2_0::NoteOff << 4) | (channel & 0x0f)),
			noteNumber,
			gmpi::midi_2_0::attribute_type::None,
			static_cast<uint8_t>(rawValue >> 8),
			static_cast<uint8_t>(rawValue & 0xff),
			0,
			0
		};
	}
} // midi_2_0
}

// deprecated
namespace GmpiMidiHdProtocol
{
	/*
	enum WrappedHdMessage
	{
		size = 20, // bytes.
	};
*/
	/*
	// MIDI HD-PROTOCOL. (hd protocol)

	*The short packet note - on has fields for:
	-specifying one of 14 channel groups and one of 254 channels in the group
	- Specifying one of 256 notes
	- 12 bits of velocity
	- 20 bits of articulation, direct pitch, or timestamp. [0 - 0xFFFFF]
	A long packet has fields for articulation, direct pitch, AND timestamp

	NOTES:
	HD is exclusively based on 32 bits unsigned integer atoms.
	shortest HD message is 3 atoms long (12 bytes...). http://comments.gmane.org/gmane.comp.multimedia.puredata.devel/12921
	*/

	struct Midi2
	{
		unsigned char sysexWrapper[8];
		unsigned char status;	// status (low 4 bits) + chan-group in upper 4 bits (0x0f = All).
		unsigned char channel;	// 254 channels, 0xFF = All.
		unsigned char key;		// 255 keys, 0xFF = All.
		unsigned char unused; // was controllerClass; // 
		uint32_t value;			// upper 12 bits are Velocity/Controller Number, lower 20 bits are direct pitch or articulation.
		unsigned char sysexWrapperEnd[1];

		const unsigned char* data()
		{
			return (unsigned char*) this;
		}
		int size()
		{
			return (int) sizeof(Midi2);
		}
	};

	inline void setMidiMessage(Midi2& msg, int status, int value = 0, int key = 0xff, int velocity = 0, int channel = 0, int channelGroup = 0)
	{
		// Form SYSEX wrapper.
		msg.sysexWrapper[0] = GmpiMidi::MIDI_SystemMessage;
		msg.sysexWrapper[1] = 0x7f;	// universal real-time.
		msg.sysexWrapper[2] = 0x00; // device-ID. "channel"
		msg.sysexWrapper[3] = 0x70; // sub-ID#1. (Jeff's HD-PROTOCOL wrapper)
		msg.sysexWrapper[4] = 0x00; // sub-ID#2. (Version 0)
		msg.sysexWrapper[5] = 0x00; // unused padding for alignment. TODO move to front of struct
		msg.sysexWrapper[6] = 0x00;
		msg.sysexWrapper[7] = 0x00;

		msg.sysexWrapperEnd[0] = GmpiMidi::MIDI_SystemMessageEnd;

		msg.status = ( status & 0xF0 ) | ( channelGroup & 0x0F );
		msg.channel = static_cast<unsigned char>(channel);
		msg.key = static_cast<unsigned char>(key);
		msg.value = ( value & 0x0FFF ) | ( velocity << 20 );
		msg.unused /* was controllerClass */ = 0;
	}

	/* confirmed MIDI 2.0
	16 channels x 16 channel groups
	16 bit velocity
	16 bit articulation, 256 types
	32 bit CCs
	128 CCs
	256 registered per note controllers
	256 assignable per note controllers
	16384 RPNs/NRPNs


	inline void setMidiMessage2(Midi2& msg, int status, int value = 0, int key = 0xff, int velocity = 0, int channel = 0, int channelGroup = 0)
	{
		// Form SYSEX wrapper.
		msg.sysexWrapper[0] = GmpiMidi::MIDI_SystemMessage;
		msg.sysexWrapper[1] = 0x7f;	// universal real-time.
		msg.sysexWrapper[2] = 0x00; // device-ID. "channel"
		msg.sysexWrapper[3] = 0x70; // sub-ID#1. (Jeff's HD-PROTOCOL wrapper)
		msg.sysexWrapper[4] = 0x01; // sub-ID#2. (Version 1)
		msg.sysexWrapper[5] = 0x00; // unused padding for alignment. TODO move to front of struct
		msg.sysexWrapper[6] = 0x00;
		msg.sysexWrapper[7] = 0x00;

		msg.sysexWrapperEnd[0] = GmpiMidi::MIDI_SystemMessageEnd;

		msg.status = (status & 0xF0) | (channelGroup & 0x0F);
		msg.channel = static_cast<unsigned char>(channel);
		msg.key = static_cast<unsigned char>(key);

		msg.value = (velocity & 0x0FFF) | (value << 20);
		msg.unused / * was controllerClass * / = 0;
	}
*/

	inline bool isWrappedHdProtocol(const unsigned char* m, int size)
	{
		return size == sizeof(Midi2) && m[0] == GmpiMidi::MIDI_SystemMessage && m[1] == 0x7f && m[2] == 0x00 && m[3] == 0x70 && m[4] == 0x00;
	}

	inline void DecodeHdMessage(const unsigned char* midiMessage, int /* size */, int& status, int& midiChannel, int& channelGroup, int& keyNumber, int& val_12b, int& val_20b)
	{
		Midi2* m2 = (Midi2*) midiMessage;
		status = m2->status & 0xF0;
		channelGroup = m2->status & 0x0f;
		midiChannel = m2->channel;
		keyNumber = m2->key;
		val_20b = m2->value & 0x0FFF;
		val_12b = m2->value >> 20;
	}

	inline float val20BitToFloat(int val_20b)
	{
		// due to bug in implementation, CCs are only 12 bits
		constexpr float recip = 1.0f / (float)0xFFF;
		return recip * static_cast<float>(val_20b);
	}
}

#pragma pack(pop)
