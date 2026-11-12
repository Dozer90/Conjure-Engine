#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "engine/executable_path.h"

#include <filesystem>
#include <string>
#include <system_error>

// executable_path.h (docs/design/06_engine_loop.md 3.4 steps 5, 7 and 15): the directory of the running
// executable, which engine://, the shader index and the engine plugins are resolved against. A path longer than
// MAX_PATH needs LongPathsEnabled on the machine and is not automated.

using namespace conjure;
using namespace conjure::testing;

TEST_SUITE("engine/executable_path") {

    TEST_CASE("executable_directory - names the running executable's directory whatever the current directory") {
        temp_directory directory("engine_executable_path");
        std::error_code error = {};
        const std::filesystem::path previous = std::filesystem::current_path(error);
        REQUIRE_FALSE(error);
        std::filesystem::current_path(directory.path(), error);
        REQUIRE_FALSE(error);
        const std::filesystem::path found = engine::executable_directory();
        std::filesystem::current_path(previous, error);
        REQUIRE_FALSE(error);

        CHECK(found.is_absolute());
        CHECK_FALSE(std::filesystem::equivalent(found, directory.path(), error));

        // The test executable itself lives there (conjure_engine_tests, with the platform's suffix).
        bool holdsTests = false;
        std::filesystem::directory_iterator walker(found, error);
        const std::filesystem::directory_iterator end = {};
        while (!error && walker != end) {
            std::error_code status = {};
            const bool regular = walker->is_regular_file(status);
            if (regular && walker->path().stem().string().starts_with("conjure_engine_tests")) {
                holdsTests = true;
            }
            walker.increment(error);
        }
        CHECK_FALSE(error);
        CHECK(holdsTests);
    }
}
