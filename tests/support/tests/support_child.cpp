// Cases run by conjure_support_tests in a child process (see tests/support/tests/CMakeLists.txt).
// Each is selected with -tc="<name>" and observed through its exit code and output; several fail on
// purpose, which is why this program is not registered with CTest. Direct writes to stdout are
// deliberate here: the parent reads them to see how far a case got.

#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"

#include <cstdio>
#include <memory>
#include <vector>

using namespace conjure;
using namespace conjure::testing;

namespace {
    core::result<int> positive(int value) {
        if (value <= 0) {
            return core::make_error(core::error_code::INVALID_ARGUMENT, "{} is not positive", value);
        }
        return value;
    }
}

TEST_SUITE("testing/support_child") {

    /*-----------------------------------------------------------------------*/
    // Preconditions and environment

    TEST_CASE("support_child - unmet precondition") {
        CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW);
        FAIL("REAL_WINDOW must not hold when CONJURE_TEST_WINDOW is unset");
    }

    TEST_CASE("support_child - met precondition runs the case") {
        CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW);
        CHECK(environment().realWindowAllowed);
    }

    TEST_CASE("support_child - downloaded assets precondition") {
        CONJURE_TEST_REQUIRE(precondition::DOWNLOADED_ASSETS);
        CHECK(precondition_holds(precondition::DOWNLOADED_ASSETS));
        CHECK_FALSE(environment().downloadedAssetRoot.empty());
    }

    TEST_CASE("support_child - environment reflects the arguments") {
        CHECK(environment().seed == 0x1234);
        CHECK(environment().jobThreadCount == 3);
        CHECK_FALSE(environment().isolatedProcess);
    }

    /*-----------------------------------------------------------------------*/
    // memory_guard

    TEST_CASE("support_child - leaked allocation fails the memory guard") {
        // Destroyed after the guard so the guard's destructor sees the live block; then freed.
        std::unique_ptr<int> leaked = nullptr;
        memory_guard guard = {};
        leaked = std::make_unique<int>(42);
        CHECK(*leaked == 42);
    }

    TEST_CASE("support_child - clean memory guard passes") {
        memory_guard guard = {};
        {
            std::vector<int> values = {};
            values.resize(1000);
            CHECK(values.size() == 1000);
        }
    }

    TEST_CASE("support_child - memory guard tolerates doctest message macros") {
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
        int sum = 0;
        CONJURE_CHECK_NO_ALLOCATIONS(sum = 1 + 2);
        CHECK(sum == 3);
    }

    TEST_CASE("support_child - memory guard around subcases") {
        // Encloses the SUBCASE boundaries: doctest's per-case bookkeeping is counted (memory_guard.h).
        memory_guard guard = {};
        int chosen = 0;
        SUBCASE("first") {
            chosen = 1;
        }
        SUBCASE("second") {
            chosen = 2;
        }
        CHECK(chosen != 0);
    }

    TEST_CASE("support_child - memory guard rebaselined after subcase selection") {
        memory_guard guard = {};
        int chosen = 0;
        SUBCASE("first") {
            chosen = 1;
        }
        SUBCASE("second") {
            chosen = 2;
        }
        guard.reset_baseline();

        std::vector<int> values(static_cast<usize>(chosen) * 16);
        CHECK(values.size() == static_cast<usize>(chosen) * 16);
        values.clear();
        values.shrink_to_fit();
    }

    TEST_CASE("support_child - memory guard created inside the subcase") {
        SUBCASE("first") {
            memory_guard guard = {};
            std::vector<int> values(16);
            CHECK(values.size() == 16);
            values.clear();
            values.shrink_to_fit();
        }
        SUBCASE("second") {
            memory_guard guard = {};
            std::vector<int> values(32);
            CHECK(values.size() == 32);
            values.clear();
            values.shrink_to_fit();
        }
    }

    /*-----------------------------------------------------------------------*/
    // log_capture

    TEST_CASE("support_child - unallowed error fails the log capture") {
        log_capture logs = {};
        CONJURE_LOG_ERROR(core::LOG_CORE, "deliberate error for the support self-test");
        CHECK(logs.count(core::log_level::ERR) == 1);
    }

    TEST_CASE("support_child - allowed error passes the log capture") {
        log_capture logs = {};
        logs.allow_errors();
        CONJURE_LOG_ERROR(core::LOG_CORE, "deliberate error for the support self-test");
        CHECK(logs.contains(core::log_level::ERR, "deliberate error"));
    }

    /*-----------------------------------------------------------------------*/
    // Asserts and allocation gates

    TEST_CASE("support_child - unexpected assert fails the case") {
        CONJURE_ASSERT(false, "deliberate assert for the support self-test");
        std::puts("reached after the assert");
        std::fflush(stdout);
        CHECK(true);
    }

    TEST_CASE("support_child - allocation inside CONJURE_CHECK_NO_ALLOCATIONS fails") {
        std::vector<int> values = {};
        CONJURE_CHECK_NO_ALLOCATIONS(values.resize(64));
        CHECK(values.size() == 64);
    }

    TEST_CASE("support_child - CONJURE_CHECK_ASSERTS with no assert fails") {
        int calls = 0;
        CONJURE_CHECK_ASSERTS(++calls);
        CHECK(calls == (CONJURE_ASSERTS_ENABLED ? 1 : 0));
    }

    TEST_CASE("support_child - CONJURE_CHECK_ASSERTS with two asserts fails") {
        int calls = 0;
        const auto violateTwice = [&calls] {
            ++calls;
            CONJURE_ASSERT(calls < 0, "first");
            CONJURE_ASSERT(calls < 0, "second");
        };
        CONJURE_CHECK_ASSERTS(violateTwice());
        CHECK(calls == (CONJURE_ASSERTS_ENABLED ? 1 : 0));
    }

    TEST_CASE("support_child - passes") {
        CHECK(true);
    }
}
