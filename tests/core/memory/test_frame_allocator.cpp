#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/memory/frame_allocator.h"
#include "conjure/core/memory/tracked_allocation.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <random>
#include <thread>
#include <type_traits>
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

    struct range {
        const byte *begin = nullptr;
        const byte *end = nullptr;
    };

    // Trivially destructible but not copyable: allocate_span only needs the former.
    struct unique_handle {
        u32 id = 0;

        unique_handle() = default;
        unique_handle(const unique_handle &) = delete;
        unique_handle &operator=(const unique_handle &) = delete;
    };
    static_assert(std::is_trivially_destructible_v<unique_handle>);
    static_assert(!std::is_trivially_copyable_v<unique_handle>);

    constexpr usize USIZE_MAX = std::numeric_limits<usize>::max();

    const byte *as_bytes(const void *pointer) {
        return static_cast<const byte *>(pointer);
    }

    void wait_for(const std::atomic<bool> &flag) {
        while (!flag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }
}

TEST_SUITE("core/frame_allocator") {

    TEST_CASE("frame_allocator - construction rounds the block to cache lines and the arena to whole blocks") {
        const memory_snapshot before = snapshot();
        frame_allocator frame(1000, 100);
        CHECK(frame.thread_block_bytes() == 128);
        CHECK(frame.capacity_bytes() == 1024);
        CHECK(frame.used_bytes() == 0);
        CHECK(frame.overflow_count() == 0);
        CHECK(frame.overflow_bytes() == 0);
        CHECK(frame.high_water_bytes() == 0);
        CHECK(frame.epoch() != 0);
        CHECK(snapshot().allocationCount == before.allocationCount + 1);

        frame_allocator exact(4 * KIB, 1 * KIB);
        CHECK(exact.capacity_bytes() == 4 * KIB);
        CHECK(exact.thread_block_bytes() == 1 * KIB);

        frame_allocator minimal(1, 1);
        CHECK(minimal.thread_block_bytes() == CACHE_LINE_SIZE);
        CHECK(minimal.capacity_bytes() == CACHE_LINE_SIZE);
    }

    TEST_CASE("frame_allocator - absurd constructor arguments degrade to a heap-only allocator") {
        // Regression: a block size near the usize maximum rounded to 0 and divided by zero; an arena
        // size near the maximum wrapped the capacity to 0 while a 64-byte arena was allocated, and
        // the block claim then ran past its end.
        testing::expect_assert asserts = {};
        const memory_snapshot before = snapshot();

        frame_allocator hugeBlock(1024, USIZE_MAX - 10);
        CHECK(hugeBlock.capacity_bytes() == 0);
        CHECK(hugeBlock.thread_block_bytes() % CACHE_LINE_SIZE == 0);
        CHECK(hugeBlock.thread_block_bytes() > 1024);

        frame_allocator hugeArena(USIZE_MAX, 64);
        CHECK(hugeArena.capacity_bytes() == 0);
        CHECK(hugeArena.thread_block_bytes() == 64);

        frame_allocator hugeBoth(USIZE_MAX, USIZE_MAX);
        CHECK(hugeBoth.capacity_bytes() == 0);

        frame_allocator tooLargeForTheHeap(MAX_ALLOCATION_BYTES, 64);
        CHECK(tooLargeForTheHeap.capacity_bytes() == 0);

        CHECK(asserts.count() == 0);
        CHECK(snapshot() == before);

        // Heap-only allocators keep serving: every request overflows, nothing is claimed.
        for (int round = 0; round < 3; ++round) {
            void *block = hugeArena.allocate(100);
            REQUIRE(block != nullptr);
            std::memset(block, 0x11, 100);
            void *other = hugeBlock.allocate(100);
            REQUIRE(other != nullptr);
            std::memset(other, 0x22, 100);
        }
        CHECK(hugeArena.overflow_count() == 3);
        CHECK(hugeArena.used_bytes() == 0);
        CHECK(hugeBlock.overflow_count() == 3);
        CHECK(hugeBlock.used_bytes() == 0);
        CHECK(snapshot().allocationCount == before.allocationCount + 6);

        hugeArena.reset();
        hugeBlock.reset();
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("frame_allocator - a zero-byte arena serves everything from the heap") {
        const memory_snapshot before = snapshot();
        frame_allocator frame(0, 1 * KIB);
        CHECK(frame.capacity_bytes() == 0);
        CHECK(snapshot().allocationCount == before.allocationCount);

        void *block = frame.allocate(32);
        REQUIRE(block != nullptr);
        CHECK(is_aligned(block, DEFAULT_ALIGNMENT));
        std::memset(block, 0xAA, 32);
        CHECK(frame.overflow_count() == 1);
        CHECK(frame.overflow_bytes() == 32);
        CHECK(frame.used_bytes() == 0);
        CHECK(snapshot().allocationCount == before.allocationCount + 1);

        frame.reset();
        CHECK(frame.overflow_count() == 0);
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("frame_allocator - allocations bump within the calling thread's block") {
        frame_allocator frame(64 * KIB, 4 * KIB);
        void *first = frame.allocate(16);
        void *second = frame.allocate(16);
        void *third = frame.allocate(8, 8);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(third != nullptr);

        CHECK(as_bytes(second) == as_bytes(first) + 16);
        CHECK(as_bytes(third) == as_bytes(second) + 16);
        CHECK(frame.used_bytes() == 4 * KIB);
        CHECK(frame.overflow_count() == 0);
        CHECK(is_aligned(first, CACHE_LINE_SIZE));
    }

    TEST_CASE("frame_allocator - alignment is honoured up to 4096 inside blocks and in overflow") {
        frame_allocator frame(64 * KIB, 8 * KIB);
        for (usize alignment = 1; alignment <= 4096; alignment *= 2) {
            REQUIRE(frame.allocate(1, 1) != nullptr);
            void *block = frame.allocate(alignment, alignment);
            REQUIRE(block != nullptr);
            CHECK(is_aligned(block, alignment));
            std::memset(block, 0xBB, alignment);
        }
        CHECK(frame.overflow_count() == 0);

        frame_allocator heapOnly(0, 64);
        for (usize alignment = 1; alignment <= 4096; alignment *= 2) {
            void *block = heapOnly.allocate(alignment, alignment);
            REQUIRE(block != nullptr);
            CHECK(is_aligned(block, alignment));
            std::memset(block, 0xCC, alignment);
        }
        CHECK(heapOnly.overflow_count() == 13);
    }

    TEST_CASE("frame_allocator - a request that does not fit the remaining block claims a new block") {
        frame_allocator frame(64 * KIB, 1 * KIB);
        void *first = frame.allocate(1000);
        void *second = frame.allocate(100);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);

        CHECK(frame.used_bytes() == 2 * KIB);
        CHECK(as_bytes(second) == as_bytes(first) + 1 * KIB);
        CHECK(frame.overflow_count() == 0);

        // The new block is now the thread's block (the next request lands after the aligned 100 bytes).
        void *third = frame.allocate(100);
        REQUIRE(third != nullptr);
        CHECK(as_bytes(third) == as_bytes(second) + align_up(100, DEFAULT_ALIGNMENT));
        CHECK(frame.used_bytes() == 2 * KIB);
    }

    TEST_CASE("frame_allocator - a request larger than a block overflows to the heap") {
        frame_allocator frame(64 * KIB, 1 * KIB);
        const memory_snapshot before = snapshot();

        void *large = frame.allocate(2000, 32);
        REQUIRE(large != nullptr);
        CHECK(is_aligned(large, 32));
        std::memset(large, 0xDD, 2000);
        CHECK(frame.overflow_count() == 1);
        CHECK(frame.overflow_bytes() == 2000);
        CHECK(frame.used_bytes() == 0);
        CHECK(snapshot().allocationCount == before.allocationCount + 1);

        // An over-aligned request that would not fit a fresh block after padding overflows too.
        void *overAligned = frame.allocate(1 * KIB, 4096);
        REQUIRE(overAligned != nullptr);
        CHECK(is_aligned(overAligned, 4096));
        CHECK(frame.overflow_count() == 2);
        CHECK(frame.used_bytes() == 0);
    }

    TEST_CASE("frame_allocator - arena exhaustion overflows to the heap") {
        frame_allocator frame(2 * KIB, 1 * KIB);
        REQUIRE(frame.allocate(1000) != nullptr);
        REQUIRE(frame.allocate(1000) != nullptr);
        CHECK(frame.used_bytes() == 2 * KIB);
        CHECK(frame.overflow_count() == 0);

        void *spilled = frame.allocate(1000);
        REQUIRE(spilled != nullptr);
        std::memset(spilled, 0xEE, 1000);
        CHECK(frame.overflow_count() == 1);
        CHECK(frame.overflow_bytes() == 1000);
        CHECK(frame.used_bytes() == 2 * KIB);

        // Small requests still fit the thread's current block even though the arena is exhausted.
        void *small = frame.allocate(8);
        REQUIRE(small != nullptr);
        CHECK(frame.overflow_count() == 1);

        // Failed claims never move the offset past the capacity: used_bytes() stays exact and the
        // high-water mark does not drift however often the exhausted arena is asked again.
        for (int index = 0; index < 100; ++index) {
            REQUIRE(frame.allocate(1000) != nullptr);
        }
        CHECK(frame.used_bytes() == 2 * KIB);
        CHECK(frame.capacity_bytes() == 2 * KIB);
        CHECK(frame.overflow_count() == 101);
        CHECK(frame.high_water_bytes() == 2 * KIB + 101 * 1000);

        frame.reset();
        CHECK(frame.used_bytes() == 0);
        void *afterReset = frame.allocate(16);
        REQUIRE(afterReset != nullptr);
        CHECK(frame.used_bytes() == 1 * KIB);
        CHECK(frame.overflow_count() == 0);
    }

    TEST_CASE("frame_allocator - absurd sizes return nullptr and leave the allocator and the counters untouched") {
        testing::expect_assert asserts = {};
        frame_allocator frame(4 * KIB, 1 * KIB);
        void *first = frame.allocate(16);
        REQUIRE(first != nullptr);
        const memory_snapshot before = snapshot();

        CHECK(frame.allocate(USIZE_MAX) == nullptr);
        CHECK(frame.allocate(USIZE_MAX - 64) == nullptr);
        CHECK(frame.allocate(USIZE_MAX - 64, 64) == nullptr);
        CHECK(frame.allocate(MAX_ALLOCATION_BYTES) == nullptr);
        CHECK(frame.allocate_span<u64>(MAX_ALLOCATION_BYTES / sizeof(u64)).empty());

        CHECK(asserts.count() == 0);
        CHECK(snapshot() == before);
        CHECK(frame.overflow_count() == 0);
        CHECK(frame.overflow_bytes() == 0);
        CHECK(frame.used_bytes() == 1 * KIB);

        void *second = frame.allocate(16);
        REQUIRE(second != nullptr);
        CHECK(as_bytes(second) == as_bytes(first) + 16);
    }

    TEST_CASE("frame_allocator - reset frees overflow blocks and recycles the arena") {
        frame_allocator frame(1 * KIB, 1 * KIB);
        void *first = frame.allocate(16);
        REQUIRE(first != nullptr);
        REQUIRE(frame.allocate(5000) != nullptr);
        REQUIRE(frame.allocate(5000) != nullptr);
        REQUIRE(frame.overflow_count() == 2);
        const memory_snapshot before = snapshot();

        frame.reset();

        CHECK(snapshot().freeCount == before.freeCount + 2);
        CHECK(frame.overflow_count() == 0);
        CHECK(frame.overflow_bytes() == 0);
        CHECK(frame.used_bytes() == 0);

        void *reused = frame.allocate(16);
        CHECK(reused == first);
        CHECK(frame.used_bytes() == 1 * KIB);
        CHECK(snapshot().allocationCount == before.allocationCount);
    }

    TEST_CASE("frame_allocator - reset bumps the epoch and invalidates the cached block") {
        frame_allocator frame(4 * KIB, 1 * KIB);
        void *first = frame.allocate(16);
        REQUIRE(first != nullptr);
        const u64 firstEpoch = frame.epoch();

        frame.reset();
        CHECK(frame.epoch() > firstEpoch);

        // A stale cursor would hand out first + 16; a re-acquired block 0 hands out 'first' again.
        void *afterReset = frame.allocate(16);
        CHECK(afterReset == first);
        CHECK(frame.used_bytes() == 1 * KIB);
    }

    TEST_CASE("frame_allocator - epochs are unique across allocators") {
        frame_allocator first(1 * KIB, 1 * KIB);
        frame_allocator second(1 * KIB, 1 * KIB);
        CHECK(second.epoch() > first.epoch());

        first.reset();
        CHECK(first.epoch() > second.epoch());
    }

    TEST_CASE("frame_allocator - a thread switching between allocators re-acquires a block on every switch") {
        frame_allocator first(8 * KIB, 1 * KIB);
        frame_allocator second(8 * KIB, 1 * KIB);

        REQUIRE(first.allocate(16) != nullptr);
        REQUIRE(second.allocate(16) != nullptr);
        REQUIRE(first.allocate(16) != nullptr);

        CHECK(first.used_bytes() == 2 * KIB);
        CHECK(second.used_bytes() == 1 * KIB);
        CHECK(first.overflow_count() == 0);
        CHECK(second.overflow_count() == 0);
    }

    TEST_CASE("frame_allocator - high_water_bytes tracks the largest frame including overflow") {
        frame_allocator frame(2 * KIB, 1 * KIB);
        REQUIRE(frame.allocate(16) != nullptr);
        CHECK(frame.high_water_bytes() == 1 * KIB);

        frame.reset();
        CHECK(frame.high_water_bytes() == 1 * KIB);

        REQUIRE(frame.allocate(1000) != nullptr);
        REQUIRE(frame.allocate(1000) != nullptr);
        REQUIRE(frame.allocate(100) != nullptr);
        CHECK(frame.overflow_count() == 1);
        CHECK(frame.high_water_bytes() == 2 * KIB + 100);

        frame.reset();
        CHECK(frame.high_water_bytes() == 2 * KIB + 100);

        REQUIRE(frame.allocate(16) != nullptr);
        CHECK(frame.high_water_bytes() == 2 * KIB + 100);
    }

    TEST_CASE("frame_allocator - allocate_span returns typed storage and an empty span for zero or impossible counts") {
        frame_allocator frame(16 * KIB, 4 * KIB);

        std::span<u32> numbers = frame.allocate_span<u32>(100);
        REQUIRE(numbers.size() == 100);
        CHECK(is_aligned(numbers.data(), alignof(u32)));
        for (u32 index = 0; index < 100; ++index) {
            numbers[index] = index * 3;
        }
        CHECK(numbers[99] == 297);

        std::span<cache_line_value> lines = frame.allocate_span<cache_line_value>(3);
        REQUIRE(lines.size() == 3);
        CHECK(is_aligned(lines.data(), 64));

        // Only trivial destruction is required: a non-copyable handle type is fine.
        std::span<unique_handle> handles = frame.allocate_span<unique_handle>(4);
        REQUIRE(handles.size() == 4);
        for (u32 index = 0; index < 4; ++index) {
            ::new (static_cast<void *>(&handles[index])) unique_handle{};
            handles[index].id = index + 1;
        }
        CHECK(handles[3].id == 4);

        CHECK(frame.allocate_span<u32>(0).empty());
        CHECK(frame.allocate_span<u32>(std::numeric_limits<usize>::max()).empty());
        CHECK(frame.overflow_count() == 0);
    }

    TEST_CASE("frame_allocator - construct places trivially destructible objects") {
        frame_allocator frame(16 * KIB, 4 * KIB);
        REQUIRE(frame.allocate(1, 1) != nullptr);

        point *located = frame.construct<point>(5, 6);
        REQUIRE(located != nullptr);
        CHECK(located->x == 5);
        CHECK(located->y == 6);
        CHECK(is_aligned(located, alignof(point)));

        cache_line_value *aligned = frame.construct<cache_line_value>();
        REQUIRE(aligned != nullptr);
        CHECK(aligned->value == 0);
        CHECK(is_aligned(aligned, 64));
    }

    TEST_CASE("frame_allocator - zero bytes and invalid alignments return nullptr") {
        testing::expect_assert asserts = {};
        frame_allocator frame(4 * KIB, 1 * KIB);

        CHECK(frame.allocate(0) == nullptr);
        CHECK(asserts.count() == 0);
        CHECK(frame.used_bytes() == 0);

        CHECK(frame.allocate(16, 3) == nullptr);
        CHECK(frame.allocate(16, 0) == nullptr);
        CHECK(asserts.count() == 2);
        CHECK(frame.used_bytes() == 0);
        CHECK(frame.overflow_count() == 0);
    }

    TEST_CASE("frame_allocator - the destructor frees the arena and the overflow blocks") {
        const memory_snapshot before = snapshot();
        {
            frame_allocator frame(1 * KIB, 1 * KIB);
            REQUIRE(frame.allocate(16) != nullptr);
            REQUIRE(frame.allocate(3000) != nullptr);
            REQUIRE(frame.allocate(3000) != nullptr);
            CHECK(snapshot().allocationCount == before.allocationCount + 3);
        }
        CHECK(snapshot().freeCount == before.freeCount + 3);
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("frame_allocator - threads allocating concurrently each claim their own block") {
        constexpr int THREAD_COUNT = 8;
        frame_allocator frame(THREAD_COUNT * 1 * KIB, 1 * KIB);
        std::vector<void *> pointers(THREAD_COUNT, nullptr);
        std::atomic<bool> start = false;

        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([threadIndex, &frame, &pointers, &start] {
                wait_for(start);
                pointers[static_cast<usize>(threadIndex)] = frame.allocate(16);
            });
        }
        start.store(true, std::memory_order_release);
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(frame.used_bytes() == THREAD_COUNT * 1 * KIB);
        CHECK(frame.overflow_count() == 0);
        std::sort(pointers.begin(), pointers.end());
        for (int index = 0; index < THREAD_COUNT; ++index) {
            REQUIRE(pointers[static_cast<usize>(index)] != nullptr);
            if (index > 0) {
                const byte *previous = as_bytes(pointers[static_cast<usize>(index - 1)]);
                CHECK(as_bytes(pointers[static_cast<usize>(index)]) == previous + 1 * KIB);
            }
        }
    }

    TEST_CASE("frame_allocator - threads receive disjoint ranges through blocks and overflow alike") {
        constexpr int THREAD_COUNT = 8;
        constexpr int PER_THREAD = 2000;
        frame_allocator frame(1 * MIB, 16 * KIB);
        std::vector<std::vector<range>> perThread(THREAD_COUNT);
        std::vector<int> threadPassed(THREAD_COUNT, 0);   // not vector<bool>: threads write distinct elements
        std::atomic<bool> start = false;

        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([threadIndex, &frame, &perThread, &threadPassed, &start] {
                std::mt19937_64 engine(0x5EED + static_cast<u64>(threadIndex));
                std::uniform_int_distribution<usize> sizes(1, 200);
                std::vector<range> &ranges = perThread[static_cast<usize>(threadIndex)];
                ranges.reserve(PER_THREAD);
                const byte marker = static_cast<byte>(threadIndex + 1);

                wait_for(start);
                bool passed = true;
                for (int index = 0; index < PER_THREAD; ++index) {
                    const usize bytes = sizes(engine);
                    void *block = frame.allocate(bytes);
                    if (block == nullptr || !is_aligned(block, DEFAULT_ALIGNMENT)) {
                        passed = false;
                        continue;
                    }
                    std::memset(block, static_cast<int>(marker), bytes);
                    ranges.push_back({as_bytes(block), as_bytes(block) + bytes});
                }

                for (const range &entry : ranges) {
                    for (const byte *cursor = entry.begin; cursor != entry.end; ++cursor) {
                        passed = passed && *cursor == marker;
                    }
                }
                threadPassed[static_cast<usize>(threadIndex)] = passed ? 1 : 0;
            });
        }
        start.store(true, std::memory_order_release);
        for (std::thread &thread : threads) {
            thread.join();
        }

        std::vector<range> all = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            CHECK(threadPassed[static_cast<usize>(threadIndex)] == 1);
            const std::vector<range> &ranges = perThread[static_cast<usize>(threadIndex)];
            CHECK(ranges.size() == PER_THREAD);
            all.insert(all.end(), ranges.begin(), ranges.end());
        }

        std::sort(all.begin(), all.end(), [](const range &left, const range &right) {
            return left.begin < right.begin;
        });
        bool overlap = false;
        for (usize index = 1; index < all.size(); ++index) {
            overlap = overlap || all[index].begin < all[index - 1].end;
        }
        CHECK_FALSE(overlap);

        // Eight threads of ~100-byte average requests exceed 1 MiB, so both paths ran.
        CHECK(frame.used_bytes() == frame.capacity_bytes());
        CHECK(frame.overflow_count() > 0);
        CHECK(frame.high_water_bytes() == frame.capacity_bytes() + frame.overflow_bytes());

        const u32 overflowBlocks = frame.overflow_count();
        const memory_snapshot before = snapshot();
        frame.reset();
        CHECK(snapshot().freeCount == before.freeCount + overflowBlocks);
        CHECK(frame.overflow_count() == 0);
        CHECK(frame.used_bytes() == 0);
    }

    TEST_CASE("frame_allocator - reset invalidates the block cached by another thread") {
        frame_allocator frame(4 * KIB, 1 * KIB);
        std::atomic<bool> firstDone = false;
        std::atomic<bool> resetDone = false;
        void *first = nullptr;
        void *second = nullptr;

        std::thread worker([&] {
            first = frame.allocate(16);
            firstDone.store(true, std::memory_order_release);
            wait_for(resetDone);
            second = frame.allocate(16);
        });

        wait_for(firstDone);
        CHECK(frame.used_bytes() == 1 * KIB);
        frame.reset();
        resetDone.store(true, std::memory_order_release);
        worker.join();

        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        // A stale cursor would have produced first + 16; the epoch bump forces a fresh block, which
        // after reset is block 0 again.
        CHECK(second == first);
        CHECK(frame.used_bytes() == 1 * KIB);
    }

    TEST_CASE("frame_allocator - a block cached by a finished thread is never handed to the main thread") {
        frame_allocator frame(4 * KIB, 1 * KIB);
        void *fromWorker = nullptr;
        std::thread worker([&] {
            fromWorker = frame.allocate(16);
        });
        worker.join();
        REQUIRE(fromWorker != nullptr);

        void *fromMain = frame.allocate(16);
        REQUIRE(fromMain != nullptr);
        CHECK(fromMain != fromWorker);
        CHECK(as_bytes(fromMain) == as_bytes(fromWorker) + 1 * KIB);
        CHECK(frame.used_bytes() == 2 * KIB);
    }
}
