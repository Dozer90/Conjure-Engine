#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "renderer_test_support.h"

#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/core/image.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/texture_format.h"

#include <memory>
#include <string>

// The failure paths (docs/design/02_renderer.md 6, 10 "test_renderer_failures"): creation refusals
// name their cause, bad arguments are rejected before any GPU work, device loss stops rendering
// for good, and a bad scene never aborts a frame.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

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
        CONJURE_REQUIRE_OK(directory.write_text("index.json", "{ \"version\": 1, \"shaders\": [] }"));
        std::unique_ptr<rhi::device> device = make_device();
        core::result<std::unique_ptr<rhi::shader_library>> empty = rhi::shader_library::open(*device, directory.path());
        CONJURE_REQUIRE_OK(empty);
        const core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(
          headless_desc(*device, *empty.value())
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

    TEST_CASE("failures - a bad scene is clamped: invalid bounds, zero-size viewports and NaN-free extremes") {
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
        scene.shadows.cascadeCount = 0;
        scene.shadows.distance = -5.0f;
        scene.postProcess.manualEv100 = 1000.0f;
        scene.postProcess.exposure = exposure_mode::MANUAL;
        scene.ssao.radius = -1.0f;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 1);
        CHECK(fixture->stats().instancesCulled == 1);   // the invalid box is never visible
        CHECK(fixture->stats().instancesDrawn >= 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
        // Settings at their extremes are clamped as well.
        scene.shadows.cascadeCount = 99;
        scene.shadows.splitLambda = 7.0f;
        scene.shadows.casterPadding = -10.0f;
        scene.postProcess.bloomIntensity = -1.0f;
        scene.environment.rotationRadians = 1.0e6f;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 1);
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
