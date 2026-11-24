#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Failure codes of the module system (docs/design/07_modules.md section 7). Every code is returned
// as a core::error built with the generic core::make_error(module_error::X, ...) (R63), whose message
// names the module and, where relevant, the expected and actual values, so the editor can show it
// verbatim.

namespace conjure::modules {

    enum class module_error : u16 {
        NONE = 0,
        MANIFEST_NOT_FOUND,         // module.json (or the discover directory) does not exist
        MANIFEST_INVALID,           // JSON or schema error; the message carries file and JSON pointer
        INVALID_NAME,               // name outside [a-z0-9_.]{1,64}
        DUPLICATE_NAME,             // a module of that name is already registered
        PLATFORM_UNSUPPORTED,       // the manifest excludes the host platform (state SKIPPED)
        ENGINE_VERSION_MISMATCH,    // manifest "engine" range does not contain the host version
        FEATURE_UNAVAILABLE,        // manifest "requires" names a feature the host was built without
        DEPENDENCY_MISSING,         // a non-optional dependency is not registered (or SKIPPED)
        DEPENDENCY_VERSION,         // the dependency exists but its version is outside the range
        DEPENDENCY_CYCLE,           // the message lists the cycle
        DEPENDENCY_FAILED,          // a dependency failed; the message names the root cause
        MULTIPLE_GAME_MODULES,      // only one module of kind GAME may exist
        SHARED_LIBRARIES_DISABLED,  // module_manager_desc::allowSharedLibraries is false
        BINARY_NOT_FOUND,           // the derived bin/<triplet>/<binary> path does not exist
        BINARY_COPY_FAILED,         // the shadow copy could not be written
        BINARY_LOAD_FAILED,         // the operating system refused to load the binary (OS text)
        ENTRY_SYMBOL_MISSING,       // conjure_module_describe / conjure_module_entry not exported
        ABI_MISMATCH,               // a signature field differs (field, expected, actual)
        NAME_MISMATCH,              // the binary's name differs from the manifest name
        ENTRY_FAILED,               // the entry returned FAILED or called fail(); all reasons listed
        REGISTRATION_CONFLICT,      // a consumer vetoed the registrations (consumer + offending key)
        SERVICE_MISSING,            // module_context::require_service found nothing
        HAS_DEPENDENTS,             // unload refused: loaded modules depend on this one
        IN_USE,                     // unload refused under REFUSE_IF_IN_USE (count in the message)
        UNLOAD_BLOCKED,             // a consumer vetoed the unload (play mode)
        NOT_LOADED,                 // the operation needs a LOADED module
        ALREADY_LOADED,             // load of a LOADED module
        INVALID_ID,                 // the id never referred to a module
        STALE_ID,                   // the id's generation is behind the record
        FAULTED,                    // the record is FAULTED and accepts no further operation
        EXPORT_FAILED,              // export_layout could not produce the layout (see export_layout)
        COUNT
    };

    /// <summary>
    /// The enumerator name of a code ("ABI_MISMATCH"), "" for values outside the enumeration.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view module_error_name(module_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::modules::module_error, "modules", conjure::modules::module_error_name);
