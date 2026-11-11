#pragma once

#include "conjure/engine/engine_context.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"

#include <string_view>

// Subsystem adapters (docs/design/06_engine_loop.md 2.9, D7): the one interface the engine uses to start,
// register, step and stop every module object, so the startup unwind is uniform. The built-in adapters
// (platform, rhi, assets, ecs, scene, audio, physics, renderer, scripting, modules) live in
// engine/src/engine/subsystems; executables and plugins add their own through application::add_subsystem
// before initialise() (the bindings subsystem is the first user).

namespace conjure::ecs {
    class scheduler;
}

namespace conjure::engine {

    class CONJURE_API engine_subsystem {
        public:
            engine_subsystem();
            virtual ~engine_subsystem();

            engine_subsystem(const engine_subsystem &) = delete;
            engine_subsystem &operator=(const engine_subsystem &) = delete;

        public:
            /// <summary>
            /// The name used in startup and shutdown logs ("platform", "renderer", ...).
            /// </summary>
            [[nodiscard]] virtual std::string_view name() const noexcept = 0;

            /// <summary>
            /// Startup step of this subsystem (3.4). Creates the module object, publishes its pointer in the
            /// context. On failure the already-started subsystems are shut down in reverse by the application.
            /// </summary>
            [[nodiscard]] virtual core::result<void> startup(engine_context &context) = 0;

            /// <summary>
            /// Runs after every subsystem's startup (step 17): the assets scan, the scripting handshake.
            /// </summary>
            [[nodiscard]] virtual core::result<void> post_startup(engine_context &context);

            /// <summary>
            /// Adds this subsystem's systems to the scheduler (step 18); the application finalises afterwards.
            /// </summary>
            virtual void register_systems(ecs::scheduler &scheduler, engine_context &context);

            /// <summary>
            /// Main thread, frame step 5 (before FRAME_BEGIN).
            /// </summary>
            virtual void begin_frame(engine_context &context);

            /// <summary>
            /// Main thread, frame step 13 (after FRAME_END).
            /// </summary>
            virtual void end_frame(engine_context &context);

            /// <summary>
            /// Destroys the module object and clears its context pointer. Must be safe after a failed or
            /// never attempted startup and when called twice.
            /// </summary>
            virtual void shutdown(engine_context &context) = 0;
    };
}
