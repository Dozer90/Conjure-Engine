#include "conjure/testing/doctest.h"

#include "conjure/bindings/bindings.h"

#include <type_traits>

// bindings.h: the umbrella header exposes every public type of the module.

using namespace conjure;
using namespace conjure::bindings;

TEST_SUITE("bindings/bindings") {

    TEST_CASE("bindings - the umbrella header exposes every public type") {
        CHECK(std::is_enum_v<bindings_error>);
        CHECK(std::is_enum_v<native_function_id>);
        CHECK(std::is_default_constructible_v<binding_context>);
        CHECK(std::is_default_constructible_v<script_residency>);
        CHECK(std::is_base_of_v<engine::engine_subsystem, scripting_bindings_subsystem>);
        CHECK(std::is_default_constructible_v<marshal::structural_guard>);
        CHECK(NATIVE_FUNCTION_COUNT == CONJURE_NATIVE_API_ENTRY_COUNT);
        CHECK(to_status(bindings_error::NONE) == 0);
        CHECK(LOG_BINDINGS.name() != nullptr);
    }
}
