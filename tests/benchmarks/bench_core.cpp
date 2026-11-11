// Core scenario (docs/design/08_testing.md 7.1 "core.job_parallel_for_1m", 10_core.md jobs): one
// job_system::parallel_for over 1,000,000 trivial items (an increment per item) with the default grain of
// the per-index helpers, so the dispatch and join cost of the job system over the benchmark worker count
// is measured with next to no work per range. The body is a std::function built once outside the loop,
// like the engine's callers; the block pool warms up during the warm-up iterations, after which the
// dispatch allocates nothing (job_system.h parallel_for).

#include "conjure/bench/benchmark.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/parallel_for.h"
#include "conjure/core/types.h"

#include <functional>
#include <vector>

using namespace conjure;
using namespace conjure::bench;

namespace {

    constexpr usize ITEM_COUNT = 1'000'000;
}

CONJURE_BENCHMARK(core.job_parallel_for_1m) {
    core::jobs::job_system &jobs = context.jobs();
    std::vector<u32> items(ITEM_COUNT, 0u);
    const usize grain = core::jobs::default_grain(ITEM_COUNT, jobs.worker_count());
    const std::function<void(usize, usize)> body = [&items](usize begin, usize end) {
        for (usize index = begin; index < end; ++index) {
            items[index] += 1u;
        }
    };

    context.set_items(ITEM_COUNT);
    context.measure([&]() {
        jobs.parallel_for(ITEM_COUNT, grain, body);
    });
}
