#pragma once

#include "conjure/modules/module_service.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

// Extension descriptors shared by the fixture modules and the module tests. Real descriptor types
// are defined by the consuming areas (ecs, assets, editor); these stand in for them here so the
// module system is tested without any of those modules.

namespace conjure::modules::fixtures {

    /// <summary>
    /// An event the test host publishes; module_alpha retains a subscription to it through its
    /// context so the test can prove the callback dies with the module.
    /// </summary>
    struct host_ping {
        u32 value = 0;
    };

    /// <summary>
    /// A host service handing a module the host's event bus. Provided by the test when it wants
    /// module_alpha to subscribe.
    /// </summary>
    struct host_event_service {
        CONJURE_MODULE_SERVICE(conjure::modules::fixtures::host_event_service);

        core::event_bus *bus = nullptr;
    };

    struct component_desc {
        static constexpr core::string_id EXTENSION_POINT{"test.component"};

        const char *name = "";
        u32 size = 0;
        u32 alignment = 1;
    };

    struct system_desc {
        static constexpr core::string_id EXTENSION_POINT{"test.system"};

        const char *name = "";
        u32 phase = 0;
    };

    struct panel_desc {
        static constexpr core::string_id EXTENSION_POINT{"test.panel"};

        const char *title = "";
    };

    /// <summary>
    /// Pointers into a module's own counters so a test can watch its lifecycle callbacks run
    /// without a second handle to the binary.
    /// </summary>
    struct probe_desc {
        static constexpr core::string_id EXTENSION_POINT{"test.probe"};

        const u32 *activatedCount = nullptr;
        const u32 *deactivatingCount = nullptr;
    };
}
