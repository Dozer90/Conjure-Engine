#pragma once

#include "conjure/modules/module_types.h"

#include "conjure/core/error.h"
#include "conjure/core/string_id.h"

// Events the module manager publishes on the engine's core::event_bus (docs/design/07_modules.md
// 4.7). All are published synchronously on the main thread from inside the manager's mutators.

namespace conjure::modules {

    /// <summary>
    /// A module is LOADED: every consumer attached and onActivated ran.
    /// </summary>
    struct module_loaded_event {
        module_id id = {};
    };

    /// <summary>
    /// A module is about to detach. 'reloading' is true during a reload cascade, when the module
    /// comes back under the same id.
    /// </summary>
    struct module_unloading_event {
        module_id id = {};
        bool reloading = false;
    };

    /// <summary>
    /// A module is UNLOADED and its id is stale; the name is kept for listeners that key by name.
    /// </summary>
    struct module_unloaded_event {
        module_id id = {};
        core::string_id name = {};
    };

    /// <summary>
    /// A load attempt failed and applied nothing; the error is also module_registry::last_error().
    /// </summary>
    struct module_load_failed_event {
        module_id id = {};
        core::error error = {};
    };
}
