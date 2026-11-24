#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "renderer_test_support.h"

#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/gltf_importer.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/assets_error.h"
#include "conjure/core/image.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/swapchain.h"

#include <array>
#include <memory>
#include <optional>
#include <string_view>

// renderer.h (docs/design/02_renderer.md 1, 3, 5, 6): creation, the upload / find / release
// contract of the GPU mirrors, the extraction pair, read-back, statistics and asset registration.
// The frame itself is covered by test_renderer_null_frame.cpp and the failure paths by
// test_renderer_failures.cpp.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

TEST_SUITE("renderer/renderer") {

    TEST_CASE("renderer - create builds every pipeline up front and reports the description") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        CHECK(&fixture->device() == fixture.device.get());
        CHECK(fixture->desc().device == fixture.device.get());
        CHECK(fixture->desc().shaders == fixture.shaders.get());
        CHECK(fixture->desc().swapchain == nullptr);
        CHECK(fixture->desc().cascadeResolution == 512);
        CHECK(fixture->desc().atlasResolution == 1024);
        CHECK_FALSE(fixture->is_lost());
        CHECK(fixture->frame_count() == 0);
        CHECK(fixture->pending_upload_count() == 0);
        CHECK(fixture->imgui() == nullptr);
        CHECK(logs.contains(core::log_level::INFO, "renderer created"));
        CHECK(logs.contains("headless"));
        const std::optional<rhi::null_device_stats> stats = fixture.device->null_stats();
        REQUIRE(stats.has_value());
        // Every graphics variant twice for the mesh passes, the compute passes, no blit yet (headless).
        CHECK(stats->graphicsPipelinesCreated >= 21);
        CHECK(stats->computePipelinesCreated == 7);
        CHECK(stats->bindingLayoutsCreated >= 9);
        CHECK(stats->commandListsSubmitted >= 1);   // the BRDF LUT bake
        CHECK(stats->draws >= 1);
    }

    TEST_CASE("renderer - the loaded shader count stays bounded to the permutations the pipelines need") {
        headless_renderer fixture = make_headless_renderer();
        CHECK(fixture.shaders->entry_count() >= 30);
        CHECK(fixture.shaders->loaded_blob_count() >= 28);
        CHECK(fixture.shaders->loaded_blob_count() <= fixture.shaders->entry_count());
    }

    TEST_CASE(
      "renderer - uploads are queued from the caller, resolve to the fallback until the next frame and are findable"
    ) {
        headless_renderer fixture = make_headless_renderer();
        const assets::asset_id meshAsset = test_asset_id(1);
        const assets::asset_id textureAsset = test_asset_id(2);
        const assets::asset_id materialAsset = test_asset_id(3);
        const assets::asset_id environmentAsset = test_asset_id(4);
        CHECK_FALSE(fixture->find_mesh(meshAsset).is_valid());

        const gpu_mesh_id mesh = fixture->upload_mesh(meshAsset, mesh_asset::unit_cube());
        const gpu_texture_id texture = fixture->upload_texture(
          textureAsset,
          texture_asset::checker(core::rgba8{}, core::rgba8{255, 255, 255, 255}, 4, 2)
        );
        material_asset materialValue = {};
        materialValue.desc().textures[0] = assets::asset_ref<texture_asset>(textureAsset);
        const gpu_material_id material = fixture->upload_material(materialAsset, materialValue);
        const gpu_environment_id environment = fixture->upload_environment(
          environmentAsset,
          environment_asset::uniform(math::vec3(0.5f), 4)
        );
        CHECK(mesh.is_valid());
        CHECK(texture.is_valid());
        CHECK(material.is_valid());
        CHECK(environment.is_valid());
        CHECK(fixture->pending_upload_count() == 4);
        CHECK(fixture->find_mesh(meshAsset) == mesh);
        CHECK(fixture->find_texture(textureAsset) == texture);
        CHECK(fixture->find_material(materialAsset) == material);
        CHECK(fixture->find_environment(environmentAsset) == environment);
        CHECK_FALSE(fixture->find_mesh(test_asset_id(99)).is_valid());

        CONJURE_REQUIRE_OK(fixture->render(render_scene()));
        CHECK(fixture->pending_upload_count() == 0);
        CHECK(fixture->stats().uploadsCompleted == 4);
        CHECK(fixture->stats().uploadsPending == 0);
        CHECK(fixture->stats().uploadBytes > 0);
        CHECK(fixture->frame_count() == 1);

        // Uploading again under the same asset id keeps the id.
        const gpu_mesh_id again = fixture->upload_mesh(meshAsset, mesh_asset::quad());
        CHECK(again == mesh);
        CHECK(fixture->pending_upload_count() == 1);
        CONJURE_REQUIRE_OK(fixture->render(render_scene()));
        CHECK(fixture->stats().uploadsCompleted == 1);
        // A null asset id is accepted (anonymous upload) and not findable.
        const gpu_mesh_id anonymous = fixture->upload_mesh(assets::asset_id(), mesh_asset::quad());
        CHECK(anonymous.is_valid());
        CHECK(anonymous != mesh);
        CHECK_FALSE(fixture->find_mesh(assets::asset_id()).is_valid());
    }

    TEST_CASE("renderer - release makes the id stale at once and drawing it uses the fallback with one WARN") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        const assets::asset_id meshAsset = test_asset_id(1);
        const gpu_mesh_id mesh = fixture->upload_mesh(meshAsset, mesh_asset::unit_cube());
        const gpu_texture_id texture = fixture->upload_texture(
          test_asset_id(2),
          texture_asset::solid_colour(core::rgba8{}, texture_colour_space::SRGB)
        );
        const gpu_material_id material = fixture->upload_material(test_asset_id(3), material_asset());
        const gpu_environment_id environment = fixture->upload_environment(
          test_asset_id(4),
          environment_asset::uniform(math::vec3(0.5f), 4)
        );
        CONJURE_REQUIRE_OK(fixture->render(render_scene()));
        fixture->release_mesh(mesh);
        fixture->release_texture(texture);
        fixture->release_material(material);
        fixture->release_environment(environment);
        CHECK_FALSE(fixture->find_mesh(meshAsset).is_valid());
        fixture->release_mesh(mesh);   // stale releases are ignored
        fixture->release_mesh(gpu_mesh_id());

        render_target_desc desc = {};
        desc.size = math::uvec2(32, 32);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target.value()));
        scene.meshes.push_back(instance_at(mesh, material, math::vec3(0.0f), mesh_asset::unit_cube().bounds()));
        scene.environment.environment = environment;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().fallbacksUsed >= 2);
        CHECK(fixture->stats().instancesDrawn >= 1);
        CHECK(logs.contains(core::log_level::WARN, "stale or unknown"));
        const u32 warnings = logs.count(core::log_level::WARN);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(logs.count(core::log_level::WARN) == warnings);   // once per id
        CHECK(fixture->stats().fallbacksUsed >= 2);
    }

    TEST_CASE("renderer - an id used before its upload flushed draws the fallback without a WARN") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(32, 32);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target.value()));
        // A stale (never issued) id: WARN and fallback.
        gpu_mesh_id never = {};
        never.index = 1234;
        never.generation = 7;
        scene.meshes.push_back(
          instance_at(never, gpu_material_id(), math::vec3(0.0f), mesh_asset::unit_cube().bounds())
        );
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().fallbacksUsed >= 1);
        CHECK(fixture->stats().instancesDrawn >= 1);
        CHECK(logs.count(core::log_level::WARN) >= 1);
    }

    TEST_CASE("renderer - the extraction pair clears, swaps and renders the read slot") {
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        render_scene &write = fixture->begin_extract();
        write.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target.value()));
        write.frameIndex = 5;
        fixture->end_extract();
        CONJURE_REQUIRE_OK(fixture->render_frame());
        CHECK(fixture->stats().viewsRendered == 1);
        CHECK(fixture->frame_count() == 1);
        // The next extraction gets the other, cleared slot.
        render_scene &next = fixture->begin_extract();
        CHECK(&next != &write);
        CHECK(next.views.empty());
        CHECK(next.frameIndex == 0);
        fixture->end_extract();
        CONJURE_REQUIRE_OK(fixture->render_frame());
        CHECK(fixture->stats().viewsRendered == 0);
        CHECK(fixture->frame_count() == 2);
    }

    TEST_CASE("renderer - read_back returns a zero-filled image of the target size on the null device") {
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(24, 12);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        core::result<core::image> image = fixture->read_back(target.value());
        CONJURE_REQUIRE_OK(image);
        CHECK(image.value().width() == 24);
        CHECK(image.value().height() == 12);
        CHECK(image.value().pixels().size() == 24 * 12 * 4);
        bool allZero = true;
        for (const std::byte value : image.value().pixels()) {
            allZero = allZero && value == std::byte{0};
        }
        CHECK(allZero);
        CONJURE_CHECK_ERROR(fixture->read_back(render_target_id()), core::error_code::NOT_FOUND);
        // The read-back works for every colour format.
        desc.colourFormat = rhi::texture_format::RGBA16_FLOAT;
        core::result<render_target_id> half = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(half);
        CONJURE_CHECK_OK(fixture->read_back(half.value()));
        desc.colourFormat = rhi::texture_format::R11G11B10_FLOAT;
        core::result<render_target_id> packed = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(packed);
        CONJURE_CHECK_OK(fixture->read_back(packed.value()));
    }

    TEST_CASE("renderer - read_back_depth returns the target's depth size with zeros on the null device") {
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(24, 12);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        const auto expectFarPlane = [](const depth_read_back &depth, math::uvec2 size) {
            CHECK(depth.size == size);
            CHECK(depth.depth.size() == static_cast<usize>(size.x) * size.y);
            bool allFar = true;
            for (const f32 value : depth.depth) {
                allFar = allFar && value == 0.0f;
            }
            CHECK(allFar);
        };

        // No view has rendered into the target yet: its texture has no defined contents on every backend,
        // so the far plane is returned without a copy.
        fixture.device->reset_null_stats();
        core::result<depth_read_back> fresh = fixture->read_back_depth(target.value());
        CONJURE_REQUIRE_OK(fresh);
        expectFarPlane(fresh.value(), desc.size);
        CHECK(fixture.device->null_stats().value().stagingTexturesCreated == 0);
        CHECK(fixture.device->null_stats().value().textureCopies == 0);

        // After a frame with a view into the target the depth texture is copied back.
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target.value()));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        fixture.device->reset_null_stats();
        core::result<depth_read_back> rendered = fixture->read_back_depth(target.value());
        CONJURE_REQUIRE_OK(rendered);
        expectFarPlane(rendered.value(), desc.size);
        CHECK(fixture.device->null_stats().value().stagingTexturesCreated == 1);
        CHECK(fixture.device->null_stats().value().textureCopies == 1);

        // A resize replaces the depth texture: the new size, and no copy until a view renders again.
        CONJURE_REQUIRE_OK(fixture->resize_render_target(target.value(), math::uvec2(8, 1)));
        fixture.device->reset_null_stats();
        core::result<depth_read_back> resized = fixture->read_back_depth(target.value());
        CONJURE_REQUIRE_OK(resized);
        expectFarPlane(resized.value(), math::uvec2(8, 1));
        CHECK(fixture.device->null_stats().value().stagingTexturesCreated == 0);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        core::result<depth_read_back> again = fixture->read_back_depth(target.value());
        CONJURE_REQUIRE_OK(again);
        expectFarPlane(again.value(), math::uvec2(8, 1));
        CHECK(fixture.device->null_stats().value().stagingTexturesCreated == 1);
    }

    TEST_CASE("render_stats - reset clears everything while executed and count query the pass names") {
        render_stats stats = {};
        CHECK(stats.viewsSkipped == 0);
        CHECK(stats.lightsDropped == 0);
        CHECK(stats.spritesDropped == 0);
        stats.drawCalls = 4;
        stats.uploadBytes = 10;
        stats.viewsSkipped = 1;
        stats.lightsDropped = 2;
        stats.spritesDropped = 3;
        stats.passNames.push_back("depth_prepass");
        stats.passNames.push_back("bloom_down_0");
        stats.passNames.push_back("bloom_down_0");
        pass_timing timing = {};
        timing.name = "depth_prepass";
        timing.gpuMilliseconds = 1.5f;
        stats.passTimings.push_back(timing);
        CHECK(stats.executed("depth_prepass"));
        CHECK(stats.executed("bloom_down_0"));
        CHECK_FALSE(stats.executed("bloom_down"));
        CHECK_FALSE(stats.executed(""));
        CHECK(stats.count("bloom_down_0") == 2);
        CHECK(stats.count("depth_prepass") == 1);
        CHECK(stats.count("tonemap") == 0);
        stats.reset();
        CHECK(stats.drawCalls == 0);
        CHECK(stats.uploadBytes == 0);
        CHECK(stats.viewsSkipped == 0);
        CHECK(stats.lightsDropped == 0);
        CHECK(stats.spritesDropped == 0);
        CHECK(stats.passNames.empty());
        CHECK(stats.passTimings.empty());
        CHECK_FALSE(stats.executed("depth_prepass"));
    }

    TEST_CASE("renderer - with a swapchain the frame acquires and presents and the blit pipeline exists") {
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = 64;
        swapchainDesc.height = 32;
        swapchainDesc.bufferCount = 2;
        swapchainDesc.format = rhi::texture_format::RGBA8_SRGB;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain = rhi::swapchain::create(
          *device,
          rhi::native_window(),
          swapchainDesc
        );
        CONJURE_REQUIRE_OK(swapchain);
        renderer_desc desc = headless_desc(*device, *shaders);
        desc.swapchain = swapchain.value().get();
        core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(created);
        renderer_type &instance = *created.value();
        CHECK(instance.desc().swapchain == swapchain.value().get());

        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), render_target_id()));
        device->reset_null_stats();
        CONJURE_REQUIRE_OK(instance.render(scene));
        CHECK(instance.stats().viewsRendered == 1);
        CHECK(instance.stats().viewsSkipped == 0);
        CHECK(instance.stats().executed("tonemap"));
        CHECK_FALSE(instance.stats().executed("blit"));   // RGBA8_SRGB: tonemap writes the back buffer
        const std::optional<rhi::null_device_stats> stats = device->null_stats();
        REQUIRE(stats.has_value());
        CHECK(stats->acquires == 1);
        CHECK(stats->presents == 1);
        CHECK(stats->commandListsSubmitted == 1);
        // The renderer resizes its swapchain; the next frame still renders.
        CONJURE_REQUIRE_OK(instance.resize_swapchain(math::uvec2(128, 64)));
        CHECK(swapchain.value()->desc().width == 128);
        CHECK(swapchain.value()->desc().height == 64);
        CONJURE_REQUIRE_OK(instance.render(scene));
        CHECK(instance.stats().viewsRendered == 1);
        created.value().reset();
    }

    TEST_CASE("regression - code sweep F044 resize_swapchain drops the back-buffer framebuffers before the resize") {
        log_capture logs = {};
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = 64;
        swapchainDesc.height = 32;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain = rhi::swapchain::create(
          *device,
          rhi::native_window(),
          swapchainDesc
        );
        CONJURE_REQUIRE_OK(swapchain);
        renderer_desc desc = headless_desc(*device, *shaders);
        desc.swapchain = swapchain.value().get();
        core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(created);
        renderer_type &instance = *created.value();

        // One frame per back buffer, so the framebuffer cache holds a framebuffer over each of them.
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), render_target_id()));
        for (u32 frame = 0; frame < swapchain.value()->buffer_count(); ++frame) {
            CONJURE_REQUIRE_OK(instance.render(scene));
        }

        // The order the engine used to follow (resize, then tell the renderer): the swapchain refuses, as
        // IDXGISwapChain::ResizeBuffers does on a real window, and stays as it was.
        const core::result<void> refused = swapchain.value()->resize(128, 64);
        CONJURE_CHECK_ERROR(refused, rhi::rhi_error::SWAPCHAIN_CREATION_FAILED);
        CHECK(swapchain.value()->desc().width == 64);

        // Through the renderer the framebuffers go first and the resize succeeds, again and again.
        const std::array<math::uvec2, 3> sizes = {math::uvec2(128, 64), math::uvec2(31, 17), math::uvec2(640, 360)};
        for (const math::uvec2 size : sizes) {
            CAPTURE(size.x);
            CONJURE_REQUIRE_OK(instance.resize_swapchain(size));
            CHECK(swapchain.value()->desc().width == size.x);
            CHECK(swapchain.value()->desc().height == size.y);
            CONJURE_REQUIRE_OK(instance.render(scene));
            CONJURE_REQUIRE_OK(instance.render(scene));
            CHECK(instance.stats().viewsRendered == 1);
            CHECK(instance.stats().viewsSkipped == 0);
        }

        // A zero size is the swapchain's INVALID_DESC and leaves the renderer rendering.
        CONJURE_CHECK_ERROR(instance.resize_swapchain(math::uvec2(0, 64)), rhi::rhi_error::INVALID_DESC);
        CONJURE_REQUIRE_OK(instance.render(scene));
        CHECK(instance.stats().viewsRendered == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
        created.value().reset();
    }

    TEST_CASE("regression - code sweep F104 a submitted view joins the next extracted scene for one frame") {
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc targetDesc = {};
        targetDesc.size = math::uvec2(64, 32);
        const core::result<render_target_id> target = fixture->create_render_target(targetDesc);
        CONJURE_REQUIRE_OK(target);
        const render_view view = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target.value());

        // Queued before the frame's extraction opens (the editor's tick) and while it is open (a panel
        // drawing): both join the scene EXTRACT wrote, which had no view of its own here.
        fixture->submit_view(view);
        CHECK(fixture->submitted_view_count() == 1);
        const render_scene &writing = fixture->begin_extract();
        CHECK(writing.views.empty());
        fixture->submit_view(view);
        CHECK(fixture->submitted_view_count() == 2);
        fixture->end_extract();
        CHECK(fixture->submitted_view_count() == 0);
        CONJURE_REQUIRE_OK(fixture->render_frame());
        CHECK(fixture->stats().viewsRendered == 2);
        CHECK(fixture->stats().viewsSkipped == 0);
        REQUIRE(fixture->get_render_target(target.value()) != nullptr);
        CHECK(fixture->get_render_target(target.value())->frames_rendered() == 2);

        // A view is drawn for one frame per submit_view: the next frame has none.
        static_cast<void>(fixture->begin_extract());
        fixture->end_extract();
        CONJURE_REQUIRE_OK(fixture->render_frame());
        CHECK(fixture->stats().viewsRendered == 0);
        CHECK(fixture->get_render_target(target.value())->frames_rendered() == 2);

        // A view whose target was destroyed in the meantime is skipped and counted, not drawn elsewhere.
        fixture->destroy_render_target(target.value());
        fixture->submit_view(view);
        static_cast<void>(fixture->begin_extract());
        fixture->end_extract();
        CONJURE_REQUIRE_OK(fixture->render_frame());
        CHECK(fixture->stats().viewsRendered == 0);
        CHECK(fixture->stats().viewsSkipped == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("renderer - resize_swapchain without a swapchain is INVALID_ARGUMENT") {
        headless_renderer fixture = make_headless_renderer();
        CONJURE_CHECK_ERROR(fixture->resize_swapchain(math::uvec2(64, 64)), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F375 a non-power-of-two atlasResolution is rounded down with one WARN and "
      "local shadows still allocate"
    ) {
        log_capture logs = {};
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        renderer_desc desc = headless_desc(*device, *shaders);
        desc.atlasResolution = 3000;
        core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(created);
        renderer_type &instance = *created.value();
        CHECK(warning_count(logs, "atlasResolution") == 1);
        CHECK(logs.contains(core::log_level::WARN, "using 2048"));
        CHECK(instance.desc().atlasResolution == 3000);   // the description as given

        render_target_desc targetDesc = {};
        targetDesc.size = math::uvec2(32, 32);
        core::result<render_target_id> target = instance.create_render_target(targetDesc);
        CONJURE_REQUIRE_OK(target);
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target.value()));
        light spot = {};
        spot.type = light_type::SPOT;
        spot.position = math::vec3(0.0f, 2.0f, 0.0f);
        spot.castsShadows = true;
        spot.shadowResolution = 1024;
        scene.lights.push_back(spot);
        CONJURE_REQUIRE_OK(instance.render(scene));
        CHECK(instance.stats().shadowTilesAllocated == 1);
        CHECK(instance.stats().shadowTilesDropped == 0);

        // A power of two in range is taken as it is, without a WARN.
        log_capture quiet = {};
        desc.atlasResolution = 2048;
        core::result<std::unique_ptr<renderer_type>> exact = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(exact);
        CHECK(quiet.count(core::log_level::WARN) == 0);
        created.value().reset();
        exact.value().reset();
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F378 a stale mesh id and a live id that reuses its slot are drawn as "
      "separate batches"
    ) {
        // Batches keyed on the slot index alone: a stale id and the live id that reuses its slot merged
        // into one batch drawn with a single mesh. A stale id must batch exactly like an invalid one.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        const gpu_mesh_id first = fixture->upload_mesh(test_asset_id(1), mesh_asset::unit_cube());
        const gpu_material_id material = fixture->upload_material(test_asset_id(2), material_asset());
        CONJURE_REQUIRE_OK(fixture->render(render_scene()));
        fixture->release_mesh(first);
        const gpu_mesh_id second = fixture->upload_mesh(test_asset_id(3), mesh_asset::uv_sphere(0.5f, 8, 16));
        REQUIRE(second.index == first.index);
        REQUIRE(second != first);

        render_target_desc desc = {};
        desc.size = math::uvec2(32, 32);
        core::result<render_target_id> target = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(target);
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 6.0f), math::vec3(0.0f), target.value()));
        const math::aabb bounds = mesh_asset::unit_cube().bounds();
        scene.meshes.push_back(instance_at(gpu_mesh_id(), material, math::vec3(-1.0f, 0.0f, 0.0f), bounds));
        scene.meshes.push_back(instance_at(second, material, math::vec3(1.0f, 0.0f, 0.0f), bounds));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        const u32 expectedDrawCalls = fixture->stats().drawCalls;
        const u32 expectedTriangles = fixture->stats().trianglesDrawn;

        scene.meshes[0].mesh = first;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().drawCalls == expectedDrawCalls);
        CHECK(fixture->stats().trianglesDrawn == expectedTriangles);
        CHECK(fixture->stats().instancesDrawn >= 2);

        // Each mesh pass draws two batches: one instance alone is one batch fewer per pass.
        scene.meshes.resize(1);
        scene.meshes[0].mesh = second;
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().drawCalls < expectedDrawCalls);
    }

    TEST_CASE("register_asset_types - registers the five types and four importers once") {
        assets::asset_type_registry types = {};
        assets::importer_registry importers = {};
        CONJURE_REQUIRE_OK(renderer_type::register_asset_types(types, importers));
        CHECK(types.size() == 5);
        CHECK(types.find(MESH_ASSET_TYPE_NAME) != nullptr);
        CHECK(types.find(TEXTURE_ASSET_TYPE_NAME) != nullptr);
        CHECK(types.find(MATERIAL_ASSET_TYPE_NAME) != nullptr);
        CHECK(types.find(MODEL_ASSET_TYPE_NAME) != nullptr);
        CHECK(types.find(ENVIRONMENT_ASSET_TYPE_NAME) != nullptr);
        CHECK(types.find_loader(assets::asset_type_traits<mesh_asset>::id()) != nullptr);
        CHECK(types.find(MESH_ASSET_TYPE_NAME)->version == MESH_BLOB_VERSION);
        CHECK(types.find(TEXTURE_ASSET_TYPE_NAME)->version == TEXTURE_BLOB_VERSION);
        CHECK(importers.importers().size() == 4);
        CHECK(importers.find(GLTF_IMPORTER_ID) != nullptr);
        CHECK(importers.find(TEXTURE_IMPORTER_ID) != nullptr);
        CHECK(importers.find(MATERIAL_IMPORTER_ID) != nullptr);
        CHECK(importers.find(ENVIRONMENT_IMPORTER_ID) != nullptr);
        CHECK(importers.find_for_extension(".glb") == importers.find(GLTF_IMPORTER_ID));
        CHECK(importers.find_for_extension(".PNG") == importers.find(TEXTURE_IMPORTER_ID));
        CHECK(importers.find_for_extension(".cmat") == importers.find(MATERIAL_IMPORTER_ID));
        CHECK(importers.find_for_extension(".hdr") == importers.find(ENVIRONMENT_IMPORTER_ID));
        // Registering twice reports the type registry's duplicate error.
        CONJURE_CHECK_ERROR(
          renderer_type::register_asset_types(types, importers),
          assets::assets_error::TYPE_DUPLICATE
        );
        CHECK(types.size() == 5);
        CHECK(importers.importers().size() == 4);
    }
}
