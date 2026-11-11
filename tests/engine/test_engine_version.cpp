#include "conjure/testing/doctest.h"

#include "conjure/core/build_info.h"
#include "conjure/engine/engine_version.h"

#include <format>
#include <string>

// engine_version.h: the engine name and version as the build defines them.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/engine_version") {

    TEST_CASE("engine_version - the constants mirror build_info") {
        CHECK(ENGINE_VERSION_MAJOR == CONJURE_VERSION_MAJOR);
        CHECK(ENGINE_VERSION_MINOR == CONJURE_VERSION_MINOR);
        CHECK(ENGINE_VERSION_PATCH == CONJURE_VERSION_PATCH);
    }

    TEST_CASE("engine_version - the strings match the constants") {
        CHECK(engine_version_string() == CONJURE_VERSION_STRING);
        const std::string expected =
          std::format("{}.{}.{}", ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_PATCH);
        CHECK(engine_version_string() == expected);
        CHECK(engine_name() == CONJURE_ENGINE_NAME);
        CHECK_FALSE(engine_name().empty());
    }
}
