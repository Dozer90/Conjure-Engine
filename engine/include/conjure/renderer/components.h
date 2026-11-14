#pragma once

#include "conjure/renderer/camera.h"
#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/sprite_batch.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <string_view>
#include <type_traits>

// Component data the engine's renderer_subsystem registers into the ECS (docs/design/02_renderer.md
// 4.2, R7). Plain reflected structs; this module never includes ecs. The engine's extraction system
// maps them 1:1 onto the render_scene structs, resolving asset references through renderer::find_*.

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

    static_assert(std::is_trivially_copyable_v<mesh_renderer_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<sprite_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<camera_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<light_component>, "components are memcpy relocated");
    static_assert(std::is_trivially_copyable_v<environment_component>, "components are memcpy relocated");

    inline constexpr std::string_view MESH_RENDERER_COMPONENT_TYPE_NAME = "conjure.renderer.mesh_renderer";
    inline constexpr std::string_view SPRITE_COMPONENT_TYPE_NAME = "conjure.renderer.sprite";
    inline constexpr std::string_view CAMERA_COMPONENT_TYPE_NAME = "conjure.renderer.camera";
    inline constexpr std::string_view LIGHT_COMPONENT_TYPE_NAME = "conjure.renderer.light";
    inline constexpr std::string_view ENVIRONMENT_COMPONENT_TYPE_NAME = "conjure.renderer.environment";

    /// <summary>
    /// The view a camera component at a world matrix renders: the view matrix is the rigid inverse of
    /// the matrix (its scale is dropped, so a scaled parent does not warp the projection), with the
    /// camera's position, camera data and layer mask, NO_POST_PROCESS unless postProcess, into 'target'
    /// (invalid = the swapchain). The engine's extraction builds the scene cameras' views with it and
    /// the editor's screenshots through a scene camera do too, so both show the same picture.
    /// </summary>
    [[nodiscard]] CONJURE_API render_view make_camera_view(
      const camera_component &camera,
      const math::mat4 &worldFromCamera,
      render_target_id target = {}
    ) noexcept;

    /// <summary>
    /// Registers the enums, the nested camera and light structs and the five components with a
    /// reflection registry. Registering twice is harmless (ALREADY_EXISTS is skipped); any other
    /// registry error is returned.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_components(core::reflect::type_registry &registry);
}

CONJURE_REFLECT_ENUM(conjure::renderer::projection_type, "conjure.renderer.projection_type", PERSPECTIVE, ORTHOGRAPHIC);
CONJURE_REFLECT_ENUM(conjure::renderer::light_type, "conjure.renderer.light_type", DIRECTIONAL, POINT, SPOT);
CONJURE_REFLECT_ENUM(conjure::renderer::shadow_filter, "conjure.renderer.shadow_filter", DEFAULT, HARD, PCF, PCSS);
CONJURE_REFLECT_ENUM(conjure::renderer::sprite_space, "conjure.renderer.sprite_space", WORLD, SCREEN);
CONJURE_REFLECT_STRUCT(conjure::renderer::camera, "conjure.renderer.camera_data",
    CONJURE_FIELD(projection), CONJURE_FIELD(verticalFovRadians, .min = 0.01, .max = 3.1),
    CONJURE_FIELD(orthographicHeight, .min = 0.001), CONJURE_FIELD(nearPlane, .min = 0.0001),
    CONJURE_FIELD(farPlane, .min = 0.001));
CONJURE_REFLECT_STRUCT(conjure::renderer::light, "conjure.renderer.light_data",
    CONJURE_FIELD(type), CONJURE_FIELD(position), CONJURE_FIELD(direction), CONJURE_FIELD(colour),
    CONJURE_FIELD(intensity, .min = 0.0), CONJURE_FIELD(range, .min = 0.0),
    CONJURE_FIELD(
      innerConeRadians,
      .min = 0.0,
      .max = 1.5708
    ), CONJURE_FIELD(outerConeRadians, .min = 0.0, .max = 1.5708),
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
    CONJURE_FIELD(environment), CONJURE_FIELD(intensity, .min = 0.0), CONJURE_FIELD(rotationRadians));
