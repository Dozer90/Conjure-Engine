#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/test_environment.h"

#include "conjure/bench/benchmark.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

// The harness core (docs/design/08_testing.md 2.4, 3.6, 4.7, 11): percentiles, the measure loop with its
// exact allocation count, the context's items, phases, skip and job system, the registry and the
// CONJURE_BENCHMARK registration macro.

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::testing;

namespace {

    [[nodiscard]] bench_options quick_options(u32 warmup = 3, u32 measured = 10) {
        bench_options options = {};
        options.warmupIterations = warmup;
        options.measuredIterations = measured;
        options.jobThreadCount = 2;
        return options;
    }

    std::atomic<u32> sMacroRuns = 0;
}

// Registered at static initialisation like every scenario file does.
CONJURE_BENCHMARK(test.macro_registered) {
    context.set_items(7);
    ++sMacroRuns;
}

TEST_SUITE("benchmarks/benchmark") {

    TEST_CASE("percentile - selects samples[floor(q * (n - 1))] of a sorted set") {
        const std::vector<f64> sorted = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
        CHECK(percentile(sorted, 0.0) == 1.0);
        CHECK(percentile(sorted, 0.5) == 5.0);     // floor(0.5 * 9) = 4
        CHECK(percentile(sorted, 0.95) == 9.0);    // floor(0.95 * 9) = 8
        CHECK(percentile(sorted, 0.99) == 9.0);    // floor(0.99 * 9) = 8
        CHECK(percentile(sorted, 1.0) == 10.0);
        CHECK(percentile(sorted, 2.0) == 10.0);    // clamped
        CHECK(percentile(sorted, -1.0) == 1.0);
        CHECK(percentile({}, 0.5) == 0.0);
        const std::vector<f64> single = {4.5};
        CHECK(percentile(single, 0.99) == 4.5);
    }

    TEST_CASE("compute_statistics - sorts a copy and fills min p50 p95 p99 max") {
        const std::vector<f64> samples = {5.0, 1.0, 3.0, 2.0, 4.0};
        const bench_statistics statistics = compute_statistics(samples);
        CHECK(statistics.minMilliseconds == 1.0);
        CHECK(statistics.p50Milliseconds == 3.0);     // floor(0.5 * 4) = 2
        CHECK(statistics.p95Milliseconds == 4.0);     // floor(0.95 * 4) = 3
        CHECK(statistics.p99Milliseconds == 4.0);
        CHECK(statistics.maxMilliseconds == 5.0);
        CHECK(samples[0] == 5.0);                     // the input is untouched
        CHECK(compute_statistics({}) == bench_statistics{});
    }

    TEST_CASE("resolve_job_thread_count - a request wins, 0 stays inline and AUTO is hardware minus one clamped") {
        CHECK(resolve_job_thread_count(4, 32) == 4);
        CHECK(resolve_job_thread_count(0, 32) == 0);
        CHECK(resolve_job_thread_count(AUTO_JOB_THREAD_COUNT, 8) == 7);
        CHECK(resolve_job_thread_count(AUTO_JOB_THREAD_COUNT, 1) == 1);
        CHECK(resolve_job_thread_count(AUTO_JOB_THREAD_COUNT, 0) == 1);
        CHECK(resolve_job_thread_count(AUTO_JOB_THREAD_COUNT, 64) == 16);
        CHECK(bench_options{}.jobThreadCount == AUTO_JOB_THREAD_COUNT);
    }

    TEST_CASE("to_string - machine classes and statuses have stable names") {
        CHECK(to_string(machine_class::DESKTOP) == "desktop");
        CHECK(to_string(machine_class::CI) == "ci");
        CHECK(to_string(bench_status::PASSED) == "PASSED");
        CHECK(to_string(bench_status::FAILED) == "FAILED");
        CHECK(to_string(bench_status::UNTHRESHOLDED) == "UNTHRESHOLDED");
        CHECK(to_string(bench_status::SKIPPED) == "SKIPPED");
    }

    TEST_CASE("bench_context - measure runs the warm-up untimed and keeps one sample per measured iteration") {
        bench_context context(quick_options(3, 10));
        u32 calls = 0;
        context.measure([&calls]() {
            ++calls;
        });
        CHECK(calls == 13);
        CHECK(context.samples_milliseconds().size() == 10);
        CHECK(context.is_measured());
        for (const f64 sample : context.samples_milliseconds()) {
            CHECK(sample >= 0.0);
        }
        CHECK(context.max_allocations_per_iteration() == 0);
    }

    TEST_CASE("bench_context - the allocation count is the exact maximum over the measured iterations") {
        bench_context context(quick_options(2, 6));
        u32 iteration = 0;
        context.measure([&iteration]() {
            ++iteration;
            // The warm-up iterations allocate the most; they must not count. Iteration 5 of the measured
            // run allocates three blocks, the others one.
            const u32 blocks = iteration <= 2 ? 10u : (iteration == 7 ? 3u : 1u);
            std::vector<std::unique_ptr<u32>> kept = {};
            for (u32 index = 0; index < blocks; ++index) {
                kept.push_back(std::make_unique<u32>(index));
            }
        });
        // Every iteration also grows the vector's storage; the exact figure is blocks plus growth.
        CHECK(context.max_allocations_per_iteration() >= 3);
        CHECK(context.max_allocations_per_iteration() < 10);
        CHECK(context.samples_milliseconds().size() == 6);
    }

    TEST_CASE("bench_context - a second measure replaces the samples") {
        bench_context context(quick_options(0, 4));
        context.measure([]() {});
        CHECK(context.samples_milliseconds().size() == 4);
        context.measure([]() {});
        CHECK(context.samples_milliseconds().size() == 4);
    }

    TEST_CASE("bench_context - skip prevents the measure loop from running") {
        bench_context context(quick_options());
        context.skip("no runtime");
        u32 calls = 0;
        context.measure([&calls]() {
            ++calls;
        });
        CHECK(calls == 0);
        CHECK(context.is_skipped());
        CHECK(context.skip_reason() == "no runtime");
        CHECK_FALSE(context.is_measured());
        CHECK(context.samples_milliseconds().empty());
    }

    TEST_CASE("bench_context - fail keeps the samples, records the first reason and never skips") {
        bench_context context(quick_options(0, 4));
        CHECK_FALSE(context.is_failed());
        CHECK(context.failure_reason().empty());
        context.measure([]() {});
        context.fail("a measured frame returned an error");
        context.fail("later reasons are ignored");
        CHECK(context.is_failed());
        CHECK(context.failure_reason() == "a measured frame returned an error");
        CHECK(context.is_measured());
        CHECK_FALSE(context.is_skipped());
        CHECK(context.samples_milliseconds().size() == 4);
    }

    TEST_CASE("bench_context - items, phases, seed and options are reported back") {
        bench_options options = quick_options();
        options.entityCount = 123;
        options.seed = 99;
        bench_context context(options);
        CHECK(context.entity_count() == 123);
        CHECK(context.seed() == 99);
        CHECK(context.options().measuredIterations == 10);
        context.set_items(42);
        CHECK(context.items_per_iteration() == 42);
        const std::vector<f64> phaseSamples = {3.0, 1.0, 2.0};
        context.add_phase("UPDATE", phaseSamples);
        context.add_phase("TRANSFORM", {});
        REQUIRE(context.phases().size() == 2);
        CHECK(context.phases()[0].name == "UPDATE");
        CHECK(context.phases()[0].statistics.minMilliseconds == 1.0);
        CHECK(context.phases()[0].statistics.p50Milliseconds == 2.0);
        CHECK(context.phases()[0].statistics.maxMilliseconds == 3.0);
        CHECK(context.phases()[1].statistics == bench_statistics{});
    }

    TEST_CASE("bench_context - the job system starts lazily with the resolved worker count [parallel]") {
        bench_options options = quick_options();
        options.jobThreadCount = 3;
        bench_context context(options);
        CHECK(context.job_thread_count() == 3);
        core::jobs::job_system &jobs = context.jobs();
        CHECK(jobs.is_running());
        CHECK(jobs.worker_count() == 3);
        CHECK(&context.jobs() == &jobs);
        std::atomic<u32> visited = 0;
        jobs.parallel_for(1000, [&visited](usize) {
            ++visited;
        });
        CHECK(visited == 1000);
    }

    TEST_CASE("bench_context - a thread count of 0 starts an inline job system like jobs_settings") {
        bench_options options = quick_options();
        options.jobThreadCount = 0;
        bench_context context(options);
        CHECK(context.job_thread_count() == 0);
        core::jobs::job_system &jobs = context.jobs();
        CHECK(jobs.is_running());
        CHECK(jobs.worker_count() == 0);
        u32 visited = 0;
        jobs.parallel_for(100, [&visited](usize) {
            ++visited;
        });
        CHECK(visited == 100);
        CHECK(jobs.stats().inlineExecutions >= 1);
    }

    TEST_CASE("bench_registry - add keeps the descriptors sorted and find is exact") {
        bench_registry registry = {};
        const auto first = [](bench_context &) {};
        const auto second = [](bench_context &) {};
        registry.add(bench_descriptor{"scene.load", first});
        registry.add(bench_descriptor{"ecs.iterate", second});
        REQUIRE(registry.descriptors().size() == 2);
        CHECK(registry.descriptors()[0].name == "ecs.iterate");
        CHECK(registry.descriptors()[1].name == "scene.load");
        REQUIRE(registry.find("ecs.iterate") != nullptr);
        CHECK(registry.find("ecs.iterate")->function == static_cast<bench_function>(second));
        CHECK(registry.find("ecs") == nullptr);
        CHECK(registry.find("") == nullptr);
        registry.clear();
        CHECK(registry.descriptors().empty());
    }

    TEST_CASE("bench_registry - a duplicate name, an empty name and a null function assert and are ignored") {
        bench_registry registry = {};
        const auto body = [](bench_context &) {};
        registry.add(bench_descriptor{"ecs.iterate", body});
        CONJURE_CHECK_ASSERTS(registry.add(bench_descriptor{"ecs.iterate", body}));
        CONJURE_CHECK_ASSERTS(registry.add(bench_descriptor{"", body}));
        CONJURE_CHECK_ASSERTS(registry.add(bench_descriptor{"ecs.other", nullptr}));
        CHECK(registry.descriptors().size() == 1);
    }

    TEST_CASE("CONJURE_BENCHMARK - registers the scenario with the process-wide registry and it runs") {
        const bench_descriptor *registered = bench_registry::instance().find("test.macro_registered");
        REQUIRE(registered != nullptr);
        REQUIRE(registered->function != nullptr);
        bench_context context(quick_options());
        const u32 before = sMacroRuns.load();
        registered->function(context);
        CHECK(sMacroRuns.load() == before + 1);
        CHECK(context.items_per_iteration() == 7);
    }
}
