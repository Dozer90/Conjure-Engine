#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "jobs_test_support.h"

#include "conjure/core/jobs/job_handle.h"
#include "conjure/core/jobs/job_system.h"

#include <atomic>
#include <functional>
#include <memory>
#include <span>
#include <vector>

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

    /*-----------------------------------------------------------------------*/
    // job_fence

    TEST_CASE("job_fence - a fresh fence is complete with nothing pending") {
        const job_fence fence = {};

        CHECK(fence.is_complete());
        CHECK(fence.pending() == 0);
    }

    TEST_CASE("job_fence - a batch scheduled on a fence is pending until wait returns [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::atomic<int> ran = 0;
        std::vector<std::function<void()>> jobs = {};
        for (int index = 0; index < 4; ++index) {
            jobs.emplace_back([&ran] {
                ran.fetch_add(1);
            });
        }

        job_fence fence = {};
        fixture->schedule(std::span<const std::function<void()>>(jobs), fence);
        // Inline execution (0 workers) finishes before schedule returns; workers may still be running.
        CHECK(fence.pending() <= 4);
        fixture->wait(fence);

        CHECK(fence.is_complete());
        CHECK(fence.pending() == 0);
        CHECK(ran.load() == 4);

        // Waiting again on a complete fence returns at once and the fence is reusable.
        fixture->wait(fence);
        fixture->schedule(std::span<const std::function<void()>>(jobs), fence);
        fixture->wait(fence);
        CHECK(fence.is_complete());
        CHECK(ran.load() == 8);
    }

    TEST_CASE("job_fence - several batches gather on one fence and one wait covers them all [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::atomic<int> ran = 0;
        std::vector<std::function<void()>> first = {};
        std::vector<std::function<void()>> second = {};
        for (int index = 0; index < 3; ++index) {
            first.emplace_back([&ran] {
                ran.fetch_add(1);
            });
            second.emplace_back([&ran] {
                ran.fetch_add(10);
            });
        }

        job_fence fence = {};
        fixture->schedule(std::span<const std::function<void()>>(first), fence);
        fixture->schedule(std::span<const std::function<void()>>(second), fence, job_priority::HIGH);
        fixture->wait(fence);

        CHECK(fence.is_complete());
        CHECK(ran.load() == 33);
        CHECK(fixture->stats().scheduled == 6);
    }

    TEST_CASE("job_fence - an empty batch leaves the fence complete and empty entries are skipped [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        testing::expect_assert asserts = {};
        std::atomic<int> ran = 0;

        job_fence fence = {};
        const std::vector<std::function<void()>> empty = {};
        fixture->schedule(std::span<const std::function<void()>>(empty), fence);
        CHECK(fence.is_complete());
        CHECK(fixture->stats().scheduled == 0);

        std::vector<std::function<void()>> sparse = {};
        sparse.emplace_back([&ran] { ran.fetch_add(1); });
        sparse.emplace_back(std::function<void()>());
        sparse.emplace_back([&ran] { ran.fetch_add(1); });
        fixture->schedule(std::span<const std::function<void()>>(sparse), fence);
        fixture->wait(fence);

        CHECK(ran.load() == 2);
        CHECK(asserts.count() == 1);
        CHECK(fixture->stats().scheduled == 2);
    }
}
