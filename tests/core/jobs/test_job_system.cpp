#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"

#include "jobs_test_support.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/jobs/job_system.h"
#include "core/jobs/thread_name.h"

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::jobs;

namespace {
    // A job that occupies its worker until released, so a test can pile work up behind it.
    struct gate {
        std::atomic<bool> open = false;
        std::atomic<bool> entered = false;

        std::function<void()> job() {
            return [this] {
                entered.store(true);
                while (!open.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            };
        }
    };

    // Ranges of the dispatch the allocation gates measure: start() pre-allocates a pool of 16 blocks,
    // enough for the 15 queued ranges plus the caller's own, so nothing of the dispatch allocates.
    constexpr usize GATED_RANGES = 16;
    constexpr usize GATED_GRAIN = 4;
    constexpr usize GATED_COUNT = GATED_RANGES * GATED_GRAIN;

    // Starts a pool and immediately gates its first dispatch and wait_idle() on zero allocations.
    // Without the start() handshake a worker whose prologue still allocated would be charged here.
    void check_first_dispatch_allocates_nothing(u32 workers) {
        std::atomic<usize> visited = 0;
        const std::function<void(usize, usize)> body = [&visited](usize begin, usize end) {
            visited.fetch_add(end - begin, std::memory_order_relaxed);
        };

        job_system system = {};
        job_system_desc desc = {};
        desc.workerCount = workers;
        REQUIRE(system.start(desc).has_value());

        const auto dispatch = [&system, &body] {
            system.parallel_for(GATED_COUNT, GATED_GRAIN, body);
            system.wait_idle();
        };
        CONJURE_CHECK_NO_ALLOCATIONS(dispatch());
        CHECK(visited.load() == GATED_COUNT);
        CHECK(system.stats().poolGrowths == 0);
        system.stop();
    }

    // Frees a job parked in wait_idle() whose wake-up never came, so a case that detects the lost
    // wake-up reports a failure instead of hanging the fixture's stop(): a job entering wait_idle()
    // publishes itself to the idle-waiter depth and notifies the monitor, and the parked job then
    // sees active <= depth whichever thread runs the rescue job.
    void release_parked_idle_waiter(job_system &system) {
        static_cast<void>(system.schedule([&system] {
            system.wait_idle();
        }));
    }
}

TEST_SUITE("core/job_system") {

    /*-----------------------------------------------------------------------*/
    // Lifecycle

    TEST_CASE("job_system - a new system is not running, has no workers and zero statistics") {
        const job_system system = {};

        CHECK_FALSE(system.is_running());
        CHECK(system.worker_count() == 0);

        const job_system_stats stats = system.stats();
        CHECK(stats.scheduled == 0);
        CHECK(stats.executed == 0);
        CHECK(stats.stolen == 0);
        CHECK(stats.helped == 0);
        CHECK(stats.inlineExecutions == 0);
        CHECK(stats.queueFullFallbacks == 0);
        CHECK(stats.maxQueueDepth == 0);
        CHECK(stats.queuedNow == 0);
        CHECK(stats.poolGrowths == 0);
        CHECK(stats.poolCapacity == 0);
    }

    TEST_CASE("job_system - start returns with its workers running: the first dispatch allocates nothing [parallel]") {
        u32 workers = 0;
        SUBCASE("1 worker") {
            workers = 1;
        }
        SUBCASE("4 workers") {
            workers = 4;
        }
        SUBCASE("8 workers") {
            workers = 8;
        }
        check_first_dispatch_allocates_nothing(workers);
    }

    TEST_CASE("job_system - 50 fresh starts on 1, 4 and 8 workers never charge a dispatch [parallel]") {
        constexpr u32 REPETITIONS = 50;
        constexpr std::array<u32, 3> WORKER_COUNTS = {1, 4, 8};
        for (u32 repetition = 0; repetition < REPETITIONS; ++repetition) {
            for (const u32 workers : WORKER_COUNTS) {
                check_first_dispatch_allocates_nothing(workers);
            }
        }
    }

    TEST_CASE("job_system - start creates the requested workers and stop joins them [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        job_system system = {};
        job_system_desc desc = {};
        desc.workerCount = workers;

        REQUIRE(system.start(desc).has_value());
        CHECK(system.is_running());
        CHECK(system.worker_count() == workers);

        system.stop();
        CHECK_FALSE(system.is_running());
        CHECK(system.worker_count() == 0);

        // Stopping twice is harmless.
        system.stop();
        CHECK_FALSE(system.is_running());
    }

    TEST_CASE("job_system - starting a running system fails with INVALID_STATE") {
        job_system system = {};
        job_system_desc desc = {};
        desc.workerCount = 2;
        REQUIRE(system.start(desc).has_value());

        const result<void> second = system.start(desc);
        REQUIRE_FALSE(second.has_value());
        CHECK(second.get_error().is(error_code::INVALID_STATE));
        CHECK(system.is_running());
        CHECK(system.worker_count() == 2);
    }

    TEST_CASE("job_system - start rejects invalid descriptions with INVALID_ARGUMENT") {
        job_system system = {};

        SUBCASE("zero queue capacity") {
            job_system_desc desc = {};
            desc.maxQueuedJobs = 0;
            const result<void> outcome = system.start(desc);
            REQUIRE_FALSE(outcome.has_value());
            CHECK(outcome.get_error().is(error_code::INVALID_ARGUMENT));
        }
        SUBCASE("queue capacity above the maximum") {
            job_system_desc desc = {};
            desc.maxQueuedJobs = MAX_QUEUED_JOBS + 1;
            const result<void> outcome = system.start(desc);
            REQUIRE_FALSE(outcome.has_value());
            CHECK(outcome.get_error().is(error_code::INVALID_ARGUMENT));
        }
        SUBCASE("too many workers") {
            job_system_desc desc = {};
            desc.workerCount = MAX_WORKER_COUNT + 1;
            const result<void> outcome = system.start(desc);
            REQUIRE_FALSE(outcome.has_value());
            CHECK(outcome.get_error().is(error_code::INVALID_ARGUMENT));
        }
        SUBCASE("null thread name prefix") {
            job_system_desc desc = {};
            desc.threadNamePrefix = nullptr;
            const result<void> outcome = system.start(desc);
            REQUIRE_FALSE(outcome.has_value());
            CHECK(outcome.get_error().is(error_code::INVALID_ARGUMENT));
        }
        SUBCASE("empty thread name prefix") {
            job_system_desc desc = {};
            desc.threadNamePrefix = "";
            const result<void> outcome = system.start(desc);
            REQUIRE_FALSE(outcome.has_value());
            CHECK(outcome.get_error().is(error_code::INVALID_ARGUMENT));
        }

        CHECK_FALSE(system.is_running());
    }

    TEST_CASE("job_system - start accepts the boundary values of the description") {
        job_system system = {};
        std::atomic<int> ran = 0;

        SUBCASE("MAX_WORKER_COUNT workers") {
            job_system_desc desc = {};
            desc.workerCount = MAX_WORKER_COUNT;
            REQUIRE(system.start(desc).has_value());
            CHECK(system.worker_count() == MAX_WORKER_COUNT);
        }
        SUBCASE("MAX_QUEUED_JOBS queue capacity") {
            job_system_desc desc = {};
            desc.workerCount = 1;
            desc.maxQueuedJobs = MAX_QUEUED_JOBS;
            REQUIRE(system.start(desc).has_value());
            CHECK(system.worker_count() == 1);
        }

        system.wait(system.schedule([&ran] {
            ran.fetch_add(1);
        }));
        system.stop();

        CHECK(ran.load() == 1);
        CHECK_FALSE(system.is_running());
    }

    TEST_CASE("job_system - a stopped system restarts with a different worker count") {
        job_system system = {};
        std::atomic<int> ran = 0;
        job_system_desc desc = {};

        desc.workerCount = 2;
        REQUIRE(system.start(desc).has_value());
        system.wait(system.schedule([&ran] {
            ran.fetch_add(1);
        }));
        system.stop();

        desc.workerCount = 4;
        REQUIRE(system.start(desc).has_value());
        CHECK(system.worker_count() == 4);
        system.wait(system.schedule([&ran] {
            ran.fetch_add(1);
        }));
        CHECK(system.stats().scheduled == 1);
        system.stop();

        CHECK(ran.load() == 2);
    }

    TEST_CASE("job_system - the destructor stops a running system and completes queued jobs") {
        std::atomic<int> ran = 0;
        constexpr int JOB_COUNT = 500;
        {
            job_system system = {};
            job_system_desc desc = {};
            desc.workerCount = 4;
            REQUIRE(system.start(desc).has_value());
            for (int index = 0; index < JOB_COUNT; ++index) {
                static_cast<void>(system.schedule([&ran] {
                    ran.fetch_add(1);
                }));
            }
        }
        CHECK(ran.load() == JOB_COUNT);
    }

    TEST_CASE("job_system - a system that is not running executes jobs inline") {
        job_system system = {};
        std::atomic<int> ran = 0;
        u32 observedIndex = 99;

        const job_handle handle = system.schedule([&ran, &observedIndex] {
            ran.fetch_add(1);
            observedIndex = job_system::thread_index();
        });

        CHECK(ran.load() == 1);
        CHECK(observedIndex == 0);
        CHECK(handle.is_valid());
        CHECK(handle.is_complete());

        const job_system_stats stats = system.stats();
        CHECK(stats.scheduled == 1);
        CHECK(stats.executed == 1);
        CHECK(stats.inlineExecutions == 1);
    }

    /*-----------------------------------------------------------------------*/
    // Scheduling and waiting

    TEST_CASE("job_system - schedule runs a job and wait completes its handle [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::atomic<int> ran = 0;

        const job_handle handle = fixture->schedule([&ran] {
            ran.fetch_add(1);
        });
        REQUIRE(handle.is_valid());
        fixture->wait(handle);

        CHECK(handle.is_complete());
        CHECK(ran.load() == 1);

        // Waiting again on a complete handle returns immediately.
        fixture->wait(handle);
        CHECK(ran.load() == 1);
    }

    TEST_CASE("job_system - every priority lane accepts and runs jobs [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::atomic<int> ran = 0;

        const job_handle high = fixture->schedule([&ran] { ran.fetch_add(1); }, job_priority::HIGH);
        const job_handle normal = fixture->schedule([&ran] { ran.fetch_add(1); }, job_priority::NORMAL);
        const job_handle low = fixture->schedule([&ran] { ran.fetch_add(1); }, job_priority::LOW);
        fixture->wait(high);
        fixture->wait(normal);
        fixture->wait(low);

        CHECK(ran.load() == 3);
    }

    TEST_CASE("job_system - schedule rejects an empty job") {
        testing::expect_assert asserts = {};
        testing::scoped_job_system fixture(0);

        const job_handle handle = fixture->schedule(std::function<void()>());

        CHECK_FALSE(handle.is_valid());
        CHECK(asserts.count() == 1);
        CHECK(fixture->stats().scheduled == 0);
    }

    TEST_CASE("job_system - a batch completes under one handle [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr usize BATCH_SIZE = 64;
        std::atomic<usize> ran = 0;

        std::vector<std::function<void()>> jobs = {};
        for (usize index = 0; index < BATCH_SIZE; ++index) {
            jobs.emplace_back([&ran] {
                ran.fetch_add(1);
            });
        }

        const job_handle handle = fixture->schedule(std::span<const std::function<void()>>(jobs));
        REQUIRE(handle.is_valid());
        fixture->wait(handle);

        CHECK(handle.is_complete());
        CHECK(handle.pending() == 0);
        CHECK(ran.load() == BATCH_SIZE);
        CHECK(fixture->stats().scheduled == BATCH_SIZE);
    }

    TEST_CASE("job_system - an empty batch yields an invalid handle and empty entries are skipped [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        testing::expect_assert asserts = {};
        std::atomic<int> ran = 0;

        const std::vector<std::function<void()>> empty = {};
        CHECK_FALSE(fixture->schedule(std::span<const std::function<void()>>(empty)).is_valid());

        std::vector<std::function<void()>> sparse = {};
        sparse.emplace_back([&ran] { ran.fetch_add(1); });
        sparse.emplace_back(std::function<void()>());
        sparse.emplace_back([&ran] { ran.fetch_add(1); });

        const job_handle handle = fixture->schedule(std::span<const std::function<void()>>(sparse));
        REQUIRE(handle.is_valid());
        fixture->wait(handle);

        CHECK(ran.load() == 2);
        CHECK(asserts.count() == 1);
        CHECK(fixture->stats().scheduled == 2);
    }

    TEST_CASE("job_system - wait on an invalid handle returns immediately [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);

        fixture->wait(job_handle());
        CHECK(fixture->stats().helped == 0);
    }

    TEST_CASE("job_system - wait from inside a job helps and completes [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        std::atomic<int> innerRan = 0;
        std::atomic<bool> innerCompleteWhenOuterChecked = false;

        const job_handle outer = system.schedule([&] {
            const job_handle inner = system.schedule([&innerRan] {
                innerRan.fetch_add(1);
            });
            system.wait(inner);
            innerCompleteWhenOuterChecked.store(inner.is_complete());
        });
        system.wait(outer);

        CHECK(outer.is_complete());
        CHECK(innerRan.load() == 1);
        CHECK(innerCompleteWhenOuterChecked.load());
    }

    TEST_CASE("job_system - wait never deadlocks on a single worker") {
        testing::scoped_job_system fixture(1);
        job_system &system = fixture.system();
        std::atomic<int> innerRan = 0;
        std::atomic<bool> outerEntered = false;

        // The only worker runs the outer job; waiting inside it must execute the inner jobs itself.
        const job_handle outer = system.schedule([&] {
            outerEntered.store(true, std::memory_order_release);
            for (int index = 0; index < 4; ++index) {
                const job_handle inner = system.schedule([&innerRan] {
                    innerRan.fetch_add(1);
                });
                system.wait(inner);
            }
        });
        // Wait for the worker to be inside the outer job first, otherwise this thread's wait() could
        // pop the outer job itself and the inner jobs would be stolen by the worker, not helped.
        REQUIRE(testing::spin_until([&outerEntered] {
            return outerEntered.load(std::memory_order_acquire);
        }));
        system.wait(outer);

        CHECK(innerRan.load() == 4);
        CHECK(fixture->stats().helped >= 4);
    }

    TEST_CASE("job_system - wait_idle returns once every job has run [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr int JOB_COUNT = 1000;
        std::atomic<int> ran = 0;

        for (int index = 0; index < JOB_COUNT; ++index) {
            static_cast<void>(fixture->schedule([&ran] {
                ran.fetch_add(1);
            }));
        }
        fixture->wait_idle();

        CHECK(ran.load() == JOB_COUNT);
        const job_system_stats stats = fixture->stats();
        CHECK(stats.queuedNow == 0);
        CHECK(stats.executed == JOB_COUNT);

        // Idle again: returns immediately.
        fixture->wait_idle();
    }

    TEST_CASE("job_system - wait_idle from inside a job waits for every other job and returns [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        constexpr int SIBLING_COUNT = 50;
        std::atomic<int> siblingsDone = 0;
        std::atomic<int> siblingsDoneWhenIdle = -1;
        std::atomic<bool> returned = false;

        const job_handle waiter = system.schedule([&] {
            for (int index = 0; index < SIBLING_COUNT; ++index) {
                static_cast<void>(system.schedule([&siblingsDone] {
                    std::this_thread::sleep_for(std::chrono::microseconds(20));
                    siblingsDone.fetch_add(1);
                }));
            }
            system.wait_idle();
            siblingsDoneWhenIdle.store(siblingsDone.load());
            returned.store(true);
        });
        system.wait(waiter);

        CHECK(returned.load());
        CHECK(siblingsDoneWhenIdle.load() == SIBLING_COUNT);
        system.wait_idle();
        CHECK(system.stats().queuedNow == 0);
    }

    TEST_CASE("job_system - two in-flight jobs can call wait_idle at the same time without deadlocking") {
        u32 workers = 1;
        SUBCASE("1 worker") {
            workers = 1;
        }
        SUBCASE("8 workers") {
            workers = 8;
        }
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        std::atomic<int> arrived = 0;
        std::atomic<bool> rendezvousTimedOut = false;
        std::atomic<int> finished = 0;

        // Each job waits until the other one is running and then waits for the system to go idle:
        // "idle" has to exclude the other waiting job or the two wait for each other forever. On a
        // single worker the second job runs on this thread, which helps from inside wait_idle().
        const auto meetThenWaitIdle = [&] {
            arrived.fetch_add(1);
            if (!testing::spin_until([&arrived] { return arrived.load() == 2; })) {
                rendezvousTimedOut.store(true);
            }
            system.wait_idle();
            finished.fetch_add(1);
        };
        static_cast<void>(system.schedule(meetThenWaitIdle));
        static_cast<void>(system.schedule(meetThenWaitIdle));
        system.wait_idle();

        CHECK_FALSE(rendezvousTimedOut.load());
        // A wait_idle() made outside every job returns only once both jobs have fully finished.
        CHECK(finished.load() == 2);
        CHECK(system.stats().queuedNow == 0);
        CHECK(system.stats().executed == 2);
    }

    TEST_CASE("job_system - wait_idle nested inside a helped job still waits for unrelated jobs") {
        testing::scoped_job_system fixture(2);
        job_system &system = fixture.system();
        gate blocker = {};
        std::atomic<bool> blockerFinished = false;
        std::atomic<bool> innerEnteredWaitIdle = false;
        std::atomic<bool> blockerFinishedWhenInnerReturned = false;
        std::atomic<bool> outerDone = false;

        static_cast<void>(system.schedule([&] {
            blocker.job()();
            blockerFinished.store(true);
        }));
        REQUIRE(testing::spin_until([&blocker] { return blocker.entered.load(); }));

        // The outer job queues the inner one and runs it itself while helping inside wait_idle();
        // the inner job's own wait_idle() must still wait for the gated job, which belongs to
        // neither of them, even though the outer job already excluded itself from "idle".
        static_cast<void>(system.schedule([&] {
            static_cast<void>(system.schedule([&] {
                innerEnteredWaitIdle.store(true);
                system.wait_idle();
                blockerFinishedWhenInnerReturned.store(blockerFinished.load());
            }));
            system.wait_idle();
            outerDone.store(true);
        }));

        REQUIRE(testing::spin_until([&innerEnteredWaitIdle] { return innerEnteredWaitIdle.load(); }));
        // A best-effort negative check: a stalled runner can let it pass without the inner wait_idle()
        // having waited at all. The deterministic assertion is blockerFinishedWhenInnerReturned below.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK_FALSE(outerDone.load());
        blocker.open.store(true, std::memory_order_release);

        REQUIRE(testing::spin_until([&outerDone] { return outerDone.load(); }));
        CHECK(blockerFinishedWhenInnerReturned.load());
        system.wait_idle();
        CHECK(system.stats().executed == 3);
    }

    TEST_CASE("job_system - wait_idle inside a batch job wakes when a sibling of its own batch finishes") {
        // Both jobs share the batch's counter, so the sibling finishing drops neither the counter nor
        // the active count to zero: the waiter's wake-up has to come from finish_job noticing a
        // published idle waiter. The sibling is queued first and holds its worker until the waiter is
        // running, so the waiter cannot run it nested while helping, and it outlives the waiter's spin
        // window so the waiter is parked when it finishes: the 20 ms margin is against a waiter that is
        // preempted for a scheduler quantum between announcing itself and parking, not only the 50 us
        // window. The shared state is declared before the fixture because on the lost wake-up path the
        // rescued jobs may still be running when the case unwinds; the fixture's stop() joins them first.
        std::atomic<bool> waiterArrived = false;
        std::atomic<bool> siblingDone = false;
        std::atomic<bool> siblingDoneWhenIdle = false;
        testing::scoped_job_system fixture(2);
        job_system &system = fixture.system();

        const std::array<std::function<void()>, 2> jobs = {
            [&waiterArrived, &siblingDone] {
                static_cast<void>(testing::spin_until([&waiterArrived] { return waiterArrived.load(); }));
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                siblingDone.store(true);
            },
            [&system, &waiterArrived, &siblingDone, &siblingDoneWhenIdle] {
                waiterArrived.store(true);
                system.wait_idle();
                siblingDoneWhenIdle.store(siblingDone.load());
            },
        };
        const job_handle batch = system.schedule(std::span<const std::function<void()>>(jobs));

        // Polled rather than waited for: helping would let this thread run one of the two itself.
        const bool completed = testing::spin_until([&batch] { return batch.is_complete(); });
        if (!completed) {
            release_parked_idle_waiter(system);
        }
        REQUIRE(completed);
        CHECK(siblingDoneWhenIdle.load());
    }

    TEST_CASE("job_system - wait_idle inside a parallel_for range wakes when a sibling range finishes") {
        // The parallel_for variant of the batch case: every queued range counts on the dispatch's stack
        // counter, so one range finishing never drops it to zero while another is still running. Range
        // 1 (queued first) is the sibling, range 2 the waiter; the dispatching thread runs range 0 and
        // then helps, so a dedicated thread dispatches and this thread watches with a timeout. The
        // sibling's 20 ms outlive a waiter preempted between announcing itself and parking.
        testing::scoped_job_system fixture(2);
        job_system &system = fixture.system();
        std::atomic<bool> waiterArrived = false;
        std::atomic<bool> siblingDone = false;
        std::atomic<bool> siblingDoneWhenIdle = false;
        std::atomic<bool> dispatched = false;

        const std::function<void(usize, usize)> body = [&](usize begin, usize) {
            if (begin == 1) {
                static_cast<void>(testing::spin_until([&waiterArrived] { return waiterArrived.load(); }));
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                siblingDone.store(true);
            } else if (begin == 2) {
                waiterArrived.store(true);
                system.wait_idle();
                siblingDoneWhenIdle.store(siblingDone.load());
            }
        };
        dedicated_thread dispatcher("dispatcher", [&system, &body, &dispatched](std::stop_token) {
            system.parallel_for(3, 1, body);
            dispatched.store(true);
        });

        const bool completed = testing::spin_until([&dispatched] { return dispatched.load(); });
        if (!completed) {
            release_parked_idle_waiter(system);
        }
        REQUIRE(completed);
        CHECK(siblingDoneWhenIdle.load());
    }

    TEST_CASE("job_system - stop completes pending jobs before returning [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        constexpr int JOB_COUNT = 2000;
        std::atomic<int> ran = 0;
        job_system system = {};
        job_system_desc desc = {};
        desc.workerCount = workers;
        REQUIRE(system.start(desc).has_value());

        for (int index = 0; index < JOB_COUNT; ++index) {
            static_cast<void>(system.schedule([&ran] {
                ran.fetch_add(1);
            }));
        }
        system.stop();

        CHECK(ran.load() == JOB_COUNT);
        CHECK(system.stats().executed == JOB_COUNT);
        CHECK(system.stats().queuedNow == 0);
    }

    TEST_CASE("job_system - jobs scheduled by a running job during stop still execute") {
        // Declared before the system so they outlive it.
        std::atomic<int> ran = 0;
        std::atomic<int> inlineProbes = 0;
        std::atomic<bool> stopEntered = false;
        std::atomic<bool> outerSawStop = false;
        job_system system = {};
        job_system_desc desc = {};
        desc.workerCount = 2;
        REQUIRE(system.start(desc).has_value());

        // The outer job holds its worker until stop() has been entered, then schedules the follow-up. It
        // records whether it saw stop() rather than giving up, so a stalled runner cannot pass vacuously.
        static_cast<void>(system.schedule([&] {
            outerSawStop.store(testing::spin_until([&stopEntered] { return stopEntered.load(); }));
            static_cast<void>(system.schedule([&ran] {
                ran.fetch_add(1);
            }));
            ran.fetch_add(1);
        }));

        // stop() clears "accepting jobs" under the queue lock, after which a job scheduled from outside
        // the workers runs inline on the scheduling thread: the prober's first inline probe proves stop()
        // has been entered. At most one probe is queued at a time, so the queue-full inline fallback can
        // never fire, and the probe is polled rather than waited for because wait() would help and run
        // queued work on the prober.
        dedicated_thread prober("prober", [&](std::stop_token) {
            const std::thread::id self = std::this_thread::get_id();
            while (!stopEntered.load()) {
                const job_handle probe = system.schedule([&inlineProbes, self] {
                    if (std::this_thread::get_id() == self) {
                        inlineProbes.fetch_add(1);
                    }
                });
                static_cast<void>(testing::spin_until([&probe] { return probe.is_complete(); }));
                if (inlineProbes.load() > 0) {
                    stopEntered.store(true);
                }
            }
        });
        system.stop();
        prober.join();

        REQUIRE(stopEntered.load());
        CHECK(outerSawStop.load());
        CHECK(ran.load() == 2);
        CHECK(system.stats().queueFullFallbacks == 0);
    }

    TEST_CASE("job_system - jobs scheduled from outside after stop run inline") {
        job_system system = {};
        job_system_desc desc = {};
        desc.workerCount = 2;
        REQUIRE(system.start(desc).has_value());
        system.stop();

        std::atomic<int> ran = 0;
        const job_handle handle = system.schedule([&ran] {
            ran.fetch_add(1);
        });

        CHECK(ran.load() == 1);
        CHECK(handle.is_complete());
    }

    TEST_CASE("job_system - stop and start racing with producers on other threads loses nothing and does not hang") {
        constexpr int PRODUCER_COUNT = 4;
        constexpr int CYCLE_COUNT = 12;
        constexpr usize ELEMENTS_PER_LOOP = 64;
        job_system system = {};
        std::atomic<u64> scheduledJobs = 0;
        std::atomic<u64> ranJobs = 0;
        std::atomic<u64> expectedElements = 0;
        std::atomic<u64> visitedElements = 0;
        std::atomic<int> incompleteHandles = 0;
        std::atomic<int> producersFinished = 0;
        job_system_desc desc = {};
        desc.workerCount = 4;
        REQUIRE(system.start(desc).has_value());

        {
            std::vector<dedicated_thread> producers = {};
            for (int producer = 0; producer < PRODUCER_COUNT; ++producer) {
                producers.emplace_back("producer", [&](std::stop_token token) {
                    // Every entry point has to work whether the system is running, stopping,
                    // stopped or being restarted underneath the caller.
                    while (!token.stop_requested()) {
                        scheduledJobs.fetch_add(1);
                        const job_handle handle = system.schedule([&ranJobs] {
                            ranJobs.fetch_add(1);
                        });
                        system.wait(handle);
                        if (!handle.is_complete()) {
                            incompleteHandles.fetch_add(1);
                        }

                        expectedElements.fetch_add(ELEMENTS_PER_LOOP);
                        system.parallel_for(ELEMENTS_PER_LOOP, 4, [&visitedElements](usize begin, usize end) {
                            visitedElements.fetch_add(end - begin);
                        });
                        system.wait_idle();
                    }
                    producersFinished.fetch_add(1);
                });
            }

            for (int cycle = 0; cycle < CYCLE_COUNT; ++cycle) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                system.stop();
                CHECK_FALSE(system.is_running());
                // Alternate a tiny and a large queue so both the full-queue and the queued paths race.
                desc.maxQueuedJobs = cycle % 2 == 0 ? 8 : 4096;
                REQUIRE(system.start(desc).has_value());
            }

            for (dedicated_thread &producer : producers) {
                producer.request_stop();
            }
            // Fail loudly instead of hanging in the join when a producer is stuck.
            const auto allProducersFinished = [&producersFinished] {
                return producersFinished.load() == PRODUCER_COUNT;
            };
            CHECK(testing::spin_until(allProducersFinished, 30.0));
        }
        system.stop();

        CHECK(scheduledJobs.load() > 0);
        CHECK(ranJobs.load() == scheduledJobs.load());
        CHECK(visitedElements.load() == expectedElements.load());
        CHECK(incompleteHandles.load() == 0);
        CHECK(system.stats().queuedNow == 0);
    }

    TEST_CASE("job_system - a job can schedule follow-up work [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        constexpr int CHAIN_LENGTH = 100;
        std::atomic<int> links = 0;

        std::function<void()> link = {};
        link = [&] {
            if (links.fetch_add(1) + 1 < CHAIN_LENGTH) {
                static_cast<void>(system.schedule(link));
            }
        };
        static_cast<void>(system.schedule(link));
        fixture->wait_idle();

        CHECK(links.load() == CHAIN_LENGTH);
    }

    /*-----------------------------------------------------------------------*/
    // Priorities and back-pressure

    TEST_CASE("job_system - higher priority jobs run first on a single worker") {
        testing::scoped_job_system fixture(1);
        gate blocker = {};
        std::mutex orderMutex = {};
        std::vector<job_priority> order = {};
        const auto record = [&orderMutex, &order](job_priority priority) {
            return [&orderMutex, &order, priority] {
                const std::scoped_lock lock(orderMutex);
                order.push_back(priority);
            };
        };

        static_cast<void>(fixture->schedule(blocker.job(), job_priority::HIGH));
        REQUIRE(testing::spin_until([&blocker] { return blocker.entered.load(); }));

        static_cast<void>(fixture->schedule(record(job_priority::LOW), job_priority::LOW));
        static_cast<void>(fixture->schedule(record(job_priority::NORMAL), job_priority::NORMAL));
        static_cast<void>(fixture->schedule(record(job_priority::HIGH), job_priority::HIGH));
        CHECK(fixture->stats().queuedNow == 3);

        blocker.open.store(true, std::memory_order_release);
        // Poll instead of wait_idle() so the worker, not this thread, runs the three jobs.
        REQUIRE(testing::spin_until([&orderMutex, &order] {
            const std::scoped_lock lock(orderMutex);
            return order.size() == 3;
        }));

        const std::scoped_lock lock(orderMutex);
        REQUIRE(order.size() == 3);
        CHECK(order[0] == job_priority::HIGH);
        CHECK(order[1] == job_priority::NORMAL);
        CHECK(order[2] == job_priority::LOW);
    }

    TEST_CASE("job_system - a full queue runs the job inline on the scheduling thread") {
        constexpr usize QUEUE_CAPACITY = 4;
        testing::scoped_job_system fixture(1, QUEUE_CAPACITY);
        gate blocker = {};
        std::atomic<int> ran = 0;
        std::atomic<u32> inlineThreadIndex = 99;

        static_cast<void>(fixture->schedule(blocker.job()));
        REQUIRE(testing::spin_until([&blocker] { return blocker.entered.load(); }));

        for (usize index = 0; index < QUEUE_CAPACITY; ++index) {
            static_cast<void>(fixture->schedule([&ran] {
                ran.fetch_add(1);
            }));
        }
        CHECK(fixture->stats().queuedNow == QUEUE_CAPACITY);
        CHECK(ran.load() == 0);

        const job_handle overflow = fixture->schedule([&ran, &inlineThreadIndex] {
            ran.fetch_add(1);
            inlineThreadIndex.store(job_system::thread_index());
        });
        CHECK(overflow.is_complete());
        CHECK(ran.load() == 1);
        CHECK(inlineThreadIndex.load() == 0);

        job_system_stats stats = fixture->stats();
        CHECK(stats.queueFullFallbacks == 1);
        CHECK(stats.inlineExecutions == 1);
        CHECK(stats.maxQueueDepth == QUEUE_CAPACITY);

        blocker.open.store(true, std::memory_order_release);
        fixture->wait_idle();
        CHECK(ran.load() == static_cast<int>(QUEUE_CAPACITY) + 1);
        stats = fixture->stats();
        CHECK(stats.executed == QUEUE_CAPACITY + 2);
        CHECK(stats.queuedNow == 0);
    }

    TEST_CASE("job_system - a batch on a warmed-up fence allocates nothing [parallel]") {
        // The handle overload pays one heap allocation per call for its shared counter; the fence
        // overload is the per-frame path (the ECS scheduler's stages) and must pay none once the block
        // pool holds enough blocks. The closures fit the std::function small buffer.
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        std::atomic<usize> ran = 0;
        std::vector<std::function<void()>> jobs = {};
        for (usize index = 0; index < 8; ++index) {
            jobs.emplace_back([&ran] {
                ran.fetch_add(1, std::memory_order_relaxed);
            });
        }
        job_fence fence = {};
        const auto dispatch = [&fixture, &jobs, &fence] {
            fixture->schedule(std::span<const std::function<void()>>(jobs), fence);
            fixture->wait(fence);
        };

        dispatch();
        CHECK(ran.load() == 8);
        for (u32 repetition = 0; repetition < 10; ++repetition) {
            CAPTURE(repetition);
            CONJURE_CHECK_NO_ALLOCATIONS(dispatch());
        }
        CHECK(ran.load() == 88);
        CHECK(fence.is_complete());
        CHECK(fixture->stats().scheduled == 88);
        CHECK(fixture->stats().executed == 88);
    }

    TEST_CASE("job_system - a full queue runs part of a fenced batch inline and the fence still completes") {
        constexpr usize QUEUE_CAPACITY = 2;
        testing::scoped_job_system fixture(1, QUEUE_CAPACITY);
        gate blocker = {};
        std::atomic<int> ran = 0;

        static_cast<void>(fixture->schedule(blocker.job()));
        REQUIRE(testing::spin_until([&blocker] { return blocker.entered.load(); }));

        std::vector<std::function<void()>> jobs = {};
        for (int index = 0; index < 5; ++index) {
            jobs.emplace_back([&ran] {
                ran.fetch_add(1);
            });
        }
        job_fence fence = {};
        fixture->schedule(std::span<const std::function<void()>>(jobs), fence);

        // Two were queued behind the gate; the other three ran on this thread.
        CHECK(ran.load() == 3);
        CHECK(fence.pending() == 2);
        CHECK(fixture->stats().queueFullFallbacks >= 1);

        blocker.open.store(true, std::memory_order_release);
        fixture->wait(fence);
        CHECK(ran.load() == 5);
        CHECK(fence.is_complete());
    }

    TEST_CASE("job_system - a fenced batch scheduled from inside a job can be waited for there [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        std::atomic<int> innerRan = 0;
        std::vector<std::function<void()>> inner = {};
        for (int index = 0; index < 4; ++index) {
            inner.emplace_back([&innerRan] {
                innerRan.fetch_add(1);
            });
        }

        const job_handle outer = system.schedule([&system, &inner, &innerRan] {
            job_fence fence = {};
            system.schedule(std::span<const std::function<void()>>(inner), fence);
            system.wait(fence);
            CHECK(fence.is_complete());
            CHECK(innerRan.load() == 4);
        });
        system.wait(outer);
        CHECK(innerRan.load() == 4);
    }

    TEST_CASE("job_system - a full queue falls back inline for part of a batch") {
        constexpr usize QUEUE_CAPACITY = 2;
        testing::scoped_job_system fixture(1, QUEUE_CAPACITY);
        gate blocker = {};
        std::atomic<int> ran = 0;

        static_cast<void>(fixture->schedule(blocker.job()));
        REQUIRE(testing::spin_until([&blocker] { return blocker.entered.load(); }));

        std::vector<std::function<void()>> jobs = {};
        for (int index = 0; index < 5; ++index) {
            jobs.emplace_back([&ran] {
                ran.fetch_add(1);
            });
        }
        const job_handle handle = fixture->schedule(std::span<const std::function<void()>>(jobs));

        // Two were queued behind the gate; the other three ran on this thread.
        CHECK(ran.load() == 3);
        CHECK(handle.pending() == 2);
        CHECK(fixture->stats().queueFullFallbacks >= 1);

        blocker.open.store(true, std::memory_order_release);
        fixture->wait(handle);
        CHECK(ran.load() == 5);
        CHECK(handle.is_complete());
    }

    /*-----------------------------------------------------------------------*/
    // Thread identity

    TEST_CASE("job_system - thread_index and current identify worker threads [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        constexpr int JOB_COUNT = 200;
        std::mutex observedMutex = {};
        std::set<u32> observedIndices = {};
        std::atomic<int> consistent = 0;
        std::atomic<int> onWorkers = 0;

        CHECK(job_system::thread_index() == 0);
        CHECK(job_system::current() == nullptr);

        for (int index = 0; index < JOB_COUNT; ++index) {
            static_cast<void>(system.schedule([&] {
                // A job runs either on a worker (index 1..N, current() == &system) or on the
                // main thread, inline or while helping in wait_idle() (index 0, current() null).
                const bool onWorker = job_system::thread_index() != 0;
                const bool currentIsSystem = job_system::current() == &system;
                if (onWorker == currentIsSystem) {
                    consistent.fetch_add(1);
                }
                if (onWorker) {
                    onWorkers.fetch_add(1);
                }
                // Keep workers busy long enough for several of them to take part.
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                const std::scoped_lock lock(observedMutex);
                observedIndices.insert(job_system::thread_index());
            }));
        }
        fixture->wait_idle();

        CHECK(consistent.load() == JOB_COUNT);
        CHECK(job_system::thread_index() == 0);
        CHECK(job_system::current() == nullptr);

        const std::scoped_lock lock(observedMutex);
        if (workers == 0) {
            CHECK(onWorkers.load() == 0);
            CHECK(observedIndices == std::set<u32>{0});
        } else {
            CHECK(onWorkers.load() > 0);
            CHECK(observedIndices.size() > 1);
            for (const u32 observed : observedIndices) {
                CHECK(observed <= workers);
            }
        }
    }

    TEST_CASE("job_system - workers are named after the thread name prefix") {
        job_system system = {};
        job_system_desc desc = {};
        desc.workerCount = 4;
        desc.threadNamePrefix = "test-pool";
        REQUIRE(system.start(desc).has_value());

        std::mutex namesMutex = {};
        std::set<std::string> names = {};
        for (int index = 0; index < 64; ++index) {
            static_cast<void>(system.schedule([&] {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                // Only workers carry the prefix; a job helped along by the main thread does not.
                if (job_system::thread_index() == 0) {
                    return;
                }
                const std::string name = jobs::detail::current_thread_name();
                const std::scoped_lock lock(namesMutex);
                names.insert(name);
            }));
        }
        system.wait_idle();
        system.stop();

        REQUIRE_FALSE(names.empty());
        for (const std::string &name : names) {
            CHECK(name.starts_with("test-pool-"));
            CHECK(name.size() > std::string("test-pool-").size());
        }
    }

    TEST_CASE("job_system - stop called from a worker asserts and is ignored") {
        testing::expect_assert asserts = {};
        testing::scoped_job_system fixture(1);
        job_system &system = fixture.system();
        std::atomic<bool> stillRunning = false;
        std::atomic<bool> done = false;

        const job_handle handle = system.schedule([&] {
            system.stop();
            stillRunning.store(system.is_running());
            done.store(true);
        });
        // Poll rather than wait(): waiting would let this thread help and run the job itself. The body
        // sets 'done' before it returns and the counter drops only after it has, so the handle is polled
        // too (the check right after 'done' raced the worker and failed once in 50 runs).
        REQUIRE(testing::spin_until([&done] { return done.load(); }));
        CHECK(testing::spin_until([&handle] { return handle.is_complete(); }));

        CHECK(asserts.count() == 1);
        CHECK(stillRunning.load());
        CHECK(system.is_running());
    }

    /*-----------------------------------------------------------------------*/
    // Statistics

    TEST_CASE("job_system - stats count scheduled and executed work [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr int JOB_COUNT = 100;

        for (int index = 0; index < JOB_COUNT; ++index) {
            static_cast<void>(fixture->schedule([] {}));
        }
        fixture->wait_idle();

        const job_system_stats stats = fixture->stats();
        CHECK(stats.scheduled == JOB_COUNT);
        CHECK(stats.executed == JOB_COUNT);
        CHECK(stats.queuedNow == 0);
        // Every job here was scheduled by this thread, so it was either stolen by a worker, helped
        // along in wait_idle() or executed inline.
        CHECK(stats.stolen + stats.helped + stats.inlineExecutions == stats.executed);
        if (workers == 0) {
            CHECK(stats.inlineExecutions == JOB_COUNT);
            CHECK(stats.maxQueueDepth == 0);
            CHECK(stats.poolCapacity > 0);
        } else {
            CHECK(stats.maxQueueDepth >= 1);
            CHECK(stats.maxQueueDepth <= JOB_COUNT);
            CHECK(stats.poolCapacity >= stats.maxQueueDepth);
        }
    }

    TEST_CASE("job_system - a job taken by a worker from another thread counts as stolen") {
        testing::scoped_job_system fixture(1);
        std::atomic<bool> ran = false;

        const job_handle handle = fixture->schedule([&ran] {
            ran.store(true);
        });
        // Poll instead of wait(): waiting would let this thread help and run the job itself.
        REQUIRE(testing::spin_until([&handle] { return handle.is_complete(); }));

        const job_system_stats stats = fixture->stats();
        CHECK(ran.load());
        CHECK(stats.executed == 1);
        CHECK(stats.stolen == 1);
        CHECK(stats.helped == 0);
        CHECK(stats.inlineExecutions == 0);
    }

    TEST_CASE("job_system - a job a worker queues and runs itself is neither stolen nor helped") {
        testing::scoped_job_system fixture(1);
        job_system &system = fixture.system();
        std::atomic<bool> followUpRan = false;

        static_cast<void>(system.schedule([&] {
            static_cast<void>(system.schedule([&followUpRan] {
                followUpRan.store(true);
            }));
        }));
        REQUIRE(testing::spin_until([&followUpRan] { return followUpRan.load(); }));
        REQUIRE(testing::spin_until([&system] { return system.stats().executed == 2; }));

        // Only the first job changed threads; the sum identity documented on job_system_stats
        // therefore does not hold here.
        const job_system_stats stats = fixture->stats();
        CHECK(stats.stolen == 1);
        CHECK(stats.helped == 0);
        CHECK(stats.inlineExecutions == 0);
        CHECK(stats.stolen + stats.helped + stats.inlineExecutions < stats.executed);
    }

    /*-----------------------------------------------------------------------*/
    // Concurrency

    TEST_CASE("job_system - many threads can schedule concurrently [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        job_system &system = fixture.system();
        constexpr int PRODUCER_COUNT = 8;
        constexpr int JOBS_PER_PRODUCER = 500;
        std::atomic<int> ran = 0;

        {
            std::vector<dedicated_thread> producers = {};
            for (int producer = 0; producer < PRODUCER_COUNT; ++producer) {
                producers.emplace_back("producer", [&](std::stop_token) {
                    for (int index = 0; index < JOBS_PER_PRODUCER; ++index) {
                        static_cast<void>(system.schedule([&ran] {
                            ran.fetch_add(1);
                        }));
                    }
                });
            }
        }
        fixture->wait_idle();

        CHECK(ran.load() == PRODUCER_COUNT * JOBS_PER_PRODUCER);
        CHECK(fixture->stats().executed == PRODUCER_COUNT * JOBS_PER_PRODUCER);
    }

    TEST_CASE("job_system - a handle can be waited on from several threads at once") {
        testing::scoped_job_system fixture(8);
        job_system &system = fixture.system();
        gate blocker = {};
        std::atomic<int> waitersDone = 0;

        const job_handle handle = system.schedule(blocker.job());
        REQUIRE(testing::spin_until([&blocker] { return blocker.entered.load(); }));
        {
            std::vector<dedicated_thread> waiters = {};
            for (int index = 0; index < 4; ++index) {
                waiters.emplace_back("waiter", [&](std::stop_token) {
                    system.wait(handle);
                    waitersDone.fetch_add(1);
                });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            CHECK(waitersDone.load() == 0);
            blocker.open.store(true, std::memory_order_release);
        }

        CHECK(waitersDone.load() == 4);
        CHECK(handle.is_complete());
    }

    TEST_CASE("job_system - a handle from one system can be waited on through another") {
        testing::scoped_job_system producer(2);
        testing::scoped_job_system observer(2);
        gate blocker = {};

        const job_handle handle = producer->schedule(blocker.job());
        REQUIRE(testing::spin_until([&blocker] { return blocker.entered.load(); }));

        dedicated_thread opener("opener", [&blocker](std::stop_token) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            blocker.open.store(true, std::memory_order_release);
        });
        observer->wait(handle);

        CHECK(handle.is_complete());
    }

    TEST_CASE("job_system - a worker of another system scheduling here counts with the external threads") {
        // Statistics are kept per thread, indexed by the worker's own index; a thread that is a worker of
        // a different system has a non-zero index there but is an external thread here, so its work
        // lands in slot 0 and the totals still add up. The outer job is polled, not waited for, so it
        // runs on one of the outer system's workers and not on this thread. The inner system has no
        // workers, so the batch runs inline on that outer worker and the inner totals sum slot 0 alone:
        // counting in the outer worker's slot 1 or 2 instead would leave both totals at zero.
        testing::scoped_job_system outer(2);
        testing::scoped_job_system inner(0);
        constexpr usize BATCH_SIZE = 4;
        std::atomic<usize> ran = 0;
        std::atomic<bool> ranOnOuterWorker = false;
        std::vector<std::function<void()>> jobs = {};
        for (usize index = 0; index < BATCH_SIZE; ++index) {
            jobs.emplace_back([&ran] {
                ran.fetch_add(1);
            });
        }

        const job_handle outerJob = outer->schedule([&] {
            ranOnOuterWorker.store(job_system::current() == &outer.system() && job_system::thread_index() != 0);
            const job_handle batch = inner->schedule(std::span<const std::function<void()>>(jobs));
            inner->wait(batch);
        });
        REQUIRE(testing::spin_until([&outerJob] { return outerJob.is_complete(); }));

        CHECK(ranOnOuterWorker.load());
        CHECK(ran.load() == BATCH_SIZE);
        const job_system_stats innerStats = inner->stats();
        CHECK(innerStats.scheduled == BATCH_SIZE);
        CHECK(innerStats.executed == BATCH_SIZE);
        CHECK(innerStats.inlineExecutions == BATCH_SIZE);
        const job_system_stats outerStats = outer->stats();
        CHECK(outerStats.scheduled == 1);
        CHECK(outerStats.executed == 1);
    }

    TEST_CASE("job_system - 100k tiny jobs complete [parallel]") {
        CONJURE_JOBS_WORKER_SUBCASES(workers);
        testing::scoped_job_system fixture(workers);
        constexpr int JOB_COUNT = 100000;
        std::atomic<i64> sum = 0;

        for (int index = 0; index < JOB_COUNT; ++index) {
            static_cast<void>(fixture->schedule([&sum, index] {
                sum.fetch_add(index, std::memory_order_relaxed);
            }));
        }
        fixture->wait_idle();

        constexpr i64 EXPECTED = static_cast<i64>(JOB_COUNT) * (JOB_COUNT - 1) / 2;
        CHECK(sum.load() == EXPECTED);
        const job_system_stats stats = fixture->stats();
        CHECK(stats.scheduled == JOB_COUNT);
        CHECK(stats.executed == JOB_COUNT);
        CHECK(stats.queuedNow == 0);
    }
}
