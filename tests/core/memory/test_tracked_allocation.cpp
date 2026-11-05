#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/memory/tracked_allocation.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core::memory;

namespace {
    void fill_pattern(void *block, usize bytes, u8 seed) {
        byte *data = static_cast<byte *>(block);
        for (usize index = 0; index < bytes; ++index) {
            data[index] = static_cast<byte>(static_cast<u8>(seed + index));
        }
    }

    bool check_pattern(const void *block, usize bytes, u8 seed) {
        const byte *data = static_cast<const byte *>(block);
        for (usize index = 0; index < bytes; ++index) {
            if (data[index] != static_cast<byte>(static_cast<u8>(seed + index))) {
                return false;
            }
        }
        return true;
    }

    void wait_for(const std::atomic<bool> &flag) {
        while (!flag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    void wait_for_count(const std::atomic<int> &counter, int target) {
        while (counter.load(std::memory_order_acquire) < target) {
            std::this_thread::yield();
        }
    }

    constexpr usize USIZE_MAX = std::numeric_limits<usize>::max();
}

TEST_SUITE("core/tracked_allocation") {

    TEST_CASE("tracked_allocation - is_power_of_two accepts exactly the powers of two") {
        static_assert(is_power_of_two(1));
        static_assert(is_power_of_two(2));
        static_assert(is_power_of_two(1024));
        static_assert(!is_power_of_two(0));
        static_assert(!is_power_of_two(3));
        static_assert(!is_power_of_two(96));
        CHECK(is_power_of_two(usize{1} << 63));
        CHECK_FALSE(is_power_of_two(std::numeric_limits<usize>::max()));
    }

    TEST_CASE("tracked_allocation - align_up rounds to the next multiple and keeps aligned values") {
        static_assert(align_up(0, 16) == 0);
        static_assert(align_up(1, 16) == 16);
        static_assert(align_up(16, 16) == 16);
        static_assert(align_up(17, 16) == 32);
        static_assert(align_up(123, 1) == 123);
        static_assert(align_up(4095, 4096) == 4096);
        CHECK(align_up(65, 64) == 128);
    }

    TEST_CASE("tracked_allocation - is_aligned checks the address and treats nullptr as aligned") {
        CHECK(is_aligned(nullptr, 4096));

        alignas(64) byte buffer[128] = {};
        CHECK(is_aligned(buffer, 64));
        CHECK(is_aligned(buffer + 16, 16));
        CHECK_FALSE(is_aligned(buffer + 1, 2));
        CHECK_FALSE(is_aligned(buffer + 8, 16));
    }

    TEST_CASE("tracked_allocation - allocate returns writable memory and free releases it") {
        const memory_snapshot before = snapshot();

        void *block = tracked_allocate(100);
        REQUIRE(block != nullptr);
        CHECK(is_aligned(block, DEFAULT_ALIGNMENT));
        fill_pattern(block, 100, 7);
        CHECK(check_pattern(block, 100, 7));

        memory_snapshot during = snapshot();
        CHECK(during.allocationCount == before.allocationCount + 1);
        CHECK(during.allocatedBytes == before.allocatedBytes + 100);
        CHECK(during.freeCount == before.freeCount);

        tracked_free(block, 100);
        const memory_snapshot after = snapshot();
        CHECK(after.freeCount == before.freeCount + 1);
        CHECK(after.live_count() == before.live_count());
    }

    TEST_CASE("tracked_allocation - allocated bytes count the requested size, not the rounded one") {
        const memory_snapshot before = snapshot();
        void *block = tracked_allocate(1, 4096);
        REQUIRE(block != nullptr);
        CHECK(snapshot().allocatedBytes == before.allocatedBytes + 1);
        tracked_free(block, 1);
    }

    TEST_CASE("tracked_allocation - alignment is honoured for every power of two up to 4096") {
        for (usize alignment = 1; alignment <= 4096; alignment *= 2) {
            void *block = tracked_allocate(alignment + 3, alignment);
            REQUIRE(block != nullptr);
            CHECK(is_aligned(block, alignment));
            fill_pattern(block, alignment + 3, static_cast<u8>(alignment));
            CHECK(check_pattern(block, alignment + 3, static_cast<u8>(alignment)));
            tracked_free(block, alignment + 3);
        }
    }

    TEST_CASE("tracked_allocation - alignments below the default are raised to it") {
        void *block = tracked_allocate(24, 1);
        REQUIRE(block != nullptr);
        CHECK(is_aligned(block, DEFAULT_ALIGNMENT));
        tracked_free(block, 24);
    }

    TEST_CASE("tracked_allocation - zero-byte requests return unique non-null pointers") {
        const memory_snapshot before = snapshot();
        void *first = tracked_allocate(0);
        void *second = tracked_allocate(0);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(first != second);
        CHECK(snapshot().allocationCount == before.allocationCount + 2);
        CHECK(snapshot().allocatedBytes == before.allocatedBytes);

        tracked_free(first, 0);
        tracked_free(second, 0);
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("tracked_allocation - free of nullptr is ignored and not counted") {
        const memory_snapshot before = snapshot();
        tracked_free(nullptr, 0);
        tracked_free(nullptr, 128);
        CHECK(snapshot() == before);
    }

    TEST_CASE("tracked_allocation - an alignment that is not a power of two asserts and returns nullptr") {
        testing::expect_assert asserts = {};
        const memory_snapshot before = snapshot();

        CHECK(tracked_allocate(64, 0) == nullptr);
        CHECK(tracked_allocate(64, 3) == nullptr);
        CHECK(tracked_allocate(64, 48) == nullptr);

        CHECK(asserts.count() == 3);
        // The assert messages are formatted through the test executable's tracked operator new, so
        // only the live count (not the cumulative counters) is untouched by the rejected requests.
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("tracked_allocation - absurd sizes return nullptr without asserting and leave the counters untouched") {
        // Regression: rounding the size to the alignment used to wrap for requests near the usize
        // maximum, so the runtime was asked for a few bytes and a live pointer came back.
        testing::expect_assert asserts = {};
        const memory_snapshot before = snapshot();
        static_assert(MAX_ALLOCATION_BYTES < USIZE_MAX);
        static_assert(MAX_ALLOCATION_BYTES >= usize{1} << 62);

        CHECK(tracked_allocate(USIZE_MAX) == nullptr);
        CHECK(tracked_allocate(USIZE_MAX - 5, 16) == nullptr);
        CHECK(tracked_allocate(USIZE_MAX - 3, DEFAULT_ALIGNMENT) == nullptr);
        CHECK(tracked_allocate(USIZE_MAX, 4096) == nullptr);
        CHECK(tracked_allocate(MAX_ALLOCATION_BYTES + 1) == nullptr);
        CHECK(tracked_reallocate(nullptr, 0, USIZE_MAX) == nullptr);

        CHECK(asserts.count() == 0);
        CHECK(snapshot() == before);
    }

    TEST_CASE("tracked_allocation - reallocate to an absurd size fails and leaves the block intact") {
        testing::expect_assert asserts = {};
        void *block = tracked_allocate(16);
        REQUIRE(block != nullptr);
        fill_pattern(block, 16, 3);
        const memory_snapshot before = snapshot();

        CHECK(tracked_reallocate(block, 16, USIZE_MAX) == nullptr);
        CHECK(tracked_reallocate(block, 16, USIZE_MAX - 5, 16) == nullptr);
        CHECK(tracked_reallocate(block, 16, MAX_ALLOCATION_BYTES + 1) == nullptr);

        CHECK(asserts.count() == 0);
        CHECK(snapshot() == before);
        CHECK(check_pattern(block, 16, 3));
        tracked_free(block, 16);
    }

    TEST_CASE("tracked_allocation - reallocate preserves contents when growing and shrinking") {
        void *block = tracked_allocate(32);
        REQUIRE(block != nullptr);
        fill_pattern(block, 32, 1);

        void *grown = tracked_reallocate(block, 32, 4096);
        REQUIRE(grown != nullptr);
        CHECK(is_aligned(grown, DEFAULT_ALIGNMENT));
        CHECK(check_pattern(grown, 32, 1));
        fill_pattern(grown, 4096, 2);

        void *shrunk = tracked_reallocate(grown, 4096, 8);
        REQUIRE(shrunk != nullptr);
        CHECK(check_pattern(shrunk, 8, 2));

        tracked_free(shrunk, 8);
    }

    TEST_CASE("tracked_allocation - reallocate keeps a larger alignment") {
        void *block = tracked_allocate(16, 256);
        REQUIRE(block != nullptr);
        fill_pattern(block, 16, 9);

        void *grown = tracked_reallocate(block, 16, 1000, 256);
        REQUIRE(grown != nullptr);
        CHECK(is_aligned(grown, 256));
        CHECK(check_pattern(grown, 16, 9));
        tracked_free(grown, 1000);
    }

    TEST_CASE("tracked_allocation - reallocate from nullptr allocates and to zero bytes frees") {
        const memory_snapshot before = snapshot();

        void *block = tracked_reallocate(nullptr, 0, 40);
        REQUIRE(block != nullptr);
        CHECK(snapshot().allocationCount == before.allocationCount + 1);
        CHECK(snapshot().allocatedBytes == before.allocatedBytes + 40);

        CHECK(tracked_reallocate(block, 40, 0) == nullptr);
        CHECK(snapshot().freeCount == before.freeCount + 1);
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("tracked_allocation - reallocate counts one free and one allocation") {
        void *block = tracked_allocate(16);
        REQUIRE(block != nullptr);
        const memory_snapshot before = snapshot();

        void *grown = tracked_reallocate(block, 16, 64);
        REQUIRE(grown != nullptr);

        const memory_snapshot after = snapshot();
        CHECK(after.allocationCount == before.allocationCount + 1);
        CHECK(after.freeCount == before.freeCount + 1);
        CHECK(after.allocatedBytes == before.allocatedBytes + 64);
        CHECK(after.live_count() == before.live_count());

        tracked_free(grown, 64);
    }

    TEST_CASE("tracked_allocation - reallocate with an invalid alignment asserts and leaves the block intact") {
        testing::expect_assert asserts = {};
        void *block = tracked_allocate(16);
        REQUIRE(block != nullptr);
        fill_pattern(block, 16, 5);
        const memory_snapshot before = snapshot();

        CHECK(tracked_reallocate(block, 16, 64, 24) == nullptr);

        CHECK(asserts.count() == 1);
        CHECK(snapshot().live_count() == before.live_count());   // see the alignment case above
        CHECK(check_pattern(block, 16, 5));
        tracked_free(block, 16);
    }

    TEST_CASE("tracked_allocation - allocations from many threads are distinct and counted exactly") {
        constexpr int THREAD_COUNT = 8;
        constexpr int PER_THREAD = 500;
        std::vector<int> threadPassed(THREAD_COUNT, 0);   // not vector<bool>: threads write distinct elements
        std::atomic<int> readyCount = 0;
        std::atomic<int> doneCount = 0;
        std::atomic<bool> start = false;
        std::atomic<bool> finish = false;

        // Every heap acquisition that is not a tracked_allocate (thread state, the vectors) happens
        // before the 'before' snapshot or after the 'after' one, so the exact counts below hold even
        // once global operator new is routed through the counters.
        std::vector<std::thread> threads = {};
        threads.reserve(THREAD_COUNT);
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([threadIndex, &threadPassed, &readyCount, &doneCount, &start, &finish] {
                std::vector<void *> blocks = {};
                blocks.reserve(PER_THREAD);
                std::vector<void *> sorted = {};
                sorted.reserve(PER_THREAD);
                readyCount.fetch_add(1, std::memory_order_release);
                wait_for(start);

                bool passed = true;
                for (int index = 0; index < PER_THREAD; ++index) {
                    const usize bytes = static_cast<usize>(index % 97) + 1;
                    void *block = tracked_allocate(bytes, 32);
                    passed = passed && block != nullptr && is_aligned(block, 32);
                    if (block != nullptr) {
                        fill_pattern(block, bytes, static_cast<u8>(threadIndex));
                    }
                    blocks.push_back(block);
                }

                sorted.assign(blocks.begin(), blocks.end());
                std::sort(sorted.begin(), sorted.end());
                passed = passed && std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end();

                for (int index = 0; index < PER_THREAD; ++index) {
                    const usize bytes = static_cast<usize>(index % 97) + 1;
                    void *block = blocks[static_cast<usize>(index)];
                    passed = passed && check_pattern(block, bytes, static_cast<u8>(threadIndex));
                    tracked_free(block, bytes);
                }
                threadPassed[static_cast<usize>(threadIndex)] = passed ? 1 : 0;
                doneCount.fetch_add(1, std::memory_order_release);
                wait_for(finish);
            });
        }

        wait_for_count(readyCount, THREAD_COUNT);
        const memory_snapshot before = snapshot();
        start.store(true, std::memory_order_release);
        wait_for_count(doneCount, THREAD_COUNT);
        const memory_snapshot after = snapshot();
        finish.store(true, std::memory_order_release);
        for (std::thread &thread : threads) {
            thread.join();
        }

        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            CHECK(threadPassed[static_cast<usize>(threadIndex)] == 1);
        }
        constexpr u64 TOTAL = static_cast<u64>(THREAD_COUNT) * PER_THREAD;
        CHECK(after.allocationCount == before.allocationCount + TOTAL);
        CHECK(after.freeCount == before.freeCount + TOTAL);
        CHECK(after.live_count() == before.live_count());
    }
}
