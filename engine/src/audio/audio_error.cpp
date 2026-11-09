#include "conjure/audio/audio_error.h"

using namespace conjure;
using namespace conjure::audio;

std::string_view conjure::audio::audio_error_name(audio_error code) noexcept {
    switch (code) {
        case audio_error::BACKEND_INIT_FAILED:
            return "BACKEND_INIT_FAILED";
        case audio_error::DEVICE_NOT_FOUND:
            return "DEVICE_NOT_FOUND";
        case audio_error::DEVICE_START_FAILED:
            return "DEVICE_START_FAILED";
        case audio_error::DEVICE_LOST:
            return "DEVICE_LOST";
        case audio_error::UNSUPPORTED_FORMAT:
            return "UNSUPPORTED_FORMAT";
        case audio_error::DECODE_FAILED:
            return "DECODE_FAILED";
        case audio_error::INVALID_CLIP:
            return "INVALID_CLIP";
        case audio_error::CLIP_NOT_LOADED:
            return "CLIP_NOT_LOADED";
        case audio_error::VOICE_LIMIT_REACHED:
            return "VOICE_LIMIT_REACHED";
        case audio_error::INVALID_VOICE:
            return "INVALID_VOICE";
        case audio_error::STREAM_SLOT_LIMIT:
            return "STREAM_SLOT_LIMIT";
        case audio_error::INVALID_GROUP:
            return "INVALID_GROUP";
        case audio_error::GROUP_LIMIT_REACHED:
            return "GROUP_LIMIT_REACHED";
        case audio_error::GROUP_CYCLE:
            return "GROUP_CYCLE";
        case audio_error::DUPLICATE_GROUP:
            return "DUPLICATE_GROUP";
        case audio_error::COMMAND_QUEUE_FULL:
            return "COMMAND_QUEUE_FULL";
        case audio_error::INVALID_SETTINGS:
            return "INVALID_SETTINGS";
        case audio_error::NOT_INITIALISED:
            return "NOT_INITIALISED";
    }
    return "";
}
