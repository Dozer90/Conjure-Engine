#pragma once

#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Error codes of the physics module (docs/design/14_physics.md 4.1). Every fallible physics call returns a
// core::result carrying one of these through the "physics" category; errors are built with the generic
// core::make_error(physics_error::X, "...") (R63).
//
//   core::result<void> pushed = world.add_force(target, force);
//   if (!pushed.has_value() && pushed.get_error().is(physics_error::NO_BODY)) { ... }

namespace conjure::physics {

    /// <summary>
    /// Failure conditions reported by the physics module.
    /// </summary>
    enum class physics_error : u16 {
        NONE = 0,
        INVALID_SETTINGS,               // physics_settings::validate rejected a field (the message names it)
        LAYER_CONFIG_INVALID,           // extensions.physics_layers has no usable "names" array
        LAYER_LIMIT_REACHED,            // more than MAX_PHYSICS_LAYERS layers
        DUPLICATE_LAYER,                // a layer name appears twice
        UNKNOWN_LAYER,                  // an "ignore" pair or a body names a layer that does not exist
        BODY_LIMIT_REACHED,             // Jolt refused to create a body (maxBodies)
        NO_BODY,                        // the entity has no body (and is not pending)
        NOT_A_CHARACTER,                // the entity's body is not a character controller
        NOT_ATTACHED,                   // attach() was not called yet
        ALREADY_ATTACHED,               // attach() called twice
        SHAPE_CREATE_FAILED,            // a Jolt ShapeSettings::Create() reported an error
        INVALID_SHAPE_SCALE,            // the entity's world scale cannot be applied to its shape
        UNSUPPORTED_SHAPE_FOR_MOTION,   // mesh_collider on a DYNAMIC body
        MIXED_TRIGGER_FLAGS,            // colliders of one body disagree on isTrigger
        CONFLICTING_BODY_COMPONENTS,    // rigid_body and character_controller on one entity
        GEOMETRY_INVALID,               // collision mesh indices out of range or no triangles
        COLLISION_MESH_PARSE_FAILED,    // glTF / OBJ source could not be parsed
        STEP_IN_PROGRESS,               // a query or body call arrived while step() runs
        WRONG_THREAD,                   // an owner-thread call from another thread
        COMMAND_QUEUE_FULL,             // maxQueuedCommands reached; the command was dropped
        COUNT
    };

    /// <summary>
    /// The stable upper-case enumerator name of a code ("NO_BODY"); "" for unknown values.
    /// </summary>
    [[nodiscard]] std::string_view physics_error_name(physics_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::physics::physics_error, "physics", conjure::physics::physics_error_name);
