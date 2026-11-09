// Fixture: built with NAME module_wrong_name_actual while its manifest says module_wrong_name, so
// the describe() name never matches. The entry counts its runs; it must never run.

#include "conjure/modules/modules.h"

#include <cstdint>

using namespace conjure;
using namespace conjure::modules;

namespace {
    std::uint32_t sEntryCount = 0;

    module_entry_status wrong_name_entry(module_context &) {
        ++sEntryCount;
        return module_entry_status::OK;
    }
}

extern "C" CONJURE_MODULE_EXPORT std::uint32_t conjure_fixture_entry_count(void) {
    return sEntryCount;
}

CONJURE_DECLARE_MODULE(wrong_name_entry)
