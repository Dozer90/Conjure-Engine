#include "conjure/testing/doctest.h"

#include "jobs_test_support.h"

#include "conjure/core/jobs/jobs.h"

#include <atomic>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::jobs;

TEST_SUITE("core/jobs") {

    TEST_CASE("jobs - the umbrella header exposes every jobs type") {
        // One use of each header's main entry point proves the umbrella includes them all.
        const job_handle handle = {};
        CHECK(handle.is_complete());

        job_system_desc desc = {};
        CHECK(desc.workerCount == 0);
        CHECK(desc.maxQueuedJobs == 16384);
        CHECK(std::string_view(desc.threadNamePrefix) == "conjure-worker");

        CHECK(default_grain(64, 8) == 2);
        CHECK(range_count(64, 2) == 32);

        set_main_thread();
        CHECK(is_main_thread());

        std::atomic<bool> ran = false;
        {
            dedicated_thread thread("umbrella", [&ran](std::stop_token) {
                ran.store(true);
            });
        }
        CHECK(ran.load());
    }

    TEST_CASE("jobs - the pieces work together [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        set_main_thread();

        std::vector<u32> values(256, 0);
        parallel_for_each(fixture.system(), std::span<u32>(values), [](u32 &value) {
            value = 7;
        });

        std::atomic<int> posted = 0;
        const job_handle handle = fixture->schedule([&posted] {
            post_to_main_thread([&posted] {
                posted.fetch_add(1);
            });
        });
        fixture->wait(handle);

        CHECK(drain_main_thread_queue() == 1);
        CHECK(posted.load() == 1);
        for (const u32 value : values) {
            CHECK(value == 7);
        }
    }
}
