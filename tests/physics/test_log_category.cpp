#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/log.h"
#include "conjure/physics/log_category.h"

#include <string>
#include <vector>

using namespace conjure;

TEST_SUITE("physics/log_category") {

    TEST_CASE("log_category - LOG_PHYSICS is registered as physics") {
        CHECK(std::string(physics::LOG_PHYSICS.name()) == "physics");
        CHECK(core::log::find_category("physics") == &physics::LOG_PHYSICS);
    }

    TEST_CASE("log_category - messages carry the physics category") {
        testing::log_capture logs = {};
        CONJURE_LOG_INFO(physics::LOG_PHYSICS, "bodies {}", 7);
        CHECK(logs.contains(core::log_level::INFO, "bodies 7"));
        const std::vector<testing::captured_log_entry> entries = logs.entries();
        REQUIRE(entries.size() == 1);
        CHECK(entries.front().category == "physics");
    }
}
