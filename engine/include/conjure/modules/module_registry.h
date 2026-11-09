#pragma once

#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>
#include <span>

// Read-only view of every registered module (docs/design/07_modules.md 4.6). Mutated only by the
// owning module_manager at the structural sync point; between sync points it is immutable and may
// be read from job threads. Getters taking an id assert on an invalid or stale id and return an
// empty value, so a stale handle never dereferences another module's record.

namespace conjure::modules {

    struct module_record;
    class module_manager;

    class CONJURE_API module_registry {
        private:
            struct registry_state;
            std::unique_ptr<registry_state> mState = nullptr;

            friend class module_manager;

        public:
            module_registry();
            ~module_registry();

            module_registry(const module_registry &) = delete;
            module_registry &operator=(const module_registry &) = delete;

        public:
            /// <summary>
            /// The module registered under the name, or an invalid id.
            /// </summary>
            [[nodiscard]] module_id find(core::string_id name) const noexcept;

            /// <summary>
            /// True when the id refers to a registered module and its generation is current.
            /// </summary>
            [[nodiscard]] bool is_valid(module_id id) const noexcept;

            [[nodiscard]] module_state state(module_id id) const noexcept;
            [[nodiscard]] const module_manifest &manifest(module_id id) const noexcept;
            [[nodiscard]] const module_registration_set &registrations(module_id id) const noexcept;

            /// <summary>
            /// The error of the last failed load or unload, or an empty error.
            /// </summary>
            [[nodiscard]] const core::error &last_error(module_id id) const noexcept;

            /// <summary>
            /// The directory the manifest lives in; empty for static modules.
            /// </summary>
            [[nodiscard]] const std::filesystem::path &root_directory(module_id id) const noexcept;

            /// <summary>
            /// The derived binary path; empty for static and manifest-only modules.
            /// </summary>
            [[nodiscard]] const std::filesystem::path &binary_path(module_id id) const noexcept;

            /// <summary>
            /// Number of times the module reached LOADED.
            /// </summary>
            [[nodiscard]] u32 load_count(module_id id) const noexcept;

            /// <summary>
            /// Every registered module, in registration order.
            /// </summary>
            [[nodiscard]] std::span<const module_id> all() const noexcept;

            /// <summary>
            /// The LOADED modules in the order they were loaded.
            /// </summary>
            [[nodiscard]] std::span<const module_id> load_order() const noexcept;

            /// <summary>
            /// LOADED modules that depend on the module.
            /// </summary>
            [[nodiscard]] std::span<const module_id> dependents(module_id id) const noexcept;

            /// <summary>
            /// LOADED modules the module depends on.
            /// </summary>
            [[nodiscard]] std::span<const module_id> dependencies(module_id id) const noexcept;

            /// <summary>
            /// Number of registered modules.
            /// </summary>
            [[nodiscard]] usize size() const noexcept;

            /// <summary>
            /// Records that 'owner' provided 'key' under 'point' (a component type name under
            /// "ecs.component", say) so the inspector can show "provided by". Called by consumers
            /// in attach(); the manager forgets a module's entries when it unloads.
            /// </summary>
            void note_ownership(core::string_id point, core::string_id key, module_id owner);

            /// <summary>
            /// Drops every ownership entry of the module.
            /// </summary>
            void forget_ownership(module_id owner);

            /// <summary>
            /// The module that provided the key, or an invalid id.
            /// </summary>
            [[nodiscard]] module_id owner_of(core::string_id point, core::string_id key) const noexcept;

        private:
            // Record access for the owning manager (engine/src/modules/module_record.h).
            [[nodiscard]] module_record *record(module_id id) noexcept;
            [[nodiscard]] const module_record *record(module_id id) const noexcept;
            [[nodiscard]] module_record &record_at(u16 index) noexcept;
            [[nodiscard]] module_record &add_record(core::string_id name);
            void retire_generation(module_record &record);
            void append_load_order(module_id id);
            void remove_load_order(module_id id);
    };
}
