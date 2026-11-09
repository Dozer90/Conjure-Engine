#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/audio/log_category.h"

#include "conjure/core/log.h"

#include <string>

using namespace conjure;
using namespace conjure::audio;

TEST_SUITE("audio/log_category") {

    TEST_CASE("log_category - LOG_AUDIO is registered as \"audio\"") {
        CHECK(std::string(LOG_AUDIO.name()) == "audio");
        CHECK(core::log::find_category("audio") == &LOG_AUDIO);
    }

    TEST_CASE("log_category - messages logged through LOG_AUDIO carry the audio category") {
        testing::log_capture logs = {};
        CONJURE_LOG_INFO(LOG_AUDIO, "voice {} started", 42);
        CHECK(logs.contains(core::log_level::INFO, "voice 42 started"));
        const std::vector<testing::captured_log_entry> entries = logs.entries();
        REQUIRE_FALSE(entries.empty());
        CHECK(entries.back().category == "audio");
    }

    TEST_CASE("log_category - the category level filters audio messages") {
        testing::log_capture logs = {};
        const core::log_level previous = LOG_AUDIO.level();
        LOG_AUDIO.set_level(core::log_level::WARN);
        CONJURE_LOG_INFO(LOG_AUDIO, "filtered out");
        CONJURE_LOG_WARN(LOG_AUDIO, "passes through");
        LOG_AUDIO.set_level(previous);
        CHECK_FALSE(logs.contains("filtered out"));
        CHECK(logs.contains(core::log_level::WARN, "passes through"));
    }
}
