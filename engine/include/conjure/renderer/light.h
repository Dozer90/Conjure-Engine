#pragma once

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <string_view>

// Lights as the extraction contract carries them (docs/design/02_renderer.md 2.4). Units follow
// glTF KHR_lights_punctual: lux for directional lights, candela for point and spot lights.

namespace conjure::renderer {

    enum class light_type : u8 {
        DIRECTIONAL = 0,
        POINT,
        SPOT
    };

    /// <summary>
    /// Per-light shadow filter. DEFAULT resolves to shadow_settings::defaultFilter at render time
    /// so the editor can flip every light at once.
    /// </summary>
    enum class shadow_filter : u8 {
        DEFAULT = 0,
        HARD,
        PCF,
        PCSS
    };

    struct light {
        light_type type = light_type::POINT;
        math::vec3 position = math::vec3(0.0f);
        math::vec3 direction = math::vec3(0.0f, -1.0f, 0.0f);
        math::vec3 colour = math::vec3(1.0f);           // linear
        f32 intensity = 10.0f;                          // lux (directional) or candela (point/spot)
        f32 range = 10.0f;
        f32 innerConeRadians = 0.4f;
        f32 outerConeRadians = 0.6f;
        f32 sourceRadius = 0.05f;                       // PCSS penumbra; angular radius for directional
        bool castsShadows = false;
        shadow_filter filter = shadow_filter::DEFAULT;
        u16 shadowResolution = 512;                     // atlas tile request, clamped to {256, 512, 1024}
        // Stable identity across frames so a local light keeps its shadow atlas tile (the engine's
        // extraction writes the entity id; 0 = derive the identity from the light's attributes).
        // Runtime only: not reflected, never serialised.
        u32 entityId = 0;

        [[nodiscard]] bool operator==(const light &other) const noexcept = default;
    };

    /// <summary>
    /// The smallest and largest atlas tile a local light may request.
    /// </summary>
    inline constexpr u16 MIN_SHADOW_TILE_SIZE = 256;
    inline constexpr u16 MAX_SHADOW_TILE_SIZE = 1024;

    /// <summary>
    /// Rounds a requested tile size to the nearest allowed size (256, 512 or 1024).
    /// </summary>
    [[nodiscard]] CONJURE_API u16 clamp_shadow_resolution(u16 requested) noexcept;

    /// <summary>
    /// The enumerator name of a light type ("POINT").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(light_type type) noexcept;

    /// <summary>
    /// The enumerator name of a shadow filter ("PCF").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(shadow_filter filter) noexcept;
}
