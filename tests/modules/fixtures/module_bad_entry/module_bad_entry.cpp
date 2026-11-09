// Fixture: the entry records two failure reasons and returns FAILED; the load must report both.

#include "fixture_descriptors.h"

#include "conjure/modules/modules.h"

using namespace conjure;
using namespace conjure::modules;

namespace {
    module_entry_status bad_entry(module_context &context) {
        module_registrar &registrar = context.registrar();
        registrar.register_extension(fixtures::component_desc{"never_applied", 8, 8});
        registrar.fail("first reason: the fixture refuses to load");
        registrar.fail("second reason: and says so twice");
        return module_entry_status::FAILED;
    }
}

CONJURE_DECLARE_MODULE(bad_entry)
