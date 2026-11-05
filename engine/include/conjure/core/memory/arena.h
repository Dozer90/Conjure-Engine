#pragma once

#include "conjure/core/memory/tracked_allocation.h"
#include "conjure/core/types.h"

#include <new>
#include <type_traits>
#include <utility>

namespace conjure::core::memory {

    /// <summary>
    /// Block size an arena uses when the caller does not choose one.
    /// </summary>
    inline constexpr usize DEFAULT_ARENA_BLOCK_BYTES = 64 * KIB;

    /// <summary>
    /// Linear (bump) allocator over a chain of fixed-size blocks. Individual allocations are never
    /// freed; reset() recycles the whole arena at once, which is what command buffers, parsers and
    /// per-load scratch memory want. Addresses are stable until reset() or release(). A request that
    /// does not fit in a block gets a dedicated block of its own; the current block keeps serving
    /// smaller requests. Blocks are obtained with tracked_allocate, so every acquisition shows up in
    /// the allocation counters.
    ///
    /// Not thread-safe: one arena belongs to one thread (or to a lock held by its owner).
    /// </summary>
    class arena {
        private:
            struct block;

            usize mBlockBytes = DEFAULT_ARENA_BLOCK_BYTES;
            block *mFirst = nullptr;        // head of the chain, kept across reset()
            block *mCurrent = nullptr;      // block that bump allocations are served from
            usize mBytesAllocated = 0;
            usize mBytesReserved = 0;
            usize mBlockCount = 0;

        public:
            explicit arena(usize blockBytes = DEFAULT_ARENA_BLOCK_BYTES);
            ~arena();

            arena(const arena &) = delete;
            arena &operator=(const arena &) = delete;

            arena(arena &&other) noexcept;
            arena &operator=(arena &&other) noexcept;

        public:
            /// <summary>
            /// Returns 'bytes' bytes aligned to 'alignment' (a power of two, up to and beyond 256).
            /// Bumps within the current block; a request that does not fit in a block of the configured
            /// size gets its own block. Returns nullptr for a zero-byte request, for an alignment that
            /// is not a power of two (asserts) and when the heap is exhausted.
            /// </summary>
            [[nodiscard]] void *allocate(usize bytes, usize alignment = DEFAULT_ALIGNMENT) noexcept;

            /// <summary>
            /// Allocates and constructs a T in the arena. T must be trivially destructible because
            /// reset() and release() never run destructors. Returns nullptr when allocation fails.
            /// </summary>
            template <typename T, typename... Args>
            [[nodiscard]] T *construct(Args &&...args) {
                static_assert(
                  std::is_trivially_destructible_v<T>,
                  "arena::construct requires a trivially destructible type: reset() never runs destructors"
                );
                void *storage = allocate(sizeof(T), alignof(T));
                if (storage == nullptr) {
                    return nullptr;
                }
                return ::new (storage) T(std::forward<Args>(args)...);
            }

            /// <summary>
            /// Invalidates every allocation. The first block is kept for reuse, every other block is
            /// returned to the heap.
            /// </summary>
            void reset() noexcept;

            /// <summary>
            /// Invalidates every allocation and returns every block to the heap. The arena stays
            /// usable; the next allocate() obtains a fresh block.
            /// </summary>
            void release() noexcept;

            /// <summary>
            /// Sum of the bytes handed out since the last reset(), excluding alignment padding.
            /// </summary>
            [[nodiscard]] usize bytes_allocated() const noexcept {
                return mBytesAllocated;
            }

            /// <summary>
            /// Sum of the capacities of the blocks currently held (the usable bytes, excluding the
            /// small per-block header).
            /// </summary>
            [[nodiscard]] usize bytes_reserved() const noexcept {
                return mBytesReserved;
            }

            /// <summary>
            /// Number of blocks currently held.
            /// </summary>
            [[nodiscard]] usize block_count() const noexcept {
                return mBlockCount;
            }

            /// <summary>
            /// Capacity of a regular block, as passed to the constructor.
            /// </summary>
            [[nodiscard]] usize block_bytes() const noexcept {
                return mBlockBytes;
            }

            /// <summary>
            /// True when 'pointer' lies inside the used part of one of the arena's blocks. Linear in
            /// the block count; meant for assertions and tests.
            /// </summary>
            [[nodiscard]] bool owns(const void *pointer) const noexcept;

        private:
            block *acquire_block(usize capacity) noexcept;
            void free_blocks_after(block *keep) noexcept;
    };
}
