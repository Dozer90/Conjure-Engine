#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/memory/pool_allocator.h"
#include "conjure/core/memory/tracked_allocation.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core::memory;

namespace {
    struct big_payload {
        byte data[128] = {};
    };

    // Fills its alignment exactly so MSVC does not warn about padding (C4324).
    struct alignas(64) over_aligned {
        int value = 0;
        int padding[15] = {};
    };

    struct counted {
        static inline int sLive = 0;
        int value = 0;

        explicit counted(int initial) :
            value(initial) {
            ++sLive;
        }

        ~counted() {
            --sLive;
        }

        counted(const counted &) = delete;
        counted &operator=(const counted &) = delete;
    };

    const byte *as_bytes(const void *pointer) {
        return static_cast<const byte *>(pointer);
    }
}

TEST_SUITE("core/pool_allocator") {

    TEST_CASE("pool_allocator - block size is rounded up to the alignment and to a pointer") {
        pool_allocator tiny(1, 1, 4);
        CHECK(tiny.block_size() == sizeof(void *));
        CHECK(tiny.alignment() == alignof(void *));

        pool_allocator rounded(20, 16, 4);
        CHECK(rounded.block_size() == 32);

        pool_allocator chunks(16 * KIB, 64, 4);
        CHECK(chunks.block_size() == 16 * KIB);
        CHECK(chunks.alignment() == 64);
        CHECK(chunks.blocks_per_slab() == 4);
        CHECK(chunks.max_slabs() == 0);
        CHECK(chunks.slab_count() == 0);
        CHECK(chunks.capacity() == 0);
        CHECK(chunks.live_count() == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F222: a small-alignment pool_allocator hands out pointer-aligned blocks for u64"
    ) {
        testing::expect_assert asserts = {};
        pool_allocator pool(16, 4, 64);
        CHECK(pool.alignment() == alignof(void *));
        CHECK(pool.block_size() == 16);

        u64 *value = pool.allocate_as<u64>();
        REQUIRE(value != nullptr);
        CHECK(is_aligned(value, alignof(u64)));
        CHECK(asserts.count() == 0);

        // Every block of the slab shares the guarantee, not just the first.
        void *second = pool.allocate();
        REQUIRE(second != nullptr);
        CHECK(is_aligned(second, alignof(void *)));

        pool.deallocate(second);
        pool.deallocate(value);
        CHECK(pool.live_count() == 0);
    }

    TEST_CASE("pool_allocator - an invalid alignment or an empty slab asserts and falls back") {
        testing::expect_assert asserts = {};
        pool_allocator pool(10, 12, 0);
        CHECK(asserts.count() == 2);
        CHECK(pool.alignment() == DEFAULT_ALIGNMENT);
        CHECK(pool.blocks_per_slab() == 1);
        // 10 bytes rounded to the fallback alignment and to a pointer: 16 whether max_align_t is 8 or 16.
        CHECK(pool.block_size() == 16);

        void *block = pool.allocate();
        REQUIRE(block != nullptr);
        CHECK(is_aligned(block, DEFAULT_ALIGNMENT));
        pool.deallocate(block);
    }

    TEST_CASE("pool_allocator - allocate hands out distinct aligned blocks and grows by slabs") {
        pool_allocator pool(64, 64, 4);
        std::vector<void *> blocks = {};
        blocks.reserve(5);
        const memory_snapshot before = snapshot();

        for (int index = 0; index < 5; ++index) {
            void *block = pool.allocate();
            REQUIRE(block != nullptr);
            CHECK(is_aligned(block, 64));
            CHECK(pool.owns(block));
            blocks.push_back(block);
        }

        CHECK(pool.slab_count() == 2);
        CHECK(pool.capacity() == 8);
        CHECK(pool.live_count() == 5);
        // Two slabs plus the slab table, which is counted like every other pool allocation.
        CHECK(snapshot().allocationCount == before.allocationCount + 3);

        // The first slab is handed out front to back.
        for (usize index = 1; index < 4; ++index) {
            CHECK(as_bytes(blocks[index]) == as_bytes(blocks[index - 1]) + 64);
        }

        std::vector<void *> sorted = blocks;
        std::sort(sorted.begin(), sorted.end());
        CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
    }

    TEST_CASE("pool_allocator - deallocate recycles the most recently freed block first") {
        pool_allocator pool(32, 16, 4);
        void *first = pool.allocate();
        void *second = pool.allocate();
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);

        pool.deallocate(first);
        CHECK(pool.live_count() == 1);
        pool.deallocate(second);
        CHECK(pool.live_count() == 0);

        CHECK(pool.allocate() == second);
        CHECK(pool.allocate() == first);
        CHECK(pool.live_count() == 2);
        CHECK(pool.slab_count() == 1);
    }

    TEST_CASE("pool_allocator - a slab cap makes allocate return nullptr on exhaustion") {
        pool_allocator pool(32, 16, 4, 2);
        CHECK(pool.max_slabs() == 2);

        std::vector<void *> blocks = {};
        for (int index = 0; index < 8; ++index) {
            void *block = pool.allocate();
            REQUIRE(block != nullptr);
            blocks.push_back(block);
        }
        CHECK(pool.slab_count() == 2);
        CHECK(pool.capacity() == 8);

        CHECK(pool.allocate() == nullptr);
        CHECK(pool.allocate() == nullptr);
        CHECK(pool.live_count() == 8);
        CHECK(pool.slab_count() == 2);

        pool.deallocate(blocks[3]);
        CHECK(pool.allocate() == blocks[3]);
        CHECK(pool.allocate() == nullptr);
    }

    TEST_CASE("pool_allocator - a slab cap of zero means unlimited growth") {
        pool_allocator pool(16, 16, 1);
        const memory_snapshot before = snapshot();
        for (int index = 0; index < 100; ++index) {
            REQUIRE(pool.allocate() != nullptr);
        }
        CHECK(pool.slab_count() == 100);
        CHECK(pool.capacity() == 100);

        // The slab table grows by doubling from 8 entries: 8, 16, 32, 64, 128 is one allocation and
        // four counted reallocations (each one free plus one allocation).
        const memory_snapshot after = snapshot();
        CHECK(after.allocationCount == before.allocationCount + 100 + 5);
        CHECK(after.freeCount == before.freeCount + 4);
        CHECK(after.live_count() == before.live_count() + 101);
    }

    TEST_CASE("pool_allocator - a block size too large for any slab makes allocate return nullptr") {
        testing::expect_assert asserts = {};
        constexpr usize USIZE_MAX = std::numeric_limits<usize>::max();
        const memory_snapshot before = snapshot();

        pool_allocator oversized(MAX_ALLOCATION_BYTES, 8, 1);
        CHECK(oversized.allocate() == nullptr);
        CHECK(oversized.slab_count() == 0);
        CHECK(oversized.live_count() == 0);

        pool_allocator wrapping(USIZE_MAX / 2, 8, 4);
        CHECK(wrapping.allocate() == nullptr);
        CHECK(wrapping.slab_count() == 0);

        CHECK(asserts.count() == 0);
        // Only the slab table of the first pool was obtained; the slab itself was refused.
        CHECK(snapshot().allocationCount == before.allocationCount + 1);
        CHECK(snapshot().allocatedBytes < before.allocatedBytes + 1 * KIB);
    }

    TEST_CASE("pool_allocator - deallocate ignores nullptr") {
        testing::expect_assert asserts = {};
        pool_allocator pool(32, 16, 4);
        REQUIRE(pool.allocate() != nullptr);

        pool.deallocate(nullptr);

        CHECK(asserts.count() == 0);
        CHECK(pool.live_count() == 1);
    }

    TEST_CASE("pool_allocator - deallocate of a pointer the pool does not own asserts and is ignored") {
        testing::expect_assert asserts = {};
        pool_allocator pool(32, 16, 4);
        void *block = pool.allocate();
        REQUIRE(block != nullptr);

        int local = 0;
        pool.deallocate(&local);
        CHECK(asserts.count() == 1);
        CHECK(pool.live_count() == 1);

        // Inside a block but not at its start.
        pool.deallocate(static_cast<byte *>(block) + 8);
        CHECK(asserts.count() == 2);
        CHECK(pool.live_count() == 1);

        // Just past the end of the slab.
        pool.deallocate(static_cast<byte *>(block) + 4 * 32);
        CHECK(asserts.count() == 3);
        CHECK(pool.live_count() == 1);

        pool.deallocate(block);
        CHECK(asserts.count() == 3);
        CHECK(pool.live_count() == 0);
    }

    TEST_CASE(
      "pool_allocator - freeing a block twice in a slab with no live block asserts and keeps the free list intact"
    ) {
        testing::expect_assert asserts = {};
        pool_allocator pool(32, 16, 1);
        void *block = pool.allocate();
        REQUIRE(block != nullptr);

        pool.deallocate(block);
        pool.deallocate(block);

        CHECK(asserts.count() == 1);
        CHECK(pool.live_count() == 0);

        void *first = pool.allocate();
        void *second = pool.allocate();
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(first != second);
        CHECK(pool.live_count() == 2);
    }

    TEST_CASE("pool_allocator - freeing a block twice in a slab with live blocks asserts and is ignored") {
        // Regression: only a slab with no live block used to catch a double free, so the common case
        // pushed the same node onto the free list twice and allocate() handed it out twice.
        testing::expect_assert asserts = {};
        pool_allocator pool(32, 16, 4);
        void *first = pool.allocate();
        void *second = pool.allocate();
        void *third = pool.allocate();
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(third != nullptr);
        std::memset(first, 0xA1, 32);
        std::memset(third, 0xA3, 32);

        pool.deallocate(second);
        CHECK(pool.live_count() == 2);
        pool.deallocate(second);
        CHECK(asserts.count() == 1);
        CHECK(pool.live_count() == 2);

        // The ignored free left the free list intact: the block comes back exactly once, and the
        // last block of the slab is protected the same way.
        void *fourth = pool.allocate();
        REQUIRE(fourth == second);
        void *fifth = pool.allocate();
        REQUIRE(fifth != nullptr);
        CHECK(fifth != second);
        CHECK(pool.live_count() == 4);
        pool.deallocate(fifth);
        pool.deallocate(fifth);
        CHECK(asserts.count() == 2);
        CHECK(pool.live_count() == 3);

        // The live neighbours were never touched by the ignored frees.
        bool intact = true;
        for (usize offset = 0; offset < 32; ++offset) {
            intact = intact && as_bytes(first)[offset] == static_cast<byte>(0xA1);
            intact = intact && as_bytes(third)[offset] == static_cast<byte>(0xA3);
        }
        CHECK(intact);

        // Every remaining block is distinct: the free list holds no duplicate.
        std::vector<void *> handed = {};
        handed.reserve(8);
        handed.push_back(first);
        handed.push_back(third);
        handed.push_back(fourth);
        for (int index = 0; index < 5; ++index) {
            void *block = pool.allocate();
            REQUIRE(block != nullptr);
            handed.push_back(block);
        }
        std::sort(handed.begin(), handed.end());
        CHECK(std::adjacent_find(handed.begin(), handed.end()) == handed.end());
        CHECK(pool.live_count() == 8);
    }

    TEST_CASE("pool_allocator - blocks freed and reallocated keep their free bits in step") {
        pool_allocator pool(16, 16, 64);
        std::vector<void *> blocks = {};
        blocks.reserve(64);
        for (int index = 0; index < 64; ++index) {
            blocks.push_back(pool.allocate());
            REQUIRE(blocks.back() != nullptr);
        }
        CHECK(pool.slab_count() == 1);

        // Free every other block, then both halves in turn; the bitmap must track each transition.
        for (usize index = 0; index < 64; index += 2) {
            pool.deallocate(blocks[index]);
        }
        CHECK(pool.live_count() == 32);
        for (usize index = 0; index < 64; index += 2) {
            REQUIRE(pool.allocate() != nullptr);
        }
        CHECK(pool.live_count() == 64);
        CHECK(pool.slab_count() == 1);

        testing::expect_assert asserts = {};
        for (void *block : blocks) {
            pool.deallocate(block);
        }
        CHECK(asserts.count() == 0);
        CHECK(pool.live_count() == 0);
        for (void *block : blocks) {
            pool.deallocate(block);
        }
        CHECK(asserts.count() == 64);
        CHECK(pool.live_count() == 0);
    }

    TEST_CASE("pool_allocator - allocate_as returns typed storage and rejects types that do not fit") {
        testing::expect_assert asserts = {};
        pool_allocator pool(64, 16, 4);

        int *number = pool.allocate_as<int>();
        REQUIRE(number != nullptr);
        *number = 42;
        CHECK(is_aligned(number, alignof(int)));
        CHECK(pool.live_count() == 1);

        CHECK(pool.allocate_as<big_payload>() == nullptr);
        CHECK(asserts.count() == 1);
        CHECK(pool.allocate_as<over_aligned>() == nullptr);
        CHECK(asserts.count() == 2);
        CHECK(pool.live_count() == 1);

        pool.deallocate(number);
    }

    TEST_CASE("pool_allocator - construct and destroy run constructors and destructors") {
        pool_allocator pool(sizeof(counted), alignof(counted), 2);
        counted::sLive = 0;

        counted *first = pool.construct<counted>(1);
        counted *second = pool.construct<counted>(2);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(counted::sLive == 2);
        CHECK(first->value == 1);
        CHECK(second->value == 2);
        CHECK(pool.live_count() == 2);

        pool.destroy(first);
        CHECK(counted::sLive == 1);
        CHECK(pool.live_count() == 1);

        pool.destroy(static_cast<counted *>(nullptr));
        CHECK(counted::sLive == 1);

        pool.destroy(second);
        CHECK(counted::sLive == 0);
        CHECK(pool.live_count() == 0);
    }

    TEST_CASE("pool_allocator - construct of a type that does not fit returns nullptr") {
        testing::expect_assert asserts = {};
        pool_allocator pool(16, 16, 2);
        CHECK(pool.construct<big_payload>() == nullptr);
        CHECK(asserts.count() == 1);
        CHECK(pool.live_count() == 0);
    }

    TEST_CASE("pool_allocator - shrink_to_fit frees fully free slabs and keeps partially used ones") {
        pool_allocator pool(32, 16, 2);
        std::vector<void *> blocks = {};
        for (int index = 0; index < 6; ++index) {
            void *block = pool.allocate();
            REQUIRE(block != nullptr);
            std::memset(block, index, 32);
            blocks.push_back(block);
        }
        REQUIRE(pool.slab_count() == 3);

        // Slab 2 (blocks 2, 3) becomes empty; slab 3 keeps one live block.
        pool.deallocate(blocks[2]);
        pool.deallocate(blocks[3]);
        pool.deallocate(blocks[4]);
        const memory_snapshot before = snapshot();

        pool.shrink_to_fit();

        CHECK(snapshot().freeCount == before.freeCount + 1);
        CHECK(pool.slab_count() == 2);
        CHECK(pool.capacity() == 4);
        CHECK(pool.live_count() == 3);
        CHECK(pool.owns(blocks[0]));
        CHECK(pool.owns(blocks[1]));
        CHECK_FALSE(pool.owns(blocks[2]));
        CHECK_FALSE(pool.owns(blocks[3]));
        CHECK(pool.owns(blocks[5]));
        CHECK(as_bytes(blocks[5])[0] == static_cast<byte>(5));

        // The free block of the kept slab is still on the free list, so no slab is added for it.
        CHECK(pool.allocate() == blocks[4]);
        CHECK(pool.slab_count() == 2);
        void *grown = pool.allocate();
        REQUIRE(grown != nullptr);
        CHECK(pool.slab_count() == 3);
    }

    TEST_CASE("pool_allocator - shrink_to_fit without an empty slab changes nothing") {
        pool_allocator pool(32, 16, 2);
        void *block = pool.allocate();
        REQUIRE(block != nullptr);
        const memory_snapshot before = snapshot();

        pool.shrink_to_fit();

        CHECK(snapshot() == before);
        CHECK(pool.slab_count() == 1);
        CHECK(pool.owns(block));
    }

    TEST_CASE("pool_allocator - shrink_to_fit on a pool with no live block releases everything") {
        pool_allocator pool(32, 16, 2);
        std::vector<void *> blocks = {};
        for (int index = 0; index < 5; ++index) {
            blocks.push_back(pool.allocate());
            REQUIRE(blocks.back() != nullptr);
        }
        for (void *block : blocks) {
            pool.deallocate(block);
        }
        const memory_snapshot before = snapshot();

        pool.shrink_to_fit();

        CHECK(snapshot().freeCount == before.freeCount + 3);
        CHECK(pool.slab_count() == 0);
        CHECK(pool.capacity() == 0);
        CHECK(pool.live_count() == 0);

        void *fresh = pool.allocate();
        REQUIRE(fresh != nullptr);
        CHECK(pool.slab_count() == 1);
        CHECK(pool.owns(fresh));
    }

    TEST_CASE("pool_allocator - owns recognises block starts in every slab, live or free") {
        pool_allocator pool(32, 32, 2);
        void *first = pool.allocate();
        void *second = pool.allocate();
        void *third = pool.allocate();
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(third != nullptr);
        pool.deallocate(second);

        CHECK(pool.owns(first));
        CHECK(pool.owns(second));
        CHECK(pool.owns(third));
        CHECK(pool.owns(as_bytes(third) + 32));
        CHECK_FALSE(pool.owns(as_bytes(third) + 16));
        CHECK_FALSE(pool.owns(as_bytes(first) + 8));
        CHECK_FALSE(pool.owns(nullptr));
    }

    TEST_CASE("pool_allocator - blocks can be written in full without corrupting their neighbours") {
        pool_allocator pool(64, 64, 8);
        std::vector<void *> blocks = {};
        for (int index = 0; index < 8; ++index) {
            void *block = pool.allocate();
            REQUIRE(block != nullptr);
            std::memset(block, index + 1, 64);
            blocks.push_back(block);
        }
        CHECK(pool.slab_count() == 1);

        for (int index = 0; index < 8; ++index) {
            const byte *data = as_bytes(blocks[static_cast<usize>(index)]);
            bool intact = true;
            for (usize offset = 0; offset < 64; ++offset) {
                intact = intact && data[offset] == static_cast<byte>(index + 1);
            }
            CHECK(intact);
        }
    }

    TEST_CASE("pool_allocator - the destructor frees every slab and the slab table even with live blocks") {
        const memory_snapshot before = snapshot();
        {
            pool_allocator pool(32, 16, 2);
            for (int index = 0; index < 5; ++index) {
                REQUIRE(pool.allocate() != nullptr);
            }
            CHECK(snapshot().allocationCount == before.allocationCount + 3 + 1);
        }
        CHECK(snapshot().freeCount == before.freeCount + 3 + 1);
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("pool_allocator - a pool that never allocates touches the heap neither in construction nor destruction") {
        const memory_snapshot before = snapshot();
        {
            pool_allocator pool(32, 16, 2);
            CHECK(pool.slab_count() == 0);
        }
        CHECK(snapshot() == before);
    }

    TEST_CASE("pool_allocator - move construction and assignment transfer slabs and the free list") {
        pool_allocator source(32, 16, 2);
        void *first = source.allocate();
        void *second = source.allocate();
        void *third = source.allocate();
        REQUIRE(third != nullptr);
        source.deallocate(second);

        pool_allocator moved(std::move(source));
        CHECK(moved.slab_count() == 2);
        CHECK(moved.live_count() == 2);
        CHECK(moved.block_size() == 32);
        CHECK(moved.owns(first));
        CHECK(source.slab_count() == 0);
        CHECK(source.live_count() == 0);
        CHECK_FALSE(source.owns(first));
        CHECK(moved.allocate() == second);

        pool_allocator assigned(64, 64, 4);
        REQUIRE(assigned.allocate() != nullptr);
        const memory_snapshot before = snapshot();
        assigned = std::move(moved);
        // The target released its one slab and its slab table.
        CHECK(snapshot().freeCount == before.freeCount + 2);
        CHECK(assigned.slab_count() == 2);
        CHECK(assigned.live_count() == 3);
        CHECK(assigned.block_size() == 32);
        CHECK(assigned.alignment() == 16);
        CHECK(assigned.owns(third));
        CHECK(moved.slab_count() == 0);

        // Self-assignment through an alias (a literal self-move is a compiler warning) is a no-op.
        pool_allocator &alias = assigned;
        assigned = std::move(alias);
        CHECK(assigned.slab_count() == 2);
        CHECK(assigned.live_count() == 3);
    }

    TEST_CASE("pool_allocator - a moved-from pool is empty but still usable") {
        pool_allocator source(32, 16, 2);
        void *original = source.allocate();
        REQUIRE(original != nullptr);
        pool_allocator moved(std::move(source));
        CHECK(source.slab_count() == 0);
        CHECK(source.capacity() == 0);

        void *fresh = source.allocate();
        REQUIRE(fresh != nullptr);
        CHECK(source.slab_count() == 1);
        CHECK(source.live_count() == 1);
        CHECK(source.block_size() == 32);
        CHECK(source.blocks_per_slab() == 2);
        CHECK(source.owns(fresh));
        CHECK_FALSE(moved.owns(fresh));
        CHECK_FALSE(source.owns(original));
        std::memset(fresh, 0x5A, 32);
        source.deallocate(fresh);

        pool_allocator target(64, 64, 4);
        target = std::move(moved);
        CHECK(moved.slab_count() == 0);
        void *again = moved.allocate();
        REQUIRE(again != nullptr);
        CHECK(moved.owns(again));
        CHECK(moved.live_count() == 1);
        moved.deallocate(again);
        moved.shrink_to_fit();
        CHECK(moved.slab_count() == 0);
    }

    TEST_CASE("pool_allocator - 16 KiB blocks at 64-byte alignment as the ECS chunk pool uses them") {
        pool_allocator chunks(16 * KIB, 64, 4);
        std::vector<void *> blocks = {};
        blocks.reserve(9);
        const memory_snapshot before = snapshot();

        for (int index = 0; index < 9; ++index) {
            void *block = chunks.allocate();
            REQUIRE(block != nullptr);
            CHECK(is_aligned(block, 64));
            std::memset(block, 0xEE, 16 * KIB);
            blocks.push_back(block);
        }

        CHECK(chunks.slab_count() == 3);
        // Three slabs plus the slab table; each slab carries its free bitmap after the blocks.
        CHECK(snapshot().allocationCount == before.allocationCount + 3 + 1);
        CHECK(snapshot().allocatedBytes >= before.allocatedBytes + 3 * 4 * 16 * KIB);
        CHECK(snapshot().allocatedBytes < before.allocatedBytes + 3 * 4 * 16 * KIB + 1 * KIB);
        for (void *block : blocks) {
            chunks.deallocate(block);
        }
        CHECK(chunks.live_count() == 0);
    }
}
