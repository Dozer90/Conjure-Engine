#pragma once

#include "conjure/core/types.h"

// The exposure formulas the GPU passes implement (docs/design/02_renderer.md R11, 3.8), kept here so
// tests/renderer/test_exposure_math.cpp can pin them and the CPU manual-exposure path shares them.

namespace conjure::renderer::exposure {

    /// <summary>
    /// EV100 of an average scene luminance (candela per square metre): log2(L * 100 / 12.5).
    /// </summary>
    [[nodiscard]] f32 ev100_from_luminance(f32 luminance) noexcept;

    /// <summary>
    /// Inverse of ev100_from_luminance.
    /// </summary>
    [[nodiscard]] f32 luminance_from_ev100(f32 ev100) noexcept;

    /// <summary>
    /// The exposure multiplier of an EV100: 1 / (1.2 * 2^EV100).
    /// </summary>
    [[nodiscard]] f32 exposure_from_ev100(f32 ev100) noexcept;

    /// <summary>
    /// One adaptation step: lerp(previous, target, 1 - exp(-dt * speed)) with the up speed when the
    /// target is brighter (larger EV) and the down speed otherwise.
    /// </summary>
    [[nodiscard]] f32 adapt_ev100(
      f32 previousEv100,
      f32 targetEv100,
      f32 deltaSeconds,
      f32 speedUp,
      f32 speedDown
    ) noexcept;
}
