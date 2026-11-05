#include "conjure/testing/doctest.h"

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/memory/tracked_allocation.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core::memory;

namespace {
    void wait_for_count(const std::atomic<int> &counter, int target) {
        while (counter.load(std::memory_order_acquire) < target) {
            std::this_thread::yield();
        }
    }
}

TEST_SUITE("core/allocation_counters") {

    TEST_CASE("allocation_counters - counters returns one process-wide instance") {
        allocation_counters &first = counters();
        allocation_counters &second = counters();
        CHECK(&first == &second);
    }

    TEST_CASE("allocation_counters - snapshot copies the three counters") {
        const allocation_counters &live = counters();
        const memory_snapshot copy = snapshot();

        CHECK(copy.allocationCount == live.allocationCount.load());
        CHECK(copy.freeCount == live.freeCount.load());
        CHECK(copy.allocatedBytes == live.allocatedBytes.load());
    }

    TEST_CASE("allocation_counters - record_allocation and record_free bump the counters cumulatively") {
        const memory_snapshot before = snapshot();

        record_allocation(10);
        record_allocation(32);
        record_free();

        const memory_snapshot after = snapshot();
        CHECK(after.allocationCount == before.allocationCount + 2);
        CHECK(after.freeCount == before.freeCount + 1);
        CHECK(after.allocatedBytes == before.allocatedBytes + 42);
    }

    TEST_CASE("allocation_counters - freeing never decrements any counter") {
        record_allocation(8);
        const memory_snapshot before = snapshot();

        record_free();

        const memory_snapshot after = snapshot();
        CHECK(after.allocationCount == before.allocationCount);
        CHECK(after.allocatedBytes == before.allocatedBytes);
        CHECK(after.freeCount == before.freeCount + 1);
    }

    TEST_CASE("allocation_counters - live_count is allocations minus frees") {
        memory_snapshot sample = {};
        sample.allocationCount = 10;
        sample.freeCount = 4;
        CHECK(sample.live_count() == 6);

        const memory_snapshot before = snapshot();
        void *block = tracked_allocate(16);
        REQUIRE(block != nullptr);
        CHECK(snapshot().live_count() == before.live_count() + 1);
        tracked_free(block, 16);
        CHECK(snapshot().live_count() == before.live_count());
    }

    TEST_CASE("allocation_counters - snapshots compare field by field") {
        memory_snapshot left = {};
        memory_snapshot right = {};
        CHECK(left == right);

        right.allocatedBytes = 1;
        CHECK_FALSE(left == right);
    }

    TEST_CASE("allocation_counters - counters are monotonic across allocate and free") {
        memory_snapshot previous = snapshot();
        for (int iteration = 0; iteration < 64; ++iteration) {
            void *block = tracked_allocate(static_cast<usize>(iteration + 1));
            REQUIRE(block != nullptr);
            memory_snapshot current = snapshot();
            CHECK(current.allocationCount >= previous.allocationCount);
            CHECK(current.freeCount >= previous.freeCount);
            CHECK(current.allocatedBytes >= previous.allocatedBytes);
            previous = current;

            tracked_free(block, static_cast<usize>(iteration + 1));
            current = snapshot();
            CHECK(current.allocationCount >= previous.allocationCount);
            CHECK(current.freeCount >= previous.freeCount);
            CHECK(current.allocatedBytes >= previous.allocatedBytes);
            previous = current;
        }
    }

    TEST_CASE("allocation_counters - counters stay exact under concurrent updates") {
        constexpr int THREAD_COUNT = 8;
        constexpr int PER_THREAD = 2000;
        std::atomic<int> readyCount = 0;
        std::atomic<int> doneCount = 0;
        std::atomic<bool> start = false;
        std::atomic<bool> finish = false;

        // Thread start-up and tear-down allocate; both are kept outside the measured window so the
        // exact counts survive global operator new being routed through the counters.
        std::vector<std::thread> threads = {};
        threads.reserve(THREAD_COUNT);
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&readyCount, &doneCount, &start, &finish] {
                readyCount.fetch_add(1, std::memory_order_release);
                while (!start.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (int index = 0; index < PER_THREAD; ++index) {
                    record_allocation(3);
                    record_free();
                }
                doneCount.fetch_add(1, std::memory_order_release);
                while (!finish.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
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

        constexpr u64 TOTAL = static_cast<u64>(THREAD_COUNT) * PER_THREAD;
        CHECK(after.allocationCount == before.allocationCount + TOTAL);
        CHECK(after.freeCount == before.freeCount + TOTAL);
        CHECK(after.allocatedBytes == before.allocatedBytes + TOTAL * 3);
    }
}
