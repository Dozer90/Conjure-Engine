#pragma once

#include "conjure/core/api.h"
#include "conjure/core/memory/tracked_allocation.h"
#include "conjure/core/types.h"

#include <new>
#include <utility>

namespace conjure::core::memory {

    /// <summary>
    /// Fixed-size block allocator. Blocks are carved from slabs of blocksPerSlab blocks; the pool
    /// grows by one slab whenever the free list is empty (up to maxSlabs when a cap is given) and
    /// shrink_to_fit() returns slabs with no live block. Blocks are pure data: a block is only used
    /// as a free-list link while it is free, so a 16 KiB block really holds 16 KiB.
    ///
    /// allocate() and deallocate() are O(log slabCount): a sorted slab table resolves the owning slab,
    /// which keeps per-slab live counts exact and lets every slab carry a free bitmap, so a double
    /// free is detected (asserted and ignored) even while neighbouring blocks are live. Every byte the
    /// pool holds, the slab table included, comes from tracked_allocate: the pool is fully counted and
    /// never throws. Not thread-safe: the ECS wraps a pool under its own lock.
    /// </summary>
    class CONJURE_API pool_allocator {
        private:
            struct slab {
                byte *blocks = nullptr;     // first block; the free bitmap follows the last block
                u32 liveCount = 0;
            };

            struct free_block {
                free_block *next = nullptr;
            };

            usize mBlockBytes = 0;          // distance between blocks: blockSize rounded up to the alignment
            usize mSlabBytes = 0;           // one slab allocation: blocks plus bitmap; 0 when that would overflow
            usize mAlignment = DEFAULT_ALIGNMENT;
            u32 mBlocksPerSlab = 1;
            u32 mMaxSlabs = 0;              // 0 = unlimited
            slab *mSlabs = nullptr;         // sorted by address so lookups can binary search
            usize mSlabCount = 0;
            usize mSlabCapacity = 0;        // entries the table can hold before it is regrown
            free_block *mFreeList = nullptr;
            usize mLiveCount = 0;

        public:
            /// <summary>
            /// 'blockSize' is rounded up to a multiple of 'alignment' and to at least one pointer so a
            /// free block can hold its link. 'alignment' must be a power of two (asserts and falls back
            /// to DEFAULT_ALIGNMENT otherwise), 'blocksPerSlab' at least 1 (asserts and uses 1
            /// otherwise). 'maxSlabs' caps growth; 0 means unlimited. Nothing is allocated until the
            /// first allocate().
            /// </summary>
            pool_allocator(usize blockSize, usize alignment, u32 blocksPerSlab, u32 maxSlabs = 0);
            ~pool_allocator();

            pool_allocator(const pool_allocator &) = delete;
            pool_allocator &operator=(const pool_allocator &) = delete;

            pool_allocator(pool_allocator &&other) noexcept;
            pool_allocator &operator=(pool_allocator &&other) noexcept;

        public:
            /// <summary>
            /// Returns one block, growing by a slab when none is free. Returns nullptr when the slab cap
            /// is reached, a slab would be larger than MAX_ALLOCATION_BYTES, or a new slab (or the room
            /// to record it) cannot be obtained from the heap.
            /// </summary>
            [[nodiscard]] void *allocate() noexcept;

            /// <summary>
            /// Returns a block to the pool. nullptr is ignored. A pointer the pool does not own, one that
            /// is not the start of a block, or a block that is already free asserts and is ignored.
            /// </summary>
            void deallocate(void *block) noexcept;

            /// <summary>
            /// allocate() typed as a pointer to uninitialised storage for a T. Asserts (and returns
            /// nullptr) when a T does not fit a block or needs more alignment than the pool gives.
            /// </summary>
            template <typename T>
            [[nodiscard]] T *allocate_as() noexcept {
                if (!fits(sizeof(T), alignof(T))) {
                    return nullptr;
                }
                return static_cast<T *>(allocate());
            }

            /// <summary>
            /// Allocates a block and constructs a T in it. Returns nullptr when allocation fails or a T
            /// does not fit a block. Pair with destroy().
            /// </summary>
            template <typename T, typename... Args>
            [[nodiscard]] T *construct(Args &&...args) {
                T *storage = allocate_as<T>();
                if (storage == nullptr) {
                    return nullptr;
                }
                return ::new (static_cast<void *>(storage)) T(std::forward<Args>(args)...);
            }

            /// <summary>
            /// Destroys an object made by construct() and returns its block. nullptr is ignored.
            /// </summary>
            template <typename T>
            void destroy(T *object) noexcept {
                if (object == nullptr) {
                    return;
                }
                object->~T();
                deallocate(object);
            }

            /// <summary>
            /// Returns every slab that has no live block to the heap. Blocks in the remaining slabs are
            /// untouched; the slab table keeps its capacity.
            /// </summary>
            void shrink_to_fit() noexcept;

            /// <summary>
            /// Blocks currently handed out.
            /// </summary>
            [[nodiscard]] usize live_count() const noexcept {
                return mLiveCount;
            }

            /// <summary>
            /// Blocks the held slabs can hold in total (live and free).
            /// </summary>
            [[nodiscard]] usize capacity() const noexcept {
                return mSlabCount * mBlocksPerSlab;
            }

            /// <summary>
            /// Slabs currently held.
            /// </summary>
            [[nodiscard]] usize slab_count() const noexcept {
                return mSlabCount;
            }

            /// <summary>
            /// Effective block size: the requested size rounded up to the alignment and to a pointer.
            /// </summary>
            [[nodiscard]] usize block_size() const noexcept {
                return mBlockBytes;
            }

            /// <summary>
            /// Alignment of every block.
            /// </summary>
            [[nodiscard]] usize alignment() const noexcept {
                return mAlignment;
            }

            /// <summary>
            /// Blocks each slab holds, as passed to the constructor (at least 1).
            /// </summary>
            [[nodiscard]] u32 blocks_per_slab() const noexcept {
                return mBlocksPerSlab;
            }

            /// <summary>
            /// Slab cap given to the constructor; 0 means unlimited.
            /// </summary>
            [[nodiscard]] u32 max_slabs() const noexcept {
                return mMaxSlabs;
            }

            /// <summary>
            /// True when 'pointer' is the start of a block in one of the pool's slabs (live or free).
            /// </summary>
            [[nodiscard]] bool owns(const void *pointer) const noexcept;

        private:
            [[nodiscard]] bool fits(usize size, usize alignment) const noexcept;
            [[nodiscard]] usize find_slab_index(const void *pointer) const noexcept;
            [[nodiscard]] bool is_block_start(const slab &owner, const void *pointer) const noexcept;
            [[nodiscard]] u32 block_index(const slab &owner, const void *pointer) const noexcept;
            [[nodiscard]] u64 *free_bits(const slab &owner) const noexcept;
            [[nodiscard]] bool is_free(const slab &owner, u32 index) const noexcept;
            void mark_free(const slab &owner, u32 index, bool isFree) const noexcept;
            [[nodiscard]] bool reserve_slab_table() noexcept;
            bool grow() noexcept;
            void release_slabs() noexcept;
    };
}
