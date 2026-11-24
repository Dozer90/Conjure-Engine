#include "conjure/ecs/entity_archive.h"

#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"

#include "conjure/core/assert.h"

#include <cstring>
#include <format>
#include <unordered_map>
#include <utility>

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

entity_archive::entity_archive(const component_registry &registry) noexcept :
    mRegistry(&registry) {}

entity_archive::~entity_archive() {
    destroy_payloads();
}

entity_archive::entity_archive(entity_archive &&other) noexcept :
    mRegistry(other.mRegistry),
    mEntries(std::move(other.mEntries)) {
    other.mEntries.clear();
}

entity_archive &entity_archive::operator=(entity_archive &&other) noexcept {
    if (this != &other) {
        destroy_payloads();
        mRegistry = other.mRegistry;
        mEntries = std::move(other.mEntries);
        other.mEntries.clear();
    }
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<entity_archive> entity_archive::capture(
  const world &source,
  std::span<const entity> entities,
  archive_options options
) {
    if (source.is_locked()) {
        return core::make_error(ecs_error::STRUCTURE_LOCKED, "entity_archive::capture while the world is locked");
    }
    const component_registry &registry = source.registry();
    entity_archive archive(registry);

    std::unordered_map<entity, u32> localIndex = {};
    localIndex.reserve(entities.size());
    for (usize index = 0; index < entities.size(); ++index) {
        if (!source.is_alive(entities[index])) {
            return core::make_error(ecs_error::ENTITY_NOT_ALIVE, "entity {} is not alive", entities[index]);
        }
        localIndex.emplace(entities[index], static_cast<u32>(index));
    }

    for (const entity target : entities) {
        archive_entry entry = {};
        const component_mask mask = source.mask_of(target);
        mask.for_each_set([&](component_id id) {
            const component_info &info = registry.info(id);
            if (info.is_transient()) {
                return;
            }
            archive_component component = {};
            component.id = id;
            if (!info.is_tag()) {
                component.bytes.assign(info.size, std::byte{0});
                const void *data = source.get_raw(target, id);
                if (info.ops.copy != nullptr) {
                    info.ops.copy(component.bytes.data(), data, 1);
                } else {
                    std::memcpy(component.bytes.data(), data, info.size);
                }
            }
            entry.mask.set(id);
            entry.components.push_back(std::move(component));
        });
        archive.mEntries.push_back(std::move(entry));
    }

    // Internal references become local indices; external ones null unless kept. Reflection-less types
    // that store references of their own (R29 script sets) rewrite them through their hook with the
    // same table world::instantiate applies on the way back. Indexed, so each hook lookup is a binary
    // search instead of a scan over every captured entity.
    entity_remap table(std::vector<entity>(entities.begin(), entities.end()));
    table.index_targets();
    for (archive_entry &entry : archive.mEntries) {
        for (archive_component &component : entry.components) {
            const component_info &info = registry.info(component.id);
            if (info.reflection == nullptr && info.serializer.remapEntities != nullptr && !info.is_tag()) {
                info.serializer.remapEntities(component.bytes.data(), table);
            }
            for (const u32 offset : info.entityFieldOffsets) {
                entity reference = {};
                std::memcpy(&reference, component.bytes.data() + offset, sizeof(entity));
                if (reference.is_null()) {
                    continue;
                }
                const auto found = localIndex.find(reference);
                if (found != localIndex.end()) {
                    reference = entity::archive_local(found->second);
                } else if (!options.keepExternalReferences) {
                    reference = NULL_ENTITY;
                }
                std::memcpy(component.bytes.data() + offset, &reference, sizeof(entity));
            }
        }
    }
    return archive;
}

core::result<void> entity_archive::validate() const {
    const u32 count = entity_count();
    for (u32 index = 0; index < mEntries.size(); ++index) {
        const archive_entry &entry = mEntries[index];
        component_mask seen = {};
        for (const archive_component &component : entry.components) {
            if (!mRegistry->is_registered(component.id)) {
                return core::make_error(
                  ecs_error::ARCHIVE_CORRUPT,
                  std::format("entry {}: unregistered component id", index)
                );
            }
            if (!entry.mask.test(component.id)) {
                return core::make_error(
                  ecs_error::ARCHIVE_CORRUPT,
                  std::format("entry {}: component missing from the mask", index)
                );
            }
            if (seen.test(component.id)) {
                return core::make_error(ecs_error::ARCHIVE_CORRUPT, "entry {}: duplicate component", index);
            }
            seen.set(component.id);

            const component_info &info = mRegistry->info(component.id);
            const usize expected = info.is_tag() ? 0u : info.size;
            if (component.bytes.size() != expected) {
                return core::make_error(
                  ecs_error::ARCHIVE_CORRUPT,
                  std::format(
                    "entry {}: payload of '{}' has {} bytes, expected {}",
                    index,
                    info.name,
                    component.bytes.size(),
                    expected
                  )
                );
            }
            for (const u32 offset : info.entityFieldOffsets) {
                entity reference = {};
                std::memcpy(&reference, component.bytes.data() + offset, sizeof(entity));
                if (reference.is_archive_local() && reference.index >= count) {
                    return core::make_error(
                      ecs_error::ARCHIVE_CORRUPT,
                      std::format(
                        "entry {}: local reference {} is outside the archive of {} entries",
                        index,
                        reference.index,
                        count
                      )
                    );
                }
            }
        }
        if (seen != entry.mask) {
            return core::make_error(ecs_error::ARCHIVE_CORRUPT, "entry {}: mask bit without a component", index);
        }
    }
    return core::result<void>::success();
}

void entity_archive::push_entry(archive_entry entry) {
    mEntries.push_back(std::move(entry));
}

std::span<const std::byte> entity_archive::component_bytes(u32 entryIndex, component_id id) const noexcept {
    if (entryIndex >= mEntries.size()) {
        return {};
    }
    for (const archive_component &component : mEntries[entryIndex].components) {
        if (component.id == id) {
            return component.bytes;
        }
    }
    return {};
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void entity_archive::destroy_payloads() noexcept {
    for (archive_entry &entry : mEntries) {
        for (archive_component &component : entry.components) {
            if (component.bytes.empty() || !mRegistry->is_registered(component.id)) {
                continue;
            }
            const component_info &info = mRegistry->info(component.id);
            if (info.ops.destruct != nullptr) {
                info.ops.destruct(component.bytes.data(), 1);
            }
            component.bytes.clear();
        }
    }
    mEntries.clear();
}
