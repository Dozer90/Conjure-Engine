#include "conjure/audio/components.h"

#include <cstddef>

// The component structs are plain data described by the reflection macros in the header; this
// translation unit pins their layouts (the C# mirrors of docs/design/15_bindings.md 2.6 copy them
// byte for byte) and holds the one function the header declares.

using namespace conjure;
using namespace conjure::audio;

namespace conjure::audio {

    static_assert(offsetof(audio_listener, gain) == 0, "audio_listener::gain leads the C# mirror");
    static_assert(offsetof(audio_listener, active) == 4, "audio_listener::active follows gain");
    static_assert(offsetof(source_handle, voice) == 0, "source_handle::voice leads the opaque mirror");
    static_assert(offsetof(source_handle, clipVersion) == 8, "source_handle::clipVersion follows voice");
    static_assert(offsetof(source_handle, flags) == 12, "source_handle::flags follows clipVersion");

    static_assert(offsetof(audio_source, clip) == 0, "audio_source::clip leads the C# mirror");
    static_assert(offsetof(audio_source, volume) == 24, "audio_source::volume follows the 24 byte clip reference");
    static_assert(offsetof(audio_source, pitch) == 28, "audio_source::pitch");
    static_assert(offsetof(audio_source, loop) == 32, "audio_source::loop");
    static_assert(offsetof(audio_source, spatial) == 33, "audio_source::spatial");
    static_assert(offsetof(audio_source, playOnStart) == 34, "audio_source::playOnStart");
    static_assert(offsetof(audio_source, oneShot) == 35, "audio_source::oneShot");
    static_assert(offsetof(audio_source, group) == 40, "audio_source::group is 8 byte aligned");
    static_assert(offsetof(audio_source, attenuation) == 48, "audio_source::attenuation");
    static_assert(offsetof(audio_source, minDistance) == 52, "audio_source::minDistance");
    static_assert(offsetof(audio_source, maxDistance) == 56, "audio_source::maxDistance");
    static_assert(offsetof(audio_source, rolloff) == 60, "audio_source::rolloff");
    static_assert(offsetof(audio_source, playbackRequest) == 64, "audio_source::playbackRequest");
    static_assert(offsetof(audio_source, isPlaying) == 65, "audio_source::isPlaying");
    static_assert(offsetof(audio_source, handle) == 68, "audio_source::handle is 4 byte aligned");
}

play_params conjure::audio::play_params_from(const audio_source &source, group_id group) noexcept {
    play_params params = {};
    params.volume = source.volume;
    params.pitch = source.pitch;
    params.pan = 0.0f;
    params.loop = source.loop;
    params.spatial = source.spatial;
    params.retain = true;
    params.group = group;
    params.attenuation = source.attenuation;
    params.minDistance = source.minDistance;
    params.maxDistance = source.maxDistance;
    params.rolloff = source.rolloff;
    return params;
}
