#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "support_child_runner.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"

#include <memory>
#include <stop_token>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

namespace {
    CONJURE_DEFINE_LOG_CATEGORY(LOG_SUPPORT_TEST, "support_test");

    constexpr usize GLOBAL_RING_CAPACITY = 4096;

    // CONJURE_LOG_TRACE and CONJURE_LOG_DEBUG compile to nothing in Distribution (log.h).
    constexpr u32 VERBOSE_MESSAGES_PER_CALL = CONJURE_DISTRIBUTION ? 0 : 1;
}

TEST_SUITE("testing/log_capture") {

    TEST_CASE("log_capture - counts per level") {
        log_capture logs = {};

        CONJURE_LOG_TRACE(LOG_SUPPORT_TEST, "t");
        CONJURE_LOG_DEBUG(LOG_SUPPORT_TEST, "d");
        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "i {}", 1);
        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "i {}", 2);
        CONJURE_LOG_WARN(LOG_SUPPORT_TEST, "w");

        CHECK(logs.count() == 3 + 2 * VERBOSE_MESSAGES_PER_CALL);
        CHECK(logs.count(log_level::TRACE) == VERBOSE_MESSAGES_PER_CALL);
        CHECK(logs.count(log_level::DEBUG) == VERBOSE_MESSAGES_PER_CALL);
        CHECK(logs.count(log_level::INFO) == 2);
        CHECK(logs.count(log_level::WARN) == 1);
        CHECK(logs.count(log_level::ERR) == 0);
        CHECK(logs.count(log_level::FATAL) == 0);
    }

    TEST_CASE("log_capture - contains matches a substring at one level or at any level") {
        log_capture logs = {};

        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "device created: {}", "null");
        CONJURE_LOG_WARN(LOG_SUPPORT_TEST, "low memory");

        CHECK(logs.contains(log_level::INFO, "created"));
        CHECK_FALSE(logs.contains(log_level::WARN, "created"));
        CHECK(logs.contains("low"));
        CHECK_FALSE(logs.contains("missing"));
    }

    TEST_CASE("log_capture - entries carry level, category and text in order") {
        log_capture logs = {};

        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "first");
        CONJURE_LOG_WARN(LOG_SUPPORT_TEST, "second");

        const std::vector<captured_log_entry> entries = logs.entries();
        REQUIRE(entries.size() == 2);
        CHECK(entries[0].level == log_level::INFO);
        CHECK(entries[0].category == "support_test");
        CHECK(entries[0].text == "first");
        CHECK(entries[1].level == log_level::WARN);
        CHECK(entries[1].text == "second");

        CHECK(logs.messages() == std::vector<std::string>{"first", "second"});
        CHECK(logs.messages(log_level::WARN) == std::vector<std::string>{"second"});
    }

    TEST_CASE("log_capture - clear forgets every message") {
        log_capture logs = {};
        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "gone");

        logs.clear();

        CHECK(logs.count() == 0);
        CHECK_FALSE(logs.contains("gone"));
    }

    TEST_CASE("log_capture - captures from helper threads") {
        log_capture logs = {};
        {
            jobs::dedicated_thread worker("support-log", [](std::stop_token) {
                for (int index = 0; index < 100; ++index) {
                    CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "worker {}", index);
                }
            });
        }

        CHECK(logs.count(log_level::INFO) == 100);
        CHECK(logs.contains("worker 99"));
    }

    TEST_CASE("log_capture - nested captures each see every message") {
        log_capture outer = {};
        {
            log_capture inner = {};
            CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "both");
            CHECK(inner.count() == 1);
        }
        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "outer only");

        CHECK(outer.count() == 2);
    }

    TEST_CASE("log_capture - destructor passes on ERROR when errors are allowed") {
        log_capture logs = {};
        logs.allow_errors();

        CONJURE_LOG_ERROR(LOG_SUPPORT_TEST, "expected failure path");
        CONJURE_LOG_FATAL(LOG_SUPPORT_TEST, "expected fatal path");

        CHECK(logs.errors_allowed());
        CHECK(logs.count(log_level::ERR) == 1);
        CHECK(logs.contains(log_level::FATAL, "fatal"));
    }

    TEST_CASE("log_capture - restores the previous sinks and the console state") {
        int received = 0;
        auto sink = std::make_shared<callback_log_sink>([&received](const log_message &) {
            ++received;
        });
        log::add_sink(sink);
        const bool consoleBefore = log::console_enabled();

        {
            log_capture logs = {};
            CHECK_FALSE(log::console_enabled());
            CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "during");
        }
        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "after");
        log::remove_sink(sink);

        CHECK(received == 2);
        CHECK(log::console_enabled() == consoleBefore);
    }

    /*-----------------------------------------------------------------------*/
    // Through the child executable (the destructor fails the case; only visible from outside)

    TEST_CASE("log_capture - an unallowed ERROR fails the case") {
        const process_result run = run_support_child(
          {"-tc=support_child - unallowed error fails the log capture", "--conjure-isolated"}
        );

        CHECK(run.exitCode != 0);
        CHECK(run.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(run.standardOutput, "log_capture: unexpected ERROR"));
        CHECK(output_contains(run.standardOutput, "deliberate error for the support self-test"));
    }

    TEST_CASE("log_capture - an allowed ERROR passes the case") {
        const process_result run = run_support_child(
          {"-tc=support_child - allowed error passes the log capture", "--conjure-isolated"}
        );

        CHECK(run.exitCode == 0);
        CHECK_FALSE(output_contains(run.standardOutput, "log_capture: unexpected"));
    }

    /*-----------------------------------------------------------------------*/
    // Global ring buffer

    TEST_CASE("global ring - wraps at capacity and keeps the newest lines") {
        testing::detail::install_global_log_ring(4);
        testing::detail::clear_global_log_ring();
        CHECK(testing::detail::global_log_ring_capacity() == 4);

        for (int index = 0; index < 6; ++index) {
            CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "ring {}", index);
        }

        const std::vector<std::string> lines = testing::detail::global_log_ring_lines();
        REQUIRE(lines.size() == 4);
        CHECK(lines[0].find("ring 2") != std::string::npos);
        CHECK(lines[3].find("ring 5") != std::string::npos);
        CHECK(lines[3].find("[INFO ] [support_test]") != std::string::npos);

        testing::detail::clear_global_log_ring();
        CHECK(testing::detail::global_log_ring_lines().empty());

        testing::detail::install_global_log_ring(GLOBAL_RING_CAPACITY);
        CHECK(testing::detail::global_log_ring_capacity() == GLOBAL_RING_CAPACITY);
    }

    TEST_CASE("global ring - truncates long messages instead of allocating") {
        testing::detail::clear_global_log_ring();
        const std::string longText(1000, 'x');

        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "{}", longText);

        const std::vector<std::string> lines = testing::detail::global_log_ring_lines();
        REQUIRE(lines.size() == 1);
        CHECK(lines[0].size() < 400);
        CHECK(lines[0].find("xxxx") != std::string::npos);
    }

    TEST_CASE("global ring - survives clear_sinks through re-installation") {
        log::clear_sinks();
        testing::detail::install_global_log_ring(GLOBAL_RING_CAPACITY);
        testing::detail::clear_global_log_ring();

        CONJURE_LOG_INFO(LOG_SUPPORT_TEST, "back");

        CHECK(testing::detail::global_log_ring_lines().size() == 1);
    }
}
