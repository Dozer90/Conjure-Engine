#include "conjure/scene/scene_snapshot.h"

#include "scene/archive_tools.h"

#include "conjure/scene/scene_serializer.h"

#include <utility>
#include <vector>

namespace conjure::scene {

/*===========================================================================*/
/*===========================================================================*/
// scene_snapshot

scene_snapshot::scene_snapshot(const ecs::component_registry &registry) noexcept :
    archive(registry) {}

scene_snapshot::~scene_snapshot() {}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

core::result<scene_snapshot> capture(const scene &source) {
    scene_snapshot snapshot(source.world().registry());
    std::vector<ecs::entity> entities = {};
    const std::span<const root_entry> roots = source.roots();
    snapshot.rootEntries.reserve(roots.size());
    snapshot.rootOwners.reserve(roots.size());
    for (const root_entry &entry : roots) {
        snapshot.rootEntries.push_back(static_cast<u32>(entities.size()));
        snapshot.rootOwners.push_back(entry.owner);
        source.tree().collect_subtree(entry.entity, entities);
    }
    CONJURE_TRY_ASSIGN(snapshot.archive, ecs::entity_archive::capture(source.world(), entities));
    // restore() would refuse this layer only after it cleared the scene: refuse it now, while nothing is lost.
    CONJURE_TRY(detail::check_layers(snapshot.archive, source.ids(), entities));
    snapshot.settings = source.settings();
    snapshot.name = std::string(source.name());
    snapshot.structureVersion = source.structure_version();
    return snapshot;
}

core::result<void> restore(scene &target, const scene_snapshot &snapshot) {
    target.clear();
    target.set_settings(snapshot.settings);
    target.set_name(snapshot.name);
    if (snapshot.archive.entity_count() == 0) {
        return core::result<void>::success();
    }

    instantiate_options options = {};
    options.guids = guid_policy::KEEP;
    ecs::entity root = {};
    const core::result<instantiate_report> instantiated = target.instantiate_archive(
      detail::borrow_archive(snapshot.archive),
      1,
      options,
      std::span<ecs::entity>(&root, 1)
    );
    if (!instantiated.has_value()) {
        target.clear();
        return instantiated.get_error();
    }

    // The archive is in hierarchy order, so the new roots already follow the snapshot's root order;
    // their owners come from the parallel list.
    const std::vector<ecs::entity> &entities = instantiated.value().entities;
    std::vector<ecs::entity> ordered = {};
    for (usize slot = 0; slot < snapshot.rootEntries.size(); ++slot) {
        const u32 entry = snapshot.rootEntries[slot];
        if (entry >= entities.size()) {
            continue;
        }
        const ecs::entity entity = entities[entry];
        if (!target.root_owner(entity).has_value()) {
            continue;
        }
        ordered.push_back(entity);
        const scene_id owner = slot < snapshot.rootOwners.size() ? snapshot.rootOwners[slot] : SCENE_ID_AUTHORED;
        CONJURE_TRY(target.set_root_owner(entity, owner));
    }
    CONJURE_TRY(target.set_root_order(ordered));
    return core::result<void>::success();
}
}
