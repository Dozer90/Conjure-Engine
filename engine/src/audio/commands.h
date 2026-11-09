#pragma once

#include "conjure/audio/audio_types.h"

#include "conjure/core/types.h"

#include <type_traits>

// Main thread -> mixer commands and mixer -> main thread events (docs/design/12_audio.md 2.7).
// Both are trivially copyable so they travel through spsc_ring. Every voice command carries the
// generation of the mixer slot it targets; the mixer drops a mismatch (stale traffic for a reused
// slot) and the main thread drops events whose generation it no longer owns.

namespace conjure::audio {

    class pcm_buffer;
    class stream_ring;

    enum class command_kind : u8 {
        PLAY,
        STOP,
        PAUSE,
        RESUME,
        SET_GAIN,
        SET_PAN,
        SET_STEP,
        SET_LOOP,
        SEEK,
        SET_GROUP,
        SET_DOWNMIX,
        SET_GROUP_GAIN,
        SET_GROUP_PAUSED,
        SET_MASTER_PAUSED
    };

    /// <summary>
    /// Everything the mixer needs to start a voice in its slot.
    /// </summary>
    struct play_payload {
        const pcm_buffer *pcm = nullptr;    // PCM storage; the main thread keeps the shared_ptr alive
        stream_ring *stream = nullptr;      // STREAMED storage; the main thread owns the ring
        f32 gain = 1.0f;
        f32 pan = 0.0f;
        f64 step = 1.0;                     // pitch * clipRate / mixRate
        u64 startFrame = 0;
        u64 clipFrames = 0;                 // STREAMED: the stream length (PCM reads it from the buffer)
        bool loop = false;
        bool startPaused = false;
        bool downmix = false;               // stereo clip played as a 3D voice
        u16 channels = 1;
    };

    struct voice_command {
        command_kind kind = command_kind::PLAY;
        u8 groupIndex = 0;
        u16 reserved = 0;
        u32 voiceIndex = 0;                 // mixer slot
        u32 generation = 0;                 // slot generation the command belongs to
        u32 fadeFrames = 0;                 // STOP: length of the fade; SET_GAIN / SET_PAN: ramp length
        union payload_union {
            play_payload play;
            f32 scalar;                     // SET_GAIN, SET_PAN, SET_GROUP_GAIN
            f64 step;                       // SET_STEP
            u64 frame;                      // SEEK
            bool flag;                      // SET_LOOP, SET_DOWNMIX, SET_GROUP_PAUSED, SET_MASTER_PAUSED
        } payload = {};
    };

    static_assert(std::is_trivially_copyable_v<voice_command>, "commands travel through the SPSC ring");
    static_assert(sizeof(voice_command) <= 80, "command size is part of the ring's memory budget");

    enum class event_kind : u8 {
        STARTED,
        FINISHED,
        STOPPED,
        LOOPED,
        UNDERRUN,
        PAUSED,
        RESUMED
    };

    struct voice_event {
        event_kind kind = event_kind::STARTED;
        u32 voiceIndex = 0;
        u32 generation = 0;
        u64 frame = 0;                      // cursor when the event happened
    };

    static_assert(std::is_trivially_copyable_v<voice_event>, "events travel through the SPSC ring");
    static_assert(sizeof(voice_event) == 24, "event size is part of the ring's memory budget");
}
