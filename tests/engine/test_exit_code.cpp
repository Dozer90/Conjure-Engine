#include "conjure/testing/doctest.h"

#include "conjure/engine/exit_code.h"

// exit_code.h (docs/design/06_engine_loop.md 2.12): the process exit codes and their names.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/exit_code") {

    TEST_CASE("exit_code - values are the documented process codes") {
        CHECK(static_cast<int>(exit_code::OK) == 0);
        CHECK(static_cast<int>(exit_code::INIT_FAILURE) == 1);
        CHECK(static_cast<int>(exit_code::RUNTIME_FAILURE) == 2);
        CHECK(static_cast<int>(exit_code::BAD_ARGUMENTS) == 3);
        CHECK(static_cast<int>(exit_code::DEVICE_LOST) == 4);
    }

    TEST_CASE("exit_code - to_string names every code") {
        CHECK(to_string(exit_code::OK) == "OK");
        CHECK(to_string(exit_code::INIT_FAILURE) == "INIT_FAILURE");
        CHECK(to_string(exit_code::RUNTIME_FAILURE) == "RUNTIME_FAILURE");
        CHECK(to_string(exit_code::BAD_ARGUMENTS) == "BAD_ARGUMENTS");
        CHECK(to_string(exit_code::DEVICE_LOST) == "DEVICE_LOST");
        CHECK(to_string(static_cast<exit_code>(99)) == "UNKNOWN");
    }
}
