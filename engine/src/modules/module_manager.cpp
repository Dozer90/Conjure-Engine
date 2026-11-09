#include "conjure/modules/module_manager.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_events.h"
#include "conjure/modules/module_resolver.h"
#include "modules/log_category.h"
#include "modules/module_loader.h"
#include "modules/module_record.h"
#include "modules/shadow_copy.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/string_id.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    error sync_point_error(const char *operation) {
        return make_error(
          error_code::INVALID_STATE,
          "module_manager::{} must run on the main thread outside a frame",
          operation
        );
    }

    std::string join_names(const module_registry &registry, std::span<const module_id> ids) {
        std::string text = {};
        for (const module_id id : ids) {
            if (!text.empty()) {
                text.append(", ");
            }
            text.append(registry.is_valid(id) ? registry.manifest(id).name : to_string(id));
        }
        return text;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

module_manager::module_manager(const module_manager_desc &desc, event_bus &events) :
    mDesc(desc),
    mEvents(&events) {
    if (mDesc.shadowCopyBinaries) {
        modules::detail::clean_shadow_copy_cache(mDesc.shadowCopyDirectory);
    }
}

module_manager::~module_manager() {
    unload_all();
}


/*===========================================================================*/
/*===========================================================================*/
// Host setup

void module_manager::add_consumer(module_consumer &consumer) {
    CONJURE_ASSERT(!any_loaded(), "add_consumer while a module is loaded");
    const bool present = std::find(mConsumers.begin(), mConsumers.end(), &consumer) != mConsumers.end();
    CONJURE_ASSERT(!present, "add_consumer: consumer '{}' was already added", consumer.name());
    if (any_loaded() || present) {
        return;
    }
    mConsumers.push_back(&consumer);
}

void module_manager::remove_consumer(module_consumer &consumer) {
    CONJURE_ASSERT(!any_loaded(), "remove_consumer while a module is loaded");
    if (any_loaded()) {
        return;
    }
    mConsumers.erase(std::remove(mConsumers.begin(), mConsumers.end(), &consumer), mConsumers.end());
}


/*===========================================================================*/
/*===========================================================================*/
// Registration

result<module_id> module_manager::add_static_module(const static_module_desc &desc) {
    const std::string source = std::format("<static:{}>", desc.identifier == nullptr ? "" : desc.identifier);
    const std::string_view manifestText = desc.manifestJson == nullptr ? std::string_view() : desc.manifestJson;
    CONJURE_TRY_ASSIGN(module_manifest manifest, parse_module_manifest(manifestText, source));
    return register_manifest(std::move(manifest), {}, &desc);
}

result<module_id> module_manager::add_manifest(const std::filesystem::path &manifestPath) {
    CONJURE_TRY_ASSIGN(module_manifest manifest, load_module_manifest(manifestPath));
    std::error_code ignored = {};
    const std::filesystem::path root = std::filesystem::absolute(manifestPath, ignored).parent_path();
    return register_manifest(std::move(manifest), root, nullptr);
}

result<u32> module_manager::discover(const std::filesystem::path &searchDirectory) {
    std::error_code failure = {};
    if (!std::filesystem::is_directory(searchDirectory, failure)) {
        return make_module_error(
          module_error::MANIFEST_NOT_FOUND,
          "module search directory '{}' does not exist",
          searchDirectory.generic_string()
        );
    }

    std::vector<std::filesystem::path> manifests = {};
    const std::filesystem::directory_iterator entries(searchDirectory, failure);
    if (failure) {
        // An unreadable directory must not pass for an empty one: the project would silently
        // start without its plugins.
        return make_module_error(
          module_error::MANIFEST_NOT_FOUND,
          "cannot list module search directory '{}': {}",
          searchDirectory.generic_string(),
          failure.message()
        );
    }
    for (const std::filesystem::directory_entry &entry : entries) {
        std::error_code ignored = {};
        if (!entry.is_directory(ignored)) {
            continue;
        }
        const std::filesystem::path manifest = entry.path() / MODULE_MANIFEST_FILE_NAME;
        if (std::filesystem::is_regular_file(manifest, ignored)) {
            manifests.push_back(manifest);
        }
    }
    const auto byDirectoryName = [](const std::filesystem::path &left, const std::filesystem::path &right) {
        return left.parent_path().filename().generic_string() < right.parent_path().filename().generic_string();
    };
    std::sort(manifests.begin(), manifests.end(), byDirectoryName);

    u32 added = 0;
    std::vector<error> errors = {};
    for (const std::filesystem::path &manifest : manifests) {
        const result<module_id> registered = add_manifest(manifest);
        if (registered.has_value()) {
            ++added;
        } else {
            errors.push_back(registered.get_error());
        }
    }
    CONJURE_LOG_INFO(
      LOG_MODULES,
      "discovered {} module(s) in '{}' ({} rejected)",
      added,
      searchDirectory.generic_string(),
      errors.size()
    );

    if (errors.empty()) {
        return added;
    }
    std::string combined = {};
    for (const error &entry : errors) {
        if (!combined.empty()) {
            combined.append("; ");
        }
        combined.append(entry.message());
    }
    if (errors.front().is_category<module_error>()) {
        return error::from(static_cast<module_error>(errors.front().code()), std::move(combined));
    }
    return error(error_code::INVALID_ARGUMENT, std::move(combined));
}


/*===========================================================================*/
/*===========================================================================*/
// Synchronous operations

result<void> module_manager::load_all() {
    if (!check_sync_point("load_all")) {
        return sync_point_error("load_all");
    }

    std::vector<module_id> candidates = {};
    for (const module_id id : mRegistry.all()) {
        const module_record &record = *mRegistry.record(id);
        if (record.is_loadable()) {
            candidates.push_back(id);
        }
    }

    resolution_input input = {};
    input.candidates = candidates;
    input.loaded = mRegistry.load_order();
    input.registry = &mRegistry;
    input.engineVersion = mDesc.engineVersion;
    input.engineFeatures = mDesc.engineFeatures;
    resolution_result resolved = resolve_load_order(input);

    result<void> first = {};
    for (resolution_failure &failure : resolved.failures) {
        if (first.has_value()) {
            first = failure.error;
        }
        fail_record(*mRegistry.record(failure.id), std::move(failure.error));
    }

    const result<void> loaded = load_resolved(resolved.order, false);
    if (first.has_value() && !loaded.has_value()) {
        first = loaded.get_error();
    }
    return first;
}

result<void> module_manager::load(module_id id) {
    if (!check_sync_point("load")) {
        return sync_point_error("load");
    }
    CONJURE_TRY_ASSIGN(module_record *record, checked_record(id));
    if (record->state == module_state::LOADED) {
        return make_module_error(module_error::ALREADY_LOADED, "module '{}' is already loaded", record->manifest.name);
    }
    if (record->state == module_state::SKIPPED) {
        return make_module_error(
          module_error::PLATFORM_UNSUPPORTED,
          "module '{}' does not support this platform ({})",
          record->manifest.name,
          to_string(record->manifest.platforms)
        );
    }

    // The transitive closure of unloaded dependencies, each at most once.
    std::vector<module_id> candidates = {};
    std::vector<module_id> pending = {id};
    while (!pending.empty()) {
        const module_id current = pending.back();
        pending.pop_back();
        if (std::find(candidates.begin(), candidates.end(), current) != candidates.end()) {
            continue;
        }
        candidates.push_back(current);
        for (const module_dependency &dependency : mRegistry.manifest(current).dependencies) {
            const module_id dependencyId = mRegistry.find(string_id(dependency.name));
            const module_record *dependencyRecord = mRegistry.record(dependencyId);
            if (dependencyRecord != nullptr && dependencyRecord->is_loadable()) {
                pending.push_back(dependencyId);
            }
        }
    }

    resolution_input input = {};
    input.candidates = candidates;
    input.loaded = mRegistry.load_order();
    input.registry = &mRegistry;
    input.engineVersion = mDesc.engineVersion;
    input.engineFeatures = mDesc.engineFeatures;
    resolution_result resolved = resolve_load_order(input);

    result<void> requested = {};
    for (resolution_failure &failure : resolved.failures) {
        if (failure.id == id) {
            requested = failure.error;
        }
        fail_record(*mRegistry.record(failure.id), std::move(failure.error));
    }
    if (!requested.has_value()) {
        return requested;
    }
    return load_resolved(resolved.order, true);
}

result<void> module_manager::unload(module_id id, unload_policy policy) {
    if (!check_sync_point("unload")) {
        return sync_point_error("unload");
    }
    CONJURE_TRY_ASSIGN(module_record *record, checked_record(id));
    if (record->state != module_state::LOADED) {
        return make_module_error(
          module_error::NOT_LOADED,
          "module '{}' is {}, not LOADED",
          record->manifest.name,
          record->state
        );
    }
    CONJURE_TRY(precheck_unload(*record, policy));
    return unload_checked(*record);
}

result<void> module_manager::unload(module_id id) {
    return unload(id, mDesc.defaultUnloadPolicy);
}

result<void> module_manager::reload(module_id id, reload_scope scope) {
    if (!check_sync_point("reload")) {
        return sync_point_error("reload");
    }
    CONJURE_TRY_ASSIGN(module_record *record, checked_record(id));
    if (record->state != module_state::LOADED) {
        return make_module_error(
          module_error::NOT_LOADED,
          "module '{}' is {}, not LOADED",
          record->manifest.name,
          record->state
        );
    }

    std::vector<module_id> cascade = {};
    collect_reload_cascade(*record, cascade);

    // Pre-checks are side-effect free: a veto anywhere in the cascade reloads nothing. Instances
    // are snapshotted and restored, so they never count as "in use" here.
    for (const module_id member : cascade) {
        for (module_consumer *consumer : mConsumers) {
            if (scope == reload_scope::MANAGED_ONLY && !consumer->participates_in_managed_reload()) {
                continue;
            }
            CONJURE_TRY(consumer->can_detach(member, unload_policy::REMOVE_INSTANCES));
        }
    }

    CONJURE_LOG_INFO(
      LOG_MODULES,
      "reloading module '{}' ({}, cascade: {})",
      record->manifest.name,
      to_string(scope),
      join_names(mRegistry, cascade)
    );
    if (scope == reload_scope::MANAGED_ONLY) {
        return reload_managed(cascade);
    }
    return reload_full(cascade);
}

void module_manager::unload_all() {
    if (!check_sync_point("unload_all")) {
        return;
    }
    while (!mRegistry.load_order().empty()) {
        const module_id id = mRegistry.load_order().back();
        module_record &record = *mRegistry.record(id);
        const result<void> unloaded = unload_checked(record);
        if (!unloaded.has_value()) {
            // unload_checked already took the record out of the load order (FAULTED), so the loop
            // makes progress; the error is kept in last_error().
            CONJURE_LOG_ERROR(LOG_MODULES, "unload_all: {}", unloaded.get_error().message());
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Deferred operations

void module_manager::request_load(module_id id) {
    pending_operation operation = {};
    operation.kind = pending_kind::LOAD;
    operation.id = id;
    enqueue(operation);
}

void module_manager::request_unload(module_id id, unload_policy policy) {
    pending_operation operation = {};
    operation.kind = pending_kind::UNLOAD;
    operation.id = id;
    operation.policy = policy;
    enqueue(operation);
}

void module_manager::request_reload(module_id id, reload_scope scope) {
    pending_operation operation = {};
    operation.kind = pending_kind::RELOAD;
    operation.id = id;
    operation.scope = scope;
    enqueue(operation);
}

void module_manager::apply_pending() {
    if (!mHasPending.load(std::memory_order_relaxed)) {
        return;
    }
    if (!check_sync_point("apply_pending")) {
        return;
    }

    // Requests enqueued by handlers while applying wait for the next frame.
    std::vector<pending_operation> operations = std::move(mPending);
    mPending = {};
    mHasPending.store(false, std::memory_order_relaxed);
    for (const pending_operation &operation : operations) {
        apply_one(operation);
    }
}

usize module_manager::pending_count() const noexcept {
    return mPending.size();
}


/*===========================================================================*/
/*===========================================================================*/
// Views

const module_registry &module_manager::registry() const noexcept {
    return mRegistry;
}

module_registry &module_manager::registry() noexcept {
    return mRegistry;
}

const service_table &module_manager::services() const noexcept {
    return mServices;
}

const module_manager_desc &module_manager::desc() const noexcept {
    return mDesc;
}

bool module_manager::any_loaded() const noexcept {
    return !mRegistry.load_order().empty();
}

result<void> module_manager::export_layout(
  const std::filesystem::path &outputDirectory,
  build_config targetConfig
) const {
    const binary_triplet triplet = host_binary_triplet(targetConfig);
    for (const module_id id : mRegistry.load_order()) {
        const module_record &record = *mRegistry.record(id);
        const module_manifest &manifest = record.manifest;
        if (record.staticDesc != nullptr || manifest.kind == module_kind::ENGINE || manifest.editorOnly) {
            continue;
        }

        const std::filesystem::path targetRoot = outputDirectory / "modules" / manifest.name;
        std::error_code failure = {};
        std::filesystem::create_directories(targetRoot, failure);
        if (failure) {
            return make_module_error(
              module_error::EXPORT_FAILED,
              "module '{}': cannot create '{}': {}",
              manifest.name,
              targetRoot.generic_string(),
              failure.message()
            );
        }

        // The exported manifest lists every assembly the runtime has to load, entry-registered
        // ones included, so the exported tree describes itself completely.
        module_manifest exportedManifest = manifest;
        exportedManifest.managedAssemblies = record.registrations.managedAssemblies;
        const json::document exported = write_module_manifest(exportedManifest, false);
        const result<void> written = exported.write_file(targetRoot / MODULE_MANIFEST_FILE_NAME, true);
        if (!written.has_value()) {
            return make_module_error(
              module_error::EXPORT_FAILED,
              "module '{}': cannot write its manifest: {}",
              manifest.name,
              written.get_error().message()
            );
        }

        if (!manifest.binary.empty()) {
            const std::filesystem::path sourceBin = module_binary_directory(record.rootDirectory, triplet);
            if (!std::filesystem::is_directory(sourceBin, failure)) {
                return make_module_error(
                  module_error::BINARY_NOT_FOUND,
                  "module '{}': no {} binaries at '{}'; build the project in {} first",
                  manifest.name,
                  to_string(triplet),
                  sourceBin.generic_string(),
                  to_string(targetConfig)
                );
            }
            const std::filesystem::path targetBin = module_binary_directory(targetRoot, triplet);
            std::filesystem::create_directories(targetBin, failure);
            const std::filesystem::copy_options options =
              std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive;
            std::filesystem::copy(sourceBin, targetBin, options, failure);
            if (failure) {
                return make_module_error(
                  module_error::EXPORT_FAILED,
                  "module '{}': cannot copy '{}' to '{}': {}",
                  manifest.name,
                  sourceBin.generic_string(),
                  targetBin.generic_string(),
                  failure.message()
                );
            }
        }

        // The registration set holds the manifest's assemblies plus those the entry added through
        // module_registrar::register_managed_assembly(); the exported game needs every one of them.
        for (const std::string &assembly : record.registrations.managedAssemblies) {
            const std::filesystem::path source = record.rootDirectory / assembly;
            const std::filesystem::path target = targetRoot / assembly;
            if (!std::filesystem::is_regular_file(source, failure)) {
                return make_module_error(
                  module_error::EXPORT_FAILED,
                  "module '{}': managed assembly '{}' does not exist",
                  manifest.name,
                  source.generic_string()
                );
            }
            std::filesystem::create_directories(target.parent_path(), failure);
            std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, failure);
            if (failure) {
                return make_module_error(
                  module_error::EXPORT_FAILED,
                  "module '{}': cannot copy '{}' to '{}': {}",
                  manifest.name,
                  source.generic_string(),
                  target.generic_string(),
                  failure.message()
                );
            }
        }
        CONJURE_LOG_INFO(LOG_MODULES, "exported module '{}' to '{}'", manifest.name, targetRoot.generic_string());
    }
    return {};
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods: checks and registration

bool module_manager::check_sync_point(const char *operation) const {
    const bool mainThread = jobs::is_main_thread();
    CONJURE_ASSERT(mainThread, "module_manager::{} must run on the main thread", operation);
    const bool insideFrame = mDesc.isInsideFrame != nullptr && mDesc.isInsideFrame();
    CONJURE_ASSERT(!insideFrame, "module_manager::{} must run outside a frame", operation);
    return mainThread && !insideFrame;
}

result<module_record *> module_manager::checked_record(module_id id) {
    if (!id.is_valid() || id.index >= mRegistry.size()) {
        return make_module_error(module_error::INVALID_ID, "module id {} does not name a module", id);
    }
    module_record &record = mRegistry.record_at(id.index);
    if (record.id.generation != id.generation) {
        return make_module_error(
          module_error::STALE_ID,
          "module id {} is stale: module '{}' is now {}",
          id,
          record.manifest.name,
          record.id
        );
    }
    if (record.state == module_state::FAULTED) {
        return make_module_error(
          module_error::FAULTED,
          "module '{}' is faulted: {}",
          record.manifest.name,
          record.lastError.message()
        );
    }
    return &record;
}

result<module_id> module_manager::register_manifest(
  module_manifest manifest,
  const std::filesystem::path &rootDirectory,
  const static_module_desc *staticDesc
) {
    const string_id name(manifest.name);
    if (mRegistry.find(name).is_valid()) {
        return make_module_error(
          module_error::DUPLICATE_NAME,
          "a module named '{}' is already registered",
          manifest.name
        );
    }

    module_record &record = mRegistry.add_record(name);
    record.manifest = std::move(manifest);
    record.rootDirectory = rootDirectory;
    record.staticDesc = staticDesc;

    if (!has_flag(record.manifest.platforms, host_platform_flag())) {
        record.state = module_state::SKIPPED;
        CONJURE_LOG_INFO(
          LOG_MODULES,
          "module '{}' skipped: supports {} only",
          record.manifest.name,
          to_string(record.manifest.platforms)
        );
        return record.id;
    }

    if (staticDesc == nullptr && !record.manifest.binary.empty()) {
        record.binaryPath = module_binary_path(record.manifest, rootDirectory, host_binary_triplet(mDesc.binaryConfig));
    }
    record.state = module_state::DISCOVERED;
    CONJURE_LOG_DEBUG(
      LOG_MODULES,
      "registered module '{}' {} ({})",
      record.manifest.name,
      record.manifest.version,
      staticDesc != nullptr ? "static" : rootDirectory.generic_string()
    );
    return record.id;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods: loading

result<void> module_manager::load_one(module_record &record) {
    CONJURE_ASSERT(record.is_loadable(), "load_one: module '{}' is {}", record.manifest.name, record.state);
    if (record.state == module_state::LOADED) {
        return make_module_error(module_error::ALREADY_LOADED, "module '{}' is already loaded", record.manifest.name);
    }

    conjure_module_entry_fn entry = nullptr;
    if (record.staticDesc != nullptr) {
        entry = record.staticDesc->entry;
    } else if (!record.binaryPath.empty()) {
        result<modules::detail::opened_binary> opened =
          modules::detail::open_module_binary(record.manifest, record.binaryPath, mDesc, record.loadCount + 1);
        if (!opened.has_value()) {
            fail_record(record, opened.get_error());
            return opened.get_error();
        }
        if (mDesc.keepBinariesResident && record.library.is_loaded()) {
            // dynamic_library's move assignment would unload the handle it replaces; the debug aid
            // promises the earlier image (and its shadow copy) stays until the manager is destroyed.
            resident_binary resident = {};
            resident.library = std::move(record.library);
            resident.loadedBinaryPath = std::move(record.loadedBinaryPath);
            record.residentBinaries.push_back(std::move(resident));
        }
        record.library = std::move(opened->library);
        record.loadedBinaryPath = std::move(opened->loadedPath);
        entry = opened->entry;
    }

    record.context = std::make_unique<module_context>(
      record.id,
      mDesc.host,
      mDesc.engineVersion,
      record.manifest,
      record.rootDirectory,
      mServices
    );
    record.registrations = {};
    record.registrations.managedAssemblies = record.manifest.managedAssemblies;

    if (entry != nullptr) {
        module_registrar registrar(record.registrations);
        record.context->begin_entry(registrar);
        const u32 status = entry(record.context.get());
        record.context->end_entry();

        if (status != static_cast<u32>(module_entry_status::OK) || !record.registrations.errors.empty()) {
            std::string reasons = {};
            for (const std::string &reason : record.registrations.errors) {
                if (!reasons.empty()) {
                    reasons.append("; ");
                }
                reasons.append(reason);
            }
            if (reasons.empty()) {
                reasons = std::format("the entry returned status {}", status);
            }
            error failure = make_module_error(
              module_error::ENTRY_FAILED,
              "module '{}' entry failed: {}",
              record.manifest.name,
              reasons
            );
            close_binary(record);
            fail_record(record, failure);
            return failure;
        }
    }

    for (module_consumer *consumer : mConsumers) {
        const result<void> validated = consumer->validate(record.id, record.registrations, mRegistry);
        if (!validated.has_value()) {
            error failure = make_module_error(
              module_error::REGISTRATION_CONFLICT,
              "consumer '{}' rejected module '{}': {}",
              consumer->name(),
              record.manifest.name,
              validated.get_error().message()
            );
            close_binary(record);
            fail_record(record, failure);
            return failure;
        }
    }
    for (module_consumer *consumer : mConsumers) {
        consumer->attach(record.id, *record.context, record.registrations);
    }

    record.state = module_state::LOADED;
    record.loadOrder = mNextLoadOrder;
    ++mNextLoadOrder;
    ++record.loadCount;
    record.lastError = {};
    mRegistry.append_load_order(record.id);
    link_dependencies(record);

    if (record.pendingSnapshot != nullptr) {
        for (module_consumer *consumer : mConsumers) {
            consumer->restore(record.id, *record.pendingSnapshot);
        }
        record.pendingSnapshot.reset();
    }

    if (record.registrations.lifecycle.onActivated != nullptr) {
        record.registrations.lifecycle.onActivated(*record.context);
    }
    CONJURE_LOG_INFO(
      LOG_MODULES,
      "loaded module '{}' {} ({} extension record(s))",
      record.manifest.name,
      record.manifest.version,
      record.registrations.extensions.size()
    );
    mEvents->publish(module_loaded_event{record.id});
    return {};
}

result<void> module_manager::load_resolved(const std::vector<module_id> &order, bool stopAtFailure) {
    result<void> first = {};
    for (const module_id id : order) {
        module_record *record = mRegistry.record(id);
        if (record == nullptr || record->state == module_state::LOADED) {
            continue;
        }

        // Resolution ran once up front; a dependency that failed while loading (its entry, a
        // consumer veto, a missing binary) fails its dependents here so none of them loads alone.
        result<void> loaded = {};
        const module_record *failedDependency = failed_dependency_of(*record);
        if (failedDependency != nullptr) {
            loaded = make_module_error(
              module_error::DEPENDENCY_FAILED,
              "module '{}' depends on '{}', which failed to load: {}",
              record->manifest.name,
              failedDependency->manifest.name,
              failedDependency->lastError.message()
            );
            fail_record(*record, loaded.get_error());
        } else {
            // RESOLVED is set only now, so a load that stops early leaves the untouched remainder in
            // the state it had before instead of a stale "about to load".
            record->state = module_state::RESOLVED;
            loaded = load_one(*record);
        }
        if (loaded.has_value()) {
            continue;
        }
        if (first.has_value()) {
            first = loaded.get_error();
        }
        if (stopAtFailure) {
            return first;
        }
    }
    return first;
}

const module_record *module_manager::failed_dependency_of(const module_record &record) const noexcept {
    for (const module_dependency &dependency : record.manifest.dependencies) {
        if (dependency.optional) {
            continue;
        }
        const module_record *dependencyRecord = mRegistry.record(mRegistry.find(string_id(dependency.name)));
        if (dependencyRecord != nullptr && dependencyRecord->state != module_state::LOADED) {
            return dependencyRecord;
        }
    }
    return nullptr;
}

void module_manager::fail_record(module_record &record, error failure) {
    record.state = module_state::FAILED;
    record.lastError = std::move(failure);
    CONJURE_LOG_ERROR(LOG_MODULES, "module '{}' failed to load: {}", record.manifest.name, record.lastError.message());
    mEvents->publish(module_load_failed_event{record.id, record.lastError});
}

void module_manager::close_binary(module_record &record) {
    record.context.reset();
    record.registrations = {};
    if (mDesc.keepBinariesResident) {
        return;
    }
    record.library.unload();
    modules::detail::remove_shadow_copy(record.loadedBinaryPath);
    record.loadedBinaryPath.clear();
}

void module_manager::link_dependencies(module_record &record) {
    for (const module_dependency &dependency : record.manifest.dependencies) {
        const module_id dependencyId = mRegistry.find(string_id(dependency.name));
        module_record *dependencyRecord = mRegistry.record(dependencyId);
        if (dependencyRecord == nullptr || dependencyRecord->state != module_state::LOADED) {
            continue;
        }
        record.dependencies.push_back(dependencyId);
        dependencyRecord->dependents.push_back(record.id);
    }

    // An optional dependency may become LOADED after its dependent did; the dependent may already
    // be using it (services, owner_of), so the link is made in both directions here as well, which
    // keeps HAS_DEPENDENTS and the reload cascade honest whichever side loaded first.
    for (const module_id loadedId : mRegistry.load_order()) {
        module_record *dependent = mRegistry.record(loadedId);
        if (dependent == nullptr || dependent == &record) {
            continue;
        }
        const bool alreadyLinked =
          std::find(dependent->dependencies.begin(), dependent->dependencies.end(), record.id)
          != dependent->dependencies.end();
        if (alreadyLinked) {
            continue;
        }
        for (const module_dependency &dependency : dependent->manifest.dependencies) {
            if (dependency.name != record.manifest.name) {
                continue;
            }
            dependent->dependencies.push_back(record.id);
            record.dependents.push_back(dependent->id);
            break;
        }
    }
}

void module_manager::unlink_dependencies(module_record &record) {
    for (const module_id dependencyId : record.dependencies) {
        module_record *dependencyRecord = mRegistry.record(dependencyId);
        if (dependencyRecord == nullptr) {
            continue;
        }
        small_vector<module_id, 8> &dependents = dependencyRecord->dependents;
        dependents.erase(std::remove(dependents.begin(), dependents.end(), record.id), dependents.end());
    }
    record.dependencies.clear();

    // Dependents linked late (an optional dependency loaded after them) drop their side too, so a
    // later load of this module links them again from a clean slate.
    for (const module_id dependentId : record.dependents) {
        module_record *dependentRecord = mRegistry.record(dependentId);
        if (dependentRecord == nullptr) {
            continue;
        }
        small_vector<module_id, 8> &dependencies = dependentRecord->dependencies;
        dependencies.erase(std::remove(dependencies.begin(), dependencies.end(), record.id), dependencies.end());
    }
    record.dependents.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods: unloading and reloading

result<void> module_manager::precheck_unload(module_record &record, unload_policy policy) {
    for (const module_id dependent : record.dependents) {
        const module_record *dependentRecord = mRegistry.record(dependent);
        if (dependentRecord != nullptr && dependentRecord->state == module_state::LOADED) {
            return make_module_error(
              module_error::HAS_DEPENDENTS,
              "module '{}' cannot unload: loaded module(s) depend on it ({})",
              record.manifest.name,
              join_names(mRegistry, record.dependents)
            );
        }
    }
    for (module_consumer *consumer : mConsumers) {
        CONJURE_TRY(consumer->can_detach(record.id, policy));
    }
    return {};
}

void module_manager::detach_record(module_record &record, bool reloading, reload_context *snapshot) {
    if (record.registrations.lifecycle.onDeactivating != nullptr) {
        record.registrations.lifecycle.onDeactivating(*record.context);
    }
    mEvents->publish(module_unloading_event{record.id, reloading});

    for (auto iterator = mConsumers.rbegin(); iterator != mConsumers.rend(); ++iterator) {
        (*iterator)->detach(record.id, snapshot);
    }
    mRegistry.forget_ownership(record.id);

    // A consumer that still reports live users after detach left the engine in an unknown state:
    // the record becomes FAULTED and its binary stays resident for the process lifetime, because a
    // leaked library beats closing one whose code may still be referenced.
    for (module_consumer *consumer : mConsumers) {
        const u32 remaining = consumer->live_use_count(record.id);
        CONJURE_ASSERT(
          remaining == 0,
          "consumer '{}' still has {} live use(s) of module '{}' after detach",
          consumer->name(),
          remaining,
          record.manifest.name
        );
        if (remaining == 0) {
            continue;
        }
        record.state = module_state::FAULTED;
        record.lastError = make_module_error(
          module_error::FAULTED,
          "module '{}' is faulted: consumer '{}' kept {} live use(s) after detach; the binary stays resident",
          record.manifest.name,
          consumer->name(),
          remaining
        );
        CONJURE_LOG_ERROR(LOG_MODULES, "{}", record.lastError.message());
        record.context.reset();
        record.registrations = {};
        unlink_dependencies(record);
        mRegistry.remove_load_order(record.id);
        return;
    }

    record.context.reset();
    record.registrations = {};
}

result<void> module_manager::unload_checked(module_record &record) {
    detach_record(record, false, nullptr);
    if (record.state == module_state::FAULTED) {
        return record.lastError;
    }

    close_binary(record);
    const module_id oldId = record.id;
    unlink_dependencies(record);
    mRegistry.remove_load_order(oldId);
    mRegistry.retire_generation(record);
    record.state = module_state::UNLOADED;

    CONJURE_LOG_INFO(LOG_MODULES, "unloaded module '{}'", record.manifest.name);
    mEvents->publish(module_unloaded_event{oldId, record.name});
    return {};
}

void module_manager::collect_reload_cascade(module_record &record, std::vector<module_id> &cascade) const {
    std::vector<module_id> pending = {record.id};
    while (!pending.empty()) {
        const module_id current = pending.back();
        pending.pop_back();
        if (std::find(cascade.begin(), cascade.end(), current) != cascade.end()) {
            continue;
        }
        cascade.push_back(current);
        for (const module_id dependent : mRegistry.dependents(current)) {
            if (mRegistry.is_valid(dependent) && mRegistry.state(dependent) == module_state::LOADED) {
                pending.push_back(dependent);
            }
        }
    }

    // Reverse load order: dependents detach before what they depend on.
    std::sort(cascade.begin(), cascade.end(), [this](module_id left, module_id right) {
        return mRegistry.record(left)->loadOrder > mRegistry.record(right)->loadOrder;
    });
}

result<void> module_manager::reload_full(const std::vector<module_id> &cascade) {
    for (const module_id id : cascade) {
        module_record &record = *mRegistry.record(id);
        std::unique_ptr<reload_context> snapshot = std::make_unique<reload_context>();
        detach_record(record, true, snapshot.get());
        if (record.state == module_state::FAULTED) {
            return record.lastError;
        }
        close_binary(record);
        unlink_dependencies(record);
        mRegistry.remove_load_order(id);
        record.pendingSnapshot = std::move(snapshot);
        record.state = module_state::UNLOADED;
    }

    for (auto iterator = cascade.rbegin(); iterator != cascade.rend(); ++iterator) {
        module_record &record = *mRegistry.record(*iterator);
        CONJURE_TRY(load_one(record));
    }
    return {};
}

result<void> module_manager::reload_managed(const std::vector<module_id> &cascade) {
    // The lifecycle callbacks bracket the managed side exactly as they bracket a FULL reload
    // (07_modules.md 3.5): the module sees its scripts go away and come back.
    for (const module_id id : cascade) {
        module_record &record = *mRegistry.record(id);
        std::unique_ptr<reload_context> snapshot = std::make_unique<reload_context>();
        if (record.registrations.lifecycle.onDeactivating != nullptr) {
            record.registrations.lifecycle.onDeactivating(*record.context);
        }
        mEvents->publish(module_unloading_event{id, true});
        for (auto iterator = mConsumers.rbegin(); iterator != mConsumers.rend(); ++iterator) {
            if ((*iterator)->participates_in_managed_reload()) {
                (*iterator)->detach(id, snapshot.get());
            }
        }
        record.pendingSnapshot = std::move(snapshot);
    }

    for (auto iterator = cascade.rbegin(); iterator != cascade.rend(); ++iterator) {
        module_record &record = *mRegistry.record(*iterator);
        for (module_consumer *consumer : mConsumers) {
            if (!consumer->participates_in_managed_reload()) {
                continue;
            }
            consumer->attach(record.id, *record.context, record.registrations);
            consumer->restore(record.id, *record.pendingSnapshot);
        }
        record.pendingSnapshot.reset();
        if (record.registrations.lifecycle.onActivated != nullptr) {
            record.registrations.lifecycle.onActivated(*record.context);
        }
        mEvents->publish(module_loaded_event{record.id});
    }
    return {};
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods: request queue

void module_manager::enqueue(pending_operation operation) {
    CONJURE_ASSERT(jobs::is_main_thread(), "module_manager::request_* must run on the main thread");
    if (!jobs::is_main_thread()) {
        return;
    }
    mPending.push_back(operation);
    mHasPending.store(true, std::memory_order_relaxed);
}

void module_manager::apply_one(const pending_operation &operation) {
    if (!mRegistry.is_valid(operation.id)) {
        CONJURE_LOG_WARN(
          LOG_MODULES,
          "dropping a pending module request for the stale or invalid id {}",
          operation.id
        );
        return;
    }

    result<void> outcome = {};
    switch (operation.kind) {
        case pending_kind::LOAD:
            outcome = load(operation.id);
            break;
        case pending_kind::UNLOAD:
            outcome = unload(operation.id, operation.policy);
            break;
        case pending_kind::RELOAD:
            outcome = reload(operation.id, operation.scope);
            break;
    }
    if (outcome.has_value()) {
        return;
    }

    // Load failures are already logged and stored by fail_record; refusals of the other kinds are
    // recorded here so the editor's module panel can show them next to the module.
    module_record *record = mRegistry.record(operation.id);
    if (record != nullptr && record->state != module_state::FAILED) {
        record->lastError = outcome.get_error();
        CONJURE_LOG_WARN(
          LOG_MODULES,
          "pending module request for '{}' refused: {}",
          record->manifest.name,
          outcome.get_error().message()
        );
    }
}
