#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/uuid.h"

#include <string>
#include <string_view>
#include <type_traits>

// components.h (docs/design/02_renderer.md 4.2, R7): plain reflected structs the engine registers
// into the ECS; this module never includes ecs. Registration, type names and the JSON round trip
// through the reflection system are what the tests pin.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    template <typename T>
    [[nodiscard]] std::string to_json_text(const T &value) {
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(core::reflect::to_json(value, out));
        return out.finish();
    }

    template <typename T>
    [[nodiscard]] T from_json_text(std::string_view text) {
        const core::result<core::json::document> document = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(document);
        T value = {};
        CONJURE_REQUIRE_OK(core::reflect::from_json(value, document.value().root()));
        return value;
    }
}

TEST_SUITE("renderer/components") {

    TEST_CASE("components - the defaults match the render_scene structs they map onto") {
        const mesh_renderer_component meshRenderer = {};
        CHECK_FALSE(meshRenderer.mesh.is_bound());
        CHECK(meshRenderer.mesh.id().is_null());
        CHECK(meshRenderer.layerMask == 1);
        CHECK(meshRenderer.castsShadows);
        CHECK(meshRenderer.receivesShadows);
        const sprite_component spriteValue = {};
        CONJURE_CHECK_APPROX(spriteValue.uvRect, math::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        CONJURE_CHECK_APPROX(spriteValue.colour, math::vec4(1.0f));
        CHECK(spriteValue.space == sprite_space::WORLD);
        const camera_component cameraValue = {};
        CHECK(cameraValue.cameraData == camera());
        CHECK(cameraValue.layerMask == 0xFFFFFFFFu);
        CHECK(cameraValue.priority == 0);
        CHECK(cameraValue.isMain);
        CHECK(cameraValue.postProcess);
        const light_component lightValue = {};
        CHECK(lightValue.lightData == light());
        const environment_component environmentValue = {};
        CHECK(environmentValue.intensity == doctest::Approx(1.0f));
        CHECK(environmentValue.rotationRadians == doctest::Approx(0.0f));
        static_assert(std::is_trivially_copyable_v<mesh_renderer_component>, "memcpy relocated");
        static_assert(std::is_trivially_copyable_v<environment_component>, "memcpy relocated");
    }

    TEST_CASE("make_camera_view - the view is the rigid inverse of the camera's world matrix with its settings") {
        camera_component cameraValue = {};
        cameraValue.cameraData.projection = projection_type::ORTHOGRAPHIC;
        cameraValue.cameraData.orthographicHeight = 6.0f;
        cameraValue.layerMask = 0x5u;
        cameraValue.postProcess = false;

        // At (1, 2, 3), turned 90 degrees about +y (so it looks along -x) and scaled: the scale must not reach
        // the view matrix, or a scaled parent would warp the projection.
        const math::mat4 rotation = glm::rotate(math::mat4(1.0f), math::HALF_PI, math::vec3(0.0f, 1.0f, 0.0f));
        const math::mat4 worldFromCamera = glm::translate(math::mat4(1.0f), math::vec3(1.0f, 2.0f, 3.0f)) * rotation *
          glm::scale(math::mat4(1.0f), math::vec3(3.0f, 0.5f, 2.0f));
        const render_target_id target = render_target_id::from_u64(0x0000000100000007ull);
        const render_view view = make_camera_view(cameraValue, worldFromCamera, target);

        CONJURE_CHECK_APPROX(view.cameraPosition, math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(view.cameraData == cameraValue.cameraData);
        CHECK(view.layerMask == 0x5u);
        CHECK(view.target == target);
        CHECK(view.flags == view_flags::NO_POST_PROCESS);
        // The camera's own position is the view-space origin, a point one unit ahead of it (along world -x)
        // is one unit down the view's -z axis, and world up stays up.
        CONJURE_CHECK_APPROX(math::vec3(view.viewFromWorld * math::vec4(1.0f, 2.0f, 3.0f, 1.0f)), math::vec3(0.0f));
        CONJURE_CHECK_APPROX(
          math::vec3(view.viewFromWorld * math::vec4(0.0f, 2.0f, 3.0f, 1.0f)),
          math::vec3(0.0f, 0.0f, -1.0f)
        );
        CONJURE_CHECK_APPROX(
          math::vec3(view.viewFromWorld * math::vec4(1.0f, 3.0f, 3.0f, 1.0f)),
          math::vec3(0.0f, 1.0f, 0.0f)
        );

        // The defaults: the swapchain, every layer, post-processing on.
        const render_view plain = make_camera_view(camera_component{}, math::mat4(1.0f));
        CHECK_FALSE(plain.target.is_valid());
        CHECK(plain.flags == view_flags::NONE);
        CHECK(plain.layerMask == 0xFFFFFFFFu);
        CONJURE_CHECK_APPROX(plain.viewFromWorld, math::mat4(1.0f));
    }

    TEST_CASE("components - the reflected type names are the documented ones") {
        CHECK(core::reflect::reflect_traits<mesh_renderer_component>::NAME == MESH_RENDERER_COMPONENT_TYPE_NAME);
        CHECK(core::reflect::reflect_traits<sprite_component>::NAME == SPRITE_COMPONENT_TYPE_NAME);
        CHECK(core::reflect::reflect_traits<camera_component>::NAME == CAMERA_COMPONENT_TYPE_NAME);
        CHECK(core::reflect::reflect_traits<light_component>::NAME == LIGHT_COMPONENT_TYPE_NAME);
        CHECK(core::reflect::reflect_traits<environment_component>::NAME == ENVIRONMENT_COMPONENT_TYPE_NAME);
        CHECK(MESH_RENDERER_COMPONENT_TYPE_NAME == "conjure.renderer.mesh_renderer");
        CHECK(SPRITE_COMPONENT_TYPE_NAME == "conjure.renderer.sprite");
        CHECK(CAMERA_COMPONENT_TYPE_NAME == "conjure.renderer.camera");
        CHECK(LIGHT_COMPONENT_TYPE_NAME == "conjure.renderer.light");
        CHECK(ENVIRONMENT_COMPONENT_TYPE_NAME == "conjure.renderer.environment");
        // The nested structs reflect under their own names so the editor can show them inline.
        CHECK(core::reflect::reflect_traits<camera>::NAME == "conjure.renderer.camera_data");
        CHECK(core::reflect::reflect_traits<light>::NAME == "conjure.renderer.light_data");
    }

    TEST_CASE("register_components - registers the enums, the nested structs and the five components once") {
        core::reflect::type_registry registry = {};
        CONJURE_REQUIRE_OK(register_components(registry));
        CHECK(registry.find(MESH_RENDERER_COMPONENT_TYPE_NAME) != nullptr);
        CHECK(registry.find(SPRITE_COMPONENT_TYPE_NAME) != nullptr);
        CHECK(registry.find(CAMERA_COMPONENT_TYPE_NAME) != nullptr);
        CHECK(registry.find(LIGHT_COMPONENT_TYPE_NAME) != nullptr);
        CHECK(registry.find(ENVIRONMENT_COMPONENT_TYPE_NAME) != nullptr);
        CHECK(registry.find("conjure.renderer.camera_data") != nullptr);
        CHECK(registry.find("conjure.renderer.light_data") != nullptr);
        CHECK(registry.find_enum("conjure.renderer.projection_type") != nullptr);
        CHECK(registry.find_enum("conjure.renderer.light_type") != nullptr);
        CHECK(registry.find_enum("conjure.renderer.shadow_filter") != nullptr);
        CHECK(registry.find_enum("conjure.renderer.sprite_space") != nullptr);
        const usize types = registry.type_count();
        const usize enums = registry.enum_count();
        CHECK(types >= 7);
        CHECK(enums >= 4);
        // Registering again is harmless and adds nothing.
        CONJURE_REQUIRE_OK(register_components(registry));
        CHECK(registry.type_count() == types);
        CHECK(registry.enum_count() == enums);
    }

    TEST_CASE("register_components - the component descriptions list every field with its limits") {
        core::reflect::type_registry registry = {};
        CONJURE_REQUIRE_OK(register_components(registry));
        const core::reflect::type_info *meshRenderer = registry.find(MESH_RENDERER_COMPONENT_TYPE_NAME);
        REQUIRE(meshRenderer != nullptr);
        CHECK(meshRenderer->fields.size() == 5);
        CHECK(meshRenderer->fields[0].name == "mesh");
        CHECK(meshRenderer->fields[1].name == "materials");
        const core::reflect::type_info *lightData = registry.find("conjure.renderer.light_data");
        REQUIRE(lightData != nullptr);
        CHECK(lightData->fields.size() == 12);
        bool foundResolution = false;
        for (const core::reflect::field_info &field : lightData->fields) {
            if (field.name == "shadowResolution") {
                foundResolution = true;
                REQUIRE(field.attributes.min.has_value());
                REQUIRE(field.attributes.max.has_value());
                CHECK(*field.attributes.min == doctest::Approx(256.0));
                CHECK(*field.attributes.max == doctest::Approx(1024.0));
            }
        }
        CHECK(foundResolution);
        const core::reflect::type_info *cameraComponent = registry.find(CAMERA_COMPONENT_TYPE_NAME);
        REQUIRE(cameraComponent != nullptr);
        CHECK(cameraComponent->fields.size() == 5);
        CHECK(cameraComponent->fields[0].name == "cameraData");
    }

    TEST_CASE("components - camera and light components round-trip through reflected JSON") {
        camera_component cameraValue = {};
        cameraValue.cameraData.projection = projection_type::ORTHOGRAPHIC;
        cameraValue.cameraData.orthographicHeight = 12.5f;
        cameraValue.cameraData.nearPlane = 0.5f;
        cameraValue.cameraData.farPlane = 250.0f;
        cameraValue.layerMask = 0x0000FF00u;
        cameraValue.priority = -3;
        cameraValue.isMain = false;
        cameraValue.postProcess = false;
        const std::string cameraText = to_json_text(cameraValue);
        CHECK(cameraText.find("\"ORTHOGRAPHIC\"") != std::string::npos);
        CHECK(cameraText.find("\"cameraData\"") != std::string::npos);
        const camera_component cameraRestored = from_json_text<camera_component>(cameraText);
        CHECK(cameraRestored == cameraValue);

        light_component lightValue = {};
        lightValue.lightData.type = light_type::SPOT;
        lightValue.lightData.position = math::vec3(1.0f, 2.0f, 3.0f);
        lightValue.lightData.direction = math::vec3(0.0f, 0.0f, -1.0f);
        lightValue.lightData.colour = math::vec3(0.5f, 0.25f, 1.0f);
        lightValue.lightData.intensity = 800.0f;
        lightValue.lightData.range = 15.0f;
        lightValue.lightData.innerConeRadians = 0.2f;
        lightValue.lightData.outerConeRadians = 0.5f;
        lightValue.lightData.sourceRadius = 0.1f;
        lightValue.lightData.castsShadows = true;
        lightValue.lightData.filter = shadow_filter::PCSS;
        lightValue.lightData.shadowResolution = 1024;
        const std::string lightText = to_json_text(lightValue);
        CHECK(lightText.find("\"SPOT\"") != std::string::npos);
        CHECK(lightText.find("\"PCSS\"") != std::string::npos);
        const light_component lightRestored = from_json_text<light_component>(lightText);
        CHECK(lightRestored == lightValue);
    }

    TEST_CASE("components - asset references serialise as uuids and read back unbound") {
        mesh_renderer_component meshRenderer = {};
        meshRenderer.mesh = assets::asset_ref<mesh_asset>(core::uuid(0x1122334455667788ull, 0x99AABBCCDDEEFF00ull));
        meshRenderer.materials[0] = assets::asset_ref<material_asset>(core::uuid(1, 2));
        meshRenderer.materials[3] = assets::asset_ref<material_asset>(core::uuid(3, 4));
        meshRenderer.layerMask = 6;
        meshRenderer.castsShadows = false;
        const std::string text = to_json_text(meshRenderer);
        CHECK(text.find(core::uuid(0x1122334455667788ull, 0x99AABBCCDDEEFF00ull).to_string()) != std::string::npos);
        CHECK(text.find(core::uuid(3, 4).to_string()) != std::string::npos);

        // A bound destination reads back unbound with the stored uuid.
        assets::asset_slot slot = {};
        mesh_renderer_component restored = {};
        restored.mesh = assets::asset_ref<mesh_asset>(core::uuid(9, 9), &slot);
        CHECK(restored.mesh.is_bound());
        const core::result<core::json::document> document = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(document);
        CONJURE_REQUIRE_OK(core::reflect::from_json(restored, document.value().root()));
        CHECK_FALSE(restored.mesh.is_bound());
        CHECK(restored.mesh.id() == meshRenderer.mesh.id());
        CHECK(restored.materials[0].id() == core::uuid(1, 2));
        CHECK(restored.materials[3].id() == core::uuid(3, 4));
        CHECK(restored.materials[1].id().is_null());
        CHECK(restored.layerMask == 6);
        CHECK_FALSE(restored.castsShadows);
        CHECK(restored.receivesShadows);

        sprite_component spriteValue = {};
        spriteValue.texture = assets::asset_ref<texture_asset>(core::uuid(5, 6));
        spriteValue.space = sprite_space::SCREEN;
        spriteValue.layer = -2;
        spriteValue.order = 7;
        spriteValue.size = math::vec2(64.0f, 32.0f);
        const sprite_component spriteRestored = from_json_text<sprite_component>(to_json_text(spriteValue));
        CHECK(spriteRestored == spriteValue);

        environment_component environmentValue = {};
        environmentValue.environment = assets::asset_ref<environment_asset>(core::uuid(7, 8));
        environmentValue.intensity = 0.5f;
        environmentValue.rotationRadians = 1.5f;
        const environment_component environmentRestored = from_json_text<environment_component>(
          to_json_text(environmentValue)
        );
        CHECK(environmentRestored == environmentValue);
    }
}
