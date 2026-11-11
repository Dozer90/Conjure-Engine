#include "conjure/ecs/world.h"

#include "ecs/chunk.h"
#include "ecs/chunk_pool.h"
#include "ecs/entity_allocator.h"
#include "ecs/observer_table.h"

#include "conjure/ecs/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/core/math/math_utils.h"

#include <algorithm>
#include <format>
#include <thread>
#include <utility>

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// iteration_scope

iteration_scope::iteration_scope(world &target) noexcept :
    mWorld(&target) {
    target.mLockDepth.fetch_add(1, std::memory_order_acq_rel);
}

iteration_scope::~iteration_scope() {
    release();
}

iteration_scope::iteration_scope(iteration_scope &&other) noexcept :
    mWorld(other.mWorld) {
    other.mWorld = nullptr;
}

iteration_scope &iteration_scope::operator=(iteration_scope &&other) noexcept {
    if (this != &other) {
        release();
        mWorld = other.mWorld;
        other.mWorld = nullptr;
    }
    return *this;
}

void iteration_scope::release() noexcept {
    if (mWorld == nullptr) {
        return;
    }
    mWorld->mLockDepth.fetch_sub(1, std::memory_order_acq_rel);
    mWorld = nullptr;
}


/*===========================================================================*/
/*===========================================================================*/
// world: constructors and destructors

world::world(const component_registry &registry, world_desc desc) :
    mRegistry(&registry),
    mName(std::move(desc.name)),
    mOwnerThread(std::this_thread::get_id()),
    mAllocator(std::make_unique<entity_allocator>()),
    mChunkPool(std::make_unique<chunk_pool>(desc.chunksPerSlab, desc.maxChunkSlabs)),
    mObservers(std::make_unique<observer_table>()) {
    mRecords.reserve(desc.initialEntityCapacity);
    mDeferred = std::make_unique<command_buffer>(*this);
    mDeferredFlushing = std::make_unique<command_buffer>(*this);

    // The empty archetype always exists so create_entity() without a mask never allocates a layout.
    const core::result<archetype *> empty = archetype_for(component_mask{});
    CONJURE_ASSERT(empty.has_value(), "the empty archetype must always fit a chunk");
    mEmptyArchetype = empty.has_value() ? empty.value() : nullptr;

    registry.attach_world(this);
}

world::~world() {
    // Destroy without observers: nothing may enqueue follow-up work into a dying world.
    for (u32 index = 0; index < mRecords.size(); ++index) {
        entity_record &record = mRecords[index];
        if (record.state == entity_state::ALIVE) {
            destroy_alive(entity{index, record.generation}, record, false);
        }
    }
    mDeferred->clear();
    mDeferredFlushing->clear();
    for (std::unique_ptr<archetype> &owner : mArchetypes) {
        for (std::unique_ptr<chunk> &block : owner->mChunks) {
            mChunkPool->deallocate(block->block());
        }
        owner->mChunks.clear();
    }
    mRegistry->detach_world(this);
}


/*===========================================================================*/
/*===========================================================================*/
// world: entities

bool world::is_alive(entity target) const noexcept {
    return find_alive(target) != nullptr;
}

component_mask world::mask_of(entity target) const noexcept {
    const entity_record *record = find_alive(target);
    if (record == nullptr) {
        return component_mask{};
    }
    return record->owner->mask();
}


/*===========================================================================*/
/*===========================================================================*/
// world: untyped components

void *world::get_raw(entity target, component_id id, bool forWrite) noexcept {
    entity_record *record = find_alive(target);
    if (record == nullptr) {
        return nullptr;
    }
    archetype &owner = *record->owner;
    const u16 column = owner.mLayout.column_of(id);
    if (column == chunk_layout::NO_COLUMN) {
        return nullptr;
    }
    chunk &block = *owner.mChunks[record->chunkIndex];
    if (forWrite) {
        block.stamp(column, version());
    }
    return block.cell(owner.mLayout, column, record->row);
}

const void *world::get_raw(entity target, component_id id) const noexcept {
    const entity_record *record = find_alive(target);
    if (record == nullptr) {
        return nullptr;
    }
    const archetype &owner = *record->owner;
    const u16 column = owner.mLayout.column_of(id);
    if (column == chunk_layout::NO_COLUMN) {
        return nullptr;
    }
    return owner.mChunks[record->chunkIndex]->cell(owner.mLayout, column, record->row);
}

bool world::has_raw(entity target, component_id id) const noexcept {
    const entity_record *record = find_alive(target);
    return record != nullptr && record->owner->has(id);
}

u32 world::instance_count(component_id id) const noexcept {
    u32 total = 0;
    for (const archetype *owner : mArchetypePointers) {
        if (owner->has(id)) {
            total += owner->entity_count();
        }
    }
    return total;
}


/*===========================================================================*/
/*===========================================================================*/
// world: queries, locking

query world::create_query(const query_desc &desc) {
    return query(*this, desc);
}

iteration_scope world::lock_iteration() noexcept {
    return iteration_scope(*this);
}


/*===========================================================================*/
/*===========================================================================*/
// world: observers

observer_id world::observe(component_id id, component_event event, component_observer_fn fn) {
    if (std::this_thread::get_id() != mOwnerThread) {
        CONJURE_ASSERT(false, "world::observe off the owner thread");
        return observer_id{};
    }
    if (!mRegistry->is_registered(id)) {
        CONJURE_ASSERT(false, "world::observe with an unregistered component id");
        return observer_id{};
    }
    return mObservers->add_component_observer(id, event, std::move(fn));
}

observer_id world::observe_destroyed(entity_observer_fn fn) {
    if (std::this_thread::get_id() != mOwnerThread) {
        CONJURE_ASSERT(false, "world::observe_destroyed off the owner thread");
        return observer_id{};
    }
    return mObservers->add_entity_observer(std::move(fn));
}

void world::unobserve(observer_id id) {
    if (std::this_thread::get_id() != mOwnerThread) {
        CONJURE_ASSERT(false, "world::unobserve off the owner thread");
        return;
    }
    mObservers->remove(id);
}


/*===========================================================================*/
/*===========================================================================*/
// world: misc

core::result<void> world::set_owner_thread(std::thread::id thread) {
    if (is_locked()) {
        CONJURE_ASSERT(false, "world::set_owner_thread while the world is locked");
        return make_error(ecs_error::STRUCTURE_LOCKED, "set_owner_thread while locked");
    }
    mOwnerThread = thread;
    return core::result<void>::success();
}

world_stats world::stats() const noexcept {
    world_stats result = {};
    result.aliveEntities = mAliveCount;
    result.archetypes = static_cast<u32>(mArchetypes.size());
    for (const archetype *owner : mArchetypePointers) {
        result.chunks += owner->chunk_count();
    }
    result.chunkBytes = static_cast<u64>(result.chunks) * CHUNK_SIZE;
    result.version = version();
    return result;
}

core::result<void> world::validate() const {
    const auto fail = [](std::string message) -> core::result<void> {
        return core::error(core::error_code::INVALID_STATE, std::move(message));
    };

    // Archetype masks map back to themselves and the index list matches.
    for (u32 index = 0; index < mArchetypes.size(); ++index) {
        const archetype &owner = *mArchetypes[index];
        if (mArchetypePointers[index] != &owner || owner.index() != index) {
            return fail(std::format("archetype {} is not at its index", index));
        }
        const auto found = mArchetypeByMask.find(owner.mask());
        if (found == mArchetypeByMask.end() || found->second != &owner) {
            return fail(std::format("archetype {} mask does not map back to it", index));
        }

        // Column offsets aligned and inside the block.
        const chunk_layout &layout = owner.layout();
        for (u32 column = 0; column < layout.column_count(); ++column) {
            const component_info &info = mRegistry->info(layout.components[column]);
            const u32 alignment = std::max(info.alignment, COLUMN_ALIGNMENT);
            if (!math::is_aligned(layout.columnOffsets[column], alignment)) {
                return fail(std::format("archetype {} column {} is misaligned", index, column));
            }
            const u64 end = static_cast<u64>(layout.columnOffsets[column]) +
              static_cast<u64>(layout.columnSizes[column]) * layout.capacity;
            if (end > CHUNK_SIZE) {
                return fail(std::format("archetype {} column {} overruns the chunk", index, column));
            }
        }

        // Chunk counts sum up; every chunk before the last non-empty one is full and at most one
        // drained spare trails it; rows agree with records.
        u32 total = 0;
        usize lastNonEmpty = 0;
        bool anyNonEmpty = false;
        for (usize chunkIndex = 0; chunkIndex < owner.mChunks.size(); ++chunkIndex) {
            if (owner.mChunks[chunkIndex]->count() > 0) {
                lastNonEmpty = chunkIndex;
                anyNonEmpty = true;
            }
        }
        const usize spares = owner.mChunks.size() - (anyNonEmpty ? lastNonEmpty + 1 : 0);
        if (spares > 1) {
            return fail(std::format("archetype {} keeps {} spare chunks", index, spares));
        }
        for (u32 chunkIndex = 0; chunkIndex < owner.mChunks.size(); ++chunkIndex) {
            const chunk &block = *owner.mChunks[chunkIndex];
            if (block.count() > layout.capacity) {
                return fail(std::format("archetype {} chunk {} exceeds its capacity", index, chunkIndex));
            }
            if (anyNonEmpty && chunkIndex < lastNonEmpty && block.count() != layout.capacity) {
                return fail(std::format("archetype {} chunk {} is partial but not last", index, chunkIndex));
            }
            total += block.count();
            for (u32 row = 0; row < block.count(); ++row) {
                const entity stored = block.entities()[row];
                if (stored.index >= mRecords.size()) {
                    return fail(std::format("row {} of archetype {} names an unknown entity", row, index));
                }
                const entity_record &record = mRecords[stored.index];
                if (record.state != entity_state::ALIVE || record.generation != stored.generation ||
                  record.owner != &owner || record.chunkIndex != chunkIndex || record.row != row) {
                    return fail(std::format("record of entity {} disagrees with its row", stored));
                }
            }
        }
        if (total != owner.entity_count()) {
            return fail(
              std::format("archetype {} entity count {} differs from its rows {}", index, owner.entity_count(), total)
            );
        }
    }

    // Records point at real rows; alive count matches.
    u32 alive = 0;
    for (u32 index = 0; index < mRecords.size(); ++index) {
        const entity_record &record = mRecords[index];
        if (record.state != entity_state::ALIVE) {
            if (record.owner != nullptr) {
                return fail(std::format("free record {} still points at an archetype", index));
            }
            continue;
        }
        ++alive;
        if (record.owner == nullptr || record.chunkIndex >= record.owner->mChunks.size()) {
            return fail(std::format("record {} points outside its archetype", index));
        }
        const chunk &block = *record.owner->mChunks[record.chunkIndex];
        if (record.row >= block.count() || !(block.entities()[record.row] == entity{index, record.generation})) {
            return fail(std::format("record {} does not match the row it names", index));
        }
    }
    if (alive != mAliveCount) {
        return fail(std::format("alive count {} differs from the records ({})", mAliveCount, alive));
    }

    // Free-list indices are FREE and carry the record's next generation.
    for (const entity slot : mAllocator->free_slots()) {
        if (slot.index < mRecords.size()) {
            const entity_record &record = mRecords[slot.index];
            if (record.state != entity_state::FREE || record.generation != slot.generation) {
                return fail(std::format("free slot {} does not match its record", slot.index));
            }
        }
    }

    // Every chunk block is accounted for in the pool.
    usize chunks = 0;
    for (const archetype *owner : mArchetypePointers) {
        chunks += owner->chunk_count();
    }
    if (chunks != mChunkPool->live_count()) {
        return fail(std::format("{} chunks held but {} blocks live in the pool", chunks, mChunkPool->live_count()));
    }
    return core::result<void>::success();
}

void world::clear() {
    const core::result<void> allowed = check_structural();
    if (!allowed.has_value()) {
        return;
    }
    // Observers may enqueue creations into the deferred buffer; keep destroying until nothing is left.
    begin_structural();
    bump_version();
    for (u32 pass = 0; pass < 1024 && mAliveCount > 0; ++pass) {
        for (u32 index = 0; index < mRecords.size(); ++index) {
            entity_record &record = mRecords[index];
            if (record.state == entity_state::ALIVE) {
                destroy_alive(entity{index, record.generation}, record, true);
            }
        }
        --mStructuralDepth;
        drain_deferred();
        ++mStructuralDepth;
    }
    CONJURE_ASSERT(mAliveCount == 0, "world::clear: observers keep creating entities");
    end_structural();

    // Drop every chunk, including spares and the parked chunk metadata. The id space stays: every
    // destroyed index is on the free list with a bumped generation, so ids still reserved by command
    // buffers (scheduler systems, scripts) cannot alias the entities created next.
    for (std::unique_ptr<archetype> &owner : mArchetypes) {
        for (std::unique_ptr<chunk> &block : owner->mChunks) {
            mChunkPool->deallocate(block->block());
        }
        owner->mChunks.clear();
        owner->mSpareChunks.clear();
        owner->mSpareChunks.shrink_to_fit();
        owner->mEntityCount = 0;
    }
    mChunkPool->shrink_to_fit();

    // The structural scratch is sized by the largest flush and placement the world has seen; clear() is
    // the one point where that peak is given back (the deferred buffers are empty: the drain loop above
    // ran until nothing was left).
    mFlushPlan.clear();
    mFlushPlan.shrink_to_fit();
    mPlacedScratch.clear();
    mPlacedScratch.shrink_to_fit();
    mDeferred->shrink_to_fit();
    mDeferredFlushing->shrink_to_fit();
    bump_version();
}


/*===========================================================================*/
/*===========================================================================*/
// world: guards and lookups

core::result<void> world::check_structural() const {
    if (std::this_thread::get_id() != mOwnerThread) {
        CONJURE_ASSERT(false, "structural change to world '{}' from a thread that does not own it", mName);
        return make_error(ecs_error::WRONG_THREAD, "structural change off the owner thread");
    }
    if (is_locked()) {
        CONJURE_ASSERT(false, "structural change to world '{}' while it is locked for iteration", mName);
        return make_error(ecs_error::STRUCTURE_LOCKED, "structural change while the world is locked");
    }
    if (mInObserver) {
        CONJURE_ASSERT(false, "observer of world '{}' called a structural method; use the deferred buffer", mName);
        return make_error(ecs_error::STRUCTURE_LOCKED, "structural change from inside an observer");
    }
    if (mStructuralDepth > 0) {
        // The only code of a structural call that is not the world's own is a component's constructor or
        // destructor (ops.constructDefault, ops.destruct, ops.relocate); one that calls back into its world
        // would re-enter place_rows or flush_internal over the scratch they use in place.
        CONJURE_ASSERT(
          false,
          "structural change to world '{}' from inside a structural call (a component constructor or destructor)",
          mName
        );
        return make_error(ecs_error::STRUCTURE_LOCKED, "structural change from inside a structural call");
    }
    return core::result<void>::success();
}

world::entity_record *world::find_alive(entity target) noexcept {
    if (target.index >= mRecords.size()) {
        return nullptr;
    }
    entity_record &record = mRecords[target.index];
    if (record.state != entity_state::ALIVE || record.generation != target.generation) {
        return nullptr;
    }
    return &record;
}

const world::entity_record *world::find_alive(entity target) const noexcept {
    if (target.index >= mRecords.size()) {
        return nullptr;
    }
    const entity_record &record = mRecords[target.index];
    if (record.state != entity_state::ALIVE || record.generation != target.generation) {
        return nullptr;
    }
    return &record;
}

bool world::is_reserved(entity target) const noexcept {
    // A FREE record carries the generation the next occupant receives; a handle holding exactly that
    // generation can only have come from a reservation. Indices past the table are always fresh ids.
    if (target.generation == 0 || target.generation == ARCHIVE_LOCAL_GENERATION) {
        return false;
    }
    if (target.index >= mRecords.size()) {
        return target.index < mAllocator->peak_index();
    }
    const entity_record &record = mRecords[target.index];
    return record.state == entity_state::FREE && record.generation == target.generation;
}

core::result<void> world::ensure_records(u32 highestIndex) {
    if (highestIndex < mRecords.size()) {
        return core::result<void>::success();
    }
    if (highestIndex >= MAX_ENTITY_INDEX) {
        return make_error(ecs_error::ENTITY_LIMIT_REACHED, "entity index space exhausted");
    }
    const usize oldSize = mRecords.size();
    const usize newSize = static_cast<usize>(highestIndex) + 1;
    mRecords.resize(newSize);
    for (usize index = oldSize; index < newSize; ++index) {
        // Fresh indices start at generation 1, which is what the allocator issues for them.
        mRecords[index].generation = 1;
    }
    return core::result<void>::success();
}

core::result<archetype *> world::archetype_for(const component_mask &mask) {
    const auto found = mArchetypeByMask.find(mask);
    if (found != mArchetypeByMask.end()) {
        return found->second;
    }
    CONJURE_TRY_ASSIGN(chunk_layout layout, chunk_layout::compute(mask, *mRegistry));
    const u32 index = static_cast<u32>(mArchetypes.size());
    mArchetypes.push_back(std::make_unique<archetype>(mask, std::move(layout), index));
    archetype *created = mArchetypes.back().get();
    mArchetypePointers.push_back(created);
    mArchetypeByMask.emplace(mask, created);
    return created;
}

core::result<archetype *> world::archetype_with(archetype &source, component_id id, bool added) {
    archetype::archetype_edge &edge = source.edge(id);
    archetype *cached = added ? edge.withAdded : edge.withRemoved;
    if (cached != nullptr) {
        return cached;
    }
    component_mask mask = source.mask();
    if (added) {
        mask.set(id);
    } else {
        mask.clear(id);
    }
    CONJURE_TRY_ASSIGN(archetype *resolved, archetype_for(mask));
    // archetype_for may have grown the edge vector of 'source'; look the edge up again.
    archetype::archetype_edge &refreshed = source.edge(id);
    if (added) {
        refreshed.withAdded = resolved;
    } else {
        refreshed.withRemoved = resolved;
    }
    return resolved;
}

void world::relayout_archetypes(component_id id) {
    for (std::unique_ptr<archetype> &owner : mArchetypes) {
        if (!owner->has(id)) {
            continue;
        }
        CONJURE_ASSERT(owner->entity_count() == 0, "relayout of an archetype that still holds entities");
        if (owner->entity_count() != 0) {
            continue;
        }
        for (std::unique_ptr<chunk> &block : owner->mChunks) {
            mChunkPool->deallocate(block->block());
        }
        owner->mChunks.clear();
        // Parked spares carry the old column count; the new layout may have another.
        owner->mSpareChunks.clear();
        core::result<chunk_layout> layout = chunk_layout::compute(owner->mask(), *mRegistry);
        if (layout.has_value()) {
            owner->mLayout = std::move(layout).value();
        } else {
            CONJURE_LOG_WARN(LOG_ECS, "archetype no longer fits a chunk after redefining component {}", id.value);
            owner->mLayout.capacity = 0;
        }
    }
}

void world::begin_structural() noexcept {
    ++mStructuralDepth;
}

void world::end_structural() {
    --mStructuralDepth;
    if (mStructuralDepth == 0) {
        drain_deferred();
    }
}
