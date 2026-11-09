// Fixture: a plain shared library that exports neither conjure_module_describe nor
// conjure_module_entry.

#include "conjure/core/platform_detect.h"

#include <cstdint>

extern "C" CONJURE_SYMBOL_EXPORT std::uint32_t conjure_no_symbol_marker(void) {
    return 7;
}
