#pragma once

#include "conjure/modules/module_entry.h"

#include <span>

// The manifest-only static modules of the built-in libraries (docs/design/07_modules.md 4.1,
// 09_cross_area_resolutions.md R18): one CONJURE_DECLARE_STATIC_MODULE per enabled engine library, kind
// "engine", with a null entry. They exist so game modules can declare dependencies on "conjure.physics"
// and feature flags resolve uniformly; the subsystem adapters do the actual registration.

namespace conjure::engine {

    /// <summary>
    /// Every built-in module descriptor in dependency order (core first, engine last). The descriptors are
    /// function-local statics and outlive any module_manager.
    /// </summary>
    [[nodiscard]] std::span<const modules::static_module_desc *const> builtin_static_modules() noexcept;
}
