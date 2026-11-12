#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/platform_detect.h"
#include "core/system/system_internal.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::core;

namespace {
    // Built next to the test executable by tests/core/system/CMakeLists.txt.
    const std::filesystem::path LIBRARY_PATH = CONJURE_TEST_SHARED_LIBRARY_PATH;
    const std::filesystem::path DEPENDENT_LIBRARY_PATH = CONJURE_TEST_DEPENDENT_LIBRARY_PATH;

    using add_fn = int (*)(int, int);

    dynamic_library load_test_library() {
        result<dynamic_library> loaded = dynamic_library::load(LIBRARY_PATH);
        REQUIRE(loaded.has_value());
        return std::move(loaded).value();
    }

    std::filesystem::path copy_to(const std::filesystem::path &file, const std::filesystem::path &directory) {
        const std::filesystem::path target = directory / file.filename();
        std::error_code failure = {};
        std::filesystem::copy_file(file, target, std::filesystem::copy_options::overwrite_existing, failure);
        REQUIRE_FALSE(static_cast<bool>(failure));
        return target;
    }
}

TEST_SUITE("core/dynamic_library") {

    TEST_CASE("dynamic_library - decorate_name adds the platform prefix and extension") {
        const std::string decorated = dynamic_library::decorate_name("conjure_foo");
        const std::string expected = std::string(dynamic_library::prefix()) + "conjure_foo"
          + std::string(dynamic_library::extension());
        CHECK(decorated == expected);

        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK(decorated == "conjure_foo.dll");
            CHECK(dynamic_library::extension() == ".dll");
            CHECK(dynamic_library::prefix().empty());
        }
        if constexpr (CONJURE_PLATFORM_LINUX == 1) {
            CHECK(decorated == "libconjure_foo.so");
            CHECK(dynamic_library::extension() == ".so");
            CHECK(dynamic_library::prefix() == "lib");
        }
        if constexpr (CONJURE_PLATFORM_MACOS == 1) {
            CHECK(decorated == "libconjure_foo.dylib");
            CHECK(dynamic_library::extension() == ".dylib");
            CHECK(dynamic_library::prefix() == "lib");
        }
    }

    TEST_CASE("dynamic_library - decorate_name leaves decorated and empty names alone") {
        const std::string decorated = dynamic_library::decorate_name("conjure_foo");
        CHECK(dynamic_library::decorate_name(decorated) == decorated);
        CHECK(dynamic_library::decorate_name("").empty());
    }

    TEST_CASE("dynamic_library - decorate_name keeps a directory component in front of the prefix") {
        const std::string prefix(dynamic_library::prefix());
        const std::string extension(dynamic_library::extension());

        const std::string expected = "modules/" + prefix + "conjure_foo" + extension;
        const std::string expectedBackslash = "plugins\\" + prefix + "conjure_foo" + extension;
        CHECK(dynamic_library::decorate_name("modules/conjure_foo") == expected);
        CHECK(dynamic_library::decorate_name("a/b/c") == "a/b/" + prefix + "c" + extension);
        CHECK(dynamic_library::decorate_name("plugins\\conjure_foo") == expectedBackslash);
        CHECK(dynamic_library::decorate_name(expected) == expected);
        if constexpr (CONJURE_PLATFORM_LINUX == 1) {
            CHECK(dynamic_library::decorate_name("modules/conjure_foo") == "modules/libconjure_foo.so");
        }
    }

    TEST_CASE("dynamic_library - a library finds a dependency placed next to itself from any directory") {
        // Neither library lives in the application directory, the current directory or PATH once
        // copied, so only the loading library's own directory can supply the dependency.
        const testing::temp_directory directory("core_dynamic_library_sibling");
        const std::filesystem::path dependent = copy_to(DEPENDENT_LIBRARY_PATH, directory.path());
        copy_to(LIBRARY_PATH, directory.path());

        result<dynamic_library> loaded = dynamic_library::load(dependent);
        REQUIRE(loaded.has_value());
        CHECK(loaded->is_loaded());
        const add_fn addTwice = loaded->symbol_as<add_fn>("conjure_test_add_twice");
        REQUIRE(addTwice != nullptr);
        CHECK(addTwice(2, 3) == 10);
        loaded->unload();
    }

    TEST_CASE("dynamic_library - a library whose dependency is missing is BACKEND_FAILURE and names the file") {
        const testing::temp_directory directory("core_dynamic_library_missing_dependency");
        const std::filesystem::path dependent = copy_to(DEPENDENT_LIBRARY_PATH, directory.path());

        const result<dynamic_library> loaded = dynamic_library::load(dependent);
        REQUIRE_FALSE(loaded.has_value());
        CHECK(loaded.get_error().generic_code() == error_code::BACKEND_FAILURE);
        CHECK(loaded.get_error().message().find(dependent.filename().generic_string()) != std::string::npos);
    }

    TEST_CASE("dynamic_library - decorate_name matches the build output of a shared library") {
        const std::string expected = dynamic_library::decorate_name("conjure_test_shared_library");
        CHECK(LIBRARY_PATH.filename().generic_string() == expected);
    }

    TEST_CASE("dynamic_library - a default-constructed library is not loaded") {
        dynamic_library library = {};
        CHECK_FALSE(library.is_loaded());
        CHECK(library.path().empty());
        CHECK(library.symbol("conjure_test_add") == nullptr);
        CHECK(library.symbol_as<add_fn>("conjure_test_add") == nullptr);
        library.unload();
        CHECK_FALSE(library.is_loaded());
    }

    TEST_CASE("dynamic_library - loading a missing file is NOT_FOUND and names the file") {
        const testing::temp_directory directory("core_dynamic_library_missing");
        const std::filesystem::path missing = directory.path() / dynamic_library::decorate_name("nope");
        const result<dynamic_library> loaded = dynamic_library::load(missing);

        REQUIRE_FALSE(loaded.has_value());
        CHECK(loaded.get_error().generic_code() == error_code::NOT_FOUND);
        const std::string &message = loaded.get_error().message();
        CHECK(message.find("nope") != std::string::npos);
        CHECK(message.size() > missing.generic_string().size() + 10);   // carries the OS text too
    }

    TEST_CASE("dynamic_library - loading an empty path is INVALID_ARGUMENT") {
        const result<dynamic_library> loaded = dynamic_library::load(std::filesystem::path());
        REQUIRE_FALSE(loaded.has_value());
        CHECK(loaded.get_error().generic_code() == error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("dynamic_library - loading a file that is not a library fails with the loader's message") {
        const testing::temp_directory directory("core_dynamic_library_bogus");
        const std::filesystem::path bogus = directory.write_file(
          dynamic_library::decorate_name("bogus"),
          "this is not a shared library"
        );
        const result<dynamic_library> loaded = dynamic_library::load(bogus);

        REQUIRE_FALSE(loaded.has_value());
        CHECK(loaded.get_error().generic_code() == error_code::BACKEND_FAILURE);
        CHECK(loaded.get_error().message().find("bogus") != std::string::npos);
    }

    TEST_CASE("dynamic_library - the test library loads and exports a function and a variable") {
        const dynamic_library library = load_test_library();
        CHECK(library.is_loaded());
        CHECK(library.path() == LIBRARY_PATH);

        CHECK(library.symbol("conjure_test_add") != nullptr);
        const add_fn add = library.symbol_as<add_fn>("conjure_test_add");
        REQUIRE(add != nullptr);
        CHECK(add(2, 3) == 5);
        CHECK(add(-7, 7) == 0);

        const int *constant = library.symbol_as<int *>("conjure_test_constant");
        REQUIRE(constant != nullptr);
        CHECK(*constant == 42);

        CHECK(library.symbol("conjure_does_not_exist") == nullptr);
        CHECK(library.symbol_as<add_fn>("conjure_does_not_exist") == nullptr);
        CHECK(library.symbol(nullptr) == nullptr);
        CHECK(library.symbol("") == nullptr);
    }

    TEST_CASE("dynamic_library - unload releases the library and is idempotent") {
        dynamic_library library = load_test_library();
        REQUIRE(library.is_loaded());

        library.unload();
        CHECK_FALSE(library.is_loaded());
        CHECK(library.path().empty());
        CHECK(library.symbol("conjure_test_add") == nullptr);

        library.unload();
        CHECK_FALSE(library.is_loaded());
    }

    TEST_CASE("dynamic_library - move construction and assignment transfer ownership") {
        dynamic_library original = load_test_library();

        dynamic_library moved(std::move(original));
        CHECK_FALSE(original.is_loaded());
        CHECK(original.path().empty());
        CHECK(moved.is_loaded());
        CHECK(moved.path() == LIBRARY_PATH);
        CHECK(moved.symbol_as<add_fn>("conjure_test_add")(1, 1) == 2);

        dynamic_library assigned = {};
        assigned = std::move(moved);
        CHECK_FALSE(moved.is_loaded());
        CHECK(assigned.is_loaded());
        CHECK(assigned.symbol_as<add_fn>("conjure_test_add")(20, 22) == 42);

        // Assigning over a loaded library unloads it first and keeps the new one.
        dynamic_library second = load_test_library();
        assigned = std::move(second);
        CHECK(assigned.is_loaded());
        CHECK_FALSE(second.is_loaded());
        CHECK(assigned.symbol_as<add_fn>("conjure_test_add")(3, 4) == 7);
    }

    TEST_CASE("dynamic_library - the same library can be loaded twice at once and reloaded after unloading") {
        {
            const dynamic_library first = load_test_library();
            const dynamic_library second = load_test_library();
            CHECK(first.symbol_as<add_fn>("conjure_test_add")(1, 2) == 3);
            CHECK(second.symbol_as<add_fn>("conjure_test_add")(3, 4) == 7);
        }
        {
            const dynamic_library again = load_test_library();
            CHECK(again.symbol_as<add_fn>("conjure_test_add")(5, 6) == 11);
        }
    }

    TEST_CASE("dynamic_library - a system library is found through the search path by bare name") {
        std::string name = {};
        const char *symbolName = "";
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            name = "kernel32.dll";
            symbolName = "GetCurrentProcessId";
        }
        if constexpr (CONJURE_PLATFORM_LINUX == 1) {
            name = "libm.so.6";
            symbolName = "cos";
        }
        if constexpr (CONJURE_PLATFORM_MACOS == 1) {
            name = "libSystem.B.dylib";
            symbolName = "cos";
        }

        const result<dynamic_library> loaded = dynamic_library::load(name);
        REQUIRE(loaded.has_value());
        CHECK(loaded->is_loaded());
        CHECK(loaded->path() == std::filesystem::path(name));
        CHECK(loaded->symbol(symbolName) != nullptr);
    }

    TEST_CASE("dynamic_library - a bare name that no search path holds is NOT_FOUND") {
        const std::string name = dynamic_library::decorate_name("conjure_no_such_library");
        const result<dynamic_library> loaded = dynamic_library::load(name);
        REQUIRE_FALSE(loaded.has_value());
        CHECK(loaded.get_error().generic_code() == error_code::NOT_FOUND);
    }

    TEST_CASE("dynamic_library - bare-name loader messages about symbols or dependencies are BACKEND_FAILURE") {
        // The texts glibc and dyld produce; only a message about the requested file itself means it is missing.
        constexpr error_code MISSING = error_code::NOT_FOUND;
        constexpr error_code BROKEN = error_code::BACKEND_FAILURE;
        struct loader_case {
            std::string_view requested = {};
            std::string_view message = {};
            error_code expected = error_code::BACKEND_FAILURE;
        };
        const std::array<loader_case, 11> cases = {{
            {"libgame.so", "libgame.so: cannot open shared object file: No such file or directory", MISSING},
            {"libgame.so", "libdep.so: cannot open shared object file: No such file or directory", BROKEN},
            {"libgame.so", "/opt/libdep.so: cannot open shared object file: No such file or directory", BROKEN},
            {"libgame.so", "libgame.so: undefined symbol: engine_fn", BROKEN},
            {"libgame.so", "libgame.so: invalid ELF header", BROKEN},
            {"libgame.dylib", "dlopen(libgame.dylib, 0x0006): tried: 'libgame.dylib' (no such file)", MISSING},
            {"libgame.dylib", "dlopen(libgame.dylib, 6): image not found", MISSING},
            {"libgame.dylib", "dlopen(libgame.dylib, 0x0006): Symbol not found: _engine_fn", BROKEN},
            {
                "libgame.dylib",
                "dlopen(libgame.dylib, 0x0006): Library not loaded: @rpath/libdep.dylib\n  Reason: tried: "
                "'/opt/libdep.dylib' (no such file)",
                BROKEN
            },
            {
                "libgame.dylib",
                "dlopen(libgame.dylib, 0x0006): tried: 'libgame.dylib' (mach-o file, but is an incompatible "
                "architecture (have 'x86_64', need 'arm64')), '/usr/lib/libgame.dylib' (no such file)",
                BROKEN
            },
            {
                "libgame.dylib",
                "dlopen(libgame.dylib, 0x0006): tried: 'libgame.dylib' (code signature in 'libgame.dylib' not "
                "valid for use in process), '/usr/lib/libgame.dylib' (no such file)",
                BROKEN
            },
        }};
        for (const loader_case &entry : cases) {
            CAPTURE(entry.message);
            CHECK(core::detail::classify_bare_name_load_failure(entry.requested, entry.message) == entry.expected);
        }
    }
}
