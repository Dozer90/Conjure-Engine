#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/renderer/renderer.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "engine/subsystems/model_prefab_converter.h"
#include "engine/subsystems/renderer_components.h"
#include "engine/subsystems/renderer_subsystem.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// renderer_subsystem (docs/design/06_engine_loop.md 3.4 step 13, 09 R7/R8/R10 and section 3): the renderer
// components exist even headless, a LOADED mesh asset becomes a GPU mirror through the asset events, the
// extraction fills views, instances and lights from render_world_transform, and a model_asset converts into
// a prefab document through prefab_builder.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    // Frames the empty-frame baseline is taken over (test_hierarchy_propagation.cpp uses the same count): the
    // maximum over several frames absorbs a periodic block (an asset manager sweep, a stats sample) that two
    // frames would miss and the relative gate below would then report against the extraction.
    constexpr u32 BASELINE_FRAMES = 5;

    [[nodiscard]] renderer_subsystem *find_renderer_subsystem(application &app) {
        for (const std::unique_ptr<engine_subsystem> &subsystem : app.subsystems()) {
            if (subsystem->name() == "renderer") {
                return dynamic_cast<renderer_subsystem *>(subsystem.get());
            }
        }
        return nullptr;
    }

    [[nodiscard]] assets::cooked_blob mesh_blob(const assets::asset_id &id) {
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<renderer::mesh_asset>::id();
        header.typeVersion = renderer::MESH_BLOB_VERSION;
        header.importerVersion = 1;
        header.id = id;
        return assets::make_cooked_blob(header, {}, renderer::mesh_asset::unit_cube().write_blob());
    }

    [[nodiscard]] ecs::entity create_camera(application &app) {
        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> entity = active.create_entity(core::string_id("camera"));
        CONJURE_REQUIRE_OK(entity);
        scene::local_transform where = {};
        where.position = math::vec3(0.0f, 1.0f, 5.0f);
        CONJURE_REQUIRE_OK(active.set_local_transform(entity.value(), where));
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::camera_component>(entity.value()));
        return entity.value();
    }
}

TEST_SUITE("engine/renderer_subsystem") {

    TEST_CASE("renderer_subsystem - the renderer components are registered even without rendering") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        const ecs::component_registry &registry = *app.context().components;
        CHECK(registry.find(core::string_id(renderer::MESH_RENDERER_COMPONENT_TYPE_NAME)).is_valid());
        CHECK(registry.find(core::string_id(renderer::SPRITE_COMPONENT_TYPE_NAME)).is_valid());
        CHECK(registry.find(core::string_id(renderer::CAMERA_COMPONENT_TYPE_NAME)).is_valid());
        CHECK(registry.find(core::string_id(renderer::LIGHT_COMPONENT_TYPE_NAME)).is_valid());
        CHECK(registry.find(core::string_id(renderer::ENVIRONMENT_COMPONENT_TYPE_NAME)).is_valid());
        CHECK(app.context().assets->types().find(renderer::MESH_ASSET_TYPE_NAME) != nullptr);
        CHECK(app.context().renderer == nullptr);
        app.shutdown();
    }

    TEST_CASE("renderer_subsystem - shader_directory prefers the configured path") {
        engine_config config = engine_config::defaults();
        config.rendering.shaderDirectory.clear();
        const std::filesystem::path expected = std::filesystem::path("C:/engine") / "shaders";
        CHECK(renderer_subsystem::shader_directory(config, "C:/engine") == expected);
        config.rendering.shaderDirectory = "D:/custom";
        CHECK(renderer_subsystem::shader_directory(config, "C:/engine") == std::filesystem::path("D:/custom"));
    }

    TEST_CASE("renderer_subsystem - a loaded mesh is uploaded and extracted with the camera view") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().renderer != nullptr);
        renderer_subsystem *subsystem = find_renderer_subsystem(app);
        REQUIRE(subsystem != nullptr);

        // The empty frame's own cost in this executable (0 in Development; an MSVC Debug build sees the
        // debug-iterator block of asset_manager::update, test_hierarchy_propagation.cpp): the extraction must add
        // nothing to it. The gate is relative, so a standard library whose empty frame costs more or less than
        // MSVC's (libstdc++, libc++) measures its own baseline.
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        u64 emptyFrame = 0;
        for (u32 frame = 0; frame < BASELINE_FRAMES; ++frame) {
            allocation_scope scope = {};
            CONJURE_REQUIRE_OK(app.step_frame());
            emptyFrame = std::max(emptyFrame, scope.allocation_count());
        }

        // A cube from a memory source added next to the project's cache source.
        const assets::asset_id meshId = core::uuid::generate();
        std::unique_ptr<assets::memory_asset_source> source = std::make_unique<assets::memory_asset_source>();
        source->add(mesh_blob(meshId), "assets://meshes/cube.mesh");
        app.context().assets->add_source(std::move(source));
        assets::asset_handle handle = app.context().assets->load(meshId);

        CONJURE_UNUSED(create_camera(app));
        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> cube = active.create_entity(core::string_id("cube"));
        CONJURE_REQUIRE_OK(cube);
        renderer::mesh_renderer_component meshRenderer = {};
        meshRenderer.mesh = app.context().assets->bind<renderer::mesh_asset>(meshId);
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::mesh_renderer_component>(cube.value(), meshRenderer));
        const core::result<ecs::entity> lamp = active.create_entity(core::string_id("lamp"));
        CONJURE_REQUIRE_OK(lamp);
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::light_component>(lamp.value()));
        const core::result<ecs::entity> hidden = active.create_entity(core::string_id("hidden"));
        CONJURE_REQUIRE_OK(hidden);
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::light_component>(hidden.value()));
        CONJURE_REQUIRE_OK(app.context().world->add<scene::disabled>(hidden.value()));

        for (u32 frame = 0; frame < 8 && handle.state() != assets::asset_state::LOADED; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
        }
        REQUIRE(handle.state() == assets::asset_state::LOADED);
        CONJURE_REQUIRE_OK(app.step_frame());

        CHECK(subsystem->stats().uploads >= 1);
        CHECK(app.context().renderer->find_mesh(meshId).is_valid());
        CHECK(subsystem->stats().views == 1);
        CHECK(subsystem->stats().meshes == 1);
        CHECK(subsystem->stats().meshesSkipped == 0);
        CHECK(subsystem->stats().lights == 1);
        CHECK_FALSE(subsystem->stats().environmentSeen);
        CHECK(app.context().renderer->frame_count() >= 1);

        // Steady state (08_testing.md 7.1, engine.full_frame_10k_transform_render): the camera candidate list and
        // the render_scene keep their capacity, so a frame with a camera, a mesh and a light costs no more than
        // the empty frame.
        for (u32 frame = 0; frame < 3; ++frame) {
            CAPTURE(frame);
            allocation_scope scope = {};
            CONJURE_REQUIRE_OK(app.step_frame());
            const u64 allocations = scope.allocation_count();
            CHECK_MESSAGE(
              allocations <= emptyFrame,
              "a warmed-up extraction frame allocated " << allocations << " block(s); the empty frame " << emptyFrame
            );
        }
        CHECK(subsystem->stats().views == 1);
        CHECK(subsystem->stats().meshes == 1);

        // The test's own reference goes before shutdown: the manager asserts on live handles.
        CONJURE_REQUIRE_OK(app.context().world->destroy_entity(cube.value()));
        handle = assets::asset_handle{};
        app.shutdown();
    }

    TEST_CASE("renderer_subsystem - a mesh renderer without a GPU mesh is skipped and counted") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = find_renderer_subsystem(app);
        REQUIRE(subsystem != nullptr);
        CONJURE_UNUSED(create_camera(app));
        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> ghost = active.create_entity(core::string_id("ghost"));
        CONJURE_REQUIRE_OK(ghost);
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::mesh_renderer_component>(ghost.value()));
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->stats().meshes == 0);
        CHECK(subsystem->stats().meshesSkipped == 1);
        CHECK(subsystem->stats().views == 1);
        app.shutdown();
    }

    TEST_CASE("renderer_subsystem - alternating loaded and unknown meshes are each extracted or skipped") {
        // The extraction remembers the last mesh lookup; an unknown mesh between two uses of the same loaded one
        // must neither reuse the loaded mirror nor hide it from the second cube.
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = find_renderer_subsystem(app);
        REQUIRE(subsystem != nullptr);

        const assets::asset_id meshId = core::uuid::generate();
        std::unique_ptr<assets::memory_asset_source> source = std::make_unique<assets::memory_asset_source>();
        source->add(mesh_blob(meshId), "assets://meshes/cube.mesh");
        app.context().assets->add_source(std::move(source));
        assets::asset_handle handle = app.context().assets->load(meshId);

        CONJURE_UNUSED(create_camera(app));
        scene::scene &active = app.context().scenes->active();
        std::vector<ecs::entity> cubes = {};
        for (const std::string_view name : {"cube_a", "ghost", "cube_b"}) {
            const core::result<ecs::entity> entity = active.create_entity(core::string_id::intern(name));
            CONJURE_REQUIRE_OK(entity);
            renderer::mesh_renderer_component meshRenderer = {};
            if (name != "ghost") {
                meshRenderer.mesh = app.context().assets->bind<renderer::mesh_asset>(meshId);
                cubes.push_back(entity.value());
            }
            CONJURE_REQUIRE_OK(
              app.context().world->add<renderer::mesh_renderer_component>(entity.value(), meshRenderer)
            );
        }

        for (u32 frame = 0; frame < 8 && handle.state() != assets::asset_state::LOADED; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
        }
        REQUIRE(handle.state() == assets::asset_state::LOADED);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->stats().meshes == 2);
        CHECK(subsystem->stats().meshesSkipped == 1);

        for (const ecs::entity cube : cubes) {
            CONJURE_REQUIRE_OK(app.context().world->destroy_entity(cube));
        }
        handle = assets::asset_handle{};
        app.shutdown();
    }

    TEST_CASE("model_prefab_converter - nodes become prefab nodes with renderer components") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());

        renderer::model_asset model = {};
        model.set_name("lamp_post");
        const u32 mesh = model.add_mesh(core::uuid::generate());
        const u32 material = model.add_material(core::uuid::generate());
        renderer::model_node root = {};
        root.name = "root";
        root.localTransform.position = math::vec3(1.0f, 2.0f, 3.0f);
        const u32 rootIndex = model.add_node(root);
        renderer::model_node post = {};
        post.name = "post";
        post.parent = rootIndex;
        post.mesh = mesh;
        post.materials[0] = material;
        model.add_node(post);
        renderer::model_node bulb = {};
        bulb.name = "bulb";
        bulb.parent = rootIndex;
        bulb.hasLight = true;
        bulb.lightData.intensity = 42.0f;
        bulb.hasCamera = true;
        model.add_node(bulb);

        {
            model_prefab_converter converter(*app.context().components);
            const core::result<scene::prefab_document> document = converter.convert(model);
            CONJURE_REQUIRE_OK(document);
            CHECK(document.value().name == "lamp_post");
            REQUIRE(document.value().archive != nullptr);
            CHECK(document.value().archive->entity_count() == 3);
            CHECK(converter.stats().nodes == 3);
            CHECK(converter.stats().meshRenderers == 1);
            CHECK(converter.stats().lights == 1);
            CHECK(converter.stats().cameras == 1);

            // An empty model still yields a usable one-node prefab.
            renderer::model_asset empty = {};
            const core::result<scene::prefab_document> single = converter.convert(empty, "placeholder");
            CONJURE_REQUIRE_OK(single);
            CHECK(single.value().name == "placeholder");
            CHECK(converter.stats().nodes == 1);
        }
        // The archives reference the registry: they died before the application shuts down.
        app.shutdown();
    }
}
