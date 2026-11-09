#include "conjure/ecs/ecs_error.h"

#include <string_view>

using namespace conjure;
using namespace conjure::ecs;

std::string_view conjure::ecs::ecs_error_name(ecs_error code) noexcept {
    switch (code) {
        case ecs_error::NONE:
            return "NONE";
        case ecs_error::ENTITY_NOT_ALIVE:
            return "ENTITY_NOT_ALIVE";
        case ecs_error::ENTITY_LIMIT_REACHED:
            return "ENTITY_LIMIT_REACHED";
        case ecs_error::COMPONENT_ALREADY_PRESENT:
            return "COMPONENT_ALREADY_PRESENT";
        case ecs_error::COMPONENT_NOT_PRESENT:
            return "COMPONENT_NOT_PRESENT";
        case ecs_error::UNKNOWN_COMPONENT:
            return "UNKNOWN_COMPONENT";
        case ecs_error::COMPONENT_LIMIT_REACHED:
            return "COMPONENT_LIMIT_REACHED";
        case ecs_error::COMPONENT_IN_USE:
            return "COMPONENT_IN_USE";
        case ecs_error::COMPONENT_TOO_LARGE:
            return "COMPONENT_TOO_LARGE";
        case ecs_error::BAD_ALIGNMENT:
            return "BAD_ALIGNMENT";
        case ecs_error::NAME_CONFLICT:
            return "NAME_CONFLICT";
        case ecs_error::UNSUPPORTED_FIELD:
            return "UNSUPPORTED_FIELD";
        case ecs_error::ARCHETYPE_TOO_LARGE:
            return "ARCHETYPE_TOO_LARGE";
        case ecs_error::STRUCTURE_LOCKED:
            return "STRUCTURE_LOCKED";
        case ecs_error::WRONG_THREAD:
            return "WRONG_THREAD";
        case ecs_error::OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";
        case ecs_error::ARCHIVE_CORRUPT:
            return "ARCHIVE_CORRUPT";
        case ecs_error::SERIALIZATION_FAILED:
            return "SERIALIZATION_FAILED";
        case ecs_error::SYSTEM_CYCLE:
            return "SYSTEM_CYCLE";
        case ecs_error::SYSTEM_NOT_FOUND:
            return "SYSTEM_NOT_FOUND";
        case ecs_error::COUNT:
            break;
    }
    return "";
}
