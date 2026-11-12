#include "scene/archive_tools.h"

#include "conjure/scene/scene_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/ecs/serialization.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

using namespace conjure;
using namespace conjure::scene;
using namespace conjure::scene::detail;

namespace {

    /// <summary>
    /// The archive-local parent entry of one entry, or NO_ENTRY for an absent, null or external parent.
    /// </summary>
    [[nodiscard]] u32 local_parent(
      const ecs::entity_archive &archive,
      u32 entryIndex,
      const scene_component_ids &ids
    ) noexcept {
        const std::optional<parent> link = read_component<parent>(archive, entryIndex, ids.parent);
        if (!link.has_value() || !link->value.is_archive_local() || link->value.index >= archive.entity_count()) {
            return NO_ENTRY;
        }
        return link->value.index;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// component_image

component_image::component_image(const ecs::component_info &info, const void *source) :
    mInfo(&info),
    mBytes(info.size, std::byte{0}) {
    if (info.size == 0 || source == nullptr) {
        return;
    }
    if (info.ops.copy != nullptr) {
        info.ops.copy(mBytes.data(), source, 1);
    } else {
        std::memcpy(mBytes.data(), source, info.size);
    }
}

component_image::~component_image() {
    destroy();
}

component_image::component_image(component_image &&other) noexcept :
    mInfo(other.mInfo),
    mBytes(std::move(other.mBytes)) {
    other.mBytes.clear();
    other.mInfo = nullptr;
}

component_image &component_image::operator=(component_image &&other) noexcept {
    if (this != &other) {
        destroy();
        mInfo = other.mInfo;
        mBytes = std::move(other.mBytes);
        other.mBytes.clear();
        other.mInfo = nullptr;
    }
    return *this;
}

std::vector<std::byte> component_image::release() noexcept {
    std::vector<std::byte> bytes = std::move(mBytes);
    mBytes.clear();
    mInfo = nullptr;
    return bytes;
}

void component_image::destroy() noexcept {
    if (mInfo != nullptr && mInfo->ops.destruct != nullptr && !mBytes.empty()) {
        mInfo->ops.destruct(mBytes.data(), 1);
    }
    mBytes.clear();
    mInfo = nullptr;
}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

std::optional<core::uuid> conjure::scene::detail::entry_guid(
  const ecs::entity_archive &archive,
  u32 entryIndex,
  const scene_component_ids &ids
) noexcept {
    const std::optional<guid> value = read_component<guid>(archive, entryIndex, ids.guid);
    if (!value.has_value() || value->value.is_null()) {
        return std::nullopt;
    }
    return value->value;
}

void conjure::scene::detail::remove_entry_component(
  const ecs::component_registry &registry,
  ecs::archive_entry &entry,
  ecs::component_id id
) {
    for (usize index = 0; index < entry.components.size(); ++index) {
        ecs::archive_component &component = entry.components[index];
        if (!(component.id == id)) {
            continue;
        }
        const ecs::component_info &info = registry.info(id);
        if (info.ops.destruct != nullptr && !component.bytes.empty()) {
            info.ops.destruct(component.bytes.data(), 1);
        }
        component.bytes.clear();
        entry.components.erase(entry.components.begin() + static_cast<isize>(index));
        entry.mask.clear(id);
        return;
    }
}

ecs::archive_entry conjure::scene::detail::clone_entry(
  const ecs::component_registry &registry,
  const ecs::archive_entry &source
) {
    ecs::archive_entry copy = {};
    copy.mask = source.mask;
    copy.components.reserve(source.components.size());
    for (const ecs::archive_component &component : source.components) {
        ecs::archive_component cloned = {};
        cloned.id = component.id;
        if (!component.bytes.empty()) {
            component_image image(registry.info(component.id), component.bytes.data());
            cloned.bytes = image.release();
        }
        copy.components.push_back(std::move(cloned));
    }
    return copy;
}

ecs::entity_archive conjure::scene::detail::clone_archive(
  const ecs::entity_archive &source,
  const std::function<void(u32, ecs::archive_entry &)> &edit
) {
    const ecs::component_registry &registry = source.registry();
    ecs::entity_archive copy(registry);
    const std::span<const ecs::archive_entry> entries = source.entries();
    for (u32 index = 0; index < entries.size(); ++index) {
        ecs::archive_entry entry = clone_entry(registry, entries[index]);
        if (edit) {
            edit(index, entry);
        }
        copy.push_entry(std::move(entry));
    }
    return copy;
}

std::vector<u32> conjure::scene::detail::archive_parents(
  const ecs::entity_archive &archive,
  const scene_component_ids &ids
) {
    const u32 count = archive.entity_count();
    std::vector<u32> parents(count, NO_ENTRY);
    for (u32 index = 0; index < count; ++index) {
        parents[index] = local_parent(archive, index, ids);
    }
    return parents;
}

std::vector<u32> conjure::scene::detail::archive_roots(
  const ecs::entity_archive &archive,
  const scene_component_ids &ids
) {
    const std::vector<u32> parents = archive_parents(archive, ids);
    std::vector<u32> roots = {};
    for (u32 index = 0; index < parents.size(); ++index) {
        if (parents[index] == NO_ENTRY) {
            roots.push_back(index);
        }
    }
    return roots;
}

core::result<archive_shape> conjure::scene::detail::compute_archive_shape(
  const ecs::entity_archive &archive,
  const scene_component_ids &ids
) {
    archive_shape shape = {};
    shape.parents = archive_parents(archive, ids);
    const u32 count = static_cast<u32>(shape.parents.size());
    shape.depth.assign(count, 0);
    shape.order.reserve(count);

    // Children lists through a counting sort so the pass costs O(n) and keeps archive order.
    std::vector<u32> childCount(count + 1, 0);
    for (u32 index = 0; index < count; ++index) {
        if (shape.parents[index] == NO_ENTRY) {
            shape.roots.push_back(index);
        } else {
            ++childCount[shape.parents[index] + 1];
        }
    }
    for (u32 index = 1; index <= count; ++index) {
        childCount[index] += childCount[index - 1];
    }
    std::vector<u32> childList(count, 0);
    std::vector<u32> fill(childCount.begin(), childCount.end() - 1);
    for (u32 index = 0; index < count; ++index) {
        const u32 owner = shape.parents[index];
        if (owner != NO_ENTRY) {
            childList[fill[owner]] = index;
            ++fill[owner];
        }
    }

    std::vector<u32> stack = {};
    for (usize rootIndex = shape.roots.size(); rootIndex > 0; --rootIndex) {
        stack.push_back(shape.roots[rootIndex - 1]);
    }
    while (!stack.empty()) {
        const u32 current = stack.back();
        stack.pop_back();
        shape.order.push_back(current);
        const u32 depth = shape.depth[current];
        const u32 begin = childCount[current];
        const u32 end = childCount[current + 1];
        for (u32 slot = end; slot > begin; --slot) {
            const u32 child = childList[slot - 1];
            if (depth + 1 >= MAX_HIERARCHY_DEPTH) {
                return core::make_error(
                  scene_error::HIERARCHY_TOO_DEEP,
                  "archive entry {} would sit below depth {}",
                  child,
                  MAX_HIERARCHY_DEPTH - 1
                );
            }
            shape.depth[child] = static_cast<u8>(depth + 1);
            stack.push_back(child);
        }
    }
    if (shape.order.size() != count) {
        return core::make_error(scene_error::HIERARCHY_CYCLE, "archive parent links form a cycle");
    }
    return shape;
}

core::result<void> conjure::scene::detail::check_unique_guids(
  const ecs::entity_archive &archive,
  const scene_component_ids &ids
) {
    std::unordered_set<core::uuid> seen = {};
    for (u32 index = 0; index < archive.entity_count(); ++index) {
        const std::optional<core::uuid> id = entry_guid(archive, index, ids);
        if (id.has_value() && !seen.insert(*id).second) {
            return core::make_error(scene_error::DUPLICATE_GUID, "guid {} appears twice in the archive", *id);
        }
    }
    return core::result<void>::success();
}

prefab_root_report conjure::scene::detail::inspect_prefab_roots(
  const ecs::entity_archive &archive,
  const scene_component_ids &ids
) noexcept {
    prefab_root_report report = {};
    const u32 count = archive.entity_count();
    if (count == 0) {
        report.problem = prefab_root_problem::EMPTY;
        return report;
    }
    u32 rootCount = 0;
    u32 firstRoot = NO_ENTRY;
    for (u32 index = 0; index < count; ++index) {
        if (local_parent(archive, index, ids) != NO_ENTRY) {
            continue;
        }
        if (rootCount == 0) {
            firstRoot = index;
        }
        ++rootCount;
    }
    if (rootCount == 1 && firstRoot != 0) {
        report.problem = prefab_root_problem::MISPLACED_ROOT;
        report.value = firstRoot;
    } else if (rootCount != 1) {
        report.problem = prefab_root_problem::ROOT_COUNT;
        report.value = rootCount;
    }
    return report;
}

core::error conjure::scene::detail::prefab_roots_error(const prefab_root_report &report, std::string_view what) {
    CONJURE_ASSERT(report.problem != prefab_root_problem::NONE, "prefab_roots_error: '{}' has the prefab shape", what);
    if (report.problem == prefab_root_problem::EMPTY) {
        return core::make_error(scene_error::BAD_FORMAT, "{}: the archive is empty; a prefab needs one root entry",
          what);
    }
    if (report.problem == prefab_root_problem::MISPLACED_ROOT) {
        return core::make_error(scene_error::PREFAB_MULTIPLE_ROOTS,
          "{}: the only root is entry {}; a prefab's root must be entry 0", what, report.value);
    }
    return core::make_error(scene_error::PREFAB_MULTIPLE_ROOTS,
      "{}: a prefab needs exactly one root at entry 0; found {} roots", what, report.value);
}

core::result<void> conjure::scene::detail::check_loadable(
  const ecs::entity_archive &archive,
  const scene_component_ids &ids
) {
    CONJURE_TRY(archive.validate());
    CONJURE_TRY(compute_archive_shape(archive, ids));
    return check_unique_guids(archive, ids);
}

u32 conjure::scene::detail::count_unresolved(const ecs::entity_archive &archive) {
    u32 total = 0;
    for (u32 index = 0; index < archive.entity_count(); ++index) {
        const std::span<const std::byte> bytes = archive.component_bytes(index, UNRESOLVED_COMPONENTS_ID);
        if (bytes.size() != sizeof(ecs::unresolved_components)) {
            continue;
        }
        const ecs::unresolved_components *unresolved =
          reinterpret_cast<const ecs::unresolved_components *>(bytes.data());
        total += static_cast<u32>(unresolved->entries.size());
    }
    return total;
}

std::vector<std::string> conjure::scene::detail::unresolved_names(const ecs::entity_archive &archive) {
    std::vector<std::string> names = {};
    for (u32 index = 0; index < archive.entity_count(); ++index) {
        const std::span<const std::byte> bytes = archive.component_bytes(index, UNRESOLVED_COMPONENTS_ID);
        if (bytes.size() != sizeof(ecs::unresolved_components)) {
            continue;
        }
        const ecs::unresolved_components *unresolved =
          reinterpret_cast<const ecs::unresolved_components *>(bytes.data());
        for (const ecs::unresolved_component &entry : unresolved->entries) {
            names.push_back(entry.name);
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

std::vector<assets::asset_id> conjure::scene::detail::collect_asset_refs(const ecs::entity_archive &archive) {
    const ecs::component_registry &registry = archive.registry();
    std::vector<assets::asset_id> ids = {};
    for (const ecs::archive_entry &entry : archive.entries()) {
        for (const ecs::archive_component &component : entry.components) {
            const ecs::component_info &info = registry.info(component.id);
            for (const u32 offset : info.assetRefFieldOffsets) {
                if (offset + sizeof(assets::asset_id) > component.bytes.size()) {
                    continue;
                }
                assets::asset_id id = {};
                std::memcpy(&id, component.bytes.data() + offset, sizeof(assets::asset_id));
                if (!id.is_null()) {
                    ids.push_back(id);
                }
            }
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

std::shared_ptr<const ecs::entity_archive> conjure::scene::detail::borrow_archive(
  const ecs::entity_archive &archive
) noexcept {
    // The aliasing constructor with an empty owner: a shared_ptr that points without owning.
    return std::shared_ptr<const ecs::entity_archive>(std::shared_ptr<const ecs::entity_archive>(), &archive);
}
