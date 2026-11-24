#include "engine/module_consumers/ecs_module_consumer.h"

#include "conjure/engine/log_category.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/log.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_error.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::engine;

namespace {

    constexpr core::string_id EXTENSION_POINT_COMPONENT{"ecs.component"};

    [[nodiscard]] std::string_view module_name(const modules::module_registry &registry, modules::module_id id) {
        if (!registry.is_valid(id)) {
            return "<unknown>";
        }
        return registry.manifest(id).name;
    }

    [[nodiscard]] core::error conflict(std::string_view module, std::string_view detail) {
        return core::make_error(
          modules::module_error::REGISTRATION_CONFLICT,
          "ecs: module '{}': {}",
          module,
          detail
        );
    }

    /// <summary>
    /// The component names a module system declared, copied out of the module's record so the system's
    /// resolveAccess hook owns them.
    /// </summary>
    struct access_names {
        std::vector<core::string_id> reads = {};
        std::vector<core::string_id> writes = {};
    };

    /// <summary>
    /// Sets the bit of every name the registry knows; a name it does not know yet is left out.
    /// </summary>
    void set_registered(
      const ecs::component_registry &registry,
      std::span<const core::string_id> names,
      ecs::component_mask &mask
    ) {
        for (const core::string_id name : names) {
            const ecs::component_id id = registry.find(name);
            if (id.is_valid()) {
                mask.set(id);
            }
        }
    }

    /// <summary>
    /// One WARN per name of the list the registry does not know.
    /// </summary>
    void warn_unregistered(
      const ecs::component_registry &registry,
      std::string_view module,
      const char *system,
      std::string_view list,
      std::span<const core::string_id> names
    ) {
        for (usize index = 0; index < names.size(); ++index) {
            if (registry.find(names[index]).is_valid()) {
                continue;
            }
            CONJURE_LOG_WARN(
              LOG_ENGINE,
              "ecs module consumer: module '{}': system '{}' names component '{}' ({}[{}]), which is not "
              "registered; the system runs without that access until a component of the name is registered",
              module,
              system,
              names[index],
              list,
              index
            );
        }
    }

    /// <summary>
    /// Turns a trivially copyable system record into the scheduler's system_desc. The read and write
    /// names become the resolveAccess hook instead of fixed masks (01_ecs.md D15): the scheduler calls it
    /// when the system is added and again whenever the registry has gained component types, so a name
    /// that is unknown at attach (a component of an optional dependency that is not loaded) joins the
    /// access set once it is registered.
    /// </summary>
    [[nodiscard]] ecs::system_desc to_system_desc(const ecs::system_extension_desc &record) {
        ecs::system_desc desc = {};
        // Interned so scheduler diagnostics and the profiler can print the module system's name.
        desc.name = core::string_id::intern(std::string_view(record.name));
        desc.phase = record.phase;
        desc.flags = record.flags;
        access_names names = {};
        names.reads.assign(record.reads, record.reads + record.readCount);
        names.writes.assign(record.writes, record.writes + record.writeCount);
        desc.resolveAccess = [names = std::move(names)](
          const ecs::component_registry &registry,
          ecs::component_mask &reads,
          ecs::component_mask &writes
        ) {
            set_registered(registry, names.reads, reads);
            set_registered(registry, names.writes, writes);
        };
        for (u32 index = 0; index < record.afterCount; ++index) {
            desc.after.push_back(record.after[index]);
        }
        for (u32 index = 0; index < record.beforeCount; ++index) {
            desc.before.push_back(record.before[index]);
        }
        void (*run)(ecs::system_context &) = record.run;
        desc.run = [run](ecs::system_context &context) {
            run(context);
        };
        return desc;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

ecs_module_consumer::ecs_module_consumer(
  ecs::component_registry &registry,
  ecs::world &world,
  ecs::scheduler &scheduler
) :
    mRegistry(&registry),
    mWorld(&world),
    mScheduler(&scheduler),
    mName(core::string_id::intern("ecs")) {}

ecs_module_consumer::~ecs_module_consumer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::string_id ecs_module_consumer::name() const noexcept {
    return mName;
}

core::result<void> ecs_module_consumer::validate(
  modules::module_id id,
  const modules::module_registration_set &set,
  const modules::module_registry &registry
) {
    const std::string_view module = module_name(registry, id);

    for (const ecs::component_type_desc *record : modules::extensions_of<ecs::component_type_desc>(set)) {
        if (record->name == nullptr || std::string_view(record->name).empty()) {
            return conflict(module, "a component record has no name");
        }
        if (record->size > ecs::MAX_COMPONENT_SIZE) {
            return conflict(module, std::format("component '{}' is larger than MAX_COMPONENT_SIZE", record->name));
        }
        const ecs::component_id existing = mRegistry->find(core::string_id(std::string_view(record->name)));
        if (!existing.is_valid()) {
            continue;
        }
        const ecs::component_info &info = mRegistry->info(existing);
        if (info.size != record->size || info.alignment != record->alignment) {
            return conflict(
              module,
              std::format("component '{}' is already registered with another layout", record->name)
            );
        }
    }

    const core::small_vector<const ecs::system_extension_desc *, 8> systems =
      modules::extensions_of<ecs::system_extension_desc>(set);
    for (usize index = 0; index < systems.size(); ++index) {
        const ecs::system_extension_desc *record = systems[index];
        if (record->name == nullptr || std::string_view(record->name).empty()) {
            return conflict(module, "a system record has no name");
        }
        if (record->run == nullptr) {
            return conflict(module, std::format("system '{}' has no run function", record->name));
        }
        if (static_cast<u32>(record->phase) >= ecs::SYSTEM_PHASE_COUNT) {
            return conflict(module, std::format("system '{}' names an unknown phase", record->name));
        }
        const bool exclusive = has_flag(record->flags, ecs::system_flags::EXCLUSIVE);
        if (!exclusive && record->readCount == 0 && record->writeCount == 0) {
            return conflict(module, std::format("system '{}' declares no access set", record->name));
        }

        // scheduler::add_system refuses a name its phase already holds (01_ecs.md D13). Checked here,
        // against the scheduler and against the module's own earlier records, so the clash fails the load
        // instead of leaving attach() to drop the system. Only the systems added so far can clash: the
        // engine adds its own at startup step 18, after the startup modules, and is then the one refused.
        const std::string_view systemName(record->name);
        const std::string_view phaseName = ecs::to_string(record->phase);
        if (mScheduler->find_system(record->phase, core::string_id(systemName)).is_valid()) {
            return conflict(
              module,
              std::format("system '{}' is already registered in phase {}", systemName, phaseName)
            );
        }
        for (usize earlier = 0; earlier < index; ++earlier) {
            if (systems[earlier]->phase == record->phase && std::string_view(systems[earlier]->name) == systemName) {
                return conflict(
                  module,
                  std::format("system '{}' is registered twice in phase {}", systemName, phaseName)
                );
            }
        }
    }
    return core::result<void>::success();
}

void ecs_module_consumer::attach(
  modules::module_id id,
  modules::module_context &context,
  const modules::module_registration_set &set
) {
    const std::string_view module = context.manifest().name;
    module_record record = {};
    record.id = id;

    for (const ecs::component_type_desc *desc : modules::extensions_of<ecs::component_type_desc>(set)) {
        core::result<ecs::component_id> registered = mRegistry->register_described(*desc);
        if (!registered.has_value()) {
            // validate() accepted the layout; anything left is a registry limit worth hearing about.
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "ecs module consumer: component '{}' could not be registered: {}",
              desc->name,
              registered.get_error().to_string()
            );
            continue;
        }
        record.components.push_back(registered.value());
    }

    for (const ecs::system_extension_desc *desc : modules::extensions_of<ecs::system_extension_desc>(set)) {
        core::result<ecs::system_id> added = mScheduler->add_system(to_system_desc(*desc));
        if (!added.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "ecs module consumer: system '{}' could not be added: {}",
              desc->name,
              added.get_error().to_string()
            );
            continue;
        }
        record.systems.push_back(added.value());
        // The module's own components are registered above, so a name unknown here belongs to a module
        // that is not loaded, or is misspelt. Neither fails the load (the first is what an optional
        // dependency looks like); both are said once, and the scheduler completes the access set when
        // the component appears.
        const std::span<const core::string_id> reads(desc->reads, desc->readCount);
        const std::span<const core::string_id> writes(desc->writes, desc->writeCount);
        warn_unregistered(*mRegistry, module, desc->name, "reads", reads);
        warn_unregistered(*mRegistry, module, desc->name, "writes", writes);
    }

    const core::result<void> finalised = mScheduler->finalise();
    if (!finalised.has_value()) {
        CONJURE_LOG_ERROR(
          LOG_ENGINE,
          "ecs module consumer: scheduler finalise failed: {}",
          finalised.get_error().to_string()
        );
    }
    mRecords.push_back(std::move(record));
}

core::result<void> ecs_module_consumer::can_detach(modules::module_id id, modules::unload_policy policy) {
    if (policy != modules::unload_policy::REFUSE_IF_IN_USE) {
        return core::result<void>::success();
    }
    const u32 uses = live_use_count(id);
    if (uses == 0) {
        return core::result<void>::success();
    }
    return core::make_error(
      modules::module_error::IN_USE,
      "ecs: {} entity component(s) of module {} are still alive",
      uses,
      modules::to_string(id)
    );
}

void ecs_module_consumer::detach(modules::module_id id, modules::reload_context *reload) {
    module_record *record = find_record(id);
    if (record == nullptr) {
        return;
    }

    for (const ecs::system_id system : record->systems) {
        const core::result<void> removed = mScheduler->remove_system(system);
        CONJURE_UNUSED(removed);
    }

    for (const ecs::component_id component : record->components) {
        const ecs::component_info &info = mRegistry->info(component);
        const bool keep = reload != nullptr && info.is_trivial();
        if (keep) {
            continue;
        }
        const core::result<void> stripped = mWorld->remove_from_all(component);
        if (!stripped.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "ecs module consumer: could not strip component '{}': {}",
              info.name.to_string(),
              stripped.get_error().to_string()
            );
        }
    }

    if (reload != nullptr) {
        // The snapshot is the list of component names whose columns stayed in place.
        std::string names = {};
        for (const ecs::component_id component : record->components) {
            if (!names.empty()) {
                names.push_back(';');
            }
            names += mRegistry->info(component).name.to_string();
        }
        const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(names.data()), names.size());
        reload->store(core::string_id(std::string_view("ecs.snapshot")), bytes);
    }

    const usize index = static_cast<usize>(record - mRecords.data());
    mRecords.erase(mRecords.begin() + static_cast<isize>(index));
}

void ecs_module_consumer::restore(modules::module_id id, const modules::reload_context &reload) {
    CONJURE_UNUSED(reload);
    CONJURE_LOG_DEBUG(
      LOG_ENGINE,
      "ecs module consumer: module {} restored (columns kept in place)",
      modules::to_string(id)
    );
}

u32 ecs_module_consumer::live_use_count(modules::module_id id) const {
    const module_record *record = find_record(id);
    if (record == nullptr) {
        return 0;
    }
    u32 total = 0;
    for (const ecs::component_id component : record->components) {
        total += mWorld->instance_count(component);
    }
    return total;
}

std::vector<ecs::component_id> ecs_module_consumer::components_of(modules::module_id id) const {
    const module_record *record = find_record(id);
    return record == nullptr ? std::vector<ecs::component_id>{} : record->components;
}

std::vector<ecs::system_id> ecs_module_consumer::systems_of(modules::module_id id) const {
    const module_record *record = find_record(id);
    return record == nullptr ? std::vector<ecs::system_id>{} : record->systems;
}

core::string_id ecs_module_consumer::snapshot_key(const modules::module_registry &registry, modules::module_id id) {
    return core::string_id::intern("ecs." + std::string(module_name(registry, id)));
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

ecs_module_consumer::module_record *ecs_module_consumer::find_record(modules::module_id id) noexcept {
    for (module_record &record : mRecords) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

const ecs_module_consumer::module_record *ecs_module_consumer::find_record(modules::module_id id) const noexcept {
    for (const module_record &record : mRecords) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}
