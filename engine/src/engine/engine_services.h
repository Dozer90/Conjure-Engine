#pragma once

#include "conjure/engine/diagnostics/check_registry.h"
#include "conjure/engine/frame_memory.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_pack.h"
#include "conjure/assets/file_watcher.h"
#include "conjure/assets/hot_reload.h"
#include "conjure/core/event_bus.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/core/vfs/mount_table.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/world.h"
#include "conjure/modules/module_manager.h"
#include "conjure/platform/input.h"
#include "conjure/platform/window.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/swapchain.h"
#include "conjure/scene/scene_manager.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/debug_draw.h"
    #include "conjure/renderer/renderer.h"
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_world.h"
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #include "conjure/audio/audio_engine.h"
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/hot_reload.h"
    #include "conjure/scripting/script_runtime.h"
#endif

#include <filesystem>
#include <memory>
#include <vector>

// Every module object the application owns (docs/design/06_engine_loop.md 5, ARCHITECTURE.md 2.3):
// created by the subsystem adapters in startup order and destroyed by their shutdown in reverse. Nothing
// else owns a module object; engine_context holds raw pointers into this struct. Members are declared in
// the order the design starts them so an owner that forgot shutdown() still destroys them in reverse.

namespace conjure::engine {

    class ecs_module_consumer;
    class assets_module_consumer;
    class scripting_module_consumer;

    struct engine_services {
        /*---------------------------------------------------------------*/
        // Core (application::start_core)

        std::filesystem::path engineDirectory = {};            // the executable's directory
        std::shared_ptr<core::log_sink> logFileSink = nullptr;
        core::log_level previousGlobalLevel = core::log_level::TRACE;
        core::jobs::job_system jobs = {};
        std::unique_ptr<frame_memory> frameMemory = nullptr;
        core::event_bus events = {};
        diagnostics::check_registry checks = {};
        std::shared_ptr<core::vfs::file_system> projectFileSystem = nullptr;   // the project root (or a memory tree)
        std::shared_ptr<core::vfs::file_system> assetsFileSystem = nullptr;    // the assets root (development project)
        std::shared_ptr<core::vfs::file_system> engineFileSystem = nullptr;    // <executable>/engine (or a memory tree)
        std::shared_ptr<core::vfs::file_system> cookedFileSystem = nullptr;    // the cooked root of an exported project
        core::vfs::mount_table mounts = {};
        bool cooked = false;

        /*---------------------------------------------------------------*/
        // Platform (step 6)

        std::unique_ptr<platform::window> window = nullptr;
        std::unique_ptr<platform::input> input = nullptr;

        /*---------------------------------------------------------------*/
        // Rhi (step 7)

        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<rhi::swapchain> swapchain = nullptr;
        std::unique_ptr<rhi::shader_library> shaders = nullptr;

        /*---------------------------------------------------------------*/
        // Assets (step 8)

        assets::importer_registry importers = {};
        std::unique_ptr<assets::asset_manager> assetManager = nullptr;
        std::unique_ptr<assets::asset_database> assetDatabase = nullptr;
        std::unique_ptr<assets::pack_reader> packReader = nullptr;
        std::unique_ptr<assets::polling_file_watcher> fileWatcher = nullptr;
        std::unique_ptr<assets::hot_reload_controller> hotReload = nullptr;

        /*---------------------------------------------------------------*/
        // Ecs and scene (steps 9, 10)

        std::unique_ptr<ecs::component_registry> components = nullptr;
        std::unique_ptr<ecs::world> world = nullptr;
        std::unique_ptr<ecs::scheduler> scheduler = nullptr;
        std::unique_ptr<scene::scene_manager> scenes = nullptr;

        /*---------------------------------------------------------------*/
        // Optional modules (steps 11-14)

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        std::unique_ptr<audio::audio_engine> audio = nullptr;
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        std::unique_ptr<physics::physics_world> physics = nullptr;
#endif
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        std::unique_ptr<renderer::renderer> renderer = nullptr;
        std::unique_ptr<renderer::debug_draw> debugDraw = nullptr;
        renderer::render_scene *extractTarget = nullptr;       // the write slot between begin_extract and end_extract
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        std::unique_ptr<scripting::script_runtime> scripting = nullptr;
        std::unique_ptr<scripting::hot_reload_controller> scriptReload = nullptr;
#endif

        /*---------------------------------------------------------------*/
        // Modules (step 15)

        std::unique_ptr<ecs_module_consumer> ecsConsumer = nullptr;
        std::unique_ptr<assets_module_consumer> assetsConsumer = nullptr;
        std::unique_ptr<scripting_module_consumer> scriptingConsumer = nullptr;
        std::unique_ptr<modules::module_manager> modules = nullptr;

        engine_services();
        ~engine_services();

        engine_services(const engine_services &) = delete;
        engine_services &operator=(const engine_services &) = delete;
    };
}
