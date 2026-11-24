#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Error codes of the ecs module (docs/design/01_ecs.md 4.1). Every fallible ecs call returns a
// core::result carrying one of these through the "ecs" error category, built with the generic
// core::make_error (R63):
//
//   return core::make_error(ecs_error::ENTITY_NOT_ALIVE, "entity was destroyed");
//
//   core::result<entity> created = world.create_entity();
//   if (!created.has_value() && created.get_error().is(ecs_error::STRUCTURE_LOCKED)) { ... }

namespace conjure::ecs {

    /// <summary>
    /// Failure conditions reported by the ecs module. Contract violations (a structural call while the
    /// world is locked, a call from the wrong thread) assert in Debug and Development and still return
    /// their code so Distribution builds stay defined.
    /// </summary>
    enum class ecs_error : u16 {
        NONE = 0,
        ENTITY_NOT_ALIVE,
        ENTITY_LIMIT_REACHED,
        COMPONENT_ALREADY_PRESENT,
        COMPONENT_NOT_PRESENT,
        UNKNOWN_COMPONENT,
        COMPONENT_LIMIT_REACHED,
        COMPONENT_IN_USE,
        COMPONENT_TOO_LARGE,
        BAD_ALIGNMENT,
        NAME_CONFLICT,
        UNSUPPORTED_FIELD,
        ARCHETYPE_TOO_LARGE,
        STRUCTURE_LOCKED,
        WRONG_THREAD,
        OUT_OF_MEMORY,
        ARCHIVE_CORRUPT,
        SERIALIZATION_FAILED,
        SYSTEM_CYCLE,
        SYSTEM_NOT_FOUND,
        COUNT
    };

    /// <summary>
    /// The stable upper-case enumerator name of a code ("ENTITY_NOT_ALIVE"); "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view ecs_error_name(ecs_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::ecs::ecs_error, "ecs", conjure::ecs::ecs_error_name);
