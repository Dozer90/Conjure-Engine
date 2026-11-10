#include "renderer/exposure_math.h"

#include <algorithm>
#include <cmath>

using namespace conjure;

f32 conjure::renderer::exposure::ev100_from_luminance(f32 luminance) noexcept {
    return std::log2(std::max(luminance, 1.0e-6f) * 100.0f / 12.5f);
}

f32 conjure::renderer::exposure::luminance_from_ev100(f32 ev100) noexcept {
    return std::exp2(ev100) * 12.5f / 100.0f;
}

f32 conjure::renderer::exposure::exposure_from_ev100(f32 ev100) noexcept {
    return 1.0f / (1.2f * std::exp2(ev100));
}

f32 conjure::renderer::exposure::adapt_ev100(
  f32 previousEv100,
  f32 targetEv100,
  f32 deltaSeconds,
  f32 speedUp,
  f32 speedDown
) noexcept {
    const f32 speed = targetEv100 > previousEv100 ? speedUp : speedDown;
    const f32 blend = 1.0f - std::exp(-std::max(deltaSeconds, 0.0f) * std::max(speed, 0.0f));
    return previousEv100 + (targetEv100 - previousEv100) * blend;
}
