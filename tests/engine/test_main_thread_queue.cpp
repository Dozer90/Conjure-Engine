#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/types.h"
#include "engine/main_thread_queue.h"

#include <atomic>
#include <vector>

// main_thread_queue.h (docs/design/06_engine_loop.md 2.9 "post_to_main_thread", 6): tasks posted from
// workers run on the main thread in post order; a task posted during the drain waits for the next one.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/main_thread_queue") {

    TEST_CASE("main_thread_queue - tasks from workers run on main in post order") {
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 4;
        CONJURE_REQUIRE_OK(jobs.start(desc));

        main_thread_queue queue = {};
        std::vector<u32> order = {};
        std::atomic<u32> posted = 0;
        for (u32 index = 0; index < 64; ++index) {
            const core::jobs::job_handle handle = jobs.schedule([&queue, &order, &posted, index]() {
                queue.post([&order, index]() {
                    CHECK(core::jobs::is_main_thread());
                    order.push_back(index);
                });
                posted.fetch_add(1, std::memory_order_relaxed);
            });
            // Waiting per job keeps the post order equal to the index order.
            jobs.wait(handle);
        }
        jobs.stop();

        CHECK(queue.pending_count() == 64);
        CHECK(queue.drain() == 64);
        CHECK(queue.pending_count() == 0);
        CHECK(queue.total_run() == 64);
        REQUIRE(order.size() == 64);
        for (u32 index = 0; index < 64; ++index) {
            CHECK(order[index] == index);
        }
    }

    TEST_CASE("main_thread_queue - a post during the drain is deferred to the next drain") {
        main_thread_queue queue = {};
        u32 runs = 0;
        queue.post([&queue, &runs]() {
            ++runs;
            CHECK(queue.is_draining());
            queue.post([&runs]() {
                ++runs;
            });
        });
        CHECK(queue.drain() == 1);
        CHECK(runs == 1);
        CHECK(queue.pending_count() == 1);
        CHECK(queue.drain() == 1);
        CHECK(runs == 2);
        CHECK(queue.drain() == 0);
    }

    TEST_CASE("main_thread_queue - clear drops pending tasks without running them") {
        main_thread_queue queue = {};
        u32 runs = 0;
        queue.post([&runs]() {
            ++runs;
        });
        queue.clear();
        CHECK(queue.pending_count() == 0);
        CHECK(queue.drain() == 0);
        CHECK(runs == 0);
    }

    TEST_CASE("main_thread_queue - a nested drain asserts and runs nothing") {
        main_thread_queue queue = {};
        u32 inner = 0;
        queue.post([&queue, &inner]() {
            CONJURE_CHECK_ASSERTS(inner = static_cast<u32>(queue.drain()));
        });
        CHECK(queue.drain() == 1);
        CHECK(inner == 0);
    }
}
