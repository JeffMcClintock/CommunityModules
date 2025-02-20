#pragma once
#include <optional>

// ivstmidi2extension.h
// https://forum.juce.com/t/proposal-extension-for-native-vst3-midi-support/65277/8

namespace vst3_ext_midi
{
    namespace sb = Steinberg;
    namespace sbv = sb::Vst;

    /// An event representing a Universal MIDI Packet.
    /// This is intended to be passed between host and client using the IEventQueue
    /// mechanism.
    struct UMPEvent
    {
        /// A stand-in EventType value for this event.
        /// Event::type should be set to this value to indicate that the payload is
        /// a UMPEvent.
        static constexpr auto kType = 0x100;

        /// Words of a Universal MIDI Packet.
        /// A UMPEvent will only ever contain a single packet, which means that
        /// the words at indices 2 and 3 may not be used for some events.
        /// Check the first word to find the real length of the packet, and avoid
        /// reading or writing to words that are not contained in the packet.
        sb::uint32 words[4];

        /// If the event is a UMPEvent, returns that event.
        /// Otherwise, returns nullopt.
        static std::optional<UMPEvent> fromEvent(const sbv::Event& e)
        {
            if (e.type != kType)
                return {};

            UMPEvent result;
            memcpy(&result, &e.noteOn, sizeof(UMPEvent));
            return result;
        }

        /// Returns an Event with this UMPEvent as the payload.
        sbv::Event toEvent(sb::int32 busIndex,
            sb::int32 sampleOffset,
            sbv::TQuarterNotes ppqPos,
            sb::uint16 flags) const
        {
            sbv::Event result{ busIndex, sampleOffset, ppqPos, flags, kType, {} };
            memcpy(&result.noteOn, this, sizeof(UMPEvent));
            return result;
        }
    };

    // UMPEvent will re-use the storage of NoteOnEvent.
    static_assert (sizeof(UMPEvent) <= sizeof(sbv::NoteOnEvent));
    static_assert (alignof (UMPEvent) <= alignof (sbv::NoteOnEvent));

} // namespace vst3_ext_midi