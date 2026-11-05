#include "conjure/testing/doctest.h"

#include "jobs_test_support.h"

#include "conjure/core/jobs/job_handle.h"

#include <atomic>
#include <memory>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::jobs;

TEST_SUITE("core/job_handle") {

    TEST_CASE("job_handle - a default handle is invalid and reports itself complete") {
        const job_handle handle = {};

        CHECK_FALSE(handle.valid());
        CHECK(handle.is_complete());
        CHECK(handle.pending() == 0);
    }

    TEST_CASE("job_handle - a handle over a counter reports pending work until the counter reaches zero") {
        std::shared_ptr<jobs::detail::job_counter> counter = std::make_shared<jobs::detail::job_counter>();
        counter->remaining.store(2);
        const job_handle handle(counter);

        CHECK(handle.valid());
        CHECK_FALSE(handle.is_complete());
        CHECK(handle.pending() == 2);

        counter->remaining.store(1);
        CHECK_FALSE(handle.is_complete());
        CHECK(handle.pending() == 1);

        counter->remaining.store(0);
        CHECK(handle.is_complete());
        CHECK(handle.pending() == 0);
    }

    TEST_CASE("job_handle - the counter holds more than 2^32 pending jobs without truncation") {
        // parallel_for stores its range count minus one here; a 32-bit counter would wrap for huge
        // ranges and report completion while ranges were still queued.
        constexpr u64 HUGE_COUNT = (u64{1} << 32) + 5;
        std::shared_ptr<jobs::detail::job_counter> counter = std::make_shared<jobs::detail::job_counter>();
        counter->remaining.store(HUGE_COUNT);
        const job_handle handle(counter);

        CHECK(handle.valid());
        CHECK_FALSE(handle.is_complete());
        CHECK(handle.pending() == HUGE_COUNT);

        counter->remaining.fetch_sub(u64{1} << 32);
        CHECK_FALSE(handle.is_complete());
        CHECK(handle.pending() == 5);

        counter->remaining.fetch_sub(5);
        CHECK(handle.is_complete());
    }

    TEST_CASE("job_handle - copies share the counter and compare equal") {
        std::shared_ptr<jobs::detail::job_counter> counter = std::make_shared<jobs::detail::job_counter>();
        counter->remaining.store(1);
        const job_handle original(counter);
        const job_handle copy = original;
        job_handle moved = original;
        const job_handle target = std::move(moved);

        CHECK(copy == original);
        CHECK(target == original);
        CHECK_FALSE(copy.is_complete());

        counter->remaining.store(0);
        CHECK(copy.is_complete());
        CHECK(target.is_complete());

        const job_handle unrelated(std::make_shared<jobs::detail::job_counter>());
        CHECK_FALSE(unrelated == original);
        CHECK(job_handle() == job_handle());
    }

    TEST_CASE("job_handle - reset makes a handle invalid without affecting its copies") {
        std::shared_ptr<jobs::detail::job_counter> counter = std::make_shared<jobs::detail::job_counter>();
        counter->remaining.store(1);
        job_handle handle(counter);
        const job_handle copy = handle;

        handle.reset();

        CHECK_FALSE(handle.valid());
        CHECK(handle.is_complete());
        CHECK(copy.valid());
        CHECK_FALSE(copy.is_complete());
        CHECK_FALSE(handle == copy);
    }

    TEST_CASE("job_handle - handles from a job system complete after wait [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::atomic<int> ran = 0;

        const job_handle handle = fixture->schedule([&ran] {
            ran.fetch_add(1);
        });
        REQUIRE(handle.valid());

        fixture->wait(handle);
        CHECK(handle.is_complete());
        CHECK(handle.pending() == 0);
        CHECK(ran.load() == 1);
    }

    TEST_CASE("job_handle - a handle outlives the job system that produced it") {
        job_handle handle = {};
        {
            testing::scoped_job_system fixture(8);
            handle = fixture->schedule([] {});
            fixture->wait(handle);
        }
        CHECK(handle.valid());
        CHECK(handle.is_complete());
    }
}
