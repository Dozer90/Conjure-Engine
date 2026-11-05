#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/memory/arena.h"
#include "conjure/core/memory/tracked_allocation.h"

#include <cstring>
#include <limits>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core::memory;

namespace {
    struct point {
        int x = 0;
        int y = 0;

        point(int xValue, int yValue) :
            x(xValue),
            y(yValue) {}
    };

    // Fills its alignment exactly so MSVC does not warn about padding (C4324).
    struct alignas(64) cache_line_value {
        u32 value = 0;
        u32 padding[15] = {};
    };

    const byte *as_bytes(const void *pointer) {
        return static_cast<const byte *>(pointer);
    }
}

TEST_SUITE("core/arena") {

    TEST_CASE("arena - a new arena holds no blocks until the first allocation") {
        arena scratch{};
        CHECK(scratch.block_bytes() == DEFAULT_ARENA_BLOCK_BYTES);
        CHECK(scratch.block_count() == 0);
        CHECK(scratch.bytes_reserved() == 0);
        CHECK(scratch.bytes_allocated() == 0);

        const memory_snapshot before = snapshot();
        void *first = scratch.allocate(8);
        REQUIRE(first != nullptr);
        CHECK(scratch.block_count() == 1);
        CHECK(scratch.bytes_reserved() == DEFAULT_ARENA_BLOCK_BYTES);
        CHECK(scratch.bytes_allocated() == 8);
        CHECK(snapshot().allocationCount == before.allocationCount + 1);
    }

    TEST_CASE("arena - a zero block size asserts and falls back to the default") {
        testing::expect_assert asserts = {};
        arena scratch(0);
        CHECK(asserts.count() == 1);
        CHECK(scratch.block_bytes() == DEFAULT_ARENA_BLOCK_BYTES);
        CHECK(scratch.allocate(1) != nullptr);
    }

    TEST_CASE("arena - allocations bump through the current block") {
        arena scratch(1 * KIB);
        void *first = scratch.allocate(16);
        void *second = scratch.allocate(16);
        void *third = scratch.allocate(8, 8);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(third != nullptr);

        CHECK(as_bytes(second) == as_bytes(first) + 16);
        CHECK(as_bytes(third) == as_bytes(second) + 16);
        CHECK(scratch.block_count() == 1);
        CHECK(scratch.bytes_allocated() == 40);
    }

    TEST_CASE("arena - allocations honour alignments up to 256 and beyond") {
        arena scratch(16 * KIB);
        for (usize alignment = 1; alignment <= 4096; alignment *= 2) {
            // Misalign the cursor first so the padding path runs for every alignment.
            REQUIRE(scratch.allocate(1, 1) != nullptr);
            void *block = scratch.allocate(alignment, alignment);
            REQUIRE(block != nullptr);
            CHECK(is_aligned(block, alignment));
            CHECK(scratch.owns(block));
            std::memset(block, 0xAB, alignment);
        }
    }

    TEST_CASE("arena - a request larger than a block gets its own block and the current block keeps serving") {
        arena scratch(256);
        void *small = scratch.allocate(16);
        void *large = scratch.allocate(1000);
        void *next = scratch.allocate(16);
        REQUIRE(small != nullptr);
        REQUIRE(large != nullptr);
        REQUIRE(next != nullptr);

        CHECK(as_bytes(next) == as_bytes(small) + 16);
        CHECK(scratch.block_count() == 2);
        CHECK(scratch.bytes_reserved() == 256 + 1000);
        CHECK(scratch.bytes_allocated() == 1032);
        CHECK(scratch.owns(large));
        std::memset(large, 0xCD, 1000);
    }

    TEST_CASE("arena - a request that exactly fills a block fits in one block") {
        arena scratch(256);
        void *full = scratch.allocate(256, DEFAULT_ALIGNMENT);
        REQUIRE(full != nullptr);
        CHECK(scratch.block_count() == 1);
        CHECK(scratch.bytes_reserved() == 256);

        void *overflow = scratch.allocate(1);
        REQUIRE(overflow != nullptr);
        CHECK(scratch.block_count() == 2);
    }

    TEST_CASE("arena - an over-aligned request near the block size gets a dedicated block") {
        arena scratch(256);
        void *block = scratch.allocate(256, 128);
        REQUIRE(block != nullptr);
        CHECK(is_aligned(block, 128));
        CHECK(scratch.block_count() == 1);
        CHECK(scratch.bytes_reserved() == 256 + 128 - DEFAULT_ALIGNMENT);
    }

    TEST_CASE("arena - when the current block is full a new regular block becomes current") {
        arena scratch(64);
        void *first = scratch.allocate(48);
        void *second = scratch.allocate(32);
        void *third = scratch.allocate(16);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(third != nullptr);

        CHECK(scratch.block_count() == 2);
        CHECK(as_bytes(third) == as_bytes(second) + 32);
        CHECK(scratch.bytes_reserved() == 128);
    }

    TEST_CASE("arena - reset keeps the first block, frees the rest and reuses the first address") {
        arena scratch(64);
        void *first = scratch.allocate(64);
        REQUIRE(scratch.allocate(64) != nullptr);
        REQUIRE(scratch.allocate(500) != nullptr);
        REQUIRE(scratch.block_count() == 3);
        const memory_snapshot before = snapshot();

        scratch.reset();

        CHECK(snapshot().freeCount == before.freeCount + 2);
        CHECK(scratch.block_count() == 1);
        CHECK(scratch.bytes_reserved() == 64);
        CHECK(scratch.bytes_allocated() == 0);
        CHECK_FALSE(scratch.owns(first));

        void *reused = scratch.allocate(16);
        CHECK(reused == first);
        CHECK(snapshot().allocationCount == before.allocationCount);
    }

    TEST_CASE("arena - reset on an empty arena is harmless") {
        arena scratch(64);
        scratch.reset();
        CHECK(scratch.block_count() == 0);
        CHECK(scratch.allocate(8) != nullptr);
    }

    TEST_CASE("arena - release returns every block and the arena stays usable") {
        arena scratch(64);
        REQUIRE(scratch.allocate(64) != nullptr);
        REQUIRE(scratch.allocate(64) != nullptr);
        const memory_snapshot before = snapshot();

        scratch.release();

        CHECK(snapshot().freeCount == before.freeCount + 2);
        CHECK(scratch.block_count() == 0);
        CHECK(scratch.bytes_reserved() == 0);
        CHECK(scratch.bytes_allocated() == 0);

        void *fresh = scratch.allocate(8);
        REQUIRE(fresh != nullptr);
        CHECK(scratch.block_count() == 1);
        CHECK(snapshot().allocationCount == before.allocationCount + 1);
    }

    TEST_CASE("arena - construct places trivially destructible objects with their alignment") {
        arena scratch{};
        REQUIRE(scratch.allocate(1, 1) != nullptr);

        point *located = scratch.construct<point>(3, 4);
        REQUIRE(located != nullptr);
        CHECK(located->x == 3);
        CHECK(located->y == 4);
        CHECK(is_aligned(located, alignof(point)));

        cache_line_value *aligned = scratch.construct<cache_line_value>();
        REQUIRE(aligned != nullptr);
        CHECK(aligned->value == 0);
        CHECK(is_aligned(aligned, 64));
        CHECK(scratch.owns(aligned));
    }

    TEST_CASE("arena - zero bytes and invalid alignments return nullptr") {
        testing::expect_assert asserts = {};
        arena scratch{};

        CHECK(scratch.allocate(0) == nullptr);
        CHECK(asserts.count() == 0);
        CHECK(scratch.block_count() == 0);

        CHECK(scratch.allocate(16, 3) == nullptr);
        CHECK(scratch.allocate(16, 0) == nullptr);
        CHECK(asserts.count() == 2);
        CHECK(scratch.block_count() == 0);
        CHECK(scratch.bytes_allocated() == 0);
    }

    TEST_CASE("arena - absurd sizes return nullptr and leave the arena and the counters untouched") {
        // Regression: a request just under the usize maximum passed the arena's own guards, the
        // rounded block size wrapped inside tracked_allocate and the block header was written into
        // a zero-byte heap block.
        testing::expect_assert asserts = {};
        constexpr usize USIZE_MAX = std::numeric_limits<usize>::max();
        arena scratch(256);
        void *first = scratch.allocate(16);
        REQUIRE(first != nullptr);
        const memory_snapshot before = snapshot();

        CHECK(scratch.allocate(USIZE_MAX) == nullptr);
        CHECK(scratch.allocate(USIZE_MAX - 27) == nullptr);
        CHECK(scratch.allocate(USIZE_MAX - 64, 64) == nullptr);
        CHECK(scratch.allocate(MAX_ALLOCATION_BYTES) == nullptr);

        CHECK(asserts.count() == 0);
        CHECK(snapshot() == before);
        CHECK(scratch.block_count() == 1);
        CHECK(scratch.bytes_reserved() == 256);
        CHECK(scratch.bytes_allocated() == 16);

        // The current block still serves regular requests afterwards.
        void *second = scratch.allocate(16);
        REQUIRE(second != nullptr);
        CHECK(as_bytes(second) == as_bytes(first) + 16);
    }

    TEST_CASE("arena - bytes_allocated counts requested bytes without padding") {
        arena scratch{};
        REQUIRE(scratch.allocate(1, 1) != nullptr);
        REQUIRE(scratch.allocate(1, 64) != nullptr);
        CHECK(scratch.bytes_allocated() == 2);
    }

    TEST_CASE("arena - owns reports only addresses inside handed-out ranges") {
        arena scratch(128);
        void *block = scratch.allocate(32);
        REQUIRE(block != nullptr);

        CHECK(scratch.owns(block));
        CHECK(scratch.owns(as_bytes(block) + 31));
        CHECK_FALSE(scratch.owns(as_bytes(block) + 32));
        CHECK_FALSE(scratch.owns(nullptr));

        int local = 0;
        CHECK_FALSE(scratch.owns(&local));
    }

    TEST_CASE("arena - every block acquisition is reported to the counters") {
        arena scratch(128);
        const memory_snapshot before = snapshot();
        for (int index = 0; index < 10; ++index) {
            REQUIRE(scratch.allocate(128) != nullptr);
        }
        CHECK(scratch.block_count() == 10);
        CHECK(snapshot().allocationCount == before.allocationCount + 10);
        CHECK(snapshot().allocatedBytes >= before.allocatedBytes + 10 * 128);
    }

    TEST_CASE("arena - move construction and assignment transfer the blocks") {
        arena source(128);
        void *block = source.allocate(32);
        REQUIRE(block != nullptr);

        arena moved(std::move(source));
        CHECK(moved.block_count() == 1);
        CHECK(moved.bytes_allocated() == 32);
        CHECK(moved.block_bytes() == 128);
        CHECK(moved.owns(block));
        CHECK(source.block_count() == 0);
        CHECK(source.bytes_allocated() == 0);
        CHECK_FALSE(source.owns(block));

        arena assigned(64);
        REQUIRE(assigned.allocate(8) != nullptr);
        const memory_snapshot before = snapshot();
        assigned = std::move(moved);
        CHECK(snapshot().freeCount == before.freeCount + 1);
        CHECK(assigned.block_count() == 1);
        CHECK(assigned.block_bytes() == 128);
        CHECK(assigned.owns(block));
        CHECK(moved.block_count() == 0);

        // Self-assignment through an alias (a literal self-move is a compiler warning) is a no-op.
        arena &alias = assigned;
        assigned = std::move(alias);
        CHECK(assigned.block_count() == 1);
        CHECK(assigned.owns(block));
    }

    TEST_CASE("arena - a moved-from arena is empty but still usable") {
        arena source(128);
        REQUIRE(source.allocate(32) != nullptr);
        arena moved(std::move(source));
        CHECK(source.block_count() == 0);

        void *fresh = source.allocate(16);
        REQUIRE(fresh != nullptr);
        CHECK(source.block_count() == 1);
        CHECK(source.block_bytes() == 128);
        CHECK(source.owns(fresh));
        CHECK_FALSE(moved.owns(fresh));
        std::memset(fresh, 0x5A, 16);

        arena target(64);
        target = std::move(moved);
        CHECK(moved.block_count() == 0);
        void *again = moved.allocate(8);
        REQUIRE(again != nullptr);
        CHECK(moved.owns(again));
        CHECK(moved.bytes_allocated() == 8);
    }

    TEST_CASE("arena - the destructor frees every block") {
        const memory_snapshot before = snapshot();
        {
            arena scratch(64);
            REQUIRE(scratch.allocate(64) != nullptr);
            REQUIRE(scratch.allocate(64) != nullptr);
            REQUIRE(scratch.allocate(300) != nullptr);
            CHECK(snapshot().allocationCount == before.allocationCount + 3);
        }
        CHECK(snapshot().freeCount == before.freeCount + 3);
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("arena - many small allocations spill across blocks and stay distinct") {
        arena scratch(1 * KIB);
        std::vector<u64 *> values = {};
        for (u64 index = 0; index < 1000; ++index) {
            u64 *value = scratch.construct<u64>(index);
            REQUIRE(value != nullptr);
            values.push_back(value);
        }

        CHECK(scratch.block_count() == 8);
        CHECK(scratch.bytes_allocated() == 8000);
        for (u64 index = 0; index < 1000; ++index) {
            CHECK(*values[static_cast<usize>(index)] == index);
        }
    }
}
