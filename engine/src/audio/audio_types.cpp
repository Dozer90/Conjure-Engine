#include "conjure/audio/audio_types.h"

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr std::string_view UNKNOWN_NAME = "UNKNOWN";
}

std::string_view conjure::audio::to_string(sample_format value) noexcept {
    switch (value) {
        case sample_format::F32:
            return "F32";
    }
    return UNKNOWN_NAME;
}

std::string_view conjure::audio::to_string(clip_storage value) noexcept {
    switch (value) {
        case clip_storage::PCM:
            return "PCM";
        case clip_storage::STREAMED:
            return "STREAMED";
    }
    return UNKNOWN_NAME;
}

std::string_view conjure::audio::to_string(clip_encoding value) noexcept {
    switch (value) {
        case clip_encoding::UNKNOWN:
            return "UNKNOWN";
        case clip_encoding::WAV:
            return "WAV";
        case clip_encoding::OGG_VORBIS:
            return "OGG_VORBIS";
        case clip_encoding::MP3:
            return "MP3";
        case clip_encoding::FLAC:
            return "FLAC";
    }
    return UNKNOWN_NAME;
}

std::string_view conjure::audio::to_string(audio_backend_kind value) noexcept {
    switch (value) {
        case audio_backend_kind::AUTO:
            return "AUTO";
        case audio_backend_kind::MINIAUDIO:
            return "MINIAUDIO";
        case audio_backend_kind::NULL_BACKEND:
            return "NULL_BACKEND";
    }
    return UNKNOWN_NAME;
}

std::string_view conjure::audio::to_string(device_state value) noexcept {
    switch (value) {
        case device_state::STOPPED:
            return "STOPPED";
        case device_state::STARTING:
            return "STARTING";
        case device_state::RUNNING:
            return "RUNNING";
        case device_state::LOST:
            return "LOST";
    }
    return UNKNOWN_NAME;
}

std::string_view conjure::audio::to_string(voice_state value) noexcept {
    switch (value) {
        case voice_state::FREE:
            return "FREE";
        case voice_state::PRIMING:
            return "PRIMING";
        case voice_state::PLAYING:
            return "PLAYING";
        case voice_state::PAUSED:
            return "PAUSED";
        case voice_state::STOPPING:
            return "STOPPING";
        case voice_state::FINISHED:
            return "FINISHED";
        case voice_state::STOPPED:
            return "STOPPED";
    }
    return UNKNOWN_NAME;
}

std::string_view conjure::audio::to_string(attenuation_model value) noexcept {
    switch (value) {
        case attenuation_model::NONE:
            return "NONE";
        case attenuation_model::INVERSE:
            return "INVERSE";
        case attenuation_model::LINEAR:
            return "LINEAR";
        case attenuation_model::EXPONENTIAL:
            return "EXPONENTIAL";
    }
    return UNKNOWN_NAME;
}
