#include "conjure/physics/physics_error.h"

namespace conjure::physics {

std::string_view physics_error_name(physics_error code) noexcept {
    switch (code) {
        case physics_error::NONE:
            return "NONE";
        case physics_error::INVALID_SETTINGS:
            return "INVALID_SETTINGS";
        case physics_error::LAYER_CONFIG_INVALID:
            return "LAYER_CONFIG_INVALID";
        case physics_error::LAYER_LIMIT_REACHED:
            return "LAYER_LIMIT_REACHED";
        case physics_error::DUPLICATE_LAYER:
            return "DUPLICATE_LAYER";
        case physics_error::UNKNOWN_LAYER:
            return "UNKNOWN_LAYER";
        case physics_error::BODY_LIMIT_REACHED:
            return "BODY_LIMIT_REACHED";
        case physics_error::NO_BODY:
            return "NO_BODY";
        case physics_error::NOT_A_CHARACTER:
            return "NOT_A_CHARACTER";
        case physics_error::NOT_ATTACHED:
            return "NOT_ATTACHED";
        case physics_error::ALREADY_ATTACHED:
            return "ALREADY_ATTACHED";
        case physics_error::SHAPE_CREATE_FAILED:
            return "SHAPE_CREATE_FAILED";
        case physics_error::INVALID_SHAPE_SCALE:
            return "INVALID_SHAPE_SCALE";
        case physics_error::UNSUPPORTED_SHAPE_FOR_MOTION:
            return "UNSUPPORTED_SHAPE_FOR_MOTION";
        case physics_error::MIXED_TRIGGER_FLAGS:
            return "MIXED_TRIGGER_FLAGS";
        case physics_error::CONFLICTING_BODY_COMPONENTS:
            return "CONFLICTING_BODY_COMPONENTS";
        case physics_error::GEOMETRY_INVALID:
            return "GEOMETRY_INVALID";
        case physics_error::COLLISION_MESH_PARSE_FAILED:
            return "COLLISION_MESH_PARSE_FAILED";
        case physics_error::STEP_IN_PROGRESS:
            return "STEP_IN_PROGRESS";
        case physics_error::WRONG_THREAD:
            return "WRONG_THREAD";
        case physics_error::COMMAND_QUEUE_FULL:
            return "COMMAND_QUEUE_FULL";
        case physics_error::WRONG_BODY_KIND:
            return "WRONG_BODY_KIND";
        case physics_error::COUNT:
            break;
    }
    return "";
}
}
