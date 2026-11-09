#include "conjure/audio/mix_groups.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::audio;

f32 conjure::audio::clamp_group_gain(f32 gain) noexcept {
    if (!std::isfinite(gain)) {
        return 1.0f;
    }
    return std::clamp(gain, 0.0f, 2.0f);
}
