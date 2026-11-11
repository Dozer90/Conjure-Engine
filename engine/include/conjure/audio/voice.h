#pragma once

#include "conjure/audio/audio_types.h"
#include "conjure/audio/mix_groups.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/slot_map.h"
#include "conjure/core/types.h"

#include <compare>
#include <type_traits>

// Voice identity and parameters (docs/design/12_audio.md 2.3).

namespace conjure::audio {

    /// <summary>
    /// Handle of a voice: the slot key of the engine's voice table. 8 bytes, trivially copyable,
    /// mirrored by the C# AudioSource struct. A stale id is rejected by every engine call.
    /// </summary>
    struct voice_id {
        core::slot_key key = {};

        /// <summary>
        /// True for every id the engine issued; false for the null id.
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return key.is_valid();
        }

        /// <summary>
        /// Packs the id into one integer (generation in the high 32 bits).
        /// </summary>
        [[nodiscard]] constexpr u64 to_u64() const noexcept {
            return key.to_u64();
        }

        /// <summary>
        /// Inverse of to_u64().
        /// </summary>
        [[nodiscard]] static constexpr voice_id from_u64(u64 packed) noexcept {
            return voice_id{core::slot_key::from_u64(packed)};
        }

        /// <summary>
        /// The null id. Equivalent to voice_id{}.
        /// </summary>
        [[nodiscard]] static constexpr voice_id null() noexcept {
            return voice_id{};
        }

        [[nodiscard]] constexpr auto operator<=>(const voice_id &) const noexcept = default;
        [[nodiscard]] constexpr bool operator==(const voice_id &) const noexcept = default;
    };

    static_assert(sizeof(voice_id) == 8, "voice_id must pack into 64 bits");
    static_assert(std::is_trivially_copyable_v<voice_id>, "voice_id must be trivially copyable");

    /// <summary>
    /// Everything play() needs. Values outside the documented ranges are clamped at the API
    /// boundary (A15); NaN becomes the default.
    /// </summary>
    struct play_params {
        f32 volume = 1.0f;                  // [0, 2]
        f32 pitch = 1.0f;                   // [0.25, 4]
        f32 pan = 0.0f;                     // [-1, 1]; used only when spatial == false
        bool loop = false;
        bool spatial = false;               // 3D: attenuation + pan from position (set_position)
        bool retain = false;                // false = one-shot, released automatically when finished (A5)
        bool startPaused = false;
        u8 priority = 128;                  // Voice stealing: lower is stolen first
        group_id group = MASTER_GROUP;
        f64 startSeconds = 0.0;             // Seek before start
        attenuation_model attenuation = attenuation_model::INVERSE;
        f32 minDistance = 1.0f;             // >= 0.01
        f32 maxDistance = 100.0f;           // >= minDistance
        f32 rolloff = 1.0f;                 // [0, 10]

        [[nodiscard]] bool operator==(const play_params &other) const noexcept = default;
    };

    /// <summary>
    /// Clamp rules of A15 applied to every field: volume [0, 2], pitch [0.25, 4], pan [-1, 1],
    /// minDistance >= 0.01, maxDistance >= minDistance, rolloff [0, 10], startSeconds >= 0; NaN
    /// becomes the field's default.
    /// </summary>
    [[nodiscard]] CONJURE_API play_params clamp_play_params(const play_params &params) noexcept;

    /// <summary>
    /// Individual clamp helpers shared by play_params and the engine setters.
    /// </summary>
    [[nodiscard]] CONJURE_API f32 clamp_volume(f32 volume) noexcept;
    [[nodiscard]] CONJURE_API f32 clamp_pitch(f32 pitch) noexcept;
    [[nodiscard]] CONJURE_API f32 clamp_pan(f32 pan) noexcept;

    /// <summary>
    /// Main-thread mirror of a voice, refreshed by audio_engine::update() and end_frame().
    /// </summary>
    struct voice_info {
        voice_state state = voice_state::FREE;
        u64 cursorFrames = 0;               // relaxed atomic from the callback
        u64 clipFrames = 0;
        f32 volume = 1.0f;
        f32 pitch = 1.0f;
        f32 effectiveGain = 1.0f;           // volume * groupGain * attenuation * listenerGain, last value sent
        group_id group = MASTER_GROUP;
        bool loop = false;
        bool spatial = false;
        bool retain = false;
        bool streamed = false;
        u32 clipVersion = 0;                // asset_ref::version() at play time, for reload detection

        [[nodiscard]] bool operator==(const voice_info &other) const noexcept = default;
    };
}
