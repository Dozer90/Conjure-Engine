#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/project.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "engine/executable_path.h"
#include "engine/subsystems/model_prefab_converter.h"
#include "engine/subsystems/renderer_components.h"
#include "engine/subsystems/renderer_subsystem.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// renderer_subsystem (docs/design/06_engine_loop.md 3.4 step 13, 09 R7/R8/R10 and section 3): the renderer
// components exist even headless, a LOADED mesh asset becomes a GPU mirror through the asset events, the
// extraction fills views, instances and lights from render_world_transform, and a model_asset converts into
// a prefab document through prefab_builder. The project's rendering defaults (render-settings design 2.3, 2.4)
// are read at startup, strictly by a runtime and leniently by the editor, and the extraction resolves them with
// the render_settings and post_process components into the scene's settings groups and the camera views.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    // Frames the empty-frame baseline is taken over (test_hierarchy_propagation.cpp uses the same count): the
    // maximum over several frames absorbs a periodic block (an asset manager sweep, a stats sample) that two
    // frames would miss and the relative gate below would then report against the extraction.
    constexpr u32 BASELINE_FRAMES = 5;

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

    /// <summary>
    /// The descriptor of a project whose project.json carries 'rendering' (the JSON of the "rendering" member) in
    /// 'directory', started as the editor or as a runtime, with rendering on the null device or disabled.
    /// </summary>
    [[nodiscard]] application_desc rendering_project_desc(
      const temp_directory &directory,
      std::string_view rendering,
      bool editor,
      bool nullRendering
    ) {
        const std::string member = std::format(R"("rendering": {})", rendering);
        project loaded = engine_test::write_project(directory.path(), "rendering_defaults", {}, member);
        engine_config config = engine_test::inline_config();
        config.projectPath = loaded.root_directory();
        config.editor = editor;
        loaded.apply_to(config);
        if (nullRendering) {
            engine_test::enable_null_rendering(config);
        }
        application_desc desc = engine_test::make_desc(config);
        desc.project = std::move(loaded);
        return desc;
    }

    /// <summary>
    /// How many messages of a level the capture holds that contain 'text'.
    /// </summary>
    [[nodiscard]] usize count_containing(const log_capture &logs, core::log_level level, std::string_view text) {
        const std::vector<std::string> messages = logs.messages(level);
        return static_cast<usize>(std::count_if(messages.begin(), messages.end(), [text](const std::string &message) {
            return message.find(text) != std::string::npos;
        }));
    }

    /// <summary>
    /// A root entity of the active scene with a fixed scene guid (so the tie-break order is known) and a
    /// render_settings component.
    /// </summary>
    [[nodiscard]] ecs::entity create_settings_entity(
      application &app,
      const core::uuid &guid,
      const renderer::render_settings_component &settings
    ) {
        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> entity = active.create_entity(core::string_id("render_settings"));
        CONJURE_REQUIRE_OK(entity);
        CONJURE_REQUIRE_OK(active.set_guid(entity.value(), guid));
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::render_settings_component>(entity.value(), settings));
        return entity.value();
    }

    // The project defaults of the settings tests: one leaf of each group, all away from the engine defaults.
    constexpr std::string_view PROJECT_DEFAULTS = R"({"defaults": {"post_process": {"tonemap": "agx",
      "bloom_intensity": 0.5}, "shadows": {"distance": 60.0}, "ssao": {"radius": 0.75}}})";

    /// <summary>
    /// The engine defaults with PROJECT_DEFAULTS applied.
    /// </summary>
    [[nodiscard]] renderer::render_settings project_defaults() {
        renderer::render_settings settings = {};
        settings.postProcess.tonemap = renderer::tonemap_operator::AGX;
        settings.postProcess.bloomIntensity = 0.5f;
        settings.shadows.distance = 60.0f;
        settings.ssao.radius = 0.75f;
        return settings;
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
        CHECK(registry.find(core::string_id(renderer::RENDER_SETTINGS_COMPONENT_TYPE_NAME)).is_valid());
        CHECK(registry.find(core::string_id(renderer::POST_PROCESS_COMPONENT_TYPE_NAME)).is_valid());
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

// A multi-configuration tree puts the executables in bin/<Config>/ and the shaders in bin/shaders, so the
// default lookup cannot work there until code sweep F329 gives each configuration its own shader directory.
#if !defined(CONJURE_TEST_MULTI_CONFIG) || !CONJURE_TEST_MULTI_CONFIG
    TEST_CASE("regression - code sweep F027 the shader index is built beside the executable") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        const engine_config config = engine_config::defaults();
        REQUIRE(config.rendering.shaderDirectory.empty());
        const std::filesystem::path beside = renderer_subsystem::shader_directory(config, executable_directory());
        CHECK(std::filesystem::is_regular_file(beside / "index.json"));
        // The directory conjure_shaders builds into is the one the engine opens by default.
        std::error_code ignored = {};
        CHECK(std::filesystem::equivalent(beside, engine_test::shader_directory(), ignored));
    }

    TEST_CASE("regression - code sweep F027 a renderer starts with no shader directory configured") {
        // What conjure_editor and conjure_runtime do on a windowed start: no rendering.shaderDirectory, so the
        // library is opened next to the executable.
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        config.rendering.shaderDirectory.clear();
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.context().renderer != nullptr);
        app.shutdown();
    }
#endif

    TEST_CASE("renderer_subsystem - a loaded mesh is uploaded and extracted with the camera view") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().renderer != nullptr);
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
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

    TEST_CASE("regression - code sweep F104 a submitted view is drawn into its own target by the engine frame") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer::renderer *instance = app.context().renderer;
        REQUIRE(instance != nullptr);
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
        REQUIRE(subsystem != nullptr);
        CONJURE_UNUSED(create_camera(app));

        renderer::render_target_desc targetDesc = {};
        targetDesc.size = math::uvec2(96, 64);
        const core::result<renderer::render_target_id> target = instance->create_render_target(targetDesc);
        CONJURE_REQUIRE_OK(target);
        renderer::render_view view = {};
        view.viewFromWorld = glm::lookAt(math::vec3(0.0f, 2.0f, 8.0f), math::vec3(0.0f), math::vec3(0.0f, 1.0f, 0.0f));
        view.cameraPosition = math::vec3(0.0f, 2.0f, 8.0f);
        view.target = target.value();

        // Headless there is no swapchain, so the scene camera is counted but draws nowhere; the submitted view
        // has its target and is what the frame renders. This is how the editor's viewport and screenshots get
        // their picture of the scene.
        instance->submit_view(view);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->stats().views == 1);
        CHECK(instance->stats().viewsRendered == 1);
        CHECK(instance->submitted_view_count() == 0);
        REQUIRE(instance->get_render_target(target.value()) != nullptr);
        CHECK(instance->get_render_target(target.value())->frames_rendered() == 1);

        // Not submitted again: the next frame leaves the target alone.
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(instance->stats().viewsRendered == 0);
        CHECK(instance->get_render_target(target.value())->frames_rendered() == 1);
        CHECK(logs.count(core::log_level::WARN) == 0);
        app.shutdown();
    }

    TEST_CASE("renderer_subsystem - a mesh renderer without a GPU mesh is skipped and counted") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
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
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
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

    TEST_CASE("regression - code sweep W3-RS-L2 a runtime refuses to start on invalid rendering defaults") {
        // The shipped game must not run with a look its author did not write: the runtime reads the defaults
        // strictly, before the rendering early-out, so a headless server fails the same way.
        log_capture logs = {};
        logs.allow_errors();
        temp_directory directory("engine_renderer_strict_defaults");
        bool nullRendering = false;
        SUBCASE("headless, rendering disabled") {
            nullRendering = false;
        }
        SUBCASE("rendering on the null device") {
            nullRendering = true;
        }
        CAPTURE(nullRendering);
        constexpr std::string_view RENDERING =
          R"({"defaults": {"post_process": {"bloom_intensity": 0.5}, "ssao": {"radius": -1.0}}})";
        application app(rendering_project_desc(directory, RENDERING, false, nullRendering));
        const core::result<void> started = app.initialise();
        CONJURE_CHECK_ERROR(started, engine_error::RENDERER_INIT_FAILED);
        REQUIRE_FALSE(started.has_value());
        CHECK(started.get_error().message().find("reading the rendering defaults") != std::string::npos);
        CHECK(started.get_error().message().find("rendering/defaults/ssao/radius") != std::string::npos);
        CHECK(app.state() == application_state::CREATED);
        CHECK(app.context().renderDefaults == nullptr);
        CHECK(app.context().renderer == nullptr);
    }

    TEST_CASE("renderer_subsystem - the editor starts on invalid rendering defaults, each bad leaf at its default") {
        // The editor must open the project so its settings can be repaired: one ERR per problem, and only the bad
        // leaves fall back; a valid leaf next to them still applies. Unknown keys are one WARN each on both hosts.
        log_capture logs = {};
        logs.allow_errors();
        temp_directory directory("engine_renderer_lenient_defaults");
        constexpr std::string_view RENDERING = R"({"defaults": {"post_process": {"bloom_intensity": 5.0,
          "tonemap": "reinhard_extended", "vignette": 0.2}, "ssao": {"radius": "wide"}, "fog": {}}})";
        application app(rendering_project_desc(directory, RENDERING, true, false));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().renderDefaults != nullptr);
        const renderer::render_settings &defaults = app.context().renderDefaults->get();
        const renderer::render_settings engineDefaults = {};
        CHECK(defaults.postProcess.bloomIntensity == engineDefaults.postProcess.bloomIntensity);
        CHECK(defaults.postProcess.tonemap == renderer::tonemap_operator::REINHARD_EXTENDED);
        CHECK(defaults.ssao.radius == engineDefaults.ssao.radius);
        CHECK(defaults.shadows == engineDefaults.shadows);
        CHECK(logs.count(core::log_level::ERR) == 2);
        CHECK(count_containing(logs, core::log_level::ERR, "rendering/defaults/post_process/bloom_intensity") == 1);
        CHECK(count_containing(logs, core::log_level::ERR, "rendering/defaults/ssao/radius") == 1);
        CHECK(count_containing(logs, core::log_level::WARN, "rendering/defaults/post_process/vignette") == 1);
        CHECK(count_containing(logs, core::log_level::WARN, "rendering/defaults/fog") == 1);
        app.shutdown();
    }

    TEST_CASE("renderer_subsystem - renderDefaults is published also headless and is null after shutdown") {
        log_capture logs = {};
        temp_directory directory("engine_renderer_published_defaults");
        bool editor = false;
        SUBCASE("runtime") {
            editor = false;
        }
        SUBCASE("editor") {
            editor = true;
        }
        CAPTURE(editor);
        // An unknown key is not an error on either host: one WARN, and the project starts.
        constexpr std::string_view RENDERING = R"({"defaults": {"post_process": {"tonemap": "agx",
          "bloom_intensity": 0.5}, "shadows": {"distance": 60.0}, "ssao": {"radius": 0.75, "samples": 16}}})";
        application app(rendering_project_desc(directory, RENDERING, editor, false));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.context().renderer == nullptr);
        REQUIRE(app.context().renderDefaults != nullptr);
        CHECK(app.context().renderDefaults->get() == project_defaults());
        CHECK(app.context().renderDefaults->revision() >= 1);
        CHECK(count_containing(logs, core::log_level::WARN, "rendering/defaults/ssao/samples") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
        app.shutdown();
        CHECK(app.context().renderDefaults == nullptr);
    }

    TEST_CASE("renderer_subsystem - without a project or without defaults the rendering defaults are the engine's") {
        log_capture logs = {};
        temp_directory directory("engine_renderer_no_defaults");
        application_desc desc = {};
        SUBCASE("no project") {
            desc = engine_test::make_desc(engine_test::inline_config());
        }
        SUBCASE("a rendering section without defaults") {
            desc = rendering_project_desc(directory, R"({"enabled": false})", false, false);
        }
        SUBCASE("null defaults") {
            desc = rendering_project_desc(directory, R"({"defaults": null})", false, false);
        }
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().renderDefaults != nullptr);
        CHECK(app.context().renderDefaults->get() == renderer::render_settings{});
        CHECK(logs.count(core::log_level::WARN) == 0);
        app.shutdown();
    }

    TEST_CASE("regression - code sweep W3-RS-L2 the extracted settings are the project defaults and group winners") {
        // Extraction used to leave render_scene's settings at their defaults, so neither the project nor a
        // render_settings component changed the picture.
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        temp_directory directory("engine_renderer_extract_settings");
        application app(rendering_project_desc(directory, PROJECT_DEFAULTS, false, true));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
        REQUIRE(subsystem != nullptr);
        render_extract_runner extraction(app.context(), *subsystem);
        renderer::render_scene extracted = {};
        const renderer::render_settings defaults = project_defaults();

        // Two components of the same priority over disjoint groups (a base scene's mood and an additively loaded
        // level's shadows): both apply, and the group neither overrides keeps the project's values.
        renderer::render_settings_component mood = {};
        mood.overridePostProcess = true;
        mood.postProcess.tonemap = renderer::tonemap_operator::NONE;
        mood.postProcess.bloomEnabled = false;
        mood.shadows.distance = 15.0f;          // not overridden: never applies
        const ecs::entity moodEntity = create_settings_entity(app, core::uuid(0, 1), mood);
        renderer::render_settings_component level = {};
        level.overrideShadows = true;
        level.shadows.distance = 40.0f;
        level.shadows.cascadeCount = 2;
        CONJURE_UNUSED(create_settings_entity(app, core::uuid(0, 2), level));
        renderer::render_settings_component inert = {};
        inert.priority = 100;
        inert.ssao.enabled = false;             // no flag set: takes part in no group
        CONJURE_UNUSED(create_settings_entity(app, core::uuid(0, 3), inert));

        extraction.run(extracted);
        CHECK(subsystem->stats().settingsCandidates == 3);
        CHECK(extracted.postProcess == mood.postProcess);
        CHECK(extracted.shadows == level.shadows);
        CHECK(extracted.ssao == defaults.ssao);

        SUBCASE("a higher priority wins only the groups it overrides") {
            renderer::render_settings_component boss = {};
            boss.priority = 5;
            boss.overridePostProcess = true;
            boss.overrideSsao = true;
            boss.postProcess.exposure = renderer::exposure_mode::MANUAL;
            boss.ssao.intensity = 2.0f;
            const ecs::entity bossEntity = create_settings_entity(app, core::uuid(0, 9), boss);
            extraction.run(extracted);
            CHECK(extracted.postProcess == boss.postProcess);
            CHECK(extracted.ssao == boss.ssao);
            CHECK(extracted.shadows == level.shadows);

            // A winner that is disabled, inactive or gone leaves its groups to the next candidate or the project.
            SUBCASE("disabled") {
                CONJURE_REQUIRE_OK(app.context().world->add<scene::disabled>(bossEntity));
            }
            SUBCASE("inactive") {
                CONJURE_REQUIRE_OK(app.context().world->add<scene::inactive>(bossEntity));
            }
            SUBCASE("destroyed") {
                CONJURE_REQUIRE_OK(app.context().world->destroy_entity(bossEntity));
            }
            extraction.run(extracted);
            CHECK(extracted.postProcess == mood.postProcess);
            CHECK(extracted.ssao == defaults.ssao);
            CHECK(extracted.shadows == level.shadows);
        }
        SUBCASE("a group without a candidate is the project's") {
            CONJURE_REQUIRE_OK(app.context().world->remove<renderer::render_settings_component>(moodEntity));
            extraction.run(extracted);
            CHECK(subsystem->stats().settingsCandidates == 2);
            CHECK(extracted.postProcess == defaults.postProcess);
            CHECK(extracted.shadows == level.shadows);
        }
        CHECK(logs.count(core::log_level::WARN) == 0);
        // No app.shutdown() here: the application shuts down when it goes out of scope, after the runner
        // that reads its world.
    }

    TEST_CASE("renderer_subsystem - tied render_settings components log one WARN per group until the tie ends") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
        REQUIRE(subsystem != nullptr);
        render_extract_runner extraction(app.context(), *subsystem);
        renderer::render_scene extracted = {};

        renderer::render_settings_component first = {};
        first.priority = 3;
        first.overridePostProcess = true;
        first.overrideSsao = true;
        first.postProcess.bloomIntensity = 0.1f;
        renderer::render_settings_component second = first;
        second.postProcess.bloomIntensity = 0.2f;
        const core::uuid lowGuid(0, 0xA1);
        ecs::entity secondEntity = ecs::NULL_ENTITY;
        std::string secondName = {};
        // The second candidate is created first in both variants, so it would win if the first one found or the
        // lower entity index won.
        SUBCASE("both have a scene guid: the lowest guid wins") {
            const core::uuid highGuid(0, 0xB2);
            secondEntity = create_settings_entity(app, highGuid, second);
            secondName = highGuid.to_string();
        }
        SUBCASE("one has no scene guid: it comes after the one with a guid despite its lower index") {
            // A scene entity always gets a guid, so this one is a bare world entity, as a system may spawn it.
            const core::result<ecs::entity> unnamed = app.context().world->create_entity();
            CONJURE_REQUIRE_OK(unnamed);
            secondEntity = unnamed.value();
            CONJURE_REQUIRE_OK(app.context().world->add<renderer::render_settings_component>(secondEntity, second));
            REQUIRE(app.context().world->get<scene::guid>(secondEntity) == nullptr);
            secondName = std::format("entity {}", secondEntity.index);
        }
        CAPTURE(secondName);
        const ecs::entity firstEntity = create_settings_entity(app, lowGuid, first);
        // Without the guid rule the lower entity index would win the tie.
        CHECK(secondEntity.index < firstEntity.index);
        constexpr std::string_view TIE = "tie at priority";

        extraction.run(extracted);
        CHECK(extracted.postProcess == first.postProcess);
        CHECK(count_containing(logs, core::log_level::WARN, TIE) == 2);
        CHECK(count_containing(logs, core::log_level::WARN, "post_process group") == 1);
        CHECK(count_containing(logs, core::log_level::WARN, "ssao group") == 1);
        CHECK(count_containing(logs, core::log_level::WARN, "shadows group") == 0);
        CHECK(count_containing(logs, core::log_level::WARN, lowGuid.to_string()) == 2);
        CHECK(count_containing(logs, core::log_level::WARN, secondName) == 2);
        // The WARN names the winner, so it is right also where the guid order is not the rule.
        CHECK(count_containing(logs, core::log_level::WARN, std::format("); {} wins", lowGuid.to_string())) == 2);

        // While the tie lasts nothing more is logged.
        extraction.run(extracted);
        CHECK(count_containing(logs, core::log_level::WARN, TIE) == 2);

        // An extraction without the tie re-arms the latch; the next tie is logged again.
        renderer::render_settings_component *raised =
          app.context().world->get<renderer::render_settings_component>(secondEntity);
        REQUIRE(raised != nullptr);
        raised->priority = 4;
        extraction.run(extracted);
        CHECK(extracted.postProcess == second.postProcess);
        CHECK(count_containing(logs, core::log_level::WARN, TIE) == 2);
        raised->priority = 3;
        extraction.run(extracted);
        CHECK(count_containing(logs, core::log_level::WARN, TIE) == 4);
        // No app.shutdown() here: the application shuts down when it goes out of scope, after the runner
        // that reads its world.
    }

    TEST_CASE("renderer_subsystem - a post_process component without a camera is counted and logs one WARN") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
        REQUIRE(subsystem != nullptr);
        render_extract_runner extraction(app.context(), *subsystem);
        renderer::render_scene extracted = {};
        constexpr std::string_view ORPHAN = "without a camera component";

        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> stray = active.create_entity(core::string_id("stray"));
        CONJURE_REQUIRE_OK(stray);
        renderer::post_process_component overrides = {};
        overrides.overridePostProcess = true;
        overrides.postProcess.tonemap = renderer::tonemap_operator::NONE;
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::post_process_component>(stray.value(), overrides));
        const core::result<ecs::entity> hidden = active.create_entity(core::string_id("hidden"));
        CONJURE_REQUIRE_OK(hidden);
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::post_process_component>(hidden.value(), overrides));
        CONJURE_REQUIRE_OK(app.context().world->add<scene::disabled>(hidden.value()));

        extraction.run(extracted);
        CHECK(subsystem->stats().orphanPostProcess == 1);
        CHECK(count_containing(logs, core::log_level::WARN, ORPHAN) == 1);
        // It changes nothing: the scene keeps the project's (here the engine's) post-process group.
        CHECK(extracted.postProcess == renderer::post_process_settings{});
        extraction.run(extracted);
        CHECK(count_containing(logs, core::log_level::WARN, ORPHAN) == 1);

        // On a camera's entity it is no orphan; taken off again, the next orphan frame logs again.
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::camera_component>(stray.value()));
        extraction.run(extracted);
        CHECK(subsystem->stats().orphanPostProcess == 0);
        CONJURE_REQUIRE_OK(app.context().world->remove<renderer::camera_component>(stray.value()));
        extraction.run(extracted);
        CHECK(subsystem->stats().orphanPostProcess == 1);
        CHECK(count_containing(logs, core::log_level::WARN, ORPHAN) == 2);
        // No app.shutdown() here: the application shuts down when it goes out of scope, after the runner
        // that reads its world.
    }

    TEST_CASE("regression - code sweep W3-RS-L2 a camera's post_process component reaches its view") {
        // The camera view used to be built without the camera's overrides, so a per-camera look never rendered.
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
        REQUIRE(subsystem != nullptr);
        CONJURE_UNUSED(create_camera(app));
        const ecs::entity scoped = create_camera(app);
        renderer::post_process_component overrides = {};
        SUBCASE("post-process group") {
            overrides.overridePostProcess = true;
            overrides.postProcess.exposure = renderer::exposure_mode::MANUAL;
        }
        SUBCASE("SSAO group") {
            overrides.overrideSsao = true;
            overrides.ssao.enabled = false;
        }
        CONJURE_REQUIRE_OK(app.context().world->add<renderer::post_process_component>(scoped, overrides));
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->stats().views == 2);
        CHECK(subsystem->stats().viewsWithSettingsOverride == 1);
        CHECK(subsystem->stats().orphanPostProcess == 0);

        // A component that overrides no group carries nothing into the view.
        renderer::post_process_component *current = app.context().world->get<renderer::post_process_component>(scoped);
        REQUIRE(current != nullptr);
        current->overridePostProcess = false;
        current->overrideSsao = false;
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->stats().viewsWithSettingsOverride == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
        app.shutdown();
    }

    TEST_CASE("renderer_subsystem - a steady-state extraction with settings components allocates nothing") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        temp_directory directory("engine_renderer_extract_allocations");
        application app(rendering_project_desc(directory, PROJECT_DEFAULTS, false, true));
        CONJURE_REQUIRE_OK(app.initialise());
        renderer_subsystem *subsystem = renderer_subsystem::find(app.subsystems());
        REQUIRE(subsystem != nullptr);
        for (u32 index = 0; index < 4; ++index) {
            CAPTURE(index);
            const ecs::entity camera = create_camera(app);
            if (index % 2 == 0) {
                renderer::post_process_component overrides = {};
                overrides.overridePostProcess = true;
                CONJURE_REQUIRE_OK(app.context().world->add<renderer::post_process_component>(camera, overrides));
            }
        }
        renderer::render_settings_component settings = {};
        settings.overrideShadows = true;
        CONJURE_UNUSED(create_settings_entity(app, core::uuid(0, 1), settings));
        settings.overrideShadows = false;
        settings.overrideSsao = true;
        CONJURE_UNUSED(create_settings_entity(app, core::uuid(0, 2), settings));
        // The cameras need their render_world_transform, which the frame's transform phase writes.
        CONJURE_REQUIRE_OK(app.step_frame());

        render_extract_runner extraction(app.context(), *subsystem);
        renderer::render_scene extracted = {};
        extraction.run(extracted);
        CHECK(subsystem->stats().views == 4);
        CHECK(subsystem->stats().viewsWithSettingsOverride == 2);
        CHECK(subsystem->stats().settingsCandidates == 2);
        for (u32 frame = 0; frame < 3; ++frame) {
            CAPTURE(frame);
            allocation_scope scope = {};
            extraction.run(extracted);
            CHECK(scope.allocation_count() == 0);
        }
        CHECK(logs.count(core::log_level::WARN) == 0);
        // No app.shutdown() here: the application shuts down when it goes out of scope, after the runner
        // that reads its world.
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
