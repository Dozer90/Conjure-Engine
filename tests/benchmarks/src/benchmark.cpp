#include "conjure/testing/allocation_scope.h"

#include "conjure/bench/benchmark.h"
#include "conjure/core/assert.h"
#include "conjure/core/clock.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <utility>

using namespace conjure;
using namespace conjure::bench;

namespace {

    constexpr u32 MIN_AUTO_THREADS = 1;
    constexpr u32 MAX_AUTO_THREADS = 16;
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

std::string_view conjure::bench::to_string(machine_class value) noexcept {
    return value == machine_class::CI ? "ci" : "desktop";
}

std::string_view conjure::bench::to_string(bench_status value) noexcept {
    switch (value) {
        case bench_status::PASSED:
            return "PASSED";
        case bench_status::FAILED:
            return "FAILED";
        case bench_status::UNTHRESHOLDED:
            return "UNTHRESHOLDED";
        case bench_status::SKIPPED:
            return "SKIPPED";
    }
    return "UNKNOWN";
}

f64 conjure::bench::percentile(std::span<const f64> sortedSamples, f64 quantile) noexcept {
    if (sortedSamples.empty()) {
        return 0.0;
    }
    const f64 clamped = std::clamp(quantile, 0.0, 1.0);
    const f64 position = std::floor(clamped * static_cast<f64>(sortedSamples.size() - 1));
    const usize index = static_cast<usize>(position);
    return sortedSamples[std::min(index, sortedSamples.size() - 1)];
}

bench_statistics conjure::bench::compute_statistics(std::span<const f64> samples) {
    bench_statistics statistics = {};
    if (samples.empty()) {
        return statistics;
    }
    std::vector<f64> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    statistics.minMilliseconds = sorted.front();
    statistics.maxMilliseconds = sorted.back();
    statistics.p50Milliseconds = percentile(sorted, 0.50);
    statistics.p95Milliseconds = percentile(sorted, 0.95);
    statistics.p99Milliseconds = percentile(sorted, 0.99);
    return statistics;
}

u32 conjure::bench::resolve_job_thread_count(u32 requested, u32 hardwareConcurrency) noexcept {
    if (requested != AUTO_JOB_THREAD_COUNT) {
        return requested;
    }
    const u32 available = hardwareConcurrency == 0 ? MIN_AUTO_THREADS + 1 : hardwareConcurrency;
    return std::clamp<u32>(available - 1, MIN_AUTO_THREADS, MAX_AUTO_THREADS);
}


/*===========================================================================*/
/*===========================================================================*/
// bench_context

bench_context::bench_context(const bench_options &options) :
    mOptions(options),
    mJobThreadCount(resolve_job_thread_count(options.jobThreadCount, std::thread::hardware_concurrency())) {}

bench_context::~bench_context() {
    if (mJobs != nullptr) {
        mJobs->stop();
    }
}

void bench_context::add_phase(std::string_view name, std::span<const f64> samplesMilliseconds) {
    bench_phase_result phase = {};
    phase.name = std::string(name);
    phase.statistics = compute_statistics(samplesMilliseconds);
    mPhases.push_back(std::move(phase));
}

void bench_context::skip(std::string_view reason) {
    mSkipped = true;
    mSkipReason = std::string(reason);
}

void bench_context::fail(std::string_view reason) {
    if (mFailed) {
        return;
    }
    mFailed = true;
    mFailureReason = std::string(reason);
}

core::jobs::job_system &bench_context::jobs() {
    if (mJobs == nullptr) {
        mJobs = std::make_unique<core::jobs::job_system>();
        core::jobs::job_system_desc desc = {};
        desc.workerCount = mJobThreadCount;
        desc.threadNamePrefix = "conjure-bench";
        const core::result<void> started = mJobs->start(desc);
        CONJURE_ASSERT(started.has_value(), "the benchmark job system failed to start");
    }
    return *mJobs;
}

void bench_context::measure_impl(body_fn invoke, void *body) {
    if (mSkipped) {
        return;
    }

    for (u32 iteration = 0; iteration < mOptions.warmupIterations; ++iteration) {
        invoke(body);
    }

    // Everything the loop touches is reserved before the first timed call so the measurement itself
    // never shows up in the allocation count.
    mSamplesMilliseconds.clear();
    mSamplesMilliseconds.reserve(mOptions.measuredIterations);
    mMaxAllocationsPerIteration = 0;
    testing::allocation_scope allocations = {};
    for (u32 iteration = 0; iteration < mOptions.measuredIterations; ++iteration) {
        allocations.reset();
        const u64 start = core::ticks_now();
        invoke(body);
        const u64 end = core::ticks_now();
        const u64 allocated = allocations.allocation_count();
        mMaxAllocationsPerIteration = std::max(mMaxAllocationsPerIteration, allocated);
        mSamplesMilliseconds.push_back(core::ticks_to_seconds(end - start) * 1000.0);
    }
    mMeasured = true;
}


/*===========================================================================*/
/*===========================================================================*/
// bench_registry

bench_registry::bench_registry() {}

bench_registry::~bench_registry() {}

bench_registry &bench_registry::instance() {
    static bench_registry sRegistry = {};
    return sRegistry;
}

void bench_registry::add(const bench_descriptor &descriptor) {
    if (descriptor.function == nullptr || descriptor.name.empty()) {
        CONJURE_ASSERT(false, "bench_registry::add: a scenario needs a name and a function");
        return;
    }
    if (find(descriptor.name) != nullptr) {
        CONJURE_ASSERT(false, "bench_registry::add: scenario '{}' is registered twice", descriptor.name);
        return;
    }
    const auto position = std::lower_bound(
      mDescriptors.begin(),
      mDescriptors.end(),
      descriptor,
      [](const bench_descriptor &left, const bench_descriptor &right) {
          return left.name < right.name;
      }
    );
    mDescriptors.insert(position, descriptor);
}

const bench_descriptor *bench_registry::find(std::string_view name) const noexcept {
    for (const bench_descriptor &descriptor : mDescriptors) {
        if (descriptor.name == name) {
            return &descriptor;
        }
    }
    return nullptr;
}

void bench_registry::clear() noexcept {
    mDescriptors.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// detail::bench_registrar

conjure::bench::detail::bench_registrar::bench_registrar(std::string_view name, bench_function function) {
    bench_registry::instance().add(bench_descriptor{name, function});
}
