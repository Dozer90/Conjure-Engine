#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/log.h"
#include "core/foundation/console_output.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    CONJURE_DEFINE_LOG_CATEGORY(LOG_TEST, "test");

    std::string read_file(const std::filesystem::path &path) {
        std::ifstream stream(path);
        std::stringstream contents = {};
        contents << stream.rdbuf();
        return contents.str();
    }
}

TEST_SUITE("core/log") {

    TEST_CASE("log - messages reach sinks with level, category and formatted text") {
        testing::log_capture capture = {};

        CONJURE_LOG_INFO(LOG_TEST, "hello {} {}", "world", 42);

        REQUIRE(capture.count() == 1);
        CHECK(capture.messages().front() == "hello world 42");
        CHECK(capture.messages(log_level::INFO).size() == 1);
    }

    TEST_CASE("log - messages carry source location, category name and thread id") {
        testing::log_capture quiet = {};
        log_message received = {};
        std::string receivedCategory = {};
        auto sink = std::make_shared<callback_log_sink>([&received, &receivedCategory](const log_message &message) {
            received = message;
            receivedCategory = std::string(message.category);
        });

        log::add_sink(sink);
        const int expectedLine = __LINE__ + 1;
        CONJURE_LOG_WARN(LOG_TEST, "located");
        log::remove_sink(sink);

        CHECK(received.level == log_level::WARN);
        CHECK(receivedCategory == "test");
        CHECK(received.line == expectedLine);
        CHECK(std::string_view(received.file) == __FILE__);
        CHECK(received.threadId == std::this_thread::get_id());
        CHECK(received.timestamp != std::chrono::system_clock::time_point{});
    }

    TEST_CASE("log - category level filters messages") {
        testing::log_capture capture = {};
        const log_level previous = LOG_TEST.level();

        LOG_TEST.set_level(log_level::WARN);
        CONJURE_LOG_INFO(LOG_TEST, "dropped");
        CONJURE_LOG_WARN(LOG_TEST, "kept");

        LOG_TEST.set_level(log_level::OFF);
        CHECK_FALSE(log::is_enabled(LOG_TEST, log_level::FATAL));
        CONJURE_LOG_FATAL(LOG_TEST, "dropped too");

        LOG_TEST.set_level(previous);

        REQUIRE(capture.count() == 1);
        CHECK(capture.messages().front() == "kept");
    }

    TEST_CASE("log - global level filters messages") {
        testing::log_capture capture = {};
        capture.allow_errors();

        log::set_global_level(log_level::ERR);
        CONJURE_LOG_WARN(LOG_TEST, "dropped");
        CONJURE_LOG_ERROR(LOG_TEST, "kept");
        log::set_global_level(log_level::TRACE);

        REQUIRE(capture.count() == 1);
        CHECK(capture.messages().front() == "kept");
        CHECK(log::global_level() == log_level::TRACE);
    }

    TEST_CASE("log - is_enabled respects both levels") {
        CHECK(log::is_enabled(LOG_TEST, log_level::INFO));
        CHECK_FALSE(log::is_enabled(LOG_TEST, log_level::OFF));

        log::set_global_level(log_level::FATAL);
        CHECK_FALSE(log::is_enabled(LOG_TEST, log_level::ERR));
        CHECK(log::is_enabled(LOG_TEST, log_level::FATAL));
        log::set_global_level(log_level::TRACE);
    }

    TEST_CASE("log - format arguments are only evaluated when the message is enabled") {
        testing::log_capture capture = {};
        int evaluations = 0;
        const auto count = [&evaluations] {
            return ++evaluations;
        };

        log::set_global_level(log_level::ERR);
        CONJURE_LOG_INFO(LOG_TEST, "{}", count());
        log::set_global_level(log_level::TRACE);
        CHECK(evaluations == 0);

        CONJURE_LOG_INFO(LOG_TEST, "{}", count());
        CHECK(evaluations == 1);
    }

    TEST_CASE("log - categories register themselves and can be adjusted by name") {
        log_category *found = log::find_category("test");
        REQUIRE(found == &LOG_TEST);

        CHECK(log::set_category_level("test", log_level::DEBUG));
        CHECK(LOG_TEST.level() == log_level::DEBUG);
        LOG_TEST.set_level(log_level::TRACE);

        CHECK_FALSE(log::set_category_level("does-not-exist", log_level::DEBUG));
        CHECK(log::find_category("does-not-exist") == nullptr);

        bool coreRegistered = false;
        for (const log_category *category : log::categories()) {
            if (category == &LOG_CORE) {
                coreRegistered = true;
            }
        }
        CHECK(coreRegistered);
    }

    TEST_CASE("log - scoped categories unregister on destruction and keep registration order") {
        const usize before = log::categories().size();
        {
            const log_category scoped("scoped-category", log_level::INFO);
            const std::vector<log_category *> all = log::categories();
            REQUIRE(all.size() == before + 1);
            CHECK(all.back() == &scoped);
            CHECK(log::find_category("scoped-category") == &scoped);
            CHECK(scoped.level() == log_level::INFO);
        }
        CHECK(log::categories().size() == before);
        CHECK(log::find_category("scoped-category") == nullptr);
    }

    TEST_CASE("log - level names round trip") {
        CHECK(to_string(log_level::TRACE) == "TRACE");
        CHECK(to_string(log_level::FATAL) == "FATAL");
        CHECK(to_string(log_level::OFF) == "OFF");
        CHECK(to_string(static_cast<log_level>(99)) == "UNKNOWN");
        CHECK(parse_log_level("warn") == log_level::WARN);
        CHECK(parse_log_level("ERROR") == log_level::ERR);
        CHECK(parse_log_level("Debug") == log_level::DEBUG);
        CHECK(parse_log_level("off") == log_level::OFF);
        CHECK(parse_log_level("") == log_level::OFF);
        CHECK(parse_log_level("bogus") == log_level::OFF);
    }

    TEST_CASE("log - file sink writes formatted lines and can append") {
        testing::log_capture quiet = {};
        testing::temp_directory directory("core_log");
        const std::filesystem::path path = directory.path() / "logs" / "engine.log";

        {
            auto sink = std::make_shared<file_log_sink>(path);
            REQUIRE(sink->is_open());
            log::add_sink(sink);
            CONJURE_LOG_INFO(LOG_TEST, "to file");
            log::flush();
            log::remove_sink(sink);
        }
        CHECK(read_file(path).find("[INFO ] [test] to file") != std::string::npos);

        {
            auto appending = std::make_shared<file_log_sink>(path, false);
            REQUIRE(appending->is_open());
            log::add_sink(appending);
            CONJURE_LOG_WARN(LOG_TEST, "appended");
            log::remove_sink(appending);
        }
        const std::string appended = read_file(path);
        CHECK(appended.find("to file") != std::string::npos);
        CHECK(appended.find("[WARN ] [test] appended") != std::string::npos);

        {
            auto truncating = std::make_shared<file_log_sink>(path, true);
            REQUIRE(truncating->is_open());
        }
        CHECK(read_file(path).empty());
    }

    TEST_CASE("log - file sink on an unwritable path reports not open and drops messages") {
        testing::log_capture quiet = {};
        testing::temp_directory directory("core_log_bad");
        const std::filesystem::path blocker = directory.write_file("not_a_directory", "x");

        auto sink = std::make_shared<file_log_sink>(blocker / "engine.log");
        CHECK_FALSE(sink->is_open());

        log::add_sink(sink);
        CONJURE_LOG_INFO(LOG_TEST, "dropped silently");
        sink->flush();
        log::remove_sink(sink);
        CHECK(true);
    }

    TEST_CASE("log - callback sink receives messages and sinks can be removed") {
        testing::log_capture quiet = {};
        int received = 0;
        auto sink = std::make_shared<callback_log_sink>([&received](const log_message &) {
            ++received;
        });

        log::add_sink(sink);
        CONJURE_LOG_INFO(LOG_TEST, "one");
        log::remove_sink(sink);
        CONJURE_LOG_INFO(LOG_TEST, "two");

        CHECK(received == 1);
    }

    TEST_CASE("log - null sinks are ignored and unknown sinks can be removed") {
        testing::log_capture capture = {};

        log::add_sink(nullptr);
        auto unknown = std::make_shared<callback_log_sink>([](const log_message &) {});
        log::remove_sink(unknown);

        CONJURE_LOG_INFO(LOG_TEST, "still works");
        CHECK(capture.count() == 1);
    }

    TEST_CASE("log - clear_sinks removes every sink") {
        testing::log_capture outer = {};
        int received = 0;
        auto sink = std::make_shared<callback_log_sink>([&received](const log_message &) {
            ++received;
        });
        log::add_sink(sink);

        log::clear_sinks();
        CONJURE_LOG_INFO(LOG_TEST, "after clear");

        CHECK(received == 0);
        CHECK(outer.count() == 0);
    }

    TEST_CASE("log - a sink may log and change the sink list from inside write") {
        testing::log_capture capture = {};
        std::shared_ptr<callback_log_sink> reentrant = nullptr;
        int inner = 0;
        reentrant = std::make_shared<callback_log_sink>([&inner, &reentrant](const log_message &message) {
            if (message.text == "outer") {
                ++inner;
                CONJURE_LOG_INFO(LOG_TEST, "inner");
                log::remove_sink(reentrant);
            }
        });

        log::add_sink(reentrant);
        CONJURE_LOG_INFO(LOG_TEST, "outer");
        CONJURE_LOG_INFO(LOG_TEST, "outer");

        CHECK(inner == 1);
        CHECK(capture.contains("inner"));
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F253: console sink routes WARN and above to stderr and colours only when asked"
    ) {
        CHECK(core::detail::console_stream_for(log_level::TRACE) == stdout);
        CHECK(core::detail::console_stream_for(log_level::DEBUG) == stdout);
        CHECK(core::detail::console_stream_for(log_level::INFO) == stdout);
        CHECK(core::detail::console_stream_for(log_level::WARN) == stderr);
        CHECK(core::detail::console_stream_for(log_level::ERR) == stderr);
        CHECK(core::detail::console_stream_for(log_level::FATAL) == stderr);

        log_message info = {};
        info.level = log_level::INFO;
        info.category = "test";
        info.text = "plain line";
        info.timestamp = std::chrono::system_clock::now();
        log_message failure = info;
        failure.level = log_level::ERR;
        failure.text = "coloured line";

        testing::temp_directory directory("core_log_console");
        const std::filesystem::path path = directory.file("console.txt");
        std::FILE *file = std::fopen(path.string().c_str(), "wb");
        REQUIRE(file != nullptr);

        core::detail::write_console_line(file, info, false);
        core::detail::write_console_line(file, failure, true);

        // Read before closing: the buffered INFO line is only on disk because the ERR line flushed.
        const std::string plainLine = format_log_line(info) + "\n";
        const std::string colouredLine = "\x1b[31m" + format_log_line(failure) + "\x1b[0m\n";
        CHECK(read_file(path) == plainLine + colouredLine);
        std::fclose(file);

        // The sink itself, plain and coloured, on the process's console.
        console_log_sink plain(false);
        plain.write(info);
        plain.flush();
        console_log_sink coloured(true);
        coloured.write(info);
        coloured.flush();
    }

    TEST_CASE("log - console can be disabled and re-enabled") {
        const bool before = log::console_enabled();
        log::set_console_enabled(!before);
        CHECK(log::console_enabled() == !before);
        log::set_console_enabled(before);
        CHECK(log::console_enabled() == before);
    }

    TEST_CASE("log - logging from many threads delivers every message exactly once") {
        testing::log_capture capture = {};
        constexpr int THREAD_COUNT = 8;
        constexpr int PER_THREAD = 500;

        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([threadIndex] {
                for (int index = 0; index < PER_THREAD; ++index) {
                    CONJURE_LOG_INFO(LOG_TEST, "thread {} message {}", threadIndex, index);
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(capture.count() == static_cast<usize>(THREAD_COUNT * PER_THREAD));
    }

    TEST_CASE("log - format_log_line produces the documented layout") {
        log_message message = {};
        message.level = log_level::WARN;
        message.category = "renderer";
        message.text = "low memory";
        message.timestamp = std::chrono::system_clock::now();

        const std::string line = format_log_line(message);
        REQUIRE(line.size() > 12);
        CHECK(line[2] == ':');
        CHECK(line[5] == ':');
        CHECK(line[8] == '.');
        CHECK(line[12] == ' ');
        CHECK(line.find("[WARN ] [renderer] low memory") != std::string::npos);

        message.level = log_level::ERR;
        CHECK(format_log_line(message).find("[ERROR] [renderer]") != std::string::npos);
        message.level = log_level::FATAL;
        CHECK(format_log_line(message).find("[FATAL] [renderer]") != std::string::npos);
    }

    TEST_CASE("log - format_log_line zero-pads the time of day") {
        log_message message = {};
        message.level = log_level::INFO;
        message.category = "c";
        message.text = "t";
        // Epoch + 5 seconds; whatever the local offset, minutes and seconds stay small.
        message.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(5));

        const std::string line = format_log_line(message);
        CHECK(line.substr(3, 6) == "00:05.");
        CHECK(line.substr(9, 3) == "000");
    }
}
