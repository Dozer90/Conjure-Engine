#include "conjure/engine/engine_error.h"

using namespace conjure;
using namespace conjure::engine;

std::string_view conjure::engine::engine_error_name(engine_error code) noexcept {
    switch (code) {
        case engine_error::NONE:
            return "NONE";
        case engine_error::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case engine_error::BAD_ARGUMENTS:
            return "BAD_ARGUMENTS";
        case engine_error::PROJECT_NOT_FOUND:
            return "PROJECT_NOT_FOUND";
        case engine_error::PROJECT_PARSE_FAILED:
            return "PROJECT_PARSE_FAILED";
        case engine_error::PROJECT_INVALID:
            return "PROJECT_INVALID";
        case engine_error::PROJECT_SCHEMA_TOO_NEW:
            return "PROJECT_SCHEMA_TOO_NEW";
        case engine_error::JOBS_START_FAILED:
            return "JOBS_START_FAILED";
        case engine_error::WINDOW_CREATE_FAILED:
            return "WINDOW_CREATE_FAILED";
        case engine_error::DEVICE_CREATE_FAILED:
            return "DEVICE_CREATE_FAILED";
        case engine_error::ASSET_DATABASE_FAILED:
            return "ASSET_DATABASE_FAILED";
        case engine_error::RENDERER_INIT_FAILED:
            return "RENDERER_INIT_FAILED";
        case engine_error::PHYSICS_INIT_FAILED:
            return "PHYSICS_INIT_FAILED";
        case engine_error::SCRIPT_RUNTIME_NOT_FOUND:
            return "SCRIPT_RUNTIME_NOT_FOUND";
        case engine_error::SCRIPT_ASSEMBLY_LOAD_FAILED:
            return "SCRIPT_ASSEMBLY_LOAD_FAILED";
        case engine_error::SCRIPT_RUNTIME_INIT_FAILED:
            return "SCRIPT_RUNTIME_INIT_FAILED";
        case engine_error::MODULE_LOAD_FAILED:
            return "MODULE_LOAD_FAILED";
        case engine_error::SCHEDULER_CONFLICT:
            return "SCHEDULER_CONFLICT";
        case engine_error::LAYER_ATTACH_FAILED:
            return "LAYER_ATTACH_FAILED";
        case engine_error::STARTUP_SCENE_FAILED:
            return "STARTUP_SCENE_FAILED";
        case engine_error::ALREADY_INITIALISED:
            return "ALREADY_INITIALISED";
        case engine_error::NOT_INITIALISED:
            return "NOT_INITIALISED";
        case engine_error::DEVICE_LOST:
            return "DEVICE_LOST";
        case engine_error::EXPORT_FAILED:
            return "EXPORT_FAILED";
        case engine_error::COUNT:
            break;
    }
    return "";
}
