#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "integration_test_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/renderer/camera.h"
#include "conjure/renderer/gltf_importer.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/texture_asset.h"
#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/rhi/shader_library.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A generated glTF goes through the whole asset pipeline (database scan, the glTF importer on a job,
// the cooked cache, the manager's blocking load with its dependencies) and lands in the renderer on
// the null device, which renders one frame of it (ARCHITECTURE.md 5 and 11 "Wave 3 exit"). The engine's
// renderer_subsystem (wave 4) does the same wiring from asset events; here the test plays that part.

using namespace conjure;
using namespace conjure::testing;
using namespace integration_test;

namespace {

    struct headless_renderer {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<rhi::shader_library> shaders = nullptr;
        std::unique_ptr<renderer::renderer> instance = nullptr;

        headless_renderer() = default;

        // Destroyed in reverse creation order: the renderer before the library, both before the device.
        ~headless_renderer() {
            instance.reset();
            shaders.reset();
            device.reset();
        }

        headless_renderer(const headless_renderer &) = delete;
        headless_renderer &operator=(const headless_renderer &) = delete;
        headless_renderer(headless_renderer &&) noexcept = default;
        headless_renderer &operator=(headless_renderer &&) noexcept = default;
    };

    [[nodiscard]] headless_renderer make_headless_renderer() {
        headless_renderer fixture = {};
        rhi::device_desc deviceDesc = {};
        deviceDesc.backend = rhi::graphics_backend::NULL_DEVICE;
        core::result<std::unique_ptr<rhi::device>> device = rhi::device::create(deviceDesc);
        CONJURE_REQUIRE_OK(device);
        fixture.device = std::move(device).value();

        const std::filesystem::path shaderDirectory = shader_directory();
        REQUIRE_FALSE(shaderDirectory.empty());
        core::result<std::unique_ptr<rhi::shader_library>> shaders =
          rhi::shader_library::open(*fixture.device, shaderDirectory);
        CONJURE_REQUIRE_OK(shaders);
        fixture.shaders = std::move(shaders).value();

        renderer::renderer_desc desc = {};
        desc.device = fixture.device.get();
        desc.shaders = fixture.shaders.get();
        desc.swapchain = nullptr;
        desc.cascadeResolution = 512;
        desc.atlasResolution = 1024;
        desc.enableImGui = false;
        core::result<std::unique_ptr<renderer::renderer>> created = renderer::renderer::create(desc);
        CONJURE_REQUIRE_OK(created);
        fixture.instance = std::move(created).value();
        return fixture;
    }

    /// <summary>
    /// What renderer_subsystem does on LOADED: every texture, mesh and material of the model, textures
    /// first so the materials resolve them, then the model's nodes become mesh instances.
    /// </summary>
    struct uploaded_model {
        u32 textures = 0;
        u32 meshes = 0;
        u32 materials = 0;
        std::vector<renderer::mesh_instance> instances = {};
    };

    [[nodiscard]] uploaded_model upload_model(
      assets::asset_manager &manager,
      renderer::renderer &instance,
      const renderer::model_asset &model
    ) {
        uploaded_model result = {};
        for (const assets::asset_id &materialId : model.materials()) {
            const assets::asset_ref<renderer::material_asset> material =
              manager.bind<renderer::material_asset>(materialId);
            REQUIRE(material.get() != nullptr);
            for (const assets::asset_ref<renderer::texture_asset> &texture : material.get()->desc().textures) {
                if (texture.id().is_null() || instance.find_texture(texture.id()).is_valid()) {
                    continue;
                }
                const assets::asset_ref<renderer::texture_asset> bound =
                  manager.bind<renderer::texture_asset>(texture.id());
                REQUIRE(bound.get() != nullptr);
                CHECK(instance.upload_texture(texture.id(), *bound.get()).is_valid());
                ++result.textures;
            }
        }
        for (const assets::asset_id &meshId : model.meshes()) {
            const assets::asset_ref<renderer::mesh_asset> mesh = manager.bind<renderer::mesh_asset>(meshId);
            REQUIRE(mesh.get() != nullptr);
            CHECK(instance.upload_mesh(meshId, *mesh.get()).is_valid());
            ++result.meshes;
        }
        for (const assets::asset_id &materialId : model.materials()) {
            const assets::asset_ref<renderer::material_asset> material =
              manager.bind<renderer::material_asset>(materialId);
            CHECK(instance.upload_material(materialId, *material.get()).is_valid());
            ++result.materials;
        }

        const std::span<const renderer::model_node> nodes = model.nodes();
        for (u32 index = 0; index < nodes.size(); ++index) {
            const renderer::model_node &node = nodes[index];
            if (node.mesh == renderer::INVALID_INDEX) {
                continue;
            }
            const assets::asset_id &meshId = model.meshes()[node.mesh];
            const assets::asset_ref<renderer::mesh_asset> mesh = manager.bind<renderer::mesh_asset>(meshId);
            renderer::mesh_instance meshInstance = {};
            meshInstance.mesh = instance.find_mesh(meshId);
            for (u32 slot = 0; slot < renderer::MAX_MATERIAL_SLOTS; ++slot) {
                if (node.materials[slot] != renderer::INVALID_INDEX) {
                    meshInstance.materials[slot] = instance.find_material(model.materials()[node.materials[slot]]);
                }
            }
            const math::transform world = model.world_transform(index);
            meshInstance.worldFromLocal = world.to_matrix();
            meshInstance.worldBounds = mesh.get()->bounds().transformed(meshInstance.worldFromLocal);
            meshInstance.entityId = index;
            result.instances.push_back(meshInstance);
        }
        return result;
    }
}

TEST_SUITE("integration/assets_renderer_headless") {

    TEST_CASE("assets_renderer_headless - a generated glTF loads through assets and renders on the null device") {
        log_capture logs = {};

        // The editor-side pipeline with the renderer's asset types and importers (R10: the loaders are
        // CPU-only, so nothing here needs a device).
        asset_pipeline pipeline = {};
        CONJURE_REQUIRE_OK(renderer::renderer::register_asset_types(pipeline.manager->types(), pipeline.importers));
        pipeline.open();
        pipeline.write_source("models/cube.gltf", textured_cube_gltf());
        pipeline.scan_and_import();

        const assets::asset_id modelId = pipeline.id_of("models/cube.gltf");
        const std::optional<assets::asset_record> record = pipeline.database->find(modelId);
        REQUIRE(record.has_value());
        CHECK(record->importerId == renderer::GLTF_IMPORTER_ID);
        CHECK(record->status == assets::asset_record_status::OK);
        CHECK(record->type == assets::asset_type_traits<renderer::model_asset>::id());
        // The model is the primary asset; mesh, material and texture are its sub-assets and dependencies.
        CHECK(pipeline.database->find_by_path("assets://models/cube.gltf#mesh/0").has_value());
        CHECK(pipeline.database->find_by_path("assets://models/cube.gltf#material/0").has_value());
        CHECK(pipeline.database->find_by_path("assets://models/cube.gltf#texture/0").has_value());

        // The runtime side: a blocking load brings the model and, dependencies first, everything it names.
        // The LOADED events queued by the load (delivered by the next update()) record that order.
        std::vector<assets::asset_event> events = {};
        const assets::asset_subscription subscription =
          pipeline.manager->subscribe([&events](const assets::asset_event &event) {
              events.push_back(event);
          });
        REQUIRE(subscription.is_valid());
        core::result<assets::asset_handle> loaded = pipeline.manager->load_blocking(modelId);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().state() == assets::asset_state::LOADED);
        const renderer::model_asset *model = loaded.value().get<renderer::model_asset>();
        REQUIRE(model != nullptr);
        REQUIRE(model->nodes().size() == 2);
        REQUIRE(model->meshes().size() == 1);
        REQUIRE(model->materials().size() == 1);
        CHECK(model->nodes()[0].name == "root");
        CHECK(model->nodes()[1].name == "cube");
        CHECK(model->nodes()[1].mesh == 0);
        CHECK(model->nodes()[1].materials[0] == 0);
        const assets::asset_ref<renderer::mesh_asset> mesh =
          pipeline.manager->bind<renderer::mesh_asset>(model->meshes()[0]);
        REQUIRE(mesh.get() != nullptr);
        CHECK(mesh.get()->vertex_count() == 24);
        CHECK(mesh.get()->index_count() == 36);
        const assets::asset_ref<renderer::material_asset> material =
          pipeline.manager->bind<renderer::material_asset>(model->materials()[0]);
        REQUIRE(material.get() != nullptr);
        // The material payload names its texture by uuid; the dependency itself is resident (loaded
        // first), which is what the renderer's upload resolves by id.
        const assets::asset_id textureId = material.get()->desc().textures[0].id();
        CHECK_FALSE(textureId.is_null());
        const assets::asset_ref<renderer::texture_asset> texture =
          pipeline.manager->bind<renderer::texture_asset>(textureId);
        CHECK(texture.state() == assets::asset_state::LOADED);
        REQUIRE(texture.get() != nullptr);

        // Load order: the model names mesh and material, the material names the texture, so the four
        // LOADED events arrive leaves first (texture before material) and the model last.
        pipeline.manager->update();
        pipeline.manager->unsubscribe(subscription);
        REQUIRE(events.size() == 4);
        std::optional<usize> textureEvent = std::nullopt;
        std::optional<usize> materialEvent = std::nullopt;
        std::optional<usize> meshEvent = std::nullopt;
        for (usize index = 0; index < events.size(); ++index) {
            CAPTURE(index);
            CHECK(events[index].kind == assets::asset_event_kind::LOADED);
            textureEvent = events[index].id == textureId ? index : textureEvent;
            materialEvent = events[index].id == model->materials()[0] ? index : materialEvent;
            meshEvent = events[index].id == model->meshes()[0] ? index : meshEvent;
        }
        REQUIRE(textureEvent.has_value());
        REQUIRE(materialEvent.has_value());
        REQUIRE(meshEvent.has_value());
        CHECK(*textureEvent < *materialEvent);
        CHECK(events[3].id == modelId);
        CHECK(events[3].type == assets::asset_type_traits<renderer::model_asset>::id());

        // Into the renderer and through one frame.
        headless_renderer fixture = make_headless_renderer();
        const uploaded_model uploaded = upload_model(*pipeline.manager, *fixture.instance, *model);
        CHECK(uploaded.textures == 1);
        CHECK(uploaded.meshes == 1);
        CHECK(uploaded.materials == 1);
        REQUIRE(uploaded.instances.size() == 1);
        CHECK(fixture.instance->pending_upload_count() == 3);
        // The root sits at (1, 2, 3) and the cube node inherits it.
        CONJURE_CHECK_APPROX(math::vec3(uploaded.instances[0].worldFromLocal[3]), math::vec3(1.0f, 2.0f, 3.0f));

        renderer::render_target_desc targetDesc = {};
        targetDesc.size = math::uvec2(128, 72);
        core::result<renderer::render_target_id> target = fixture.instance->create_render_target(targetDesc);
        CONJURE_REQUIRE_OK(target);

        renderer::render_scene &scene = fixture.instance->begin_extract();
        renderer::render_view view = {};
        view.cameraData.projection = renderer::projection_type::PERSPECTIVE;
        view.cameraData.verticalFovRadians = 1.0472f;
        view.cameraData.nearPlane = 0.1f;
        view.cameraData.farPlane = 100.0f;
        view.cameraPosition = math::vec3(1.0f, 2.0f, 8.0f);
        view.viewFromWorld =
          math::look_at(view.cameraPosition, math::vec3(1.0f, 2.0f, 3.0f), math::vec3(0.0f, 1.0f, 0.0f));
        view.target = target.value();
        scene.views.push_back(view);
        scene.meshes = uploaded.instances;
        renderer::light sun = {};
        sun.type = renderer::light_type::DIRECTIONAL;
        sun.direction = glm::normalize(math::vec3(0.3f, -1.0f, 0.2f));
        sun.castsShadows = false;
        scene.lights.push_back(sun);
        fixture.instance->end_extract();

        fixture.device->reset_null_stats();
        CONJURE_REQUIRE_OK(fixture.instance->render_frame());
        const renderer::render_stats &stats = fixture.instance->stats();
        CHECK(stats.viewsRendered == 1);
        CHECK(stats.viewsSkipped == 0);
        CHECK(stats.uploadsCompleted == 3);
        CHECK(stats.uploadsPending == 0);
        CHECK(stats.fallbacksUsed == 0);
        CHECK(stats.lightsRendered == 1);
        CHECK(stats.lightsDropped == 0);
        CHECK(stats.instancesCulled == 0);

        // One opaque instance in one view with the default settings (shadows on but no caster, SSAO,
        // automatic exposure and bloom on, no environment, sprites or debug lines, an sRGB target):
        // the graph is exactly the prepass, the three SSAO passes, the forward pass, the exposure pair,
        // the bloom chain and the tonemap, in declaration order (engine/src/renderer/renderer.cpp
        // render_view_graph). No shadow pass (the sun casts none), no light cull (no local light), no
        // skybox, blend, sprite, debug or blit pass.
        const std::array<std::string_view, 19> expectedPasses = {
          "depth_prepass",      "ssao_gtao",      "ssao_blur_h",  "ssao_blur_v",  "forward_opaque",
          "exposure_histogram", "exposure_adapt", "bloom_down_0", "bloom_down_1", "bloom_down_2",
          "bloom_down_3",       "bloom_down_4",   "bloom_down_5", "bloom_up_4",   "bloom_up_3",
          "bloom_up_2",         "bloom_up_1",     "bloom_up_0",   "tonemap",
        };
        REQUIRE(stats.passNames.size() == expectedPasses.size());
        for (usize index = 0; index < expectedPasses.size(); ++index) {
            CAPTURE(index);
            CHECK(std::string_view(stats.passNames[index]) == expectedPasses[index]);
        }
        CHECK(stats.passesExecuted == expectedPasses.size());
        CHECK(stats.passesCulled == 0);
        CHECK(stats.shadowCascadesRendered == 0);
        CHECK(stats.shadowTilesAllocated == 0);

        // The mesh passes draw the cube (12 triangles) once each; the tonemap draws its fullscreen
        // triangle (engine/src/renderer/passes/pass_shared.cpp draw_mesh_region, tonemap_pass.cpp).
        // The compute passes dispatch once each: SSAO 3, exposure 2, bloom 6 down + 5 up.
        CHECK(stats.drawCalls == 3);
        CHECK(stats.instancesDrawn == 2);
        CHECK(stats.trianglesDrawn == 2 * 12 + 1);
        CHECK(stats.dispatches == 3 + 2 + 6 + 5);
        // Transient textures of the first frame: the view normals (prepass; the depth is the render
        // target's own, imported in place of a transient since code sweep F384), the HDR target
        // (forward), the half-resolution AO, AO depth and the two blur targets (SSAO) and the six
        // bloom mips.
        CHECK(stats.transientTexturesCreated == 1 + 1 + 4 + 6);
        CHECK(stats.transientTexturesLive == stats.transientTexturesCreated);

        // What the null device recorded for the same frame (engine/src/rhi/backends/null): the draws
        // and dispatches above, one graphics state per draw (the tonemap draw is one instance), the
        // uploads flushed at the start of the frame (three mesh buffers and the material constants,
        // each written once; the texture written once per mip) and the per-frame writes of the
        // instance records, the light list and the view constants. The only other buffer created is
        // the view's light tile list, which frame_resources grows lazily on the first frame.
        const std::optional<rhi::null_device_stats> deviceStats = fixture.device->null_stats();
        REQUIRE(deviceStats.has_value());
        CHECK(deviceStats->commandListsSubmitted == 1);
        CHECK(deviceStats->draws == stats.drawCalls);
        CHECK(deviceStats->instancesDrawn == stats.instancesDrawn + 1);
        CHECK(deviceStats->drawsIndirect == 0);
        CHECK(deviceStats->dispatches == stats.dispatches);
        CHECK(deviceStats->graphicsStateChanges == stats.drawCalls);
        CHECK(deviceStats->buffersCreated == 3 + 1 + 1);
        CHECK(deviceStats->texturesCreated == 1 + stats.transientTexturesCreated);
        CHECK(deviceStats->bufferWrites == 3 + 1 + 3);
        CHECK(deviceStats->textureWrites == texture.get()->mip_count());
        CHECK(deviceStats->presents == 0);                 // headless: no swapchain
        CHECK(deviceStats->swapchainsCreated == 0);
        CHECK(deviceStats->markersBegun == deviceStats->markersEnded);
        CHECK(deviceStats->markersBegun >= stats.passesExecuted);
        CHECK(fixture.instance->frame_count() == 1);

        // A second frame draws the same instance without new uploads, and every transient texture,
        // buffer and pass is reused: the device creates nothing and records the same draws again.
        renderer::render_scene &again = fixture.instance->begin_extract();
        again.views.push_back(view);
        again.meshes = uploaded.instances;
        again.lights.push_back(sun);
        fixture.instance->end_extract();
        CONJURE_REQUIRE_OK(fixture.instance->render_frame());
        const renderer::render_stats &second = fixture.instance->stats();
        CHECK(second.uploadsCompleted == 0);
        CHECK(second.drawCalls == 3);
        CHECK(second.instancesDrawn == 2);
        CHECK(second.trianglesDrawn == 2 * 12 + 1);
        CHECK(second.passesExecuted == stats.passesExecuted);
        CHECK(second.transientTexturesCreated == 0);
        CHECK(second.transientTexturesLive == stats.transientTexturesLive);
        const std::optional<rhi::null_device_stats> cumulative = fixture.device->null_stats();
        REQUIRE(cumulative.has_value());
        CHECK(cumulative->commandListsSubmitted == 2);
        CHECK(cumulative->draws == 2 * stats.drawCalls);
        CHECK(cumulative->dispatches == 2 * stats.dispatches);
        CHECK(cumulative->buffersCreated == deviceStats->buffersCreated);
        CHECK(cumulative->texturesCreated == deviceStats->texturesCreated);
        CHECK(cumulative->textureWrites == deviceStats->textureWrites);
        CHECK(cumulative->bufferWrites == deviceStats->bufferWrites + 3);
        CHECK(fixture.instance->frame_count() == 2);
    }
}
