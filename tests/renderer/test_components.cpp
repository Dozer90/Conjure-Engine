#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/test_environment.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/random.h"
#include "conjure/core/uuid.h"

#include <array>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

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

    /// <summary>
    /// A scene component overriding the groups given, with values that tell the components apart.
    /// </summary>
    [[nodiscard]] render_settings_component scene_component(
      i32 priority,
      bool postProcess,
      bool shadows,
      bool ssao,
      f32 marker
    ) {
        render_settings_component component = {};
        component.priority = priority;
        component.overridePostProcess = postProcess;
        component.overrideShadows = shadows;
        component.overrideSsao = ssao;
        component.postProcess.bloomIntensity = marker;
        component.shadows.distance = marker * 100.0f;
        component.ssao.radius = marker * 10.0f;
        return component;
    }

    [[nodiscard]] scene_settings_candidate candidate_of(
      const render_settings_component &component,
      core::uuid guid,
      u32 entityIndex
    ) {
        scene_settings_candidate candidate = {};
        candidate.component = &component;
        candidate.guid = guid;
        candidate.entityIndex = entityIndex;
        return candidate;
    }

    constexpr usize POST_PROCESS_GROUP = static_cast<usize>(render_settings_group::POST_PROCESS);
    constexpr usize SHADOWS_GROUP = static_cast<usize>(render_settings_group::SHADOWS);
    constexpr usize SSAO_GROUP = static_cast<usize>(render_settings_group::SSAO);

    [[nodiscard]] post_process_settings random_post_process(core::random::engine &engine) {
        post_process_settings settings = {};
        settings.manualEv100 = core::random::uniform(engine, -30.0f, 30.0f);
        settings.exposureCompensation = core::random::uniform(engine, -12.0f, 12.0f);
        settings.adaptationSpeedUp = core::random::uniform(engine, 0.0f, 120.0f);
        settings.adaptationSpeedDown = core::random::uniform(engine, 0.0f, 120.0f);
        settings.minEv100 = core::random::uniform(engine, -20.0f, 20.0f);
        settings.maxEv100 = core::random::uniform(engine, -20.0f, 30.0f);
        settings.bloomIntensity = core::random::uniform(engine, 0.0f, 2.0f);
        settings.reinhardWhitePoint = core::random::uniform(engine, 0.0f, 200.0f);
        settings.exposure = static_cast<exposure_mode>(core::random::uniform<core::random::engine, u8>(engine, 0, 1));
        settings.tonemap = static_cast<tonemap_operator>(core::random::uniform<core::random::engine, u8>(engine, 0, 3));
        settings.agxLook = static_cast<agx_look>(core::random::uniform<core::random::engine, u8>(engine, 0, 2));
        settings.bloomEnabled = core::random::chance(engine, 0.5);
        return settings;
    }

    [[nodiscard]] ssao_settings random_ssao(core::random::engine &engine) {
        ssao_settings settings = {};
        settings.radius = core::random::uniform(engine, 0.0f, 12.0f);
        settings.intensity = core::random::uniform(engine, 0.0f, 10.0f);
        settings.thickness = core::random::uniform(engine, 0.0f, 1.5f);
        settings.enabled = core::random::chance(engine, 0.5);
        return settings;
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

    TEST_CASE("register_components - registers the enums, the nested structs and the seven components once") {
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
        CHECK(registry.find(RENDER_SETTINGS_COMPONENT_TYPE_NAME) != nullptr);
        CHECK(registry.find(POST_PROCESS_COMPONENT_TYPE_NAME) != nullptr);
        // Two nested camera and light structs, four settings structs and seven components; seven enums.
        const usize types = registry.type_count();
        const usize enums = registry.enum_count();
        CHECK(types == 13);
        CHECK(enums == 7);
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

    TEST_CASE("register_components - the angle fields are stored in radians and say so, the others have no unit") {
        core::reflect::type_registry registry = {};
        CONJURE_REQUIRE_OK(register_components(registry));
        struct angle_field {
            std::string_view type = {};
            std::string_view field = {};
        };
        const angle_field angles[] = {
          {"conjure.renderer.camera_data", "verticalFovRadians"},
          {"conjure.renderer.light_data", "innerConeRadians"},
          {"conjure.renderer.light_data", "outerConeRadians"},
          {"conjure.renderer.environment", "rotationRadians"},
        };
        for (const angle_field &angle : angles) {
            CAPTURE(angle.type);
            CAPTURE(angle.field);
            const core::reflect::type_info *type = registry.find(angle.type);
            REQUIRE(type != nullptr);
            const core::reflect::field_info *field = type->find_field(angle.field);
            REQUIRE(field != nullptr);
            CHECK(field->attributes.unit == core::reflect::field_unit::RADIANS);
        }
        // The limits stay in radians: a light cone ends at a right angle, the field of view short of pi.
        const core::reflect::field_info *outerCone = registry.find("conjure.renderer.light_data")
          ->find_field("outerConeRadians");
        REQUIRE(outerCone != nullptr);
        CHECK(outerCone->attributes.max == doctest::Approx(1.5708));
        const core::reflect::field_info *fov = registry.find("conjure.renderer.camera_data")
          ->find_field("verticalFovRadians");
        REQUIRE(fov != nullptr);
        CHECK(fov->attributes.max == doctest::Approx(3.1));
        for (const std::string_view typeName : {
               std::string_view("conjure.renderer.camera_data"),
               std::string_view("conjure.renderer.light_data"),
               std::string_view("conjure.renderer.environment"),
               MESH_RENDERER_COMPONENT_TYPE_NAME,
               CAMERA_COMPONENT_TYPE_NAME,
             }) {
            const core::reflect::type_info *type = registry.find(typeName);
            REQUIRE(type != nullptr);
            for (const core::reflect::field_info &field : type->fields) {
                if (field.name.ends_with("Radians")) {
                    continue;
                }
                CAPTURE(typeName);
                CAPTURE(field.name);
                CHECK(field.attributes.unit == core::reflect::field_unit::NONE);
            }
        }
    }

    TEST_CASE("render_settings_component - defaults, size and field order are pinned") {
        static_assert(std::is_trivially_copyable_v<render_settings_component>, "memcpy relocated");
        static_assert(sizeof(render_settings_component) == 88);
        CHECK(offsetof(render_settings_component, priority) == 0);
        CHECK(offsetof(render_settings_component, overridePostProcess) == 4);
        CHECK(offsetof(render_settings_component, overrideShadows) == 5);
        CHECK(offsetof(render_settings_component, overrideSsao) == 6);
        // Byte 7 stays padding for a later group's flag.
        CHECK(offsetof(render_settings_component, postProcess) == 8);
        CHECK(offsetof(render_settings_component, shadows) == 44);
        CHECK(offsetof(render_settings_component, ssao) == 72);
        const render_settings_component component = {};
        CHECK(component.priority == 0);
        CHECK_FALSE(component.overridePostProcess);
        CHECK_FALSE(component.overrideShadows);
        CHECK_FALSE(component.overrideSsao);
        CHECK(component.postProcess == post_process_settings());
        CHECK(component.shadows == shadow_settings());
        CHECK(component.ssao == ssao_settings());
        CHECK(core::reflect::reflect_traits<render_settings_component>::NAME == RENDER_SETTINGS_COMPONENT_TYPE_NAME);
        CHECK(RENDER_SETTINGS_COMPONENT_TYPE_NAME == "conjure.renderer.render_settings");

        const core::reflect::type_info description = core::reflect::describe_struct<render_settings_component>();
        const std::vector<std::string> expected = {
          "priority", "overridePostProcess", "overrideShadows", "overrideSsao", "postProcess", "shadows", "ssao"};
        std::vector<std::string> names = {};
        for (const core::reflect::field_info &field : description.fields) {
            names.push_back(field.name);
        }
        CHECK(names == expected);
        CHECK(description.size == 88);
    }

    TEST_CASE("post_process_component - defaults, size and field order are pinned") {
        static_assert(std::is_trivially_copyable_v<post_process_component>, "memcpy relocated");
        static_assert(sizeof(post_process_component) == 56);
        CHECK(offsetof(post_process_component, overridePostProcess) == 0);
        CHECK(offsetof(post_process_component, overrideSsao) == 1);
        // Bytes 2 and 3 stay padding (byte 2 for a later group's flag).
        CHECK(offsetof(post_process_component, postProcess) == 4);
        CHECK(offsetof(post_process_component, ssao) == 40);
        const post_process_component component = {};
        CHECK_FALSE(component.overridePostProcess);
        CHECK_FALSE(component.overrideSsao);
        CHECK(component.postProcess == post_process_settings());
        CHECK(component.ssao == ssao_settings());
        CHECK(core::reflect::reflect_traits<post_process_component>::NAME == POST_PROCESS_COMPONENT_TYPE_NAME);
        CHECK(POST_PROCESS_COMPONENT_TYPE_NAME == "conjure.renderer.post_process");

        const core::reflect::type_info description = core::reflect::describe_struct<post_process_component>();
        const std::vector<std::string> expected = {"overridePostProcess", "overrideSsao", "postProcess", "ssao"};
        std::vector<std::string> names = {};
        for (const core::reflect::field_info &field : description.fields) {
            names.push_back(field.name);
        }
        CHECK(names == expected);
        CHECK(description.size == 56);
    }

    TEST_CASE("register_components - every settings and component name resolves to its own type and none repeats") {
        core::reflect::type_registry registry = {};
        CONJURE_REQUIRE_OK(register_components(registry));
        struct named_size {
            std::string_view name = {};
            usize size = 0;
        };
        const named_size structs[] = {
          {"conjure.renderer.post_process_settings", sizeof(post_process_settings)},
          {"conjure.renderer.shadow_settings", sizeof(shadow_settings)},
          {"conjure.renderer.ssao_settings", sizeof(ssao_settings)},
          {"conjure.renderer.render_settings_data", sizeof(render_settings)},
          {RENDER_SETTINGS_COMPONENT_TYPE_NAME, sizeof(render_settings_component)},
          {POST_PROCESS_COMPONENT_TYPE_NAME, sizeof(post_process_component)},
        };
        for (const named_size &entry : structs) {
            CAPTURE(entry.name);
            const core::reflect::type_info *type = registry.find(entry.name);
            REQUIRE(type != nullptr);
            CHECK(type->size == entry.size);
        }
        const std::string_view enums[] = {
          "conjure.renderer.agx_look",
          "conjure.renderer.exposure_mode",
          "conjure.renderer.tonemap_operator",
          "conjure.renderer.shadow_filter",
        };
        for (const std::string_view name : enums) {
            CAPTURE(name);
            const core::reflect::enum_info *info = registry.find_enum(name);
            REQUIRE(info != nullptr);
            CHECK(info->underlyingSize == 1);
        }

        // Every registered name is distinct, so no description was dropped as a duplicate.
        std::set<std::string> names = {};
        for (const core::reflect::type_info *type : registry.types()) {
            names.insert(type->name);
        }
        for (const core::reflect::enum_info *info : registry.enums()) {
            names.insert(info->name);
        }
        CHECK(names.size() == registry.type_count() + registry.enum_count());

        // Nested settings fields point at the registry's own copies.
        const core::reflect::type_info *component = registry.find(RENDER_SETTINGS_COMPONENT_TYPE_NAME);
        REQUIRE(component != nullptr);
        const core::reflect::field_info *shadows = component->find_field("shadows");
        REQUIRE(shadows != nullptr);
        CHECK(shadows->structType == registry.find("conjure.renderer.shadow_settings"));

        // Registering twice changes nothing.
        const usize typeCount = registry.type_count();
        const usize enumCount = registry.enum_count();
        CONJURE_REQUIRE_OK(register_components(registry));
        CHECK(registry.type_count() == typeCount);
        CHECK(registry.enum_count() == enumCount);
    }

    TEST_CASE("regression - code sweep W3-RS-L1A the settings data does not take the render_settings component name") {
        // register_components skips ALREADY_EXISTS, so a settings struct reflected under a component's name would
        // silently replace the component in the registry.
        CHECK(core::reflect::reflect_traits<render_settings>::NAME != RENDER_SETTINGS_COMPONENT_TYPE_NAME);
        core::reflect::type_registry registry = {};
        CONJURE_REQUIRE_OK(register_components(registry));
        const core::reflect::type_info *component = registry.find(RENDER_SETTINGS_COMPONENT_TYPE_NAME);
        REQUIRE(component != nullptr);
        CHECK(component->size == sizeof(render_settings_component));
        CHECK(component->find_field("priority") != nullptr);
    }

    TEST_CASE("components - the settings components round-trip through reflected JSON") {
        render_settings_component sceneValue = scene_component(5, true, false, true, 0.25f);
        sceneValue.postProcess.tonemap = tonemap_operator::AGX;
        sceneValue.postProcess.agxLook = agx_look::GOLDEN;
        sceneValue.shadows.defaultFilter = shadow_filter::PCSS;
        const std::string sceneText = to_json_text(sceneValue);
        CHECK(sceneText.find("\"GOLDEN\"") != std::string::npos);
        CHECK(sceneText.find("\"overrideShadows\"") != std::string::npos);
        CHECK(from_json_text<render_settings_component>(sceneText) == sceneValue);

        post_process_component cameraValue = {};
        cameraValue.overrideSsao = true;
        cameraValue.ssao.radius = 0.5f;
        cameraValue.postProcess.exposure = exposure_mode::MANUAL;
        CHECK(from_json_text<post_process_component>(to_json_text(cameraValue)) == cameraValue);
    }

    TEST_CASE("choose_scene_settings - a higher priority wins only the groups it overrides") {
        const render_settings_component low = scene_component(0, true, true, true, 0.1f);
        const render_settings_component high = scene_component(10, true, false, false, 0.2f);
        const std::array<scene_settings_candidate, 2> candidates = {
          candidate_of(low, core::uuid(0, 1), 1),
          candidate_of(high, core::uuid(0, 2), 2),
        };
        const scene_settings_choice choice = choose_scene_settings(candidates);
        CHECK(choice.winner[POST_PROCESS_GROUP] == 1);
        CHECK(choice.winner[SHADOWS_GROUP] == 0);
        CHECK(choice.winner[SSAO_GROUP] == 0);
        CHECK(choice.ambiguous == std::array<bool, SETTINGS_GROUP_COUNT>{});
    }

    TEST_CASE("choose_scene_settings - a component without override flags never wins") {
        const render_settings_component inert = scene_component(100, false, false, false, 0.9f);
        const render_settings_component active = scene_component(-5, false, true, false, 0.1f);
        const std::array<scene_settings_candidate, 2> candidates = {
          candidate_of(inert, core::uuid(0, 1), 0),
          candidate_of(active, core::uuid(0, 2), 1),
        };
        const scene_settings_choice choice = choose_scene_settings(candidates);
        CHECK(choice.winner[POST_PROCESS_GROUP] == NO_SETTINGS_CANDIDATE);
        CHECK(choice.winner[SHADOWS_GROUP] == 1);
        CHECK(choice.winner[SSAO_GROUP] == NO_SETTINGS_CANDIDATE);
        const std::array<scene_settings_candidate, 1> alone = {candidate_of(inert, core::uuid(0, 1), 0)};
        const scene_settings_choice none = choose_scene_settings(alone);
        for (usize group = 0; group < SETTINGS_GROUP_COUNT; ++group) {
            CAPTURE(group);
            CHECK(none.winner[group] == NO_SETTINGS_CANDIDATE);
            CHECK_FALSE(none.ambiguous[group]);
        }
    }

    TEST_CASE("choose_scene_settings - two components overriding different groups both apply without ambiguity") {
        // A base scene sets the mood, an additively loaded level the shadows, both at priority 0.
        const render_settings_component mood = scene_component(0, true, false, false, 0.3f);
        const render_settings_component level = scene_component(0, false, true, false, 0.4f);
        const std::array<scene_settings_candidate, 2> candidates = {
          candidate_of(mood, core::uuid(0, 7), 3),
          candidate_of(level, core::uuid(0, 2), 9),
        };
        const scene_settings_choice choice = choose_scene_settings(candidates);
        CHECK(choice.winner[POST_PROCESS_GROUP] == 0);
        CHECK(choice.winner[SHADOWS_GROUP] == 1);
        CHECK(choice.winner[SSAO_GROUP] == NO_SETTINGS_CANDIDATE);
        CHECK(choice.ambiguous == std::array<bool, SETTINGS_GROUP_COUNT>{});
        const resolved_render_settings resolved = resolve_scene_settings(render_settings(), candidates, choice);
        CHECK(resolved.settings.postProcess == mood.postProcess);
        CHECK(resolved.settings.shadows == level.shadows);
        CHECK(resolved.settings.ssao == ssao_settings());
    }

    TEST_CASE("choose_scene_settings - a priority tie goes to the lowest guid, guid-less last, then the entity index") {
        const render_settings_component first = scene_component(3, true, true, true, 0.1f);
        const render_settings_component second = scene_component(3, true, true, true, 0.2f);
        const render_settings_component third = scene_component(3, true, true, true, 0.3f);

        SUBCASE("lowest guid, whatever the order") {
            const std::array<scene_settings_candidate, 3> candidates = {
              candidate_of(first, core::uuid(0, 30), 0),
              candidate_of(second, core::uuid(0, 10), 1),
              candidate_of(third, core::uuid(1, 0), 2),
            };
            const scene_settings_choice choice = choose_scene_settings(candidates);
            for (usize group = 0; group < SETTINGS_GROUP_COUNT; ++group) {
                CAPTURE(group);
                CHECK(choice.winner[group] == 1);
                CHECK(choice.ambiguous[group]);
            }
        }
        SUBCASE("a candidate without a guid loses to any candidate that has one") {
            const std::array<scene_settings_candidate, 2> candidates = {
              candidate_of(first, core::uuid(), 0),
              candidate_of(second, core::uuid(~0ull, ~0ull), 5),
            };
            CHECK(choose_scene_settings(candidates).winner[SHADOWS_GROUP] == 1);
        }
        SUBCASE("guid-less candidates go by entity index") {
            const std::array<scene_settings_candidate, 3> candidates = {
              candidate_of(first, core::uuid(), 9),
              candidate_of(second, core::uuid(), 4),
              candidate_of(third, core::uuid(), 6),
            };
            const scene_settings_choice choice = choose_scene_settings(candidates);
            CHECK(choice.winner[POST_PROCESS_GROUP] == 1);
            CHECK(choice.ambiguous[POST_PROCESS_GROUP]);
        }
        SUBCASE("the same guid goes by entity index") {
            const std::array<scene_settings_candidate, 2> candidates = {
              candidate_of(first, core::uuid(0, 4), 8),
              candidate_of(second, core::uuid(0, 4), 2),
            };
            CHECK(choose_scene_settings(candidates).winner[SSAO_GROUP] == 1);
        }
    }

    TEST_CASE("choose_scene_settings - an empty span or null components choose nothing") {
        const scene_settings_choice empty = choose_scene_settings({});
        CHECK(empty.winner == scene_settings_choice().winner);
        CHECK(empty.ambiguous == std::array<bool, SETTINGS_GROUP_COUNT>{});
        const std::array<scene_settings_candidate, 2> nulls = {scene_settings_candidate{}, scene_settings_candidate{}};
        const scene_settings_choice choice = choose_scene_settings(nulls);
        for (usize group = 0; group < SETTINGS_GROUP_COUNT; ++group) {
            CAPTURE(group);
            CHECK(choice.winner[group] == NO_SETTINGS_CANDIDATE);
            CHECK_FALSE(choice.ambiguous[group]);
        }
        const resolved_render_settings resolved = resolve_scene_settings(render_settings(), nulls, choice);
        CHECK(resolved.settings == render_settings());
        CHECK(resolved.source == std::array<render_settings_source, SETTINGS_GROUP_COUNT>{});
    }

    TEST_CASE("choose_scene_settings - ambiguity is reported per group and cleared by a higher priority") {
        const render_settings_component tiedA = scene_component(1, true, true, false, 0.1f);
        const render_settings_component tiedB = scene_component(1, true, true, true, 0.2f);
        const render_settings_component higher = scene_component(2, false, true, false, 0.3f);
        const std::array<scene_settings_candidate, 3> candidates = {
          candidate_of(tiedA, core::uuid(0, 1), 0),
          candidate_of(tiedB, core::uuid(0, 2), 1),
          candidate_of(higher, core::uuid(0, 3), 2),
        };
        const scene_settings_choice choice = choose_scene_settings(candidates);
        CHECK(choice.winner[POST_PROCESS_GROUP] == 0);
        CHECK(choice.ambiguous[POST_PROCESS_GROUP]);
        CHECK(choice.winner[SHADOWS_GROUP] == 2);
        CHECK_FALSE(choice.ambiguous[SHADOWS_GROUP]);
        CHECK(choice.winner[SSAO_GROUP] == 1);
        CHECK_FALSE(choice.ambiguous[SSAO_GROUP]);
        // A lower priority after the top does not make the group ambiguous either.
        const std::array<scene_settings_candidate, 3> reordered = {
          candidate_of(higher, core::uuid(0, 3), 2),
          candidate_of(tiedA, core::uuid(0, 1), 0),
          candidate_of(tiedB, core::uuid(0, 2), 1),
        };
        const scene_settings_choice again = choose_scene_settings(reordered);
        CHECK(again.winner[SHADOWS_GROUP] == 0);
        CHECK_FALSE(again.ambiguous[SHADOWS_GROUP]);
        CHECK(again.ambiguous[POST_PROCESS_GROUP]);
    }

    TEST_CASE("resolve_scene_settings - each group with a winner takes its values and source, the rest the project's") {
        render_settings project = {};
        project.postProcess.tonemap = tonemap_operator::REINHARD_EXTENDED;
        project.shadows.cascadeCount = 2;
        project.ssao.intensity = 2.0f;
        for (u32 mask = 0; mask < 8; ++mask) {
            CAPTURE(mask);
            const render_settings_component component = scene_component(
              0,
              (mask & 1u) != 0,
              (mask & 2u) != 0,
              (mask & 4u) != 0,
              0.5f
            );
            const std::array<scene_settings_candidate, 1> candidates = {candidate_of(component, core::uuid(0, 1), 0)};
            const resolved_render_settings resolved = resolve_scene_settings(
              project,
              candidates,
              choose_scene_settings(candidates)
            );
            const std::array<bool, SETTINGS_GROUP_COUNT> overridden = {
              (mask & 1u) != 0,
              (mask & 2u) != 0,
              (mask & 4u) != 0,
            };
            CHECK(resolved.settings.postProcess == (overridden[0] ? component.postProcess : project.postProcess));
            CHECK(resolved.settings.shadows == (overridden[1] ? component.shadows : project.shadows));
            CHECK(resolved.settings.ssao == (overridden[2] ? component.ssao : project.ssao));
            for (usize group = 0; group < SETTINGS_GROUP_COUNT; ++group) {
                CAPTURE(group);
                const render_settings_source source = overridden[group] ? render_settings_source::SCENE
                                                                        : render_settings_source::PROJECT;
                CHECK(resolved.source[group] == source);
                CHECK(resolved.sceneWinner[group] == (overridden[group] ? 0u : NO_SETTINGS_CANDIDATE));
                CHECK_FALSE(resolved.ambiguous[group]);
            }
            CHECK_FALSE(resolved.postProcessDisabled);
        }
    }

    TEST_CASE("resolve_scene_settings - ambiguity is copied and an invalid winner asserts and keeps the project's") {
        const render_settings_component component = scene_component(0, true, true, true, 0.5f);
        const std::array<scene_settings_candidate, 1> candidates = {candidate_of(component, core::uuid(0, 1), 0)};
        scene_settings_choice choice = {};
        choice.winner[SHADOWS_GROUP] = 0;
        choice.ambiguous[SHADOWS_GROUP] = true;
        const resolved_render_settings ambiguous = resolve_scene_settings(render_settings(), candidates, choice);
        CHECK(ambiguous.ambiguous[SHADOWS_GROUP]);
        CHECK(ambiguous.settings.shadows == component.shadows);

        scene_settings_choice invalid = {};
        invalid.winner[POST_PROCESS_GROUP] = 4;
        resolved_render_settings resolved = {};
        CONJURE_CHECK_ASSERTS(resolved = resolve_scene_settings(render_settings(), candidates, invalid));
        CHECK(resolved.settings == render_settings());
        CHECK(resolved.source[POST_PROCESS_GROUP] == render_settings_source::PROJECT);
        CHECK(resolved.sceneWinner[POST_PROCESS_GROUP] == NO_SETTINGS_CANDIDATE);
    }

    TEST_CASE("resolve_camera_settings - each override group takes the camera's values and source CAMERA") {
        const render_settings_component sceneComponent = scene_component(0, true, true, true, 0.5f);
        const std::array<scene_settings_candidate, 1> candidates = {candidate_of(sceneComponent, core::uuid(0, 1), 0)};
        const resolved_render_settings scene = resolve_scene_settings(
          render_settings(),
          candidates,
          choose_scene_settings(candidates)
        );
        const camera_component camera = {};

        SUBCASE("no component") {
            const resolved_render_settings resolved = resolve_camera_settings(scene, camera, nullptr);
            CHECK(resolved.settings == scene.settings);
            CHECK(resolved.source == scene.source);
            CHECK_FALSE(resolved.postProcessDisabled);
        }
        SUBCASE("every flag combination") {
            for (u32 mask = 0; mask < 4; ++mask) {
                CAPTURE(mask);
                post_process_component overrides = {};
                overrides.overridePostProcess = (mask & 1u) != 0;
                overrides.overrideSsao = (mask & 2u) != 0;
                overrides.postProcess.tonemap = tonemap_operator::NONE;
                overrides.ssao.thickness = 0.9f;
                const resolved_render_settings resolved = resolve_camera_settings(scene, camera, &overrides);
                const bool ownPostProcess = overrides.overridePostProcess;
                const bool ownSsao = overrides.overrideSsao;
                const post_process_settings &expectedPostProcess = ownPostProcess ? overrides.postProcess
                                                                                  : scene.settings.postProcess;
                CHECK(resolved.settings.postProcess == expectedPostProcess);
                CHECK(resolved.settings.ssao == (ownSsao ? overrides.ssao : scene.settings.ssao));
                CHECK(resolved.settings.shadows == scene.settings.shadows);
                const render_settings_source postProcessSource = ownPostProcess ? render_settings_source::CAMERA
                                                                                : render_settings_source::SCENE;
                const render_settings_source ssaoSource = ownSsao ? render_settings_source::CAMERA
                                                                  : render_settings_source::SCENE;
                CHECK(resolved.source[POST_PROCESS_GROUP] == postProcessSource);
                CHECK(resolved.source[SHADOWS_GROUP] == render_settings_source::SCENE);
                CHECK(resolved.source[SSAO_GROUP] == ssaoSource);
                // The scene layer's winners stay reported, also where the camera replaced the group.
                CHECK(resolved.sceneWinner == scene.sceneWinner);
                CHECK_FALSE(resolved.postProcessDisabled);
            }
        }
        SUBCASE("over the project layer") {
            post_process_component overrides = {};
            overrides.overrideSsao = true;
            const resolved_render_settings resolved = resolve_camera_settings(
              resolved_render_settings(),
              camera,
              &overrides
            );
            CHECK(resolved.source[POST_PROCESS_GROUP] == render_settings_source::PROJECT);
            CHECK(resolved.source[SSAO_GROUP] == render_settings_source::CAMERA);
            CHECK(resolved.sceneWinner[SSAO_GROUP] == NO_SETTINGS_CANDIDATE);
        }
    }

    TEST_CASE("resolve_camera_settings - post-processing off keeps the configured values and says it is disabled") {
        camera_component camera = {};
        camera.postProcess = false;
        post_process_component overrides = {};
        overrides.overridePostProcess = true;
        overrides.postProcess.exposure = exposure_mode::MANUAL;
        overrides.postProcess.manualEv100 = 9.0f;
        const resolved_render_settings resolved = resolve_camera_settings(
          resolved_render_settings(),
          camera,
          &overrides
        );
        CHECK(resolved.postProcessDisabled);
        CHECK(resolved.settings.postProcess == overrides.postProcess);
        CHECK(resolved.source[POST_PROCESS_GROUP] == render_settings_source::CAMERA);
        const resolved_render_settings plain = resolve_camera_settings(resolved_render_settings(), camera, nullptr);
        CHECK(plain.postProcessDisabled);
        CHECK(plain.settings == render_settings());
    }

    TEST_CASE("make_camera_view - copies only the enabled override groups and names them in settingsOverride") {
        camera_component camera = {};
        SUBCASE("no component") {
            const render_view view = make_camera_view(camera, math::mat4(1.0f), {}, nullptr);
            CHECK(view.settingsOverride == view_settings_override_flags::NONE);
            CHECK(view.postProcess == post_process_settings());
            CHECK(view.ssao == ssao_settings());
            CHECK(view.debugMode == view_debug_mode::NONE);
        }
        SUBCASE("every flag combination, with post-processing on and off") {
            for (u32 mask = 0; mask < 8; ++mask) {
                CAPTURE(mask);
                camera.postProcess = (mask & 4u) == 0;
                post_process_component overrides = {};
                overrides.overridePostProcess = (mask & 1u) != 0;
                overrides.overrideSsao = (mask & 2u) != 0;
                overrides.postProcess.bloomEnabled = false;
                overrides.ssao.radius = 4.0f;
                const render_view view = make_camera_view(camera, math::mat4(1.0f), {}, &overrides);
                const bool ownPostProcess = overrides.overridePostProcess;
                const bool ownSsao = overrides.overrideSsao;
                CHECK(has_flag(view.settingsOverride, view_settings_override_flags::POST_PROCESS) == ownPostProcess);
                CHECK(has_flag(view.settingsOverride, view_settings_override_flags::SSAO) == ownSsao);
                CHECK(view.postProcess == (ownPostProcess ? overrides.postProcess : post_process_settings()));
                CHECK(view.ssao == (ownSsao ? overrides.ssao : ssao_settings()));
                CHECK(view.flags == (camera.postProcess ? view_flags::NONE : view_flags::NO_POST_PROCESS));
            }
        }
    }

    TEST_CASE("resolve_camera_settings - agrees with effective_view_settings of make_camera_view for random settings") {
        core::random::engine engine(testing::environment().seed);
        for (u32 iteration = 0; iteration < 200; ++iteration) {
            CAPTURE(iteration);
            render_settings_component sceneComponent = {};
            sceneComponent.overridePostProcess = core::random::chance(engine, 0.5);
            sceneComponent.overrideSsao = core::random::chance(engine, 0.5);
            sceneComponent.postProcess = random_post_process(engine);
            sceneComponent.ssao = random_ssao(engine);
            render_settings project = {};
            project.postProcess = random_post_process(engine);
            project.ssao = random_ssao(engine);
            camera_component camera = {};
            camera.postProcess = core::random::chance(engine, 0.5);
            post_process_component overrides = {};
            overrides.overridePostProcess = core::random::chance(engine, 0.5);
            overrides.overrideSsao = core::random::chance(engine, 0.5);
            overrides.postProcess = random_post_process(engine);
            overrides.ssao = random_ssao(engine);
            const bool withComponent = core::random::chance(engine, 0.75);

            const std::array<scene_settings_candidate, 1> candidates = {
              candidate_of(sceneComponent, core::uuid(0, 1), 0),
            };
            const resolved_render_settings scene = resolve_scene_settings(
              project,
              candidates,
              choose_scene_settings(candidates)
            );
            const post_process_component *cameraOverrides = withComponent ? &overrides : nullptr;
            const resolved_render_settings resolved = resolve_camera_settings(scene, camera, cameraOverrides);
            const render_view view = make_camera_view(camera, math::mat4(1.0f), {}, cameraOverrides);
            const view_render_settings effective = effective_view_settings(
              scene.settings.postProcess,
              scene.settings.ssao,
              view
            );
            CHECK(resolved.settings.postProcess == effective.postProcess);
            CHECK(resolved.settings.ssao == effective.ssao);
            CHECK(resolved.postProcessDisabled == has_flag(view.flags, view_flags::NO_POST_PROCESS));
        }
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
