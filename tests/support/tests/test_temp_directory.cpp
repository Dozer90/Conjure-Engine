#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace conjure;
using namespace conjure::testing;

namespace {
    std::string read_file(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        std::stringstream contents = {};
        contents << stream.rdbuf();
        return contents.str();
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
