#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/log.h"
#include "conjure/engine/log_category.h"

#include <string_view>

// log_category.h: the engine's log category.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

TEST_SUITE("engine/log_category") {

    TEST_CASE("log_category - LOG_ENGINE is named 'engine' and routes messages") {
        CHECK(std::string_view(LOG_ENGINE.name()) == "engine");
        log_capture logs = {};
        CONJURE_LOG_INFO(LOG_ENGINE, "engine category probe {}", 7);
        CHECK(logs.contains(core::log_level::INFO, "engine category probe 7"));
        const std::vector<captured_log_entry> entries = logs.entries();
        REQUIRE_FALSE(entries.empty());
        CHECK(entries.back().category == "engine");
    }
}
