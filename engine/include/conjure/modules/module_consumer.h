#pragma once

#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/flat_map.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <vector>

// Consumers apply what modules register (docs/design/07_modules.md D3, D6, 4.4). The ECS, assets,
// scripting and editor each implement one; the manager calls them in add order on attach and in
// reverse on detach, always on the main thread between frames.

namespace conjure::modules {

    class module_context;
    class module_registry;

    /// <summary>
    /// Keyed binary blobs a consumer stores on detach during a reload and reads back after attach.
    /// </summary>
    class CONJURE_API reload_context {
        private:
            core::flat_map<core::string_id, std::vector<std::byte>> mBlobs = {};

        public:
            reload_context();
            ~reload_context();

            reload_context(const reload_context &) = delete;
            reload_context &operator=(const reload_context &) = delete;

        public:
            /// <summary>
            /// Stores a copy of the bytes under the key, replacing an earlier blob.
            /// </summary>
            void store(core::string_id key, std::span<const std::byte> bytes);

            /// <summary>
            /// The blob stored under the key; empty when missing.
            /// </summary>
            [[nodiscard]] std::span<const std::byte> fetch(core::string_id key) const noexcept;

            [[nodiscard]] bool contains(core::string_id key) const noexcept;

            /// <summary>
            /// Number of stored blobs.
            /// </summary>
            [[nodiscard]] usize size() const noexcept;
    };

    /// <summary>
    /// One area's view of module registrations. Every call runs on the main thread with no jobs
    /// in flight.
    /// </summary>
    class CONJURE_API module_consumer {
        public:
            module_consumer();
            virtual ~module_consumer();

            module_consumer(const module_consumer &) = delete;
            module_consumer &operator=(const module_consumer &) = delete;

        public:
            /// <summary>
            /// The consumer's name ("ecs"), used in REGISTRATION_CONFLICT messages.
            /// </summary>
            [[nodiscard]] virtual core::string_id name() const noexcept = 0;

            /// <summary>
            /// Rejects conflicting registrations before anything is applied. Runs for every
            /// consumer before the first attach, so a veto leaves the engine untouched.
            /// </summary>
            [[nodiscard]] virtual core::result<void> validate(
              module_id id,
              const module_registration_set &set,
              const module_registry &registry
            ) = 0;

            /// <summary>
            /// Applies the registrations. Cannot fail: validate() already accepted them.
            /// </summary>
            virtual void attach(module_id id, module_context &context, const module_registration_set &set) = 0;

            /// <summary>
            /// Side-effect free veto: IN_USE under REFUSE_IF_IN_USE, UNLOAD_BLOCKED in play mode.
            /// </summary>
            [[nodiscard]] virtual core::result<void> can_detach(module_id id, unload_policy policy) = 0;

            /// <summary>
            /// Removes everything the module registered. 'reload' is null for a plain unload and
            /// points at the snapshot to fill during a reload.
            /// </summary>
            virtual void detach(module_id id, reload_context *reload) = 0;

            /// <summary>
            /// Restores what detach() stored, after the module was attached again.
            /// </summary>
            virtual void restore(module_id id, const reload_context &reload) = 0;

            /// <summary>
            /// How many live objects still use the module's registrations (entities with its
            /// components, open assets, ...).
            /// </summary>
            [[nodiscard]] virtual u32 live_use_count(module_id id) const = 0;

            /// <summary>
            /// True for consumers that take part in reload_scope::MANAGED_ONLY (scripting).
            /// </summary>
            [[nodiscard]] virtual bool participates_in_managed_reload() const noexcept;
    };
}
