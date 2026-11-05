#pragma once

#include "conjure/core/platform_detect.h"

// Symbol visibility for the engine's public API.
//
// Engine modules are static libraries. When they are bundled into the umbrella shared library
// (conjure_shared, see docs/design/07_modules.md section 5) the build defines
// CONJURE_SHARED_EXPORTS while compiling it and CONJURE_SHARED_IMPORTS while compiling code that
// links against it (plugins, executables). Static consumers define neither and CONJURE_API is empty.

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
