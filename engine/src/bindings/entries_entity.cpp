#include "bindings/entry_support.h"

#include "conjure/core/string_id.h"
#include "conjure/scene/components.h"
#include "conjure/scene/hierarchy.h"
#include "conjure/scene/scene.h"
#include "conjure/scripting/script_runtime.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

// The hierarchy half of entity_* (docs/design/15_bindings.md 2.3, 3.3, R64): names, parents, children,
// enabled state, tags, layers, guids, duplication and bulk creation over scene::scene. Structural requests
// go through the deferred command buffer while a query scope holds the world locked (R5).

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    template <typename T>
    [[nodiscard]] std::span<const std::byte> bytes_of(const T &value) noexcept {
        return std::as_bytes(std::span<const T, 1>(&value, 1));
    }

    [[nodiscard]] bool resolve_parent(
      native_function_id id,
      uint64_t packed,
      ecs::entity &parent,
      int32_t &status
    ) noexcept {
        if (packed == 0) {
            parent = ecs::NULL_ENTITY;
            return true;
        }
        parent = resolve_alive(id, packed, status);
        return !parent.is_null();
    }

    [[nodiscard]] int32_t locked_refused(native_function_id id) noexcept {
        return fail_entry(id, bindings_error::STRUCTURE_LOCKED, "no deferred command buffer");
    }

    /// <summary>
    /// The entity when the scripting runtime reserved it in the open query scope (entity_create,
    /// entity_create_many or a deferred instantiation), NULL_ENTITY otherwise. Such an id is not alive until the
    /// flush, yet the name, tag, layer and enabled setters record their request on it behind the create, as the
    /// core component_* entries do (17_demo_games.md P2).
    /// </summary>
    [[nodiscard]] ecs::entity reserved_in_scope(const marshal::structural_guard &guard, uint64_t packed) noexcept {
        if (!guard.deferred() || !guard.can_defer()) {
            return ecs::NULL_ENTITY;
        }
        const ecs::entity entity = marshal::to_entity(packed);
        return guard.runtime->is_deferred_reserved(entity) ? entity : ecs::NULL_ENTITY;
    }
}

int32_t conjure::bindings::entries::entity_set_name(uint64_t entity, const char *utf8, int32_t length) {
    constexpr native_function_id ID = native_function_id::entity_set_name;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> text = marshal::read_utf8(utf8, length);
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    const marshal::structural_guard reserving = structural();
    const ecs::entity reserved = reserved_in_scope(reserving, entity);
    if (!reserved.is_null()) {
        // Recorded behind the create; the runtime's table answers entity_find_by_name once the flush placed it.
        const scene::name component{text->empty() ? core::string_id::null() : core::string_id(*text)};
        static_cast<void>(
          reserving.runtime->record_reserved_add(reserved, active_scene()->ids().name, bytes_of(component))
        );
        reserving.runtime->set_entity_name(reserved, *text);
        return 0;
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    const core::string_id name = text->empty() ? core::string_id::null() : core::string_id(*text);
    scene::scene &active = *active_scene();
    if (active.world().has<scene::name>(target)) {
        // Non-structural: the component exists, the write stamps its column.
        active.world().get<scene::name>(target)->value = name;
    } else {
        marshal::structural_guard guard = structural();
        if (guard.deferred()) {
            if (!guard.can_defer()) {
                return locked_refused(ID);
            }
            const scene::name component{name};
            guard.commands().add_raw(target, active.ids().name, bytes_of(component));
        } else {
            const core::result<void> named = active.set_name(target, name);
            if (!named.has_value()) {
                return fail_entry(ID, named.get_error());
            }
        }
    }
    // The core entity_get_name / entity_find_by_name entries read the runtime's table: keep both in step.
    if (active_context().scripting != nullptr && active_context().scripting->world() == &active.world()) {
        active_context().scripting->set_entity_name(target, *text);
    }
    return 0;
}

uint64_t conjure::bindings::entries::entity_get_parent(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::entity_get_parent;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return 0;
    }
    return marshal::from_entity(active_scene()->tree().parent_of(target));
}

uint64_t conjure::bindings::entries::entity_get_root(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::entity_get_root;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return 0;
    }
    return marshal::from_entity(active_scene()->tree().root_of(target));
}

int32_t conjure::bindings::entries::entity_get_children(uint64_t entity, uint64_t *out, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::entity_get_children;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    return marshal::write_entities(active_scene()->tree().children_of(target), out, capacity);
}

int32_t conjure::bindings::entries::entity_set_enabled(uint64_t entity, int32_t enabled) {
    constexpr native_function_id ID = native_function_id::entity_set_enabled;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const ecs::entity reserved = reserved_in_scope(structural(), entity);
    if (!reserved.is_null()) {
        // Applied after the flush that places the entity, like any deferred enable (skipped if a destroy won).
        scene_fixup fixup = {};
        fixup.operation = scene_fixup::kind::SET_ENABLED;
        fixup.entity = reserved;
        fixup.enabled = enabled != 0;
        queue_fixup(fixup);
        return 0;
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    scene::scene &active = *active_scene();
    const bool wanted = enabled != 0;
    marshal::structural_guard guard = structural();
    if (guard.deferred()) {
        if (!guard.can_defer()) {
            // A request that changes nothing needs no buffer: it succeeds as it does on the unlocked path.
            return active.is_enabled(target) == wanted ? 0 : locked_refused(ID);
        }
        // Always queued: the live state cannot change while the scope is open, so comparing against it would drop
        // the second request of a disable-then-enable pair. Applied in order once the world unlocks, where
        // scene::set_enabled ignores an unchanged state and owns the descendants' inactive tags.
        scene_fixup fixup = {};
        fixup.operation = scene_fixup::kind::SET_ENABLED;
        fixup.entity = target;
        fixup.enabled = wanted;
        queue_fixup(fixup);
        return 0;
    }
    if (active.is_enabled(target) == wanted) {
        return 0;
    }
    const core::result<void> toggled = active.set_enabled(target, wanted);
    if (!toggled.has_value()) {
        return fail_entry(ID, toggled.get_error());
    }
    return 0;
}

int32_t conjure::bindings::entries::entity_is_enabled(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::entity_is_enabled;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    return active_scene()->is_enabled(target) ? 1 : 0;
}

uint64_t conjure::bindings::entries::entity_get_tag(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::entity_get_tag;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return 0;
    }
    return active_scene()->tag_of(target).hash();
}

int32_t conjure::bindings::entries::entity_set_tag(uint64_t entity, const char *utf8, int32_t length) {
    constexpr native_function_id ID = native_function_id::entity_set_tag;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> text = marshal::read_utf8(utf8, length);
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    const marshal::structural_guard reserving = structural();
    const ecs::entity reserved = reserved_in_scope(reserving, entity);
    if (!reserved.is_null()) {
        // An add overwrites at the flush, so the last tag set in the scope wins; a null tag reads as none.
        const scene::tag component{text->empty() ? core::string_id::null() : core::string_id(*text)};
        static_cast<void>(
          reserving.runtime->record_reserved_add(reserved, active_scene()->ids().tag, bytes_of(component))
        );
        return 0;
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    const core::string_id tag = text->empty() ? core::string_id::null() : core::string_id(*text);
    scene::scene &active = *active_scene();
    if (active.world().has<scene::tag>(target)) {
        active.world().get<scene::tag>(target)->value = tag;
        return 0;
    }
    marshal::structural_guard guard = structural();
    if (guard.deferred()) {
        if (!guard.can_defer()) {
            return locked_refused(ID);
        }
        const scene::tag component{tag};
        guard.commands().add_raw(target, active.ids().tag, bytes_of(component));
        return 0;
    }
    const core::result<void> tagged = active.set_tag(target, tag);
    if (!tagged.has_value()) {
        return fail_entry(ID, tagged.get_error());
    }
    return 0;
}

int32_t conjure::bindings::entries::entity_get_tag_text(uint64_t entity, char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::entity_get_tag_text;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    const core::string_id tag = active_scene()->tag_of(target);
    if (tag.is_null()) {
        return 0;
    }
    // entity_set_tag and a scene file that spells the tag intern the text. A hash without a spelling (a C#
    // StringId.Of mirror write, a scene value stored as "#<hex>" or as a number) has none to return, and that
    // must not read as "no tag": the caller still has the hash through entity_get_tag.
    const std::string_view text = tag.view();
    if (text.empty()) {
        return fail_entry(ID, bindings_error::NOT_FOUND, "the tag hash has no interned text");
    }
    return marshal::write_utf8(text, utf8, capacity);
}

int32_t conjure::bindings::entries::entity_get_layer(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::entity_get_layer;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    return static_cast<int32_t>(active_scene()->layer_of(target));
}

int32_t conjure::bindings::entries::entity_set_layer(uint64_t entity, int32_t layer) {
    constexpr native_function_id ID = native_function_id::entity_set_layer;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (layer < 0 || layer >= static_cast<int32_t>(scene::layer_table::LAYER_COUNT)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "layer must be 0..31");
    }
    const u8 index = static_cast<u8>(layer);
    const marshal::structural_guard reserving = structural();
    const ecs::entity reserved = reserved_in_scope(reserving, entity);
    if (!reserved.is_null()) {
        const scene::layer component{index};
        static_cast<void>(
          reserving.runtime->record_reserved_add(reserved, active_scene()->ids().layer, bytes_of(component))
        );
        return 0;
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    scene::scene &active = *active_scene();
    if (active.world().has<scene::layer>(target)) {
        active.world().get<scene::layer>(target)->index = index;
        return 0;
    }
    marshal::structural_guard guard = structural();
    if (guard.deferred()) {
        if (!guard.can_defer()) {
            return locked_refused(ID);
        }
        const scene::layer component{index};
        guard.commands().add_raw(target, active.ids().layer, bytes_of(component));
        return 0;
    }
    const core::result<void> layered = active.set_layer(target, index);
    if (!layered.has_value()) {
        return fail_entry(ID, layered.get_error());
    }
    return 0;
}

int32_t conjure::bindings::entries::entity_get_guid(uint64_t entity, conjure_uuid *out) {
    constexpr native_function_id ID = native_function_id::entity_get_guid;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    // A read (2.3): an entity without a guid reports COMPONENT_MISSING instead of growing one here, so the
    // getter has no structural side effect and answers the same inside and outside a query scope. Guids come
    // from scene::create_entity, the serializer and scene::ensure_guid (editor and tools).
    const std::optional<core::uuid> existing = active_scene()->guid_of(target);
    if (!existing.has_value()) {
        return fail_entry(ID, bindings_error::COMPONENT_MISSING, "guid");
    }
    *out = marshal::from_uuid(*existing);
    return 0;
}

uint64_t conjure::bindings::entries::entity_find_by_guid(const conjure_uuid *guid) {
    constexpr native_function_id ID = native_function_id::entity_find_by_guid;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    if (guid == nullptr) {
        static_cast<void>(fail_entry(ID, bindings_error::INVALID_ARGUMENT, "guid"));
        return 0;
    }
    if (active_scene() == nullptr) {
        static_cast<void>(unavailable(ID, "scene"));
        return 0;
    }
    return marshal::from_entity(active_scene()->find_guid(marshal::to_uuid(*guid)));
}

int32_t conjure::bindings::entries::entity_find_by_tag(
  const char *utf8,
  int32_t length,
  uint64_t *out,
  int32_t capacity
) {
    constexpr native_function_id ID = native_function_id::entity_find_by_tag;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> text = marshal::read_utf8(utf8, length);
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (active_scene() == nullptr) {
        return unavailable(ID, "scene");
    }
    // A lookup hashes only: interning every string a script ever queries (typos and generated names included)
    // would grow the process-wide table without bound. Only entity_set_tag interns, where tools must see it.
    const core::string_id tag =
      text->empty() ? core::string_id::null() : core::string_id::from_hash(core::hash::fnv1a64(*text));
    std::vector<ecs::entity> &found = scratch_entities();
    active_scene()->find_by_tag(tag, found);
    return marshal::write_entities(found, out, capacity);
}

int32_t conjure::bindings::entries::entity_find_by_layer(int32_t layer, uint64_t *out, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::entity_find_by_layer;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (layer < 0 || layer >= static_cast<int32_t>(scene::layer_table::LAYER_COUNT)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "layer must be 0..31");
    }
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (active_scene() == nullptr) {
        return unavailable(ID, "scene");
    }
    std::vector<ecs::entity> &found = scratch_entities();
    active_scene()->find_by_layer(static_cast<u8>(layer), found);
    return marshal::write_entities(found, out, capacity);
}

uint64_t conjure::bindings::entries::entity_duplicate(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::entity_duplicate;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    int32_t status = 0;
    const ecs::entity source = resolve_alive(ID, entity, status);
    if (source.is_null()) {
        return 0;
    }
    scene::scene &active = *active_scene();
    if (structural().deferred()) {
        // Capturing a subtree is itself structural (entity_archive::capture refuses a locked world), so a
        // duplicate has no command form: the one request of the table that reports STRUCTURE_LOCKED (B7).
        static_cast<void>(
          fail_entry(ID, bindings_error::STRUCTURE_LOCKED, "duplicate has no command form inside a query scope")
        );
        return 0;
    }
    const core::result<ecs::entity> copy = active.duplicate_subtree(source);
    if (!copy.has_value()) {
        static_cast<void>(fail_entry(ID, copy.get_error()));
        return 0;
    }
    return marshal::from_entity(copy.value());
}

int32_t conjure::bindings::entries::entity_create_many(int32_t count, uint64_t parent, uint64_t *out) {
    constexpr native_function_id ID = native_function_id::entity_create_many;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (count < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "count");
    }
    if (count == 0) {
        return 0;
    }
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    if (active_scene() == nullptr) {
        return unavailable(ID, "scene");
    }
    int32_t status = 0;
    ecs::entity parentEntity = ecs::NULL_ENTITY;
    if (!resolve_parent(ID, parent, parentEntity, status)) {
        return status;
    }
    scene::scene &active = *active_scene();
    marshal::structural_guard guard = structural();
    if (guard.deferred()) {
        if (!guard.can_defer()) {
            return locked_refused(ID);
        }
        // The ids are real now; the entities exist at the flush and the scene adopts them (roots or children
        // of 'parent', companions from the local_transform observer) through the fixups of query_end. The
        // runtime remembers each id, so the core component_* / script_add / entity_destroy entries record their
        // requests on it behind the create (17_demo_games.md P2).
        ecs::command_buffer &commands = guard.commands();
        const std::array<ecs::component_id, 1> recorded = {active.ids().localTransform};
        for (int32_t index = 0; index < count; ++index) {
            const ecs::entity created = commands.create_entity();
            if (created.is_null()) {
                return fail_entry(ID, bindings_error::INTERNAL, "entity id space exhausted");
            }
            commands.add_raw(created, active.ids().localTransform);
            guard.runtime->note_deferred_reserved(created, recorded);
            scene_fixup fixup = {};
            fixup.operation = scene_fixup::kind::ATTACH;
            fixup.entity = created;
            fixup.parent = parentEntity;
            fixup.index = scene::APPEND_CHILD;
            queue_fixup(fixup);
            out[index] = marshal::from_entity(created);
        }
        return count;
    }
    std::vector<ecs::entity> &roots = scratch_entities();
    roots.resize(static_cast<usize>(count));
    const core::result<scene::instantiate_report> report =
      active.create_entities(static_cast<u32>(count), ecs::NULL_ENTITY, parentEntity, roots);
    if (!report.has_value()) {
        return fail_entry(ID, report.get_error());
    }
    for (int32_t index = 0; index < count; ++index) {
        out[index] = marshal::from_entity(roots[static_cast<usize>(index)]);
    }
    return count;
}
