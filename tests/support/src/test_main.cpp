// Shared doctest entry point of every Conjure test executable (docs/design/08_testing.md 3.1).
//
// Compiled into each test executable by conjure_add_test_module(), never into the support library:
// the compile definitions it reads (CONJURE_TEST_*_ROOT, CONJURE_TEST_EXECUTABLE_NAME) are set per
// executable, and the doctest listener below must be registered from an object the linker keeps.
//
//   1. --conjure-* arguments (environment variables as fallback) -> test_environment
//   2. quiet console: the ring-buffer sink replaces the console sink; a doctest listener prints the
//      ring when a case fails
//   3. the assert handler records inside expect_assert scopes and fails the case (then aborts)
//      otherwise
//   4. seed the default random engine, designate the main thread, print "seed=<n> threads=<n>" to
//      stderr (stdout belongs to the reporter: xml and junit documents must stay well formed), warm
//      doctest's per-thread INFO stack so message macros do not look like leaks to memory_guard, run

#define DOCTEST_CONFIG_IMPLEMENT
#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/random.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#ifndef CONJURE_TEST_EXECUTABLE_NAME
    #define CONJURE_TEST_EXECUTABLE_NAME "conjure_tests"
#endif
#ifndef CONJURE_TEST_REPO_ASSET_ROOT
    #define CONJURE_TEST_REPO_ASSET_ROOT ""
#endif
#ifndef CONJURE_TEST_DOWNLOADED_ASSET_ROOT
    #define CONJURE_TEST_DOWNLOADED_ASSET_ROOT ""
#endif
#ifndef CONJURE_TEST_OUTPUT_ROOT
    #define CONJURE_TEST_OUTPUT_ROOT ""
#endif
#ifndef CONJURE_TEST_TEMP_ROOT
    #define CONJURE_TEST_TEMP_ROOT ""
#endif
#ifndef CONJURE_TEST_GOLDEN_IMAGES
    #define CONJURE_TEST_GOLDEN_IMAGES 0
#endif

using namespace conjure;
using namespace conjure::testing;

namespace {

    /*=======================================================================*/
    /*=======================================================================*/
    // Listener: dumps the captured log when a case fails

    class conjure_listener final : public doctest::IReporter {
        public:
            explicit conjure_listener(const doctest::ContextOptions &) {}
            ~conjure_listener() override = default;

        public:
            void report_query(const doctest::QueryData &) override {}
            void test_run_start() override {}
            void test_run_end(const doctest::TestRunStats &) override {}

            void test_case_start(const doctest::TestCaseData &) override {
                // Re-register the ring in case a test cleared the sink list, then start clean.
                detail::install_global_log_ring(detail::global_log_ring_capacity());
                detail::clear_global_log_ring();
            }

            void test_case_reenter(const doctest::TestCaseData &) override {}

            void test_case_end(const doctest::CurrentTestCaseStats &stats) override {
                if (stats.testCaseSuccess) {
                    return;
                }
                const std::vector<std::string> lines = detail::global_log_ring_lines();
                if (lines.empty()) {
                    return;
                }
                std::fputs("--- log captured during the failed test case ---\n", stderr);
                for (const std::string &line : lines) {
                    std::fputs(line.c_str(), stderr);
                    std::fputc('\n', stderr);
                }
                std::fflush(stderr);
            }

            void test_case_exception(const doctest::TestCaseException &) override {}
            void subcase_start(const doctest::SubcaseSignature &) override {}
            void subcase_end() override {}
            void log_assert(const doctest::AssertData &) override {}
            void log_message(const doctest::MessageData &) override {}
            void test_case_skipped(const doctest::TestCaseData &) override {}
    };

    REGISTER_LISTENER("conjure", 1, conjure_listener);


    /*=======================================================================*/
    /*=======================================================================*/
    // Argument and environment parsing

    constexpr usize LOG_RING_CAPACITY = 4096;

    std::optional<u64> parse_u64(std::string_view text) {
        int base = 10;
        if (text.starts_with("0x") || text.starts_with("0X")) {
            text.remove_prefix(2);
            base = 16;
        }
        if (text.empty()) {
            return std::nullopt;
        }

        u64 value = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size()) {
            return std::nullopt;
        }
        return value;
    }

    std::optional<std::string> environment_variable(const char *name) {
        const char *value = std::getenv(name);
        if (value == nullptr || *value == '\0') {
            return std::nullopt;
        }
        return std::string(value);
    }

    bool environment_flag(const char *name) {
        const std::optional<std::string> value = environment_variable(name);
        return value.has_value() && *value != "0" && *value != "false" && *value != "FALSE";
    }

    u32 default_thread_count() {
        const u32 hardware = std::thread::hardware_concurrency();
        return std::clamp<u32>(hardware == 0 ? 2 : hardware, 2, 8);
    }

    std::filesystem::path existing_directory_or_empty(const char *path) {
        if (*path == '\0') {
            return {};
        }
        std::error_code ignored = {};
        const std::filesystem::path candidate(path);
        return std::filesystem::is_directory(candidate, ignored) ? candidate : std::filesystem::path();
    }

    bool is_listing_argument(std::string_view argument) {
        // doctest's query modes print machine-readable lists; nothing at all is printed then, not
        // even the banner on stderr. doctest accepts every option with or without its "dt-" prefix
        // (--dt-list-test-cases, -dt-ltc), so the prefix is dropped before matching.
        std::string normalized(argument);
        if (normalized.starts_with("--dt-")) {
            normalized.erase(2, 3);
        } else if (normalized.starts_with("-dt-")) {
            normalized.erase(1, 3);
        }

        const std::string_view option(normalized);
        return option.starts_with("--list-test-cases") || option.starts_with("-ltc")
          || option.starts_with("--list-test-suites") || option.starts_with("-lts")
          || option.starts_with("--list-reporters") || option.starts_with("-lr") || option.starts_with("--count")
          || option == "-c" || option.starts_with("--version") || option == "-v" || option.starts_with("--help")
          || option == "-h" || option == "-?";
    }

    // Pre-grows doctest's thread_local INFO context stack (a std::vector that keeps its storage once
    // grown) so the first INFO / CAPTURE / CHECK_MESSAGE of a case does not show up as a live
    // allocation in a memory_guard. Nested scopes are needed because capacity follows the deepest
    // simultaneous nesting, not the number of calls; see memory_guard.h for the remaining limits.
    constexpr u32 INFO_STACK_WARM_DEPTH = 16;

    void warm_info_stack(u32 depth) {
        if (depth == 0) {
            return;
        }
        INFO("test_main warm-up");
        warm_info_stack(depth - 1);
    }

    struct parsed_arguments {
        test_environment environment = {};
        std::vector<char *> doctestArguments = {};
        bool listing = false;
        bool valid = true;
    };

    bool apply_conjure_argument(std::string_view argument, test_environment &environment) {
        if (argument == "--conjure-isolated") {
            environment.isolatedProcess = true;
            return true;
        }
        if (argument == "--conjure-update-golden") {
            environment.updateGoldens = true;
            return true;
        }
        if (argument == "--conjure-continue-on-assert") {
            environment.continueOnAssert = true;
            return true;
        }
        if (argument.starts_with("--conjure-threads=")) {
            const std::optional<u64> value = parse_u64(argument.substr(std::string_view("--conjure-threads=").size()));
            if (!value.has_value() || *value > 1024) {
                std::fprintf(stderr, "%s: invalid thread count in '%.*s'\n", CONJURE_TEST_EXECUTABLE_NAME,
                  static_cast<int>(argument.size()), argument.data());
                return false;
            }
            environment.jobThreadCount = static_cast<u32>(*value);
            return true;
        }
        if (argument.starts_with("--conjure-seed=")) {
            const std::optional<u64> value = parse_u64(argument.substr(std::string_view("--conjure-seed=").size()));
            if (!value.has_value()) {
                std::fprintf(stderr, "%s: invalid seed in '%.*s'\n", CONJURE_TEST_EXECUTABLE_NAME,
                  static_cast<int>(argument.size()), argument.data());
                return false;
            }
            environment.seed = *value;
            return true;
        }

        std::fprintf(stderr, "%s: unknown option '%.*s' (known: --conjure-isolated, --conjure-update-golden, "
          "--conjure-continue-on-assert, --conjure-threads=N, --conjure-seed=N)\n", CONJURE_TEST_EXECUTABLE_NAME,
          static_cast<int>(argument.size()), argument.data());
        return false;
    }

    parsed_arguments parse_arguments(int argc, char **argv) {
        parsed_arguments parsed = {};
        test_environment &environment = parsed.environment;

        // Compile-time facts and environment variables first; arguments override them.
        environment.executableName = CONJURE_TEST_EXECUTABLE_NAME;
        environment.repoAssetRoot = std::filesystem::path(CONJURE_TEST_REPO_ASSET_ROOT);
        environment.downloadedAssetRoot = existing_directory_or_empty(CONJURE_TEST_DOWNLOADED_ASSET_ROOT);
        environment.outputRoot = std::filesystem::path(CONJURE_TEST_OUTPUT_ROOT);
        environment.temporaryRoot = std::filesystem::path(CONJURE_TEST_TEMP_ROOT);
        environment.goldenEnabled = CONJURE_TEST_GOLDEN_IMAGES != 0;
        environment.jobThreadCount = default_thread_count();
        environment.realWindowAllowed = environment_flag("CONJURE_TEST_WINDOW");
        environment.isolatedProcess = environment_flag("CONJURE_TEST_ISOLATED");
        environment.updateGoldens = environment_flag("CONJURE_TEST_UPDATE_GOLDEN");
        environment.continueOnAssert = environment_flag("CONJURE_TEST_CONTINUE_ON_ASSERT");

        // The one variable that overrides a compile definition (test_environment.h): it lets the
        // downloaded-assets precondition be exercised against a scratch directory.
        if (const std::optional<std::string> root = environment_variable("CONJURE_TEST_DOWNLOADED_ASSET_ROOT")) {
            environment.downloadedAssetRoot = existing_directory_or_empty(root->c_str());
        }

        if (const std::optional<std::string> seed = environment_variable("CONJURE_TEST_SEED")) {
            if (const std::optional<u64> value = parse_u64(*seed)) {
                environment.seed = *value;
            } else {
                std::fprintf(stderr, "%s: ignoring invalid CONJURE_TEST_SEED '%s'\n", CONJURE_TEST_EXECUTABLE_NAME,
                  seed->c_str());
            }
        }
        if (const std::optional<std::string> threads = environment_variable("CONJURE_TEST_THREADS")) {
            const std::optional<u64> value = parse_u64(*threads);
            if (value.has_value() && *value <= 1024) {
                environment.jobThreadCount = static_cast<u32>(*value);
            } else {
                std::fprintf(stderr, "%s: ignoring invalid CONJURE_TEST_THREADS '%s'\n", CONJURE_TEST_EXECUTABLE_NAME,
                  threads->c_str());
            }
        }

        parsed.doctestArguments.reserve(static_cast<usize>(argc) + 1);
        parsed.doctestArguments.push_back(argv[0]);
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            if (argument.starts_with("--conjure-")) {
                parsed.valid = apply_conjure_argument(argument, environment) && parsed.valid;
                continue;
            }
            if (is_listing_argument(argument)) {
                parsed.listing = true;
            }
            parsed.doctestArguments.push_back(argv[index]);
        }
        return parsed;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Entry point

int main(int argc, char **argv) {
    parsed_arguments parsed = parse_arguments(argc, argv);
    if (!parsed.valid) {
        return 3;
    }
    detail::set_environment(parsed.environment);

    // Quiet console: the ring buffer is the only sink; the listener prints it on failure.
    core::log::set_console_enabled(false);
    detail::install_global_log_ring(LOG_RING_CAPACITY);

    core::set_assert_handler(&detail::testing_assert_handler);

    core::random::seed(parsed.environment.seed);
    core::jobs::set_main_thread();

    // Diagnostics go to stderr: stdout carries the reporter's document (console, xml or junit).
    if (!parsed.listing) {
        std::fprintf(stderr, "seed=%llu threads=%u\n", static_cast<unsigned long long>(parsed.environment.seed),
          parsed.environment.jobThreadCount);
        std::fflush(stderr);
    }

    doctest::Context context(static_cast<int>(parsed.doctestArguments.size()), parsed.doctestArguments.data());

    warm_info_stack(INFO_STACK_WARM_DEPTH);
    const int result = context.run();

    core::log::flush();
    return result;
}
