#pragma once

#include "conjure/audio/audio_engine.h"
#include "conjure/audio/null_backend.h"

#include "conjure/core/types.h"

// Shared helpers of the engine-level audio tests: a null-backend settings block and frame stepping.

namespace conjure::testing {

    /// <summary>
    /// 1/60 s, the frame the engine tests step by (800 frames at 48 kHz).
    /// </summary>
    inline constexpr f32 FRAME_SECONDS = 1.0f / 60.0f;
    inline constexpr u32 FRAME_FRAMES = 800;

    /// <summary>
    /// Settings for a headless engine at 48 kHz.
    /// </summary>
    [[nodiscard]] inline audio::audio_settings null_settings() {
        audio::audio_settings settings = {};
        settings.backend = audio::audio_backend_kind::NULL_BACKEND;
        settings.sampleRate = 48000;
        return settings;
    }

    /// <summary>
    /// Runs update(FRAME_SECONDS) + end_frame() 'frames' times.
    /// </summary>
    inline void step_frames(audio::audio_engine &engine, u32 frames) {
        for (u32 frame = 0; frame < frames; ++frame) {
            engine.update(FRAME_SECONDS);
            engine.end_frame();
        }
    }

    /// <summary>
    /// Runs frames until the voice is no longer playing or 'maxFrames' elapsed; returns the count.
    /// </summary>
    inline u32 step_until_ended(audio::audio_engine &engine, audio::voice_id voice, u32 maxFrames) {
        u32 frames = 0;
        while (frames < maxFrames && engine.is_playing(voice)) {
            engine.update(FRAME_SECONDS);
            engine.end_frame();
            ++frames;
        }
        return frames;
    }

    /// <summary>
    /// The null backend's snapshot of a mixer slot.
    /// </summary>
    [[nodiscard]] inline const audio::null_voice_stats &slot_stats(audio::audio_engine &engine, u32 slot) {
        return engine.null_backend_instance()->stats().voices[slot];
    }
}
