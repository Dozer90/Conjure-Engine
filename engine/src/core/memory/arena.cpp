#include "conjure/core/memory/arena.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

using namespace conjure::core::memory;

/*===========================================================================*/
/*===========================================================================*/
// Block

// A block is one tracked_allocate call: this header followed by 'capacity' bytes of data. The data
// area starts DEFAULT_ALIGNMENT-aligned, so a request with that alignment or less never needs
// padding in a fresh block and "fits in a block" is exact.
struct arena::block {
    block *next = nullptr;
    usize capacity = 0;
    usize used = 0;

    [[nodiscard]] static constexpr usize header_bytes() noexcept {
        return align_up(sizeof(block), DEFAULT_ALIGNMENT);
    }

    [[nodiscard]] byte *data() noexcept {
        return reinterpret_cast<byte *>(this) + header_bytes();
    }

    [[nodiscard]] const byte *data() const noexcept {
        return reinterpret_cast<const byte *>(this) + header_bytes();
    }

    /// <summary>
    /// Bumps 'bytes' bytes at 'alignment' out of the unused tail, or returns nullptr when they do
    /// not fit.
    /// </summary>
    [[nodiscard]] void *bump(usize bytes, usize alignment) noexcept {
        byte *cursor = data() + used;
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(cursor);
        const usize padding = align_up(address, alignment) - address;
        const usize available = capacity - used;
        if (padding > available || bytes > available - padding) {
            return nullptr;
        }

        used += padding + bytes;
        return cursor + padding;
    }
};

namespace {
    // Worst-case padding a request needs in a fresh block, whose data starts DEFAULT_ALIGNMENT-aligned.
    conjure::usize worst_padding(conjure::usize alignment) noexcept {
        return alignment > DEFAULT_ALIGNMENT ? alignment - DEFAULT_ALIGNMENT : 0;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

arena::arena(usize blockBytes) :
    mBlockBytes(blockBytes) {
    CONJURE_ASSERT(blockBytes > 0, "an arena block must hold at least one byte");
    if (blockBytes == 0) {
        mBlockBytes = DEFAULT_ARENA_BLOCK_BYTES;
    }
}

arena::~arena() {
    release();
}

arena::arena(arena &&other) noexcept :
    mBlockBytes(other.mBlockBytes),
    mFirst(std::exchange(other.mFirst, nullptr)),
    mCurrent(std::exchange(other.mCurrent, nullptr)),
    mBytesAllocated(std::exchange(other.mBytesAllocated, 0)),
    mBytesReserved(std::exchange(other.mBytesReserved, 0)),
    mBlockCount(std::exchange(other.mBlockCount, 0)) {}

arena &arena::operator=(arena &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    release();
    mBlockBytes = other.mBlockBytes;
    mFirst = std::exchange(other.mFirst, nullptr);
    mCurrent = std::exchange(other.mCurrent, nullptr);
    mBytesAllocated = std::exchange(other.mBytesAllocated, 0);
    mBytesReserved = std::exchange(other.mBytesReserved, 0);
    mBlockCount = std::exchange(other.mBlockCount, 0);
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void *arena::allocate(usize bytes, usize alignment) noexcept {
    const bool validAlignment = is_power_of_two(alignment);
    CONJURE_ASSERT(validAlignment, "alignment {} is not a power of two", alignment);
    if (!validAlignment || bytes == 0) {
        return nullptr;
    }

    if (mCurrent != nullptr) {
        void *fromCurrent = mCurrent->bump(bytes, alignment);
        if (fromCurrent != nullptr) {
            mBytesAllocated += bytes;
            return fromCurrent;
        }
    }

    // The request does not fit the current block. A regular block is big enough for anything up to
    // mBlockBytes; anything larger gets a block sized for it alone.
    const usize padding = worst_padding(alignment);
    if (bytes > std::numeric_limits<usize>::max() - padding) {
        return nullptr;
    }
    block *fresh = acquire_block(std::max(mBlockBytes, bytes + padding));
    if (fresh == nullptr) {
        return nullptr;
    }

    void *pointer = fresh->bump(bytes, alignment);
    mBytesAllocated += bytes;
    return pointer;
}

void arena::reset() noexcept {
    mBytesAllocated = 0;
    if (mFirst == nullptr) {
        return;
    }

    free_blocks_after(mFirst);
    mFirst->used = 0;
    mCurrent = mFirst;
}

void arena::release() noexcept {
    block *node = mFirst;
    while (node != nullptr) {
        block *next = node->next;
        tracked_free(node, block::header_bytes() + node->capacity);
        node = next;
    }

    mFirst = nullptr;
    mCurrent = nullptr;
    mBytesAllocated = 0;
    mBytesReserved = 0;
    mBlockCount = 0;
}

bool arena::owns(const void *pointer) const noexcept {
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    for (const block *node = mFirst; node != nullptr; node = node->next) {
        const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(node->data());
        if (address >= begin && address < begin + node->used) {
            return true;
        }
    }
    return false;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

arena::block *arena::acquire_block(usize capacity) noexcept {
    if (capacity > std::numeric_limits<usize>::max() - block::header_bytes()) {
        return nullptr;
    }

    void *memory = tracked_allocate(block::header_bytes() + capacity, DEFAULT_ALIGNMENT);
    if (memory == nullptr) {
        return nullptr;
    }

    block *fresh = ::new (memory) block{};
    fresh->capacity = capacity;
    mBytesReserved += capacity;
    ++mBlockCount;

    if (mFirst == nullptr) {
        mFirst = fresh;
        mCurrent = fresh;
        return fresh;
    }

    // New blocks are linked right behind the current one. A dedicated oversize block leaves the
    // current block in place so its remaining space keeps serving small requests; a regular block
    // becomes the new current block.
    fresh->next = mCurrent->next;
    mCurrent->next = fresh;
    if (capacity <= mBlockBytes) {
        mCurrent = fresh;
    }
    return fresh;
}

void arena::free_blocks_after(block *keep) noexcept {
    block *node = keep->next;
    while (node != nullptr) {
        block *next = node->next;
        mBytesReserved -= node->capacity;
        --mBlockCount;
        tracked_free(node, block::header_bytes() + node->capacity);
        node = next;
    }
    keep->next = nullptr;
}
