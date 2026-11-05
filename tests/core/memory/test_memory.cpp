#include "conjure/testing/doctest.h"

#include "conjure/core/memory/memory.h"

#include <cstring>
#include <limits>
#include <type_traits>

using namespace conjure;
using namespace conjure::core::memory;

TEST_SUITE("core/memory") {

    TEST_CASE("memory - the umbrella header exposes every public type and function") {
        static_assert(std::is_same_v<decltype(counters()), allocation_counters &>);
        static_assert(std::is_same_v<decltype(snapshot()), memory_snapshot>);
        static_assert(std::is_same_v<decltype(tracked_allocate(usize{1})), void *>);
        static_assert(std::is_nothrow_move_constructible_v<arena>);
        static_assert(std::is_nothrow_move_constructible_v<pool_allocator>);
        static_assert(!std::is_copy_constructible_v<arena>);
        static_assert(!std::is_copy_constructible_v<pool_allocator>);
        static_assert(!std::is_copy_constructible_v<frame_allocator>);
        static_assert(!std::is_move_constructible_v<frame_allocator>);
        static_assert(DEFAULT_ALIGNMENT >= 8);
        static_assert(MAX_ALLOCATION_BYTES == static_cast<usize>(std::numeric_limits<isize>::max()));
        static_assert(DEFAULT_ARENA_BLOCK_BYTES == 64 * KIB);

        arena scratch{};
        pool_allocator pool(32, 16, 4);
        frame_allocator frame(4 * KIB, 1 * KIB);
        CHECK(scratch.block_count() == 0);
        CHECK(pool.slab_count() == 0);
        CHECK(frame.capacity_bytes() == 4 * KIB);
    }

    TEST_CASE("memory - every allocator reports its heap acquisitions through the counters") {
        const memory_snapshot before = snapshot();
        {
            arena scratch(1 * KIB);
            pool_allocator pool(64, 64, 8);
            frame_allocator frame(4 * KIB, 1 * KIB);
            CHECK(snapshot().allocationCount == before.allocationCount + 1);   // the frame arena

            void *fromArena = scratch.allocate(100);
            void *fromPool = pool.allocate();
            void *fromFrame = frame.allocate(100);
            void *fromHeap = tracked_allocate(100);
            REQUIRE(fromArena != nullptr);
            REQUIRE(fromPool != nullptr);
            REQUIRE(fromFrame != nullptr);
            REQUIRE(fromHeap != nullptr);
            std::memset(fromArena, 1, 100);
            std::memset(fromPool, 2, 64);
            std::memset(fromFrame, 3, 100);
            std::memset(fromHeap, 4, 100);

            // The frame arena, one arena block, one pool slab plus its slab table, and the heap block;
            // the frame request fits the arena.
            const memory_snapshot during = snapshot();
            CHECK(during.allocationCount == before.allocationCount + 5);
            CHECK(during.allocatedBytes >= before.allocatedBytes + 1 * KIB + 8 * 64 + 4 * KIB + 100);

            tracked_free(fromHeap, 100);
            pool.deallocate(fromPool);
        }
        const memory_snapshot after = snapshot();
        CHECK(after.freeCount == before.freeCount + 5);
        CHECK(after.live_count() == before.live_count());
    }
}
