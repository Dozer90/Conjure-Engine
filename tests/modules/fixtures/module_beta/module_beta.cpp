// Fixture: depends on module_alpha ^1.0.0, registers a panel record and a managed assembly path.

#include "fixture_descriptors.h"

#include "conjure/modules/modules.h"

using namespace conjure;
using namespace conjure::modules;

namespace {
    module_entry_status beta_entry(module_context &context) {
        module_registrar &registrar = context.registrar();
        registrar.register_extension(fixtures::panel_desc{"Beta Panel"});
        registrar.register_managed_assembly("managed/BetaExtra.dll");
        return module_entry_status::OK;
    }
}

CONJURE_DECLARE_MODULE(beta_entry)
