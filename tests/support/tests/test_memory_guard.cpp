#include "conjure/testing/doctest.h"

#include "conjure/testing/memory_guard.h"
#include "support_child_runner.h"

#include "conjure/core/error.h"
#include "conjure/core/memory/tracked_allocation.h"
#include "conjure/core/process.h"
#include "conjure/core/result.h"

#include <memory>
#include <vector>

using namespace conjure;
using namespace conjure::core::memory;
using namespace conjure::testing;

namespace {
    core::result<int> positive(int value) {
        if (value <= 0) {
            return core::make_error(core::error_code::INVALID_ARGUMENT, "{} is not positive", value);
        }
        return value;
    }
}

TEST_SUITE("testing/memory_guard") {

    /*-----------------------------------------------------------------------*/
    // In-process

    TEST_CASE("memory_guard - clean scope passes") {
        memory_guard guard = {};
        {
            std::vector<int> values = {};
            values.resize(1000);
            CHECK(values.size() == 1000);
        }
        CHECK(guard.live_delta() == 0);
        guard.check();
    }

    TEST_CASE("memory_guard - a live block shows as a positive delta until it is freed") {
        memory_guard guard = {};

        std::unique_ptr<int> block = std::make_unique<int>(7);
        CHECK(guard.live_delta() == 1);

        void *raw = tracked_allocate(64);
        REQUIRE(raw != nullptr);
        CHECK(guard.live_delta() == 2);

        tracked_free(raw, 64);
        block.reset();
        CHECK(guard.live_delta() == 0);
    }

    TEST_CASE("memory_guard - a freed block from before the baseline shows as a negative delta") {
        void *raw = tracked_allocate(16);
        REQUIRE(raw != nullptr);

        memory_guard guard = {};
        tracked_free(raw, 16);

        CHECK(guard.live_delta() == -1);
        guard.reset_baseline();
        CHECK(guard.live_delta() == 0);
    }

    TEST_CASE("memory_guard - reset_baseline accepts allocations kept alive on purpose") {
        std::unique_ptr<int> kept = nullptr;   // outlives the guard so the destructor check sees it
        memory_guard guard = {};
        kept = std::make_unique<int>(1);
        CHECK(guard.live_delta() == 1);

        guard.reset_baseline();

        CHECK(guard.live_delta() == 0);
        CHECK(guard.baseline().live_count() == snapshot().live_count());
    }

    TEST_CASE("memory_guard - the baseline is the snapshot at construction") {
        // Nothing allocates between the two statements (the constructor only copies the counters),
        // so the baseline is exactly the snapshot taken just before it.
        const memory_snapshot before = snapshot();
        const memory_guard guard = {};

        CHECK(guard.baseline() == before);
    }

    TEST_CASE("memory_guard - tolerates CHECK_MESSAGE and CONJURE_REQUIRE_OK inside the scope") {
        memory_guard guard = {};

        CHECK_MESSAGE(1 + 1 == 2, "arithmetic still works");
        {
            INFO("an INFO scope");
            CHECK(true);
        }
        CONJURE_REQUIRE_OK(positive(1));
        CONJURE_CHECK_OK(positive(2));
        CONJURE_CHECK_ERROR(positive(0), core::error_code::INVALID_ARGUMENT);
        for (int index = 0; index < 3; ++index) {
            CAPTURE(index);
            CHECK(index >= 0);
        }

        CHECK(guard.live_delta() == 0);
    }

    /*-----------------------------------------------------------------------*/
    // Through the child executable (the destructor fails the case; only visible from outside)

    TEST_CASE("memory_guard - a leaked block fails the case") {
        const core::process_result run = run_support_child(
          {"-tc=support_child - leaked allocation fails the memory guard", "--conjure-isolated"}
        );

        CHECK(run.exitCode != 0);
        CHECK(run.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(run.standardOutput, "memory_guard: 1 allocation(s) still live"));
    }

    TEST_CASE("memory_guard - a clean scope passes the case under isolation") {
        const core::process_result run = run_support_child(
          {"-tc=support_child - clean memory guard passes", "--conjure-isolated"}
        );

        CHECK(run.exitCode == 0);
        CHECK_FALSE(output_contains(run.standardOutput, "memory_guard:"));
    }

    TEST_CASE("memory_guard - doctest message macros inside the scope pass under isolation") {
        const core::process_result run = run_support_child(
          {"-tc=support_child - memory guard tolerates doctest message macros", "--conjure-isolated"}
        );

        CHECK(run.exitCode == 0);
        CHECK_FALSE(output_contains(run.standardOutput, "memory_guard:"));
    }

    TEST_CASE("memory_guard - a guard enclosing SUBCASEs reports doctest's bookkeeping as live") {
        // Pins the documented limitation (memory_guard.h): the per-case subcase bookkeeping is
        // retained until the case ends, so a guard around the SUBCASE boundaries fails.
        const core::process_result run = run_support_child(
          {"-tc=support_child - memory guard around subcases", "--conjure-isolated"}
        );

        CHECK(run.exitCode != 0);
        CHECK(run.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(run.standardOutput, "memory_guard:"));
        CHECK(output_contains(run.standardOutput, "allocation(s) still live"));
    }

    TEST_CASE("memory_guard - the documented SUBCASE patterns pass under isolation") {
        const core::process_result rebaselined = run_support_child(
          {"-tc=support_child - memory guard rebaselined after subcase selection", "--conjure-isolated"}
        );
        CHECK(rebaselined.exitCode == 0);
        CHECK_FALSE(output_contains(rebaselined.standardOutput, "memory_guard:"));

        const core::process_result inside = run_support_child(
          {"-tc=support_child - memory guard created inside the subcase", "--conjure-isolated"}
        );
        CHECK(inside.exitCode == 0);
        CHECK_FALSE(output_contains(inside.standardOutput, "memory_guard:"));
    }
}
