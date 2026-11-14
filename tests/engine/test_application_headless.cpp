#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/clock.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/exit_code.h"
#include "conjure/modules/module_manager.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/collision_mesh_asset.h"
    #include "conjure/physics/components.h"
    #include "conjure/physics/physics_world.h"
#endif
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/gltf_importer.h"
    #include "conjure/renderer/model_asset.h"
    #include "conjure/renderer/renderer.h"
#endif

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// The headless application (docs/design/06_engine_loop.md 7, 10 "test_application_headless", D8): null
// backends everywhere, run() with a frame budget, the context pointers, the null-device render path and a
// project booted from a temp directory with its startup scene.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// Writes "scenes/start.scene.json" (two named entities under one root) into an assets directory, using a
    /// stand-alone application as the authoring registry.
    /// </summary>
    void author_start_scene(const std::filesystem::path &assetsDirectory) {
        application authoring(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(authoring.initialise());
        scene::scene &draft = authoring.context().scenes->active();
        draft.set_name("start");
        const core::result<ecs::entity> root = draft.create_entity(core::string_id("root"));
        CONJURE_REQUIRE_OK(root);
        CONJURE_REQUIRE_OK(draft.create_entity(core::string_id("child"), root.value()));
        core::vfs::native_file_system files(assetsDirectory, false);
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene_file(draft, files, "scenes/start.scene.json"));
        authoring.shutdown();
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    /// <summary>
    /// Writes "scenes/start.scene.json" holding one dynamic sphere body of the given radius.
    /// </summary>
    void author_sphere_scene(const std::filesystem::path &assetsDirectory, f32 radius) {
        application authoring(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(authoring.initialise());
        scene::scene &draft = authoring.context().scenes->active();
        draft.set_name("start");
        const core::result<ecs::entity> ball = draft.create_entity(core::string_id("ball"));
        CONJURE_REQUIRE_OK(ball);
        ecs::world &world = *authoring.context().world;
        CONJURE_REQUIRE_OK(world.add<physics::rigid_body>(ball.value(), physics::rigid_body{}));
        physics::sphere_collider collider = {};
        collider.radius = radius;
        CONJURE_REQUIRE_OK(world.add<physics::sphere_collider>(ball.value(), collider));
        core::vfs::native_file_system files(assetsDirectory, false);
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene_file(draft, files, "scenes/start.scene.json"));
        authoring.shutdown();
    }
#endif

    /// <summary>
    /// The descriptor of a project boot: the project's settings applied over the inline configuration.
    /// </summary>
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS && defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    // One triangle with an embedded buffer: three u16 indices, then three POSITION vectors.
    constexpr std::string_view TRIANGLE_GLTF = R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0, "name": "triangle"}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 1}, "indices": 0}]}],
  "buffers": [{
    "uri": "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA=",
    "byteLength": 44
  }],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 6, "target": 34963},
    {"buffer": 0, "byteOffset": 8, "byteLength": 36, "target": 34962}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5123, "count": 3, "type": "SCALAR", "max": [2], "min": [0]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3", "max": [1, 1, 0], "min": [0, 0, 0]}
  ]
})";

    constexpr std::string_view WEDGE_OBJ = "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nf 1 3 2\nf 1 2 4\nf 1 4 3\nf 2 3 4\n";

    void write_text(const std::filesystem::path &path, std::string_view text) {
        std::error_code ignored = {};
        std::filesystem::create_directories(path.parent_path(), ignored);
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << text;
    }
#endif

    [[nodiscard]] application_desc project_desc(project loaded, u64 maxFrames) {
        engine_config config = engine_test::inline_config();
        config.projectPath = loaded.root_directory();
        config.maxFrames = maxFrames;
        loaded.apply_to(config);
        application_desc desc = engine_test::make_desc(config);
        desc.project = std::move(loaded);
        return desc;
    }
}

TEST_SUITE("engine/application_headless") {

    TEST_CASE(
      "application_headless - construction creates no subsystem and initialise / shutdown leave the object reusable"
    ) {
        const core::stopwatch watch = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        // Construction creates no module object (06 3.4). The design's 5 ms budget is only reported here:
        // wall-clock limits belong to the benchmark harness (08 11), not to a unit test on a loaded machine.
        const f64 seconds = watch.elapsed_seconds();
        MESSAGE("application construction took " << seconds * 1000.0 << " ms (design budget 5 ms, not asserted)");
        CHECK(app.subsystems().empty());
        CHECK(app.state() == application_state::CREATED);
        for (u32 cycle = 0; cycle < 2; ++cycle) {
            CONJURE_REQUIRE_OK(app.initialise());
            CONJURE_REQUIRE_OK(app.step_frame());
            app.shutdown();
            CHECK(app.state() == application_state::STOPPED);
        }
    }

    TEST_CASE("application_headless - run with maxFrames 10 returns OK after ten frames") {
        engine_config config = engine_test::inline_config();
        config.maxFrames = 10;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.run() == static_cast<int>(exit_code::OK));
        CHECK(app.time().frameIndex == 10);
        CHECK(app.state() == application_state::STOPPED);
        CHECK(app.stats().total_committed() == 10);
    }

    TEST_CASE("application_headless - ten step_frame calls equal run with ten frames") {
        engine_config config = engine_test::inline_config();
        config.maxFrames = 10;
        application stepped(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(stepped.initialise());
        for (u32 frame = 0; frame < 10; ++frame) {
            CONJURE_REQUIRE_OK(stepped.step_frame());
        }
        const engine_time steppedTime = stepped.time();
        CHECK(stepped.is_quit_requested());
        stepped.shutdown();

        application ran(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(ran.initialise());
        CHECK(ran.run() == 0);
        CHECK(ran.time() == steppedTime);
    }

    TEST_CASE("application_headless - a close request from the null window ends run with OK") {
        engine_config config = engine_test::inline_config();
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        platform::null_backend *window = app.context().window->as_null();
        REQUIRE(window != nullptr);
        window->simulate_close();
        CHECK(app.run() == static_cast<int>(exit_code::OK));
        CHECK(app.time().frameIndex == 1);
    }

    TEST_CASE("application_headless - context pointers are null for disabled modules and stable across frames") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        const engine_context &context = app.context();
        CHECK(context.window != nullptr);
        CHECK(context.input != nullptr);
        CHECK(context.device == nullptr);
        CHECK(context.renderer == nullptr);
        CHECK(context.scripting == nullptr);
        CHECK(context.assets != nullptr);
        CHECK(context.assetDatabase != nullptr);
        CHECK(context.components != nullptr);
        CHECK(context.world != nullptr);
        CHECK(context.scheduler != nullptr);
        CHECK(context.scenes != nullptr);
        CHECK(context.modules != nullptr);
        CHECK(context.jobs != nullptr);
        CHECK(context.frameMemory != nullptr);
        CHECK(context.filesystem != nullptr);
        CHECK(context.checks != nullptr);
        CHECK(context.stats == &app.stats());
        CHECK(context.project == nullptr);
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        CHECK(context.audio != nullptr);
#else
        CHECK(context.audio == nullptr);
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        CHECK(context.physics != nullptr);
#else
        CHECK(context.physics == nullptr);
#endif
        const engine_context snapshot = context;
        for (u32 frame = 0; frame < 5; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(context.world == snapshot.world);
            CHECK(context.assets == snapshot.assets);
            CHECK(context.scenes == snapshot.scenes);
            CHECK(context.modules == snapshot.modules);
            CHECK(context.audio == snapshot.audio);
            CHECK(context.physics == snapshot.physics);
        }
        app.shutdown();
        CHECK(app.context().world == nullptr);
        CHECK(app.context().jobs == nullptr);
        CHECK(app.context().app == &app);
    }

    TEST_CASE("application_headless - disabling audio and physics leaves their pointers null") {
        engine_config config = engine_test::inline_config();
        config.audio.enabled = false;
        config.physics.enabled = false;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.context().audio == nullptr);
        CHECK(app.context().physics == nullptr);
        CONJURE_REQUIRE_OK(app.step_frame());
        app.shutdown();
    }

    TEST_CASE("application_headless - the virtual file system mounts project and engine schemes") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        core::vfs::file_system &files = *app.context().filesystem;
        CHECK(files.exists("project://assets"));
        CHECK_FALSE(files.exists("cooked://content.cpak"));
        CONJURE_CHECK_OK(files.write_text_file("project://scratch.txt", "hello"));
        const core::result<std::string> text = files.read_text_file("project://scratch.txt");
        CONJURE_REQUIRE_OK(text);
        CHECK(text.value() == "hello");
        app.shutdown();
    }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    TEST_CASE("application_headless - rendering on the null device runs extraction and render_frame") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().renderer != nullptr);
        REQUIRE(app.context().device != nullptr);
        for (u32 frame = 0; frame < 3; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
        }
        CHECK(app.context().renderer->frame_count() == 3);
        CHECK(app.stats().latest().submitMilliseconds >= 0.0f);
        app.shutdown();
        CHECK(app.context().renderer == nullptr);
        CHECK(app.context().device == nullptr);
    }
#endif

    TEST_CASE("application_headless - a project boots, scans its assets and loads the startup scene") {
        log_capture logs = {};
        temp_directory directory("engine_project_boot");
        project loaded = engine_test::write_project(directory.path(), "boot", "scenes/start.scene.json");
        author_start_scene(loaded.assets_directory());

        application app(project_desc(std::move(loaded), 5));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().project != nullptr);
        CHECK(app.context().project->description().name == "boot");
        CHECK(app.context().assets->find("assets://scenes/start.scene.json").has_value());
        CHECK(app.context().scenes->loaded_scenes().size() == 1);
        CHECK(app.context().world->entity_count() == 2);
        CHECK(app.context().filesystem->exists("assets://scenes/start.scene.json"));
        CHECK(app.run() == 0);
        CHECK(app.time().frameIndex == 5);
    }

    TEST_CASE("regression - review finding 3: an absolute assets_root indexes the assets directory, not the project") {
        log_capture logs = {};
        temp_directory directory("engine_project_absolute_assets");
        const std::filesystem::path shared = directory.path() / "shared_assets";
        project loaded = engine_test::write_project(
          directory.path() / "game",
          "absolute",
          "scenes/start.scene.json",
          {},
          shared
        );
        REQUIRE(loaded.description().assetsRoot.is_absolute());
        REQUIRE(loaded.assets_directory() == shared.lexically_normal());
        author_start_scene(loaded.assets_directory());

        application app(project_desc(std::move(loaded), 3));
        CONJURE_REQUIRE_OK(app.initialise());
        // The database and the assets:// mount agree: the scene is an asset, the project file is not.
        CHECK(app.context().assets->find("assets://scenes/start.scene.json").has_value());
        CHECK_FALSE(app.context().assets->find("assets://project.json").has_value());
        CHECK(app.context().filesystem->exists("assets://scenes/start.scene.json"));
        CHECK_FALSE(app.context().filesystem->exists("assets://project.json"));
        CHECK(app.context().scenes->loaded_scenes().size() == 1);
        CHECK(app.context().world->entity_count() == 2);
        CHECK(app.run() == 0);
    }

    TEST_CASE("regression - review finding 12: a scheme-qualified --scene path is used as given") {
        log_capture logs = {};
        temp_directory directory("engine_project_qualified_scene");
        project loaded = engine_test::write_project(directory.path(), "qualified");
        author_start_scene(loaded.assets_directory());

        application_desc desc = project_desc(std::move(loaded), 2);
        desc.config.startupSceneOverride = "assets://scenes/start.scene.json";
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.context().scenes->loaded_scenes().size() == 1);
        CHECK(app.context().world->entity_count() == 2);
        CHECK(app.run() == 0);
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS && defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    TEST_CASE("regression - code sweep F137 a fresh .gltf imports as a render model when physics is registered too") {
        // The whole engine, so both modules register their importers in the engine's own order: physics
        // first, which used to give it every glTF.
        log_capture logs = {};
        temp_directory directory("engine_gltf_importer");
        project loaded = engine_test::write_project(directory.path(), "models", "scenes/start.scene.json");
        author_start_scene(loaded.assets_directory());
        write_text(loaded.assets_directory() / "models" / "triangle.gltf", TRIANGLE_GLTF);
        write_text(loaded.assets_directory() / "collision" / "wedge.obj", WEDGE_OBJ);

        application app(project_desc(std::move(loaded), 2));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().assetDatabase != nullptr);
        const assets::asset_database &database = *app.context().assetDatabase;

        const std::optional<assets::asset_id> modelId = database.find_by_path("assets://models/triangle.gltf");
        REQUIRE(modelId.has_value());
        const std::optional<assets::asset_record> model = database.find(*modelId);
        REQUIRE(model.has_value());
        CHECK(model->importerId == renderer::GLTF_IMPORTER_ID);
        CHECK(model->type == assets::asset_type_traits<renderer::model_asset>::id());
        CHECK(model->status == assets::asset_record_status::OK);
        // The .meta written at the first scan is what pins the importer for good.
        const core::result<assets::asset_meta> meta = database.read_meta(*modelId);
        CONJURE_REQUIRE_OK(meta);
        CHECK(meta.value().importerId == renderer::GLTF_IMPORTER_ID);

        // The format only the collision importer claims still goes to it.
        const std::optional<assets::asset_id> wedgeId = database.find_by_path("assets://collision/wedge.obj");
        REQUIRE(wedgeId.has_value());
        const std::optional<assets::asset_record> wedge = database.find(*wedgeId);
        REQUIRE(wedge.has_value());
        CHECK(wedge->importerId == physics::COLLISION_MESH_IMPORTER_ID);
        CHECK(wedge->type == assets::asset_type_traits<physics::collision_mesh_asset>::id());
        CHECK(wedge->status == assets::asset_record_status::OK);

        // No two engine importers claim one extension: the registry would have said so.
        CHECK_FALSE(logs.contains(core::log_level::WARN, "already imports"));
        app.shutdown();
    }
#endif

    TEST_CASE("application_headless - a missing startup scene is fatal for the runtime and a warning for the editor") {
        log_capture logs = {};
        logs.allow_errors();
        temp_directory directory("engine_project_noscene");
        project loaded = engine_test::write_project(directory.path(), "noscene", "scenes/missing.scene.json");

        engine_config runtime = engine_test::inline_config();
        runtime.projectPath = loaded.root_directory();
        loaded.apply_to(runtime);
        application_desc desc = engine_test::make_desc(runtime);
        desc.project = loaded;
        application app(std::move(desc));
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::STARTUP_SCENE_FAILED);
        CHECK(app.state() == application_state::CREATED);

        engine_config editor = runtime;
        editor.editor = true;
        application_desc editorDesc = engine_test::make_desc(editor);
        editorDesc.project = loaded;
        application editorApp(std::move(editorDesc));
        CONJURE_REQUIRE_OK(editorApp.initialise());
        CHECK(logs.contains(core::log_level::WARN, "missing.scene.json"));
        editorApp.shutdown();
    }

    TEST_CASE(
      "application_headless - a startup scene that no longer imports is fatal for the runtime and kept for the editor"
    ) {
        log_capture logs = {};
        logs.allow_errors();
        temp_directory directory("engine_project_stale_scene");
        project loaded = engine_test::write_project(directory.path(), "stale", "scenes/start.scene.json");
        author_start_scene(loaded.assets_directory());
        {
            // The first boot imports the scene and fills the cache.
            application first(project_desc(loaded, 1));
            CONJURE_REQUIRE_OK(first.initialise());
            CHECK(first.context().scenes->loaded_scenes().size() == 1);
            CHECK(first.run() == 0);
        }
        CONJURE_REQUIRE_OK(directory.write_text("assets/scenes/start.scene.json", R"({"version": 999})"));

        application runtime(project_desc(loaded, 1));
        CONJURE_CHECK_ERROR(runtime.initialise(), engine_error::STARTUP_SCENE_FAILED);
        CHECK(runtime.state() == application_state::CREATED);

        application_desc editorDesc = project_desc(loaded, 1);
        editorDesc.config.editor = true;
        application editor(std::move(editorDesc));
        CONJURE_REQUIRE_OK(editor.initialise());
        CHECK(editor.context().scenes->loaded_scenes().size() == 1);
        editor.shutdown();
    }

    TEST_CASE("application_headless - an editor project boot shadow-copies modules into .conjure/module_cache") {
        log_capture logs = {};
        temp_directory directory("engine_project_shadow_copy");
        project loaded = engine_test::write_project(directory.path(), "shadow");

        application_desc editorDesc = project_desc(loaded, 0);
        editorDesc.config.editor = true;
        application editor(std::move(editorDesc));
        CONJURE_REQUIRE_OK(editor.initialise());
        REQUIRE(editor.context().modules != nullptr);
        const modules::module_manager_desc &shadowed = editor.context().modules->desc();
        CHECK(shadowed.shadowCopyBinaries);
        CHECK(shadowed.shadowCopyDirectory == loaded.root_directory() / ".conjure" / "module_cache");
        editor.shutdown();

        // The runtime loads module binaries in place (07 D9).
        application runtime(project_desc(loaded, 0));
        CONJURE_REQUIRE_OK(runtime.initialise());
        REQUIRE(runtime.context().modules != nullptr);
        CHECK_FALSE(runtime.context().modules->desc().shadowCopyBinaries);
        runtime.shutdown();
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("application_headless - unloading a scene collects the physics shapes its bodies used") {
        log_capture logs = {};
        temp_directory directory("engine_project_physics_unload");
        project loaded = engine_test::write_project(directory.path(), "shapes", "scenes/start.scene.json");
        author_sphere_scene(loaded.assets_directory(), 0.37f);

        application app(project_desc(std::move(loaded), 0));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().physics != nullptr);
        for (u32 frame = 0; frame < 4 && app.context().physics->stats().shapeCacheEntries == 0; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
        }
        REQUIRE(app.context().physics->stats().shapeCacheEntries >= 1);

        const std::vector<scene::loaded_scene_info> scenes = app.context().scenes->loaded_scenes();
        REQUIRE(scenes.size() == 1);
        CONJURE_REQUIRE_OK(app.context().scenes->unload(scenes[0].id));
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.context().physics->stats().bodies == 0);
        CHECK(app.context().physics->stats().shapeCacheEntries == 0);
        app.shutdown();
    }
#endif
}
