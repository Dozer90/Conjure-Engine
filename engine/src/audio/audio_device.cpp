#include "conjure/audio/audio_device.h"

// backend_desc and audio_device_info are plain data; this translation unit compiles the header as
// part of the module and checks the mix_callback contract once per build.

namespace conjure::audio {

    static_assert(sizeof(mix_callback) == sizeof(void *), "mix_callback is a plain function pointer");
    static_assert(MIX_CHANNELS == 2, "the backends and the mixer agree on stereo output");
}
