#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/platform_detect.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace conjure;
using namespace conjure::testing;

namespace {
    std::string read_file(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        std::stringstream contents = {};
        contents << stream.rdbuf();
        return contents.str();
    }

    // Lower-case hex, as uuid::to_string() writes it.
    bool is_hex_suffix(std::string_view text) {
        return !text.empty() && text.find_first_not_of("0123456789abcdef") == std::string_view::npos;
    }
}

TEST_SUITE("testing/temp_directory") {

    TEST_CASE("temp_directory - unique per instance") {
        const temp_directory first = {};
        const temp_directory second = {};

        CHECK(first.path() != second.path());
        CHECK(std::filesystem::is_directory(first.path()));
        CHECK(std::filesystem::is_directory(second.path()));
        CHECK(std::filesystem::is_empty(first.path()));
    }

    TEST_CASE("temp_directory - lives under the environment's temporary root") {
        const temp_directory directory = {};

        if (environment().temporaryRoot.empty()) {
            CHECK(directory.path().is_absolute());
        } else {
            CHECK(directory.path().parent_path() == environment().temporaryRoot);
        }
    }

    TEST_CASE("temp_directory - prefix becomes part of the name") {
        const temp_directory directory("support_prefix");

        CHECK(directory.path().filename().string().starts_with("support_prefix_"));
    }

    TEST_CASE("regression - code sweep FU-LONGPATH temp_directory names end in eight hex digits") {
        SUBCASE("with a prefix") {
            const temp_directory directory("support_short");
            const std::string name = directory.path().filename().string();
            CAPTURE(name);
            REQUIRE(name.size() == std::string_view("support_short_").size() + 8);
            CHECK(name.starts_with("support_short_"));
            CHECK(is_hex_suffix(name.substr(name.size() - 8)));
        }
        SUBCASE("without a prefix") {
            const temp_directory directory = {};
            const std::string name = directory.path().filename().string();
            CAPTURE(name);
            REQUIRE(name.size() == 8);
            CHECK(is_hex_suffix(name));
        }
    }

    TEST_CASE("temp_directory - extended_length_path is the prefixed absolute form on Windows and the path elsewhere") {
        // Linux and macOS have no MAX_PATH, so there every form comes back as it was given.
        SUBCASE("a local path is made absolute, normalised and prefixed") {
            const temp_directory directory("support_extended");
            const std::filesystem::path inside = directory.path() / "a" / "b" / ".." / "c";
            const std::filesystem::path extended = extended_length_path(inside);
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                const std::filesystem::path expected =
                  std::filesystem::path(directory.path() / "a" / "c").make_preferred();
                CHECK(extended.wstring() == LR"(\\?\)" + expected.wstring());
            } else {
                CHECK(extended == inside);
            }
        }
        SUBCASE("a share takes the UNC form") {
            const std::filesystem::path share(R"(\\server\share\a\..\b)");
            const std::filesystem::path extended = extended_length_path(share);
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                CHECK(extended.wstring() == LR"(\\?\UNC\server\share\b)");
            } else {
                CHECK(extended == share);
            }
        }
        SUBCASE("a path already in a literal form is unchanged") {
            const std::filesystem::path literal(R"(\\?\C:\a\..\b)");
            CHECK(extended_length_path(literal) == literal);
        }
        SUBCASE("a generic device path is completed but not prefixed twice") {
            const std::filesystem::path device("//?/C:/a/../b");
            const std::filesystem::path extended = extended_length_path(device);
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                CHECK(extended.wstring() == LR"(\\?\C:\b)");
            } else {
                CHECK(extended == device);
            }
        }
    }

    TEST_CASE("temp_directory - long_directory creates a folder at least as long as asked inside the directory") {
        const temp_directory directory("support_long");
        const std::filesystem::path deep = long_directory(directory, PAST_MAX_PATH);
        CHECK(deep.native().size() >= PAST_MAX_PATH);
        CHECK(deep.native().size() < PAST_MAX_PATH + 41);
        CHECK(deep.lexically_relative(directory.path()).generic_string().starts_with(std::string(40, 'd')));
        CHECK(std::filesystem::is_directory(extended_length_path(deep)));
    }

    TEST_CASE("regression - code sweep FU-LONGPATH temp_directory removes a folder past MAX_PATH inside it") {
        std::filesystem::path deep = {};
        {
            const temp_directory directory("support_deep");
            deep = long_directory(directory, PAST_MAX_PATH);
            std::ofstream(extended_length_path(deep / "file.txt"), std::ios::binary) << "payload";
            std::error_code failure = {};
            REQUIRE(std::filesystem::exists(extended_length_path(deep / "file.txt"), failure));
        }
        std::error_code failure = {};
        CHECK_FALSE(std::filesystem::exists(extended_length_path(deep), failure));
        CHECK_FALSE(static_cast<bool>(failure));
    }

    TEST_CASE("regression - code sweep FU-LONGPATH temp_directory writes files below a folder past MAX_PATH") {
        const temp_directory directory("support_deep_write");
        const std::filesystem::path deep = long_directory(directory, PAST_MAX_PATH);
        const std::string relative = deep.lexically_relative(directory.path()).generic_string();
        SUBCASE("write_text") {
            CONJURE_REQUIRE_OK(directory.write_text(relative + "/new/file.txt", "payload"));
            CHECK(read_file(extended_length_path(deep / "new" / "file.txt")) == "payload");
        }
        SUBCASE("write_file") {
            const std::filesystem::path written = directory.write_file(relative + "/new/data.bin", "abc");
            CHECK(written == directory.file(relative + "/new/data.bin"));
            CHECK(read_file(extended_length_path(written)) == "abc");
        }
    }

    TEST_CASE("temp_directory - replaying_copy copies a program with its permissions and writes its replay beside it") {
        const temp_directory directory("support_replaying_copy");
        const std::filesystem::path program = directory.write_file("tools/program.bin", "program bytes");
        std::filesystem::permissions(
          program,
          std::filesystem::perms::owner_exec,
          std::filesystem::perm_options::add
        );
        const core::result<std::filesystem::path> copied =
          replaying_copy(directory, program, "stand_in", "echo 1.2.3");
        CONJURE_REQUIRE_OK(copied);
        CHECK(copied.value() == directory.file("stand_in.bin"));
        CHECK(read_file(copied.value()) == "program bytes");
        CHECK(read_file(directory.file("stand_in.bin.replay")) == "echo 1.2.3");
        CHECK(std::filesystem::status(copied.value()).permissions() == std::filesystem::status(program).permissions());

        // A second copy of the same name replaces the first, replay included.
        CONJURE_REQUIRE_OK(replaying_copy(directory, program, "stand_in", "exit 3"));
        CHECK(read_file(directory.file("stand_in.bin.replay")) == "exit 3");
    }

    TEST_CASE("temp_directory - replaying_copy of a program that does not exist is IO_FAILURE and writes nothing") {
        const temp_directory directory("support_replaying_copy_missing");
        const core::result<std::filesystem::path> copied =
          replaying_copy(directory, directory.file("missing.bin"), "stand_in", "echo 1.2.3");
        CONJURE_CHECK_ERROR(copied, core::error_code::IO_FAILURE);
        std::error_code failure = {};
        CHECK_FALSE(std::filesystem::exists(directory.file("stand_in.bin.replay"), failure));
    }

    TEST_CASE("temp_directory - directories made with one prefix are all distinct and exist together") {
        constexpr std::size_t COUNT = 64;
        std::vector<std::unique_ptr<temp_directory>> directories = {};
        std::set<std::filesystem::path> paths = {};
        for (std::size_t index = 0; index < COUNT; ++index) {
            CAPTURE(index);
            directories.push_back(std::make_unique<temp_directory>("support_many"));
            CHECK(paths.insert(directories.back()->path()).second);
            CHECK(std::filesystem::is_directory(directories.back()->path()));
        }
        CHECK(paths.size() == COUNT);
    }

    TEST_CASE("temp_directory - removed on destruction, contents included") {
        std::filesystem::path remembered = {};
        {
            const temp_directory directory = {};
            remembered = directory.path();
            CONJURE_REQUIRE_OK(directory.write_text("nested/deep/file.txt", "payload"));
            CHECK(std::filesystem::exists(remembered / "nested" / "deep" / "file.txt"));
        }
        CHECK_FALSE(std::filesystem::exists(remembered));
    }

    TEST_CASE("temp_directory - write_text round trip") {
        const temp_directory directory = {};

        CONJURE_REQUIRE_OK(directory.write_text("scene.json", "{\"a\": 1}"));

        CHECK(read_file(directory.file("scene.json")) == "{\"a\": 1}");
        CHECK(directory.file("scene.json") == directory.path() / "scene.json");

        CONJURE_REQUIRE_OK(directory.write_text("scene.json", "replaced"));
        CHECK(read_file(directory.file("scene.json")) == "replaced");
    }

    TEST_CASE("temp_directory - write_text reports a path that cannot be a directory") {
        const temp_directory directory = {};
        CONJURE_REQUIRE_OK(directory.write_text("blocker", "x"));

        CONJURE_CHECK_ERROR(directory.write_text("blocker/child.txt", "y"), core::error_code::IO_FAILURE);
    }

    TEST_CASE("temp_directory - write_file returns the full path") {
        const temp_directory directory = {};

        const std::filesystem::path written = directory.write_file("sub/data.bin", "abc");

        CHECK(written == directory.path() / "sub" / "data.bin");
        CHECK(read_file(written) == "abc");
    }
}
