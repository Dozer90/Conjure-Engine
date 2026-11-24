#pragma once

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"

#include "conjure/core/api.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>
#include <vector>

// Rendering settings (docs/design/02_renderer.md 2.3, 3.4, 3.7, 3.8): the post-process, shadow and
// SSAO groups with their defaults and ranges, the project.json "rendering.defaults" reader and writers,
// and the live project defaults. The fields are ordered floats first so the structs carry no padding
// between members; nothing persisted holds them in binary form, only by field name.

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
    /// The look of the AgX operator (tonemap_operator::AGX only): neutral, golden or punchy.
    /// </summary>
    enum class agx_look : u8 {
        DEFAULT = 0,
        GOLDEN,
        PUNCHY
    };

    /// <summary>
    /// The settings groups, in the order render_settings holds them. Each group is overridden as a whole by
    /// the scene and camera components. COUNT is the number of groups, not a group.
    /// </summary>
    enum class render_settings_group : u8 {
        POST_PROCESS = 0,
        SHADOWS,
        SSAO,
        COUNT
    };

    /// <summary>
    /// The groups a view replaces with its own values instead of the scene's (render_view::settingsOverride).
    /// Bit 2 is kept free for a later group.
    /// </summary>
    enum class view_settings_override_flags : u8 {
        NONE = 0,
        POST_PROCESS = 1 << 0,
        SSAO = 1 << 1
    };
    CONJURE_ENUM_FLAGS(view_settings_override_flags);

    /*---------------------------------------------------------------------------*/
    // Ranges: one source for sanitise() and the reflected field limits.

    inline constexpr f32 MIN_EV100 = -16.0f;                    // manualEv100, minEv100 and maxEv100 (EV100)
    inline constexpr f32 MAX_EV100 = 24.0f;
    inline constexpr f32 MAX_EXPOSURE_COMPENSATION = 10.0f;     // EV, symmetric around 0
    inline constexpr f32 MAX_ADAPTATION_SPEED = 100.0f;         // per second
    inline constexpr f32 MAX_BLOOM_INTENSITY = 1.0f;
    inline constexpr f32 MIN_REINHARD_WHITE_POINT = 1.0f;
    inline constexpr f32 MAX_REINHARD_WHITE_POINT = 100.0f;
    inline constexpr f32 MIN_SHADOW_DISTANCE = 1.0f;            // metres
    inline constexpr f32 MAX_SHADOW_DISTANCE = 10000.0f;        // metres
    inline constexpr f32 MAX_CASTER_PADDING = 1000.0f;          // metres
    inline constexpr f32 MAX_SHADOW_FILTER_TEXELS = 8.0f;       // PCF radius and normal offset (02 3.4)
    inline constexpr f32 MIN_SSAO_RADIUS = 0.05f;               // metres
    inline constexpr f32 MAX_SSAO_RADIUS = 10.0f;               // metres
    inline constexpr f32 MAX_SSAO_INTENSITY = 8.0f;

    /// <summary>
    /// HDR chain settings: exposure, bloom and the tonemap operator (R11). 36 bytes.
    /// </summary>
    struct post_process_settings {
        f32 manualEv100 = 12.0f;                        // EV100 of the scene, MANUAL only    [-16, 24]
        f32 exposureCompensation = 0.0f;                // EV added in both modes             [-10, 10]
        f32 adaptationSpeedUp = 3.0f;                   // per second, brightening            [0, 100]
        f32 adaptationSpeedDown = 1.0f;                 // per second, darkening              [0, 100]
        f32 minEv100 = -10.0f;                          // AUTOMATIC lower bound (EV100)      [-16, 24]
        f32 maxEv100 = 16.0f;                           // AUTOMATIC upper bound, >= minEv100 [-16, 24]
        f32 bloomIntensity = 0.04f;                     //                                    [0, 1]
        f32 reinhardWhitePoint = 4.0f;                  // linear                             [1, 100]
        exposure_mode exposure = exposure_mode::AUTOMATIC;
        tonemap_operator tonemap = tonemap_operator::ACES_FITTED;
        agx_look agxLook = agx_look::DEFAULT;
        bool bloomEnabled = true;

        [[nodiscard]] bool operator==(const post_process_settings &other) const noexcept = default;
    };

    /// <summary>
    /// Shadow settings shared by every light and every view (R9, 3.4). 28 bytes.
    /// </summary>
    struct shadow_settings {
        f32 distance = 100.0f;                          // metres covered by the cascades     [1, 10000]
        f32 splitLambda = 0.7f;                         // lerp(uniform, logarithmic, lambda) [0, 1]
        f32 casterPadding = 50.0f;                      // metres the depth range extends toward the light [0, 1000]
        f32 pcfRadiusTexels = 1.5f;                     //                                    [0, 8]
        f32 normalOffsetTexels = 1.5f;                  //                                    [0, 8]
        u32 cascadeCount = MAX_SHADOW_CASCADES;         //                                    [1, MAX_SHADOW_CASCADES]
        shadow_filter defaultFilter = shadow_filter::PCF;   // HARD, PCF or PCSS; sanitise() turns DEFAULT into PCF
        bool enabled = true;

        [[nodiscard]] bool operator==(const shadow_settings &other) const noexcept = default;
    };

    /// <summary>
    /// GTAO settings (R10, 3.7). 16 bytes.
    /// </summary>
    struct ssao_settings {
        f32 radius = 1.0f;                              // metres                             [0.05, 10]
        f32 intensity = 1.0f;                           // ao = pow(visibility, intensity)    [0, 8]
        f32 thickness = 0.5f;                           // thickness heuristic of the horizon search [0, 1]
        bool enabled = true;

        [[nodiscard]] bool operator==(const ssao_settings &other) const noexcept = default;
    };

    /// <summary>
    /// What the project and scene layers resolve, one member per render_settings_group (80 bytes). Not
    /// engine_config.h's rendering_settings, which holds the backend and validation choices.
    /// </summary>
    struct render_settings {
        post_process_settings postProcess = {};
        shadow_settings shadows = {};
        ssao_settings ssao = {};

        [[nodiscard]] bool operator==(const render_settings &other) const noexcept = default;
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
    /// Clamps every field of the group into its range: a NaN takes the field's default, an infinity the
    /// bound it passes, maxEv100 below minEv100 becomes minEv100 and an enumerator outside its enum takes
    /// the default one. Returns the project.json key of the first field it changed ("bloom_intensity", in
    /// declaration order), or "" when the value was already valid.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view sanitise(post_process_settings &value) noexcept;

    /// <summary>
    /// As the post-process form; defaultFilter DEFAULT (a per-light choice, 02 R9) also becomes PCF, so it
    /// never survives.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view sanitise(shadow_settings &value) noexcept;

    /// <summary>
    /// As the post-process form, for the SSAO group.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view sanitise(ssao_settings &value) noexcept;

    /// <summary>
    /// One problem of a "rendering.defaults" value: the slash path of the member ("rendering/defaults/ssao/radius")
    /// and what is wrong with it.
    /// </summary>
    struct render_settings_problem {
        std::string path = {};
        std::string reason = {};
    };

    /// <summary>
    /// What read_render_settings() found.
    /// </summary>
    struct render_settings_reading {
        render_settings settings = {};                  // engine defaults with every valid present leaf applied
        std::vector<render_settings_problem> errors = {};   // wrong type, out of range, unknown enumerator, ...
        std::vector<std::string> unknownKeys = {};      // full slash paths of members the reader does not know
    };

    /// <summary>
    /// Reads a project.json "rendering.defaults" value: an object with the optional members "post_process",
    /// "shadows" and "ssao", each an object of snake_case keys (render_settings_to_json lists them all).
    /// Numbers must be finite and inside the field's range, whole for cascade_count; booleans are true or
    /// false; enumerators are strings matched without regard to ASCII case. Never fails: a bad leaf keeps its
    /// engine default and is listed in errors, a group that is not an object keeps all its defaults, and
    /// max_ev100 below min_ev100 (after the leaves are read) puts both back to their defaults with one error
    /// at max_ev100 (at min_ev100 when only that key is present). Unknown keys are listed, never errors. A
    /// null or absent (invalid) value means engine defaults; any other non-object is an error at
    /// "rendering/defaults". The result always passes sanitise() unchanged.
    /// </summary>
    [[nodiscard]] CONJURE_API render_settings_reading read_render_settings(const core::json::value &defaults);

    /// <summary>
    /// The strict form of read_render_settings(): the settings, or the first error as
    /// renderer_error::INVALID_SETTINGS with the message "&lt;path&gt;: &lt;reason&gt;". Unknown keys are
    /// not errors.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<render_settings> render_settings_from_json(
      const core::json::value &defaults
    );

    /// <summary>
    /// One leaf in project.json form, the edit payload of the project settings panel: the group's key holding
    /// one member, the field's snake_case key, with the value as reflection JSON gives it ("bloomIntensity",
    /// 0.06 gives {"post_process": {"bloom_intensity": 0.06}}). An enumerator string is written lower case;
    /// every other value, null included (a merge patch that removes the key), is copied as it is, and an
    /// invalid value is written as null. A group outside the enum or a field the group does not have asserts
    /// and gives an empty object.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document render_setting_leaf_to_json(
      render_settings_group group,
      std::string_view camelField,
      const core::json::value &reflectedValue
    );

    /// <summary>
    /// Every field of every group in project.json form (snake_case keys, enumerators lower case), which
    /// read_render_settings() reads back to the same settings. An enumerator value outside its enum is
    /// written as its number, which the reader rejects.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document render_settings_to_json(const render_settings &settings);

    /// <summary>
    /// The enumerator name of an operator ("ACES_FITTED").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(tonemap_operator value) noexcept;

    /// <summary>
    /// The enumerator name of an exposure mode ("AUTOMATIC").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(exposure_mode value) noexcept;

    /// <summary>
    /// The enumerator name of an AgX look ("GOLDEN"); "DEFAULT" for values outside the enum.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(agx_look value) noexcept;

    /// <summary>
    /// The project.json key of a group ("post_process", "shadows", "ssao"); "" for COUNT and values outside
    /// the enum.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(render_settings_group value) noexcept;

    /// <summary>
    /// The live project defaults (project.json "rendering.defaults"), for the engine to own and publish.
    /// Not synchronised: set() and get() belong to the main thread.
    /// </summary>
    class CONJURE_API render_defaults {
        private:
            render_settings mValue = {};
            u64 mRevision = 0;

        public:
            /// <summary>
            /// The current defaults; engine defaults until the first set().
            /// </summary>
            [[nodiscard]] const render_settings &get() const noexcept {
                return mValue;
            }

            /// <summary>
            /// Bumps on every set(), also when the value does not change; 0 before the first.
            /// </summary>
            [[nodiscard]] u64 revision() const noexcept {
                return mRevision;
            }

            /// <summary>
            /// Sanitises a copy of the value (callers pass validated settings; this is the backstop) and stores
            /// it. Main thread only.
            /// </summary>
            void set(const render_settings &value) noexcept;
    };
}

CONJURE_REFLECT_ENUM(conjure::renderer::shadow_filter, "conjure.renderer.shadow_filter", DEFAULT, HARD, PCF, PCSS);
CONJURE_REFLECT_ENUM(conjure::renderer::exposure_mode, "conjure.renderer.exposure_mode", AUTOMATIC, MANUAL);
CONJURE_REFLECT_ENUM(conjure::renderer::tonemap_operator, "conjure.renderer.tonemap_operator",
    ACES_FITTED, REINHARD_EXTENDED, AGX, NONE);
CONJURE_REFLECT_ENUM(conjure::renderer::agx_look, "conjure.renderer.agx_look", DEFAULT, GOLDEN, PUNCHY);
CONJURE_REFLECT_STRUCT(conjure::renderer::post_process_settings, "conjure.renderer.post_process_settings",
    CONJURE_FIELD(manualEv100, .min = conjure::renderer::MIN_EV100, .max = conjure::renderer::MAX_EV100,
      .tooltip = "EV100 (exposure value at ISO 100) of the scene; manual exposure only"),
    CONJURE_FIELD(exposureCompensation, .min = -conjure::renderer::MAX_EXPOSURE_COMPENSATION,
      .max = conjure::renderer::MAX_EXPOSURE_COMPENSATION, .tooltip = "EV added to the exposure in both modes"),
    CONJURE_FIELD(adaptationSpeedUp, .min = 0.0, .max = conjure::renderer::MAX_ADAPTATION_SPEED,
      .tooltip = "Per second: how fast automatic exposure adapts to a brighter scene"),
    CONJURE_FIELD(adaptationSpeedDown, .min = 0.0, .max = conjure::renderer::MAX_ADAPTATION_SPEED,
      .tooltip = "Per second: how fast automatic exposure adapts to a darker scene"),
    CONJURE_FIELD(minEv100, .min = conjure::renderer::MIN_EV100, .max = conjure::renderer::MAX_EV100,
      .tooltip = "EV100 (exposure value at ISO 100): the lowest automatic exposure"),
    CONJURE_FIELD(maxEv100, .min = conjure::renderer::MIN_EV100, .max = conjure::renderer::MAX_EV100,
      .tooltip = "EV100 (exposure value at ISO 100): the highest automatic exposure, at least the lowest"),
    CONJURE_FIELD(bloomIntensity, .min = 0.0, .max = conjure::renderer::MAX_BLOOM_INTENSITY,
      .tooltip = "Unitless share of the bloom in the image"),
    CONJURE_FIELD(reinhardWhitePoint, .min = conjure::renderer::MIN_REINHARD_WHITE_POINT,
      .max = conjure::renderer::MAX_REINHARD_WHITE_POINT,
      .tooltip = "Linear luminance that maps to white (extended Reinhard only)"),
    CONJURE_FIELD(exposure, .tooltip = "Automatic (metered) or manual exposure"),
    CONJURE_FIELD(tonemap, .tooltip = "The operator that maps HDR to the display"),
    CONJURE_FIELD(agxLook, .tooltip = "The look of the AgX operator"),
    CONJURE_FIELD(bloomEnabled, .tooltip = "Adds the glow around bright pixels"));
CONJURE_REFLECT_STRUCT(conjure::renderer::shadow_settings, "conjure.renderer.shadow_settings",
    CONJURE_FIELD(distance, .min = conjure::renderer::MIN_SHADOW_DISTANCE,
      .max = conjure::renderer::MAX_SHADOW_DISTANCE, .tooltip = "Metres from the camera the cascades cover"),
    CONJURE_FIELD(splitLambda, .min = 0.0, .max = 1.0,
      .tooltip = "Cascade splits from uniform (0) to logarithmic (1)"),
    CONJURE_FIELD(casterPadding, .min = 0.0, .max = conjure::renderer::MAX_CASTER_PADDING,
      .tooltip = "Metres the cascade depth range extends toward the light, for casters outside the view"),
    CONJURE_FIELD(pcfRadiusTexels, .min = 0.0, .max = conjure::renderer::MAX_SHADOW_FILTER_TEXELS,
      .tooltip = "Texels: the PCF filter radius"),
    CONJURE_FIELD(normalOffsetTexels, .min = 0.0, .max = conjure::renderer::MAX_SHADOW_FILTER_TEXELS,
      .tooltip = "Texels: how far a receiver is pushed along its normal against shadow acne"),
    CONJURE_FIELD(cascadeCount, .min = 1.0,
      .max = static_cast<conjure::f64>(conjure::renderer::MAX_SHADOW_CASCADES),
      .tooltip = "Number of directional shadow cascades"),
    CONJURE_FIELD(defaultFilter, .tooltip = "The filter of lights whose filter is Default: hard, PCF or PCSS"),
    CONJURE_FIELD(enabled, .tooltip = "Renders shadows"));
CONJURE_REFLECT_STRUCT(conjure::renderer::ssao_settings, "conjure.renderer.ssao_settings",
    CONJURE_FIELD(radius, .min = conjure::renderer::MIN_SSAO_RADIUS, .max = conjure::renderer::MAX_SSAO_RADIUS,
      .tooltip = "Metres: the world-space radius of the occlusion search"),
    CONJURE_FIELD(intensity, .min = 0.0, .max = conjure::renderer::MAX_SSAO_INTENSITY,
      .tooltip = "Exponent on the visibility term: ao = pow(visibility, intensity)"),
    CONJURE_FIELD(thickness, .min = 0.0, .max = 1.0, .tooltip = "Thickness heuristic of the horizon search"),
    CONJURE_FIELD(enabled, .tooltip = "Renders ambient occlusion"));
CONJURE_REFLECT_STRUCT(conjure::renderer::render_settings, "conjure.renderer.render_settings_data",
    CONJURE_FIELD(postProcess), CONJURE_FIELD(shadows), CONJURE_FIELD(ssao));
