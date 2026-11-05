#include "conjure/core/memory/pool_allocator.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

using namespace conjure::core::memory;

namespace {
    // Returned by find_slab_index when no slab contains the pointer.
    constexpr conjure::usize NO_SLAB = std::numeric_limits<conjure::usize>::max();

    // Slab table entries allocated on the first grow(); the table doubles from there.
    constexpr conjure::usize INITIAL_SLAB_TABLE_CAPACITY = 8;

    constexpr conjure::u32 BITS_PER_WORD = 64;

    std::uintptr_t address_of(const void *pointer) noexcept {
        return reinterpret_cast<std::uintptr_t>(pointer);
    }

    // Words of the per-slab free bitmap (one bit per block, set while the block is free).
    constexpr conjure::usize bitmap_words(conjure::u32 blocksPerSlab) noexcept {
        return (static_cast<conjure::usize>(blocksPerSlab) + BITS_PER_WORD - 1) / BITS_PER_WORD;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

pool_allocator::pool_allocator(usize blockSize, usize alignment, u32 blocksPerSlab, u32 maxSlabs) :
    mAlignment(alignment),
    mBlocksPerSlab(blocksPerSlab),
    mMaxSlabs(maxSlabs) {
    const bool validAlignment = is_power_of_two(alignment);
    CONJURE_ASSERT(validAlignment, "pool alignment {} is not a power of two", alignment);
    if (!validAlignment) {
        mAlignment = DEFAULT_ALIGNMENT;
    }

    CONJURE_ASSERT(blocksPerSlab > 0, "a pool slab must hold at least one block");
    if (blocksPerSlab == 0) {
        mBlocksPerSlab = 1;
    }

    // A free block stores its link in place, so a block is at least one pointer and the stride is a
    // multiple of both the requested alignment and the link's alignment: every block in a slab is
    // then correctly aligned for the caller and for the free list, and the bitmap that follows the
    // blocks starts on a word boundary.
    const usize strideAlignment = std::max(mAlignment, alignof(free_block));
    mBlockBytes = align_up(std::max(blockSize, sizeof(free_block)), strideAlignment);

    // A slab allocation is the blocks followed by the free bitmap. When the size would wrap, the pool
    // simply never grows (allocate() returns nullptr) instead of asking the heap for a wrapped size.
    const usize bitmapBytes = bitmap_words(mBlocksPerSlab) * sizeof(u64);
    const bool blocksFit = mBlockBytes <= std::numeric_limits<usize>::max() / mBlocksPerSlab;
    const usize blocksBytes = blocksFit ? mBlockBytes * mBlocksPerSlab : 0;
    if (blocksFit && blocksBytes <= std::numeric_limits<usize>::max() - bitmapBytes) {
        mSlabBytes = blocksBytes + bitmapBytes;
    }
}

pool_allocator::~pool_allocator() {
    release_slabs();
}

pool_allocator::pool_allocator(pool_allocator &&other) noexcept :
    mBlockBytes(other.mBlockBytes),
    mSlabBytes(other.mSlabBytes),
    mAlignment(other.mAlignment),
    mBlocksPerSlab(other.mBlocksPerSlab),
    mMaxSlabs(other.mMaxSlabs),
    mSlabs(std::exchange(other.mSlabs, nullptr)),
    mSlabCount(std::exchange(other.mSlabCount, 0)),
    mSlabCapacity(std::exchange(other.mSlabCapacity, 0)),
    mFreeList(std::exchange(other.mFreeList, nullptr)),
    mLiveCount(std::exchange(other.mLiveCount, 0)) {}

pool_allocator &pool_allocator::operator=(pool_allocator &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    release_slabs();
    mBlockBytes = other.mBlockBytes;
    mSlabBytes = other.mSlabBytes;
    mAlignment = other.mAlignment;
    mBlocksPerSlab = other.mBlocksPerSlab;
    mMaxSlabs = other.mMaxSlabs;
    mSlabs = std::exchange(other.mSlabs, nullptr);
    mSlabCount = std::exchange(other.mSlabCount, 0);
    mSlabCapacity = std::exchange(other.mSlabCapacity, 0);
    mFreeList = std::exchange(other.mFreeList, nullptr);
    mLiveCount = std::exchange(other.mLiveCount, 0);
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void *pool_allocator::allocate() noexcept {
    if (mFreeList == nullptr && !grow()) {
        return nullptr;
    }

    free_block *node = mFreeList;
    mFreeList = node->next;

    const usize slabIndex = find_slab_index(node);
    CONJURE_ASSERT(slabIndex != NO_SLAB, "pool free list holds a block outside every slab");
    if (slabIndex != NO_SLAB) {
        slab &owner = mSlabs[slabIndex];
        const u32 index = block_index(owner, node);
        CONJURE_ASSERT(is_free(owner, index), "pool free list holds a live block");
        mark_free(owner, index, false);
        ++owner.liveCount;
    }
    ++mLiveCount;
    return node;
}

void pool_allocator::deallocate(void *block) noexcept {
    if (block == nullptr) {
        return;
    }

    const usize slabIndex = find_slab_index(block);
    const bool ownsBlock = slabIndex != NO_SLAB && is_block_start(mSlabs[slabIndex], block);
    CONJURE_ASSERT(ownsBlock, "pointer {} is not a block of this pool", block);
    if (!ownsBlock) {
        return;
    }

    slab &owner = mSlabs[slabIndex];
    const u32 index = block_index(owner, block);
    const bool alreadyFree = is_free(owner, index);
    CONJURE_ASSERT(!alreadyFree, "pointer {} was freed twice", block);
    if (alreadyFree) {
        return;
    }

    free_block *node = ::new (block) free_block{};
    node->next = mFreeList;
    mFreeList = node;
    mark_free(owner, index, true);
    --owner.liveCount;
    --mLiveCount;
}

void pool_allocator::shrink_to_fit() noexcept {
    const bool anyEmpty = std::any_of(mSlabs, mSlabs + mSlabCount, [](const slab &candidate) {
        return candidate.liveCount == 0;
    });
    if (!anyEmpty) {
        return;
    }

    // Rebuild the free list without the blocks of the slabs about to be released, keeping the order
    // of the remaining ones.
    free_block *kept = nullptr;
    free_block **tail = &kept;
    for (free_block *node = mFreeList; node != nullptr;) {
        free_block *next = node->next;
        const usize slabIndex = find_slab_index(node);
        if (slabIndex == NO_SLAB || mSlabs[slabIndex].liveCount != 0) {
            *tail = node;
            tail = &node->next;
        }
        node = next;
    }
    *tail = nullptr;
    mFreeList = kept;

    // Compact the table in place; it stays sorted because the relative order is preserved.
    usize keptCount = 0;
    for (usize index = 0; index < mSlabCount; ++index) {
        if (mSlabs[index].liveCount != 0) {
            mSlabs[keptCount] = mSlabs[index];
            ++keptCount;
            continue;
        }
        tracked_free(mSlabs[index].blocks, mSlabBytes);
    }
    mSlabCount = keptCount;
}

bool pool_allocator::owns(const void *pointer) const noexcept {
    const usize slabIndex = find_slab_index(pointer);
    return slabIndex != NO_SLAB && is_block_start(mSlabs[slabIndex], pointer);
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

bool pool_allocator::fits(usize size, usize alignment) const noexcept {
    const bool sizeFits = size <= mBlockBytes;
    const bool alignmentFits = alignment <= mAlignment;
    CONJURE_ASSERT(sizeFits, "type of {} bytes does not fit a {} byte pool block", size, mBlockBytes);
    CONJURE_ASSERT(alignmentFits, "type alignment {} exceeds pool alignment {}", alignment, mAlignment);
    return sizeFits && alignmentFits;
}

conjure::usize pool_allocator::find_slab_index(const void *pointer) const noexcept {
    const std::uintptr_t address = address_of(pointer);
    const slab *candidate = std::upper_bound(
      mSlabs,
      mSlabs + mSlabCount,
      address,
      [](std::uintptr_t value, const slab &entry) {
          return value < address_of(entry.blocks);
      }
    );
    if (candidate == mSlabs) {
        return NO_SLAB;
    }

    // A slab exists, so mBlockBytes * mBlocksPerSlab did not overflow in the constructor.
    const slab *owner = candidate - 1;
    if (address >= address_of(owner->blocks) + mBlockBytes * mBlocksPerSlab) {
        return NO_SLAB;
    }
    return static_cast<usize>(owner - mSlabs);
}

bool pool_allocator::is_block_start(const slab &owner, const void *pointer) const noexcept {
    const std::uintptr_t offset = address_of(pointer) - address_of(owner.blocks);
    return offset % mBlockBytes == 0;
}

conjure::u32 pool_allocator::block_index(const slab &owner, const void *pointer) const noexcept {
    const std::uintptr_t offset = address_of(pointer) - address_of(owner.blocks);
    return static_cast<u32>(offset / mBlockBytes);
}

conjure::u64 *pool_allocator::free_bits(const slab &owner) const noexcept {
    // The bitmap follows the blocks and starts word-aligned (the stride is a multiple of a pointer).
    return reinterpret_cast<u64 *>(owner.blocks + mBlockBytes * mBlocksPerSlab);
}

bool pool_allocator::is_free(const slab &owner, u32 index) const noexcept {
    const u64 mask = u64{1} << (index % BITS_PER_WORD);
    return (free_bits(owner)[index / BITS_PER_WORD] & mask) != 0;
}

void pool_allocator::mark_free(const slab &owner, u32 index, bool isFree) const noexcept {
    const u64 mask = u64{1} << (index % BITS_PER_WORD);
    u64 &word = free_bits(owner)[index / BITS_PER_WORD];
    if (isFree) {
        word |= mask;
    } else {
        word &= ~mask;
    }
}

bool pool_allocator::reserve_slab_table() noexcept {
    if (mSlabCount < mSlabCapacity) {
        return true;
    }

    // The table is trivially copyable, so the counted realloc may move it.
    const usize grownCapacity = mSlabCapacity == 0 ? INITIAL_SLAB_TABLE_CAPACITY : mSlabCapacity * 2;
    if (grownCapacity > std::numeric_limits<usize>::max() / sizeof(slab)) {
        return false;
    }
    const usize oldBytes = mSlabCapacity * sizeof(slab);
    void *table = tracked_reallocate(mSlabs, oldBytes, grownCapacity * sizeof(slab), alignof(slab));
    if (table == nullptr) {
        return false;
    }

    mSlabs = static_cast<slab *>(table);
    mSlabCapacity = grownCapacity;
    return true;
}

bool pool_allocator::grow() noexcept {
    if (mMaxSlabs != 0 && mSlabCount >= mMaxSlabs) {
        return false;
    }
    if (mSlabBytes == 0 || !reserve_slab_table()) {
        return false;
    }

    byte *blocks = static_cast<byte *>(tracked_allocate(mSlabBytes, mAlignment));
    if (blocks == nullptr) {
        return false;
    }

    // Every block starts free: the bitmap words are created set (bits past the last block are
    // never read).
    slab fresh = {};
    fresh.blocks = blocks;
    const usize words = bitmap_words(mBlocksPerSlab);
    for (usize word = 0; word < words; ++word) {
        ::new (static_cast<void *>(free_bits(fresh) + word)) u64{std::numeric_limits<u64>::max()};
    }

    // Insert in address order; reserve_slab_table() guaranteed the room.
    slab *position = std::lower_bound(
      mSlabs,
      mSlabs + mSlabCount,
      address_of(blocks),
      [](const slab &entry, std::uintptr_t value) {
          return address_of(entry.blocks) < value;
      }
    );
    std::copy_backward(position, mSlabs + mSlabCount, mSlabs + mSlabCount + 1);
    *position = fresh;
    ++mSlabCount;

    // Thread the blocks onto the free list in address order so consecutive allocations walk the
    // slab forwards.
    for (u32 index = mBlocksPerSlab; index > 0; --index) {
        void *storage = blocks + static_cast<usize>(index - 1) * mBlockBytes;
        free_block *node = ::new (storage) free_block{};
        node->next = mFreeList;
        mFreeList = node;
    }
    return true;
}

void pool_allocator::release_slabs() noexcept {
    for (usize index = 0; index < mSlabCount; ++index) {
        tracked_free(mSlabs[index].blocks, mSlabBytes);
    }
    tracked_free(mSlabs, mSlabCapacity * sizeof(slab));
    mSlabs = nullptr;
    mSlabCount = 0;
    mSlabCapacity = 0;
    mFreeList = nullptr;
    mLiveCount = 0;
}
