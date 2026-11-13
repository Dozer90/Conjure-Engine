#include "conjure/scene/scene.h"

#include "conjure/scene/log_category.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scene/transform_system.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"
#include "conjure/ecs/archetype.h"
#include "conjure/ecs/view.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>

namespace conjure::scene {

using name_component = ::conjure::scene::name;

namespace {

    constexpr usize NO_ROOT = std::numeric_limits<usize>::max();

    [[nodiscard]] core::error not_alive(std::string_view what, ecs::entity target) {
        return core::make_error(scene_error::INVALID_ENTITY, "{}: entity {} is not alive", what, target);
    }

    [[nodiscard]] std::span<const std::byte> bytes_of(const parent &value) noexcept {
        return std::span<const std::byte>(reinterpret_cast<const std::byte *>(&value), sizeof(parent));
    }

    [[nodiscard]] core::error violation(std::string message) {
        return core::error(core::error_code::INVALID_STATE, std::move(message));
    }
}

/*===========================================================================*/
/*===========================================================================*/
// operation_scope

scene::operation_scope::operation_scope(scene &owner) noexcept :
    mOwner(&owner),
    mPrevious(owner.mInOperation) {
    owner.mInOperation = true;
}

scene::operation_scope::~operation_scope() {
    mOwner->mInOperation = mPrevious;
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

scene::scene(ecs::world &world, assets::asset_manager *assets) :
    mWorld(&world),
    mAssets(assets),
    mIds(scene_component_ids::resolve(world.registry())),
    mTree(world, mIds),
    mScratch(world) {
    CONJURE_ASSERT(mIds.is_complete(), "scene components are not registered; call scene::register_components first");
    register_observers();
}

scene::~scene() {
    for (const ecs::observer_id id : mObservers) {
        mWorld->unobserve(id);
    }
    mScratch.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// Access

void scene::set_name(std::string_view value) {
    mName = std::string(value);
}

void scene::set_settings(core::json::document value) {
    if (!value.root().is_object()) {
        value = core::json::document::object();
    }
    mSettings = std::move(value);
    mOnSettingsChanged.emit();
}


/*===========================================================================*/
/*===========================================================================*/
// Entities

core::result<ecs::entity> scene::create_entity(core::string_id entityName, ecs::entity parentEntity, u32 index) {
    CONJURE_TRY_ASSIGN(const ecs::entity created, create_entity_unannounced(entityName, parentEntity, index));
    bump_structure();
    return created;
}

core::result<instantiate_report> scene::create_entities(
  u32 count,
  ecs::entity templateEntity,
  ecs::entity parentEntity,
  std::span<ecs::entity> out
) {
    if (out.size() < count) {
        return core::error(core::error_code::INVALID_ARGUMENT, "create_entities needs one output slot per entity");
    }
    if (templateEntity.is_null()) {
        instantiate_report report = {};
        report.entities.reserve(count);
        for (u32 index = 0; index < count; ++index) {
            const core::result<ecs::entity> created =
              create_entity_unannounced(core::string_id(), parentEntity, APPEND_CHILD);
            if (!created.has_value()) {
                // Atomic like the template path: nothing this call created outlives the failure, and the
                // net change is none, so nothing is announced.
                for (u32 undo = index; undo > 0; --undo) {
                    const core::result<void> destroyed = destroy_unannounced(out[undo - 1]);
                    CONJURE_UNUSED(destroyed);
                }
                std::fill(out.begin(), out.begin() + index, ecs::NULL_ENTITY);
                return created.get_error();
            }
            out[index] = created.value();
            report.entities.push_back(created.value());
        }
        if (count > 0) {
            bump_structure();
        }
        report.entityCount = count;
        return report;
    }
    if (!mWorld->is_alive(templateEntity)) {
        return not_alive("create_entities", templateEntity);
    }
    CONJURE_TRY_ASSIGN(ecs::entity_archive captured, scene_serializer::capture_subtree(*this, templateEntity));
    const std::shared_ptr<const ecs::entity_archive> archive =
      std::make_shared<const ecs::entity_archive>(std::move(captured));
    instantiate_options options = {};
    options.parent = parentEntity;
    options.guids = guid_policy::REGENERATE_ALL;
    return instantiate_archive(archive, count, options, out);
}

core::result<ecs::entity> scene::duplicate_subtree(ecs::entity root, ecs::entity parentEntity, u32 index) {
    if (!mWorld->is_alive(root)) {
        return not_alive("duplicate_subtree", root);
    }
    CONJURE_TRY_ASSIGN(ecs::entity_archive captured, scene_serializer::capture_subtree(*this, root));
    const std::shared_ptr<const ecs::entity_archive> archive =
      std::make_shared<const ecs::entity_archive>(std::move(captured));
    instantiate_options options = {};
    options.parent = parentEntity.is_null() ? mTree.parent_of(root) : parentEntity;
    options.index = index;
    options.guids = guid_policy::REGENERATE_ALL;
    if (options.parent.is_null()) {
        options.owner = owner_of_tree(root);
    }
    ecs::entity created = {};
    CONJURE_TRY(instantiate_archive(archive, 1, options, std::span<ecs::entity>(&created, 1)));
    return created;
}

void scene::destroy(ecs::entity target) {
    if (!mWorld->is_alive(target)) {
        return;
    }
    // No duplicate scan: destroy_pending skips entities that died with an earlier entry, so N script
    // driven destroys in one frame stay O(N).
    mDestroyQueue.push_back(target);
}

core::result<void> scene::destroy_now(ecs::entity target) {
    CONJURE_TRY(destroy_unannounced(target));
    bump_structure();
    return core::result<void>::success();
}

void scene::clear() {
    operation_scope operation(*this);
    mScratchEntities.clear();
    for (const root_entry &entry : mRoots) {
        mScratchEntities.push_back(entry.entity);
    }
    for (const ecs::entity root : mScratchEntities) {
        if (mWorld->is_alive(root)) {
            const core::result<void> destroyed = mWorld->destroy_entity(root);
            CONJURE_UNUSED(destroyed);
        }
    }
    if (mWorld->entity_count() > 0) {
        mWorld->clear();
    }
    mRoots.clear();
    mGuids.clear();
    mDestroyQueue.clear();
    mRepairQueue.clear();
    mRepairSpare.clear();
    mScratchEntities.clear();
    mMaxDepthInUse = 0;
    mExpectedRepairs = 0;
    mRawEdits = 0;
    bump_structure();
}

bool scene::contains(ecs::entity target) const noexcept {
    return mWorld->is_alive(target);
}


/*===========================================================================*/
/*===========================================================================*/
// Hierarchy

core::result<void> scene::set_parent(ecs::entity child, ecs::entity newParent, bool keepWorld, u32 index) {
    CONJURE_TRY(check_reparent(child, newParent));
    operation_scope operation(*this);

    if (keepWorld && mWorld->has_raw(child, mIds.localTransform)) {
        const math::transform childWorld = compute_world_transform(*this, child);
        const math::transform parentWorld =
          newParent.is_null() ? math::transform{} : compute_world_transform(*this, newParent);
        local_transform *local = mWorld->get<local_transform>(child);
        *local = local_transform::from_transform(parentWorld.inverse() * childWorld);
    }

    const ecs::entity oldParent = mTree.parent_of(child);
    const u8 oldDepth = mTree.depth_of(child);
    const scene_id owner = owner_of_tree(child);
    if (oldParent.is_null()) {
        erase_root(child);
    } else {
        unlink_child(child, oldParent);
    }

    u8 newDepth = 0;
    if (newParent.is_null()) {
        if (mWorld->has_raw(child, mIds.parent)) {
            mScratch.remove_raw(child, mIds.parent);
        }
        insert_root(child, index, owner);
    } else {
        newDepth = static_cast<u8>(mTree.depth_of(newParent) + 1);
        if (mWorld->has_raw(child, mIds.parent)) {
            mWorld->get<parent>(child)->value = newParent;
            link_child(child, newParent, index);
        } else {
            // The component arrives at the flush: its sibling index travels in the payload.
            const u32 position = std::min<u32>(index, static_cast<u32>(mTree.children_of(newParent).size()));
            const parent link{newParent, position};
            mScratch.add_raw(child, mIds.parent, bytes_of(link));
            link_child(child, newParent, position);
        }
    }

    if (oldDepth != newDepth) {
        retag_subtree(child, newDepth);
    }
    add_dirty(child);
    refresh_inactive_subtree(child, newParent.is_null() ? false : parent_chain_inactive(newParent));
    flush_scratch();
    bump_structure();
    return core::result<void>::success();
}

core::result<void> scene::deferred_set_parent(
  ecs::command_buffer &commands,
  ecs::entity child,
  ecs::entity newParent,
  bool keepWorld,
  u32 index
) {
    CONJURE_TRY(check_reparent(child, newParent));

    // A repair entry already waiting for this child means an earlier call in the same (unflushed)
    // buffer: the commands continue from the state that call leaves, and the entry's intent is replaced.
    repair_entry *pending = nullptr;
    for (repair_entry &queued : mRepairQueue) {
        if (queued.hasIntent && queued.entity == child) {
            pending = &queued;
        }
    }
    const bool liveHasParent = mWorld->has_raw(child, mIds.parent);
    const bool hasParent = pending != nullptr ? !pending->intendedParent.is_null() : liveHasParent;

    if (keepWorld && mWorld->has_raw(child, mIds.localTransform)) {
        const math::transform childWorld = compute_world_transform(*this, child);
        const math::transform parentWorld =
          newParent.is_null() ? math::transform{} : compute_world_transform(*this, newParent);
        commands.set<local_transform>(child, local_transform::from_transform(parentWorld.inverse() * childWorld));
    }
    if (newParent.is_null()) {
        if (hasParent) {
            commands.remove_raw(child, mIds.parent);
        }
    } else {
        const parent link{newParent, index};
        if (hasParent) {
            commands.set_raw(child, mIds.parent, bytes_of(link));
        } else {
            commands.add_raw(child, mIds.parent, bytes_of(link));
        }
    }
    if (mWorld->has_raw(child, mIds.localTransform) && !mWorld->has_raw(child, mIds.transformDirty)) {
        commands.add_raw(child, mIds.transformDirty);
    }

    // The repair runs once the buffer has landed: the entry names the parent it expects to find and
    // keeps the owner of the tree the child leaves, like set_parent does. The silent-observer count
    // follows the net change of the buffer (a flush fires observers for the mask delta only).
    if (pending != nullptr) {
        if (liveHasParent != hasParent && mExpectedRepairs > 0) {
            --mExpectedRepairs;
        }
        pending->intendedParent = newParent;
        pending->rootIndex = index;
    } else {
        repair_entry entry = {};
        entry.entity = child;
        entry.previousParent = mTree.parent_of(child);
        entry.intendedParent = newParent;
        entry.owner = owner_of_tree(child);
        entry.rootIndex = index;
        entry.hasIntent = true;
        mRepairQueue.push_back(entry);
    }
    if (liveHasParent != !newParent.is_null()) {
        ++mExpectedRepairs;
    }
    return core::result<void>::success();
}

core::result<void> scene::set_sibling_index(ecs::entity target, u32 index) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_sibling_index", target);
    }
    const ecs::entity parentEntity = mTree.parent_of(target);
    if (parentEntity.is_null()) {
        const usize current = root_index_of(target);
        if (current == NO_ROOT) {
            return core::make_error(scene_error::INVALID_SIBLING_INDEX, "entity {} is not a listed root", target);
        }
        const root_entry entry = mRoots[current];
        mRoots.erase(mRoots.begin() + static_cast<isize>(current));
        const usize position = std::min<usize>(index, mRoots.size());
        mRoots.insert(mRoots.begin() + static_cast<isize>(position), entry);
        bump_structure();
        return core::result<void>::success();
    }
    if (!mWorld->is_alive(parentEntity)) {
        return core::make_error(scene_error::INVALID_SIBLING_INDEX, "the parent of entity {} is dead", target);
    }
    children *list = mWorld->get<children>(parentEntity);
    if (list == nullptr) {
        return core::make_error(scene_error::INVALID_SIBLING_INDEX, "the parent of entity {} has no children list",
          target);
    }
    const auto found = std::find(list->entities.begin(), list->entities.end(), target);
    if (found == list->entities.end()) {
        return core::make_error(scene_error::INVALID_SIBLING_INDEX, "entity {} is missing from its parent's children",
          target);
    }
    const usize current = static_cast<usize>(found - list->entities.begin());
    list->entities.erase(found);
    const usize position = std::min<usize>(index, list->entities.size());
    list->entities.insert(list->entities.begin() + static_cast<isize>(position), target);
    renumber_children(parentEntity, std::min(current, position));
    bump_structure();
    return core::result<void>::success();
}

std::optional<scene_id> scene::root_owner(ecs::entity root) const noexcept {
    const usize index = root_index_of(root);
    if (index == NO_ROOT) {
        return std::nullopt;
    }
    return mRoots[index].owner;
}

core::result<void> scene::set_root_owner(ecs::entity root, scene_id owner) {
    const usize index = root_index_of(root);
    if (index == NO_ROOT) {
        return core::make_error(scene_error::INVALID_ENTITY, "entity {} is not a root", root);
    }
    mRoots[index].owner = owner;
    return core::result<void>::success();
}

core::result<void> scene::set_root_order(std::span<const ecs::entity> ordered) {
    std::vector<usize> positions = {};
    positions.reserve(ordered.size());
    for (const ecs::entity root : ordered) {
        const usize index = root_index_of(root);
        if (index == NO_ROOT) {
            return core::make_error(scene_error::INVALID_ENTITY, "entity {} is not a root", root);
        }
        if (std::find(positions.begin(), positions.end(), index) != positions.end()) {
            return core::make_error(scene_error::INVALID_ENTITY, "entity {} is listed twice", root);
        }
        positions.push_back(index);
    }
    std::vector<root_entry> entries = {};
    entries.reserve(positions.size());
    for (const usize index : positions) {
        entries.push_back(mRoots[index]);
    }
    std::sort(positions.begin(), positions.end());
    for (usize slot = 0; slot < positions.size(); ++slot) {
        mRoots[positions[slot]] = entries[slot];
    }
    bump_structure();
    return core::result<void>::success();
}

ecs::entity scene::find_path(std::string_view path) const {
    const std::vector<std::string_view> segments = core::str::split(path, '/', false);
    if (segments.empty()) {
        return ecs::NULL_ENTITY;
    }
    ecs::entity current = ecs::NULL_ENTITY;
    for (usize level = 0; level < segments.size(); ++level) {
        const core::string_id wanted(segments[level]);
        ecs::entity next = ecs::NULL_ENTITY;
        if (level == 0) {
            for (const root_entry &entry : mRoots) {
                if (name_of(entry.entity) == wanted) {
                    next = entry.entity;
                    break;
                }
            }
        } else {
            for (const ecs::entity child : mTree.children_of(current)) {
                if (name_of(child) == wanted) {
                    next = child;
                    break;
                }
            }
        }
        if (next.is_null()) {
            return ecs::NULL_ENTITY;
        }
        current = next;
    }
    return current;
}


/*===========================================================================*/
/*===========================================================================*/
// Transforms

core::result<void> scene::add_transform(ecs::entity target, const local_transform &value) {
    if (!mWorld->is_alive(target)) {
        return not_alive("add_transform", target);
    }
    operation_scope operation(*this);
    const ecs::component_mask mask = mWorld->mask_of(target);
    if (mask.test(mIds.localTransform)) {
        *mWorld->get<local_transform>(target) = value;
    } else {
        mScratch.add<local_transform>(target, value);
    }
    add_companions(target, mask);
    // The local_transform may still sit in mScratch, so add_dirty's live-mask test does not apply here.
    if (!mask.test(mIds.transformDirty)) {
        mScratch.add_raw(target, mIds.transformDirty);
    }
    flush_scratch();
    return core::result<void>::success();
}

core::result<void> scene::set_local_transform(ecs::entity target, const local_transform &value) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_local_transform", target);
    }
    local_transform *local = mWorld->get<local_transform>(target);
    if (local == nullptr) {
        return core::make_error(scene_error::NO_TRANSFORM, "entity {} has no local_transform", target);
    }
    *local = value;
    return core::result<void>::success();
}

core::result<void> scene::set_local_transform(ecs::entity target, const math::transform &value) {
    return set_local_transform(target, local_transform::from_transform(value));
}

core::result<void> scene::set_world_transform(ecs::entity target, const math::transform &value) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_world_transform", target);
    }
    if (!mWorld->has_raw(target, mIds.localTransform)) {
        return core::make_error(scene_error::NO_TRANSFORM, "entity {} has no local_transform", target);
    }
    const ecs::entity parentEntity = mTree.parent_of(target);
    const math::transform parentWorld =
      parentEntity.is_null() ? math::transform{} : compute_world_transform(*this, parentEntity);
    *mWorld->get<local_transform>(target) = local_transform::from_transform(parentWorld.inverse() * value);
    return core::result<void>::success();
}

std::optional<math::transform> scene::local_transform_of(ecs::entity target) const noexcept {
    const local_transform *local = mWorld->get<local_transform>(target);
    if (local == nullptr) {
        return std::nullopt;
    }
    return local->to_transform();
}

std::optional<math::transform> scene::world_transform_of(ecs::entity target) const {
    if (!mWorld->has_raw(target, mIds.localTransform)) {
        return std::nullopt;
    }
    return compute_world_transform(*this, target);
}

std::optional<math::aabb> scene::world_bounds(ecs::entity target) const {
    const local_bounds *bounds = mWorld->get<local_bounds>(target);
    if (bounds == nullptr || !bounds->box.is_valid()) {
        return std::nullopt;
    }
    const std::optional<math::transform> world = world_transform_of(target);
    if (!world.has_value()) {
        return std::nullopt;
    }
    return bounds->box.transformed(world->to_matrix());
}

std::optional<math::aabb> scene::subtree_bounds(ecs::entity target) const {
    math::aabb total = {};
    mTree.for_each_in_subtree(target, [&](ecs::entity entity) {
        const std::optional<math::aabb> bounds = world_bounds(entity);
        if (bounds.has_value()) {
            total.expand(*bounds);
        }
    });
    if (!total.is_valid()) {
        return std::nullopt;
    }
    return total;
}


/*===========================================================================*/
/*===========================================================================*/
// Identity and attributes

core::result<core::uuid> scene::ensure_guid(ecs::entity target) {
    if (!mWorld->is_alive(target)) {
        return not_alive("ensure_guid", target);
    }
    guid *identity = mWorld->get<guid>(target);
    if (identity != nullptr && !identity->value.is_null()) {
        return identity->value;
    }
    const core::uuid fresh = core::uuid::generate();
    if (identity != nullptr) {
        identity->value = fresh;
        const bool inserted = mGuids.insert(fresh, target);
        CONJURE_UNUSED(inserted);
        return fresh;
    }
    CONJURE_TRY(mWorld->add<guid>(target, guid{fresh}));
    return fresh;
}

core::result<void> scene::set_guid(ecs::entity target, const core::uuid &value) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_guid", target);
    }
    if (value.is_null()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "set_guid needs a non-null guid");
    }
    const ecs::entity existing = mGuids.find(value);
    if (!existing.is_null() && !(existing == target) && mWorld->is_alive(existing)) {
        return core::make_error(scene_error::DUPLICATE_GUID, "guid {} already belongs to entity {}", value, existing);
    }
    guid *identity = mWorld->get<guid>(target);
    if (identity != nullptr) {
        const core::uuid old = identity->value;
        identity->value = value;
        mGuids.rebind(target, old, value);
        return core::result<void>::success();
    }
    CONJURE_TRY(mWorld->add<guid>(target, guid{value}));
    return core::result<void>::success();
}

std::optional<core::uuid> scene::guid_of(ecs::entity target) const noexcept {
    const guid *identity = mWorld->get<guid>(target);
    if (identity == nullptr || identity->value.is_null()) {
        return std::nullopt;
    }
    return identity->value;
}

ecs::entity scene::find_guid(const core::uuid &id) const noexcept {
    return mGuids.find(id);
}

core::result<ecs::entity> scene::resolve_guid(const core::uuid &id) const {
    const ecs::entity found = mGuids.find(id);
    if (found.is_null() || !mWorld->is_alive(found)) {
        return core::make_error(scene_error::GUID_NOT_FOUND, "no entity carries guid {}", id);
    }
    return found;
}

core::string_id scene::name_of(ecs::entity target) const noexcept {
    const name_component *value = mWorld->get<name_component>(target);
    return value == nullptr ? core::string_id() : value->value;
}

core::string_id scene::tag_of(ecs::entity target) const noexcept {
    const tag *value = mWorld->get<tag>(target);
    return value == nullptr ? core::string_id() : value->value;
}

u8 scene::layer_of(ecs::entity target) const noexcept {
    const layer *value = mWorld->get<layer>(target);
    return value == nullptr ? u8{0} : value->index;
}

core::result<void> scene::set_name(ecs::entity target, core::string_id value) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_name", target);
    }
    name_component *current = mWorld->get<name_component>(target);
    if (current != nullptr) {
        current->value = value;
        return core::result<void>::success();
    }
    CONJURE_TRY(mWorld->add<name_component>(target, name_component{value}));
    return core::result<void>::success();
}

core::result<void> scene::set_tag(ecs::entity target, core::string_id value) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_tag", target);
    }
    tag *current = mWorld->get<tag>(target);
    if (current != nullptr) {
        current->value = value;
        return core::result<void>::success();
    }
    CONJURE_TRY(mWorld->add<tag>(target, tag{value}));
    return core::result<void>::success();
}

core::result<void> scene::set_layer(ecs::entity target, u8 value) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_layer", target);
    }
    if (value >= 32) {
        return core::error(core::error_code::OUT_OF_RANGE, std::format("layer {} is outside 0..31", value));
    }
    layer *current = mWorld->get<layer>(target);
    if (current != nullptr) {
        current->index = value;
        return core::result<void>::success();
    }
    CONJURE_TRY(mWorld->add<layer>(target, layer{value}));
    return core::result<void>::success();
}

core::result<void> scene::set_enabled(ecs::entity target, bool enabled) {
    if (!mWorld->is_alive(target)) {
        return not_alive("set_enabled", target);
    }
    const ecs::component_mask mask = mWorld->mask_of(target);
    const bool currentlyDisabled = mask.test(mIds.disabled);
    if (enabled == !currentlyDisabled) {
        return core::result<void>::success();
    }
    operation_scope operation(*this);
    if (enabled) {
        mScratch.remove_raw(target, mIds.disabled);
    } else {
        mScratch.add_raw(target, mIds.disabled);
    }
    const bool childrenInactive = !enabled || mask.test(mIds.inactive);
    for (const ecs::entity child : mTree.children_of(target)) {
        refresh_inactive_subtree(child, childrenInactive);
    }
    flush_scratch();
    return core::result<void>::success();
}

bool scene::is_enabled(ecs::entity target) const noexcept {
    return mWorld->is_alive(target) && !mWorld->has_raw(target, mIds.disabled);
}

bool scene::is_active(ecs::entity target) const noexcept {
    if (!mWorld->is_alive(target)) {
        return false;
    }
    const ecs::component_mask mask = mWorld->mask_of(target);
    return !mask.test(mIds.disabled) && !mask.test(mIds.inactive);
}

void scene::find_by_name(core::string_id value, std::vector<ecs::entity> &out) const {
    mWorld->view<const name_component>().for_each([&](ecs::entity entity, const name_component &component) {
        if (component.value == value) {
            out.push_back(entity);
        }
    });
}

void scene::find_by_tag(core::string_id value, std::vector<ecs::entity> &out) const {
    mWorld->view<const tag>().for_each([&](ecs::entity entity, const tag &component) {
        if (component.value == value) {
            out.push_back(entity);
        }
    });
}

void scene::find_by_layer(u8 value, std::vector<ecs::entity> &out) const {
    mWorld->view<const layer>().for_each([&](ecs::entity entity, const layer &component) {
        if (component.index == value) {
            out.push_back(entity);
        }
    });
}


/*===========================================================================*/
/*===========================================================================*/
// Systems support

void scene::destroy_pending() {
    if (mDestroyQueue.empty()) {
        return;
    }
    mScratchEntities.clear();
    std::swap(mScratchEntities, mDestroyQueue);
    for (const ecs::entity target : mScratchEntities) {
        if (mWorld->is_alive(target)) {
            const core::result<void> destroyed = destroy_now(target);
            CONJURE_UNUSED(destroyed);
        }
    }
    mScratchEntities.clear();
}

core::result<void> scene::validate() const {
    // Only entities carrying a scene component take part in the tree; raw ECS entities that never
    // entered the scene (gameplay-only components) are not expected among the roots.
    ecs::component_mask sceneMask = {};
    sceneMask.set(mIds.parent);
    sceneMask.set(mIds.children);
    sceneMask.set(mIds.localTransform);
    sceneMask.set(mIds.worldTransform);
    sceneMask.set(mIds.renderWorldTransform);
    sceneMask.set(mIds.interpolatedTransform);
    sceneMask.set(mIds.transformState);
    sceneMask.set(mIds.transformDirty);
    sceneMask.set(mIds.name);
    sceneMask.set(mIds.tag);
    sceneMask.set(mIds.layer);
    sceneMask.set(mIds.guid);
    sceneMask.set(mIds.disabled);
    sceneMask.set(mIds.inactive);
    sceneMask.set(mIds.localBounds);
    sceneMask.set(mIds.prefabInstance);
    sceneMask.set(mIds.prefabLink);
    for (u32 level = 1; level < MAX_HIERARCHY_DEPTH; ++level) {
        sceneMask.set(mIds.depth[level]);
    }

    // Roots: alive, parentless, unique.
    std::unordered_set<u64> seenRoots = {};
    for (usize index = 0; index < mRoots.size(); ++index) {
        const ecs::entity root = mRoots[index].entity;
        if (!mWorld->is_alive(root)) {
            return violation(std::format("root {} at position {} is dead", root, index));
        }
        if (mWorld->has_raw(root, mIds.parent)) {
            return violation(std::format("root {} carries a parent component", root));
        }
        if (!seenRoots.insert(root.to_u64()).second) {
            return violation(std::format("root {} is listed twice", root));
        }
    }

    u32 guidCount = 0;
    for (const ecs::archetype *owner : mWorld->archetypes()) {
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            for (const ecs::entity entity : owner->chunk_entities(chunkIndex)) {
                const ecs::component_mask mask = owner->mask();
                const parent *link = mWorld->get<parent>(entity);
                u8 expectedDepth = 0;
                bool ancestorDisabled = false;
                if (link != nullptr) {
                    if (!mWorld->is_alive(link->value)) {
                        return violation(std::format("entity {} has a dead parent {}", entity, link->value));
                    }
                    const std::span<const ecs::entity> siblings = mTree.children_of(link->value);
                    if (link->siblingIndex >= siblings.size() || !(siblings[link->siblingIndex] == entity)) {
                        return violation(std::format("entity {} is not at sibling index {} of parent {}",
                          entity, link->siblingIndex, link->value));
                    }
                    ecs::entity ancestor = link->value;
                    u32 steps = 0;
                    while (!ancestor.is_null()) {
                        ++steps;
                        if (steps >= MAX_HIERARCHY_DEPTH) {
                            return violation(std::format("entity {} sits in a parent cycle", entity));
                        }
                        const ecs::component_mask ancestorMask = mWorld->mask_of(ancestor);
                        if (ancestorMask.test(mIds.disabled)) {
                            ancestorDisabled = true;
                        }
                        ancestor = mTree.parent_of(ancestor);
                    }
                    expectedDepth = static_cast<u8>(steps);
                } else if (!(mask & sceneMask).none() && root_index_of(entity) == NO_ROOT) {
                    return violation(std::format("parentless entity {} is not listed as a root", entity));
                }

                u32 depthTags = 0;
                for (u32 level = 1; level < MAX_HIERARCHY_DEPTH; ++level) {
                    if (mask.test(mIds.depth[level])) {
                        ++depthTags;
                    }
                }
                if (depthTags > 1) {
                    return violation(std::format("entity {} carries {} depth tags", entity, depthTags));
                }
                if (mIds.depth_of(mask) != expectedDepth) {
                    return violation(std::format("entity {} has depth tag {} but sits at depth {}",
                      entity, mIds.depth_of(mask), expectedDepth));
                }
                if (mask.test(mIds.inactive) != ancestorDisabled) {
                    return violation(std::format("entity {} inactive tag disagrees with its ancestors", entity));
                }
                if (mask.test(mIds.localTransform) && !mIds.has_transform_companions(mask)) {
                    return violation(std::format("entity {} has a local_transform without its companions", entity));
                }

                const children *list = mWorld->get<children>(entity);
                if (list != nullptr) {
                    for (usize position = 0; position < list->entities.size(); ++position) {
                        const ecs::entity child = list->entities[position];
                        const parent *childLink = mWorld->get<parent>(child);
                        const bool agrees = childLink != nullptr && childLink->value == entity
                          && childLink->siblingIndex == position;
                        if (!agrees) {
                            return violation(std::format("child {} of entity {} disagrees with its parent link",
                              child, entity));
                        }
                    }
                }

                const guid *identity = mWorld->get<guid>(entity);
                if (identity != nullptr && !identity->value.is_null()) {
                    ++guidCount;
                    if (!(mGuids.find(identity->value) == entity)) {
                        return violation(std::format("guid {} of entity {} is not indexed", identity->value, entity));
                    }
                }
            }
        }
    }
    if (guidCount != mGuids.size()) {
        return violation(std::format("the guid index holds {} entries for {} guid components", mGuids.size(),
          guidCount));
    }
    return core::result<void>::success();
}


/*===========================================================================*/
/*===========================================================================*/
// Tree bookkeeping

core::result<ecs::entity> scene::create_entity_unannounced(
  core::string_id entityName,
  ecs::entity parentEntity,
  u32 index
) {
    u8 depth = 0;
    bool parentInactive = false;
    if (!parentEntity.is_null()) {
        if (!mWorld->is_alive(parentEntity)) {
            return not_alive("create_entity", parentEntity);
        }
        const u32 level = static_cast<u32>(mTree.depth_of(parentEntity)) + 1;
        if (level >= MAX_HIERARCHY_DEPTH) {
            return core::make_error(scene_error::HIERARCHY_TOO_DEEP, "a child of entity {} would sit at depth {}",
              parentEntity, level);
        }
        depth = static_cast<u8>(level);
        parentInactive = parent_chain_inactive(parentEntity);
    }

    ecs::component_mask mask = {};
    mask.set(mIds.guid);
    mask.set(mIds.name);
    mask.set(mIds.localTransform);
    mask.set(mIds.worldTransform);
    mask.set(mIds.renderWorldTransform);
    mask.set(mIds.transformState);
    mask.set(mIds.transformDirty);
    if (depth > 0) {
        mask.set(mIds.parent);
        mask.set(mIds.depth[depth]);
    }
    if (parentInactive) {
        mask.set(mIds.inactive);
    }

    operation_scope operation(*this);
    CONJURE_TRY_ASSIGN(const ecs::entity created, mWorld->create_entity(mask));

    guid *identity = mWorld->get<guid>(created);
    identity->value = core::uuid::generate();
    const bool indexed = mGuids.insert(identity->value, created);
    CONJURE_UNUSED(indexed);
    mWorld->get<name_component>(created)->value = entityName;

    if (depth > 0) {
        mWorld->get<parent>(created)->value = parentEntity;
        link_child(created, parentEntity, index);
        mMaxDepthInUse = std::max(mMaxDepthInUse, depth);
    } else {
        insert_root(created, index, SCENE_ID_AUTHORED);
    }
    return created;
}

core::result<void> scene::destroy_unannounced(ecs::entity target) {
    if (!mWorld->is_alive(target)) {
        return not_alive("destroy_now", target);
    }
    operation_scope operation(*this);
    const ecs::entity parentEntity = mTree.parent_of(target);
    if (parentEntity.is_null()) {
        erase_root(target);
    } else {
        unlink_child(target, parentEntity);
    }
    return mWorld->destroy_entity(target);
}

usize scene::root_index_of(ecs::entity target) const noexcept {
    for (usize index = 0; index < mRoots.size(); ++index) {
        if (mRoots[index].entity == target) {
            return index;
        }
    }
    return NO_ROOT;
}

scene_id scene::owner_of_tree(ecs::entity target) const noexcept {
    const ecs::entity root = mTree.root_of(target);
    const usize index = root_index_of(root);
    if (index == NO_ROOT) {
        return SCENE_ID_AUTHORED;
    }
    return mRoots[index].owner;
}

void scene::insert_root(ecs::entity target, u32 index, scene_id owner) {
    root_entry entry = {};
    entry.entity = target;
    entry.owner = owner.is_valid() ? owner : SCENE_ID_AUTHORED;
    const usize position = std::min<usize>(index, mRoots.size());
    mRoots.insert(mRoots.begin() + static_cast<isize>(position), entry);
}

void scene::erase_root(ecs::entity target) {
    const usize index = root_index_of(target);
    if (index != NO_ROOT) {
        mRoots.erase(mRoots.begin() + static_cast<isize>(index));
    }
}

void scene::link_child(ecs::entity child, ecs::entity parentEntity, u32 index) {
    children *list = mWorld->get<children>(parentEntity);
    if (list == nullptr) {
        const core::result<children *> added = mWorld->add<children>(parentEntity);
        if (!added.has_value()) {
            CONJURE_ASSERT(false, "scene: cannot add a children list to entity {}", parentEntity);
            return;
        }
        list = added.value();
    }
    const usize position = std::min<usize>(index, list->entities.size());
    list->entities.insert(list->entities.begin() + static_cast<isize>(position), child);
    renumber_children(parentEntity, position);
}

void scene::unlink_child(ecs::entity child, ecs::entity parentEntity) {
    children *list = mWorld->get<children>(parentEntity);
    if (list == nullptr) {
        return;
    }
    const auto found = std::find(list->entities.begin(), list->entities.end(), child);
    if (found == list->entities.end()) {
        return;
    }
    const usize position = static_cast<usize>(found - list->entities.begin());
    list->entities.erase(found);
    renumber_children(parentEntity, position);
}

void scene::renumber_children(ecs::entity parentEntity, usize from) {
    const children *list = mWorld->get<children>(parentEntity);
    if (list == nullptr) {
        return;
    }
    for (usize position = from; position < list->entities.size(); ++position) {
        parent *link = mWorld->get<parent>(list->entities[position]);
        if (link != nullptr) {
            link->siblingIndex = static_cast<u32>(position);
        }
    }
}

void scene::retag_subtree(ecs::entity root, u8 newDepth) {
    struct frame {
        ecs::entity entity = {};
        u8 depth = 0;
    };
    core::small_vector<frame, 64> stack = {};
    stack.push_back(frame{root, newDepth});
    while (!stack.empty()) {
        const frame current = stack.back();
        stack.pop_back();
        const ecs::component_mask mask = mWorld->mask_of(current.entity);
        const u8 oldDepth = mIds.depth_of(mask);
        if (oldDepth != current.depth) {
            if (oldDepth != 0) {
                mScratch.remove_raw(current.entity, mIds.depth[oldDepth]);
            }
            if (current.depth != 0) {
                mScratch.add_raw(current.entity, mIds.depth[current.depth]);
            }
        }
        mMaxDepthInUse = std::max(mMaxDepthInUse, current.depth);
        if (current.depth + 1 >= MAX_HIERARCHY_DEPTH) {
            continue;
        }
        for (const ecs::entity child : mTree.children_of(current.entity)) {
            stack.push_back(frame{child, static_cast<u8>(current.depth + 1)});
        }
    }
}

void scene::refresh_inactive_subtree(ecs::entity root, bool ancestorInactive) {
    struct frame {
        ecs::entity entity = {};
        bool inactive = false;
    };
    core::small_vector<frame, 64> stack = {};
    stack.push_back(frame{root, ancestorInactive});
    while (!stack.empty()) {
        const frame current = stack.back();
        stack.pop_back();
        const ecs::component_mask mask = mWorld->mask_of(current.entity);
        const bool has = mask.test(mIds.inactive);
        if (current.inactive && !has) {
            mScratch.add_raw(current.entity, mIds.inactive);
        } else if (!current.inactive && has) {
            mScratch.remove_raw(current.entity, mIds.inactive);
        }
        const bool childInactive = current.inactive || mask.test(mIds.disabled);
        for (const ecs::entity child : mTree.children_of(current.entity)) {
            stack.push_back(frame{child, childInactive});
        }
    }
}

void scene::add_dirty(ecs::entity target) {
    // The propagation pass is the tag's only remover and never visits a transformless entity, so such an
    // entity would keep the tag forever.
    const ecs::component_mask mask = mWorld->mask_of(target);
    if (mask.test(mIds.localTransform) && !mask.test(mIds.transformDirty)) {
        mScratch.add_raw(target, mIds.transformDirty);
    }
}

void scene::add_companions(ecs::entity target, const ecs::component_mask &mask) {
    if (!mask.test(mIds.worldTransform)) {
        mScratch.add_raw(target, mIds.worldTransform);
    }
    if (!mask.test(mIds.renderWorldTransform)) {
        mScratch.add_raw(target, mIds.renderWorldTransform);
    }
    if (!mask.test(mIds.transformState)) {
        mScratch.add_raw(target, mIds.transformState);
    }
}

void scene::flush_scratch() {
    if (mScratch.empty()) {
        return;
    }
    operation_scope operation(*this);
    const ecs::flush_report report = mWorld->flush(mScratch);
    CONJURE_UNUSED(report);
}

void scene::bump_structure() {
    ++mStructureVersion;
    mOnStructureChanged.emit();
}

bool scene::parent_chain_inactive(ecs::entity parentEntity) const noexcept {
    if (parentEntity.is_null() || !mWorld->is_alive(parentEntity)) {
        return false;
    }
    const ecs::component_mask mask = mWorld->mask_of(parentEntity);
    return mask.test(mIds.disabled) || mask.test(mIds.inactive);
}

core::result<void> scene::check_reparent(ecs::entity child, ecs::entity newParent) const {
    if (!mWorld->is_alive(child)) {
        return not_alive("set_parent", child);
    }
    if (!newParent.is_null() && !mWorld->is_alive(newParent)) {
        return not_alive("set_parent", newParent);
    }
    if (child == newParent) {
        return core::make_error(scene_error::SELF_PARENT, "entity {} cannot be its own parent", child);
    }
    if (!newParent.is_null() && mTree.is_ancestor_of(child, newParent)) {
        return core::make_error(scene_error::HIERARCHY_CYCLE, "entity {} is a descendant of entity {}", newParent,
          child);
    }
    const u32 depth = newParent.is_null() ? 0u : static_cast<u32>(mTree.depth_of(newParent)) + 1u;
    const u32 height = mTree.subtree_height(child);
    if (depth + height >= MAX_HIERARCHY_DEPTH) {
        return core::make_error(scene_error::HIERARCHY_TOO_DEEP, "entity {} would reach depth {} under entity {}",
          child, depth + height, newParent);
    }
    return core::result<void>::success();
}
}
