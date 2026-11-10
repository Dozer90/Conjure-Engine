#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/renderer/log_category.h"

#include "conjure/core/log.h"

#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

TEST_SUITE("renderer/log_category") {

    TEST_CASE("log_category - LOG_RENDERER is registered under the name renderer") {
        CHECK(std::string_view(LOG_RENDERER.name()) == "renderer");
        CHECK(core::log::find_category("renderer") == &LOG_RENDERER);
        const std::vector<core::log_category *> categories = core::log::categories();
        bool found = false;
        for (const core::log_category *category : categories) {
            found = found || category == &LOG_RENDERER;
        }
        CHECK(found);
    }

    TEST_CASE("log_category - messages written through LOG_RENDERER carry the renderer category") {
        log_capture logs = {};
        CONJURE_LOG_WARN(LOG_RENDERER, "mesh id {} is stale; drawing the fallback cube", 7);
        CHECK(logs.contains(core::log_level::WARN, "mesh id 7 is stale"));
        const std::vector<captured_log_entry> entries = logs.entries();
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].category == "renderer");
    }

    TEST_CASE("log_category - the category level filters renderer messages") {
        log_capture logs = {};
        const core::log_level previous = LOG_RENDERER.level();
        LOG_RENDERER.set_level(core::log_level::WARN);
        CONJURE_LOG_INFO(LOG_RENDERER, "filtered");
        CONJURE_LOG_WARN(LOG_RENDERER, "kept");
        LOG_RENDERER.set_level(previous);
        CHECK_FALSE(logs.contains("filtered"));
        CHECK(logs.contains(core::log_level::WARN, "kept"));
    }
}
