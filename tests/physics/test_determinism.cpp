#include "conjure/testing/doctest.h"
#include "determinism_scenario.h"
#include "physics_test_fixture.h"

#include <vector>

using namespace conjure;
using namespace physics_test;

// The 0-versus-4-workers case of this scenario ([parallel][slow], 7-10 s in Debug) lives in
// slow/test_determinism_parallel.cpp (conjure_physics_slow_tests, label slow), which the nightly presets run.

TEST_SUITE("physics/determinism") {

    TEST_CASE("determinism - two inline runs are bit-identical") {
        core::jobs::job_system first = {};
        core::jobs::job_system second = {};
        const std::vector<step_record> runA = run_determinism_scenario(first);
        const std::vector<step_record> runB = run_determinism_scenario(second);
        REQUIRE(runA.size() == runB.size());
        for (usize step = 0; step < runA.size(); ++step) {
            CAPTURE(step);
            CHECK(runA[step] == runB[step]);
        }
    }
}
