#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "support_child_runner.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/memory/arena.h"
#include "conjure/core/process.h"

#include <array>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

TEST_SUITE("testing/allocation_scope") {

    TEST_CASE("allocation_scope - counts std::vector growth") {
        std::vector<int> values = {};
        allocation_scope scope = {};

        values.reserve(8);
        CHECK(scope.allocation_count() == 1);
        CHECK(scope.allocated_bytes() >= 8 * sizeof(int));

        for (int index = 0; index < 8; ++index) {
            values.push_back(index);
        }
        CHECK(scope.allocation_count() == 1);

        values.push_back(8);
        CHECK(scope.allocation_count() == 2);
        CHECK(scope.free_count() == 1);
    }

    TEST_CASE("allocation_scope - zero for arena allocations inside the first block") {
        memory::arena arena(4 * KIB);
        // The first block is acquired lazily; take it before the scope starts counting.
        CHECK(arena.allocate(16, 16) != nullptr);
        allocation_scope scope = {};

        for (int index = 0; index < 16; ++index) {
            void *block = arena.allocate(32, 16);
            CHECK(block != nullptr);
        }

        CHECK(scope.allocation_count() == 0);
        CHECK(scope.allocated_bytes() == 0);
    }

    TEST_CASE("allocation_scope - zero for stack-only work") {
        allocation_scope scope = {};

        std::array<int, 64> values = {};
        for (usize index = 0; index < values.size(); ++index) {
            values[index] = static_cast<int>(index * 2);
        }

        CHECK(values[63] == 126);
        CHECK(scope.allocation_count() == 0);
    }

    TEST_CASE("allocation_scope - reset restarts counting") {
        allocation_scope scope = {};
        std::vector<int> values(100);
        CHECK(scope.allocation_count() >= 1);

        scope.reset();

        CHECK(scope.allocation_count() == 0);
        CHECK(scope.free_count() == 0);
        CHECK(scope.allocated_bytes() == 0);
        CHECK(values.size() == 100);
    }

    TEST_CASE("allocation_scope - counts allocations made on job threads") {
        jobs::job_system system = {};
        jobs::job_system_desc desc = {};
        desc.workerCount = 2;
        REQUIRE(system.start(desc).has_value());

        allocation_scope scope = {};
        const jobs::job_handle handle = system.schedule([] {
            std::vector<int> onWorker(4096);
            CHECK(onWorker.size() == 4096);
        });
        system.wait(handle);

        CHECK(scope.allocation_count() >= 1);
        system.stop();
    }

    TEST_CASE("CONJURE_CHECK_NO_ALLOCATIONS - passes for an allocation-free statement") {
        int sum = 0;
        CONJURE_CHECK_NO_ALLOCATIONS(sum = 1 + 2);
        CHECK(sum == 3);
    }

    TEST_CASE("CONJURE_CHECK_NO_ALLOCATIONS - an allocating statement fails the case") {
        const process_result run = run_support_child(
          {"-tc=support_child - allocation inside CONJURE_CHECK_NO_ALLOCATIONS fails", "--conjure-isolated"}
        );

        CHECK(run.exitCode != 0);
        CHECK(run.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(run.standardOutput, "expected no allocations from: values.resize(64)"));
    }
}
