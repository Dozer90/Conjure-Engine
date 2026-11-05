#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "jobs_test_support.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/jobs/parallel_for.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <set>
#include <span>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::jobs;

namespace {
    struct range {
        usize begin = 0;
        usize end = 0;
    };

    // Thread-safe range recorder used to check how [0, count) was tiled.
    struct range_log {
        std::mutex mutex = {};
        std::vector<range> ranges = {};

        void add(usize begin, usize end) {
            const std::scoped_lock lock(mutex);
            ranges.push_back({begin, end});
        }

        std::vector<range> sorted() {
            const std::scoped_lock lock(mutex);
            std::vector<range> copy = ranges;
            std::sort(copy.begin(), copy.end(), [](const range &left, const range &right) {
                return left.begin < right.begin;
            });
            return copy;
        }
    };

    void check_tiles(const std::vector<range> &sorted, usize count, usize grain) {
        REQUIRE_FALSE(sorted.empty());
        CHECK(sorted.front().begin == 0);
        CHECK(sorted.back().end == count);
        for (usize index = 0; index < sorted.size(); ++index) {
            CHECK(sorted[index].end > sorted[index].begin);
            CHECK(sorted[index].end - sorted[index].begin <= grain);
            if (index > 0) {
                CHECK(sorted[index].begin == sorted[index - 1].end);
            }
        }
    }
}

TEST_SUITE("core/parallel_for") {

    TEST_CASE("parallel_for - default_grain and range_count follow the specification") {
        static_assert(default_grain(1000, 8) == 31);
        static_assert(default_grain(10, 8) == 1);
        static_assert(default_grain(100, 0) == 25);
        static_assert(default_grain(0, 4) == 1);
        static_assert(range_count(0, 5) == 0);
        static_assert(range_count(10, 3) == 4);
        static_assert(range_count(10, 0) == 10);
        static_assert(range_count(10, 10) == 1);
        static_assert(range_count(10, 100) == 1);

        CHECK(default_grain(64, 8) == 2);
        CHECK(range_count(64, 2) == 32);
    }

    TEST_CASE("parallel_for - parallel_for visits every index exactly once [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr usize COUNT = 1000;
        std::vector<std::atomic<u32>> hits(COUNT);

        fixture->parallel_for(COUNT, 7, [&hits](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                hits[index].fetch_add(1);
            }
        });

        for (usize index = 0; index < COUNT; ++index) {
            CHECK(hits[index].load() == 1);
        }
    }

    TEST_CASE("parallel_for - parallel_for tiles 0 to count with ranges of at most grain elements [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr usize COUNT = 103;
        constexpr usize GRAIN = 10;
        range_log log = {};

        fixture->parallel_for(COUNT, GRAIN, [&log](usize begin, usize end) {
            log.add(begin, end);
        });

        const std::vector<range> sorted = log.sorted();
        if (workers == 0) {
            REQUIRE(sorted.size() == 1);
            CHECK(sorted.front().begin == 0);
            CHECK(sorted.front().end == COUNT);
        } else {
            CHECK(sorted.size() == range_count(COUNT, GRAIN));
            check_tiles(sorted, COUNT, GRAIN);
        }
    }

    TEST_CASE("parallel_for - parallel_for with count 0 does not call the body [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::atomic<int> calls = 0;

        fixture->parallel_for(0, 4, [&calls](usize, usize) {
            calls.fetch_add(1);
        });
        fixture->parallel_for(0, [&calls](usize) {
            calls.fetch_add(1);
        });

        CHECK(calls.load() == 0);
        CHECK(fixture->stats().scheduled == 0);
    }

    TEST_CASE("parallel_for - parallel_for runs inline when count <= grain [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        range_log log = {};
        std::atomic<u32> threadIndex = 99;

        fixture->parallel_for(10, 10, [&](usize begin, usize end) {
            log.add(begin, end);
            threadIndex.store(job_system::thread_index());
        });

        const std::vector<range> sorted = log.sorted();
        REQUIRE(sorted.size() == 1);
        CHECK(sorted.front().begin == 0);
        CHECK(sorted.front().end == 10);
        CHECK(threadIndex.load() == 0);

        const job_system_stats stats = fixture->stats();
        CHECK(stats.scheduled == 1);
        CHECK(stats.executed == 1);
        CHECK(stats.inlineExecutions == 1);
    }

    TEST_CASE("parallel_for - a grain of 0 is treated as 1 [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr usize COUNT = 5;
        range_log log = {};

        fixture->parallel_for(COUNT, 0, [&log](usize begin, usize end) {
            log.add(begin, end);
        });

        const std::vector<range> sorted = log.sorted();
        if (workers == 0) {
            REQUIRE(sorted.size() == 1);
            CHECK(sorted.front().end == COUNT);
        } else {
            REQUIRE(sorted.size() == COUNT);
            check_tiles(sorted, COUNT, 1);
        }
    }

    TEST_CASE("parallel_for - the caller participates in a parallel_for [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::mutex indicesMutex = {};
        std::set<u32> indices = {};
        range_log log = {};

        fixture->parallel_for(400, 4, [&](usize begin, usize end) {
            // Enough work per range for several threads to take part.
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            log.add(begin, end);
            const std::scoped_lock lock(indicesMutex);
            indices.insert(job_system::thread_index());
        });

        const std::scoped_lock lock(indicesMutex);
        CHECK(indices.contains(0));
        if (workers > 0) {
            CHECK(indices.size() > 1);
            check_tiles(log.sorted(), 400, 4);
        }
    }

    TEST_CASE("parallel_for - parallel_for results are deterministic with per-range writes [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr usize COUNT = 4096;

        std::vector<u64> expected(COUNT, 0);
        for (usize index = 0; index < COUNT; ++index) {
            expected[index] = static_cast<u64>(index) * index + 1;
        }

        for (int repetition = 0; repetition < 10; ++repetition) {
            std::vector<u64> actual(COUNT, 0);
            fixture->parallel_for(COUNT, 16, [&actual](usize begin, usize end) {
                for (usize index = begin; index < end; ++index) {
                    actual[index] = static_cast<u64>(index) * index + 1;
                }
            });
            CHECK(actual == expected);
        }
    }

    TEST_CASE("parallel_for - the index overload visits every index with the default grain [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr usize COUNT = 777;
        std::vector<std::atomic<u32>> hits(COUNT);

        fixture->parallel_for(COUNT, [&hits](usize index) {
            hits[index].fetch_add(1);
        });

        for (usize index = 0; index < COUNT; ++index) {
            CHECK(hits[index].load() == 1);
        }
        const job_system_stats stats = fixture->stats();
        if (workers == 0) {
            CHECK(stats.scheduled == 1);
        } else {
            CHECK(stats.scheduled == range_count(COUNT, default_grain(COUNT, workers)));
        }
    }

    TEST_CASE("parallel_for - nested parallel_for from inside a body completes [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        constexpr usize OUTER = 64;
        constexpr usize INNER = 100;
        std::atomic<usize> total = 0;

        system.parallel_for(OUTER, 8, [&](usize begin, usize end) {
            for (usize outer = begin; outer < end; ++outer) {
                system.parallel_for(INNER, 10, [&total](usize innerBegin, usize innerEnd) {
                    total.fetch_add(innerEnd - innerBegin, std::memory_order_relaxed);
                });
            }
        });

        CHECK(total.load() == OUTER * INNER);
    }

    TEST_CASE("parallel_for - parallel_for from inside a scheduled job completes [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        std::vector<std::atomic<u32>> hits(500);

        const job_handle handle = system.schedule([&] {
            system.parallel_for(hits.size(), 3, [&hits](usize begin, usize end) {
                for (usize index = begin; index < end; ++index) {
                    hits[index].fetch_add(1);
                }
            });
        });
        system.wait(handle);

        for (const std::atomic<u32> &hit : hits) {
            CHECK(hit.load() == 1);
        }
    }

    TEST_CASE("parallel_for - parallel_for rejects an empty body [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        testing::expect_assert asserts = {};

        fixture->parallel_for(10, 2, std::function<void(usize, usize)>());
        fixture->parallel_for(10, std::function<void(usize)>());

        CHECK(asserts.count() == 2);
        CHECK(fixture->stats().scheduled == 0);
    }

    TEST_CASE("parallel_for - parallel_for on a full queue runs the remaining ranges on the caller") {
        constexpr usize QUEUE_CAPACITY = 4;
        constexpr usize COUNT = 100;
        testing::scoped_job_system fixture(1, QUEUE_CAPACITY);
        std::atomic<bool> gateOpen = false;
        std::atomic<bool> gateEntered = false;
        std::vector<std::atomic<u32>> hits(COUNT);

        static_cast<void>(fixture->schedule([&gateOpen, &gateEntered] {
            gateEntered.store(true);
            while (!gateOpen.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }));
        REQUIRE(testing::spin_until([&gateEntered] { return gateEntered.load(); }));

        // The caller runs its own range, the ranges that did not fit, then waits for the four
        // queued ones, which the worker releases once the gate opens.
        dedicated_thread opener("opener", [&gateOpen](std::stop_token) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            gateOpen.store(true, std::memory_order_release);
        });
        fixture->parallel_for(COUNT, 1, [&hits](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                hits[index].fetch_add(1);
            }
        });

        for (const std::atomic<u32> &hit : hits) {
            CHECK(hit.load() == 1);
        }
        // parallel_for may return before the gate job itself finishes; let it drain first.
        fixture->wait_idle();
        const job_system_stats stats = fixture->stats();
        CHECK(stats.queueFullFallbacks >= 1);
        CHECK(stats.inlineExecutions >= COUNT - 1 - QUEUE_CAPACITY);
        CHECK(stats.scheduled == COUNT + 1);
        CHECK(stats.executed == COUNT + 1);
    }

    /*-----------------------------------------------------------------------*/
    // Allocation behaviour

    TEST_CASE("parallel_for - parallel_for dispatch does not grow the block pool after warm-up [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr usize COUNT = 10000;
        constexpr usize GRAIN = 10;
        std::vector<u32> values(COUNT, 0);
        const std::function<void(usize, usize)> body = [&values](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                values[index] = static_cast<u32>(index);
            }
        };

        fixture->parallel_for(COUNT, GRAIN, body);
        const job_system_stats afterWarmUp = fixture->stats();
        if (workers == 0) {
            CHECK(afterWarmUp.poolGrowths == 0);
        } else {
            // 999 ranges are queued under one lock, far beyond the initial pool size.
            CHECK(afterWarmUp.poolGrowths > 0);
            CHECK(afterWarmUp.poolCapacity >= range_count(COUNT, GRAIN) - 1);
        }

        for (int repetition = 0; repetition < 25; ++repetition) {
            fixture->parallel_for(COUNT, GRAIN, body);
        }

        const job_system_stats steady = fixture->stats();
        CHECK(steady.poolGrowths == afterWarmUp.poolGrowths);
        CHECK(steady.poolCapacity == afterWarmUp.poolCapacity);
        for (usize index = 0; index < COUNT; ++index) {
            CHECK(values[index] == index);
        }
    }

    TEST_CASE("parallel_for - parallel_for on a system that was never started runs inline on the caller") {
        job_system system = {};
        range_log log = {};
        std::atomic<u32> threadIndex = 99;
        std::atomic<int> perIndexCalls = 0;

        system.parallel_for(100, 10, [&](usize begin, usize end) {
            log.add(begin, end);
            threadIndex.store(job_system::thread_index());
        });
        system.parallel_for(100, [&perIndexCalls](usize) {
            perIndexCalls.fetch_add(1);
        });

        const std::vector<range> sorted = log.sorted();
        REQUIRE(sorted.size() == 1);
        CHECK(sorted.front().begin == 0);
        CHECK(sorted.front().end == 100);
        CHECK(threadIndex.load() == 0);
        CHECK(perIndexCalls.load() == 100);

        const job_system_stats stats = system.stats();
        CHECK(stats.scheduled == 2);
        CHECK(stats.executed == 2);
        CHECK(stats.inlineExecutions == 2);
        CHECK(stats.poolGrowths == 0);
        CHECK(stats.queuedNow == 0);
    }

    /*-----------------------------------------------------------------------*/
    // Free helpers

    TEST_CASE("parallel_for - parallel_for_range accepts any callable [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::vector<std::atomic<u32>> hits(300);

        parallel_for_range(fixture.system(), hits.size(), 7, [&hits](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                hits[index].fetch_add(1);
            }
        });

        for (const std::atomic<u32> &hit : hits) {
            CHECK(hit.load() == 1);
        }
    }

    TEST_CASE("parallel_for - parallel_for_each visits every element with an explicit and a default grain [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::vector<u32> values(1000, 0);

        parallel_for_each(fixture.system(), std::span<u32>(values), 16, [](u32 &value) {
            value += 1;
        });
        parallel_for_each(fixture.system(), std::span<u32>(values), [](u32 &value) {
            value += 10;
        });

        for (const u32 value : values) {
            CHECK(value == 11);
        }

        // A const span works for read-only bodies.
        std::atomic<u64> sum = 0;
        const std::vector<u32> &constValues = values;
        parallel_for_each(fixture.system(), std::span<const u32>(constValues), [&sum](const u32 &value) {
            sum.fetch_add(value, std::memory_order_relaxed);
        });
        CHECK(sum.load() == 11 * 1000);

        // Empty input: nothing runs.
        std::vector<u32> empty = {};
        parallel_for_each(fixture.system(), std::span<u32>(empty), [](u32 &) {
            FAIL("body called for an empty span");
        });
    }
}
