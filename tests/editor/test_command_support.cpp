#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "editor/core/commands/command_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/editor_error.h"
#include "conjure/engine/project.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

    ecs::entity entity_of(editor_test::headless_editor &editor, const std::string &guid) {
        const std::optional<core::uuid> parsed = core::uuid::parse(guid);
        REQUIRE(parsed.has_value());
        const ecs::entity entity = editor.scene().find_guid(*parsed);
        REQUIRE_FALSE(entity.is_null());
        return entity;
    }

    core::json::document parsed_json(std::string_view text) {
        core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        return std::move(parsed).value();
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

    TEST_CASE("regression - base64_decode refuses misplaced padding, truncated quads and non-zero trailing bits") {
        CONJURE_CHECK_ERROR(base64_decode("QQ=QQ"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(base64_decode("Zm9"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(base64_decode("Zh=="), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(base64_decode("Zg==="), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(base64_decode("Zg==Zg=="), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(base64_decode("Zm9vY"), protocol_error_code::INVALID_ARGS);
        const std::array<std::pair<std::string_view, std::string_view>, 4> valid = {
            std::pair{"Zg==", "f"},
            std::pair{"Zm8=", "fo"},
            std::pair{"", ""},
            std::pair{"Zm9v\nYmFy ", "foobar"},
        };
        for (const auto &[encoded, expected] : valid) {
            CAPTURE(encoded);
            const core::result<std::vector<std::byte>> decoded = base64_decode(encoded);
            CONJURE_REQUIRE_OK(decoded);
            CHECK(decoded.value() == bytes_of(expected));
        }
    }

    TEST_CASE("regression - read_vec3, read_rotation and f32_or refuse a number outside the f32 range") {
        const core::json::document huge = parsed_json(R"({"x":1e39,"vector":[1e39,0,0],"fine":[1,2,3],"y":2.5})");
        const core::json::value root = huge.root();
        CONJURE_CHECK_ERROR(read_vec3(root["vector"], "args.vector"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(read_rotation(root["vector"], "args.rotation"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(f32_or(root, "x", 0.0f), protocol_error_code::INVALID_ARGS);
        const core::result<math::vec3> fine = read_vec3(root["fine"], "args.fine");
        CONJURE_REQUIRE_OK(fine);
        CONJURE_CHECK_APPROX(fine.value(), math::vec3(1.0f, 2.0f, 3.0f));
        CONJURE_CHECK_OK(read_rotation(root["fine"], "args.fine"));
        const core::result<f32> y = f32_or(root, "y", 0.0f);
        CONJURE_REQUIRE_OK(y);
        CHECK(y.value() == doctest::Approx(2.5f));
        const core::result<f32> absent = f32_or(root, "missing", 7.0f);
        CONJURE_REQUIRE_OK(absent);
        CHECK(absent.value() == doctest::Approx(7.0f));
        CONJURE_CHECK_ERROR(f32_or(root, "fine", 0.0f), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("regression - F485 u32_or reads a whole real such as 2.0 and refuses 2.5, -1 and 4294967296") {
        const core::json::document args = parsed_json(
          R"({"whole":2.0,"plain":7,"fraction":2.5,"negative":-1,"wide":4294967296,"huge":1e20,"text":"3",)"
          R"("top":4294967295.0})"
        );
        const core::json::value root = args.root();
        const core::result<u32> whole = u32_or(root, "whole", 0);
        CONJURE_REQUIRE_OK(whole);
        CHECK(whole.value() == 2);
        const core::result<u32> plain = u32_or(root, "plain", 0);
        CONJURE_REQUIRE_OK(plain);
        CHECK(plain.value() == 7);
        const core::result<u32> top = u32_or(root, "top", 0);
        CONJURE_REQUIRE_OK(top);
        CHECK(top.value() == 4294967295u);
        const core::result<u32> absent = u32_or(root, "missing", 9);
        CONJURE_REQUIRE_OK(absent);
        CHECK(absent.value() == 9);
        CONJURE_CHECK_ERROR(u32_or(root, "fraction", 0), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(u32_or(root, "negative", 0), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(u32_or(root, "wide", 0), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(u32_or(root, "huge", 0), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(u32_or(root, "text", 0), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("unusable_path - a missing path is PATH_NOT_FOUND and one that exists IO_ERROR with the reason") {
        const testing::temp_directory directory = {};
        const core::error missing = unusable_path(directory.path() / "absent.txt", "not a regular file");
        CHECK(missing.is(protocol_error_code::PATH_NOT_FOUND));
        CHECK(missing.message().find("absent.txt") != std::string::npos);
        CHECK(missing.message().find("no such file or folder") != std::string::npos);
        // The reason belongs to a path that exists; a missing one never carries it.
        CHECK(missing.message().find("not a regular file") == std::string::npos);
        // A missing parent is a missing path too.
        CHECK(unusable_path(directory.path() / "no" / "such" / "file.txt").is(protocol_error_code::PATH_NOT_FOUND));

        const core::error present = unusable_path(directory.path(), "not a regular file");
        CHECK(present.is(protocol_error_code::IO_ERROR));
        CHECK(present.message().find("not a regular file") != std::string::npos);
        const core::error bare = unusable_path(directory.path());
        CHECK(bare.is(protocol_error_code::IO_ERROR));
        CHECK(bare.message() == std::format("cannot read '{}'", directory.path().generic_string()));
    }

    TEST_CASE("read_failure - core NOT_FOUND is PATH_NOT_FOUND and every other failure IO_ERROR") {
        const core::error missing = read_failure(
          "assets/a.cmat",
          core::error(core::error_code::NOT_FOUND, "'assets/a.cmat' does not exist")
        );
        CHECK(missing.is(protocol_error_code::PATH_NOT_FOUND));
        CHECK(missing.message().find("cannot read 'assets/a.cmat'") != std::string::npos);
        CHECK(missing.message().find("does not exist") != std::string::npos);
        const core::error denied = read_failure(
          "assets/a.cmat",
          core::error(core::error_code::PERMISSION_DENIED, "access denied")
        );
        CHECK(denied.is(protocol_error_code::IO_ERROR));
        CHECK(denied.message().find("access denied") != std::string::npos);
        CHECK(read_failure("x", core::error(core::error_code::IO_FAILURE)).is(protocol_error_code::IO_ERROR));
        CHECK(path_not_found("gone").is(protocol_error_code::PATH_NOT_FOUND));
        CHECK(path_not_found("gone").message() == "gone");
    }

    TEST_CASE("outermost - keeps the first occurrence of each outermost subtree in input order and drops duplicates "
      "and descendants") {
        editor_test::headless_editor editor = {};
        const ecs::entity a = entity_of(editor, editor.create_entity("A"));
        const ecs::entity b = entity_of(editor, editor.create_entity("B"));
        const std::string cGuid = editor.create_entity("C", "A");
        const ecs::entity c = entity_of(editor, cGuid);
        const ecs::entity d = entity_of(editor, editor.create_entity("D", cGuid));
        const ecs::entity e = entity_of(editor, editor.create_entity("E"));
        const std::vector<ecs::entity> mixed = {d, b, a, b, c, e, a};
        CHECK(outermost(editor.scene(), mixed) == std::vector<ecs::entity>{b, a, e});
        // A grandparent in the list hides a grandchild whose parent is not listed.
        const std::vector<ecs::entity> skipped = {d, a};
        CHECK(outermost(editor.scene(), skipped) == std::vector<ecs::entity>{a});
        const std::vector<ecs::entity> alone = {d, d};
        CHECK(outermost(editor.scene(), alone) == std::vector<ecs::entity>{d});
        CHECK(outermost(editor.scene(), std::vector<ecs::entity>{}).empty());
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

    TEST_CASE("project_relative_path - a relative path joins the project root, an absolute or empty one stays") {
        editor_test::headless_editor editor = {};
        const editor_context &context = editor.context();
        const std::filesystem::path root = context.project()->root_directory();
        REQUIRE(root.is_absolute());
        CHECK(project_relative_path(context, std::string_view("out/build")) == root / "out" / "build");
        CHECK(project_relative_path(context, std::filesystem::path("shot.png")) == root / "shot.png");
        // Lexical: ".." is kept, so the result may lie outside the project.
        CHECK(project_relative_path(context, std::string_view("../sibling")) == root / ".." / "sibling");
        CHECK(project_relative_path(context, std::string_view("")).empty());
        CHECK(project_relative_path(context, std::filesystem::path()).empty());

        const std::filesystem::path absolute = root / "elsewhere" / "file.json";
        CHECK(project_relative_path(context, absolute) == absolute);
        CHECK(project_relative_path(context, std::string_view(absolute.generic_string())) == absolute);

        // A path that starts with a separator: absolute on POSIX; on Windows it names no drive, so it is not
        // absolute there and lands on the project's drive.
        const std::filesystem::path rooted = project_relative_path(context, std::string_view("/rooted/x"));
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            CHECK(rooted == root.root_name() / std::filesystem::path("/rooted/x"));
            CHECK(rooted.is_absolute());
        } else {
            CHECK(rooted == std::filesystem::path("/rooted/x"));
        }
    }

    TEST_CASE("project_relative_path - without a project a relative path stays as given") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        REQUIRE(editor.context().project() == nullptr);
        const std::filesystem::path given = project_relative_path(editor.context(), std::string_view("out/build"));
        CHECK(given == std::filesystem::path("out/build"));
        CHECK(project_relative_path(editor.context(), std::string_view("")).empty());
    }
}
