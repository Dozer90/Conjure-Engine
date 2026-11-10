#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/log.h"
#include "conjure/scripting/log_category.h"

#include <string>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;

TEST_SUITE("scripting/log_category") {

    TEST_CASE("LOG_SCRIPTING - is registered under the name scripting") {
        CHECK(std::string(LOG_SCRIPTING.name()) == "scripting");
        CHECK(core::log::find_category("scripting") == &LOG_SCRIPTING);
    }

    TEST_CASE("LOG_SCRIPTING - messages reach the sinks with the category name") {
        log_capture logs = {};
        CONJURE_LOG_INFO(LOG_SCRIPTING, "hello {}", 42);
        CHECK(logs.contains(core::log_level::INFO, "hello 42"));
        REQUIRE(logs.entries().size() == 1);
        CHECK(logs.entries()[0].category == "scripting");
    }
}
