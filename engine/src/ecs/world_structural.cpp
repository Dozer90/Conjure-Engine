#include "conjure/ecs/world.h"

#include "ecs/chunk.h"
#include "ecs/chunk_pool.h"
#include "ecs/entity_allocator.h"
#include "ecs/observer_table.h"

#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

// Row movement, entity lifecycle and command buffer flush (docs/design/01_ecs.md 3.1, 3.4).

using namespace conjure;
using namespace conjure::ecs;

namespace {
    /// <summary>
    /// Guard against observer chains that never settle (a cycle in caller data).
    /// </summary>
    constexpr u32 MAX_DRAIN_PASSES = 1024;

    /// <summary>
    /// "Not in the flush plan" marker of the slot table lookup.
    /// </summary>
    constexpr u32 NO_PLAN = std::numeric_limits<u32>::max();

    void relocate_cell(const component_info &info, void *destination, void *source) noexcept {
        if (info.ops.relocate != nullptr) {
            info.ops.relocate(destination, source, 1);
        } else {
            std::memcpy(destination, source, info.size);
        }
    }

    void copy_into_cell(const component_info &info, void *destination, const void *source) noexcept {
        if (info.ops.copy != nullptr) {
            // The destination holds a default-constructed object that the copy replaces.
            if (info.ops.destruct != nullptr) {
                info.ops.destruct(destination, 1);
            }
            info.ops.copy(destination, source, 1);
        } else {
            std::memcpy(destination, source, info.size);
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Public structural API

core::result<entity> world::create_entity(const component_mask &mask) {
    CONJURE_TRY(check_structural());
    CONJURE_TRY_ASSIGN(archetype *target, archetype_for(mask));

    entity created = {};
    CONJURE_TRY(mAllocator->reserve(std::span<entity>(&created, 1)));
    const core::result<void> records = ensure_records(created.index);
    if (!records.has_value()) {
        mAllocator->return_unused(std::span<const entity>(&created, 1));
        return records.get_error();
    }

    bump_version();
    begin_structural();
    const core::result<void> placed = place_rows(*target, std::span<const entity>(&created, 1));
    if (!placed.has_value()) {
        mAllocator->return_unused(std::span<const entity>(&created, 1));
        end_structural();
        return placed.get_error();
    }
    fire_added(created, mask);
    end_structural();
    return created;
}

core::result<void> world::create_entities(const component_mask &mask, std::span<entity> out) {
    CONJURE_TRY(check_structural());
    if (out.empty()) {
        return core::result<void>::success();
    }
    CONJURE_TRY_ASSIGN(archetype *target, archetype_for(mask));
    CONJURE_TRY(mAllocator->reserve(out));

    u32 highest = 0;
    for (const entity value : out) {
        highest = std::max(highest, value.index);
    }
    const core::result<void> records = ensure_records(highest);
    if (!records.has_value()) {
        mAllocator->return_unused(out);
        return records.get_error();
    }

    bump_version();
    begin_structural();
    const core::result<void> placed = place_rows(*target, out);
    if (!placed.has_value()) {
        mAllocator->return_unused(out);
        end_structural();
        return placed.get_error();
    }
    for (const entity value : out) {
        fire_added(value, mask);
    }
    end_structural();
    return core::result<void>::success();
}

core::result<void> world::destroy_entity(entity target) {
    CONJURE_TRY(check_structural());
    entity_record *record = find_alive(target);
    if (record == nullptr) {
        return make_error(ecs_error::ENTITY_NOT_ALIVE, "destroy_entity on a dead or stale entity");
    }
    bump_version();
    begin_structural();
    destroy_alive(target, *record, true);
    end_structural();
    return core::result<void>::success();
}

core::result<void *> world::add_raw(entity target, component_id id, std::span<const std::byte> initial) {
    CONJURE_TRY_ASSIGN(void *slot, add_storage(target, id));
    if (slot != nullptr && !initial.empty()) {
        const component_info &info = mRegistry->info(id);
        if (initial.size() != info.size) {
            CONJURE_ASSERT(false, "add_raw payload of {} bytes for a {} byte component", initial.size(), info.size);
        } else {
            copy_into_cell(info, slot, initial.data());
        }
    }
    finish_add(target, id);
    return get_raw(target, id, false);
}

core::result<void> world::remove_raw(entity target, component_id id) {
    CONJURE_TRY(check_structural());
    if (!mRegistry->is_registered(id)) {
        return make_error(ecs_error::UNKNOWN_COMPONENT, "remove of an unregistered component");
    }
    entity_record *record = find_alive(target);
    if (record == nullptr) {
        return make_error(ecs_error::ENTITY_NOT_ALIVE, "remove on a dead or stale entity");
    }
    if (!record->owner->has(id)) {
        return make_error(ecs_error::COMPONENT_NOT_PRESENT, "remove of a component the entity does not hold");
    }
    CONJURE_TRY_ASSIGN(archetype *destination, archetype_with(*record->owner, id, false));

    bump_version();
    begin_structural();
    const core::result<void> moved = move_entity(target, *record, *destination);
    end_structural();
    return moved;
}

core::result<void> world::remove_from_all(component_id id) {
    CONJURE_TRY(check_structural());
    if (!mRegistry->is_registered(id)) {
        return make_error(ecs_error::UNKNOWN_COMPONENT, "remove_from_all of an unregistered component");
    }

    std::vector<entity> holders = {};
    for (const archetype *owner : mArchetypePointers) {
        if (!owner->has(id)) {
            continue;
        }
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            const std::span<const entity> rows = owner->chunk_entities(chunkIndex);
            holders.insert(holders.end(), rows.begin(), rows.end());
        }
    }
    for (const entity holder : holders) {
        // An observer of an earlier removal may have destroyed this one already.
        if (!has_raw(holder, id)) {
            continue;
        }
        CONJURE_TRY(remove_raw(holder, id));
    }
    return core::result<void>::success();
}

flush_report world::flush(command_buffer &commands) {
    flush_report report = {};
    if (&commands.target() != this) {
        CONJURE_ASSERT(false, "flushing a command buffer into a world it was not created for");
        return report;
    }
    if (!check_structural().has_value()) {
        return report;
    }
    begin_structural();
    report = flush_internal(commands);
    end_structural();
    return report;
}


/*===========================================================================*/
/*===========================================================================*/
// Rows

core::result<void> world::place_rows(archetype &target, std::span<const entity> ids) {
    const chunk_layout &layout = target.mLayout;
    const u64 stamp = version();
    // The undo list is the world's scratch (used in place, not a fresh local: a vector object costs an
    // allocation under debug iterators), cleared on exit and kept at capacity, so a bulk create of any
    // size places without allocating once the list has reached that size before. The one call-out below
    // is the component's ops.constructDefault; a constructor that called back into its world would
    // re-enter place_rows over this list, which check_structural refuses while a structural call is open.
    std::vector<entity> &placed = mPlacedScratch;
    placed.clear();

    for (const entity id : ids) {
        const core::result<archetype::row_location> pushed = target.push_row(*mChunkPool);
        if (!pushed.has_value()) {
            // Undo the rows placed so far so the caller sees an unchanged world.
            for (const entity undo : placed) {
                discard_placed(undo);
            }
            placed.clear();
            return pushed.get_error();
        }

        const archetype::row_location location = pushed.value();
        chunk &block = *target.mChunks[location.chunkIndex];
        block.entities()[location.row] = id;
        for (u32 column = 0; column < layout.column_count(); ++column) {
            const component_info &info = mRegistry->info(layout.components[column]);
            construct_column_default(info, block.cell(layout, column, location.row), 1);
        }
        block.stamp_all(stamp);

        entity_record &record = mRecords[id.index];
        record.owner = &target;
        record.chunkIndex = location.chunkIndex;
        record.row = location.row;
        record.generation = id.generation;
        record.state = entity_state::ALIVE;
        ++mAliveCount;
        placed.push_back(id);
    }
    placed.clear();
    return core::result<void>::success();
}

void world::discard_placed(entity target) {
    // Undo of a place_rows row that never became visible: no observers fired, so none fire here, and
    // the id stays reserved (the caller returns it unused) so the record keeps its generation.
    entity_record &record = mRecords[target.index];
    archetype &owner = *record.owner;
    const chunk_layout &layout = owner.mLayout;
    chunk &block = *owner.mChunks[record.chunkIndex];
    for (u32 column = 0; column < layout.column_count(); ++column) {
        destruct_column(mRegistry->info(layout.components[column]), block.cell(layout, column, record.row), 1);
    }
    remove_row(owner, record.chunkIndex, record.row);
    record.state = entity_state::FREE;
    record.owner = nullptr;
    --mAliveCount;
}

void world::cancel_destroyed_copies(command_buffer::instantiate_entry &entry, std::span<const u64> destroyed) {
    const usize count = entry.archive == nullptr ? 0 : static_cast<usize>(entry.archive->entity_count());
    if (count == 0 || entry.ids.size() != count * entry.copies) {
        return;
    }
    // Copy-major ids (copy * count + entry): the surviving copies close up in place, ahead of where they were.
    u32 kept = 0;
    for (u32 copy = 0; copy < entry.copies; ++copy) {
        const usize first = static_cast<usize>(copy) * count;
        if (!std::binary_search(destroyed.begin(), destroyed.end(), entry.ids[first].to_u64())) {
            if (kept != copy) {
                std::copy_n(
                  entry.ids.begin() + static_cast<std::ptrdiff_t>(first),
                  count,
                  entry.ids.begin() + static_cast<std::ptrdiff_t>(static_cast<usize>(kept) * count)
                );
            }
            ++kept;
            continue;
        }
        // The root's destroy cascade could not reach the inner entities in this flush (a scene links them to the
        // root only after it), so the copy is never placed. The root stays reserved: the plan entry of its destroy
        // hands it back like any reserved id a destroy wins on, with the commands recorded on it.
        for (usize index = 1; index < count; ++index) {
            release_reserved(entry.ids[first + index]);
        }
    }
    entry.ids.resize(static_cast<usize>(kept) * count);
    entry.copies = kept;
}

void world::release_reserved(entity target) {
    // A reserved id that never became alive: hand it back with a bumped generation so the handle the
    // caller still holds can never alias the next occupant of the index. An index the record table has
    // not reached yet (a discarded deferred instantiation) is added first: a table grown later would
    // start it at generation 1 while the free list holds the bumped one.
    if (ensure_records(target.index).has_value()) {
        entity_record &reserved = mRecords[target.index];
        const u32 next = target.generation + 1;
        reserved.generation = (next >= ARCHIVE_LOCAL_GENERATION || next == 0) ? ARCHIVE_LOCAL_GENERATION : next;
    }
    mAllocator->release(target);
}

void world::remove_row(archetype &owner, u32 chunkIndex, u32 row) {
    const chunk_layout &layout = owner.mLayout;
    chunk &target = *owner.mChunks[chunkIndex];
    const u32 lastChunkIndex = static_cast<u32>(owner.mChunks.size() - 1);
    chunk *last = owner.mChunks[lastChunkIndex].get();
    if (last->count() == 0 && lastChunkIndex > 0) {
        // A drained spare is trailing; the real last row lives in the chunk before it.
        last = owner.mChunks[lastChunkIndex - 1].get();
    }
    const u32 lastRow = last->count() - 1;

    if (last != &target || lastRow != row) {
        // Swap-remove: the archetype's last row fills the hole.
        for (u32 column = 0; column < layout.column_count(); ++column) {
            const component_info &info = mRegistry->info(layout.components[column]);
            relocate_cell(info, target.cell(layout, column, row), last->cell(layout, column, lastRow));
        }
        const entity moved = last->entities()[lastRow];
        target.entities()[row] = moved;
        entity_record &movedRecord = mRecords[moved.index];
        movedRecord.chunkIndex = chunkIndex;
        movedRecord.row = row;
        target.stamp_all(version());
    }
    last->set_count(last->count() - 1);
    --owner.mEntityCount;

    // Keep one empty trailing chunk as a spare; return the blocks of any further empties to the pool
    // and park their metadata on the archetype, so the next growth re-attaches it instead of
    // allocating a chunk object (spawn/destroy churn stays allocation-free).
    while (owner.mChunks.size() >= 2 && owner.mChunks.back()->count() == 0 &&
      owner.mChunks[owner.mChunks.size() - 2]->count() == 0) {
        std::unique_ptr<chunk> drained = std::move(owner.mChunks.back());
        owner.mChunks.pop_back();
        mChunkPool->deallocate(drained->release_block());
        owner.mSpareChunks.push_back(std::move(drained));
    }
}

core::result<void> world::move_entity(entity target, entity_record &record, archetype &destination) {
    archetype &source = *record.owner;
    if (&source == &destination) {
        return core::result<void>::success();
    }
    // Make the move infallible before any observer runs: a failed reservation leaves the world as it
    // was (01_ecs.md 6), and no half-written destination row is visible while observers iterate.
    CONJURE_TRY(destination.reserve_row(*mChunkPool));
    const component_mask removed = source.mask() & ~destination.mask();
    fire_removed(target, removed);

    // Observers may not change structure, so the record is still where it was and the reserved row is
    // still free.
    CONJURE_TRY_ASSIGN(const archetype::row_location location, destination.push_row(*mChunkPool));
    const chunk_layout &sourceLayout = source.mLayout;
    const chunk_layout &destinationLayout = destination.mLayout;
    chunk &sourceChunk = *source.mChunks[record.chunkIndex];
    chunk &destinationChunk = *destination.mChunks[location.chunkIndex];

    for (u32 column = 0; column < destinationLayout.column_count(); ++column) {
        const component_id id = destinationLayout.components[column];
        const component_info &info = mRegistry->info(id);
        void *cell = destinationChunk.cell(destinationLayout, column, location.row);
        const u16 sourceColumn = sourceLayout.column_of(id);
        if (sourceColumn != chunk_layout::NO_COLUMN) {
            relocate_cell(info, cell, sourceChunk.cell(sourceLayout, sourceColumn, record.row));
        } else {
            construct_column_default(info, cell, 1);
        }
    }
    for (u32 column = 0; column < sourceLayout.column_count(); ++column) {
        const component_id id = sourceLayout.components[column];
        if (destinationLayout.column_of(id) == chunk_layout::NO_COLUMN) {
            destruct_column(mRegistry->info(id), sourceChunk.cell(sourceLayout, column, record.row), 1);
        }
    }
    destinationChunk.entities()[location.row] = target;
    destinationChunk.stamp_all(version());

    remove_row(source, record.chunkIndex, record.row);
    record.owner = &destination;
    record.chunkIndex = location.chunkIndex;
    record.row = location.row;
    return core::result<void>::success();
}

void world::destroy_alive(entity target, entity_record &record, bool fireObservers) {
    archetype &owner = *record.owner;
    if (fireObservers) {
        fire_removed(target, owner.mask());
        fire_destroyed(target);
    }

    const chunk_layout &layout = owner.mLayout;
    chunk &block = *owner.mChunks[record.chunkIndex];
    for (u32 column = 0; column < layout.column_count(); ++column) {
        destruct_column(mRegistry->info(layout.components[column]), block.cell(layout, column, record.row), 1);
    }
    remove_row(owner, record.chunkIndex, record.row);

    record.owner = nullptr;
    record.state = entity_state::FREE;
    const u32 next = target.generation + 1;
    record.generation = (next >= ARCHIVE_LOCAL_GENERATION || next == 0) ? ARCHIVE_LOCAL_GENERATION : next;
    --mAliveCount;
    mAllocator->release(target);
}

core::result<void *> world::add_storage(entity target, component_id id) {
    CONJURE_TRY(check_structural());
    if (!mRegistry->is_registered(id)) {
        return make_error(ecs_error::UNKNOWN_COMPONENT, "add of an unregistered component");
    }
    entity_record *record = find_alive(target);
    if (record == nullptr) {
        return make_error(ecs_error::ENTITY_NOT_ALIVE, "add on a dead or stale entity");
    }
    if (record->owner->has(id)) {
        return make_error(ecs_error::COMPONENT_ALREADY_PRESENT, "add of a component the entity already holds");
    }
    CONJURE_TRY_ASSIGN(archetype *destination, archetype_with(*record->owner, id, true));

    bump_version();
    begin_structural();
    const core::result<void> moved = move_entity(target, *record, *destination);
    end_structural();
    if (!moved.has_value()) {
        return moved.get_error();
    }
    return get_raw(target, id, false);
}

void world::finish_add(entity target, component_id id) {
    component_mask added = {};
    added.set(id);
    begin_structural();
    fire_added(target, added);
    end_structural();
}

void world::fire_added(entity target, const component_mask &added) {
    if (!mObservers->has_component_observers() || added.none()) {
        return;
    }
    const bool previous = mInObserver;
    mInObserver = true;
    added.for_each_set([&](component_id id) {
        mObservers->fire_component(*this, target, id, component_event::ADDED, *mDeferred);
    });
    mInObserver = previous;
}

void world::fire_removed(entity target, const component_mask &removed) {
    if (!mObservers->has_component_observers() || removed.none()) {
        return;
    }
    const bool previous = mInObserver;
    mInObserver = true;
    removed.for_each_set([&](component_id id) {
        mObservers->fire_component(*this, target, id, component_event::REMOVED, *mDeferred);
    });
    mInObserver = previous;
}

void world::fire_destroyed(entity target) {
    if (!mObservers->has_entity_observers()) {
        return;
    }
    const bool previous = mInObserver;
    mInObserver = true;
    mObservers->fire_destroyed(*this, target, *mDeferred);
    mInObserver = previous;
}

void world::drain_deferred() {
    u32 passes = 0;
    while (!mDeferred->empty()) {
        ++passes;
        if (passes > MAX_DRAIN_PASSES) {
            CONJURE_ASSERT(false, "observers of world '{}' keep enqueuing work: a cycle in caller data", mName);
            mDeferred->clear();
            return;
        }
        std::swap(*mDeferred, *mDeferredFlushing);
        begin_structural();
        const flush_report report = flush_internal(*mDeferredFlushing);
        --mStructuralDepth;
        CONJURE_UNUSED(report);
    }
}

void world::construct_column_default(const component_info &info, void *destination, u32 count) noexcept {
    if (info.size == 0) {
        return;
    }
    // Zero first so padding bytes are deterministic (world_hash reads raw bytes).
    std::memset(destination, 0, static_cast<usize>(info.size) * count);
    if (info.ops.constructDefault != nullptr) {
        info.ops.constructDefault(destination, count);
        return;
    }
    if (!info.defaultValue.empty()) {
        std::byte *cursor = static_cast<std::byte *>(destination);
        for (u32 index = 0; index < count; ++index) {
            std::memcpy(cursor, info.defaultValue.data(), info.size);
            cursor += info.size;
        }
    }
}

void world::destruct_column(const component_info &info, void *object, u32 count) noexcept {
    if (info.ops.destruct != nullptr && info.size != 0) {
        info.ops.destruct(object, count);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Flush

flush_report world::flush_internal(command_buffer &commands) {
    using record_type = command_buffer::record;
    using opcode = command_buffer::opcode;

    flush_report report = {};
    bump_version();

    // Records for every reserved index the buffer mentions. Indices the allocator never issued belong
    // to forged handles: they are skipped below and must not grow the table.
    const u32 issued = mAllocator->peak_index();
    u32 highest = 0;
    bool anyEntity = false;
    bool anyPlanned = false;
    // The destroy targets, gathered only when the buffer instantiates: a destroy of a copy's root cancels it.
    std::vector<u64> &destroyed = mFlushDestroyed;
    destroyed.clear();
    const bool instantiates = !commands.mInstantiates.empty();
    for (const record_type *command = commands.mFirst; command != nullptr; command = command->next) {
        if (command->op == opcode::INSTANTIATE) {
            for (const entity id : commands.mInstantiates[command->instantiateIndex].ids) {
                highest = std::max(highest, id.index);
                anyEntity = true;
            }
            continue;
        }
        anyPlanned = true;
        if (command->target.index < issued) {
            highest = std::max(highest, command->target.index);
            anyEntity = true;
        }
        if (instantiates && command->op == opcode::DESTROY) {
            destroyed.push_back(command->target.to_u64());
        }
    }
    std::sort(destroyed.begin(), destroyed.end());
    if (anyEntity) {
        const core::result<void> records = ensure_records(highest);
        if (!records.has_value()) {
            CONJURE_LOG_WARN(LOG_ECS, "flush could not grow the record table: {}", records.get_error().message());
            // Nothing has run yet, instantiations included: every recorded command is skipped.
            report.skipped = commands.command_count();
            commands.clear();
            return report;
        }
    }

    // Archives first: their roots are real ids that later commands of the same buffer may address, so
    // those commands must find the entities alive instead of placing the reserved ids themselves.
    for (record_type *command = commands.mFirst; command != nullptr; command = command->next) {
        if (command->op != opcode::INSTANTIATE) {
            continue;
        }
        command_buffer::instantiate_entry &entry = commands.mInstantiates[command->instantiateIndex];
        if (!destroyed.empty()) {
            cancel_destroyed_copies(entry, destroyed);
        }
        const core::result<void> created = instantiate_now(*entry.archive, entry.ids, entry.copies);
        if (!created.has_value()) {
            CONJURE_LOG_WARN(LOG_ECS, "flush could not instantiate an archive: {}", created.get_error().message());
            ++report.skipped;
        } else {
            // The ids now belong to live entities; clear() must not hand them back.
            entry.ids.clear();
            ++report.applied;
        }
    }
    if (!anyPlanned) {
        // Only instantiations (the usual shape of an observer's deferred buffer): nothing to plan.
        commands.clear();
        return report;
    }

    // Plan: one entry per touched entity, found through the stamped slot table so no per-flush
    // allocation or zero-fill scales with the record count. The plan vector is the world's scratch,
    // used in place and kept at capacity; a flush never nests inside another (observers only enqueue
    // into the deferred buffer, which drains after the outermost structural call has returned, and
    // check_structural refuses a component constructor or destructor that calls back into the world).
    std::vector<flush_entry> &plan = mFlushPlan;
    plan.clear();
    ++mFlushStamp;
    if (mFlushStamp == 0) {
        // The stamp wrapped: forget every entry so none can match the restarted sequence.
        std::fill(mFlushSlots.begin(), mFlushSlots.end(), flush_slot{});
        mFlushStamp = 1;
    }
    if (mFlushSlots.size() < mRecords.size()) {
        mFlushSlots.resize(mRecords.size());
    }

    const auto consume = [](record_type *command) {
        command->payloadLive = false;
    };
    const auto discard_payload = [this, &consume](record_type *command) {
        if (!command->payloadLive) {
            return;
        }
        // A payload only ever carries a registered type; the check keeps a corrupt record from running
        // another type's destructor over its bytes.
        if (mRegistry->is_registered(command->id)) {
            destruct_column(mRegistry->info(command->id), command->payload, 1);
        }
        consume(command);
    };
    const auto skip_entry = [this, &report, &discard_payload](flush_entry &entry) {
        for (record_type *pending : entry.payloads) {
            discard_payload(pending);
        }
        report.applied -= entry.commands;
        report.skipped += entry.commands;
        if (!entry.alive) {
            // The reserved id was never placed: free it instead of leaking the index for good.
            release_reserved(entry.id);
        }
    };

    for (record_type *command = commands.mFirst; command != nullptr; command = command->next) {
        if (command->op == opcode::INSTANTIATE) {
            continue;
        }

        const entity target = command->target;
        u32 slot = NO_PLAN;
        if (target.index < mFlushSlots.size() && mFlushSlots[target.index].stamp == mFlushStamp) {
            slot = mFlushSlots[target.index].plan;
        }
        if (slot != NO_PLAN && !(plan[slot].id == target)) {
            // A stale and a live generation of one index in the same buffer: the later one loses.
            ++report.skipped;
            discard_payload(command);
            continue;
        }
        if (slot == NO_PLAN) {
            flush_entry fresh = {};
            fresh.id = target;
            fresh.alive = find_alive(target) != nullptr;
            if (!fresh.alive && !is_reserved(target)) {
                CONJURE_LOG_DEBUG(LOG_ECS, "flush skipped a command on dead entity {}", target);
                ++report.skipped;
                discard_payload(command);
                continue;
            }
            slot = static_cast<u32>(plan.size());
            plan.push_back(std::move(fresh));
            mFlushSlots[target.index] = flush_slot{mFlushStamp, slot};
        }

        flush_entry &entry = plan[slot];
        switch (command->op) {
            case opcode::CREATE:
                break;
            case opcode::DESTROY:
                entry.destroy = true;
                break;
            case opcode::ADD:
                if (!mRegistry->is_registered(command->id)) {
                    ++report.skipped;
                    discard_payload(command);
                    continue;
                }
                entry.addMask.set(command->id);
                entry.removeMask.clear(command->id);
                if (command->payloadLive) {
                    entry.payloads.push_back(command);
                }
                break;
            case opcode::SET:
                if (!mRegistry->is_registered(command->id)) {
                    ++report.skipped;
                    discard_payload(command);
                    continue;
                }
                entry.payloads.push_back(command);
                break;
            case opcode::REMOVE:
                if (!mRegistry->is_registered(command->id)) {
                    ++report.skipped;
                    continue;
                }
                entry.removeMask.set(command->id);
                entry.addMask.clear(command->id);
                for (record_type *pending : entry.payloads) {
                    if (pending->id == command->id) {
                        discard_payload(pending);
                    }
                }
                entry.payloads.erase(
                  std::remove_if(
                    entry.payloads.begin(),
                    entry.payloads.end(),
                    [command](const record_type *pending) {
                        return pending->id == command->id;
                    }
                  ),
                  entry.payloads.end()
                );
                break;
            case opcode::INSTANTIATE:
                break;
        }
        ++report.applied;
        ++entry.commands;
    }

    // Apply: structure first, destroys last.
    for (flush_entry &entry : plan) {
        if (entry.destroy) {
            continue;
        }
        entity_record *record = entry.alive ? find_alive(entry.id) : nullptr;
        const component_mask currentMask = record != nullptr ? record->owner->mask() : component_mask{};
        const component_mask finalMask = (currentMask | entry.addMask) & ~entry.removeMask;
        const component_mask added = finalMask & ~currentMask;

        if (record == nullptr) {
            // Reserved id: place it straight into its final archetype.
            core::result<archetype *> target = archetype_for(finalMask);
            if (!target.has_value()) {
                CONJURE_LOG_WARN(
                  LOG_ECS,
                  "flush could not create entity {}: {}",
                  entry.id,
                  target.get_error().message()
                );
                skip_entry(entry);
                continue;
            }
            const core::result<void> placed = place_rows(*target.value(), std::span<const entity>(&entry.id, 1));
            if (!placed.has_value()) {
                CONJURE_LOG_WARN(
                  LOG_ECS,
                  "flush could not place entity {}: {}",
                  entry.id,
                  placed.get_error().message()
                );
                skip_entry(entry);
                continue;
            }
        } else if (finalMask != currentMask) {
            core::result<archetype *> target = archetype_for(finalMask);
            if (!target.has_value()) {
                CONJURE_LOG_WARN(LOG_ECS, "flush could not move entity {}: {}", entry.id, target.get_error().message());
                skip_entry(entry);
                continue;
            }
            const core::result<void> moved = move_entity(entry.id, *record, *target.value());
            if (!moved.has_value()) {
                CONJURE_LOG_WARN(LOG_ECS, "flush could not move entity {}: {}", entry.id, moved.get_error().message());
                skip_entry(entry);
                continue;
            }
        }

        // Payloads in record order: an add's value lands in the fresh column, a set replaces.
        for (record_type *pending : entry.payloads) {
            if (!pending->payloadLive) {
                continue;
            }
            void *cell = get_raw(entry.id, pending->id, false);
            if (cell == nullptr) {
                // SET on a component the entity does not hold at flush time.
                ++report.skipped;
                --report.applied;
                discard_payload(pending);
                continue;
            }
            const component_info &info = mRegistry->info(pending->id);
            if (info.ops.relocate != nullptr) {
                destruct_column(info, cell, 1);
                info.ops.relocate(cell, pending->payload, 1);
            } else {
                std::memcpy(cell, pending->payload, info.size);
            }
            consume(pending);
            entity_record *updated = find_alive(entry.id);
            if (updated != nullptr) {
                const u16 column = updated->owner->mLayout.column_of(pending->id);
                if (column != chunk_layout::NO_COLUMN) {
                    updated->owner->mChunks[updated->chunkIndex]->stamp(column, version());
                }
            }
        }
        fire_added(entry.id, added);
    }

    for (flush_entry &entry : plan) {
        if (!entry.destroy) {
            continue;
        }
        for (record_type *pending : entry.payloads) {
            discard_payload(pending);
        }
        entity_record *record = find_alive(entry.id);
        if (record != nullptr) {
            destroy_alive(entry.id, *record, true);
            continue;
        }
        // Reserved but never placed: hand the id back with a bumped generation.
        release_reserved(entry.id);
    }

    // The entries point into the buffer's arena, which clear() recycles: drop them first.
    plan.clear();
    commands.clear();
    return report;
}
