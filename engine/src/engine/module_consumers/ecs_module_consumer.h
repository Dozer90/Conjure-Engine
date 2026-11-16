#pragma once

#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/world.h"
#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"

#include <vector>

// The ECS module consumer (docs/design/07_modules.md D3, D6, 3.6, 9; R2, R19): turns the trivially
// copyable ecs::component_type_desc / ecs::system_extension_desc records a module registered into
// component_registry::register_described and scheduler::add_system calls, records ownership for the
// inspector, counts live instances and strips them on unload.
//
// Access sets: a system record names the components it reads and writes. The names become the system's
// resolveAccess hook, which the scheduler calls when the system is added and again whenever the registry
// has gained component types (docs/design/01_ecs.md D15). A name the registry does not know at attach is
// therefore no load failure (it is what a component of an absent optional dependency looks like): attach()
// logs one WARN per such name and the system gains the access when a component of that name is registered.
// A misspelt name stays a warning for good.
//
// Reload snapshot: component ids are assigned per name and never reused, and a module whose layout changed
// is refused with NAME_CONFLICT at validate(), so the columns of a trivially copyable component can stay in
// their chunks across a reload (the id, size and alignment are the same afterwards). Only the names are
// stored in the reload_context; non-trivial components (whose ops point into module code) are stripped.
// This replaces the column-blob snapshot of 07_modules.md 3.6 with a cheaper invariant that holds by
// construction; engine/src/engine/README.md records the deviation.

namespace conjure::engine {

    class ecs_module_consumer final : public modules::module_consumer {
        private:
            struct module_record {
                modules::module_id id = {};
                std::vector<ecs::component_id> components = {};
                std::vector<ecs::system_id> systems = {};
            };

            ecs::component_registry *mRegistry = nullptr;
            ecs::world *mWorld = nullptr;
            ecs::scheduler *mScheduler = nullptr;
            core::string_id mName = {};
            std::vector<module_record> mRecords = {};

        public:
            ecs_module_consumer(ecs::component_registry &registry, ecs::world &world, ecs::scheduler &scheduler);
            ~ecs_module_consumer() override;

        public:
            /// <summary>
            /// "ecs".
            /// </summary>
            [[nodiscard]] core::string_id name() const noexcept override;

            /// <summary>
            /// REGISTRATION_CONFLICT for a component record without a name or with a size beyond
            /// MAX_COMPONENT_SIZE, for a component already registered with another size or alignment, and for
            /// a system record without a name or run function or without a declared access set when it is not
            /// EXCLUSIVE. Also for a system whose name its phase already holds, in the scheduler
            /// (scheduler::find_system) or earlier in the same set: scheduler::add_system would answer
            /// NAME_CONFLICT, so the module fails to load instead of losing the system at attach(). The
            /// lookup sees only the systems the scheduler holds when the module loads: the engine adds its
            /// own systems at startup step 18, after the modules loaded at startup (step 15), so a startup
            /// module that takes the name of an engine system passes here and keeps the name, and the
            /// engine's later add_system is the one refused with NAME_CONFLICT (01_ecs.md D13). A read
            /// or write name the registry does not know is not refused here (see attach()).
            /// </summary>
            [[nodiscard]] core::result<void> validate(
              modules::module_id id,
              const modules::module_registration_set &set,
              const modules::module_registry &registry
            ) override;

            /// <summary>
            /// Registers the components (noting ownership under "ecs.component") and adds the systems, then
            /// finalises the scheduler, which also completes the access set of every earlier system that
            /// named one of the components registered here. Each system's read and write names are
            /// resolved by the scheduler through its resolveAccess hook; one WARN is logged per name that
            /// is still not registered once the module's own components are (the system keeps running
            /// without that access and gains it when a component of the name is registered).
            /// </summary>
            void attach(
              modules::module_id id,
              modules::module_context &context,
              const modules::module_registration_set &set
            ) override;

            /// <summary>
            /// IN_USE under REFUSE_IF_IN_USE while entities carry the module's components.
            /// </summary>
            [[nodiscard]] core::result<void> can_detach(modules::module_id id, modules::unload_policy policy) override;

            /// <summary>
            /// Removes the systems; strips every instance of the module's components (all of them on a plain
            /// unload, the non-trivial ones on a reload).
            /// </summary>
            void detach(modules::module_id id, modules::reload_context *reload) override;

            /// <summary>
            /// Nothing to restore: trivially copyable columns stayed in place (see the header comment).
            /// </summary>
            void restore(modules::module_id id, const modules::reload_context &reload) override;

            /// <summary>
            /// Entities holding at least one of the module's components, summed over its component types.
            /// </summary>
            [[nodiscard]] u32 live_use_count(modules::module_id id) const override;

            /// <summary>
            /// The component ids a LOADED module registered (empty for an unknown id).
            /// </summary>
            [[nodiscard]] std::vector<ecs::component_id> components_of(modules::module_id id) const;

            /// <summary>
            /// The system ids a LOADED module registered (empty for an unknown id).
            /// </summary>
            [[nodiscard]] std::vector<ecs::system_id> systems_of(modules::module_id id) const;

            /// <summary>
            /// The reload_context key a module's snapshot travels under: "ecs.<module name>".
            /// </summary>
            [[nodiscard]] static core::string_id snapshot_key(
              const modules::module_registry &registry,
              modules::module_id id
            );

        private:
            [[nodiscard]] module_record *find_record(modules::module_id id) noexcept;
            [[nodiscard]] const module_record *find_record(modules::module_id id) const noexcept;
    };
}
