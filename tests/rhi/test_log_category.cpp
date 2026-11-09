#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/rhi/log_category.h"

#include "conjure/core/log.h"

#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;

TEST_SUITE("rhi/log_category") {

    TEST_CASE("log_category - LOG_RHI is registered under the name rhi") {
        CHECK(std::string_view(LOG_RHI.name()) == "rhi");
        CHECK(core::log::find_category("rhi") == &LOG_RHI);
        const std::vector<core::log_category *> categories = core::log::categories();
        bool found = false;
        for (const core::log_category *category : categories) {
            found = found || category == &LOG_RHI;
        }
        CHECK(found);
    }

    TEST_CASE("log_category - messages written through LOG_RHI carry the rhi category") {
        log_capture logs = {};
        CONJURE_LOG_INFO(LOG_RHI, "device {} ready", 7);
        CHECK(logs.contains(core::log_level::INFO, "device 7 ready"));
        const std::vector<captured_log_entry> entries = logs.entries();
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].category == "rhi");
    }

    TEST_CASE("log_category - the category level filters rhi messages") {
        log_capture logs = {};
        const core::log_level previous = LOG_RHI.level();
        LOG_RHI.set_level(core::log_level::WARN);
        CONJURE_LOG_INFO(LOG_RHI, "filtered");
        CONJURE_LOG_WARN(LOG_RHI, "kept");
        LOG_RHI.set_level(previous);
        CHECK_FALSE(logs.contains("filtered"));
        CHECK(logs.contains(core::log_level::WARN, "kept"));
    }
}
