#pragma once

#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_entry.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_service.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/api.h"
#include "conjure/core/assert.h"
#include "conjure/core/event_bus.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <atomic>
#include <filesystem>
#include <vector>

// The module manager (docs/design/07_modules.md 3 and 4.5): discovers manifests, resolves
// dependencies, loads, unloads and reloads modules in order and hands registrations to consumers.
// Every mutator runs on the main thread at the engine's structural sync point (asserted); request_*
// enqueue work that apply_pending() performs at the next frame start.

namespace conjure::modules {

    struct module_record;

    struct module_manager_desc {
        module_host_kind host = module_host_kind::RUNTIME;
        semantic_version engineVersion = host_engine_version();
        module_feature_flags engineFeatures = host_feature_flags();
        build_config binaryConfig = host_build_config();        // selects bin/<platform>-<arch>-<config>
        bool allowSharedLibraries = true;
        bool shadowCopyBinaries = false;
        bool keepBinariesResident = false;                      // debug aid: never close a binary
        std::filesystem::path shadowCopyDirectory = {};
        unload_policy defaultUnloadPolicy = unload_policy::REFUSE_IF_IN_USE;
        bool (*isInsideFrame)() = nullptr;                      // engine loop hook; null = always outside
    };

    class CONJURE_API module_manager {
        private:
            enum class pending_kind : u8 {
                LOAD,
                UNLOAD,
                RELOAD
            };

            struct pending_operation {
                pending_kind kind = pending_kind::LOAD;
                module_id id = {};
                unload_policy policy = unload_policy::REFUSE_IF_IN_USE;
                reload_scope scope = reload_scope::FULL;
            };

            module_manager_desc mDesc = {};
            module_registry mRegistry = {};
            service_table mServices = {};
            std::vector<module_consumer *> mConsumers = {};     // non-owning; attach order
            std::vector<pending_operation> mPending = {};
            std::atomic<bool> mHasPending = false;
            core::event_bus *mEvents = nullptr;
            u32 mNextLoadOrder = 1;

        public:
            module_manager(const module_manager_desc &desc, core::event_bus &events);
            ~module_manager();                                  // unload_all()

            module_manager(const module_manager &) = delete;
            module_manager &operator=(const module_manager &) = delete;

        public:
            /*---------------------------------------------------------------*/
            // Host setup: before anything is LOADED.

            /// <summary>
            /// Adds a consumer; consumers attach in add order and detach in reverse. Asserts when
            /// a module is LOADED or the consumer was already added.
            /// </summary>
            void add_consumer(module_consumer &consumer);

            /// <summary>
            /// Removes a consumer. Asserts when a module is LOADED.
            /// </summary>
            void remove_consumer(module_consumer &consumer);

            /// <summary>
            /// Provides a host service to module contexts. Asserts when a module is LOADED.
            /// </summary>
            template <typename ServiceType>
            void provide_service(ServiceType &service) {
                CONJURE_ASSERT(!any_loaded(), "provide_service while a module is loaded");
                if (any_loaded()) {
                    return;
                }
                mServices.provide(service);
            }

            /*---------------------------------------------------------------*/
            // Registration.

            /// <summary>
            /// Registers a module compiled into the host. The descriptor must outlive the manager.
            /// MANIFEST_INVALID, INVALID_NAME or DUPLICATE_NAME on a bad manifest.
            /// </summary>
            [[nodiscard]] core::result<module_id> add_static_module(const static_module_desc &desc);

            /// <summary>
            /// Registers a module from its module.json; the module root is the file's directory.
            /// A manifest that excludes the host platform is registered as SKIPPED.
            /// </summary>
            [[nodiscard]] core::result<module_id> add_manifest(const std::filesystem::path &manifestPath);

            /// <summary>
            /// Registers every immediate subdirectory of 'searchDirectory' that holds a module.json,
            /// in name order, and returns how many were added. Per-module errors are collected into
            /// one error (valid modules are still added); a missing directory is MANIFEST_NOT_FOUND.
            /// </summary>
            [[nodiscard]] core::result<u32> discover(const std::filesystem::path &searchDirectory);

            /*---------------------------------------------------------------*/
            // Synchronous operations (main thread, outside a frame).

            /// <summary>
            /// Resolves every registered module and loads them in order, continuing past failures.
            /// Returns the first error.
            /// </summary>
            [[nodiscard]] core::result<void> load_all();

            /// <summary>
            /// Loads the module and the transitive closure of its dependencies in order, stopping
            /// at the first failure (earlier successes stay loaded).
            /// </summary>
            [[nodiscard]] core::result<void> load(module_id id);

            /// <summary>
            /// Unloads a LOADED module without LOADED dependents. Pre-checks (HAS_DEPENDENTS, the
            /// consumers' can_detach) are side-effect free.
            /// </summary>
            [[nodiscard]] core::result<void> unload(module_id id, unload_policy policy);
            [[nodiscard]] core::result<void> unload(module_id id);

            /// <summary>
            /// Reloads the module and its LOADED dependents: detach with snapshots in reverse load
            /// order, close and reopen the binaries (FULL), attach in order and restore. A failing
            /// load leaves that module and the rest UNLOADED with their snapshots kept for the next
            /// successful load. The id stays valid across a reload.
            /// </summary>
            [[nodiscard]] core::result<void> reload(module_id id, reload_scope scope);

            /// <summary>
            /// Unloads every LOADED module in reverse load order without the can_detach pre-check
            /// (consumers detach as under REMOVE_INSTANCES). Called by the destructor.
            /// </summary>
            void unload_all();

            /*---------------------------------------------------------------*/
            // Deferred operations: enqueue now, performed by apply_pending().

            void request_load(module_id id);
            void request_unload(module_id id, unload_policy policy);
            void request_reload(module_id id, reload_scope scope);

            /// <summary>
            /// Performs every request in order. Costs one relaxed atomic load when idle. Requests
            /// for stale ids are dropped with a warning; failures are logged and kept in last_error().
            /// </summary>
            void apply_pending();

            /// <summary>
            /// Number of requests waiting for apply_pending().
            /// </summary>
            [[nodiscard]] usize pending_count() const noexcept;

            /*---------------------------------------------------------------*/
            // Views.

            [[nodiscard]] const module_registry &registry() const noexcept;

            /// <summary>
            /// Mutable registry for consumers recording ownership in attach().
            /// </summary>
            [[nodiscard]] module_registry &registry() noexcept;

            [[nodiscard]] const service_table &services() const noexcept;
            [[nodiscard]] const module_manager_desc &desc() const noexcept;

            /// <summary>
            /// True while at least one module is LOADED.
            /// </summary>
            [[nodiscard]] bool any_loaded() const noexcept;

            /// <summary>
            /// Writes &lt;out&gt;/modules/&lt;name&gt;/ (manifest without editorOnly, the
            /// bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;/ directory and the managed assemblies) for every
            /// LOADED module that is not an engine module, not editor-only and not static.
            /// The bin directory is copied entry by entry with its subfolders, following links, and leaves
            /// out every file or folder named *.lib, *.exp, *.ilk, *.a, *.pdb or *.dSYM (ASCII case ignored):
            /// dependent libraries and data files ship, link and debug by-products and native symbols do not.
            /// Files already in the output are overwritten. Other files of an earlier export stay, except that
            /// such by-products anywhere under the output bin directory are removed first (links there are not
            /// followed), unless that directory is the module's own bin directory or holds it.
            /// BINARY_NOT_FOUND names the missing configuration; EXPORT_FAILED any create, write, list, resolve,
            /// remove or copy error, an entry that is neither a file nor a folder (a broken link), a link back to a
            /// folder being copied and a missing managed assembly. A failure leaves the files copied before it
            /// in place.
            /// </summary>
            [[nodiscard]] core::result<void> export_layout(
              const std::filesystem::path &outputDirectory,
              build_config targetConfig
            ) const;

        private:
            [[nodiscard]] bool check_sync_point(const char *operation) const;
            [[nodiscard]] core::result<module_record *> checked_record(module_id id);
            [[nodiscard]] core::result<module_id> register_manifest(
              module_manifest manifest,
              const std::filesystem::path &rootDirectory,
              const static_module_desc *staticDesc
            );
            [[nodiscard]] core::result<void> load_one(module_record &record);
            [[nodiscard]] core::result<void> load_resolved(const std::vector<module_id> &order, bool stopAtFailure);
            [[nodiscard]] core::result<void> unload_checked(module_record &record);
            [[nodiscard]] core::result<void> precheck_unload(module_record &record, unload_policy policy);
            void detach_record(module_record &record, bool reloading, reload_context *snapshot);
            void close_binary(module_record &record);
            [[nodiscard]] const module_record *failed_dependency_of(const module_record &record) const noexcept;
            void fail_record(module_record &record, core::error failure);
            void link_dependencies(module_record &record);
            void unlink_dependencies(module_record &record);
            void collect_reload_cascade(module_record &record, std::vector<module_id> &cascade) const;
            [[nodiscard]] core::result<void> reload_full(const std::vector<module_id> &cascade);
            [[nodiscard]] core::result<void> reload_managed(const std::vector<module_id> &cascade);
            void enqueue(pending_operation operation);
            void apply_one(const pending_operation &operation);
    };
}
