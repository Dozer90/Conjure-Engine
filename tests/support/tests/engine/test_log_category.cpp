#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/log_category.h"

#include "conjure/core/log.h"

#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::testing;

TEST_SUITE("testing/log_category") {

    TEST_CASE("LOG_TESTING - messages are captured under the testing category") {
        log_capture logs = {};

        CONJURE_LOG_WARN(LOG_TESTING, "fixture decision {}", 42);

        CHECK(logs.contains(core::log_level::WARN, "fixture decision 42"));
        const std::vector<captured_log_entry> entries = logs.entries();
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].category == "testing");
        CHECK(entries[0].level == core::log_level::WARN);
    }

    TEST_CASE("LOG_TESTING - the category is named after the library") {
        CHECK(std::string_view(LOG_TESTING.name()) == "testing");
    }
}
