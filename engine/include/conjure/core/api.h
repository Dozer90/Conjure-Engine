#pragma once

#include "conjure/core/platform_detect.h"

// Symbol visibility for the engine's public API (docs/design/07_modules.md 5, docs/ARCHITECTURE.md 9).
//
// Engine modules are static libraries compiled with CONJURE_SHARED_EXPORTS and hidden visibility,
// then bundled into the umbrella shared library conjure_shared. Only declarations that carry
// CONJURE_API are exported from it: every class, struct or free function of a public header whose
// definition lives in a .cpp, every `extern` variable, and explicit template specialisations whose
// members are defined out of line. Templates, inline and constexpr functions, enums and aggregates
// without out-of-line methods need no annotation. Nested classes carry their own CONJURE_API (MSVC
// does not export them with the enclosing class), and a friend declaration of an exported free
// function carries it too, because MSVC requires the attribute on the first declaration.
//
// Consumers of conjure_shared (executables, game modules, LINK_ENGINE_SHARED tests) are compiled
// with CONJURE_SHARED_IMPORTS; code that links the static libraries directly defines neither and
// CONJURE_API is empty. tools/check_exports.py (CTest entry exports.check_annotations) reports every
// public declaration that lacks the macro.

#if defined(CONJURE_SHARED_EXPORTS)
    #define CONJURE_API CONJURE_SYMBOL_EXPORT
#elif defined(CONJURE_SHARED_IMPORTS)
    #define CONJURE_API CONJURE_SYMBOL_IMPORT
#else
    #define CONJURE_API
#endif

// Entry points that plugins export with C linkage and default visibility regardless of how the
// engine itself is linked.
#define CONJURE_PLUGIN_EXPORT extern "C" CONJURE_SYMBOL_EXPORT
