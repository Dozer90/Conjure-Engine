#include "conjure/scene/scene_error.h"

using namespace conjure;
using namespace conjure::scene;

std::string_view conjure::scene::scene_error_name(scene_error code) noexcept {
    switch (code) {
        case scene_error::NONE:
            return "NONE";
        case scene_error::INVALID_ENTITY:
            return "INVALID_ENTITY";
        case scene_error::SELF_PARENT:
            return "SELF_PARENT";
        case scene_error::HIERARCHY_CYCLE:
            return "HIERARCHY_CYCLE";
        case scene_error::HIERARCHY_TOO_DEEP:
            return "HIERARCHY_TOO_DEEP";
        case scene_error::INVALID_SIBLING_INDEX:
            return "INVALID_SIBLING_INDEX";
        case scene_error::NO_TRANSFORM:
            return "NO_TRANSFORM";
        case scene_error::DUPLICATE_GUID:
            return "DUPLICATE_GUID";
        case scene_error::GUID_NOT_FOUND:
            return "GUID_NOT_FOUND";
        case scene_error::BAD_FORMAT:
            return "BAD_FORMAT";
        case scene_error::UNSUPPORTED_VERSION:
            return "UNSUPPORTED_VERSION";
        case scene_error::PREFAB_MULTIPLE_ROOTS:
            return "PREFAB_MULTIPLE_ROOTS";
        case scene_error::PREFAB_NOT_LOADED:
            return "PREFAB_NOT_LOADED";
        case scene_error::NOT_A_PREFAB_INSTANCE:
            return "NOT_A_PREFAB_INSTANCE";
        case scene_error::SCENE_NOT_LOADED:
            return "SCENE_NOT_LOADED";
        case scene_error::LOAD_FAILED:
            return "LOAD_FAILED";
        case scene_error::BUILDER_INVALID:
            return "BUILDER_INVALID";
        case scene_error::COUNT:
            break;
    }
    return "";
}
