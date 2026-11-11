#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

// Error codes of the engine module (docs/design/06_engine_loop.md 2.12). Every fallible engine call
// returns a core::result carrying one of these through the "engine" category; errors of the modules the
// engine drives (ecs, assets, scene, ...) are wrapped into the matching startup code with the original
// message quoted, so a log line names both the step and the cause.
//
//   core::result<void> started = app.initialise();
//   if (!started.has_value() && started.get_error().is(engine_error::INVALID_CONFIG)) { ... }

namespace conjure::engine {

    /// <summary>
    /// Failure conditions reported by the engine module.
    /// </summary>
    enum class engine_error : u16 {
        NONE = 0,
        INVALID_CONFIG,                 // engine_config::validate rejected a field; the message names it
        BAD_ARGUMENTS,                  // a command-line flag is unknown or carries a bad value
        PROJECT_NOT_FOUND,              // project.json (or the project directory) does not exist
        PROJECT_PARSE_FAILED,           // project.json is not valid JSON; the message carries line and column
        PROJECT_INVALID,                // project.json lacks a required member or has one of the wrong type
        PROJECT_SCHEMA_TOO_NEW,         // schema_version above PROJECT_SCHEMA_VERSION
        JOBS_START_FAILED,              // core::jobs::job_system::start refused the configuration
        WINDOW_CREATE_FAILED,           // platform::create_window failed (startup step 6)
        DEVICE_CREATE_FAILED,           // rhi::device::create or rhi::swapchain::create failed (step 7)
        ASSET_DATABASE_FAILED,          // the asset database, cache or pack could not be opened or scanned (step 8)
        RENDERER_INIT_FAILED,           // renderer::create or the shader library failed (step 13)
        PHYSICS_INIT_FAILED,            // physics_settings::from_json / validate or attach failed (step 12)
        SCRIPT_RUNTIME_NOT_FOUND,       // no .NET runtime while scripting.required (step 17)
        SCRIPT_ASSEMBLY_LOAD_FAILED,    // the game assembly did not load while scripting.required (step 17)
        SCRIPT_RUNTIME_INIT_FAILED,     // a present runtime failed its handshake or version check (step 17)
        MODULE_LOAD_FAILED,             // a module named in modules.required did not load (step 15)
        SCHEDULER_CONFLICT,             // scheduler::finalise rejected the registered systems (step 18)
        LAYER_ATTACH_FAILED,            // a layer's on_attach failed (step 19)
        STARTUP_SCENE_FAILED,           // the startup scene could not be loaded in a runtime build (step 20)
        ALREADY_INITIALISED,            // initialise() on an application that is not CREATED
        NOT_INITIALISED,                // step_frame() before initialise() or after shutdown()
        DEVICE_LOST,                    // the device was lost during a frame
        EXPORT_FAILED,                  // a step of export_pipeline::run failed; the message names it
        COUNT
    };

    /// <summary>
    /// The stable upper-case enumerator name of a code ("INVALID_CONFIG"); "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view engine_error_name(engine_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::engine::engine_error, "engine", conjure::engine::engine_error_name);

namespace conjure::engine {

    /// <summary>
    /// Builds an error of the engine category with a fixed detail message (possibly empty).
    /// </summary>
    [[nodiscard]] inline core::error make_error(engine_error code, std::string message = {}) {
        return core::error::from(code, std::move(message));
    }
}
