#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

using namespace conjure;
using namespace physics_test;

// The design's allocation gate at the design's body count (08_testing.md 7.1, physics.sync_10k: 10,000 bodies,
// 0 allocations): two scheduled steps over 10,000 boxes per job mode take about 5 s in Debug and far longer
// under the sanitizer legs, so the case is registered in conjure_physics_slow_tests (label slow, run by the
// nightly presets); tests/physics/test_component_sync.cpp keeps the same gate at 2,000 bodies in the default
// run and tests/benchmarks/bench_physics.cpp measures the 10,000-body figure.

TEST_SUITE("physics/component_sync_10k") {

    TEST_CASE(
      "component_sync - pull_transforms over 10k bodies allocates nothing after warm-up, 0 and 4 workers [slow]"
    ) {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            check_pull_allocation_free(jobs, 10000);
        });
    }
}
