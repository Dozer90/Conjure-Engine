#include "conjure/scripting/native_api.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/world.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The core-owned entries of the native table (docs/design/03_scripting.md 2.1, R26/R64): log_*,
// script_*, the five entity_* that need only ecs, component_* and query_*. Every function acts on
// script_runtime::active(); with no runtime alive they fail softly (0 / -NOT_INITIALIZED). Every entry
// but log_write is main-thread only (D10, 5): off the main thread it asserts and fails the same way.

using namespace conjure;
using namespace conjure::scripting;

namespace {

    [[nodiscard]] std::string_view view_of(const char *utf8, int32_t length) noexcept {
        if (utf8 == nullptr || length <= 0) {
            return {};
        }
        return std::string_view(utf8, static_cast<usize>(length));
    }

    /// <summary>
    /// The active runtime when called on the main thread; nullptr (after an assert) elsewhere. The
    /// managed guard throws before reaching native, so only a raw function pointer gets this far.
    /// </summary>
    [[nodiscard]] script_runtime *active_runtime() noexcept {
        const bool mainThread = core::jobs::is_main_thread();
        CONJURE_ASSERT(mainThread, "native scripting entry called off the main thread (03_scripting.md D10)");
        if (!mainThread) {
            return nullptr;
        }
        return script_runtime::active();
    }

    [[nodiscard]] script_runtime *runtime_with_world() noexcept {
        script_runtime *runtime = active_runtime();
        if (runtime == nullptr || runtime->world() == nullptr) {
            return nullptr;
        }
        return runtime;
    }

    /// <summary>
    /// True when a registered dynamic type already has exactly the requested size, alignment and fields.
    /// </summary>
    [[nodiscard]] bool same_dynamic_layout(const ecs::component_info &info, const ecs::dynamic_component_desc &desc) {
        if (info.size != desc.size || info.alignment != desc.alignment || info.reflection == nullptr) {
            return false;
        }
        const std::vector<core::reflect::field_info> &fields = info.reflection->fields;
        if (fields.size() != desc.fields.size()) {
            return false;
        }
        for (usize index = 0; index < fields.size(); ++index) {
            const ecs::dynamic_field_desc &requested = desc.fields[index];
            if (fields[index].name != requested.name.to_string() || fields[index].type != requested.type ||
              fields[index].offset != requested.offset) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] ecs::component_id component_of(uint32_t typeId, const ecs::component_registry &registry) noexcept {
        if (typeId == 0 || typeId >= ecs::MAX_COMPONENTS) {
            return {};
        }
        const ecs::component_id id{static_cast<u16>(typeId)};
        return registry.is_registered(id) ? id : ecs::component_id{};
    }

    [[nodiscard]] core::json::document parse_or_empty(const char *utf8, int32_t length) {
        const std::string_view text = view_of(utf8, length);
        if (text.empty()) {
            return core::json::document();
        }
        core::result<core::json::document> parsed = core::json::document::parse(text);
        if (!parsed.has_value()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "managed sent malformed JSON '{}': {}", text, parsed.get_error());
            return core::json::document();
        }
        return std::move(parsed).value();
    }

    /*---------------------------------------------------------------*/
    // log_*

    void entry_log_write(uint8_t level, const char *utf8, int32_t length) {
        const std::string_view text = view_of(utf8, length);
        core::log_level mapped = core::log_level::INFO;
        switch (level) {
            case 0:
                mapped = core::log_level::TRACE;
                break;
            case 1:
                mapped = core::log_level::DEBUG;
                break;
            case 2:
                mapped = core::log_level::INFO;
                break;
            case 3:
                mapped = core::log_level::WARN;
                break;
            case 4:
                mapped = core::log_level::ERR;
                break;
            default:
                mapped = core::log_level::FATAL;
                break;
        }
        if (core::log::is_enabled(LOG_SCRIPTING, mapped)) {
            core::log::write(LOG_SCRIPTING, mapped, "managed", 0, text);
        }
    }

    /*---------------------------------------------------------------*/
    // script_*

    void entry_script_register_class(const conjure_script_class_desc *desc) {
        script_runtime *runtime = active_runtime();
        if (runtime == nullptr || desc == nullptr) {
            return;
        }
        script_class_registry &classes = runtime->classes_mutable();
        if (!classes.is_staging_open()) {
            // Only an assembly load opens, commits or discards the staging table (03_scripting.md 3.7): a
            // class registered at any other time would be staged into a table nothing ever commits.
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "script_register_class: '{}' ignored outside an assembly load",
              view_of(desc->full_name, desc->name_length)
            );
            return;
        }
        script_class_info info = {};
        info.hash = desc->hash;
        info.fullName = std::string(view_of(desc->full_name, desc->name_length));
        info.callbackMask = desc->callback_mask;
        info.fields.reserve(desc->field_count);
        const core::result<void> staged = classes.stage_class(std::move(info));
        if (!staged.has_value()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "script_register_class: {}", staged.get_error());
        }
    }

    void entry_script_register_field(uint64_t classHash, const conjure_script_field_desc *desc) {
        script_runtime *runtime = active_runtime();
        if (runtime == nullptr || desc == nullptr) {
            return;
        }
        if (desc->kind >= static_cast<uint8_t>(script_field_kind::COUNT) ||
          desc->element_kind >= static_cast<uint8_t>(script_field_kind::COUNT)) {
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "script_register_field: field '{}' of {:#018x} has an unknown kind {}",
              view_of(desc->name, desc->name_length),
              classHash,
              desc->kind
            );
            return;
        }
        script_field_info field = {};
        field.hash = desc->hash;
        field.name = std::string(view_of(desc->name, desc->name_length));
        field.kind = static_cast<script_field_kind>(desc->kind);
        field.elementKind = static_cast<script_field_kind>(desc->element_kind);
        field.defaultValue = parse_or_empty(desc->default_json, desc->default_length);
        field.meta = parse_or_empty(desc->meta_json, desc->meta_length);
        const core::result<void> staged = runtime->classes_mutable().stage_field(classHash, std::move(field));
        if (!staged.has_value()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "script_register_field: {}", staged.get_error());
        }
    }

    void entry_script_mark_faulted(int32_t slot, uint8_t callback) {
        script_runtime *runtime = active_runtime();
        if (runtime == nullptr || callback >= static_cast<uint8_t>(script_callback::COUNT)) {
            return;
        }
        runtime->mark_faulted(slot, static_cast<script_callback>(callback));
    }

    int32_t entry_script_add(uint64_t entity, uint64_t classHash) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return status_of(script_error::NOT_INITIALIZED);
        }
        const script_entry_id id = runtime->add_script(ecs::entity::from_u64(entity), classHash, core::json::value());
        if (!id.is_valid()) {
            return status_of(script_error::INSTANCE_NOT_FOUND);
        }
        return static_cast<int32_t>(id.entryIndex);
    }

    int32_t entry_script_get_slots(uint64_t entity, int32_t *slots, int32_t capacity) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return status_of(script_error::NOT_INITIALIZED);
        }
        // The set is read in place: every GetScript<T>() lands here, and scripts_of() would build a vector.
        const ecs::world &world = *runtime->world();
        const script_store &store = std::as_const(*runtime).store();
        const script_component *component = world.get<script_component>(ecs::entity::from_u64(entity));
        const script_set *set = component == nullptr ? nullptr : store.find(component->setHandle);
        if (set == nullptr) {
            return 0;
        }
        int32_t count = 0;
        for (const script_entry &entryValue : set->entries) {
            if (!entryValue.is_live()) {
                continue;
            }
            if (slots != nullptr && count < capacity) {
                slots[count] = entryValue.managedSlot;
            }
            ++count;
        }
        return count;
    }

    /*---------------------------------------------------------------*/
    // entity_*

    uint64_t entry_entity_create(const char *utf8Name, int32_t length) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        ecs::world &world = *runtime->world();
        ecs::entity created = {};
        if (world.is_locked()) {
            // Inside a DOD query scope the id is reserved now and the entity exists at query_end (R5).
            created = runtime->deferred_commands().create_entity();
        } else {
            core::result<ecs::entity> result = world.create_entity();
            if (!result.has_value()) {
                CONJURE_LOG_WARN(LOG_SCRIPTING, "entity_create: {}", result.get_error());
                return 0;
            }
            created = result.value();
        }
        if (created.is_null()) {
            return 0;
        }
        runtime->set_entity_name(created, view_of(utf8Name, length));
        return created.to_u64();
    }

    int32_t entry_entity_destroy(uint64_t entity) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        const ecs::entity target = ecs::entity::from_u64(entity);
        if (!runtime->world()->is_alive(target)) {
            return 0;
        }
        runtime->request_entity_destroy(target);
        return 1;
    }

    int32_t entry_entity_is_alive(uint64_t entity) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        return runtime->world()->is_alive(ecs::entity::from_u64(entity)) ? 1 : 0;
    }

    int32_t entry_entity_get_name(uint64_t entity, char *utf8, int32_t capacity) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        const std::string_view name = runtime->entity_name(ecs::entity::from_u64(entity));
        const int32_t needed = static_cast<int32_t>(name.size());
        if (utf8 != nullptr && needed <= capacity && needed > 0) {
            std::memcpy(utf8, name.data(), name.size());
        }
        return needed;
    }

    uint64_t entry_entity_find_by_name(const char *utf8, int32_t length) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        return runtime->find_entity_by_name(view_of(utf8, length)).to_u64();
    }

    /*---------------------------------------------------------------*/
    // component_*

    uint32_t entry_component_lookup_type(uint64_t nameHash, uint32_t size, uint64_t layoutHash) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        const ecs::component_registry &registry = runtime->world()->registry();
        const ecs::component_id id = registry.find(core::string_id::from_hash(nameHash));
        if (!id.is_valid() || id.value == 0) {
            if (runtime->note_mirror_mismatch(nameHash)) {
                CONJURE_LOG_WARN(LOG_SCRIPTING, "component_lookup_type: no component named {:#018x}", nameHash);
            }
            return 0;
        }
        const ecs::component_info &info = registry.info(id);
        const u64 nativeHash = info.reflection == nullptr ? 0 : info.reflection->layout_hash();
        if (info.size != size || nativeHash != layoutHash) {
            if (runtime->note_mirror_mismatch(nameHash)) {
                CONJURE_LOG_ERROR(
                  LOG_SCRIPTING,
                  "component_lookup_type: mirror of '{}' does not match (size {} vs {}, layout {:#018x} vs {:#018x})",
                  info.name,
                  size,
                  info.size,
                  layoutHash,
                  nativeHash
                );
            }
            return 0;
        }
        return id.value;
    }

    uint32_t entry_component_register_managed(const conjure_managed_component_desc *desc) {
        script_runtime *runtime = active_runtime();
        if (runtime == nullptr || desc == nullptr) {
            return 0;
        }
        ecs::component_registry *registry = runtime->registry();
        if (registry == nullptr) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "component_register_managed without an attached component_registry");
            return 0;
        }
        const std::string fullName(view_of(desc->full_name, desc->name_length));
        const core::json::document schema = parse_or_empty(desc->schema_json, desc->schema_length);
        const core::json::value fields = schema.root()["fields"];
        if (fullName.empty() || !fields.is_array()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "component_register_managed: '{}' has no field schema", fullName);
            return 0;
        }

        std::vector<ecs::dynamic_field_desc> fieldDescs = {};
        for (const core::json::value field : fields) {
            ecs::dynamic_field_desc fieldDesc = {};
            fieldDesc.name = core::string_id(field["name"].as_string());
            fieldDesc.type = core::reflect::type_id::from_name(field["type"].as_string());
            fieldDesc.offset = static_cast<u32>(field["offset"].as_u64());
            fieldDescs.push_back(fieldDesc);
        }

        ecs::dynamic_component_desc dynamic = {};
        dynamic.name = core::string_id(fullName);
        dynamic.size = desc->size;
        dynamic.alignment = desc->alignment;
        dynamic.fields = fieldDescs;

        core::result<ecs::component_id> registered = registry->register_dynamic(dynamic);
        if (registered.has_value()) {
            return registered.value().value;
        }
        if (!registered.get_error().is(ecs::ecs_error::NAME_CONFLICT)) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "component_register_managed('{}'): {}", fullName, registered.get_error());
            return 0;
        }

        // Only a dynamic type may be redefined (R4): a game struct whose full name collides with a native
        // component is refused before anything is touched, and an unchanged layout is simply the same id.
        const ecs::component_id existing = registry->find(dynamic.name);
        if (!existing.is_valid() || existing.value == 0) {
            return 0;
        }
        const ecs::component_info &existingInfo = registry->info(existing);
        if (!has_flag(existingInfo.flags, ecs::component_flags::DYNAMIC)) {
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "[EcsComponent] '{}' has the name of the native component '{}'; not registered",
              fullName,
              existingInfo.name
            );
            return 0;
        }
        if (same_dynamic_layout(existingInfo, dynamic)) {
            return existing.value;
        }

        // The layout changed across a reload (R4): remove_from_all -> redefine_dynamic -> add_raw(zero bytes)
        // is how "reset to zero" is implemented; without instances redefine_dynamic alone suffices.
        std::vector<ecs::entity> holders = {};
        ecs::world *world = runtime->world();
        if (world != nullptr && registry->has_instances(existing)) {
            ecs::query_desc query = {};
            query.with.set(existing);
            world->create_query(query).for_each_chunk([&holders](ecs::chunk_view &view) {
                const std::span<const ecs::entity> entities = view.entities();
                holders.insert(holders.end(), entities.begin(), entities.end());
            });
            const core::result<void> removed = world->remove_from_all(existing);
            if (!removed.has_value()) {
                CONJURE_LOG_WARN(LOG_SCRIPTING, "component_register_managed('{}'): {}", fullName, removed.get_error());
                return 0;
            }
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "[EcsComponent] '{}' changed layout with {} live instance(s); their data is reset to zero",
              fullName,
              holders.size()
            );
        }
        const core::result<void> redefined = registry->redefine_dynamic(existing, dynamic);
        if (!redefined.has_value()) {
            CONJURE_LOG_WARN(LOG_SCRIPTING, "component_register_managed('{}'): {}", fullName, redefined.get_error());
            return 0;
        }
        for (const ecs::entity holder : holders) {
            static_cast<void>(world->add_raw(holder, existing));
        }
        return existing.value;
    }

    int32_t entry_component_has(uint64_t entity, uint32_t typeId) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        ecs::world &world = *runtime->world();
        const ecs::component_id id = component_of(typeId, world.registry());
        return id.is_valid() && world.has_raw(ecs::entity::from_u64(entity), id) ? 1 : 0;
    }

    int32_t entry_component_add(uint64_t entity, uint32_t typeId, const void *data) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        ecs::world &world = *runtime->world();
        const ecs::component_id id = component_of(typeId, world.registry());
        const ecs::entity target = ecs::entity::from_u64(entity);
        if (!id.is_valid() || !world.is_alive(target)) {
            return 0;
        }
        const ecs::component_info &info = world.registry().info(id);
        const std::span<const std::byte> bytes = data == nullptr
          ? std::span<const std::byte>()
          : std::span<const std::byte>(static_cast<const std::byte *>(data), info.size);
        if (world.is_locked()) {
            runtime->deferred_commands().add_raw(target, id, bytes);
            return 1;
        }
        return world.add_raw(target, id, bytes).has_value() ? 1 : 0;
    }

    int32_t entry_component_remove(uint64_t entity, uint32_t typeId) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr) {
            return 0;
        }
        ecs::world &world = *runtime->world();
        const ecs::component_id id = component_of(typeId, world.registry());
        const ecs::entity target = ecs::entity::from_u64(entity);
        if (!id.is_valid() || !world.has_raw(target, id)) {
            return 0;
        }
        if (world.is_locked()) {
            runtime->deferred_commands().remove_raw(target, id);
            return 1;
        }
        return world.remove_raw(target, id).has_value() ? 1 : 0;
    }

    int32_t entry_component_get(uint64_t entity, uint32_t typeId, void *out) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr || out == nullptr) {
            return 0;
        }
        const ecs::world &world = *runtime->world();
        const ecs::component_id id = component_of(typeId, world.registry());
        if (!id.is_valid()) {
            return 0;
        }
        const ecs::entity target = ecs::entity::from_u64(entity);
        if (world.registry().info(id).is_tag()) {
            // A tag has no column to read (get_raw answers nullptr): held is all there is to report.
            return world.has_raw(target, id) ? 1 : 0;
        }
        const void *data = world.get_raw(target, id);
        if (data == nullptr) {
            return 0;
        }
        std::memcpy(out, data, world.registry().info(id).size);
        return 1;
    }

    int32_t entry_component_set(uint64_t entity, uint32_t typeId, const void *data) {
        script_runtime *runtime = runtime_with_world();
        if (runtime == nullptr || data == nullptr) {
            return 0;
        }
        ecs::world &world = *runtime->world();
        const ecs::component_id id = component_of(typeId, world.registry());
        if (!id.is_valid()) {
            return 0;
        }
        const ecs::entity owner = ecs::entity::from_u64(entity);
        if (world.registry().info(id).is_tag()) {
            // Nothing to write into a tag: setting one the entity holds succeeds, as C# SetComponent expects.
            return world.has_raw(owner, id) ? 1 : 0;
        }
        void *target = world.get_raw(owner, id, true);
        if (target == nullptr) {
            return 0;
        }
        std::memcpy(target, data, world.registry().info(id).size);
        return 1;
    }

    /*---------------------------------------------------------------*/
    // query_*

    int32_t entry_query_begin(const uint32_t *ids, int32_t count, conjure_query_scope *out) {
        script_runtime *runtime = active_runtime();
        if (runtime == nullptr) {
            return status_of(script_error::NOT_INITIALIZED);
        }
        if (ids == nullptr || out == nullptr || count <= 0) {
            return status_of(script_error::NOT_SUPPORTED);
        }
        const std::span<const u32> typeIds(ids, static_cast<usize>(count));
        const core::result<u32> opened = runtime->begin_query_scope(typeIds, *out);
        if (opened.has_value()) {
            return 0;
        }
        const core::error &failure = opened.get_error();
        if (failure.is_category<script_error>()) {
            return -static_cast<int32_t>(failure.code());
        }
        CONJURE_LOG_WARN(LOG_SCRIPTING, "query_begin: {}", failure);
        return status_of(script_error::NOT_SUPPORTED);
    }

    void entry_query_end(uint32_t scopeId) {
        script_runtime *runtime = active_runtime();
        if (runtime != nullptr) {
            runtime->end_query_scope(scopeId);
        }
    }
}

void conjure::scripting::fill_core_native_entries(conjure_native_api &api) noexcept {
    api.log_write = &entry_log_write;
    api.script_register_class = &entry_script_register_class;
    api.script_register_field = &entry_script_register_field;
    api.script_mark_faulted = &entry_script_mark_faulted;
    api.script_add = &entry_script_add;
    api.script_get_slots = &entry_script_get_slots;
    api.entity_create = &entry_entity_create;
    api.entity_destroy = &entry_entity_destroy;
    api.entity_is_alive = &entry_entity_is_alive;
    api.entity_get_name = &entry_entity_get_name;
    api.entity_find_by_name = &entry_entity_find_by_name;
    api.component_lookup_type = &entry_component_lookup_type;
    api.component_register_managed = &entry_component_register_managed;
    api.component_has = &entry_component_has;
    api.component_add = &entry_component_add;
    api.component_remove = &entry_component_remove;
    api.component_get = &entry_component_get;
    api.component_set = &entry_component_set;
    api.query_begin = &entry_query_begin;
    api.query_end = &entry_query_end;
}
