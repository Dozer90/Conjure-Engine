#pragma once

#include "fixture_descriptors.h"

#include "conjure/modules/module_context.h"
#include "conjure/modules/module_entry.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/types.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Static modules for the manager tests: a manifest built from a few arguments plus one of the entry
// functions below. The descriptor keeps its JSON text alive, so a static_module_source must outlive
// the manager it was added to and must not be moved after add_static_module().

namespace conjure::modules::testing {

    struct dependency_spec {
        std::string name = {};
        std::string version = "*";
        bool optional = false;
    };

    /// <summary>
    /// Builds module.json text. 'extra' is appended verbatim inside the object (", \"key\": value").
    /// </summary>
    inline std::string manifest_json(
      std::string_view name,
      std::string_view version = "1.0.0",
      const std::vector<dependency_spec> &dependencies = {},
      std::string_view extra = {},
      std::string_view engine = "*"
    ) {
        std::string text =
          std::format(R"({{"schema": 1, "name": "{}", "version": "{}", "engine": "{}")", name, version, engine);
        if (!dependencies.empty()) {
            text.append(R"(, "dependencies": [)");
            for (usize index = 0; index < dependencies.size(); ++index) {
                const dependency_spec &dependency = dependencies[index];
                if (index > 0) {
                    text.append(", ");
                }
                text.append(std::format(
                  R"({{"name": "{}", "version": "{}", "optional": {}}})",
                  dependency.name,
                  dependency.version,
                  dependency.optional ? "true" : "false"
                ));
            }
            text.append("]");
        }
        text.append(extra);
        text.append("}");
        return text;
    }

    /// <summary>
    /// A static module whose manifest text lives as long as the object.
    /// </summary>
    class static_module_source {
        private:
            std::string mIdentifier = {};
            std::string mJson = {};
            static_module_desc mDesc = {};

        public:
            static_module_source(std::string identifier, std::string json, conjure_module_entry_fn entry) :
                mIdentifier(std::move(identifier)),
                mJson(std::move(json)) {
                mDesc.identifier = mIdentifier.c_str();
                mDesc.manifestJson = mJson.c_str();
                mDesc.entry = entry;
            }

            static_module_source(const static_module_source &) = delete;
            static_module_source &operator=(const static_module_source &) = delete;

        public:
            [[nodiscard]] const static_module_desc &desc() const noexcept {
                return mDesc;
            }
    };

    /*-----------------------------------------------------------------------*/
    // Entry functions. Counters are process-wide so tests reset them in their fixture.

    struct entry_counters {
        u32 entryRuns = 0;
        u32 activated = 0;
        u32 deactivating = 0;
        u32 flakyRuns = 0;
        u32 secondRunFailingRuns = 0;
        module_id lastActivatedId = {};
        module_id lastDeactivatingId = {};
    };

    inline entry_counters &counters() noexcept {
        static entry_counters sCounters = {};
        return sCounters;
    }

    inline void on_activated(module_context &context) {
        ++counters().activated;
        counters().lastActivatedId = context.id();
    }

    inline void on_deactivating(module_context &context) {
        ++counters().deactivating;
        counters().lastDeactivatingId = context.id();
    }

    /// <summary>
    /// Registers one component and one system named after the module, plus both lifecycle callbacks.
    /// </summary>
    inline module_entry_status registering_entry(module_context &context) {
        ++counters().entryRuns;
        module_registrar &registrar = context.registrar();
        registrar.register_extension(fixtures::component_desc{context.manifest().name.c_str(), 32, 8});
        registrar.register_extension(fixtures::system_desc{context.manifest().name.c_str(), 2});
        registrar.set_lifecycle(module_lifecycle{&on_activated, &on_deactivating});
        return module_entry_status::OK;
    }

    /// <summary>
    /// Fails twice and returns FAILED.
    /// </summary>
    inline module_entry_status failing_entry(module_context &context) {
        ++counters().entryRuns;
        context.registrar().fail("first reason");
        context.registrar().fail("second reason");
        return module_entry_status::FAILED;
    }

    /// <summary>
    /// Fails through the status only, without a reason.
    /// </summary>
    inline module_entry_status status_failing_entry(module_context &) {
        ++counters().entryRuns;
        return module_entry_status::FAILED;
    }

    /// <summary>
    /// Fails on its first run only, so a retry succeeds.
    /// </summary>
    inline module_entry_status flaky_entry(module_context &context) {
        ++counters().entryRuns;
        ++counters().flakyRuns;
        if (counters().flakyRuns < 2) {
            context.registrar().fail("not ready yet");
            return module_entry_status::FAILED;
        }
        return module_entry_status::OK;
    }

    /// <summary>
    /// Succeeds except on its second run (the first reload), so a failed reload can be provoked.
    /// </summary>
    inline module_entry_status second_run_failing_entry(module_context &context) {
        ++counters().entryRuns;
        ++counters().secondRunFailingRuns;
        if (counters().secondRunFailingRuns == 2) {
            context.registrar().fail("the rebuilt module is broken");
            return module_entry_status::FAILED;
        }
        return module_entry_status::OK;
    }

    inline constexpr conjure_module_entry_fn REGISTERING_ENTRY = &detail::entry_thunk<&registering_entry>;
    inline constexpr conjure_module_entry_fn FAILING_ENTRY = &detail::entry_thunk<&failing_entry>;
    inline constexpr conjure_module_entry_fn STATUS_FAILING_ENTRY = &detail::entry_thunk<&status_failing_entry>;
    inline constexpr conjure_module_entry_fn FLAKY_ENTRY = &detail::entry_thunk<&flaky_entry>;
    inline constexpr conjure_module_entry_fn SECOND_RUN_FAILING_ENTRY = &detail::entry_thunk<&second_run_failing_entry>;

    /// <summary>
    /// A manager in TEST host mode that loads no shared libraries, with its own event bus. Declare
    /// consumers and module sources before the host: the manager's destructor unloads what is still
    /// loaded and must find them alive.
    /// </summary>
    struct static_host {
        core::event_bus events = {};
        module_manager_desc desc = {};
        module_manager manager;     // no default: built from desc below

        static_host() :
            desc(make_desc()),
            manager(desc, events) {
            counters() = {};
        }

        explicit static_host(const module_manager_desc &customDesc) :
            desc(customDesc),
            manager(desc, events) {
            counters() = {};
        }

        static module_manager_desc make_desc() noexcept {
            module_manager_desc value = {};
            value.host = module_host_kind::TEST;
            value.allowSharedLibraries = false;
            return value;
        }
    };
}
