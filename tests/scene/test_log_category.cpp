#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/log.h"
#include "conjure/scene/log_category.h"

#include <string>

using namespace conjure;

TEST_SUITE("scene/log_category") {

    TEST_CASE("log_category - LOG_SCENE is registered as scene") {
        CHECK(std::string(scene::LOG_SCENE.name()) == "scene");
        CHECK(core::log::find_category("scene") == &scene::LOG_SCENE);
    }

    TEST_CASE("log_category - messages carry the scene category") {
        testing::log_capture logs = {};
        CONJURE_LOG_INFO(scene::LOG_SCENE, "hello {}", 42);
        CHECK(logs.contains(core::log_level::INFO, "hello 42"));
        const std::vector<testing::captured_log_entry> entries = logs.entries();
        REQUIRE(entries.size() == 1);
        CHECK(entries.front().category == "scene");
    }
}
