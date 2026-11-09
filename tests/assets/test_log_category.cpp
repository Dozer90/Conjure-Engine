#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"

#include "conjure/assets/log_category.h"

#include <string_view>

using namespace conjure;
using namespace conjure::assets;

TEST_SUITE("assets/log_category") {

    TEST_CASE("LOG_ASSETS - is registered under the display name assets") {
        CHECK(std::string_view(LOG_ASSETS.name()) == "assets");
        CHECK(core::log::find_category("assets") == &LOG_ASSETS);
    }

    TEST_CASE("LOG_ASSETS - messages are delivered with the category name") {
        conjure::testing::log_capture logs = {};
        CONJURE_LOG_INFO(LOG_ASSETS, "cooked {} assets", 3);
        CHECK(logs.contains(core::log_level::INFO, "cooked 3 assets"));
        CHECK(logs.entries().back().category == "assets");
    }
}
