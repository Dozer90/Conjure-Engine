#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "renderer_test_support.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/sprite_batch.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/core/image.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/native_window.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/swapchain.h"
#include "conjure/rhi/texture_format.h"

#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// The failure paths (docs/design/02_renderer.md 6, 10 "test_renderer_failures"): creation refusals
// name their cause, bad arguments are rejected before any GPU work, device loss stops rendering
// for good, and a bad scene never aborts a frame.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    /// <summary>
    /// A validated null device, its shader library, a null swapchain and a renderer presenting to it,
    /// destroyed in reverse order; the swapchain refusals are injected with rhi::null_fault.
    /// </summary>
    struct presenting_renderer {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<rhi::shader_library> shaders = nullptr;
        std::unique_ptr<rhi::swapchain> swapchain = nullptr;
        std::unique_ptr<renderer_type> instance = nullptr;
    };

    /// <summary>
    /// Creates the four objects of a presenting_renderer or fails the case.
    /// </summary>
    presenting_renderer make_presenting_renderer() {
        presenting_renderer created = {};
        created.device = make_device();
        created.shaders = open_shaders(*created.device);
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = 64;
        swapchainDesc.height = 32;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain = rhi::swapchain::create(
          *created.device,
          rhi::native_window(),
          swapchainDesc
        );
        CONJURE_REQUIRE_OK(swapchain);
        created.swapchain = std::move(swapchain).value();
        renderer_desc desc = headless_desc(*created.device, *created.shaders);
        desc.swapchain = created.swapchain.get();
        core::result<std::unique_ptr<renderer_type>> instance = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(instance);
        created.instance = std::move(instance).value();
        return created;
    }
}

TEST_SUITE("renderer/failures") {

    TEST_CASE("failures - create without a device or a shader library fails with INVALID_ARGUMENT") {
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        renderer_desc noDevice = headless_desc(*device, *shaders);
        noDevice.device = nullptr;
        CONJURE_CHECK_ERROR(renderer_type::create(noDevice), core::error_code::INVALID_ARGUMENT);
        renderer_desc noShaders = headless_desc(*device, *shaders);
        noShaders.shaders = nullptr;
        CONJURE_CHECK_ERROR(renderer_type::create(noShaders), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("failures - a missing shader permutation fails creation with SHADER_NOT_FOUND naming the key") {
        temp_directory directory("empty_shaders");
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> empty = open_written_shaders(*device, directory);
        const core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(
          headless_desc(*device, *empty)
        );
        CONJURE_CHECK_ERROR(created, renderer_error::SHADER_NOT_FOUND);
        const std::string message = created.get_error().message();
        CHECK(message.find("shader '") != std::string::npos);
        CHECK(message.find("entry '") != std::string::npos);
        CHECK(message.find("forward_pbr") != std::string::npos);
    }

    TEST_CASE(
      "failures - create_render_target rejects a zero size and a non-colour format before touching the device"
    ) {
        headless_renderer fixture = make_headless_renderer();
        fixture.device->reset_null_stats();
        render_target_desc desc = {};
        desc.size = math::uvec2(0, 16);
        CONJURE_CHECK_ERROR(fixture->create_render_target(desc), core::error_code::INVALID_ARGUMENT);
        desc.size = math::uvec2(16, 0);
        CONJURE_CHECK_ERROR(fixture->create_render_target(desc), core::error_code::INVALID_ARGUMENT);
        desc.size = math::uvec2(16, 16);
        desc.colourFormat = rhi::texture_format::D32_FLOAT;
        CONJURE_CHECK_ERROR(fixture->create_render_target(desc), core::error_code::INVALID_ARGUMENT);
        desc.colourFormat = rhi::texture_format::R8_UNORM;
        CONJURE_CHECK_ERROR(fixture->create_render_target(desc), core::error_code::INVALID_ARGUMENT);
        desc.colourFormat = rhi::texture_format::BC7_SRGB;
        const core::result<render_target_id> refused = fixture->create_render_target(desc);
        CONJURE_CHECK_ERROR(refused, core::error_code::INVALID_ARGUMENT);
        CHECK(refused.get_error().message().find("BC7_SRGB") != std::string::npos);
        CHECK(fixture.device->null_stats()->texturesCreated == 0);
        CONJURE_CHECK_ERROR(
          fixture->resize_render_target(render_target_id(), math::uvec2(1, 1)),
          core::error_code::NOT_FOUND
        );
        CONJURE_CHECK_ERROR(fixture->read_back(render_target_id()), core::error_code::NOT_FOUND);
    }

    TEST_CASE("failures - a light's tile request is clamped and never drops the frame") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(32, 32);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(1), mesh_asset::unit_cube());
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value()));
        scene.meshes.push_back(
          instance_at(cube, gpu_material_id(), math::vec3(0.0f), mesh_asset::unit_cube().bounds())
        );
        // Oversized, zero and odd tile requests all map to the allowed sizes.
        for (const u16 requested : {u16{0}, u16{1}, u16{333}, u16{65535}, u16{4096}}) {
            light value = {};
            value.type = light_type::SPOT;
            value.position = math::vec3(static_cast<f32>(requested % 7), 2.0f, 0.0f);
            value.castsShadows = true;
            value.shadowResolution = requested;
            scene.lights.push_back(value);
        }
        // Degenerate lights: zero range, zero direction, NaN-free but absurd cones.
        light broken = {};
        broken.type = light_type::SPOT;
        broken.direction = math::vec3(0.0f);
        broken.range = 0.0f;
        broken.innerConeRadians = 5.0f;
        broken.outerConeRadians = 0.0f;
        broken.castsShadows = true;
        scene.lights.push_back(broken);
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f);
        sun.castsShadows = true;
        scene.lights.push_back(sun);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 1);
        CHECK(fixture->stats().lightsRendered == 7);
        CHECK(fixture->stats().shadowTilesAllocated == 6);
        CHECK(fixture->stats().shadowTilesDropped == 0);
        CHECK(fixture->stats().shadowCascadesRendered == MAX_SHADOW_CASCADES);
        CHECK(logs.count(core::log_level::ERR) == 0);

        // More casters than the atlas holds: the least important are halved then dropped.
        scene.lights.clear();
        for (u32 index = 0; index < 40; ++index) {
            light value = {};
            value.type = light_type::POINT;
            value.position = math::vec3(static_cast<f32>(index) * 3.0f, 1.0f, 0.0f);
            value.castsShadows = true;
            value.shadowResolution = 1024;
            scene.lights.push_back(value);
        }
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().shadowTilesAllocated <= 16);   // 1024^2 atlas: sixteen 256 tiles at most
        CHECK(fixture->stats().shadowTilesDropped > 0);
        CHECK(fixture->stats().shadowTilesAllocated + fixture->stats().shadowTilesDropped * 6 >= 40 * 6 - 6 * 16);
        CHECK(fixture->stats().viewsRendered == 1);
    }

    TEST_CASE("failures - a bad scene is clamped: invalid bounds, a viewport outside its target and a huge mesh") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(1), mesh_asset::unit_cube());
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value()));
        scene.views[0].viewport = math::urect{100, 100, 50, 50};   // entirely outside: clamped to a pixel
        mesh_instance invalidBounds = instance_at(
          cube,
          gpu_material_id(),
          math::vec3(0.0f),
          mesh_asset::unit_cube().bounds()
        );
        invalidBounds.worldBounds = math::aabb();
        scene.meshes.push_back(invalidBounds);
        mesh_instance huge = instance_at(
          cube,
          gpu_material_id(),
          math::vec3(0.0f),
          mesh_asset::unit_cube().bounds(),
          1.0e6f
        );
        scene.meshes.push_back(huge);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 1);
        CHECK(fixture->stats().instancesCulled == 1);   // the invalid box is never visible
        CHECK(fixture->stats().instancesDrawn >= 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
        // An environment rotated by a huge angle still renders.
        scene.environment.rotationRadians = 1.0e6f;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep W3-RS-L1B settings out of range are clamped once per frame with one WARN") {
        // Only cascadeCount was clamped, inside the cascade builder, once per view; every other value of the
        // scene's settings groups reached the view constants as it came, and nothing was logged. The scene's
        // groups are now sanitised once per frame and a view's override groups once per view, each group
        // logged once when it starts to be out of range and counted in render_stats::settingsClamped.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(1), mesh_asset::unit_cube());
        render_scene scene = {};
        scene.meshes.push_back(
          instance_at(cube, gpu_material_id(), math::vec3(0.0f), mesh_asset::unit_cube().bounds())
        );
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.castsShadows = true;
        scene.lights.push_back(sun);
        const render_view valid = look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value());
        for (u32 index = 0; index < 3; ++index) {
            scene.views.push_back(valid);
        }

        // Each variant breaks one scene group and one override group of view 2; the texts are the WARNs.
        std::function<void(render_scene &)> breakScene = {};
        std::function<void(render_view &)> breakOverride = {};
        std::string_view sceneWarning = {};
        std::string_view overrideWarning = {};
        SUBCASE("post-process group and an SSAO override") {
            breakScene = [](render_scene &value) {
                value.postProcess.exposure = exposure_mode::MANUAL;
                value.postProcess.manualEv100 = 1000.0f;
            };
            breakOverride = [](render_view &value) {
                value.settingsOverride = view_settings_override_flags::SSAO;
                value.ssao.radius = std::numeric_limits<f32>::quiet_NaN();
            };
            sceneWarning = "scene post_process settings: manual_ev100 is out of range";
            overrideWarning = "view 2: ssao override: radius is out of range";
        }
        SUBCASE("shadow group and a post-process override") {
            breakScene = [](render_scene &value) {
                value.shadows.distance = -5.0f;
                value.shadows.cascadeCount = 99;
                value.shadows.splitLambda = 7.0f;
                value.shadows.casterPadding = -10.0f;
            };
            breakOverride = [](render_view &value) {
                value.settingsOverride = view_settings_override_flags::POST_PROCESS;
                value.postProcess.bloomIntensity = -1.0f;
            };
            sceneWarning = "scene shadows settings: distance is out of range";
            overrideWarning = "view 2: post_process override: bloom_intensity is out of range";
        }
        SUBCASE("SSAO group and an inverted exposure range") {
            breakScene = [](render_scene &value) {
                value.ssao.radius = -1.0f;
            };
            breakOverride = [](render_view &value) {
                value.settingsOverride = view_settings_override_flags::POST_PROCESS;
                value.postProcess.minEv100 = 10.0f;
                value.postProcess.maxEv100 = 0.0f;
            };
            sceneWarning = "scene ssao settings: radius is out of range";
            overrideWarning = "view 2: post_process override: max_ev100 is out of range";
        }
        SUBCASE("enumerators outside their enums") {
            breakScene = [](render_scene &value) {
                value.postProcess.tonemap = static_cast<tonemap_operator>(200);
            };
            breakOverride = [](render_view &value) {
                value.settingsOverride = view_settings_override_flags::POST_PROCESS;
                value.postProcess.agxLook = static_cast<agx_look>(9);
            };
            sceneWarning = "scene post_process settings: tonemap is out of range";
            overrideWarning = "view 2: post_process override: agx_look is out of range";
        }
        REQUIRE(breakScene);
        const auto settingsWarnings = [&logs]() {
            return warning_count(logs, "out of range and was clamped");
        };

        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().settingsClamped == 0);
        CHECK(settingsWarnings() == 0);

        // A bad scene group: one WARN and one clamp however many views render it. A view's own values without
        // its override flag are not read.
        breakScene(scene);
        scene.views[1].postProcess.bloomIntensity = -1.0f;
        scene.views[1].ssao.radius = -1.0f;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 3);
        CHECK(fixture->stats().settingsClamped == 1);
        CHECK(settingsWarnings() == 1);
        CHECK(logs.contains(core::log_level::WARN, sceneWarning));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().settingsClamped == 1);
        CHECK(settingsWarnings() == 1);

        // A bad override group of view 2: one more WARN, for that view.
        breakOverride(scene.views[2]);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 3);
        CHECK(fixture->stats().settingsClamped == 2);
        CHECK(settingsWarnings() == 2);
        CHECK(logs.contains(core::log_level::WARN, overrideWarning));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().settingsClamped == 2);
        CHECK(settingsWarnings() == 2);

        // A clean frame re-arms both warnings.
        scene.postProcess = post_process_settings();
        scene.shadows = shadow_settings();
        scene.ssao = ssao_settings();
        scene.views[2].settingsOverride = view_settings_override_flags::NONE;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().settingsClamped == 0);
        CHECK(settingsWarnings() == 2);
        breakScene(scene);
        breakOverride(scene.views[2]);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 3);
        CHECK(fixture->stats().settingsClamped == 2);
        CHECK(settingsWarnings() == 4);
        CHECK(fixture->stats().instancesDrawn >= 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep W3-RS-L1B the cascades are built from the clamped scene shadow group") {
        // The WARN and settingsClamped come from the frame's sanitise whatever the frame reads afterwards, so
        // this case pins the reads: a frame whose shadow distance is out of range draws exactly the shadow
        // casters of the frame with the value it is clamped to. A caster 30 km down the view is inside the
        // first cascade only when the cascades reach 1000 km; at 10 km (MAX_SHADOW_DISTANCE) none reaches it.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const mesh_asset cubeMesh = mesh_asset::unit_cube();
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(1), cubeMesh);
        render_scene scene = {};
        const math::vec3 eye(0.0f, 1.0f, 0.0f);
        scene.views.push_back(look_at_view(eye, math::vec3(0.0f, 1.0f, -1.0f), target.value()));
        scene.meshes.push_back(instance_at(cube, gpu_material_id(), math::vec3(0.0f, 0.5f, -5.0f), cubeMesh.bounds()));
        scene.meshes.push_back(
          instance_at(cube, gpu_material_id(), math::vec3(0.0f, 0.5f, -30000.0f), cubeMesh.bounds())
        );
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, -1.0f, 0.0f);
        sun.castsShadows = true;
        scene.lights.push_back(sun);

        f32 inRange = 0.0f;
        f32 outOfRange = 0.0f;
        SUBCASE("a distance above the range renders as MAX_SHADOW_DISTANCE") {
            inRange = MAX_SHADOW_DISTANCE;
            outOfRange = 1.0e6f;
        }
        SUBCASE("a NaN distance renders as the default distance") {
            inRange = shadow_settings().distance;
            outOfRange = std::numeric_limits<f32>::quiet_NaN();
        }
        REQUIRE(inRange > 0.0f);

        scene.shadows.distance = inRange;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const u32 expectedDrawn = fixture->stats().instancesDrawn;
        const u32 expectedCascades = fixture->stats().shadowCascadesRendered;
        CHECK(fixture->stats().settingsClamped == 0);
        CHECK(expectedCascades == MAX_SHADOW_CASCADES);

        scene.shadows.distance = outOfRange;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().settingsClamped == 1);
        CHECK(fixture->stats().instancesDrawn == expectedDrawn);
        CHECK(fixture->stats().shadowCascadesRendered == expectedCascades);
        CHECK(warning_count(logs, "scene shadows settings: distance is out of range") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F052 an invalid camera is clamped with one WARN per view and the frame renders"
    ) {
        // clip_from_view asserts on these cameras; scene data is clamped instead (a bad scene never
        // aborts a frame) and each view WARNs once until its camera is valid again.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(1), mesh_asset::unit_cube());
        render_scene scene = {};
        scene.meshes.push_back(
          instance_at(cube, gpu_material_id(), math::vec3(0.0f), mesh_asset::unit_cube().bounds())
        );
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.castsShadows = true;
        scene.lights.push_back(sun);
        const render_view valid = look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value());
        for (u32 index = 0; index < 6; ++index) {
            scene.views.push_back(valid);
        }
        scene.views[0].cameraData.nearPlane = 10.0f;
        scene.views[0].cameraData.farPlane = 5.0f;
        scene.views[1].cameraData.nearPlane = 1.0e20f;
        scene.views[2].cameraData.nearPlane = std::numeric_limits<f32>::quiet_NaN();
        scene.views[3].cameraData.verticalFovRadians = 0.0f;
        scene.views[4].cameraData.projection = projection_type::ORTHOGRAPHIC;
        scene.views[4].cameraData.orthographicHeight = 0.0f;
        // A perspective camera ignores its orthographic height: valid, no WARN.
        scene.views[5].cameraData.orthographicHeight = 0.0f;

        const auto cameraWarnings = [&logs]() {
            return warning_count(logs, "invalid camera");
        };
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 6);
        CHECK(fixture->stats().instancesDrawn >= 1);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 6);
        CHECK(cameraWarnings() == 5);
        for (u32 index = 0; index < 5; ++index) {
            CAPTURE(index);
            CHECK(logs.contains(core::log_level::WARN, "view " + std::to_string(index) + ": invalid camera"));
        }
        CHECK_FALSE(logs.contains(core::log_level::WARN, "view 5: invalid camera"));

        // A valid camera re-arms the warning of its view.
        scene.views[0] = valid;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(cameraWarnings() == 5);
        scene.views[0].cameraData.farPlane = std::numeric_limits<f32>::infinity();
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(cameraWarnings() == 6);
        CHECK(fixture->stats().viewsRendered == 6);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F612 an overflow of views or lights warns when it starts, not when it changes") {
        // The warnings used to follow the count: an overflow whose size moved from frame to frame was
        // logged every frame. They follow the condition now; render_stats has every frame's count.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        const core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const render_view view = look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value());
        light local = {};
        local.type = light_type::POINT;
        local.position = math::vec3(0.0f, 2.0f, 0.0f);
        local.range = 4.0f;

        struct frame_expectation {
            u32 extraViews = 0;         // views beyond MAX_VIEWS
            u32 extraLights = 0;        // lights beyond MAX_LIGHTS_PER_VIEW
            u32 viewWarnings = 0;       // "exceed MAX_VIEWS" warnings logged once the frame is rendered
            u32 lightWarnings = 0;      // "exceed MAX_LIGHTS_PER_VIEW" warnings by then
        };
        const std::array<frame_expectation, 9> frames = {{
          {2, 10, 1, 1},    // both overflows start
          {5, 3, 1, 1},     // both last with other counts
          {1, 40, 1, 1},
          {5, 3, 1, 1},     // a count seen before is still the same overflow
          {0, 3, 1, 1},     // the views are within the budget again; the end is not logged
          {2, 3, 2, 1},     // the view overflow starts anew while the light overflow never ended
          {2, 0, 2, 1},     // the lights are within the budget again
          {2, 7, 2, 2},     // and overflow anew while the view overflow lasts
          {0, 0, 2, 2},
        }};
        for (usize index = 0; index < frames.size(); ++index) {
            CAPTURE(index);
            const frame_expectation &expected = frames[index];
            render_scene scene = {};
            scene.views.assign(MAX_VIEWS + expected.extraViews, view);
            scene.lights.assign(MAX_LIGHTS_PER_VIEW + expected.extraLights, local);
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsRendered == MAX_VIEWS);
            CHECK(fixture->stats().viewsSkipped == expected.extraViews);
            CHECK(fixture->stats().lightsDropped == expected.extraLights * MAX_VIEWS);
            CHECK(warning_count(logs, "exceed MAX_VIEWS") == expected.viewWarnings);
            CHECK(warning_count(logs, "exceed MAX_LIGHTS_PER_VIEW") == expected.lightWarnings);
        }

        // The light budget is looked at when a view is prepared. A frame without a view does not see
        // that the overflow ended, so the frame after it inherits the overflow of the frame before.
        render_scene scene = {};
        scene.views.assign(1, view);
        scene.lights.assign(MAX_LIGHTS_PER_VIEW + 4, local);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(warning_count(logs, "exceed MAX_LIGHTS_PER_VIEW") == 3);
        CONJURE_REQUIRE_OK(fixture->render(render_scene()));
        CHECK(fixture->stats().lightsDropped == 0);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().lightsDropped == 4);
        CHECK(warning_count(logs, "exceed MAX_LIGHTS_PER_VIEW") == 3);
        CHECK(warning_count(logs, "exceed MAX_VIEWS") == 2);
    }

    TEST_CASE("regression - code sweep F612 a view with an unknown render target warns once until its index has one") {
        // The warning used to be logged for every such view in every frame.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        const core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        render_target_id unknown = {};
        unknown.index = 99;
        unknown.generation = 7;
        render_target_id otherUnknown = {};
        otherUnknown.index = 123;
        otherUnknown.generation = 2;
        const render_view known = look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value());
        render_scene scene = {};
        scene.views.assign(3, known);
        scene.views[1].target = unknown;
        scene.views[2].target = unknown;
        const char *const warningText = "targets an unknown render target";

        // Both views are skipped in every frame and logged in the first.
        for (u32 frame = 0; frame < 3; ++frame) {
            CAPTURE(frame);
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsRendered == 1);
            CHECK(fixture->stats().viewsSkipped == 2);
            CHECK(warning_count(logs, warningText) == 2);
        }
        CHECK(logs.contains(core::log_level::WARN, "view 1 targets an unknown render target (index 99, generation 7)"));
        CHECK(logs.contains(core::log_level::WARN, "view 2 targets an unknown render target (index 99, generation 7)"));

        SUBCASE("another unknown target at the same index is the same problem") {
            scene.views[1].target = otherUnknown;
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsSkipped == 2);
            CHECK(warning_count(logs, warningText) == 2);
        }

        SUBCASE("a known target re-arms the warning of its view index only") {
            scene.views[1].target = target.value();
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsRendered == 2);
            CHECK(fixture->stats().viewsSkipped == 1);
            CHECK(warning_count(logs, warningText) == 2);
            scene.views[1].target = otherUnknown;
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsSkipped == 2);
            CHECK(warning_count(logs, warningText) == 3);
            CHECK(logs.contains(core::log_level::WARN, "view 1 targets an unknown render target (index 123, "));
        }

        SUBCASE("a view of the swapchain at the index re-arms it too") {
            // Headless, so that view is skipped for its own reason, which has its own warning.
            scene.views[2].target = render_target_id();
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsSkipped == 2);
            CHECK(warning_count(logs, warningText) == 2);
            CHECK(warning_count(logs, "headless") == 1);
            scene.views[2].target = unknown;
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(warning_count(logs, warningText) == 3);
        }

        SUBCASE("destroying the target of a view starts the problem for that view") {
            fixture->destroy_render_target(target.value());
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsRendered == 0);
            CHECK(fixture->stats().viewsSkipped == 3);
            CHECK(warning_count(logs, warningText) == 3);
            CHECK(logs.contains(core::log_level::WARN, "view 0 targets an unknown render target"));
        }

        SUBCASE("an index the scene does not have for a while keeps its state") {
            // The state belongs to the view index: frames without a view there neither end nor
            // restart the problem, so the views that come back to it with the same fault stay quiet.
            render_scene oneView = {};
            oneView.views.assign(1, known);
            CONJURE_REQUIRE_OK(fixture->render(oneView));
            CHECK(fixture->stats().viewsSkipped == 0);
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().viewsSkipped == 2);
            CHECK(warning_count(logs, warningText) == 2);
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F612 render_stats counts the sprites every frame drops, the WARN comes once") {
        // spritesDropped is what dropped_count() of the renderer's own batcher says, which nothing
        // outside the renderer could read before; the warning follows the batcher's rule.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        const core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const gpu_texture_id texture = fixture->upload_texture(
          test_asset_id(1),
          texture_asset::checker(core::rgba8{}, core::rgba8{255, 255, 255, 255}, 4, 2)
        );
        sprite world = {};
        world.texture = texture;
        sprite screen = world;
        screen.space = sprite_space::SCREEN;
        const render_view view = look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value());
        const char *const warningText = "exceed MAX_SPRITES_PER_FRAME";

        struct frame_expectation {
            u32 views = 0;
            u32 sprites = 0;            // three of them SCREEN sprites, the rest WORLD
            u32 dropped = 0;
            u32 warnings = 0;           // overflow warnings logged once the frame is rendered
        };
        const std::array<frame_expectation, 6> frames = {{
          {2, MAX_SPRITES_PER_FRAME + 4, 4, 1},     // counted once for the frame, not once per view
          {1, MAX_SPRITES_PER_FRAME + 7, 7, 1},     // the overflow lasts with another count
          {0, MAX_SPRITES_PER_FRAME + 1, 1, 1},     // the batch is built before the views: counted without one
          {1, MAX_SPRITES_PER_FRAME, 0, 1},         // exactly the cap ends it
          {1, MAX_SPRITES_PER_FRAME + 2, 2, 2},     // and the next overflow is logged again
          {1, 5, 0, 2},
        }};
        for (usize index = 0; index < frames.size(); ++index) {
            CAPTURE(index);
            const frame_expectation &expected = frames[index];
            render_scene scene = {};
            scene.views.assign(expected.views, view);
            scene.sprites.assign(expected.sprites, world);
            for (usize slot = 0; slot < 3; ++slot) {
                scene.sprites[slot] = screen;
            }
            CONJURE_REQUIRE_OK(fixture->render(scene));
            const render_stats &stats = fixture->stats();
            CHECK(stats.viewsRendered == expected.views);
            CHECK(stats.spritesDropped == expected.dropped);
            CHECK(stats.spritesDrawn == (expected.sprites - expected.dropped) * expected.views);
            CHECK(warning_count(logs, warningText) == expected.warnings);
        }
        CHECK(logs.contains(core::log_level::WARN, "4 WORLD and 0 SCREEN sprites"));
        CHECK(logs.contains(core::log_level::WARN, "2 WORLD and 0 SCREEN sprites"));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F612 a refused acquire is logged once until one succeeds") {
        // Every frame a swapchain refused to acquire used to log a WARN. The null swapchain refuses one
        // acquire per injected fault, as an out-of-date one does every frame until it is resized.
        log_capture logs = {};
        const presenting_renderer fixture = make_presenting_renderer();
        renderer_type &instance = *fixture.instance;
        render_scene scene = {};
        scene.views.assign(2, look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), render_target_id()));
        const char *const warningText = " skipped: ";

        for (u32 round = 1; round <= 2; ++round) {
            CAPTURE(round);
            // Every refused frame is skipped and counts all its views; the first of the round is logged.
            for (u32 frame = 0; frame < 3; ++frame) {
                CAPTURE(frame);
                fixture.device->inject_null_fault(rhi::null_fault::ACQUIRE_OUT_OF_DATE);
                CONJURE_REQUIRE_OK(instance.render(scene));
                CHECK(instance.stats().viewsRendered == 0);
                CHECK(instance.stats().viewsSkipped == 2);
                CHECK(warning_count(logs, warningText) == round);
            }
            CHECK_FALSE(fixture.device->is_lost());

            // The acquire that succeeds re-arms the warning for the next round.
            CONJURE_REQUIRE_OK(instance.render(scene));
            CHECK(instance.stats().viewsRendered == 2);
            CHECK(instance.stats().viewsSkipped == 0);
            CHECK(warning_count(logs, warningText) == round);
        }
        CHECK(logs.contains(core::log_level::WARN, "null acquire refused as out of date"));
        CHECK(logs.contains(core::log_level::WARN, "logged once until an acquire succeeds"));
        // A skipped frame records nothing, so only the two frames that acquired presented.
        const rhi::null_device_stats stats = fixture.device->null_stats().value();
        CHECK(stats.acquires == 8);
        CHECK(stats.presents == 2);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F612 a refused present is logged once until one succeeds") {
        // Every frame whose present was refused without device loss used to log a WARN. The frame is
        // recorded and executed either way, and render() succeeds.
        log_capture logs = {};
        const presenting_renderer fixture = make_presenting_renderer();
        renderer_type &instance = *fixture.instance;
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), render_target_id()));
        const char *const warningText = "present failed";

        for (u32 round = 1; round <= 2; ++round) {
            CAPTURE(round);
            for (u32 frame = 0; frame < 3; ++frame) {
                CAPTURE(frame);
                fixture.device->inject_null_fault(rhi::null_fault::PRESENT_FAILED);
                CONJURE_REQUIRE_OK(instance.render(scene));
                CHECK(instance.stats().viewsRendered == 1);
                CHECK(warning_count(logs, warningText) == round);
            }
            CHECK_FALSE(fixture.device->is_lost());
            CHECK_FALSE(instance.is_lost());
            // No refused present advanced the image.
            CHECK(fixture.swapchain->current_index() == (round - 1) % fixture.swapchain->buffer_count());

            // The present that succeeds re-arms the warning for the next round.
            CONJURE_REQUIRE_OK(instance.render(scene));
            CHECK(instance.stats().viewsRendered == 1);
            CHECK(warning_count(logs, warningText) == round);
        }
        CHECK(logs.contains(core::log_level::WARN, "null present refused"));
        CHECK(logs.contains(core::log_level::WARN, "logged once until a present succeeds"));
        CHECK_FALSE(logs.contains(core::log_level::WARN, " skipped: "));
        CHECK(fixture.device->null_stats().value().presents == 8);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("failures - device loss makes every later render fail with DEVICE_LOST and drops the mirrors") {
        log_capture logs = {};
        logs.allow_errors();
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(1), mesh_asset::unit_cube());
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value()));
        scene.meshes.push_back(
          instance_at(cube, gpu_material_id(), math::vec3(0.0f), mesh_asset::unit_cube().bounds())
        );
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK_FALSE(fixture->is_lost());

        fixture->on_device_lost();
        CHECK(fixture->is_lost());
        CHECK(logs.contains(core::log_level::ERR, "device lost"));
        CONJURE_CHECK_ERROR(fixture->render(scene), rhi::rhi_error::DEVICE_LOST);
        CONJURE_CHECK_ERROR(fixture->render_frame(), rhi::rhi_error::DEVICE_LOST);
        CONJURE_CHECK_ERROR(fixture->read_back(target.value()), rhi::rhi_error::DEVICE_LOST);
        CONJURE_CHECK_ERROR(fixture->create_render_target(desc), rhi::rhi_error::DEVICE_LOST);
        CHECK(fixture->frame_count() == 1);
        // The mirrors are gone: the id is stale now, and uploads after the loss are still accepted
        // (they would flush on a device that will never render again).
        CHECK_FALSE(fixture->find_mesh(test_asset_id(1)).is_valid());
        const gpu_mesh_id late = fixture->upload_mesh(test_asset_id(2), mesh_asset::quad());
        CHECK(late.is_valid());
        fixture->destroy_render_target(target.value());
    }

    TEST_CASE("failures - read_back_depth refuses unknown targets, targets without depth and a lost device") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value()));

        SUBCASE("an unknown or destroyed target is NOT_FOUND") {
            CONJURE_CHECK_ERROR(fixture->read_back_depth(render_target_id()), core::error_code::NOT_FOUND);
            fixture->destroy_render_target(target.value());
            CONJURE_CHECK_ERROR(fixture->read_back_depth(target.value()), core::error_code::NOT_FOUND);
        }
        SUBCASE("a target created without depth is INVALID_ARGUMENT, before and after a frame") {
            desc.withDepth = false;
            core::result<render_target_id> colourOnly = fixture->create_render_target(desc);
            CONJURE_REQUIRE_OK(colourOnly);
            CONJURE_CHECK_ERROR(fixture->read_back_depth(colourOnly.value()), core::error_code::INVALID_ARGUMENT);
            scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), colourOnly.value()));
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CONJURE_CHECK_ERROR(fixture->read_back_depth(colourOnly.value()), core::error_code::INVALID_ARGUMENT);
            // The colour read-back of the same target is unaffected.
            CONJURE_CHECK_OK(fixture->read_back(colourOnly.value()));
        }
        SUBCASE("a lost device is DEVICE_LOST, even for a target no view has rendered into") {
            logs.allow_errors();
            fixture->on_device_lost();
            CONJURE_CHECK_ERROR(fixture->read_back_depth(target.value()), rhi::rhi_error::DEVICE_LOST);
        }
        SUBCASE("a device lost while waiting for the copy is DEVICE_LOST and stays lost") {
            logs.allow_errors();
            CONJURE_REQUIRE_OK(fixture->render(scene));
            fixture.device->inject_null_fault(rhi::null_fault::WAIT_IDLE);
            CONJURE_CHECK_ERROR(fixture->read_back_depth(target.value()), rhi::rhi_error::DEVICE_LOST);
            CHECK(fixture->is_lost());
            CONJURE_CHECK_ERROR(fixture->render(scene), rhi::rhi_error::DEVICE_LOST);
        }
    }

    TEST_CASE("failures - a loss reported by the device itself is detected at the next render") {
        log_capture logs = {};
        logs.allow_errors();
        headless_renderer fixture = make_headless_renderer();
        fixture.device->simulate_device_loss();
        CHECK(fixture.device->is_lost());
        CONJURE_CHECK_ERROR(fixture->render(render_scene()), rhi::rhi_error::DEVICE_LOST);
        CHECK(fixture->is_lost());
        CONJURE_CHECK_ERROR(fixture->render(render_scene()), rhi::rhi_error::DEVICE_LOST);
        CHECK(fixture->frame_count() == 0);
    }

    TEST_CASE("failures - create on a lost device fails with DEVICE_LOST") {
        log_capture logs = {};
        logs.allow_errors();
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        device->simulate_device_loss();
        CONJURE_CHECK_ERROR(renderer_type::create(headless_desc(*device, *shaders)), rhi::rhi_error::DEVICE_LOST);
    }
}
