#include "conjure/testing/doctest.h"

#include "editor/core/commands/command_support.h"

#include "conjure/editor/editor_error.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// command_support.h (private): the helpers every command group shares; the codec and the build-tool output
// parser in particular, which module.build and asset.restore depend on.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::support;

namespace {
    std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }
}

TEST_SUITE("editor/command_support") {

    TEST_CASE("base64_encode / base64_decode - round trip every padding length and reject foreign characters") {
        CHECK(base64_encode(bytes_of("")) == "");
        CHECK(base64_encode(bytes_of("f")) == "Zg==");
        CHECK(base64_encode(bytes_of("fo")) == "Zm8=");
        CHECK(base64_encode(bytes_of("foo")) == "Zm9v");
        CHECK(base64_encode(bytes_of("foobar")) == "Zm9vYmFy");
        const std::array<std::string_view, 6> samples = {
            "", "a", "ab", "abc", "abcd", std::string_view("\x00\xFF\x7F\x80", 4),
        };
        for (const std::string_view text : samples) {
            const std::vector<std::byte> original = bytes_of(text);
            const core::result<std::vector<std::byte>> decoded = base64_decode(base64_encode(original));
            CONJURE_REQUIRE_OK(decoded);
            CHECK(decoded.value() == original);
        }
        const core::result<std::vector<std::byte>> spaced = base64_decode("Zm9v\nYmFy ");
        CONJURE_REQUIRE_OK(spaced);
        CHECK(spaced.value() == bytes_of("foobar"));
        CONJURE_CHECK_ERROR(base64_decode("Zm9v*"), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("parse_tool_diagnostic - MSVC, linker, GCC/Clang and CMake formats are recognised") {
        const std::optional<tool_diagnostic> msvc = parse_tool_diagnostic(
          R"(A:\src\mod\game.cpp(42,17): error C2065: 'foo': undeclared identifier [A:\build\game.vcxproj])"
        );
        REQUIRE(msvc.has_value());
        CHECK(msvc->isError);
        CHECK(msvc->file == R"(A:\src\mod\game.cpp)");
        CHECK(msvc->line == 42);
        CHECK(msvc->column == 17);
        CHECK(msvc->code == "C2065");
        CHECK(msvc->message.find("undeclared identifier") != std::string::npos);

        const std::optional<tool_diagnostic> msvcWarning = parse_tool_diagnostic(
          "src/error_handling.cpp(10): warning C4100: 'x': unreferenced formal parameter"
        );
        REQUIRE(msvcWarning.has_value());
        CHECK_FALSE(msvcWarning->isError);
        CHECK(msvcWarning->file == "src/error_handling.cpp");
        CHECK(msvcWarning->line == 10);
        CHECK(msvcWarning->column == 0);
        CHECK(msvcWarning->code == "C4100");

        const std::optional<tool_diagnostic> link = parse_tool_diagnostic(
          "LINK : fatal error LNK1104: cannot open file"
        );
        REQUIRE(link.has_value());
        CHECK(link->isError);
        CHECK(link->file == "LINK");
        CHECK(link->code == "LNK1104");
        CHECK(link->message == "cannot open file");

        const std::optional<tool_diagnostic> gcc = parse_tool_diagnostic(
          "/home/dev/mod/game.cpp:42:17: error: 'foo' was not declared in this scope"
        );
        REQUIRE(gcc.has_value());
        CHECK(gcc->isError);
        CHECK(gcc->file == "/home/dev/mod/game.cpp");
        CHECK(gcc->line == 42);
        CHECK(gcc->column == 17);
        CHECK(gcc->code.empty());
        CHECK(gcc->message == "'foo' was not declared in this scope");

        const std::optional<tool_diagnostic> clangWarning = parse_tool_diagnostic(
          "game.cpp:7: warning: unused variable 'y' [-Wunused-variable]"
        );
        REQUIRE(clangWarning.has_value());
        CHECK_FALSE(clangWarning->isError);
        CHECK(clangWarning->line == 7);
        CHECK(clangWarning->column == 0);

        const std::optional<tool_diagnostic> cmake = parse_tool_diagnostic(
          "CMake Error at CMakeLists.txt:12 (add_library):"
        );
        REQUIRE(cmake.has_value());
        CHECK(cmake->isError);
        const std::optional<tool_diagnostic> cmakeWarning = parse_tool_diagnostic("CMake Warning (dev) at x.cmake:3:");
        REQUIRE(cmakeWarning.has_value());
        CHECK_FALSE(cmakeWarning->isError);
    }

    TEST_CASE("regression - lines that merely mention the word error are not diagnostics") {
        CHECK_FALSE(parse_tool_diagnostic("[12/40] Building CXX object src/error_handling.cpp.obj").has_value());
        CHECK_FALSE(parse_tool_diagnostic("  error_handling.cpp").has_value());
        CHECK_FALSE(parse_tool_diagnostic("ninja: build stopped: subcommand failed.").has_value());
        CHECK_FALSE(parse_tool_diagnostic("-- Found error_tools: /usr/lib").has_value());
        CHECK_FALSE(parse_tool_diagnostic("game.cpp: In function 'void error_out()':").has_value());
        CHECK_FALSE(parse_tool_diagnostic("").has_value());
        // A message that mentions "warning" after an error marker stays an error.
        const std::optional<tool_diagnostic> mixed = parse_tool_diagnostic(
          "a.cpp(1): error C2220: the following warning is treated as an error"
        );
        REQUIRE(mixed.has_value());
        CHECK(mixed->isError);
    }

    TEST_CASE("is_safe_tool_argument - accepts names and refuses options, paths and shell characters") {
        CHECK(is_safe_tool_argument("Debug"));
        CHECK(is_safe_tool_argument("conjure_game-1.2"));
        CHECK(is_safe_tool_argument("windows-msvc-debug"));
        CHECK_FALSE(is_safe_tool_argument(""));
        CHECK_FALSE(is_safe_tool_argument("--target"));
        CHECK_FALSE(is_safe_tool_argument("-DFOO=1"));
        CHECK_FALSE(is_safe_tool_argument("a b"));
        CHECK_FALSE(is_safe_tool_argument("a;rm"));
        CHECK_FALSE(is_safe_tool_argument("../x"));
        CHECK_FALSE(is_safe_tool_argument("$(cmd)"));
    }
}
