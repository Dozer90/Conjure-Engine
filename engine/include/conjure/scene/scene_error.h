#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Error codes of the scene module (docs/design/13_scene.md 4.1). Every fallible scene call returns a
// core::result carrying one of these through the "scene" category; ECS and asset errors propagate
// unchanged (ENTITY_NOT_ALIVE, ARCHIVE_CORRUPT, SERIALIZATION_FAILED, NOT_FOUND, ...).
//
//   core::result<void> moved = scene.set_parent(child, parent);
//   if (!moved.has_value() && moved.get_error().is(scene_error::HIERARCHY_CYCLE)) { ... }

namespace conjure::scene {

    /// <summary>
    /// Failure conditions reported by the scene module.
    /// </summary>
    enum class scene_error : u16 {
        NONE = 0,
        INVALID_ENTITY,           // dead, null or foreign entity handed to a mutator
        SELF_PARENT,              // set_parent(e, e)
        HIERARCHY_CYCLE,          // the new parent is a descendant of the child
        HIERARCHY_TOO_DEEP,       // the subtree would reach depth MAX_HIERARCHY_DEPTH
        INVALID_SIBLING_INDEX,    // the target has no sibling list (dead parent)
        NO_TRANSFORM,             // the entity has no local_transform
        DUPLICATE_GUID,           // a guid is already mapped to another entity
        GUID_NOT_FOUND,           // no entity carries the guid
        BAD_FORMAT,               // header "format" missing or wrong, or an entity without components
        UNSUPPORTED_VERSION,      // header "version" newer than SCENE_FORMAT_VERSION
        PREFAB_MULTIPLE_ROOTS,    // a prefab archive has several entries without a parent
        PREFAB_NOT_LOADED,        // the prefab asset of an instance is not resident
        NOT_A_PREFAB_INSTANCE,    // the entity carries no prefab_instance
        SCENE_NOT_LOADED,         // the scene_id names no loaded scene
        LOAD_FAILED,              // the scene asset FAILED while a request waited
        BUILDER_INVALID,          // prefab_builder without a single root, or an unknown node
        COUNT
    };

    /// <summary>
    /// The stable upper-case enumerator name of a code ("HIERARCHY_CYCLE"); "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view scene_error_name(scene_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::scene::scene_error, "scene", conjure::scene::scene_error_name);
