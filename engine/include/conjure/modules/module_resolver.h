#pragma once

#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/api.h"
#include "conjure/core/error.h"

#include <span>
#include <vector>

// Dependency resolution (docs/design/07_modules.md 3.2): a pure function from registered modules to
// a deterministic load order plus per-module failures.
//
//   1. Per candidate: engineVersion must contain the host version (ENGINE_VERSION_MISMATCH) and
//      requiredFeatures must be a subset of the host's (FEATURE_UNAVAILABLE).
//   2. Per dependency: found by name among candidates and loaded modules (a SKIPPED module counts as
//      missing); a missing non-optional one is DEPENDENCY_MISSING, a version outside the range
//      DEPENDENCY_VERSION.
//   3. Kahn's algorithm with ties broken by name, so the order is identical on every machine.
//      Nodes left with incoming edges are a DEPENDENCY_CYCLE (the message lists the cycle).
//   4. A failure propagates to every dependent as DEPENDENCY_FAILED naming the root cause.
//   5. A GAME module sorts after every module that does not depend on it; a second game module
//      (candidate or already loaded) is MULTIPLE_GAME_MODULES.

namespace conjure::modules {

    class module_registry;

    struct resolution_input {
        std::span<const module_id> candidates = {};     // DISCOVERED, UNLOADED, FAILED, RESOLVED
        std::span<const module_id> loaded = {};         // LOADED; satisfy dependencies, never reordered
        const module_registry *registry = nullptr;
        semantic_version engineVersion = {};
        module_feature_flags engineFeatures = module_feature_flags::NONE;
    };

    struct resolution_failure {
        module_id id = {};
        core::error error = {};
    };

    struct resolution_result {
        std::vector<module_id> order = {};              // candidates to load, in order
        std::vector<resolution_failure> failures = {};  // candidates that cannot load, in name order
    };

    /// <summary>
    /// Resolves the load order of the candidates. Pure: reads the registry, changes nothing.
    /// </summary>
    [[nodiscard]] CONJURE_API resolution_result resolve_load_order(const resolution_input &input);
}
