#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/platform/log_category.h"

#include "conjure/core/log.h"

using namespace conjure;
using namespace conjure::core;
using namespace conjure::platform;

TEST_SUITE("platform/log_category") {

    TEST_CASE("log_category - LOG_PLATFORM is registered under the name platform") {
        CHECK(std::string_view(LOG_PLATFORM.name()) == "platform");
        CHECK(log::find_category("platform") == &LOG_PLATFORM);
    }

    TEST_CASE("log_category - messages logged through LOG_PLATFORM carry the platform category") {
        testing::log_capture capture = {};

        CONJURE_LOG_INFO(LOG_PLATFORM, "window {} x {}", 1280, 720);

        REQUIRE(capture.count() == 1);
        CHECK(capture.entries().front().category == "platform");
        CHECK(capture.entries().front().text == "window 1280 x 720");
    }

    TEST_CASE("log_category - a level change is visible through the category and by name") {
        testing::log_capture capture = {};
        const log_level previous = LOG_PLATFORM.level();

        LOG_PLATFORM.set_level(log_level::WARN);
        CHECK(LOG_PLATFORM.level() == log_level::WARN);
        CHECK_FALSE(LOG_PLATFORM.is_enabled(log_level::INFO));
        CHECK(LOG_PLATFORM.is_enabled(log_level::WARN));
        CONJURE_LOG_INFO(LOG_PLATFORM, "filtered");
        CHECK(capture.count() == 0);

        CHECK(log::set_category_level("platform", log_level::TRACE));
        CHECK(LOG_PLATFORM.level() == log_level::TRACE);
        CONJURE_LOG_INFO(LOG_PLATFORM, "visible");
        CHECK(capture.count() == 1);

        LOG_PLATFORM.set_level(previous);
    }
}
