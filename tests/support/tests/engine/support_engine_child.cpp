// Cases run by conjure_support_engine_tests in a child process (see CMakeLists.txt here). Each is selected
// with -tc="<name>" and observed through its exit code and output: they fail on purpose, from a fixture's
// constructor, destructor or step_frames, which is why this program is not registered with CTest. The
// parent asserts that each failure is reported exactly once (headless_engine.h: a construction failure, an
// unallowed ERROR or late input must not drag a bogus memory_guard failure behind it).

#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/log_category.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/log.h"
#include "conjure/core/types.h"
#include "conjure/platform/button_action.h"
#include "conjure/platform/key.h"

#include <filesystem>
#include <memory>

using namespace conjure;
using namespace conjure::testing;

namespace {
    [[nodiscard]] headless_engine_options quiet_options() {
        headless_engine_options options = {};
        options.enableRenderer = false;
        options.enableAudio = false;
        return options;
    }
}

TEST_SUITE("testing/support_engine_child") {

    TEST_CASE("support_engine_child - an invalid fixed delta fails construction") {
        headless_engine_options options = quiet_options();
        options.fixedDeltaSeconds = 0.0;
        headless_engine engine(options);
        FAIL("the fixture must not construct with a zero fixed delta");
    }

    TEST_CASE("support_engine_child - a missing project directory fails construction") {
        temp_directory scratch("support_engine_child_project");
        const std::filesystem::path absent = scratch.file("absent_project");
        headless_engine_options options = quiet_options();
        options.projectDirectory = &absent;
        headless_engine engine(options);
        FAIL("the fixture must not construct without a project");
    }

    TEST_CASE("support_engine_child - an unallowed error logged during the run fails the case") {
        headless_engine engine(quiet_options());
        CONJURE_LOG_ERROR(LOG_TESTING, "deliberate error text from the child");
        engine.step_frames(1);
        CHECK(engine.frame_index() == 1);
    }

    TEST_CASE("support_engine_child - input scheduled for a frame that already ran fails the case") {
        headless_engine engine(quiet_options());
        engine.step_frames(3);
        engine.input().key(1, platform::key::W, platform::button_action::PRESS);   // meant as "one frame on"
        engine.step_frames(1);
        engine.step_frames(1);   // the late event was reported once and is not reported again
        CHECK(engine.frame_index() == 5);
    }

    TEST_CASE("support_engine_child - a leak during the run is reported with the interning budget") {
        // Destroyed after the fixture so the memory check sees the live block; then freed.
        std::unique_ptr<int> leaked = nullptr;
        headless_engine engine(quiet_options());
        leaked = std::make_unique<int>(42);
        CHECK(*leaked == 42);
    }
}
