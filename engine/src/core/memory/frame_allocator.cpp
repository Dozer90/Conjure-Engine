#include "conjure/core/memory/frame_allocator.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <cstdint>
#include <limits>

using namespace conjure::core::memory;

/*===========================================================================*/
/*===========================================================================*/
// Overflow blocks and thread slots

// One heap fallback allocation: this header, padded to the payload alignment, then the payload.
// Blocks are chained into the allocator's lock-free stack and released by reset().
struct frame_allocator::overflow_block {
    overflow_block *next = nullptr;
    usize totalBytes = 0;   // header padding plus payload, as passed to tracked_allocate
};

namespace {
    // Thread blocks start on a cache line so two threads never write the same line.
    constexpr conjure::usize BLOCK_ALIGNMENT = conjure::CACHE_LINE_SIZE;

    // The block a thread is currently bumping from. Owner and epoch identify which allocator and
    // which frame it belongs to; a mismatch means "grab a new block". Once a claim has failed the
    // arena stays exhausted until the next reset(), so the thread stops touching the shared offset.
    struct thread_slot {
        const frame_allocator *owner = nullptr;
        conjure::u64 epoch = 0;
        conjure::byte *cursor = nullptr;
        conjure::byte *end = nullptr;
        bool arenaExhausted = false;
    };

    thread_slot &current_slot() noexcept {
        thread_local thread_slot slot = {};
        return slot;
    }

    // Epochs come from one process-wide counter so no two frame allocators, live or destroyed, ever
    // share one: a slot left behind by a dead allocator can never match a new one at the same address.
    std::atomic<conjure::u64> sNextEpoch = 0;

    conjure::u64 next_epoch() noexcept {
        return sNextEpoch.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    void *bump(thread_slot &slot, conjure::usize bytes, conjure::usize alignment) noexcept {
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(slot.cursor);
        const conjure::usize padding = align_up(address, alignment) - address;
        const conjure::usize available = static_cast<conjure::usize>(slot.end - slot.cursor);
        if (padding > available || bytes > available - padding) {
            return nullptr;
        }

        conjure::byte *result = slot.cursor + padding;
        slot.cursor = result + bytes;
        return result;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

frame_allocator::frame_allocator(usize arenaBytes, usize threadBlockBytes) {
    // Clamp before rounding so the rounding cannot wrap to zero (and divide by zero below).
    constexpr usize MAX_BLOCK_BYTES = std::numeric_limits<usize>::max() - (BLOCK_ALIGNMENT - 1);
    mThreadBlockBytes = align_up(std::clamp(threadBlockBytes, BLOCK_ALIGNMENT, MAX_BLOCK_BYTES), BLOCK_ALIGNMENT);

    if (arenaBytes > 0) {
        const usize fullBlocks = arenaBytes / mThreadBlockBytes;
        const usize blockCount = fullBlocks + (arenaBytes % mThreadBlockBytes != 0 ? 1 : 0);
        if (blockCount <= std::numeric_limits<usize>::max() / mThreadBlockBytes) {
            mCapacityBytes = blockCount * mThreadBlockBytes;
            mArena = static_cast<byte *>(tracked_allocate(mCapacityBytes, BLOCK_ALIGNMENT));
        }
        if (mArena == nullptr) {
            // The arena is impossible (its size would wrap) or the heap refused it: stay usable by
            // serving everything from the heap.
            mCapacityBytes = 0;
        }
    }

    // acquire_block() relies on this: a non-null arena holds at least one whole block.
    CONJURE_ASSERT(mArena == nullptr || mCapacityBytes >= mThreadBlockBytes, "frame arena smaller than a block");
    mEpoch.store(next_epoch(), std::memory_order_release);
}

frame_allocator::~frame_allocator() {
    free_overflow_blocks();
    tracked_free(mArena, mCapacityBytes);
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void *frame_allocator::allocate(usize bytes, usize alignment) noexcept {
    const bool validAlignment = is_power_of_two(alignment);
    CONJURE_ASSERT(validAlignment, "alignment {} is not a power of two", alignment);
    if (!validAlignment || bytes == 0) {
        return nullptr;
    }

    const u64 epoch = mEpoch.load(std::memory_order_acquire);
    thread_slot &slot = current_slot();
    if (slot.owner != this || slot.epoch != epoch) {
        // Another allocator's block or a previous frame's block: drop it, the first bump below fails
        // and a fresh block is claimed.
        slot = {};
        slot.owner = this;
        slot.epoch = epoch;
    }

    void *fromBlock = bump(slot, bytes, alignment);
    if (fromBlock != nullptr) {
        return fromBlock;
    }

    if (fits_in_fresh_block(bytes, alignment) && !slot.arenaExhausted) {
        byte *block = acquire_block();
        if (block != nullptr) {
            slot.cursor = block;
            slot.end = block + mThreadBlockBytes;
            return bump(slot, bytes, alignment);
        }
        slot.arenaExhausted = true;
    }

    return allocate_overflow(bytes, alignment);
}

void frame_allocator::reset() noexcept {
    const usize frameBytes = used_bytes() + mOverflowBytes.load(std::memory_order_relaxed);
    const usize highWater = std::max(mHighWaterBytes.load(std::memory_order_relaxed), frameBytes);
    mHighWaterBytes.store(highWater, std::memory_order_relaxed);

    free_overflow_blocks();
    mOverflowCount.store(0, std::memory_order_relaxed);
    mOverflowBytes.store(0, std::memory_order_relaxed);
    mOffset.store(0, std::memory_order_relaxed);

    // Published last: a thread that sees the new epoch must also see the recycled arena.
    mEpoch.store(next_epoch(), std::memory_order_release);
}

conjure::usize frame_allocator::high_water_bytes() const noexcept {
    const usize currentFrame = used_bytes() + mOverflowBytes.load(std::memory_order_relaxed);
    return std::max(mHighWaterBytes.load(std::memory_order_relaxed), currentFrame);
}

conjure::usize frame_allocator::used_bytes() const noexcept {
    return mOffset.load(std::memory_order_relaxed);
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

bool frame_allocator::fits_in_fresh_block(usize bytes, usize alignment) const noexcept {
    // A fresh block starts BLOCK_ALIGNMENT-aligned, so only larger alignments can need padding.
    const usize worstPadding = alignment > BLOCK_ALIGNMENT ? alignment - BLOCK_ALIGNMENT : 0;
    if (worstPadding > mThreadBlockBytes) {
        return false;
    }
    return bytes <= mThreadBlockBytes - worstPadding;
}

conjure::byte *frame_allocator::acquire_block() noexcept {
    if (mArena == nullptr) {
        return nullptr;
    }

    // Claim with a compare-exchange that never moves the offset past the capacity, so a failed claim
    // leaves the accounting exact and the offset can never wrap back into the arena. The offset is
    // always at or below the capacity, which keeps the subtraction safe.
    usize offset = mOffset.load(std::memory_order_relaxed);
    do {
        if (mCapacityBytes - offset < mThreadBlockBytes) {
            return nullptr;
        }
    } while (!mOffset.compare_exchange_weak(
      offset,
      offset + mThreadBlockBytes,
      std::memory_order_relaxed,
      std::memory_order_relaxed
    ));
    return mArena + offset;
}

void *frame_allocator::allocate_overflow(usize bytes, usize alignment) noexcept {
    const usize actualAlignment = std::max(alignment, DEFAULT_ALIGNMENT);
    const usize headerBytes = align_up(sizeof(overflow_block), actualAlignment);
    if (bytes > std::numeric_limits<usize>::max() - headerBytes) {
        return nullptr;
    }

    const usize totalBytes = headerBytes + bytes;
    void *memory = tracked_allocate(totalBytes, actualAlignment);
    if (memory == nullptr) {
        return nullptr;
    }

    overflow_block *node = ::new (memory) overflow_block{};
    node->totalBytes = totalBytes;

    overflow_block *head = mOverflowHead.load(std::memory_order_relaxed);
    do {
        node->next = head;
    } while (!mOverflowHead.compare_exchange_weak(head, node, std::memory_order_release, std::memory_order_relaxed));

    mOverflowCount.fetch_add(1, std::memory_order_relaxed);
    mOverflowBytes.fetch_add(bytes, std::memory_order_relaxed);
    return static_cast<byte *>(memory) + headerBytes;
}

void frame_allocator::free_overflow_blocks() noexcept {
    overflow_block *node = mOverflowHead.exchange(nullptr, std::memory_order_acquire);
    while (node != nullptr) {
        overflow_block *next = node->next;
        tracked_free(node, node->totalBytes);
        node = next;
    }
}
