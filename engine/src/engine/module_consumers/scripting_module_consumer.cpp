#include "engine/module_consumers/scripting_module_consumer.h"

#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_manifest.h"

#include <algorithm>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
scripting_module_consumer::scripting_module_consumer(scripting::script_runtime *runtime) :
    mRuntime(runtime),
    mName(core::string_id::intern("scripting")) {}
#else
scripting_module_consumer::scripting_module_consumer() :
    mName(core::string_id::intern("scripting")) {}
#endif

scripting_module_consumer::~scripting_module_consumer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::string_id scripting_module_consumer::name() const noexcept {
    return mName;
}

core::result<void> scripting_module_consumer::validate(
  modules::module_id id,
  const modules::module_registration_set &set,
  const modules::module_registry &registry
) {
    CONJURE_UNUSED(id);
    CONJURE_UNUSED(set);
    CONJURE_UNUSED(registry);
    return core::result<void>::success();
}

void scripting_module_consumer::attach(
  modules::module_id id,
  modules::module_context &context,
  const modules::module_registration_set &set
) {
    module_record record = {};
    record.id = id;
    for (const std::string &relative : context.manifest().managedAssemblies) {
        record.assemblies.push_back(context.root_directory() / relative);
    }
    for (const std::string &relative : set.managedAssemblies) {
        const std::filesystem::path full = context.root_directory() / relative;
        if (std::find(record.assemblies.begin(), record.assemblies.end(), full) == record.assemblies.end()) {
            record.assemblies.push_back(full);
        }
    }
    for (const std::filesystem::path &assembly : record.assemblies) {
        CONJURE_LOG_INFO(
          LOG_ENGINE,
          "module '{}' brings managed assembly '{}'",
          context.manifest().name,
          assembly.generic_string()
        );
    }
    mRecords.push_back(std::move(record));
}

core::result<void> scripting_module_consumer::can_detach(modules::module_id id, modules::unload_policy policy) {
    CONJURE_UNUSED(id);
    CONJURE_UNUSED(policy);
    return core::result<void>::success();
}

void scripting_module_consumer::detach(modules::module_id id, modules::reload_context *reload) {
    module_record *record = find_record(id);
    if (record == nullptr) {
        return;
    }
    const usize index = static_cast<usize>(record - mRecords.data());
    mRecords.erase(mRecords.begin() + static_cast<isize>(index));
    if (reload == nullptr) {
        return;
    }
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    if (mRuntime != nullptr && mRuntime->is_assembly_loaded()) {
        // A MANAGED_ONLY reload is script_runtime::reload_game_assembly (R36); the runtime preserves fields.
        const core::result<void> reloaded = mRuntime->reload_game_assembly(mRuntime->assembly_path());
        ++mManagedReloads;
        if (!reloaded.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "scripting module consumer: game assembly reload failed: {}",
              reloaded.get_error().to_string()
            );
        }
    }
#endif
}

void scripting_module_consumer::restore(modules::module_id id, const modules::reload_context &reload) {
    CONJURE_UNUSED(id);
    CONJURE_UNUSED(reload);
}

u32 scripting_module_consumer::live_use_count(modules::module_id id) const {
    CONJURE_UNUSED(id);
    return 0;
}

bool scripting_module_consumer::participates_in_managed_reload() const noexcept {
    return true;
}

std::vector<std::filesystem::path> scripting_module_consumer::assemblies_of(modules::module_id id) const {
    for (const module_record &record : mRecords) {
        if (record.id == id) {
            return record.assemblies;
        }
    }
    return {};
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

scripting_module_consumer::module_record *scripting_module_consumer::find_record(modules::module_id id) noexcept {
    for (module_record &record : mRecords) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}
