#include "conjure/scene/prefab.h"

#include "scene/archive_tools.h"

#include "conjure/scene/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/log.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/view.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace conjure::scene {

using name_component = ::conjure::scene::name;

namespace {

    [[nodiscard]] core::error not_instance(ecs::entity root) {
        return core::make_error(scene_error::NOT_A_PREFAB_INSTANCE, "entity {} carries no prefab_instance", root);
    }

    [[nodiscard]] core::error not_loaded(ecs::entity root, const assets::asset_id &id) {
        return core::make_error(scene_error::PREFAB_NOT_LOADED, "the prefab {} of entity {} is not resident", id, root);
    }

    /// <summary>
    /// Canonical JSON text of one component image, or nullopt when the type cannot be serialised.
    /// </summary>
    [[nodiscard]] std::optional<std::string> component_text(const ecs::component_info &info, const void *bytes) {
        core::json::writer out = {};
        const core::result<void> written = ecs::write_component_json(info, bytes, out);
        if (!written.has_value()) {
            return std::nullopt;
        }
        return out.finish();
    }

    /// <summary>
    /// Rewrites the archive-local entity references of an instance component through the remap table
    /// so they compare against the asset's indices; the image is a deep copy.
    /// </summary>
    [[nodiscard]] detail::component_image remapped_image(
      const ecs::component_info &info,
      const std::vector<std::byte> &bytes,
      const ecs::entity_remap &remap
    ) {
        detail::component_image image(info, bytes.empty() ? nullptr : bytes.data());
        for (const u32 offset : info.entityFieldOffsets) {
            if (offset + sizeof(ecs::entity) > image.size()) {
                continue;
            }
            ecs::entity reference = {};
            std::memcpy(&reference, image.data() + offset, sizeof(ecs::entity));
            reference = remap.resolve(reference);
            std::memcpy(image.data() + offset, &reference, sizeof(ecs::entity));
        }
        if (info.reflection == nullptr && info.serializer.remapEntities != nullptr && image.size() > 0) {
            info.serializer.remapEntities(image.data(), remap);
        }
        return image;
    }

    [[nodiscard]] const ecs::archive_component *find_component(
      const ecs::archive_entry &entry,
      ecs::component_id id
    ) noexcept {
        for (const ecs::archive_component &component : entry.components) {
            if (component.id == id) {
                return &component;
            }
        }
        return nullptr;
    }
}

namespace prefab {

core::result<prefab_document> build_document(const scene &source, ecs::entity root, std::string_view name) {
    CONJURE_TRY_ASSIGN(ecs::entity_archive archive, scene_serializer::capture_subtree(source, root));
    const std::vector<u32> roots = detail::archive_roots(archive, source.ids());
    if (roots.size() != 1 || roots.front() != 0) {
        return core::make_error(scene_error::PREFAB_MULTIPLE_ROOTS, "the subtree of entity {} has {} roots", root,
          roots.size());
    }
    prefab_document document = {};
    document.name = std::string(name);
    document.archive = std::make_shared<const ecs::entity_archive>(std::move(archive));
    return document;
}

core::result<void> mark_instance(scene &target, ecs::entity root, assets::asset_ref<prefab_asset> asset) {
    if (!target.contains(root)) {
        return core::make_error(scene_error::INVALID_ENTITY, "mark_instance: entity {} is not alive", root);
    }
    ecs::world &world = target.world();
    std::vector<ecs::entity> entities = {};
    target.tree().collect_subtree(root, entities);
    ecs::command_buffer commands(world);
    for (const ecs::entity entity : entities) {
        CONJURE_TRY_ASSIGN(const core::uuid id, target.ensure_guid(entity));
        prefab_link *link = world.get<prefab_link>(entity);
        if (link != nullptr) {
            link->assetGuid = id;
        } else {
            commands.add<prefab_link>(entity, prefab_link{id});
        }
    }
    prefab_instance *instance = world.get<prefab_instance>(root);
    if (instance != nullptr) {
        instance->prefab = asset;
    } else {
        commands.add<prefab_instance>(root, prefab_instance{asset});
    }
    const ecs::flush_report report = world.flush(commands);
    CONJURE_UNUSED(report);
    return core::result<void>::success();
}

core::result<ecs::entity> instantiate(scene &target, const prefab_asset &asset, const instantiate_options &options) {
    ecs::entity root = {};
    CONJURE_TRY(instantiate_many(target, asset, 1, options, std::span<ecs::entity>(&root, 1)));
    return root;
}

core::result<void> instantiate_many(
  scene &target,
  const prefab_asset &asset,
  u32 count,
  const instantiate_options &options,
  std::span<ecs::entity> outRoots
) {
    if (asset.archive == nullptr) {
        return core::make_error(scene_error::PREFAB_NOT_LOADED, "the prefab asset {} has no archive", asset.id);
    }
    if (outRoots.size() < count) {
        return core::error(core::error_code::INVALID_ARGUMENT, "instantiate_many needs one root slot per instance");
    }
    const scene_component_ids &ids = target.ids();
    const std::vector<u32> roots = detail::archive_roots(*asset.archive, ids);
    if (roots.size() != 1 || roots.front() != 0) {
        return core::make_error(scene_error::PREFAB_MULTIPLE_ROOTS, "prefab {} has {} roots", asset.id, roots.size());
    }
    instantiate_options fixed = options;
    fixed.guids = guid_policy::REGENERATE_ALL;
    CONJURE_TRY_ASSIGN(
      const instantiate_report report,
      target.instantiate_archive(asset.archive, count, fixed, outRoots)
    );

    // Markers: every entity links to its entry's guid, every root carries the bound asset.
    ecs::world &world = target.world();
    const u32 entryCount = asset.archive->entity_count();
    ecs::command_buffer commands(world);
    for (u32 copy = 0; copy < count; ++copy) {
        for (u32 entry = 0; entry < entryCount; ++entry) {
            const ecs::entity entity = report.entities[static_cast<usize>(copy) * entryCount + entry];
            if (!world.has_raw(entity, ids.prefabLink)) {
                const std::optional<core::uuid> entryId = detail::entry_guid(*asset.archive, entry, ids);
                if (entryId.has_value()) {
                    commands.add<prefab_link>(entity, prefab_link{*entryId});
                }
            }
        }
        const ecs::entity root = report.entities[static_cast<usize>(copy) * entryCount];
        if (!world.has_raw(root, ids.prefabInstance)) {
            assets::asset_ref<prefab_asset> reference(asset.id);
            if (target.assets() != nullptr && !asset.id.is_null()) {
                reference = target.assets()->bind<prefab_asset>(asset.id);
            }
            commands.add<prefab_instance>(root, prefab_instance{reference});
        }
    }
    const ecs::flush_report flushed = world.flush(commands);
    CONJURE_UNUSED(flushed);
    return core::result<void>::success();
}

ecs::entity instance_root_of(const scene &source, ecs::entity entity) noexcept {
    const scene_component_ids &ids = source.ids();
    ecs::entity current = entity;
    for (u32 step = 0; step < MAX_HIERARCHY_DEPTH && !current.is_null() && source.contains(current); ++step) {
        if (source.world().has_raw(current, ids.prefabInstance)) {
            return current;
        }
        current = source.tree().parent_of(current);
    }
    return ecs::NULL_ENTITY;
}

core::result<prefab_diff> diff(const scene &source, ecs::entity root) {
    const ecs::world &world = source.world();
    const scene_component_ids &ids = source.ids();
    const prefab_instance *instance = world.get<prefab_instance>(root);
    if (instance == nullptr) {
        return not_instance(root);
    }
    const prefab_asset *asset = instance->prefab.get();
    if (asset == nullptr || asset->archive == nullptr) {
        return not_loaded(root, instance->prefab.id());
    }
    const ecs::entity_archive &assetArchive = *asset->archive;
    CONJURE_TRY_ASSIGN(const ecs::entity_archive instanceArchive, scene_serializer::capture_subtree(source, root));
    std::vector<ecs::entity> instanceEntities = {};
    source.tree().collect_subtree(root, instanceEntities);

    // Match entries through the prefab_link guids.
    std::unordered_map<core::uuid, u32> assetByGuid = {};
    for (u32 entry = 0; entry < assetArchive.entity_count(); ++entry) {
        const std::optional<core::uuid> id = detail::entry_guid(assetArchive, entry, ids);
        if (id.has_value()) {
            assetByGuid.emplace(*id, entry);
        }
    }
    const u32 instanceCount = instanceArchive.entity_count();
    std::vector<u32> matched(instanceCount, detail::NO_ENTRY);
    std::vector<core::uuid> linkGuids(instanceCount);
    std::vector<bool> assetSeen(assetArchive.entity_count(), false);
    ecs::entity_remap remap = {};
    remap.resize(instanceCount);
    for (u32 entry = 0; entry < instanceCount; ++entry) {
        const std::optional<prefab_link> link =
          detail::read_component<prefab_link>(instanceArchive, entry, ids.prefabLink);
        if (!link.has_value() || link->assetGuid.is_null()) {
            continue;
        }
        linkGuids[entry] = link->assetGuid;
        const auto found = assetByGuid.find(link->assetGuid);
        if (found == assetByGuid.end() || assetSeen[found->second]) {
            continue;
        }
        matched[entry] = found->second;
        assetSeen[found->second] = true;
        remap.set(entry, ecs::entity::archive_local(found->second));
    }

    const ecs::component_registry &registry = world.registry();
    const std::span<const ecs::archive_entry> instanceEntries = instanceArchive.entries();
    const std::span<const ecs::archive_entry> assetEntries = assetArchive.entries();
    prefab_diff result = {};
    for (u32 entry = 0; entry < instanceCount; ++entry) {
        if (matched[entry] == detail::NO_ENTRY) {
            entity_diff added = {};
            added.assetGuid = linkGuids[entry];
            added.instanceEntity = instanceEntities[entry];
            added.kind = entity_diff_kind::ADDED;
            result.entries.push_back(std::move(added));
            continue;
        }
        const ecs::archive_entry &instanceEntry = instanceEntries[entry];
        const ecs::archive_entry &assetEntry = assetEntries[matched[entry]];
        std::vector<core::string_id> changed = {};
        const ecs::component_mask combined = instanceEntry.mask | assetEntry.mask;
        combined.for_each_set([&](ecs::component_id id) {
            if (id == ids.guid || id == ids.prefabLink || id == ids.prefabInstance) {
                return;
            }
            const ecs::component_info &info = registry.info(id);
            if (info.is_transient()) {
                return;
            }
            const ecs::archive_component *mine = find_component(instanceEntry, id);
            const ecs::archive_component *theirs = find_component(assetEntry, id);
            if (mine == nullptr || theirs == nullptr) {
                changed.push_back(info.name);
                return;
            }
            if (id == ids.parent) {
                parent myLink = {};
                parent theirLink = {};
                std::memcpy(&myLink, mine->bytes.data(), sizeof(parent));
                std::memcpy(&theirLink, theirs->bytes.data(), sizeof(parent));
                const ecs::entity myParent = remap.resolve(myLink.value);
                if (!(myParent == theirLink.value) || myLink.siblingIndex != theirLink.siblingIndex) {
                    changed.push_back(info.name);
                }
                return;
            }
            if (info.is_tag()) {
                return;
            }
            const detail::component_image image = remapped_image(info, mine->bytes, remap);
            const std::optional<std::string> myText = component_text(info, image.data());
            const std::optional<std::string> theirText = component_text(info, theirs->bytes.data());
            if (!myText.has_value() || !theirText.has_value() || *myText != *theirText) {
                changed.push_back(info.name);
            }
        });
        if (!changed.empty()) {
            entity_diff modified = {};
            modified.assetGuid = linkGuids[entry];
            modified.instanceEntity = instanceEntities[entry];
            modified.kind = entity_diff_kind::MODIFIED;
            modified.components = std::move(changed);
            result.entries.push_back(std::move(modified));
        }
    }
    for (u32 entry = 0; entry < assetArchive.entity_count(); ++entry) {
        if (assetSeen[entry]) {
            continue;
        }
        entity_diff removed = {};
        removed.assetGuid = detail::entry_guid(assetArchive, entry, ids).value_or(core::uuid());
        removed.kind = entity_diff_kind::REMOVED;
        result.entries.push_back(std::move(removed));
    }
    return result;
}

core::result<void> revert(scene &target, ecs::entity root) {
    ecs::world &world = target.world();
    const prefab_instance *instance = world.get<prefab_instance>(root);
    if (instance == nullptr) {
        return not_instance(root);
    }
    const prefab_asset *asset = instance->prefab.get();
    if (asset == nullptr || asset->archive == nullptr) {
        return not_loaded(root, instance->prefab.id());
    }
    const std::optional<core::uuid> rootGuid = target.guid_of(root);
    const core::string_id rootName = target.name_of(root);
    const bool hasName = world.has_raw(root, target.ids().name);
    const std::optional<math::transform> rootLocal = target.local_transform_of(root);
    const ecs::entity parentEntity = target.tree().parent_of(root);
    const u32 index = parentEntity.is_null() ? APPEND_CHILD : target.tree().sibling_index(root);
    const std::optional<scene_id> owner = target.root_owner(root);
    u32 rootPosition = index;
    if (parentEntity.is_null()) {
        const std::span<const root_entry> roots = target.roots();
        for (u32 slot = 0; slot < roots.size(); ++slot) {
            if (roots[slot].entity == root) {
                rootPosition = slot;
                break;
            }
        }
    }

    // The asset payload stays resident across the destroy: keep the archive alive by value.
    const prefab_asset kept = *asset;
    CONJURE_TRY(target.destroy_now(root));

    instantiate_options options = {};
    options.parent = parentEntity;
    options.index = rootPosition;
    options.owner = owner.value_or(scene_id{});
    if (hasName) {
        options.rootName = rootName;
    }
    if (rootLocal.has_value()) {
        options.rootTransform = local_transform::from_transform(*rootLocal);
    }
    CONJURE_TRY_ASSIGN(const ecs::entity created, instantiate(target, kept, options));
    if (rootGuid.has_value()) {
        CONJURE_TRY(target.set_guid(created, *rootGuid));
    }
    return core::result<void>::success();
}

core::result<prefab_document> apply(const scene &source, ecs::entity root) {
    const ecs::world &world = source.world();
    const scene_component_ids &ids = source.ids();
    const prefab_instance *instance = world.get<prefab_instance>(root);
    if (instance == nullptr) {
        return not_instance(root);
    }
    CONJURE_TRY_ASSIGN(const prefab_document captured, build_document(source, root, source.name_of(root).view()));
    const ecs::component_registry &registry = world.registry();
    prefab_document document = {};
    document.name = captured.name;
    document.archive = std::make_shared<const ecs::entity_archive>(
      detail::clone_archive(*captured.archive, [&](u32 index, ecs::archive_entry &entry) {
          const std::optional<prefab_link> link =
            detail::read_component<prefab_link>(*captured.archive, index, ids.prefabLink);
          const core::uuid entryGuid =
            link.has_value() && !link->assetGuid.is_null() ? link->assetGuid : core::uuid::generate();
          detail::set_entry_component(entry, ids.guid, guid{entryGuid});
          detail::set_entry_component(entry, ids.prefabLink, prefab_link{entryGuid});
          if (index == 0) {
              detail::remove_entry_component(registry, entry, ids.prefabInstance);
          }
      })
    );
    return document;
}

u32 refresh_instances(scene &target, const assets::asset_id &prefab, bool onlyUnmodified) {
    std::vector<ecs::entity> candidates = {};
    target.world().view<const prefab_instance>().for_each([&](ecs::entity entity, const prefab_instance &instance) {
        if (instance.prefab.id() == prefab) {
            candidates.push_back(entity);
        }
    });
    u32 reverted = 0;
    for (const ecs::entity root : candidates) {
        if (!target.contains(root)) {
            continue;
        }
        if (onlyUnmodified) {
            const core::result<prefab_diff> changes = diff(target, root);
            if (!changes.has_value() || !changes.value().empty()) {
                continue;
            }
        }
        if (revert(target, root).has_value()) {
            ++reverted;
        }
    }
    return reverted;
}

core::result<void> unpack(scene &target, ecs::entity root, bool recursive) {
    ecs::world &world = target.world();
    const scene_component_ids &ids = target.ids();
    if (!world.has_raw(root, ids.prefabInstance)) {
        return not_instance(root);
    }
    ecs::command_buffer commands(world);
    core::small_vector<ecs::entity, 64> stack = {};
    stack.push_back(root);
    while (!stack.empty()) {
        const ecs::entity current = stack.back();
        stack.pop_back();
        const ecs::component_mask mask = world.mask_of(current);
        const bool nestedRoot = !(current == root) && mask.test(ids.prefabInstance);
        if (nestedRoot && !recursive) {
            continue;
        }
        if (mask.test(ids.prefabInstance)) {
            commands.remove_raw(current, ids.prefabInstance);
        }
        if (mask.test(ids.prefabLink)) {
            commands.remove_raw(current, ids.prefabLink);
        }
        for (const ecs::entity child : target.tree().children_of(current)) {
            stack.push_back(child);
        }
    }
    const ecs::flush_report report = world.flush(commands);
    CONJURE_UNUSED(report);
    return core::result<void>::success();
}
}


/*===========================================================================*/
/*===========================================================================*/
// prefab_builder

prefab_builder::prefab_builder(const ecs::component_registry &registry) :
    mRegistry(&registry),
    mIds(scene_component_ids::resolve(registry)) {}

prefab_builder::~prefab_builder() {
    destroy_images();
}

u32 prefab_builder::add_node(u32 parentIndex, std::string_view name, const local_transform &transform) {
    node added = {};
    added.parentIndex = parentIndex;
    added.name = core::string_id(name);
    added.transform = transform;
    mNodes.push_back(std::move(added));
    return static_cast<u32>(mNodes.size() - 1);
}

core::result<void> prefab_builder::add_component_json(
  u32 node,
  core::string_id component,
  const core::json::value &value
) {
    if (node >= mNodes.size()) {
        return core::make_error(scene_error::BUILDER_INVALID, "node {} does not exist", node);
    }
    const ecs::component_id id = mRegistry->find(component);
    if (!id.is_valid() || id.value == 0) {
        return ecs::make_error(
          ecs::ecs_error::UNKNOWN_COMPONENT,
          "component '" + component.to_string() + "' is not registered"
        );
    }
    if (is_reserved(id)) {
        return core::make_error(scene_error::BUILDER_INVALID, "component '{}' is owned by the builder", component);
    }
    const ecs::component_info &info = mRegistry->info(id);
    if (info.is_tag()) {
        return add_tag(node, id);
    }
    // A fresh image (zeroed bytes) default-constructed in place, or the registered default value.
    detail::component_image image(info, nullptr);
    if (info.ops.constructDefault != nullptr) {
        info.ops.constructDefault(image.data(), 1);
    } else if (!info.defaultValue.empty()) {
        std::memcpy(image.data(), info.defaultValue.data(), info.size);
    }
    CONJURE_TRY(ecs::read_component_json(info, image.data(), value));
    std::vector<node_component> &components = mNodes[node].components;
    for (node_component &existing : components) {
        if (existing.id == id) {
            if (info.ops.destruct != nullptr) {
                info.ops.destruct(existing.bytes.data(), 1);
            }
            existing.bytes = image.release();
            return core::result<void>::success();
        }
    }
    components.push_back(node_component{id, image.release()});
    return core::result<void>::success();
}

core::result<void> prefab_builder::add_component_raw(
  u32 node,
  ecs::component_id component,
  std::span<const std::byte> bytes
) {
    if (node >= mNodes.size()) {
        return core::make_error(scene_error::BUILDER_INVALID, "node {} does not exist", node);
    }
    if (!mRegistry->is_registered(component) || component.value == 0) {
        return ecs::make_error(ecs::ecs_error::UNKNOWN_COMPONENT, "component id is not registered");
    }
    if (is_reserved(component)) {
        return core::make_error(scene_error::BUILDER_INVALID, "component {} is owned by the builder", component.value);
    }
    const ecs::component_info &info = mRegistry->info(component);
    if (info.is_tag()) {
        return add_tag(node, component);
    }
    if (bytes.size() != info.size) {
        return core::make_error(scene_error::BUILDER_INVALID, "component '{}' needs {} bytes, {} given", info.name,
          info.size, bytes.size());
    }
    detail::component_image image(info, bytes.data());
    std::vector<node_component> &components = mNodes[node].components;
    for (node_component &existing : components) {
        if (existing.id == component) {
            if (info.ops.destruct != nullptr) {
                info.ops.destruct(existing.bytes.data(), 1);
            }
            existing.bytes = image.release();
            return core::result<void>::success();
        }
    }
    components.push_back(node_component{component, image.release()});
    return core::result<void>::success();
}

core::result<void> prefab_builder::add_tag(u32 node, ecs::component_id component) {
    if (node >= mNodes.size()) {
        return core::make_error(scene_error::BUILDER_INVALID, "node {} does not exist", node);
    }
    if (!mRegistry->is_registered(component) || component.value == 0) {
        return ecs::make_error(ecs::ecs_error::UNKNOWN_COMPONENT, "component id is not registered");
    }
    if (is_reserved(component)) {
        return core::make_error(scene_error::BUILDER_INVALID, "component {} is owned by the builder", component.value);
    }
    if (!mRegistry->info(component).is_tag()) {
        return core::make_error(scene_error::BUILDER_INVALID, "component '{}' is not a tag",
          mRegistry->info(component).name);
    }
    std::vector<ecs::component_id> &tags = mNodes[node].tags;
    if (std::find(tags.begin(), tags.end(), component) == tags.end()) {
        tags.push_back(component);
    }
    return core::result<void>::success();
}

core::result<prefab_document> prefab_builder::build(std::string_view name) const {
    const u32 count = static_cast<u32>(mNodes.size());
    u32 rootCount = 0;
    for (const node &entry : mNodes) {
        if (entry.parentIndex == NO_PARENT) {
            ++rootCount;
        } else if (entry.parentIndex >= count) {
            return core::make_error(scene_error::BUILDER_INVALID, "node parent {} does not exist", entry.parentIndex);
        }
    }
    if (rootCount != 1) {
        return core::make_error(scene_error::BUILDER_INVALID, "a prefab needs exactly one root node, {} given",
          rootCount);
    }

    // Hierarchy order: the root, then depth first with children in insertion order.
    std::vector<std::vector<u32>> childLists(count);
    u32 root = 0;
    for (u32 index = 0; index < count; ++index) {
        if (mNodes[index].parentIndex == NO_PARENT) {
            root = index;
        } else {
            childLists[mNodes[index].parentIndex].push_back(index);
        }
    }
    std::vector<u32> order = {};
    std::vector<u32> position(count, detail::NO_ENTRY);
    std::vector<u32> stack = {root};
    while (!stack.empty()) {
        const u32 current = stack.back();
        stack.pop_back();
        position[current] = static_cast<u32>(order.size());
        order.push_back(current);
        const std::vector<u32> &list = childLists[current];
        for (usize slot = list.size(); slot > 0; --slot) {
            stack.push_back(list[slot - 1]);
        }
    }
    if (order.size() != count) {
        return core::make_error(scene_error::BUILDER_INVALID, "node parents form a cycle");
    }
    for (const u32 index : order) {
        u32 depth = 0;
        u32 current = index;
        while (mNodes[current].parentIndex != NO_PARENT) {
            current = mNodes[current].parentIndex;
            ++depth;
        }
        if (depth >= MAX_HIERARCHY_DEPTH) {
            return core::make_error(scene_error::HIERARCHY_TOO_DEEP, "node {} sits at depth {}", index, depth);
        }
    }

    ecs::entity_archive archive(*mRegistry);
    for (const u32 index : order) {
        const node &source = mNodes[index];
        ecs::archive_entry entry = {};
        const core::uuid id = core::uuid::generate();
        detail::set_entry_component(entry, mIds.guid, guid{id});
        detail::set_entry_component(entry, mIds.name, name_component{source.name});
        detail::set_entry_component(entry, mIds.localTransform, source.transform);
        detail::set_entry_component(entry, mIds.prefabLink, prefab_link{id});
        if (source.parentIndex != NO_PARENT) {
            parent link = {};
            link.value = ecs::entity::archive_local(position[source.parentIndex]);
            const std::vector<u32> &siblings = childLists[source.parentIndex];
            link.siblingIndex = static_cast<u32>(std::find(siblings.begin(), siblings.end(), index) - siblings.begin());
            detail::set_entry_component(entry, mIds.parent, link);
        }
        for (const node_component &component : source.components) {
            const ecs::component_info &info = mRegistry->info(component.id);
            detail::component_image image(info, component.bytes.data());
            ecs::archive_component copy = {};
            copy.id = component.id;
            copy.bytes = image.release();
            entry.components.push_back(std::move(copy));
            entry.mask.set(component.id);
        }
        for (const ecs::component_id tagId : source.tags) {
            ecs::archive_component copy = {};
            copy.id = tagId;
            entry.components.push_back(std::move(copy));
            entry.mask.set(tagId);
        }
        std::sort(entry.components.begin(), entry.components.end(),
          [](const ecs::archive_component &left, const ecs::archive_component &right) {
              return left.id.value < right.id.value;
          });
        archive.push_entry(std::move(entry));
    }
    CONJURE_TRY(archive.validate());

    prefab_document document = {};
    document.name = std::string(name);
    document.archive = std::make_shared<const ecs::entity_archive>(std::move(archive));
    return document;
}

bool prefab_builder::is_reserved(ecs::component_id id) const noexcept {
    // Everything build() writes from the node itself: a second copy through add_component_* would make
    // the entry carry the component twice (ARCHIVE_CORRUPT at build) instead of BUILDER_INVALID now.
    const bool structural = id == mIds.parent || id == mIds.children || id == mIds.guid || id == mIds.prefabLink;
    const bool authored = id == mIds.name || id == mIds.localTransform;
    const bool companion = id == mIds.worldTransform || id == mIds.renderWorldTransform || id == mIds.transformState
      || id == mIds.transformDirty || id == mIds.inactive;
    if (structural || authored || companion) {
        return true;
    }
    for (u32 level = 1; level < MAX_HIERARCHY_DEPTH; ++level) {
        if (id == mIds.depth[level]) {
            return true;
        }
    }
    return false;
}

void prefab_builder::destroy_images() noexcept {
    for (node &entry : mNodes) {
        for (node_component &component : entry.components) {
            if (component.bytes.empty()) {
                continue;
            }
            const ecs::component_info &info = mRegistry->info(component.id);
            if (info.ops.destruct != nullptr) {
                info.ops.destruct(component.bytes.data(), 1);
            }
            component.bytes.clear();
        }
    }
    mNodes.clear();
}
}
