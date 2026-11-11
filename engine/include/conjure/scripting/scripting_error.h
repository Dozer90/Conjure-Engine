#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Error codes of the scripting module (docs/design/03_scripting.md 4, 6). Built with the generic
// core::make_error(script_error::X, detail) and bound to the "scripting" category below (R63). The codes
// start at 1 so the negated value the native table entries return (-code, native_api.inl) is never 0.

namespace conjure::scripting {

    enum class script_error : u16 {
        RUNTIME_NOT_FOUND = 1,      // hostfxr could not be located: no .NET runtime installed
        HOSTFXR_INCOMPATIBLE,       // hostfxr loaded but lacks a required export (older than .NET 8)
        RUNTIME_CONFIG_INVALID,     // Conjure.Bootstrap.runtimeconfig.json missing or malformed
        MANAGED_SDK_MISSING,        // Conjure.Managed.dll / Conjure.Bootstrap.dll not in managed_directory
        HOST_ALREADY_RUNNING,       // a runtime of another mode already lives in this process
        ABI_MISMATCH,               // size, version or layout hash of a table differs between the sides
        ASSEMBLY_LOAD_FAILED,       // the game assembly did not load; the previous one keeps running
        ASSEMBLY_LEAKED,            // the collectible load context survived unload (zombie)
        CLASS_NOT_FOUND,            // no script class with this hash in the active registry
        FIELD_NOT_FOUND,            // the class has no field with this hash
        FIELD_BLOB_INVALID,         // a field blob failed validation (magic, version, bounds)
        INSTANCE_NOT_FOUND,         // the entry or slot does not resolve to a live instance
        WRONG_THREAD,               // called off the main thread
        NOT_IN_PHASE,               // a query scope was requested outside a script phase
        BUILD_FAILED,               // dotnet build reported errors
        NOT_INITIALIZED,            // the host or runtime is not READY
        NOT_SUPPORTED,              // the entry or feature is not available in this configuration
        COUNT
    };

    /// <summary>
    /// The enumerator name of a code ("ABI_MISMATCH"); "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view script_error_name(script_error code) noexcept;

    /// <summary>
    /// The negative status a native table entry returns for this error (-code).
    /// </summary>
    [[nodiscard]] constexpr i32 status_of(script_error code) noexcept {
        return -static_cast<i32>(code);
    }
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::scripting::script_error, "scripting", conjure::scripting::script_error_name);
