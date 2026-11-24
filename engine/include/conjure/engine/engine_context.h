#pragma once

#include "conjure/modules/module_service.h"

// The non-owning service table (docs/design/06_engine_loop.md 2.5, R22, R51): what layers, subsystems,
// plugins and bindings reach the engine through. A null pointer means "compiled out or disabled by
// configuration" and every consumer must early-out on it. Pointers are stable from
// application::initialise() to shutdown(). Every engine and module type is forward-declared; the members
// whose type lives in a namespace of the same name are spelled with the conjure:: prefix because the member
// name would otherwise change the meaning of the namespace name inside the struct.
// The struct is a module service (CONJURE_MODULE_SERVICE): a module entry reaches it with
// context.require_service<engine::engine_context>() (R19). That macro is the one include this header pays
// for (conjure/modules/module_service.h, which brings core/containers/flat_map.h and core/hash.h) and the
// reason conjure::modules is a public dependency; the design's "forward declarations only" is otherwise kept.

namespace conjure::core {
    namespace jobs {
        class job_system;
    }
    namespace vfs {
        class file_system;
    }
}

namespace conjure::platform {
    class window;
    class input;
}

namespace conjure::rhi {
    class device;
}

namespace conjure::assets {
    class asset_manager;
    class asset_database;
}

namespace conjure::ecs {
    class component_registry;
    class world;
    class scheduler;
}

namespace conjure::scene {
    class scene_manager;
}

namespace conjure::renderer {
    class renderer;
    class render_defaults;
}

namespace conjure::physics {
    class physics_world;
}

namespace conjure::audio {
    class audio_engine;
}

namespace conjure::scripting {
    class script_runtime;
}

namespace conjure::modules {
    class module_manager;
}

namespace conjure::engine {

    class application;
    struct engine_config;
    class project;
    struct engine_time;
    class frame_memory;
    class frame_stats;

    namespace diagnostics {
        class check_registry;
    }

    struct engine_context {
        CONJURE_MODULE_SERVICE(conjure::engine::engine_context);

        application *app = nullptr;
        const engine_config *config = nullptr;
        const class project *project = nullptr;           // nullptr when no project is loaded
        const engine_time *time = nullptr;
        frame_memory *frameMemory = nullptr;
        core::jobs::job_system *jobs = nullptr;
        core::vfs::file_system *filesystem = nullptr;
        platform::window *window = nullptr;               // null backend when headless, never nullptr after init
        platform::input *input = nullptr;
        rhi::device *device = nullptr;                    // nullptr when rendering.enabled == false
        conjure::assets::asset_manager *assets = nullptr; // always present: cache source (dev) or pack source (cooked)
        conjure::assets::asset_database *assetDatabase = nullptr;    // nullptr in cooked (exported) projects
        ecs::component_registry *components = nullptr;
        ecs::world *world = nullptr;
        ecs::scheduler *scheduler = nullptr;
        scene::scene_manager *scenes = nullptr;
        conjure::renderer::renderer *renderer = nullptr;  // nullptr when rendering disabled or compiled out
        conjure::physics::physics_world *physics = nullptr;
        conjure::audio::audio_engine *audio = nullptr;
        conjure::scripting::script_runtime *scripting = nullptr;    // nullptr when disabled or unavailable (R27)
        conjure::modules::module_manager *modules = nullptr;        // registry() for reads; request_* for the editor
        diagnostics::check_registry *checks = nullptr;    // self-verifying samples (08_testing.md 2.5)
        const frame_stats *stats = nullptr;               // read by the editor profiler and diagnostics_frame_stats
        // Appended after every other member so none of them moves. The live project.json "rendering.defaults",
        // owned by the renderer subsystem: set at its startup, also when rendering is disabled; nullptr when the
        // renderer is compiled out, before that startup, after its shutdown and when a runtime's startup refused
        // invalid defaults. Main thread only.
        conjure::renderer::render_defaults *renderDefaults = nullptr;
    };
}
