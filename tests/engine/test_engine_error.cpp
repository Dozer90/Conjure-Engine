#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_error.h"

#include <string_view>

// engine_error.h (docs/design/06_engine_loop.md 2.12): the error category, its enumerator names and
// make_error.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/engine_error") {

    TEST_CASE("engine_error - every code has a name and COUNT has none") {
        for (u16 value = 0; value < static_cast<u16>(engine_error::COUNT); ++value) {
            CHECK_FALSE(engine_error_name(static_cast<engine_error>(value)).empty());
        }
        CHECK(engine_error_name(engine_error::COUNT).empty());
        CHECK(engine_error_name(engine_error::INVALID_CONFIG) == "INVALID_CONFIG");
        CHECK(engine_error_name(engine_error::SCRIPT_RUNTIME_INIT_FAILED) == "SCRIPT_RUNTIME_INIT_FAILED");
        CHECK(engine_error_name(engine_error::EXPORT_FAILED) == "EXPORT_FAILED");
    }

    TEST_CASE("engine_error - make_error builds an error of the engine category with the message") {
        const core::error error = make_error(engine_error::PROJECT_NOT_FOUND, "no project.json");
        CHECK(error.is(engine_error::PROJECT_NOT_FOUND));
        CHECK_FALSE(error.is(engine_error::PROJECT_INVALID));
        CHECK(error.category_name() == "engine");
        CHECK(error.code_name() == "PROJECT_NOT_FOUND");
        CHECK(error.message() == "no project.json");
        CHECK(error.to_string().find("PROJECT_NOT_FOUND") != std::string::npos);
    }

    TEST_CASE("engine_error - a result carrying an engine error reports it") {
        const core::result<int> failed = make_error(engine_error::NOT_INITIALISED);
        CHECK_FALSE(failed.has_value());
        CHECK(failed.get_error().is(engine_error::NOT_INITIALISED));
        CONJURE_CHECK_ERROR(failed, engine_error::NOT_INITIALISED);
    }
}
