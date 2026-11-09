#pragma once

#include "conjure/core/memory/pool_allocator.h"
#include "conjure/core/types.h"

// RAII owner of the 16 KiB, 64-byte aligned chunk blocks of one world (docs/design/01_ecs.md 2.3).
// Not thread-safe: the world serialises every allocation through its structural operations.

namespace conjure::ecs {

    class chunk_pool {
        private:
            core::memory::pool_allocator mPool;   // no default: sized by the constructor arguments

        public:
            /// <summary>
            /// 'maxSlabs' caps growth (allocate() returns nullptr beyond it); 0 means unlimited.
            /// </summary>
            chunk_pool(u32 chunksPerSlab, u32 maxSlabs);
            ~chunk_pool();

            chunk_pool(const chunk_pool &) = delete;
            chunk_pool &operator=(const chunk_pool &) = delete;
            chunk_pool(chunk_pool &&) = delete;
            chunk_pool &operator=(chunk_pool &&) = delete;

        public:
            /// <summary>
            /// One CHUNK_SIZE block aligned to CHUNK_ALIGNMENT, or nullptr when the heap is exhausted.
            /// </summary>
            [[nodiscard]] void *allocate() noexcept;

            /// <summary>
            /// Returns a block. nullptr is ignored.
            /// </summary>
            void deallocate(void *block) noexcept;

            /// <summary>
            /// Blocks handed out and not yet returned.
            /// </summary>
            [[nodiscard]] usize live_count() const noexcept {
                return mPool.live_count();
            }

            /// <summary>
            /// Returns fully free slabs to the heap.
            /// </summary>
            void shrink_to_fit() noexcept {
                mPool.shrink_to_fit();
            }
    };
}
