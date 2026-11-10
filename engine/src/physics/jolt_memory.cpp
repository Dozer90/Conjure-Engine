#include "physics/jolt_memory.h"

#include "physics/jolt_include.h"

#include "conjure/core/assert.h"
#include "conjure/core/memory/tracked_allocation.h"

#include <Jolt/Core/Memory.h>

#include <algorithm>
#include <cstring>

namespace conjure::physics::jolt_memory {

namespace {

    /// <summary>
    /// Stored at (block - HEADER_BYTES) of every block Jolt sees.
    /// </summary>
    struct block_header {
        u64 totalBytes = 0;          // bytes asked from tracked_allocate
        u32 offsetToBase = 0;        // distance from the header to the tracked base
        u32 alignment = 0;           // alignment the tracked base was requested with
    };

    static_assert(sizeof(block_header) == HEADER_BYTES, "the header must be exactly 16 bytes");

    [[nodiscard]] block_header *header_of(void *block) noexcept {
        return reinterpret_cast<block_header *>(static_cast<std::byte *>(block) - HEADER_BYTES);
    }

    [[nodiscard]] void *allocate_with_alignment(std::size_t bytes, std::size_t alignment) noexcept {
        const usize usedAlignment = std::max<usize>(alignment, HEADER_BYTES);
        if (!core::memory::is_power_of_two(usedAlignment)) {
            CONJURE_ASSERT(false, "jolt_memory: alignment {} is not a power of two", alignment);
            return nullptr;
        }
        // The block starts one alignment past the base, so it is aligned and the header fits before it.
        const usize total = bytes + usedAlignment;
        std::byte *base = static_cast<std::byte *>(core::memory::tracked_allocate(total, usedAlignment));
        if (base == nullptr) {
            return nullptr;
        }
        std::byte *block = base + usedAlignment;
        block_header *header = header_of(block);
        header->totalBytes = total;
        header->offsetToBase = static_cast<u32>(usedAlignment - HEADER_BYTES);
        header->alignment = static_cast<u32>(usedAlignment);
        return block;
    }

    void release_block(void *block) noexcept {
        if (block == nullptr) {
            return;
        }
        block_header *header = header_of(block);
        std::byte *base = reinterpret_cast<std::byte *>(header) - header->offsetToBase;
        core::memory::tracked_free(base, static_cast<usize>(header->totalBytes));
    }
}

void *allocate(std::size_t bytes) noexcept {
    return allocate_with_alignment(bytes, HEADER_BYTES);
}

void *reallocate(void *block, std::size_t oldBytes, std::size_t newBytes) noexcept {
    if (block == nullptr) {
        return allocate(newBytes);
    }
    if (newBytes == 0) {
        release_block(block);
        return nullptr;
    }
    void *grown = allocate(newBytes);
    if (grown == nullptr) {
        return nullptr;
    }
    std::memcpy(grown, block, std::min(oldBytes, newBytes));
    release_block(block);
    return grown;
}

void release(void *block) noexcept {
    release_block(block);
}

void *aligned_allocate(std::size_t bytes, std::size_t alignment) noexcept {
    return allocate_with_alignment(bytes, alignment);
}

void aligned_release(void *block) noexcept {
    release_block(block);
}

void install_hooks() noexcept {
    JPH::Allocate = &allocate;
    JPH::Reallocate = &reallocate;
    JPH::Free = &release;
    JPH::AlignedAllocate = &aligned_allocate;
    JPH::AlignedFree = &aligned_release;
}
}
