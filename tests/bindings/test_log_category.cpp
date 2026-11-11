#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/bindings/log_category.h"
#include "conjure/core/log.h"

#include <string>

// log_category.h: the "bindings" category exists, is registered under its display name and carries messages.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;

TEST_SUITE("bindings/log_category") {

    TEST_CASE("log_category - LOG_BINDINGS is registered as \"bindings\"") {
        CHECK(std::string(LOG_BINDINGS.name()) == "bindings");
        CHECK(core::log::find_category("bindings") == &LOG_BINDINGS);
    }

    TEST_CASE("log_category - messages reach the sinks under the bindings category") {
        log_capture logs = {};
        CONJURE_LOG_INFO(LOG_BINDINGS, "table filled with {} entries", 126);
        CHECK(logs.contains(core::log_level::INFO, "table filled with 126 entries"));
        REQUIRE(logs.count() == 1);
        CHECK(logs.entries()[0].category == "bindings");
    }
}
