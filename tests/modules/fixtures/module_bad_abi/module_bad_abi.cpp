// Fixture: compiled with CONJURE_TEST_ABI_VERSION_OVERRIDE so its signature never matches the host.
// The entry counts its runs; the host must refuse the module before it ever runs.

#include "conjure/modules/modules.h"

#include <cstdint>

using namespace conjure;
using namespace conjure::modules;

namespace {
    std::uint32_t sEntryCount = 0;

    module_entry_status bad_abi_entry(module_context &) {
        ++sEntryCount;
        return module_entry_status::OK;
    }
}

extern "C" CONJURE_MODULE_EXPORT std::uint32_t conjure_fixture_entry_count(void) {
    return sEntryCount;
}

CONJURE_DECLARE_MODULE(bad_abi_entry)
