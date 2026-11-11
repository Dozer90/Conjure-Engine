#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Error codes of the audio module (docs/design/12_audio.md 4.1). Built with the generic
// core::make_error(audio_error::X, detail) and bound to the category "audio" below.

namespace conjure::audio {

    enum class audio_error : u16 {
        BACKEND_INIT_FAILED = 1,
        DEVICE_NOT_FOUND,
        DEVICE_START_FAILED,
        DEVICE_LOST,
        UNSUPPORTED_FORMAT,
        DECODE_FAILED,
        INVALID_CLIP,
        CLIP_NOT_LOADED,
        VOICE_LIMIT_REACHED,
        INVALID_VOICE,
        STREAM_SLOT_LIMIT,
        INVALID_GROUP,
        GROUP_LIMIT_REACHED,
        GROUP_CYCLE,
        DUPLICATE_GROUP,
        COMMAND_QUEUE_FULL,
        INVALID_SETTINGS,
        NOT_INITIALISED
    };

    /// <summary>
    /// The enumerator name of a code ("DECODE_FAILED"); "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view audio_error_name(audio_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::audio::audio_error, "audio", conjure::audio::audio_error_name);
