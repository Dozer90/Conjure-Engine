#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/core/error.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/types.h"
#include "conjure/engine/export.h"

#include <array>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// The .NET SDK functions of export.h (owner decision OWN-INSTALL), a topic file beside test_export.cpp: the SDK
// version global.json names, baked in at configure time, the download page, the version comparison and the probe.
// The probe runs against stand-ins: a program that does not
// exist, conjure_process_child (which answers --version with its usage line and exit code 2, as a dotnet without an
// SDK answers with an error), and a copy of it that replays a fixed mode (process_child.cpp, "<program>.replay"),
// standing in for a dotnet that prints a version or never answers.

using namespace conjure;
using namespace conjure::engine;

namespace {

    /// <summary>
    /// The three numbers of required_dotnet_sdk_version().
    /// </summary>
    [[nodiscard]] std::array<u32, 3> required_numbers() {
        std::array<u32, 3> numbers = {};
        const std::vector<std::string_view> parts = core::str::split(required_dotnet_sdk_version(), '.');
        REQUIRE(parts.size() == numbers.size());
        for (usize index = 0; index < numbers.size(); ++index) {
            CAPTURE(index);
            const std::optional<u32> number = core::str::parse_integer<u32>(parts[index]);
            REQUIRE(number.has_value());
            numbers[index] = *number;
        }
        return numbers;
    }

    [[nodiscard]] std::string version_text(const std::array<u32, 3> &numbers) {
        return std::format("{}.{}.{}", numbers[0], numbers[1], numbers[2]);
    }

    /// <summary>
    /// A copy of conjure_process_child in 'directory' that runs 'replay' (a mode and its words) whatever arguments
    /// it gets, standing in for a dotnet; returns its path.
    /// </summary>
    [[nodiscard]] std::filesystem::path fake_dotnet(const testing::temp_directory &directory, std::string_view replay) {
        const core::result<std::filesystem::path> fake = testing::replaying_copy(
          directory,
          std::filesystem::path(CONJURE_ENGINE_TEST_PROCESS_CHILD),
          "fake_dotnet",
          replay
        );
        CONJURE_REQUIRE_OK(fake);
        return fake.value();
    }
}

TEST_SUITE("engine/dotnet_sdk") {

    TEST_CASE("dotnet_sdk - the required version is the one global.json names") {
        CHECK(required_dotnet_sdk_version() == CONJURE_ENGINE_TEST_GLOBAL_JSON_SDK);
        CHECK(is_usable_dotnet_sdk_version(required_dotnet_sdk_version()));
    }

    TEST_CASE("dotnet_sdk - the download page is Microsoft's page of the required release") {
        const std::array<u32, 3> required = required_numbers();
        const std::string page = dotnet_sdk_download_page();
        CHECK(page == std::format("https://dotnet.microsoft.com/download/dotnet/{}.{}", required[0], required[1]));
    }

    TEST_CASE("dotnet_sdk - the required version and later ones are usable, earlier ones and prereleases are not") {
        const std::array<u32, 3> required = required_numbers();
        std::array<u32, 3> later = required;
        bool usable = false;
        std::string version = {};
        SUBCASE("the required version itself") {
            version = version_text(required);
            usable = true;
        }
        SUBCASE("a later patch, feature band, minor and major") {
            for (usize index = 0; index < later.size(); ++index) {
                CAPTURE(index);
                later = required;
                ++later[index];
                CHECK(is_usable_dotnet_sdk_version(version_text(later)));
            }
            version = version_text(required);
            usable = true;
        }
        SUBCASE("an earlier version in each position that is not zero") {
            for (usize index = 0; index < required.size(); ++index) {
                CAPTURE(index);
                if (required[index] == 0) {
                    continue;
                }
                std::array<u32, 3> earlier = required;
                --earlier[index];
                CHECK_FALSE(is_usable_dotnet_sdk_version(version_text(earlier)));
            }
            version = "0.0.0";
            usable = false;
        }
        SUBCASE("a prerelease of the required version") {
            version = version_text(required) + "-rc.2.25502.107";
            usable = false;
        }
        SUBCASE("a prerelease of a later major") {
            later[0] = required[0] + 1;
            version = version_text(later) + "-preview.1";
            usable = true;
        }
        SUBCASE("build metadata after the required version") {
            version = version_text(required) + "+abc123";
            usable = true;
        }
        SUBCASE("whitespace and a line end around the version") {
            version = " " + version_text(required) + "\r\n";
            usable = true;
        }
        CAPTURE(version);
        CHECK(is_usable_dotnet_sdk_version(version) == usable);
    }

    TEST_CASE("dotnet_sdk - text that is no version is not usable") {
        const std::string required = std::string(required_dotnet_sdk_version());
        const std::array<std::string, 9> texts = {
            "",
            "10",
            "10.0",
            "SDK",
            required + "x",
            required + ".1",
            "-" + required,
            "+" + required,
            "99999999999.0.0",
        };
        for (const std::string &text : texts) {
            CAPTURE(text);
            CHECK_FALSE(is_usable_dotnet_sdk_version(text));
        }
    }

    TEST_CASE("dotnet_sdk - the message names what was found, the required version and the download page") {
        const std::string message = missing_dotnet_sdk_message("'dotnet' could not be started");
        CHECK(message.find("'dotnet' could not be started") != std::string::npos);
        CHECK(message.find(std::string(required_dotnet_sdk_version())) != std::string::npos);
        CHECK(message.find("global.json") != std::string::npos);
        CHECK(message.find(dotnet_sdk_download_page()) != std::string::npos);
    }

    TEST_CASE("dotnet_sdk - find_dotnet_sdk fails naming a dotnet that cannot be started") {
        testing::temp_directory directory("engine_dotnet_sdk_missing");
        const std::filesystem::path missing = directory.file("no_dotnet_here");
        const std::u8string missingText = missing.u8string();
        const std::string dotnet(missingText.begin(), missingText.end());
        const core::result<std::string> found = find_dotnet_sdk(dotnet, directory.path(), 30.0);
        CONJURE_CHECK_ERROR(found, core::error_code::DEPENDENCY_FAILURE);
        REQUIRE_FALSE(found.has_value());
        const std::string_view message = found.get_error().message();
        CHECK(message.find("no_dotnet_here") != std::string_view::npos);
        CHECK(message.find("could not be started") != std::string_view::npos);
        CHECK(message.find(required_dotnet_sdk_version()) != std::string_view::npos);
        CHECK(message.find(dotnet_sdk_download_page()) != std::string_view::npos);
    }

    TEST_CASE("dotnet_sdk - find_dotnet_sdk fails on a dotnet that answers --version with an error and quotes it") {
        // conjure_process_child prints "unknown mode '--version'" and exits with 2, as a dotnet host without an SDK
        // prints "No .NET SDKs were found" and exits with an error.
        testing::temp_directory directory("engine_dotnet_sdk_no_sdk");
        const core::result<std::string> found =
          find_dotnet_sdk(CONJURE_ENGINE_TEST_PROCESS_CHILD, directory.path(), 30.0);
        CONJURE_CHECK_ERROR(found, core::error_code::DEPENDENCY_FAILURE);
        REQUIRE_FALSE(found.has_value());
        const std::string_view message = found.get_error().message();
        CHECK(message.find("--version' exited with 2") != std::string_view::npos);
        CHECK(message.find("unknown mode '--version'") != std::string_view::npos);
        CHECK(message.find(dotnet_sdk_download_page()) != std::string_view::npos);
    }

    TEST_CASE("dotnet_sdk - find_dotnet_sdk returns the version a dotnet reports and refuses an older one") {
        testing::temp_directory directory("engine_dotnet_sdk_version");
        const std::array<u32, 3> required = required_numbers();
        std::string reported = version_text(required);
        bool usable = true;
        SUBCASE("the required version") {
            usable = true;
        }
        SUBCASE("a later major") {
            std::array<u32, 3> later = required;
            ++later[0];
            reported = version_text(later);
            usable = true;
        }
        SUBCASE("an older version") {
            reported = "1.0.100";
            usable = false;
        }
        SUBCASE("a prerelease of the required version") {
            reported = version_text(required) + "-rc.1";
            usable = false;
        }
        CAPTURE(reported);
        const std::filesystem::path fake = fake_dotnet(directory, "echo " + reported);
        const std::u8string fakeText = fake.u8string();
        const core::result<std::string> found =
          find_dotnet_sdk(std::string(fakeText.begin(), fakeText.end()), directory.path(), 30.0);
        if (usable) {
            CONJURE_REQUIRE_OK(found);
            CHECK(found.value() == reported);
            return;
        }
        CONJURE_CHECK_ERROR(found, core::error_code::DEPENDENCY_FAILURE);
        REQUIRE_FALSE(found.has_value());
        const std::string_view message = found.get_error().message();
        CHECK(message.find(std::format("reports the SDK '{}'", reported)) != std::string_view::npos);
        CHECK(message.find(required_dotnet_sdk_version()) != std::string_view::npos);
    }

    TEST_CASE("dotnet_sdk - find_dotnet_sdk runs dotnet in the working directory it is given") {
        // dotnet reads the global.json in or above its current directory, so the export (in the project root) and
        // script.compile (in the scripts root) depend on the directory reaching the child. The stand-in prints its
        // current directory as the version, which the message quotes.
        testing::temp_directory directory("engine_dotnet_sdk_directory");
        const std::filesystem::path fake = fake_dotnet(directory, "cwd");
        const std::filesystem::path workingDirectory = directory.path() / "project" / "scripts";
        std::error_code error = {};
        std::filesystem::create_directories(workingDirectory, error);
        REQUIRE_FALSE(error);
        const std::u8string fakeText = fake.u8string();
        const core::result<std::string> found =
          find_dotnet_sdk(std::string(fakeText.begin(), fakeText.end()), workingDirectory, 30.0);
        CONJURE_CHECK_ERROR(found, core::error_code::DEPENDENCY_FAILURE);
        REQUIRE_FALSE(found.has_value());
        const std::string message(found.get_error().message());
        CAPTURE(message);
        const std::string_view opening = "reports the SDK '";
        const usize start = message.find(opening);
        REQUIRE(start != std::string::npos);
        const usize first = start + opening.size();
        const usize end = message.find('\'', first);
        REQUIRE(end != std::string::npos);
        const std::filesystem::path reported(message.substr(first, end - first));
        CHECK(std::filesystem::equivalent(reported, workingDirectory, error));
        CHECK_FALSE(error);
    }

    TEST_CASE("dotnet_sdk - find_dotnet_sdk gives up on a dotnet that does not answer in time") {
        testing::temp_directory directory("engine_dotnet_sdk_timeout");
        const std::filesystem::path fake = fake_dotnet(directory, "sleep 20000");
        const std::u8string fakeText = fake.u8string();
        const core::result<std::string> found =
          find_dotnet_sdk(std::string(fakeText.begin(), fakeText.end()), directory.path(), 0.5);
        CONJURE_CHECK_ERROR(found, core::error_code::DEPENDENCY_FAILURE);
        REQUIRE_FALSE(found.has_value());
        CHECK(found.get_error().message().find("did not answer within 0.5 s") != std::string_view::npos);
    }

#if CONJURE_ENGINE_TEST_HAS_DOTNET
    TEST_CASE("dotnet_sdk - find_dotnet_sdk finds the SDK the engine was built with") {
        // The configure ran the same dotnet in the source root, where dotnet reads the engine's global.json, and
        // refused to go on without the SDK it names; asked from the same folder, it answers with that SDK.
        const std::filesystem::path sourceRoot = std::filesystem::path(CONJURE_ENGINE_TEST_SOURCE_DIR);
        const core::result<std::string> found = find_dotnet_sdk(CONJURE_ENGINE_TEST_DOTNET, sourceRoot, 120.0);
        CONJURE_REQUIRE_OK(found);
        CHECK(is_usable_dotnet_sdk_version(found.value()));
    }
#endif
}
