#include "conjure/testing/doctest.h"

#include "conjure/modules/module_error.h"

#include "conjure/core/error.h"

#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

TEST_SUITE("modules/module_error") {

    TEST_CASE("module_error - every code has its enumerator name and values past COUNT have none") {
        CHECK(module_error_name(module_error::NONE) == "NONE");
        CHECK(module_error_name(module_error::MANIFEST_NOT_FOUND) == "MANIFEST_NOT_FOUND");
        CHECK(module_error_name(module_error::ABI_MISMATCH) == "ABI_MISMATCH");
        CHECK(module_error_name(module_error::REGISTRATION_CONFLICT) == "REGISTRATION_CONFLICT");
        CHECK(module_error_name(module_error::EXPORT_FAILED) == "EXPORT_FAILED");
        CHECK(module_error_name(module_error::COUNT).empty());
        CHECK(module_error_name(static_cast<module_error>(999)).empty());

        for (u16 code = 0; code < static_cast<u16>(module_error::COUNT); ++code) {
            CAPTURE(code);
            CHECK_FALSE(module_error_name(static_cast<module_error>(code)).empty());
        }
    }

    TEST_CASE("module_error - make_module_error binds the modules category") {
        const error formatted =
          make_module_error(module_error::BINARY_NOT_FOUND, "module '{}' has no binary at {}", "alpha", 3);
        CHECK(formatted.is(module_error::BINARY_NOT_FOUND));
        CHECK_FALSE(formatted.is(module_error::ABI_MISMATCH));
        CHECK(formatted.is_category<module_error>());
        CHECK_FALSE(formatted.is_category<error_code>());
        CHECK(formatted.category_name() == "modules");
        CHECK(formatted.code_name() == "BINARY_NOT_FOUND");
        CHECK(formatted.message() == "module 'alpha' has no binary at 3");
        CHECK(formatted.to_string() == "modules.BINARY_NOT_FOUND: module 'alpha' has no binary at 3");
        CHECK(formatted.generic_code() == error_code::NONE);

        const error fixed = make_module_error(module_error::STALE_ID, "braces {} stay");
        CHECK(fixed.is(module_error::STALE_ID));
        CHECK(fixed.message() == "braces {} stay");

        const error bare = make_module_error(module_error::FAULTED);
        CHECK(bare.is(module_error::FAULTED));
        CHECK(bare.message().empty());
        CHECK(bare.to_string() == "modules.FAULTED");
    }

    TEST_CASE("module_error - the category id is the hash of its name and differs from core") {
        const error moduleError = make_module_error(module_error::IN_USE);
        const error coreError(error_code::NOT_FOUND);
        CHECK(moduleError.category() == hash::fnv1a32("modules"));
        CHECK(moduleError.category() != coreError.category());
        CHECK(moduleError != coreError);
    }
}
