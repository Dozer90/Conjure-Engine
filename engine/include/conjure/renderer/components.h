#pragma once

#include "conjure/renderer/camera.h"
#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/sprite_batch.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <array>
#include <span>
#include <string_view>
#include <type_traits>

// Component data for the ECS (docs/design/02_renderer.md 4.2, R7). Plain reflected structs; this module
// never includes ecs. The engine's renderer_subsystem registers the mesh renderer, sprite, camera, light and
// environment components, and its extraction system maps them 1:1 onto the render_scene structs, resolving
// asset references through renderer::find_*. The two rendering-settings components (render_settings_component,
// post_process_component) are resolved by the pure functions below, which any caller with ECS access can use.

namespace conjure::renderer {

    struct mesh_renderer_component {
        assets::asset_ref<mesh_asset> mesh = {};
        std::array<assets::asset_ref<material_asset>, MAX_MATERIAL_SLOTS> materials = {};
        u32 layerMask = 1;
        bool castsShadows = true;
        bool receivesShadows = true;

        [[nodiscard]] bool operator==(const mesh_renderer_component &other) const noexcept = default;
    };

    struct sprite_component {
        assets::asset_ref<texture_asset> texture = {};
        math::vec4 uvRect = math::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        math::vec4 colour = math::vec4(1.0f);
        math::vec2 size = math::vec2(1.0f);
        sprite_space space = sprite_space::WORLD;
        i16 layer = 0;
        i16 order = 0;

        [[nodiscard]] bool operator==(const sprite_component &other) const noexcept = default;
    };

    struct camera_component {
        camera cameraData = {};
        u32 layerMask = 0xFFFFFFFFu;
        i32 priority = 0;
        bool isMain = true;
        bool postProcess = true;

        [[nodiscard]] bool operator==(const camera_component &other) const noexcept = default;
    };

    struct light_component {
        light lightData = {};

        [[nodiscard]] bool operator==(const light_component &other) const noexcept = default;
    };

    struct environment_component {
        assets::asset_ref<environment_asset> environment = {};
        f32 intensity = 1.0f;
        f32 rotationRadians = 0.0f;

        [[nodiscard]] bool operator==(const environment_component &other) const noexcept = default;
    };

    /// <summary>
    /// The scene layer of the rendering settings, for any entity (88 bytes). Each override flag makes this
    /// component a candidate for that whole group; per group, the highest priority among the candidates wins
    /// (choose_scene_settings). A component with no flag set takes part in no group.
    /// </summary>
    struct render_settings_component {
        i32 priority = 0;                               // per group; ties go to the lowest scene guid
        bool overridePostProcess = false;
        bool overrideShadows = false;
        bool overrideSsao = false;                      // offset 6; byte 7 is kept free for a later group's flag
        post_process_settings postProcess = {};
        shadow_settings shadows = {};
        ssao_settings ssao = {};

        [[nodiscard]] bool operator==(const render_settings_component &other) const noexcept = default;
    };

    /// <summary>
    /// The camera layer of the rendering settings, for the entity of a camera_component (56 bytes): each
    /// override flag replaces that whole group for the camera's view (resolve_camera_settings, make_camera_view).
    /// Shadows are shared by every view and are not overridable here.
    /// </summary>
    struct post_process_component {
        bool overridePostProcess = false;
        bool overrideSsao = false;                      // bytes 2 and 3 are padding; byte 2 is kept for a later flag
        post_process_settings postProcess = {};
        ssao_settings ssao = {};

        [[nodiscard]] bool operator==(const post_process_component &other) const noexcept = default;
    };

    static_assert(std::is_trivially_copyable_v<mesh_renderer_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<sprite_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<camera_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<light_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<environment_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<render_settings_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<post_process_component>, "components are memcpy relocated");

    inline constexpr std::string_view MESH_RENDERER_COMPONENT_TYPE_NAME = "conjure.renderer.mesh_renderer";
    inline constexpr std::string_view SPRITE_COMPONENT_TYPE_NAME = "conjure.renderer.sprite";
    inline constexpr std::string_view CAMERA_COMPONENT_TYPE_NAME = "conjure.renderer.camera";
    inline constexpr std::string_view LIGHT_COMPONENT_TYPE_NAME = "conjure.renderer.light";
    inline constexpr std::string_view ENVIRONMENT_COMPONENT_TYPE_NAME = "conjure.renderer.environment";
    inline constexpr std::string_view RENDER_SETTINGS_COMPONENT_TYPE_NAME = "conjure.renderer.render_settings";
    inline constexpr std::string_view POST_PROCESS_COMPONENT_TYPE_NAME = "conjure.renderer.post_process";

    /// <summary>
    /// The layer a resolved group came from.
    /// </summary>
    enum class render_settings_source : u8 {
        PROJECT = 0,
        SCENE = 1,
        CAMERA = 2
    };

    inline constexpr u32 NO_SETTINGS_CANDIDATE = 0xFFFFFFFFu;
    inline constexpr usize SETTINGS_GROUP_COUNT = static_cast<usize>(render_settings_group::COUNT);
    // A new group needs one more NO_SETTINGS_CANDIDATE in scene_settings_choice::winner and
    // resolved_render_settings::sceneWinner: an element left out would start as 0, a valid candidate index.
    static_assert(SETTINGS_GROUP_COUNT == 3, "add a NO_SETTINGS_CANDIDATE initialiser for the new settings group");

    /// <summary>
    /// One entity carrying a render_settings_component, as the caller gathered it. The caller leaves out
    /// disabled and inactive entities.
    /// </summary>
    struct scene_settings_candidate {
        const render_settings_component *component = nullptr;   // a null component takes part in no group
        core::uuid guid = {};                           // the entity's scene guid; null = it has none
        u32 entityIndex = 0;
    };

    /// <summary>
    /// The winner of every group, indexed by render_settings_group.
    /// </summary>
    struct scene_settings_choice {
        std::array<u32, SETTINGS_GROUP_COUNT> winner = {
          NO_SETTINGS_CANDIDATE,
          NO_SETTINGS_CANDIDATE,
          NO_SETTINGS_CANDIDATE
        };
        std::array<bool, SETTINGS_GROUP_COUNT> ambiguous = {};   // two or more candidates share the top priority
    };

    /// <summary>
    /// Rendering settings with where each group came from, indexed by render_settings_group.
    /// </summary>
    struct resolved_render_settings {
        render_settings settings = {};                  // the configured values, also under postProcessDisabled
        std::array<render_settings_source, SETTINGS_GROUP_COUNT> source = {};   // PROJECT by default
        // The scene layer's winner and ambiguity per group, also for a group the camera replaced.
        std::array<u32, SETTINGS_GROUP_COUNT> sceneWinner = {
          NO_SETTINGS_CANDIDATE,
          NO_SETTINGS_CANDIDATE,
          NO_SETTINGS_CANDIDATE
        };
        std::array<bool, SETTINGS_GROUP_COUNT> ambiguous = {};
        // camera_component::postProcess is false: settings.postProcess and settings.ssao are what is configured,
        // but the view renders without post-processing and SSAO.
        bool postProcessDisabled = false;
    };

    /// <summary>
    /// The scene winner rule, in one pass over the candidates for every group: per group only the candidates
    /// whose override flag for it is set compete, the highest priority wins, a tie goes to the lowest guid, with
    /// candidates without a guid after every candidate that has one, and then to the lowest entity index. A
    /// group is ambiguous when two or more of its candidates share its top priority. A group without
    /// candidates has no winner (NO_SETTINGS_CANDIDATE). Winners are indices into 'candidates'.
    /// </summary>
    [[nodiscard]] CONJURE_API scene_settings_choice choose_scene_settings(
      std::span<const scene_settings_candidate> candidates
    ) noexcept;

    /// <summary>
    /// The project defaults with each group that has a winner replaced by the winner's group (source SCENE),
    /// with the winners and the ambiguity copied from 'choice'. A winner that is not a candidate with a
    /// component asserts and the group keeps the project's values. The values are not sanitised.
    /// </summary>
    [[nodiscard]] CONJURE_API resolved_render_settings resolve_scene_settings(
      const render_settings &projectDefaults,
      std::span<const scene_settings_candidate> candidates,
      const scene_settings_choice &choice
    ) noexcept;

    /// <summary>
    /// The scene resolution with the camera's override groups applied (source CAMERA; 'overrides' may be null)
    /// and postProcessDisabled set when the camera turns post-processing off. The settings stay the configured
    /// values in that case, so a script can edit the look it would get. sceneWinner and ambiguous stay the scene
    /// layer's.
    /// </summary>
    [[nodiscard]] CONJURE_API resolved_render_settings resolve_camera_settings(
      const resolved_render_settings &scene,
      const camera_component &camera,
      const post_process_component *overrides
    ) noexcept;

    /// <summary>
    /// The view a camera component at a world matrix renders: the view matrix is the rigid inverse of
    /// the matrix (its scale is dropped, so a scaled parent does not warp the projection), with the
    /// camera's position, camera data and layer mask, NO_POST_PROCESS unless postProcess, into 'target'
    /// (invalid = the swapchain). With 'overrides' (the camera entity's post_process_component), each group
    /// whose override flag is set is copied into the view and named in settingsOverride; the other groups keep
    /// their defaults and are not named. The engine's extraction builds the scene cameras' views with it and
    /// the editor's screenshots through a scene camera do too.
    /// </summary>
    [[nodiscard]] CONJURE_API render_view make_camera_view(
      const camera_component &camera,
      const math::mat4 &worldFromCamera,
      render_target_id target = {},
      const post_process_component *overrides = nullptr
    ) noexcept;

    /// <summary>
    /// Registers the enums, the nested camera, light and settings structs (render_settings as
    /// conjure.renderer.render_settings_data) and the seven components with a reflection registry, nested
    /// types first. Registering twice is harmless (ALREADY_EXISTS is skipped); any other registry error is
    /// returned.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_components(core::reflect::type_registry &registry);
}

CONJURE_REFLECT_ENUM(conjure::renderer::projection_type, "conjure.renderer.projection_type", PERSPECTIVE, ORTHOGRAPHIC);
CONJURE_REFLECT_ENUM(conjure::renderer::light_type, "conjure.renderer.light_type", DIRECTIONAL, POINT, SPOT);
CONJURE_REFLECT_ENUM(conjure::renderer::sprite_space, "conjure.renderer.sprite_space", WORLD, SCREEN);
CONJURE_REFLECT_STRUCT(conjure::renderer::camera, "conjure.renderer.camera_data",
    CONJURE_FIELD(projection),
    CONJURE_FIELD(verticalFovRadians, .min = 0.01, .max = 3.1, .unit = conjure::core::reflect::field_unit::RADIANS),
    CONJURE_FIELD(orthographicHeight, .min = 0.001), CONJURE_FIELD(nearPlane, .min = 0.0001),
    CONJURE_FIELD(farPlane, .min = 0.001));
CONJURE_REFLECT_STRUCT(conjure::renderer::light, "conjure.renderer.light_data",
    CONJURE_FIELD(type), CONJURE_FIELD(position), CONJURE_FIELD(direction), CONJURE_FIELD(colour),
    CONJURE_FIELD(intensity, .min = 0.0), CONJURE_FIELD(range, .min = 0.0),
    CONJURE_FIELD(innerConeRadians, .min = 0.0, .max = 1.5708, .unit = conjure::core::reflect::field_unit::RADIANS),
    CONJURE_FIELD(outerConeRadians, .min = 0.0, .max = 1.5708, .unit = conjure::core::reflect::field_unit::RADIANS),
    CONJURE_FIELD(sourceRadius, .min = 0.0), CONJURE_FIELD(castsShadows), CONJURE_FIELD(filter),
    CONJURE_FIELD(shadowResolution, .min = 256.0, .max = 1024.0));
CONJURE_REFLECT_STRUCT(conjure::renderer::mesh_renderer_component, "conjure.renderer.mesh_renderer",
    CONJURE_FIELD(mesh), CONJURE_FIELD(materials), CONJURE_FIELD(layerMask), CONJURE_FIELD(castsShadows),
    CONJURE_FIELD(receivesShadows));
CONJURE_REFLECT_STRUCT(conjure::renderer::sprite_component, "conjure.renderer.sprite",
    CONJURE_FIELD(texture), CONJURE_FIELD(uvRect), CONJURE_FIELD(colour), CONJURE_FIELD(size), CONJURE_FIELD(space),
    CONJURE_FIELD(layer), CONJURE_FIELD(order));
CONJURE_REFLECT_STRUCT(conjure::renderer::camera_component, "conjure.renderer.camera",
    CONJURE_FIELD(cameraData), CONJURE_FIELD(layerMask), CONJURE_FIELD(priority), CONJURE_FIELD(isMain),
    CONJURE_FIELD(postProcess));
CONJURE_REFLECT_STRUCT(conjure::renderer::light_component, "conjure.renderer.light", CONJURE_FIELD(lightData));
CONJURE_REFLECT_STRUCT(conjure::renderer::environment_component, "conjure.renderer.environment",
    CONJURE_FIELD(environment), CONJURE_FIELD(intensity, .min = 0.0),
    CONJURE_FIELD(rotationRadians, .unit = conjure::core::reflect::field_unit::RADIANS));
CONJURE_REFLECT_STRUCT(conjure::renderer::render_settings_component, "conjure.renderer.render_settings",
    CONJURE_FIELD(priority, .tooltip = "Per group, the highest priority among the overriding components wins"),
    CONJURE_FIELD(overridePostProcess, .tooltip = "Replaces the whole post-process group of the layer below"),
    CONJURE_FIELD(overrideShadows, .tooltip = "Replaces the whole shadows group of the layer below"),
    CONJURE_FIELD(overrideSsao, .tooltip = "Replaces the whole ambient occlusion group of the layer below"),
    CONJURE_FIELD(postProcess), CONJURE_FIELD(shadows), CONJURE_FIELD(ssao));
CONJURE_REFLECT_STRUCT(conjure::renderer::post_process_component, "conjure.renderer.post_process",
    CONJURE_FIELD(overridePostProcess, .tooltip = "Replaces the whole post-process group for this camera"),
    CONJURE_FIELD(overrideSsao, .tooltip = "Replaces the whole ambient occlusion group for this camera"),
    CONJURE_FIELD(postProcess), CONJURE_FIELD(ssao));
