// Fixture: one component, one system, lifecycle callbacks that count, one log line through the
// host logger (which proves the plugin and the host share one engine copy) and, when the host
// provides fixtures::host_event_service, a retained subscription to its event bus.

#include "fixture_descriptors.h"

#include "conjure/modules/modules.h"

#include "conjure/core/log.h"
#include "conjure/core/types.h"

#include <cstdint>

using namespace conjure;
using namespace conjure::modules;

namespace {
    CONJURE_DEFINE_LOG_CATEGORY(LOG_MODULE_ALPHA, "module_alpha");

    u32 sActivatedCount = 0;
    u32 sDeactivatingCount = 0;
    u32 sPingTotal = 0;

    void on_host_ping(const fixtures::host_ping &ping) {
        sPingTotal += ping.value;
    }

    void on_activated(module_context &) {
        ++sActivatedCount;
    }

    void on_deactivating(module_context &) {
        ++sDeactivatingCount;
    }

    module_entry_status alpha_entry(module_context &context) {
        module_registrar &registrar = context.registrar();
        registrar.register_extension(fixtures::component_desc{"alpha_spinner", 16, 4});
        registrar.register_extension(fixtures::system_desc{"alpha_spin_system", 1});
        registrar.register_extension(fixtures::probe_desc{&sActivatedCount, &sDeactivatingCount});
        registrar.set_lifecycle(module_lifecycle{&on_activated, &on_deactivating});

        // The subscription's callback is module code: retaining it through the context is what
        // guarantees it is released before this binary is unmapped.
        const fixtures::host_event_service *events = context.try_service<fixtures::host_event_service>();
        if (events != nullptr && events->bus != nullptr) {
            context.retain(events->bus->subscribe<fixtures::host_ping>(&on_host_ping));
        }

        CONJURE_LOG_INFO(
          LOG_MODULE_ALPHA,
          "module_alpha entry running on host {} (engine {})",
          to_string(context.host()),
          context.engine_version()
        );
        return module_entry_status::OK;
    }
}

extern "C" CONJURE_MODULE_EXPORT std::uint32_t conjure_alpha_activated_count(void) {
    return sActivatedCount;
}

extern "C" CONJURE_MODULE_EXPORT std::uint32_t conjure_alpha_ping_total(void) {
    return sPingTotal;
}

CONJURE_DECLARE_MODULE(alpha_entry)
