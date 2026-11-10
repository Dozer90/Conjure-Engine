#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "renderer_test_support.h"

#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"

#include <array>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

// The whole frame on the null device (docs/design/02_renderer.md 10 "test_renderer_null_frame"):
// 10k instances, 64 lights with shadow casters, every pass of the graph, the view flags that cull
// passes, sprites, debug lines, the environment, blended materials and the blit for non-sRGB targets.
// The null device records what was submitted (null_device_stats), the renderer what it did
// (render_stats).

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    struct scene_assets {
        gpu_mesh_id cube = {};
        gpu_mesh_id sphere = {};
        gpu_material_id opaque = {};
        gpu_material_id masked = {};
        gpu_material_id blended = {};
        gpu_texture_id texture = {};
        gpu_environment_id environment = {};
        math::aabb cubeBounds = {};
        math::aabb sphereBounds = {};
    };

    [[nodiscard]] scene_assets upload_assets(renderer_type &instance) {
        scene_assets assets = {};
        const mesh_asset cube = mesh_asset::unit_cube();
        const mesh_asset sphere = mesh_asset::uv_sphere(0.5f, 8, 16);
        assets.cube = instance.upload_mesh(test_asset_id(1), cube);
        assets.sphere = instance.upload_mesh(test_asset_id(2), sphere);
        assets.cubeBounds = cube.bounds();
        assets.sphereBounds = sphere.bounds();
        assets.texture = instance.upload_texture(
          test_asset_id(3),
          texture_asset::checker(core::rgba8{}, core::rgba8{255, 255, 255, 255}, 8, 2)
        );
        material_asset opaque = {};
        opaque.desc().textures[0] = assets::asset_ref<texture_asset>(test_asset_id(3));
        assets.opaque = instance.upload_material(test_asset_id(4), opaque);
        material_asset masked = {};
        masked.desc().alphaMode = alpha_mode::MASK;
        masked.desc().doubleSided = true;
        assets.masked = instance.upload_material(test_asset_id(5), masked);
        material_asset blended = {};
        blended.desc().alphaMode = alpha_mode::BLEND;
        blended.desc().baseColourFactor.w = 0.5f;
        assets.blended = instance.upload_material(test_asset_id(6), blended);
        assets.environment = instance.upload_environment(
          test_asset_id(7),
          environment_asset::uniform(math::vec3(0.4f), 8)
        );
        return assets;
    }

    /// <summary>
    /// 10 000 cubes on a 100 x 100 grid in front of the camera, a quarter of them masked.
    /// </summary>
    void fill_grid(render_scene &scene, const scene_assets &assets, u32 side = 100) {
        const std::span<mesh_instance> instances = scene.allocate_meshes(side * side);
        for (u32 index = 0; index < side * side; ++index) {
            const f32 x = (static_cast<f32>(index % side) - static_cast<f32>(side) * 0.5f) * 2.0f;
            const f32 z = -(static_cast<f32>(index / side) * 2.0f) - 2.0f;
            instances[index] = instance_at(
              assets.cube,
              (index % 4 == 0) ? assets.masked : assets.opaque,
              math::vec3(x, 0.0f, z),
              assets.cubeBounds
            );
            instances[index].entityId = index;
        }
    }

    [[nodiscard]] light sun_light(bool shadows) {
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = glm::normalize(math::vec3(0.3f, -1.0f, 0.2f));
        sun.intensity = 3.0f;
        sun.castsShadows = shadows;
        return sun;
    }

    void add_local_lights(render_scene &scene, u32 count, u32 shadowCasters) {
        for (u32 index = 0; index < count; ++index) {
            light value = {};
            value.type = (index % 3 == 0) ? light_type::SPOT : light_type::POINT;
            value.position = math::vec3(
              static_cast<f32>(index % 8) * 4.0f - 16.0f,
              2.0f,
              -static_cast<f32>(index / 8) * 6.0f - 4.0f
            );
            value.range = 8.0f;
            value.castsShadows = index < shadowCasters;
            value.shadowResolution = (index % 2 == 0) ? 512 : 256;
            scene.lights.push_back(value);
        }
    }

    [[nodiscard]] render_target_id make_target(
      renderer_type &instance,
      math::uvec2 size,
      rhi::texture_format format = rhi::texture_format::RGBA8_SRGB
    ) {
        render_target_desc desc = {};
        desc.size = size;
        desc.colourFormat = format;
        core::result<render_target_id> created = instance.create_render_target(desc);
        CONJURE_REQUIRE_OK(created);
        return created.value();
    }

    [[nodiscard]] render_view grid_view(render_target_id target) {
        return look_at_view(math::vec3(0.0f, 30.0f, 20.0f), math::vec3(0.0f, 0.0f, -80.0f), target);
    }
}

TEST_SUITE("renderer/null_frame") {

    TEST_CASE("null_frame - 10k instances with 64 lights run every pass of the graph on the null device") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(320, 180));
        render_scene scene = {};
        scene.views.push_back(grid_view(target));
        fill_grid(scene, assets);
        scene.lights.push_back(sun_light(true));
        add_local_lights(scene, 63, 4);
        scene.environment.environment = assets.environment;
        scene.sprites.resize(0);

        fixture.device->reset_null_stats();
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &stats = fixture->stats();
        CHECK(stats.viewsRendered == 1);
        CHECK(stats.viewsSkipped == 0);
        CHECK(stats.uploadsCompleted == 7);
        CHECK(stats.fallbacksUsed == 0);
        CHECK(stats.instancesCulled < 10000);
        CHECK(stats.instancesDrawn > 0);
        CHECK(stats.drawCalls > 0);
        CHECK(stats.trianglesDrawn >= stats.instancesDrawn * 12);
        CHECK(stats.lightsRendered == 64);
        CHECK(stats.lightsDropped == 0);
        CHECK(stats.shadowCascadesRendered == MAX_SHADOW_CASCADES);
        CHECK(stats.shadowTilesAllocated == 4 + 2 * 5);   // spot lights 0 and 3: one tile; points 1, 2: six each
        CHECK(stats.shadowTilesDropped == 0);
        CHECK(stats.dispatches > 0);
        CHECK(stats.passesCulled == 0);

        const std::array<const char *, 19> expectedPasses = {
          "shadow_cascade_0", "shadow_cascade_1", "shadow_cascade_2", "shadow_cascade_3", "shadow_atlas",
          "depth_prepass",    "light_cull",       "ssao_gtao",        "ssao_blur_h",      "ssao_blur_v",
          "forward_opaque",   "skybox",           "exposure_histogram", "exposure_adapt", "bloom_down_0",
          "bloom_down_5",     "bloom_up_4",       "bloom_up_0",       "tonemap",
        };
        for (const char *name : expectedPasses) {
            CAPTURE(name);
            CHECK(stats.executed(name));
        }
        CHECK_FALSE(stats.executed("blit"));
        CHECK_FALSE(stats.executed("forward_blend"));
        CHECK_FALSE(stats.executed("world_sprites"));
        CHECK_FALSE(stats.executed("screen_sprites"));
        CHECK_FALSE(stats.executed("debug_draw"));
        CHECK(stats.passesExecuted == stats.passNames.size());
        // Declaration order is execution order.
        usize prepass = 0;
        usize forward = 0;
        usize tonemap = 0;
        for (usize index = 0; index < stats.passNames.size(); ++index) {
            const std::string_view name = stats.passNames[index];
            prepass = name == "depth_prepass" ? index : prepass;
            forward = name == "forward_opaque" ? index : forward;
            tonemap = name == "tonemap" ? index : tonemap;
        }
        CHECK(prepass < forward);
        CHECK(forward < tonemap);

        const std::optional<rhi::null_device_stats> nullStats = fixture.device->null_stats();
        REQUIRE(nullStats.has_value());
        CHECK(nullStats->commandListsSubmitted == 1);
        CHECK(nullStats->draws == stats.drawCalls);
        CHECK(nullStats->instancesDrawn >= stats.instancesDrawn);
        CHECK(nullStats->dispatches == stats.dispatches);
        CHECK(nullStats->markersBegun == nullStats->markersEnded);
        CHECK(nullStats->markersBegun >= stats.passesExecuted);
        CHECK(nullStats->bufferWrites > 0);
        CHECK(nullStats->bufferWriteBytes >= 10000 * 96);   // the instance records
        CHECK(nullStats->depthClears >= 1);
        CHECK(stats.transientTexturesLive > 0);
        CHECK(stats.transientTexturesCreated == stats.transientTexturesLive);
        CHECK(logs.count(core::log_level::ERR) == 0);

        // The second frame reuses every transient texture, framebuffer and binding set.
        fixture.device->reset_null_stats();
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().transientTexturesCreated == 0);
        CHECK(fixture->stats().transientTexturesLive == stats.transientTexturesLive);
        CHECK(fixture->stats().passesExecuted == stats.passesExecuted);
        const std::optional<rhi::null_device_stats> secondStats = fixture.device->null_stats();
        REQUIRE(secondStats.has_value());
        CHECK(secondStats->texturesCreated == 0);
        CHECK(secondStats->framebuffersCreated == 0);
        CHECK(secondStats->bindingSetsCreated == 0);
        CHECK(secondStats->graphicsPipelinesCreated == 0);
        CHECK(fixture->frame_count() == 2);
    }

    TEST_CASE("null_frame - culling on jobs produces the same frame as the serial path") {
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc jobsDesc = {};
        jobsDesc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(jobsDesc));
        {
            headless_renderer serial = make_headless_renderer();
            headless_renderer parallel = make_headless_renderer(false, &jobs);
            const scene_assets serialAssets = upload_assets(*serial);
            const scene_assets parallelAssets = upload_assets(*parallel);
            const render_target_id serialTarget = make_target(*serial, math::uvec2(256, 144));
            const render_target_id parallelTarget = make_target(*parallel, math::uvec2(256, 144));
            render_scene serialScene = {};
            serialScene.views.push_back(grid_view(serialTarget));
            fill_grid(serialScene, serialAssets);
            serialScene.lights.push_back(sun_light(true));
            render_scene parallelScene = {};
            parallelScene.views.push_back(grid_view(parallelTarget));
            fill_grid(parallelScene, parallelAssets);
            parallelScene.lights.push_back(sun_light(true));
            CONJURE_REQUIRE_OK(serial->render(serialScene));
            CONJURE_REQUIRE_OK(parallel->render(parallelScene));
            CHECK(parallel->stats().instancesCulled == serial->stats().instancesCulled);
            CHECK(parallel->stats().instancesDrawn == serial->stats().instancesDrawn);
            CHECK(parallel->stats().drawCalls == serial->stats().drawCalls);
            CHECK(parallel->stats().passesExecuted == serial->stats().passesExecuted);
            CHECK(parallel->stats().instancesDrawn > 0);
        }
        jobs.stop();
    }

    TEST_CASE("null_frame - everything behind the camera is culled and no mesh pass runs") {
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(64, 64));
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 500.0f), math::vec3(0.0f, 0.0f, 1000.0f), target));
        fill_grid(scene, assets, 20);
        scene.lights.push_back(sun_light(false));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &stats = fixture->stats();
        CHECK(stats.instancesCulled == 400);
        CHECK(stats.instancesDrawn == 0);
        CHECK_FALSE(stats.executed("depth_prepass"));
        CHECK_FALSE(stats.executed("forward_opaque"));
        CHECK_FALSE(stats.executed("ssao_gtao"));
        CHECK_FALSE(stats.executed("exposure_histogram"));
        CHECK_FALSE(stats.executed("bloom_down_0"));
        CHECK(stats.executed("tonemap"));
        CHECK(stats.viewsRendered == 1);
        // The layer mask culls too: a view of layer 2 sees none of the layer 1 instances.
        scene.views[0] = grid_view(target);
        scene.views[0].layerMask = 2;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().instancesCulled == 400);
        CHECK(fixture->stats().instancesDrawn == 0);
    }

    TEST_CASE("null_frame - NO_POST_PROCESS culls exposure, bloom and SSAO and SKIP_SHADOWS the shadow passes") {
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(128, 72));
        render_scene scene = {};
        scene.views.push_back(grid_view(target));
        scene.views[0].flags = view_flags::NO_POST_PROCESS;
        fill_grid(scene, assets, 10);
        scene.lights.push_back(sun_light(true));
        add_local_lights(scene, 3, 1);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &first = fixture->stats();
        CHECK(first.executed("depth_prepass"));
        CHECK(first.executed("forward_opaque"));
        CHECK(first.executed("shadow_cascade_0"));
        CHECK(first.executed("shadow_atlas"));
        CHECK(first.executed("tonemap"));
        CHECK_FALSE(first.executed("exposure_histogram"));
        CHECK_FALSE(first.executed("exposure_adapt"));
        CHECK_FALSE(first.executed("bloom_down_0"));
        CHECK_FALSE(first.executed("bloom_up_0"));
        CHECK_FALSE(first.executed("ssao_gtao"));
        CHECK(first.passesCulled == 0);

        scene.views[0].flags = view_flags::SKIP_SHADOWS;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &second = fixture->stats();
        CHECK_FALSE(second.executed("shadow_cascade_0"));
        CHECK_FALSE(second.executed("shadow_atlas"));
        CHECK(second.shadowCascadesRendered == 0);
        CHECK(second.executed("exposure_histogram"));
        CHECK(second.executed("bloom_down_0"));
        CHECK(second.executed("ssao_gtao"));

        // Scene-wide switches: shadows and SSAO off, manual exposure, bloom off.
        scene.views[0].flags = view_flags::NONE;
        scene.shadows.enabled = false;
        scene.ssao.enabled = false;
        scene.postProcess.exposure = exposure_mode::MANUAL;
        scene.postProcess.bloomEnabled = false;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &third = fixture->stats();
        CHECK_FALSE(third.executed("shadow_cascade_0"));
        CHECK_FALSE(third.executed("shadow_atlas"));
        CHECK_FALSE(third.executed("ssao_gtao"));
        CHECK_FALSE(third.executed("exposure_histogram"));
        CHECK_FALSE(third.executed("bloom_down_0"));
        CHECK(third.executed("forward_opaque"));
        CHECK(third.executed("tonemap"));
        CHECK(third.shadowTilesAllocated == 0);
    }

    TEST_CASE("null_frame - zero views is a no-op frame and a swapchain view is skipped when headless") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        fixture.device->reset_null_stats();
        CONJURE_REQUIRE_OK(fixture->render(render_scene()));
        const render_stats &stats = fixture->stats();
        CHECK(stats.viewsRendered == 0);
        CHECK(stats.viewsSkipped == 0);
        CHECK(stats.passesExecuted == 0);
        CHECK(stats.drawCalls == 0);
        CHECK(stats.passNames.empty());
        const std::optional<rhi::null_device_stats> nullStats = fixture.device->null_stats();
        REQUIRE(nullStats.has_value());
        CHECK(nullStats->commandListsSubmitted == 1);
        CHECK(nullStats->draws == 0);
        CHECK(fixture->frame_count() == 1);

        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), render_target_id()));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsSkipped == 1);
        CHECK(fixture->stats().viewsRendered == 0);
        CHECK(logs.contains(core::log_level::WARN, "headless"));
        const u32 warnings = logs.count(core::log_level::WARN);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(logs.count(core::log_level::WARN) == warnings);   // warned once
    }

    TEST_CASE("null_frame - views beyond MAX_VIEWS and lights beyond MAX_LIGHTS_PER_VIEW are clamped with a WARN") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(32, 32));
        render_scene scene = {};
        for (u32 index = 0; index < MAX_VIEWS + 2; ++index) {
            scene.views.push_back(grid_view(target));
        }
        fill_grid(scene, assets, 4);
        add_local_lights(scene, MAX_LIGHTS_PER_VIEW + 10, 0);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &stats = fixture->stats();
        CHECK(stats.viewsRendered == MAX_VIEWS);
        CHECK(stats.viewsSkipped == 2);
        CHECK(stats.lightsDropped == 10 * MAX_VIEWS);
        CHECK(stats.lightsRendered == MAX_LIGHTS_PER_VIEW * MAX_VIEWS);
        CHECK(logs.contains(core::log_level::WARN, "exceed MAX_VIEWS"));
        CHECK(logs.contains(core::log_level::WARN, "exceed MAX_LIGHTS_PER_VIEW"));
        CHECK(stats.count("tonemap") == MAX_VIEWS);
        CHECK(stats.count("light_cull") == MAX_VIEWS);
        const u32 warnings = logs.count(core::log_level::WARN);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(logs.count(core::log_level::WARN) == warnings);   // once per change of the overflow
        // A view into the target counts frames once per view.
        CHECK(fixture->get_render_target(target)->frames_rendered() == 2 * MAX_VIEWS);
    }

    TEST_CASE("null_frame - sprites, debug lines, blended meshes and the environment add their passes") {
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(96, 64));
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target));
        scene.views[0].flags = view_flags::DRAW_DEBUG;
        scene.meshes.push_back(instance_at(assets.sphere, assets.blended, math::vec3(0.0f), assets.sphereBounds));
        scene.meshes.push_back(
          instance_at(assets.cube, assets.opaque, math::vec3(1.5f, 0.0f, 0.0f), assets.cubeBounds)
        );
        scene.environment.environment = assets.environment;
        for (u32 index = 0; index < 5; ++index) {
            sprite value = {};
            value.texture = assets.texture;
            value.space = index < 3 ? sprite_space::WORLD : sprite_space::SCREEN;
            value.size = math::vec2(32.0f);
            scene.sprites.push_back(value);
        }
        scene.debugDraw.line(math::vec3(0.0f), math::vec3(1.0f), math::vec4(1.0f));
        scene.debugDraw.box(assets.cubeBounds, math::vec4(1.0f, 0.0f, 0.0f, 1.0f), false);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &stats = fixture->stats();
        CHECK(stats.executed("depth_prepass"));
        CHECK(stats.executed("forward_opaque"));
        CHECK(stats.executed("skybox"));
        CHECK(stats.executed("forward_blend"));
        CHECK(stats.executed("world_sprites"));
        CHECK(stats.executed("debug_draw"));
        CHECK(stats.executed("screen_sprites"));
        CHECK(stats.executed("tonemap"));
        CHECK(stats.spritesDrawn == 5);
        CHECK(stats.debugVerticesDrawn == 26);
        CHECK(stats.instancesDrawn >= 2);
        usize skybox = 0;
        usize blend = 0;
        usize world = 0;
        usize tonemap = 0;
        usize screen = 0;
        for (usize index = 0; index < stats.passNames.size(); ++index) {
            const std::string_view name = stats.passNames[index];
            skybox = name == "skybox" ? index : skybox;
            blend = name == "forward_blend" ? index : blend;
            world = name == "world_sprites" ? index : world;
            tonemap = name == "tonemap" ? index : tonemap;
            screen = name == "screen_sprites" ? index : screen;
        }
        CHECK(skybox < blend);
        CHECK(blend < world);
        CHECK(world < tonemap);
        CHECK(tonemap < screen);

        // Blended geometry alone still gets a depth buffer and the light lists.
        scene.meshes.resize(1);
        scene.views[0].flags = view_flags::NONE;
        add_local_lights(scene, 2, 0);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().executed("depth_prepass"));
        CHECK(fixture->stats().executed("light_cull"));
        CHECK(fixture->stats().executed("forward_blend"));
        CHECK_FALSE(fixture->stats().executed("forward_opaque"));
        CHECK_FALSE(fixture->stats().executed("debug_draw"));
        CHECK_FALSE(fixture->stats().executed("ssao_gtao"));
    }

    TEST_CASE("null_frame - a non-sRGB target adds the blit and a viewport restricts the view") {
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id half = make_target(*fixture, math::uvec2(64, 64), rhi::texture_format::RGBA16_FLOAT);
        const render_target_id unorm = make_target(*fixture, math::uvec2(64, 64), rhi::texture_format::RGBA8_UNORM);
        const render_target_id srgb = make_target(*fixture, math::uvec2(64, 64));
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), half));
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), unorm));
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), srgb));
        scene.views[2].viewport = math::urect{8, 8, 200, 200};   // clamped to the target
        scene.meshes.push_back(instance_at(assets.cube, assets.opaque, math::vec3(0.0f), assets.cubeBounds));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &stats = fixture->stats();
        CHECK(stats.viewsRendered == 3);
        CHECK(stats.count("blit") == 2);
        CHECK(stats.count("tonemap") == 3);
        CHECK(stats.count("forward_opaque") == 3);
        CHECK(stats.instancesDrawn >= 3);
    }

    TEST_CASE("null_frame - released mirrors die bufferCount + 1 frames later and the frame keeps running") {
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(32, 32));
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target));
        scene.meshes.push_back(instance_at(assets.sphere, assets.opaque, math::vec3(0.0f), assets.sphereBounds));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().fallbacksUsed == 0);
        fixture->release_mesh(assets.sphere);
        fixture->release_material(assets.opaque);
        for (u32 frame = 0; frame < 6; ++frame) {
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().instancesDrawn >= 1);
            CHECK(fixture->stats().fallbacksUsed >= 1);
        }
        // Re-uploading under the asset id after the release issues a fresh id.
        const gpu_mesh_id fresh = fixture->upload_mesh(test_asset_id(2), mesh_asset::uv_sphere(0.5f, 4, 8));
        CHECK(fresh != assets.sphere);
        CHECK(fresh.is_valid());
        scene.meshes[0].mesh = fresh;
        scene.meshes[0].materials[0] = assets.masked;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().fallbacksUsed == 0);
    }

    TEST_CASE("regression - uploads and releases from jobs while render() culls on the same job system [parallel]") {
        // The render thread helps with queued jobs inside parallel_for (culling above
        // CULLING_PARALLEL_THRESHOLD instances); a job calling upload_* / release_* must neither
        // deadlock on the cache nor invalidate the mirrors the frame already resolved.
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc jobsDesc = {};
        jobsDesc.workerCount = 1;
        CONJURE_REQUIRE_OK(jobs.start(jobsDesc));
        {
            headless_renderer fixture = make_headless_renderer(false, &jobs);
            const scene_assets assets = upload_assets(*fixture);
            const render_target_id target = make_target(*fixture, math::uvec2(64, 64));
            render_scene scene = {};
            scene.views.push_back(grid_view(target));
            fill_grid(scene, assets, 64);   // 4096 instances: culling and the instance fill run on jobs
            scene.lights.push_back(sun_light(true));
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().fallbacksUsed == 0);

            constexpr usize JOB_COUNT = 64;
            std::vector<gpu_mesh_id> uploaded(JOB_COUNT);
            std::vector<std::function<void()>> work = {};
            for (usize index = 0; index < JOB_COUNT; ++index) {
                work.emplace_back([&fixture, &uploaded, &assets, index]() {
                    uploaded[index] = fixture->upload_mesh(test_asset_id(1000 + index), mesh_asset::quad(1.0f));
                    if (index == JOB_COUNT / 2) {
                        fixture->release_mesh(assets.cube);   // the mesh every instance draws
                    }
                });
            }
            const core::jobs::job_handle handle = jobs.schedule(work);
            for (u32 frame = 0; frame < 4; ++frame) {
                CONJURE_REQUIRE_OK(fixture->render(scene));
                CHECK(fixture->stats().viewsRendered == 1);
            }
            jobs.wait(handle);
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->pending_upload_count() == 0);
            for (usize index = 0; index < JOB_COUNT; ++index) {
                CAPTURE(index);
                CHECK(uploaded[index].is_valid());
                CHECK(fixture->find_mesh(test_asset_id(1000 + index)) == uploaded[index]);
            }
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().fallbacksUsed > 0);   // the released cube draws the fallback
            CHECK(fixture->stats().instancesDrawn > 0);
        }
        jobs.stop();
    }

    TEST_CASE("regression - the shadow atlas is rendered by the first view that draws with shadows") {
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(64, 64));
        render_scene scene = {};
        scene.views.push_back(grid_view(target));
        scene.views.push_back(grid_view(target));
        scene.views[0].flags = view_flags::SKIP_SHADOWS;
        fill_grid(scene, assets, 10);
        add_local_lights(scene, 1, 1);   // one shadow-casting spot light
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &skipping = fixture->stats();
        CHECK(skipping.viewsRendered == 2);
        CHECK(skipping.shadowTilesAllocated == 1);
        CHECK(skipping.count("shadow_atlas") == 1);

        // View 0 targets an unknown render target and is skipped before its preparation.
        scene.views[0].flags = view_flags::NONE;
        render_target_id unknown = {};
        unknown.index = 99;
        unknown.generation = 7;
        scene.views[0].target = unknown;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const render_stats &unknownTarget = fixture->stats();
        CHECK(unknownTarget.viewsSkipped == 1);
        CHECK(unknownTarget.viewsRendered == 1);
        CHECK(unknownTarget.count("shadow_atlas") == 1);

        // No view draws shadows: the atlas is allocated but never rendered.
        scene.views[0].target = target;
        scene.views[0].flags = view_flags::SKIP_SHADOWS;
        scene.views[1].flags = view_flags::SKIP_SHADOWS;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 2);
        CHECK_FALSE(fixture->stats().executed("shadow_atlas"));
    }

    TEST_CASE("regression - inserting a light at the front keeps the other lights' atlas tiles") {
        headless_renderer fixture = make_headless_renderer();
        const scene_assets assets = upload_assets(*fixture);
        const render_target_id target = make_target(*fixture, math::uvec2(64, 64));
        render_scene scene = {};
        scene.views.push_back(grid_view(target));
        fill_grid(scene, assets, 4);
        const auto spot = [](const math::vec3 &position, u16 resolution) {
            light value = {};
            value.type = light_type::SPOT;
            value.position = position;
            value.range = 8.0f;
            value.castsShadows = true;
            value.shadowResolution = resolution;
            return value;
        };
        scene.lights.push_back(spot(math::vec3(-2.0f, 2.0f, -4.0f), 256));
        scene.lights.push_back(spot(math::vec3(2.0f, 2.0f, -4.0f), 512));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().shadowTilesAllocated == 2);
        CHECK(fixture->stats().shadowTilesReused == 0);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().shadowTilesReused == 2);
        // A new light at index 0 shifts the others: their tiles must stay where they were.
        scene.lights.insert(scene.lights.begin(), spot(math::vec3(0.0f, 2.0f, -2.0f), 1024));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().shadowTilesAllocated == 3);
        CHECK(fixture->stats().shadowTilesDropped == 0);
        CHECK(fixture->stats().shadowTilesReused == 2);
        // An entity id pins the identity even when the light moves.
        scene.lights[1].entityId = 42;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        scene.lights[1].position.x += 1.0f;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().shadowTilesReused == 3);
    }
}
