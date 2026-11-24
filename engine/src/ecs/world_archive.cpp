#include "conjure/ecs/world.h"

#include "ecs/chunk.h"
#include "ecs/entity_allocator.h"

#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

// Archive instantiation (docs/design/01_ecs.md 3.6).

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<entity> world::instantiate(
  const std::shared_ptr<const entity_archive> &archive,
  command_buffer *commands
) {
    entity root = {};
    CONJURE_TRY(instantiate_batch(archive, std::span<entity>(&root, 1), commands));
    return root;
}

core::result<void> world::instantiate_batch(
  const std::shared_ptr<const entity_archive> &archive,
  std::span<entity> outRoots,
  command_buffer *commands,
  std::vector<entity> *outEntities
) {
    // The id list is emptied before any early return so a caller reusing the vector never reads the
    // ids of an earlier call after a failure.
    if (outEntities != nullptr) {
        outEntities->clear();
    }
    if (archive == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "instantiate_batch needs an archive");
    }
    if (&archive->registry() != mRegistry) {
        return core::make_error(ecs_error::ARCHIVE_CORRUPT, "the archive belongs to another component registry");
    }
    if (commands != nullptr && &commands->target() != this) {
        // The ids would be reserved from, and placed in, the buffer's world instead of this one.
        CONJURE_ASSERT(false, "instantiate_batch with a command buffer of another world");
        return core::error(core::error_code::INVALID_ARGUMENT, "the command buffer targets another world");
    }
    CONJURE_TRY(archive->validate());
    const u32 count = archive->entity_count();
    const u32 copies = static_cast<u32>(outRoots.size());
    if (count == 0 || copies == 0) {
        std::fill(outRoots.begin(), outRoots.end(), NULL_ENTITY);
        return core::result<void>::success();
    }

    if (commands != nullptr) {
        CONJURE_TRY_ASSIGN(const std::span<const entity> ids, commands->push_instantiate(archive, copies));
        for (u32 copy = 0; copy < copies; ++copy) {
            outRoots[copy] = ids[static_cast<usize>(copy) * count];
        }
        if (outEntities != nullptr) {
            outEntities->assign(ids.begin(), ids.end());
        }
        return core::result<void>::success();
    }

    CONJURE_TRY(check_structural());
    std::vector<entity> ids(static_cast<usize>(copies) * count);
    CONJURE_TRY(mAllocator->reserve(ids));
    u32 highest = 0;
    for (const entity id : ids) {
        highest = std::max(highest, id.index);
    }
    const core::result<void> records = ensure_records(highest);
    if (!records.has_value()) {
        mAllocator->return_unused(ids);
        return records.get_error();
    }

    begin_structural();
    const core::result<void> created = instantiate_now(*archive, ids, copies);
    end_structural();
    if (!created.has_value()) {
        mAllocator->return_unused(ids);
        return created.get_error();
    }
    for (u32 copy = 0; copy < copies; ++copy) {
        outRoots[copy] = ids[static_cast<usize>(copy) * count];
    }
    if (outEntities != nullptr) {
        *outEntities = std::move(ids);
    }
    return core::result<void>::success();
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<void> world::instantiate_now(const entity_archive &archive, std::span<const entity> ids, u32 copies) {
    const std::span<const archive_entry> entries = archive.entries();
    const u32 count = static_cast<u32>(entries.size());
    if (count == 0 || copies == 0) {
        return core::result<void>::success();
    }
    if (ids.size() != static_cast<usize>(count) * copies) {
        return core::make_error(ecs_error::ARCHIVE_CORRUPT, "instantiate received the wrong number of ids");
    }
    bump_version();

    // Resolve every archetype before creating anything so a failure leaves the world unchanged.
    std::vector<archetype *> targets(count, nullptr);
    for (u32 entry = 0; entry < count; ++entry) {
        CONJURE_TRY_ASSIGN(targets[entry], archetype_for(entries[entry].mask));
    }

    // Place the copies of each entry together: one chunk-wise fill per archetype.
    std::vector<entity> group(copies);
    std::vector<entity> placed = {};
    placed.reserve(ids.size());
    for (u32 entry = 0; entry < count; ++entry) {
        for (u32 copy = 0; copy < copies; ++copy) {
            group[copy] = ids[static_cast<usize>(copy) * count + entry];
        }
        const core::result<void> rows = place_rows(*targets[entry], group);
        if (!rows.has_value()) {
            // Nothing was visible yet (payloads and observers come after every row is placed), so the
            // rows are discarded like place_rows' own undo; the caller returns the ids unused. A release
            // here would put a second generation of each index on the free list.
            for (const entity undo : placed) {
                discard_placed(undo);
            }
            return rows.get_error();
        }
        placed.insert(placed.end(), group.begin(), group.end());
    }

    // Payloads, then entity field rewiring per copy.
    entity_remap remap = {};
    remap.resize(count);
    for (u32 copy = 0; copy < copies; ++copy) {
        for (u32 entry = 0; entry < count; ++entry) {
            remap.set(entry, ids[static_cast<usize>(copy) * count + entry]);
        }
        for (u32 entry = 0; entry < count; ++entry) {
            const entity target = ids[static_cast<usize>(copy) * count + entry];
            for (const archive_component &component : entries[entry].components) {
                const component_info &info = mRegistry->info(component.id);
                if (info.is_tag()) {
                    continue;
                }
                void *cell = get_raw(target, component.id, false);
                if (info.ops.copy != nullptr) {
                    destruct_column(info, cell, 1);
                    info.ops.copy(cell, component.bytes.data(), 1);
                } else {
                    std::memcpy(cell, component.bytes.data(), info.size);
                }
                for (const u32 offset : info.entityFieldOffsets) {
                    std::byte *field = static_cast<std::byte *>(cell) + offset;
                    entity reference = {};
                    std::memcpy(&reference, field, sizeof(entity));
                    reference = remap.resolve(reference);
                    std::memcpy(field, &reference, sizeof(entity));
                }
                if (info.reflection == nullptr && info.serializer.remapEntities != nullptr) {
                    info.serializer.remapEntities(cell, remap);
                }
            }
        }
    }

    for (u32 copy = 0; copy < copies; ++copy) {
        for (u32 entry = 0; entry < count; ++entry) {
            fire_added(ids[static_cast<usize>(copy) * count + entry], entries[entry].mask);
        }
    }
    return core::result<void>::success();
}
