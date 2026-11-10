#pragma once

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"

#include "conjure/core/types.h"

#include <string_view>

// Per-scene rendering settings carried by the render_scene (docs/design/02_renderer.md 2.3, 3.4,
// 3.7, 3.8). Every value marked "tunable" in the design has its default here.

namespace conjure::renderer {

    enum class tonemap_operator : u8 {
        ACES_FITTED = 0,
        REINHARD_EXTENDED,
        AGX,
        NONE
    };

    enum class exposure_mode : u8 {
        AUTOMATIC = 0,
        MANUAL
    };

    /// <summary>
    /// HDR chain settings: exposure, bloom and the tonemap operator (R11).
    /// </summary>
    struct post_process_settings {
        exposure_mode exposure = exposure_mode::AUTOMATIC;
        f32 manualEv100 = 12.0f;                        // MANUAL mode: EV100 of the scene
        f32 exposureCompensation = 0.0f;                // EV added to the metered value
        f32 adaptationSpeedUp = 3.0f;                   // per second, brightening
        f32 adaptationSpeedDown = 1.0f;                 // per second, darkening
        f32 minEv100 = -10.0f;
        f32 maxEv100 = 16.0f;
        bool bloomEnabled = true;
        f32 bloomIntensity = 0.04f;
        tonemap_operator tonemap = tonemap_operator::ACES_FITTED;
        f32 reinhardWhitePoint = 4.0f;
        f32 agxLook = 0.0f;                             // 0 = default, 1 = golden, 2 = punchy

        [[nodiscard]] bool operator==(const post_process_settings &other) const noexcept = default;
    };

    /// <summary>
    /// Shadow settings shared by every light (R9, 3.4).
    /// </summary>
    struct shadow_settings {
        bool enabled = true;
        f32 distance = 100.0f;                          // metres covered by the cascades
        f32 splitLambda = 0.7f;                         // lerp(uniform, logarithmic, lambda)
        f32 casterPadding = 50.0f;                      // metres the cascade depth range extends toward the light
        f32 pcfRadiusTexels = 1.5f;
        f32 normalOffsetTexels = 1.5f;
        shadow_filter defaultFilter = shadow_filter::PCF;
        u32 cascadeCount = MAX_SHADOW_CASCADES;         // 1..MAX_SHADOW_CASCADES

        [[nodiscard]] bool operator==(const shadow_settings &other) const noexcept = default;
    };

    /// <summary>
    /// GTAO settings (R10, 3.7).
    /// </summary>
    struct ssao_settings {
        bool enabled = true;
        f32 radius = 1.0f;                              // metres
        f32 intensity = 1.0f;                           // ao = pow(visibility, intensity)
        f32 thickness = 0.5f;                           // thickness heuristic of the horizon search

        [[nodiscard]] bool operator==(const ssao_settings &other) const noexcept = default;
    };

    /// <summary>
    /// The image-based lighting environment of the scene.
    /// </summary>
    struct environment_settings {
        gpu_environment_id environment = {};            // invalid = no sky, grey fallback ambient
        f32 intensity = 1.0f;
        f32 rotationRadians = 0.0f;                     // rotation of the sky about the Y axis

        [[nodiscard]] bool operator==(const environment_settings &other) const noexcept = default;
    };

    /// <summary>
    /// The enumerator name of an operator ("ACES_FITTED").
    /// </summary>
    [[nodiscard]] std::string_view to_string(tonemap_operator value) noexcept;

    /// <summary>
    /// The enumerator name of an exposure mode ("AUTOMATIC").
    /// </summary>
    [[nodiscard]] std::string_view to_string(exposure_mode value) noexcept;
}
