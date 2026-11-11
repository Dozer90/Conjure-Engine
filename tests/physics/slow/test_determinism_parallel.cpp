#include "conjure/testing/doctest.h"
#include "determinism_scenario.h"
#include "physics_test_fixture.h"

#include <vector>

using namespace conjure;
using namespace physics_test;

// The [slow] half of tests/physics/test_determinism.cpp: the scenario run once inline and once over four
// workers (two 300-step simulations of 200 boxes, 7-10 s in Debug), registered in conjure_physics_slow_tests
// with the label slow so the default test presets skip it and the nightly presets run it.

TEST_SUITE("physics/determinism_parallel") {

    TEST_CASE("determinism - 200 boxes over 300 steps are bit-identical with 0 and 4 workers [parallel][slow]") {
        core::jobs::job_system inlineJobs = {};
        const std::vector<step_record> inlineRun = run_determinism_scenario(inlineJobs);

        core::jobs::job_system parallelJobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 4;
        REQUIRE(parallelJobs.start(desc).has_value());
        const std::vector<step_record> parallelRun = run_determinism_scenario(parallelJobs);
        parallelJobs.stop();

        REQUIRE(inlineRun.size() == parallelRun.size());
        u32 eventsSeen = 0;
        for (usize step = 0; step < inlineRun.size(); ++step) {
            CAPTURE(step);
            CHECK(inlineRun[step] == parallelRun[step]);
            eventsSeen += static_cast<u32>(inlineRun[step].events.size());
        }
        CHECK(eventsSeen > 0);
        // The boxes did land on something: the last step still has resting contacts.
        CHECK_FALSE(inlineRun.back().events.empty());
    }
}
