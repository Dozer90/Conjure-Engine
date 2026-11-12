#include "conjure/scene/scene.h"

#include "scene/archive_tools.h"

#include "conjure/scene/log_category.h"
#include "conjure/scene/scene_serializer.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/ecs/serialization.h"

#include <algorithm>
#include <utility>
#include <vector>

// Archive instantiation (docs/design/13_scene.md 3.9) and hierarchy repair (3.7).

namespace conjure::scene {

using name_component = ::conjure::scene::name;
using detail::archive_shape;
using detail::NO_ENTRY;

namespace {

    [[nodiscard]] std::span<const std::byte> bytes_of(const parent &value) noexcept {
        return std::span<const std::byte>(reinterpret_cast<const std::byte *>(&value), sizeof(parent));
    }

    /// <summary>
    /// The live external parent of an archive root entry, or NULL_ENTITY.
    /// </summary>
    [[nodiscard]] ecs::entity external_parent(
      const ecs::world &world,
      const ecs::entity_archive &archive,
      u32 entry,
      const scene_component_ids &ids
    ) noexcept {
        const std::optional<parent> link = detail::read_component<parent>(archive, entry, ids.parent);
        const bool usable = link.has_value() && !link->value.is_null() && !link->value.is_archive_local()
          && world.is_alive(link->value);
        if (!usable) {
            return ecs::NULL_ENTITY;
        }
        return link->value;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Instantiation

core::result<instantiate_report> scene::instantiate_archive(
  const std::shared_ptr<const ecs::entity_archive> &archive,
  u32 count,
  const instantiate_options &options,
  std::span<ecs::entity> outRoots
) {
    if (archive == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "instantiate_archive needs an archive");
    }
    if (outRoots.size() < count) {
        return core::error(core::error_code::INVALID_ARGUMENT, "instantiate_archive needs one root slot per copy");
    }
    CONJURE_TRY(archive->validate());
    const u32 entryCount = archive->entity_count();
    instantiate_report report = {};
    if (entryCount == 0 || count == 0) {
        std::fill(outRoots.begin(), outRoots.end(), ecs::NULL_ENTITY);
        return report;
    }
    if (!options.parent.is_null() && !mWorld->is_alive(options.parent)) {
        return core::make_error(scene_error::INVALID_ENTITY, "instantiate_archive: parent {} is not alive",
          options.parent);
    }

    // Shape and depth before anything exists, so a failure leaves the world untouched.
    CONJURE_TRY_ASSIGN(const archive_shape shape, detail::compute_archive_shape(*archive, mIds));
    const u32 baseDepth = options.parent.is_null() ? 0u : static_cast<u32>(mTree.depth_of(options.parent)) + 1u;
    for (u32 entry = 0; entry < entryCount; ++entry) {
        // shape.depth is relative to the entry's archive root, whose own depth depends on where it
        // attaches: under options.parent, under a live external parent, or as a scene root.
        u32 rootEntry = entry;
        while (shape.parents[rootEntry] != NO_ENTRY) {
            rootEntry = shape.parents[rootEntry];
        }
        u32 rootDepth = baseDepth;
        if (options.parent.is_null()) {
            const ecs::entity external = external_parent(*mWorld, *archive, rootEntry, mIds);
            rootDepth = external.is_null() ? 0u : static_cast<u32>(mTree.depth_of(external)) + 1u;
        }
        if (rootDepth + shape.depth[entry] >= MAX_HIERARCHY_DEPTH) {
            return core::make_error(scene_error::HIERARCHY_TOO_DEEP, "archive entry {} would sit at depth {}", entry,
              rootDepth + shape.depth[entry]);
        }
    }

    // Guids: KEEP must not collide with the index or inside the archive; count > 1 regenerates everything.
    const guid_policy policy = count > 1 ? guid_policy::REGENERATE_ALL : options.guids;
    if (policy == guid_policy::KEEP) {
        for (u32 index = 0; index < entryCount; ++index) {
            const std::optional<core::uuid> id = detail::entry_guid(*archive, index, mIds);
            if (!id.has_value()) {
                continue;
            }
            const ecs::entity existing = mGuids.find(*id);
            if (!existing.is_null() && mWorld->is_alive(existing)) {
                return core::make_error(scene_error::DUPLICATE_GUID, "guid {} of entry {} already belongs to entity {}",
                  *id, index, existing);
            }
        }
        CONJURE_TRY(detail::check_unique_guids(*archive, mIds));
    }
    mGuids.reserve(mGuids.size() + entryCount * count);

    std::vector<ecs::entity> entities = {};
    {
        operation_scope operation(*this);
        // The batch reports every id it created, copy-major in entry order, entries without components
        // included; nothing exists when it fails.
        const core::result<void> instantiated =
          mWorld->instantiate_batch(archive, outRoots.first(count), nullptr, &entities);
        if (!instantiated.has_value()) {
            return instantiated.get_error();
        }
        CONJURE_ASSERT(entities.size() == static_cast<usize>(entryCount) * count,
          "instantiate_batch reported {} entities for {} entries x {} copies", entities.size(), entryCount, count);

        // Guid regeneration (the observer indexed the archive guids first; collisions lost silently).
        u32 conflicts = 0;
        for (const ecs::entity spawned : entities) {
            guid *identity = mWorld->get<guid>(spawned);
            if (identity == nullptr) {
                continue;
            }
            if (identity->value.is_null()) {
                // A null guid identifies nothing: the regenerate policies give it a fresh one, not counted
                // as a conflict. KEEP leaves the archive as it is (snapshot restore relies on that).
                if (policy == guid_policy::KEEP) {
                    continue;
                }
                identity->value = core::uuid::generate();
                const bool indexed = mGuids.insert(identity->value, spawned);
                CONJURE_UNUSED(indexed);
                continue;
            }
            const bool regenerate = policy == guid_policy::REGENERATE_ALL
              || (policy == guid_policy::REGENERATE_ON_CONFLICT && !(mGuids.find(identity->value) == spawned));
            if (!regenerate) {
                continue;
            }
            if (policy == guid_policy::REGENERATE_ON_CONFLICT) {
                ++conflicts;
            }
            const core::uuid fresh = core::uuid::generate();
            mGuids.rebind(spawned, identity->value, fresh);
            identity->value = fresh;
        }
        if (conflicts > 0) {
            CONJURE_LOG_WARN(LOG_SCENE, "{} guid collision(s) regenerated while instantiating an archive", conflicts);
        }

        link_instantiated(*archive, shape, entities, count, options);
        flush_scratch();
    }

    if (options.bindAssets && mAssets != nullptr) {
        scene_serializer::bind_asset_refs(*mWorld, entities, *mAssets, &report.assetIds);
        std::sort(report.assetIds.begin(), report.assetIds.end());
        report.assetIds.erase(std::unique(report.assetIds.begin(), report.assetIds.end()), report.assetIds.end());
    }
    report.entityCount = static_cast<u32>(entities.size());
    report.unknownComponents = detail::count_unresolved(*archive) * count;
    report.entities = std::move(entities);
    bump_structure();
    return report;
}

void scene::link_instantiated(
  const ecs::entity_archive &archive,
  const archive_shape &shape,
  std::span<const ecs::entity> entities,
  u32 copies,
  const instantiate_options &options
) {
    const u32 entryCount = archive.entity_count();
    const std::span<const ecs::archive_entry> entries = archive.entries();

    const bool parentInactive = parent_chain_inactive(options.parent);
    const u8 baseDepth = options.parent.is_null() ? u8{0} : static_cast<u8>(mTree.depth_of(options.parent) + 1);
    u32 rootInsertPosition = options.index;
    const scene_id owner = options.owner.is_valid() ? options.owner : SCENE_ID_AUTHORED;

    std::vector<u8> depths(entryCount, 0);
    std::vector<bool> inactiveFlags(entryCount, false);
    // The children of the new internal nodes are gathered here and handed to the flush as their
    // children component, so an internal node moves once (3.9) instead of once for the list and once
    // more for its tags.
    std::vector<core::small_vector<ecs::entity, 4>> childLists(entryCount);
    for (u32 copy = 0; copy < copies; ++copy) {
        const usize base = static_cast<usize>(copy) * entryCount;
        for (core::small_vector<ecs::entity, 4> &list : childLists) {
            list.clear();
        }

        for (const u32 entry : shape.order) {
            const ecs::entity created = entities[base + entry];
            const ecs::component_mask &mask = entries[entry].mask;
            const u32 localParent = shape.parents[entry];
            u8 depth = 0;
            bool ancestorInactive = false;

            if (localParent != NO_ENTRY) {
                childLists[localParent].push_back(created);
                depth = static_cast<u8>(depths[localParent] + 1);
                ancestorInactive = inactiveFlags[localParent] || entries[localParent].mask.test(mIds.disabled);
            } else {
                // An archive root: under options.parent, under a live external parent, or a scene root.
                const std::optional<parent> link = detail::read_component<parent>(archive, entry, mIds.parent);
                const ecs::entity external = external_parent(*mWorld, archive, entry, mIds);
                if (!options.parent.is_null()) {
                    const u32 position =
                      std::min<u32>(rootInsertPosition, static_cast<u32>(mTree.children_of(options.parent).size()));
                    const parent fixed{options.parent, position};
                    if (link.has_value()) {
                        mScratch.set_raw(created, mIds.parent, bytes_of(fixed));
                    } else {
                        mScratch.add_raw(created, mIds.parent, bytes_of(fixed));
                    }
                    link_child(created, options.parent, position);
                    if (rootInsertPosition != APPEND_CHILD) {
                        ++rootInsertPosition;
                    }
                    depth = baseDepth;
                    ancestorInactive = parentInactive;
                } else if (!external.is_null()) {
                    const u32 position =
                      std::min<u32>(link->siblingIndex, static_cast<u32>(mTree.children_of(external).size()));
                    const parent fixed{external, position};
                    mScratch.set_raw(created, mIds.parent, bytes_of(fixed));
                    link_child(created, external, position);
                    depth = static_cast<u8>(mTree.depth_of(external) + 1);
                    ancestorInactive = parent_chain_inactive(external);
                } else {
                    if (link.has_value()) {
                        if (!link->value.is_null()) {
                            CONJURE_LOG_WARN(LOG_SCENE, "archive root {} pointed at dead parent {}; it becomes a root",
                              entry, link->value);
                        }
                        mScratch.remove_raw(created, mIds.parent);
                    }
                    insert_root(created, rootInsertPosition, owner);
                    if (rootInsertPosition != APPEND_CHILD) {
                        ++rootInsertPosition;
                    }
                }
            }
            depths[entry] = depth;
            inactiveFlags[entry] = ancestorInactive;

            if (depth > 0) {
                mScratch.add_raw(created, mIds.depth[depth]);
                mMaxDepthInUse = std::max(mMaxDepthInUse, depth);
            }
            // Only an entity with a transform gets the dirty tag: the pass, its only remover, never visits
            // a transformless one.
            if (mask.test(mIds.localTransform) || (options.rootTransform.has_value() && localParent == NO_ENTRY)) {
                add_companions(created, mask);
                mScratch.add_raw(created, mIds.transformDirty);
            }
            if (ancestorInactive && !mask.test(mIds.inactive)) {
                mScratch.add_raw(created, mIds.inactive);
            }

            if (localParent == NO_ENTRY) {
                if (options.rootName.has_value()) {
                    if (mask.test(mIds.name)) {
                        mScratch.set<name_component>(created, name_component{*options.rootName});
                    } else {
                        mScratch.add<name_component>(created, name_component{*options.rootName});
                    }
                }
                if (options.rootTransform.has_value()) {
                    if (mask.test(mIds.localTransform)) {
                        mScratch.set<local_transform>(created, *options.rootTransform);
                    } else {
                        mScratch.add<local_transform>(created, *options.rootTransform);
                    }
                }
            }
        }

        // Children gathered in archive order are sorted by the persisted sibling index, renumbered
        // through their live parent links and recorded as the parent's children component.
        for (u32 entry = 0; entry < entryCount; ++entry) {
            core::small_vector<ecs::entity, 4> &list = childLists[entry];
            if (list.empty()) {
                continue;
            }
            std::stable_sort(list.begin(), list.end(), [this](ecs::entity left, ecs::entity right) {
                return mTree.sibling_index(left) < mTree.sibling_index(right);
            });
            for (usize position = 0; position < list.size(); ++position) {
                parent *link = mWorld->get<parent>(list[position]);
                if (link != nullptr) {
                    link->siblingIndex = static_cast<u32>(position);
                }
            }
            const ecs::entity parentEntity = entities[base + entry];
            if (entries[entry].mask.test(mIds.children)) {
                mScratch.set<children>(parentEntity, children{std::move(list)});
            } else {
                mScratch.add<children>(parentEntity, children{std::move(list)});
            }
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Repair

void scene::apply_hierarchy_changes() {
    if (mRepairQueue.empty()) {
        return;
    }
    operation_scope operation(*this);
    // A swap rather than a move: both vectors keep their capacity, so a waiting entry pushed back every
    // pass does not allocate in steady state.
    mRepairSpare.clear();
    std::swap(mRepairSpare, mRepairQueue);
    bool repaired = false;
    for (const repair_entry &entry : mRepairSpare) {
        if (!mWorld->is_alive(entry.entity)) {
            continue;
        }
        if (entry.hasIntent && !(mTree.parent_of(entry.entity) == entry.intendedParent)) {
            // The command buffer of a deferred_set_parent has not been flushed yet: the live tree still
            // shows the old parent, so the repair waits for a later pass.
            mRepairQueue.push_back(entry);
            continue;
        }
        repair_entity(entry);
        // One flush per entry: retag_subtree and refresh_inactive_subtree decide from the live mask, so
        // two repairs touching one subtree in a frame must each see the previous one applied.
        flush_scratch();
        repaired = true;
    }
    mRepairSpare.clear();
    if (mRawEdits > 0) {
        CONJURE_LOG_WARN(LOG_SCENE, "{} raw parent edit(s) repaired by scene.apply_hierarchy_changes; use set_parent",
          mRawEdits);
        mRawEdits = 0;
    }
    if (repaired) {
        bump_structure();
    }
}

void scene::repair_entity(const repair_entry &entry) {
    const ecs::entity target = entry.entity;
    if (!mWorld->is_alive(target)) {
        return;
    }
    const scene_id owner = entry.owner.is_valid() ? entry.owner : owner_of_tree(target);
    erase_root(target);
    if (!entry.previousParent.is_null() && mWorld->is_alive(entry.previousParent)) {
        unlink_child(target, entry.previousParent);
    }

    u8 newDepth = 0;
    const parent *current = mWorld->get<parent>(target);
    const std::optional<parent> link = current == nullptr ? std::nullopt : std::optional<parent>(*current);
    bool becomesRoot = !link.has_value();
    if (link.has_value()) {
        const ecs::entity parentEntity = link->value;
        const bool invalid = parentEntity.is_null() || !mWorld->is_alive(parentEntity) || parentEntity == target
          || mTree.is_ancestor_of(target, parentEntity);
        const u32 depth = invalid ? 0u : static_cast<u32>(mTree.depth_of(parentEntity)) + 1u;
        if (invalid || depth + mTree.subtree_height(target) >= MAX_HIERARCHY_DEPTH) {
            CONJURE_LOG_WARN(LOG_SCENE, "entity {} has an unusable parent {}; it becomes a root", target, parentEntity);
            mScratch.remove_raw(target, mIds.parent);
            becomesRoot = true;
        } else {
            const std::span<const ecs::entity> siblings = mTree.children_of(parentEntity);
            const bool listed = std::find(siblings.begin(), siblings.end(), target) != siblings.end();
            if (!listed) {
                link_child(target, parentEntity, link->siblingIndex);
            } else {
                renumber_children(parentEntity, 0);
            }
            newDepth = static_cast<u8>(depth);
        }
    }
    if (becomesRoot) {
        insert_root(target, entry.rootIndex, owner);
    }

    retag_subtree(target, newDepth);
    add_dirty(target);
    const ecs::entity finalParent = becomesRoot ? ecs::NULL_ENTITY : link->value;
    refresh_inactive_subtree(target, parent_chain_inactive(finalParent));
}
}
