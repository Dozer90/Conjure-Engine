#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/log.h"
#include "conjure/ecs/log_category.h"

#include <string_view>

using namespace conjure;
using namespace conjure::ecs;

TEST_SUITE("ecs/log_category") {

    TEST_CASE("log_category - LOG_ECS is registered as ecs") {
        CHECK(std::string_view(LOG_ECS.name()) == "ecs");
        CHECK(core::log::find_category("ecs") == &LOG_ECS);
    }

    TEST_CASE("log_category - messages reach the sinks under the ecs name") {
        testing::log_capture logs = {};
        CONJURE_LOG_INFO(LOG_ECS, "world '{}' created", "test");
        CHECK(logs.contains(core::log_level::INFO, "world 'test' created"));
        const std::vector<testing::captured_log_entry> entries = logs.entries();
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].category == "ecs");
    }
}
