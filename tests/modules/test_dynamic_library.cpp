#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "shared_fixtures.h"

#include "conjure/modules/module_abi.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/error.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

// core::dynamic_library has its own suite in tests/core/system; this one exercises it against a
// real module binary, the way the loader uses it.

namespace {
    using count_fn = std::uint32_t (*)(void);

    dynamic_library open_alpha() {
        result<dynamic_library> loaded = dynamic_library::load(fixture_binary("module_alpha"));
        INFO("load failed: ", conjure::testing::describe_error(loaded));
        REQUIRE(loaded.has_value());
        return std::move(loaded).value();
    }
}

TEST_SUITE("modules/dynamic_library") {

    TEST_CASE("dynamic_library - opening a missing module path fails with the operating system's text") {
        const conjure::testing::temp_directory directory("modules_dynamic_library_missing");
        const std::filesystem::path missing = directory.path() / decorate_binary_name("ghost", host_platform());
        const result<dynamic_library> loaded = dynamic_library::load(missing);
        REQUIRE_FALSE(loaded.has_value());
        CHECK(loaded.get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(loaded.get_error().message().find("ghost") != std::string::npos);
        CHECK(loaded.get_error().message().size() > missing.generic_string().size() + 10);
    }

    TEST_CASE("dynamic_library - the alpha fixture opens and exports the two module symbols") {
        const dynamic_library library = open_alpha();
        CHECK(library.is_loaded());
        CHECK(library.path() == fixture_binary("module_alpha"));

        const auto describe = library.symbol_as<conjure_module_describe_fn>(CONJURE_MODULE_DESCRIBE_SYMBOL);
        REQUIRE(describe != nullptr);
        const conjure_module_info *info = describe();
        REQUIRE(info != nullptr);
        CHECK(std::string_view(info->name) == "module_alpha");
        CHECK(compare_abi(info->abi, host_abi_signature()).has_value());

        CHECK(library.symbol(CONJURE_MODULE_ENTRY_SYMBOL) != nullptr);
        const count_fn count = library.symbol_as<count_fn>("conjure_alpha_activated_count");
        REQUIRE(count != nullptr);
        CHECK(count() == 0);
        CHECK(library.symbol("conjure_module_nothing") == nullptr);
        CHECK(library.symbol_as<count_fn>("conjure_module_nothing") == nullptr);
    }

    TEST_CASE("dynamic_library - move construction and assignment transfer the module handle") {
        dynamic_library original = open_alpha();
        dynamic_library moved(std::move(original));
        CHECK_FALSE(original.is_loaded());
        CHECK(moved.is_loaded());
        CHECK(moved.symbol(CONJURE_MODULE_DESCRIBE_SYMBOL) != nullptr);

        dynamic_library assigned = {};
        assigned = std::move(moved);
        CHECK_FALSE(moved.is_loaded());
        CHECK(assigned.is_loaded());
        CHECK(assigned.symbol(CONJURE_MODULE_ENTRY_SYMBOL) != nullptr);
    }

    TEST_CASE("dynamic_library - unload is idempotent and clears the symbols") {
        dynamic_library library = open_alpha();
        library.unload();
        CHECK_FALSE(library.is_loaded());
        CHECK(library.path().empty());
        CHECK(library.symbol(CONJURE_MODULE_DESCRIBE_SYMBOL) == nullptr);
        library.unload();
        CHECK_FALSE(library.is_loaded());
    }

    TEST_CASE("dynamic_library - decorate_name agrees with the module system's platform decoration") {
        CHECK(dynamic_library::decorate_name("module_alpha") == decorate_binary_name("module_alpha", host_platform()));
        const std::string fileName = fixture_binary("module_alpha").filename().generic_string();
        CHECK(fileName == dynamic_library::decorate_name("module_alpha"));
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK(dynamic_library::decorate_name("module_alpha") == "module_alpha.dll");
        } else if constexpr (CONJURE_PLATFORM_MACOS == 1) {
            CHECK(dynamic_library::decorate_name("module_alpha") == "libmodule_alpha.dylib");
        } else {
            CHECK(dynamic_library::decorate_name("module_alpha") == "libmodule_alpha.so");
        }
    }
}
