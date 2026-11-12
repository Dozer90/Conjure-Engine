#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"

#include "jobs_test_support.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/jobs/main_thread.h"

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::jobs;

namespace {
    // The queue is process-wide; every test leaves it empty so cases stay independent.
    void drain_everything() {
        set_main_thread();
        while (drain_main_thread_queue() > 0) {
        }
    }
}

TEST_SUITE("core/main_thread") {

    /*-----------------------------------------------------------------------*/
    // Identity

    TEST_CASE("main_thread - set_main_thread designates the calling thread") {
        set_main_thread();

        CHECK(is_main_thread());
        CHECK(main_thread_id() == std::this_thread::get_id());
    }

    TEST_CASE("main_thread - other threads are not the main thread") {
        set_main_thread();
        std::atomic<bool> otherIsMain = true;
        std::thread::id otherId = {};

        {
            dedicated_thread probe("probe", [&otherIsMain, &otherId](std::stop_token) {
                otherIsMain.store(is_main_thread());
                otherId = std::this_thread::get_id();
            });
        }

        CHECK_FALSE(otherIsMain.load());
        CHECK(otherId != main_thread_id());
        CHECK(is_main_thread());
    }

    TEST_CASE("main_thread - the first thread to ask becomes the main thread when none is designated") {
        reset_main_thread();
        CHECK(main_thread_id() == std::thread::id());

        std::atomic<bool> claimed = false;
        std::thread::id claimant = {};
        {
            dedicated_thread probe("claimant", [&claimed, &claimant](std::stop_token) {
                claimed.store(is_main_thread());
                claimant = std::this_thread::get_id();
            });
        }

        CHECK(claimed.load());
        CHECK(main_thread_id() == claimant);
        CHECK_FALSE(is_main_thread());

        // Designation moves back to this thread explicitly.
        set_main_thread();
        CHECK(is_main_thread());
    }

    TEST_CASE("main_thread - set_main_thread can move the designation between threads") {
        set_main_thread();
        {
            dedicated_thread taker("taker", [](std::stop_token) {
                set_main_thread();
            });
        }
        CHECK_FALSE(is_main_thread());

        set_main_thread();
        CHECK(is_main_thread());
    }

    TEST_CASE("main_thread - constructing the first job system claims the main thread when none is designated") {
        reset_main_thread();
        {
            const job_system system = {};
        }
        CHECK(is_main_thread());
        CHECK(main_thread_id() == std::this_thread::get_id());
    }

    TEST_CASE("main_thread - constructing a job system does not steal an existing designation") {
        set_main_thread();
        const std::thread::id mainId = std::this_thread::get_id();
        {
            dedicated_thread builder("builder", [](std::stop_token) {
                const job_system system = {};
            });
        }
        CHECK(main_thread_id() == mainId);
        CHECK(is_main_thread());
    }

    /*-----------------------------------------------------------------------*/
    // Queue

    TEST_CASE("main_thread - posted tasks run once, in order, on the next drain") {
        drain_everything();
        std::vector<int> order = {};

        for (int index = 0; index < 5; ++index) {
            post_to_main_thread([&order, index] {
                order.push_back(index);
            });
        }
        CHECK(main_thread_queue_size() == 5);

        CHECK(drain_main_thread_queue() == 5);
        const std::vector<int> expected = {0, 1, 2, 3, 4};
        CHECK(order == expected);
        CHECK(main_thread_queue_size() == 0);

        CHECK(drain_main_thread_queue() == 0);
        CHECK(order.size() == 5);
    }

    TEST_CASE("main_thread - tasks posted during a drain run on the next drain") {
        drain_everything();
        int outerRuns = 0;
        int innerRuns = 0;

        post_to_main_thread([&outerRuns, &innerRuns] {
            ++outerRuns;
            post_to_main_thread([&innerRuns] {
                ++innerRuns;
            });
        });

        CHECK(drain_main_thread_queue() == 1);
        CHECK(outerRuns == 1);
        CHECK(innerRuns == 0);
        CHECK(main_thread_queue_size() == 1);

        CHECK(drain_main_thread_queue() == 1);
        CHECK(innerRuns == 1);
        CHECK(main_thread_queue_size() == 0);
    }

    TEST_CASE("main_thread - posting after a drain reuses the queue's capacity") {
        drain_everything();
        int runs = 0;
        post_to_main_thread([&runs] {
            ++runs;
        });
        CHECK(drain_main_thread_queue() == 1);

        // The drain hands its emptied buffer back, so the next post finds room without allocating.
        CONJURE_CHECK_NO_ALLOCATIONS(post_to_main_thread([&runs] {
            ++runs;
        }));
        CHECK(drain_main_thread_queue() == 1);
        CHECK(runs == 2);
    }

    TEST_CASE("main_thread - draining an empty queue returns zero") {
        drain_everything();
        CHECK(drain_main_thread_queue() == 0);
        CHECK(main_thread_queue_size() == 0);
    }

    TEST_CASE("main_thread - posting an empty task asserts and is dropped") {
        drain_everything();
        testing::expect_assert asserts = {};

        post_to_main_thread(std::function<void()>());

        CHECK(asserts.count() == 1);
        CHECK(main_thread_queue_size() == 0);
        CHECK(drain_main_thread_queue() == 0);
    }

    TEST_CASE("main_thread - draining off the main thread asserts, runs nothing and leaves the tasks queued") {
        drain_everything();
        testing::expect_assert asserts = {};
        std::atomic<int> ran = 0;
        std::atomic<usize> drained = 99;
        post_to_main_thread([&ran] {
            ran.fetch_add(1);
        });

        {
            dedicated_thread wrong("wrong-thread", [&drained](std::stop_token) {
                drained.store(drain_main_thread_queue());
            });
        }

        CHECK(asserts.count() == 1);
        CHECK(drained.load() == 0);
        CHECK(ran.load() == 0);
        CHECK(main_thread_queue_size() == 1);
        CHECK(is_main_thread());

        // The task is still there for the real main thread.
        CHECK(drain_main_thread_queue() == 1);
        CHECK(ran.load() == 1);
        CHECK(main_thread_queue_size() == 0);
    }

    TEST_CASE("main_thread - draining before a main thread is designated asserts and does not claim it") {
        drain_everything();
        reset_main_thread();
        testing::expect_assert asserts = {};
        std::atomic<int> ran = 0;
        std::atomic<usize> drained = 99;
        post_to_main_thread([&ran] {
            ran.fetch_add(1);
        });

        {
            dedicated_thread probe("probe", [&drained](std::stop_token) {
                drained.store(drain_main_thread_queue());
            });
        }

        CHECK(asserts.count() == 1);
        CHECK(drained.load() == 0);
        CHECK(ran.load() == 0);
        CHECK(main_thread_id() == std::thread::id());
        CHECK(main_thread_queue_size() == 1);

        set_main_thread();
        CHECK(drain_main_thread_queue() == 1);
        CHECK(ran.load() == 1);
    }

    TEST_CASE("main_thread - workers can post to the main thread [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        drain_everything();
        testing::scoped_job_system fixture(workers);
        constexpr int JOB_COUNT = 300;
        std::atomic<int> postedByNonMain = 0;
        std::atomic<int> executed = 0;

        for (int index = 0; index < JOB_COUNT; ++index) {
            static_cast<void>(fixture->schedule([&postedByNonMain, &executed] {
                if (!is_main_thread()) {
                    postedByNonMain.fetch_add(1);
                }
                post_to_main_thread([&executed] {
                    CHECK(is_main_thread());
                    executed.fetch_add(1);
                });
            }));
        }
        // Wait without helping: under CTest every case starts in a fresh process whose workers are
        // still waking up, and wait_idle() would let the main thread run every job itself.
        CHECK(testing::spin_until([] { return main_thread_queue_size() == JOB_COUNT; }));
        fixture->wait_idle();

        CHECK(main_thread_queue_size() == JOB_COUNT);
        CHECK(executed.load() == 0);
        CHECK(drain_main_thread_queue() == JOB_COUNT);
        CHECK(executed.load() == JOB_COUNT);
        if (workers == 0) {
            CHECK(postedByNonMain.load() == 0);
        } else {
            // Jobs the main thread helped with inside wait_idle() were posted from the main thread.
            const u64 helped = fixture->stats().helped;
            CHECK(postedByNonMain.load() > 0);
            CHECK(static_cast<u64>(postedByNonMain.load()) + helped == JOB_COUNT);
        }
    }

    TEST_CASE("main_thread - posting from many threads at once loses nothing") {
        drain_everything();
        constexpr int THREAD_COUNT = 8;
        constexpr int POSTS_PER_THREAD = 250;
        std::atomic<int> executed = 0;

        {
            std::vector<dedicated_thread> posters = {};
            for (int thread = 0; thread < THREAD_COUNT; ++thread) {
                posters.emplace_back("poster", [&executed](std::stop_token) {
                    for (int index = 0; index < POSTS_PER_THREAD; ++index) {
                        post_to_main_thread([&executed] {
                            executed.fetch_add(1);
                        });
                    }
                });
            }
        }

        CHECK(main_thread_queue_size() == THREAD_COUNT * POSTS_PER_THREAD);
        CHECK(drain_main_thread_queue() == THREAD_COUNT * POSTS_PER_THREAD);
        CHECK(executed.load() == THREAD_COUNT * POSTS_PER_THREAD);
    }
}
