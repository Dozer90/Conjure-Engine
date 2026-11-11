#pragma once

#include "conjure/core/api.h"
#include "conjure/core/build_info.h"
#include "conjure/core/types.h"

#include <string_view>

// The engine version (docs/design/06_engine_loop.md 4.1), taken from the generated
// conjure/core/build_info.h so project.json's "engine_version" and the module manifests agree with the
// build.

namespace conjure::engine {

    inline constexpr u32 ENGINE_VERSION_MAJOR = CONJURE_VERSION_MAJOR;
    inline constexpr u32 ENGINE_VERSION_MINOR = CONJURE_VERSION_MINOR;
    inline constexpr u32 ENGINE_VERSION_PATCH = CONJURE_VERSION_PATCH;

    /// <summary>
    /// "major.minor.patch" of the running engine ("0.1.0").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view engine_version_string() noexcept;

    /// <summary>
    /// The engine's display name ("Conjure").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view engine_name() noexcept;
}
