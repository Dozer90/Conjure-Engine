// Fixture: the library module_sibling imports. It is built next to the module binary only, so a load
// of the module succeeds only when the loader searches the module's own folder for its dependencies.

#include "conjure/core/platform_detect.h"

#include <cstdint>

extern "C" CONJURE_SYMBOL_EXPORT std::uint32_t conjure_sibling_helper_value(void) {
    return 42;
}
