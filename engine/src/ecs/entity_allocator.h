#pragma once

#include "conjure/ecs/entity.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <mutex>
#include <span>
#include <vector>

// Entity id allocation (docs/design/01_ecs.md 2.1). reserve() is the module's only mutex and may be
// called from any thread (command buffers refill their 64-id blocks through it); release() and
// return_unused() belong to the owner thread. The allocator never touches the world's record table.

namespace conjure::ecs {

    /// <summary>
    /// Largest index the allocator issues; one below the all-ones index so (index + 1) never wraps.
    /// </summary>
    inline constexpr u32 MAX_ENTITY_INDEX = 0xFFFFFFFEu;

    class entity_allocator {
        private:
            struct free_slot {
                u32 index = 0;
                u32 generation = 0;     // the generation the next occupant receives
            };

            mutable std::mutex mMutex = {};
            std::vector<free_slot> mFreeList = {};   // LIFO
            u32 mNextIndex = 0;
            u32 mRetiredCount = 0;

        public:
            entity_allocator();
            ~entity_allocator();

            entity_allocator(const entity_allocator &) = delete;
            entity_allocator &operator=(const entity_allocator &) = delete;
            entity_allocator(entity_allocator &&) = delete;
            entity_allocator &operator=(entity_allocator &&) = delete;

        public:
            /// <summary>
            /// Fills 'out' with unique ids: free slots first (LIFO), then fresh indices with generation
            /// 1. Thread-safe. ENTITY_LIMIT_REACHED when the index space is exhausted (nothing is taken).
            /// </summary>
            [[nodiscard]] core::result<void> reserve(std::span<entity> out);

            /// <summary>
            /// Frees the index of a dead entity: the generation is bumped for the next occupant and the
            /// slot is pushed on the free list, unless the next generation would reach
            /// ARCHIVE_LOCAL_GENERATION, in which case the index is retired for good. Owner thread.
            /// </summary>
            void release(entity target);

            /// <summary>
            /// Pushes reserved-but-never-placed ids back without bumping their generation. Owner thread.
            /// </summary>
            void return_unused(std::span<const entity> unused);

            /// <summary>
            /// One past the highest index ever issued. Thread-safe.
            /// </summary>
            [[nodiscard]] u32 peak_index() const noexcept;

            /// <summary>
            /// Slots waiting for reuse.
            /// </summary>
            [[nodiscard]] usize free_count() const noexcept {
                return mFreeList.size();
            }

            /// <summary>
            /// Indices retired because their generation space ran out.
            /// </summary>
            [[nodiscard]] u32 retired_count() const noexcept {
                return mRetiredCount;
            }

            /// <summary>
            /// The free slots (index, next generation) for validation.
            /// </summary>
            [[nodiscard]] std::vector<entity> free_slots() const;
    };
}
