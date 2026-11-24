// Fixture: a module whose entry calls into sibling_helper, a library that lies next to its binary and
// nowhere the loader searches by default. The call keeps the import, so the module loads only when the
// loader finds that library in the module's own folder.

#include "conjure/modules/modules.h"

#include "conjure/core/platform_detect.h"

#include <cstdint>

using namespace conjure;
using namespace conjure::modules;

extern "C" CONJURE_SYMBOL_IMPORT std::uint32_t conjure_sibling_helper_value(void);

namespace {
    constexpr std::uint32_t EXPECTED_HELPER_VALUE = 42;

    module_entry_status sibling_entry(module_context &) {
        if (conjure_sibling_helper_value() != EXPECTED_HELPER_VALUE) {
            return module_entry_status::FAILED;
        }
        return module_entry_status::OK;
    }
}

CONJURE_DECLARE_MODULE(sibling_entry)
