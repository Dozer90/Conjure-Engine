#pragma once

#include "conjure/audio/audio_types.h"
#include "conjure/audio/clip.h"
#include "conjure/audio/mix_groups.h"
#include "conjure/audio/voice.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/core/api.h"
#include "conjure/core/containers/slot_map.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <type_traits>

// Component data the engine's audio_subsystem registers into the ECS (docs/design/12_audio.md 2.6,
// R7, R50). Plain reflected structs; this module never includes ecs. audio_source carries an
// assets::asset_ref<audio_clip> (an ASSET_REF field: the uuid serialises, the scene bind pass binds
// it, R25); its field order is the C# AudioSourceData mirror (15_bindings.md 2.6) and is pinned by
// the static assertions in components.cpp.

namespace conjure::audio {

    /// <summary>
    /// Link between a component and its voice. Hidden from the inspector, never serialised.
    /// </summary>
    struct source_handle {
        voice_id voice = {};
        u32 clipVersion = 0;                // asset_ref::version() when the voice was started
        u8 flags = 0;                       // bit 0: started once (requests live in audio_source::playbackRequest, R50)

        /// <summary>
        /// Bit of flags recording that the source was started at least once (playOnStart consumed).
        /// </summary>
        static constexpr u8 FLAG_STARTED_ONCE = 1u << 0;

        /// <summary>
        /// True once FLAG_STARTED_ONCE is set.
        /// </summary>
        [[nodiscard]] constexpr bool started_once() const noexcept {
            return (flags & FLAG_STARTED_ONCE) != 0;
        }

        [[nodiscard]] bool operator==(const source_handle &other) const noexcept = default;
    };

    static_assert(sizeof(source_handle) == 16, "source_handle layout is mirrored opaquely by C#");
    static_assert(std::is_trivially_copyable_v<source_handle>, "components are memcpy relocated");

    /// <summary>
    /// Script-side playback command, consumed and reset to NONE by engine.audio_sync
    /// (docs/design/15_bindings.md B8, R50).
    /// </summary>
    enum class audio_playback_request : u8 {
        NONE = 0,
        PLAY,
        STOP,
        PAUSE,
        RESUME
    };

    /// <summary>
    /// The emitter component (88 bytes, 12_audio.md 2.6). Authored fields come first; the three
    /// hidden fields at the end are runtime state: playbackRequest is written by scripts and
    /// consumed by engine.audio_sync, isPlaying is written back by the engine, handle links the
    /// component to its voice and is TRANSIENT at ECS registration. Hidden fields still take part in
    /// the reflection JSON (core serialises every described field); the ECS serializer drops the
    /// whole component when it is TRANSIENT, and the engine resets these three on load.
    /// </summary>
    struct audio_source {
        assets::asset_ref<audio_clip> clip = {};
        f32 volume = 1.0f;                  // 0..2
        f32 pitch = 1.0f;                   // 0.25..4
        bool loop = false;
        // The "3D flag". Defaults to true (a component is an emitter in the world), unlike
        // play_params::spatial, which defaults to false.
        bool spatial = true;
        bool playOnStart = true;
        bool oneShot = false;               // Destroy the entity when the voice finishes
        core::string_id group = {};         // Null = MASTER
        attenuation_model attenuation = attenuation_model::INVERSE;
        f32 minDistance = 1.0f;
        f32 maxDistance = 100.0f;
        f32 rolloff = 1.0f;
        audio_playback_request playbackRequest = audio_playback_request::NONE;   // Hidden; never authored (R50)
        bool isPlaying = false;             // Hidden; written by the engine, one frame old for scripts (R50)
        source_handle handle = {};          // Hidden; TRANSIENT at ECS registration

        [[nodiscard]] bool operator==(const audio_source &other) const noexcept = default;
    };

    static_assert(sizeof(audio_source) == 88, "audio_source layout is mirrored by C# AudioSourceData");
    static_assert(std::is_trivially_copyable_v<audio_source>, "components are memcpy relocated");

    /// <summary>
    /// The play_params an audio_source describes: volume, pitch, loop, spatial, attenuation and
    /// distances copied from the fields (clamped by the engine at the API boundary, A15), 'group'
    /// as resolved by the caller (audio_engine::find_group(source.group)), retain = true because
    /// component voices are released by engine.audio_sync, pan 0 and the default priority.
    /// </summary>
    [[nodiscard]] CONJURE_API play_params play_params_from(const audio_source &source, group_id group) noexcept;

    /// <summary>
    /// The listener component. The first active one in iteration order drives
    /// audio_engine::set_listener.
    /// </summary>
    struct audio_listener {
        f32 gain = 1.0f;                    // 0..2
        bool active = true;

        [[nodiscard]] bool operator==(const audio_listener &other) const noexcept = default;
    };

    static_assert(sizeof(audio_listener) == 8, "audio_listener layout is mirrored by C# AudioListenerData");
    static_assert(std::is_trivially_copyable_v<audio_listener>, "components are memcpy relocated");

    /// <summary>
    /// Registered reflection names of the component types and enums.
    /// </summary>
    inline constexpr std::string_view AUDIO_SOURCE_TYPE_NAME = "conjure.audio.audio_source";
    inline constexpr std::string_view AUDIO_LISTENER_TYPE_NAME = "conjure.audio.audio_listener";
    inline constexpr std::string_view SOURCE_HANDLE_TYPE_NAME = "conjure.audio.source_handle";
    inline constexpr std::string_view ATTENUATION_MODEL_TYPE_NAME = "conjure.audio.attenuation_model";
    inline constexpr std::string_view PLAYBACK_REQUEST_TYPE_NAME = "conjure.audio.audio_playback_request";
}

// voice_id wraps a core::slot_key; both are described here so the hidden source_handle::voice field
// reflects. Core does not describe slot_key itself; if it ever does, this specialisation moves there.
CONJURE_REFLECT_STRUCT(conjure::core::slot_key, "conjure.core.slot_key",
    CONJURE_FIELD(index), CONJURE_FIELD(generation));
CONJURE_REFLECT_STRUCT(conjure::audio::voice_id, "conjure.audio.voice_id", CONJURE_FIELD(key));
CONJURE_REFLECT_ENUM(conjure::audio::attenuation_model, "conjure.audio.attenuation_model",
    NONE, INVERSE, LINEAR, EXPONENTIAL);
CONJURE_REFLECT_ENUM(conjure::audio::audio_playback_request, "conjure.audio.audio_playback_request",
    NONE, PLAY, STOP, PAUSE, RESUME);
CONJURE_REFLECT_STRUCT(conjure::audio::source_handle, "conjure.audio.source_handle",
    CONJURE_FIELD_HIDDEN(voice), CONJURE_FIELD_HIDDEN(clipVersion), CONJURE_FIELD_HIDDEN(flags));
CONJURE_REFLECT_STRUCT(conjure::audio::audio_source, "conjure.audio.audio_source",
    CONJURE_FIELD(clip), CONJURE_FIELD(volume, .min = 0.0, .max = 2.0), CONJURE_FIELD(pitch, .min = 0.25, .max = 4.0),
    CONJURE_FIELD(loop), CONJURE_FIELD(spatial), CONJURE_FIELD(playOnStart), CONJURE_FIELD(oneShot),
    CONJURE_FIELD(group), CONJURE_FIELD(attenuation), CONJURE_FIELD(minDistance, .min = 0.01),
    CONJURE_FIELD(maxDistance, .min = 0.01), CONJURE_FIELD(rolloff, .min = 0.0, .max = 10.0),
    CONJURE_FIELD_HIDDEN(playbackRequest), CONJURE_FIELD_HIDDEN(isPlaying), CONJURE_FIELD_HIDDEN(handle));
CONJURE_REFLECT_STRUCT(conjure::audio::audio_listener, "conjure.audio.audio_listener",
    CONJURE_FIELD(gain, .min = 0.0, .max = 2.0), CONJURE_FIELD(active));
