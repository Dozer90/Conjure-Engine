#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "system_test_support.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "core/jobs/thread_name.h"
#include "core/system/system_internal.h"

// current_process_id is compared with the operating system's own answer.
#if CONJURE_PLATFORM_WINDOWS
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <semaphore>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

// The conjure_process_child helpers system_test_support.h declares.
namespace process_child_support {
    const std::filesystem::path &child_path() {
        static const std::filesystem::path CHILD_PATH = CONJURE_PROCESS_CHILD_PATH;
        return CHILD_PATH;
    }

    process_desc child(std::vector<std::string> arguments) {
        process_desc desc = {};
        desc.executable = child_path().generic_string();
        desc.arguments = std::move(arguments);
        return desc;
    }

    std::string without_carriage_returns(std::string text) {
        std::erase(text, '\r');
        return text;
    }
}

using process_child_support::child;
using process_child_support::child_path;
using process_child_support::without_carriage_returns;

namespace {
    using steady_clock = std::chrono::steady_clock;

    // Linux children shed inherited descriptors only when the backend can add a closefrom action
    // (system_internal.h); an older C library keeps the limitation documented in
    // engine/src/core/system/README.md.
    constexpr bool CHILDREN_INHERIT_ONLY_STANDARD_STREAMS =
      CONJURE_PLATFORM_LINUX == 0 || CONJURE_SPAWN_CLOSEFROM_AVAILABLE == 1;

    // process_desc arguments are UTF-8.
    std::string utf8_of(const std::filesystem::path &path) {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char *>(text.data()), text.size());
    }

    f64 seconds_since(steady_clock::time_point start) {
        return std::chrono::duration<f64>(steady_clock::now() - start).count();
    }

    bool all_x(const std::string &text) {
        for (const char character : text) {
            if (character != 'x') {
                return false;
            }
        }
        return true;
    }

    // The calling process's identifier, asked of the operating system directly.
    u32 operating_system_process_id() {
#if CONJURE_PLATFORM_WINDOWS
        return static_cast<u32>(GetCurrentProcessId());
#else
        return static_cast<u32>(getpid());
#endif
    }

    // Polls until the child itself has exited (its grandchildren may live on).
    void wait_until_exited(const process &running) {
        const steady_clock::time_point begin = steady_clock::now();
        while (running.is_running() && seconds_since(begin) < 30.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        REQUIRE_FALSE(running.is_running());
    }

    // Changes the test process's current directory for one scope.
    class scoped_current_path {
        private:
            std::filesystem::path mPrevious = {};

        public:
            explicit scoped_current_path(const std::filesystem::path &directory) {
                std::error_code ignored = {};
                mPrevious = std::filesystem::current_path(ignored);
                std::filesystem::current_path(directory, ignored);
                REQUIRE_FALSE(static_cast<bool>(ignored));
            }

            ~scoped_current_path() {
                std::error_code ignored = {};
                std::filesystem::current_path(mPrevious, ignored);
            }

            scoped_current_path(const scoped_current_path &) = delete;
            scoped_current_path &operator=(const scoped_current_path &) = delete;
    };
}

TEST_SUITE("core/process") {

    TEST_CASE("process - run captures standard output and reports exit code zero") {
        const result<process_result> finished = process::run(child({"echo", "hello", "world"}));

        REQUIRE(finished.has_value());
        CHECK(without_carriage_returns(finished->standardOutput) == "hello world\n");
        CHECK(finished->standardError.empty());
        CHECK(finished->exitCode == 0);
        CHECK_FALSE(finished->timedOut);
        CHECK(finished->elapsedSeconds >= 0.0);
    }

    TEST_CASE("process - run captures standard error separately from standard output") {
        const result<process_result> finished = process::run(child({"both"}));

        REQUIRE(finished.has_value());
        CHECK(without_carriage_returns(finished->standardOutput) == "out\n");
        CHECK(without_carriage_returns(finished->standardError) == "err\n");
    }

    TEST_CASE("process - mergeStderr interleaves standard error into standard output") {
        process_desc desc = child({"both"});
        desc.mergeStderr = true;
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        CHECK(without_carriage_returns(finished->standardOutput) == "out\nerr\n");
        CHECK(finished->standardError.empty());
    }

    TEST_CASE("process - a child that writes nothing yields empty strings") {
        const result<process_result> finished = process::run(child({"exit", "0"}));

        REQUIRE(finished.has_value());
        CHECK(finished->standardOutput.empty());
        CHECK(finished->standardError.empty());
    }

    TEST_CASE("process - exit codes are reported and are not errors") {
        const result<process_result> fortyTwo = process::run(child({"exit", "42"}));
        REQUIRE(fortyTwo.has_value());
        CHECK(fortyTwo->exitCode == 42);
        CHECK_FALSE(fortyTwo->timedOut);

        const result<process_result> one = process::run(child({"exit", "1"}));
        REQUIRE(one.has_value());
        CHECK(one->exitCode == 1);

        // The helper exits with 2 for an unknown mode and explains on stderr.
        const result<process_result> usage = process::run(child({"no-such-mode"}));
        REQUIRE(usage.has_value());
        CHECK(usage->exitCode == 2);
        CHECK(usage->standardError.find("no-such-mode") != std::string::npos);
    }

    TEST_CASE("process - arguments survive the round trip through the operating system") {
        const std::vector<std::string> tricky = {
            "plain",
            "with space",
            "two  spaces",
            "tab\there",
            "",
            "quote\"inside",
            "back\\slash",
            "trailing\\",
            "two trailing\\\\",
            "both \"quoted\" \\ mix",
            "\\\"",
            "\"",
            "ends with quote\"",
            "c:\\path with space\\file.txt",
            "--flag=value",
            "%PATH%",
            "$HOME",
            "a&b|c>d<e^f",
        };
        std::vector<std::string> arguments = {"args"};
        arguments.insert(arguments.end(), tricky.begin(), tricky.end());

        std::string expected = {};
        for (const std::string &value : tricky) {
            expected += "[" + value + "]\n";
        }

        const result<process_result> finished = process::run(child(arguments));
        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == expected);
    }

    TEST_CASE("process - environment overrides are visible to the child and inherited variables remain") {
        process_desc desc = child({"env", "CONJURE_TEST_VARIABLE"});
        desc.environment = {{"CONJURE_TEST_VARIABLE", "hello from parent"}};
        const result<process_result> finished = process::run(desc);
        REQUIRE(finished.has_value());
        CHECK(without_carriage_returns(finished->standardOutput) == "hello from parent\n");

        // Overriding one variable must not drop the rest of the environment.
        process_desc inherited = child({"env", "PATH"});
        inherited.environment = {{"CONJURE_TEST_VARIABLE", "set"}};
        const result<process_result> withPath = process::run(inherited);
        REQUIRE(withPath.has_value());
        CHECK(without_carriage_returns(withPath->standardOutput) != "<unset>\n");
        CHECK(withPath->standardOutput.size() > 1);
    }

    TEST_CASE("process - an empty environment value removes the variable") {
        process_desc desc = child({"env", "PATH"});
        desc.environment = {{"PATH", ""}};
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        CHECK(without_carriage_returns(finished->standardOutput) == "<unset>\n");
    }

    TEST_CASE("process - environment names are case-insensitive on Windows") {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            process_desc desc = child({"env", "PATH"});
            desc.environment = {{"path", "overridden"}};
            const result<process_result> finished = process::run(desc);

            REQUIRE(finished.has_value());
            CHECK(without_carriage_returns(finished->standardOutput) == "overridden\n");
        }
    }

    TEST_CASE("process - the working directory is applied") {
        const testing::temp_directory directory("core_process_cwd");
        process_desc desc = child({"cwd"});
        desc.workingDirectory = directory.path();
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        std::string reported = without_carriage_returns(finished->standardOutput);
        REQUIRE_FALSE(reported.empty());
        reported.pop_back();   // the newline

        std::error_code ignored = {};
        CHECK(std::filesystem::equivalent(std::filesystem::path(reported), directory.path(), ignored));
    }

    TEST_CASE("process - a missing working directory is NOT_FOUND before anything starts") {
        const testing::temp_directory directory("core_process_bad_cwd");
        process_desc desc = child({"cwd"});
        desc.workingDirectory = directory.path() / "does_not_exist";
        const result<process_result> finished = process::run(desc);

        REQUIRE_FALSE(finished.has_value());
        CHECK(finished.get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(finished.get_error().message().find("working directory") != std::string::npos);
    }

    TEST_CASE("process - a missing executable is NOT_FOUND with the operating system's message") {
        const testing::temp_directory directory("core_process_missing");
        process_desc desc = {};
        desc.executable = (directory.path() / "does_not_exist").generic_string();
        const result<process_result> finished = process::run(desc);

        REQUIRE_FALSE(finished.has_value());
        CHECK(finished.get_error().generic_code() == error_code::NOT_FOUND);
        const std::string &message = finished.get_error().message();
        CHECK(message.find("does_not_exist") != std::string::npos);
        CHECK(message.size() > desc.executable.size() + 10);   // carries the OS text, not just the path
    }

    TEST_CASE("process - inconsistent descriptions are INVALID_ARGUMENT") {
        process_desc empty = {};
        const result<process_result> noExecutable = process::run(empty);
        REQUIRE_FALSE(noExecutable.has_value());
        CHECK(noExecutable.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        process_desc negative = child({"exit", "0"});
        negative.timeoutSeconds = -1.0;
        const result<process_result> badTimeout = process::run(negative);
        REQUIRE_FALSE(badTimeout.has_value());
        CHECK(badTimeout.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        process_desc merged = child({"exit", "0"});
        merged.captureStdout = false;
        merged.mergeStderr = true;
        const result<process_result> badMerge = process::run(merged);
        REQUIRE_FALSE(badMerge.has_value());
        CHECK(badMerge.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<process> notStarted = process::start(empty);
        CHECK_FALSE(notStarted.has_value());
    }

    TEST_CASE("process - an embedded NUL in the executable, an argument or the environment is INVALID_ARGUMENT") {
        // The operating system takes NUL-terminated strings, so each of these would otherwise be cut short
        // silently: a different program, a truncated argument, a shortened variable.
        const std::string withNul("a\0b", 3);

        process_desc badExecutable = child({"exit", "0"});
        badExecutable.executable = child_path().generic_string() + withNul;
        process_desc badArgument = child({"echo", withNul});
        process_desc badName = child({"exit", "0"});
        badName.environment = {{"CONJURE_FINE", "1"}, {withNul, "1"}};
        process_desc badValue = child({"exit", "0"});
        badValue.environment = {{"CONJURE_NUL_VALUE", withNul}};

        const std::vector<std::pair<process_desc, std::string>> cases = {
            {badExecutable, "executable"},
            {badArgument, "arguments[1]"},
            {badName, "environment entry 1"},
            {badValue, "environment entry 0"},
        };
        for (const std::pair<process_desc, std::string> &entry : cases) {
            const std::string &field = entry.second;
            CAPTURE(field);
            const result<process_result> finished = process::run(entry.first);
            REQUIRE_FALSE(finished.has_value());
            CHECK(finished.get_error().generic_code() == error_code::INVALID_ARGUMENT);
            CHECK(finished.get_error().message().find(field) != std::string::npos);
            CHECK(finished.get_error().message().find("NUL") != std::string::npos);
        }
    }

    TEST_CASE("process - batch files are refused on Windows because cmd.exe would re-parse the arguments") {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            const testing::temp_directory directory("core_process_batch");
            const std::filesystem::path lower = directory.write_file("tool.cmd", "@echo off\r\necho %1\r\n");
            const std::filesystem::path upper = directory.write_file("TOOL.BAT", "@echo off\r\necho %1\r\n");

            // Win32 drops trailing dots and spaces, so these spellings still run cmd.exe.
            const std::vector<std::string> executables = {
                utf8_of(lower),
                utf8_of(upper),
                utf8_of(lower) + ".",
                utf8_of(upper) + ". ",
            };
            for (const std::string &executable : executables) {
                CAPTURE(executable);
                process_desc desc = {};
                desc.executable = executable;
                desc.arguments = {"safe&echo injected"};
                const result<process> started = process::start(desc);
                REQUIRE_FALSE(started.has_value());
                CHECK(started.get_error().generic_code() == error_code::INVALID_ARGUMENT);
                CHECK(started.get_error().message().find("cmd.exe") != std::string::npos);
            }
        }
    }

    TEST_CASE("process - the timeout kills a child that runs too long") {
        process_desc desc = child({"sleep", "10000"});
        desc.timeoutSeconds = 0.5;
        const steady_clock::time_point begin = steady_clock::now();
        const result<process_result> finished = process::run(desc);
        const f64 elapsed = seconds_since(begin);

        REQUIRE(finished.has_value());
        CHECK(finished->timedOut);
        CHECK(finished->exitCode == process::KILLED_EXIT_CODE);
        CHECK(finished->elapsedSeconds >= 0.4);
        CHECK(elapsed < 8.0);
    }

    TEST_CASE("process - a child that finishes within the timeout is not reported as timed out") {
        process_desc desc = child({"echo", "quick"});
        desc.timeoutSeconds = 30.0;
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        CHECK_FALSE(finished->timedOut);
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "quick\n");
    }

    TEST_CASE("process - large output on both streams is drained without deadlock") {
        constexpr usize BYTES_PER_STREAM = 1u << 20;
        const result<process_result> finished = process::run(child({"flood", std::to_string(BYTES_PER_STREAM)}));

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(finished->standardOutput.size() == BYTES_PER_STREAM);
        CHECK(finished->standardError.size() == BYTES_PER_STREAM);
        CHECK(all_x(finished->standardOutput));
        CHECK(all_x(finished->standardError));
    }

    TEST_CASE("process - large merged output keeps every byte") {
        constexpr usize BYTES_PER_STREAM = 1u << 18;
        process_desc desc = child({"flood", std::to_string(BYTES_PER_STREAM)});
        desc.mergeStderr = true;
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        CHECK(finished->standardOutput.size() == 2 * BYTES_PER_STREAM);
        CHECK(finished->standardError.empty());
        CHECK(all_x(finished->standardOutput));
    }

    TEST_CASE("process - uncaptured streams leave the result empty and need no pipes") {
        process_desc desc = child({"exit", "7"});
        desc.captureStdout = false;
        desc.captureStderr = false;
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 7);
        CHECK(finished->standardOutput.empty());
        CHECK(finished->standardError.empty());
    }

    TEST_CASE("process - standard input is the null device so children never wait for the parent") {
        const result<process_result> finished = process::run(child({"stdin"}));

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "0\n");
    }

    TEST_CASE("process - start, is_running and wait follow the child's lifetime") {
        result<process> started = process::start(child({"sleep", "300"}));
        REQUIRE(started.has_value());
        process &running = started.value();

        CHECK(running.is_running());
        CHECK(running.process_id() != 0);

        const result<process_result> finished = running.wait();
        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK_FALSE(finished->timedOut);
        CHECK(finished->elapsedSeconds >= 0.25);
        CHECK_FALSE(running.is_running());
        CHECK(running.process_id() != 0);
    }

    TEST_CASE("process - kill stops a running child and wait reports KILLED_EXIT_CODE") {
        result<process> started = process::start(child({"sleep", "10000"}));
        REQUIRE(started.has_value());
        process &running = started.value();
        REQUIRE(running.is_running());

        const steady_clock::time_point begin = steady_clock::now();
        running.kill();
        const result<process_result> finished = running.wait();

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == process::KILLED_EXIT_CODE);
        CHECK_FALSE(finished->timedOut);
        CHECK(seconds_since(begin) < 5.0);
        CHECK_FALSE(running.is_running());

        // Killing again, or after the child is gone, is harmless.
        running.kill();
    }

    TEST_CASE("process - kill from another thread unblocks wait") {
        result<process> started = process::start(child({"sleep", "10000"}));
        REQUIRE(started.has_value());
        process &running = started.value();

        std::thread killer([&running] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            running.kill();
        });
        const steady_clock::time_point begin = steady_clock::now();
        const result<process_result> finished = running.wait();
        killer.join();

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == process::KILLED_EXIT_CODE);
        CHECK_FALSE(finished->timedOut);
        CHECK(seconds_since(begin) < 5.0);
    }

    TEST_CASE("process - is_running polled from another thread while wait blocks keeps the exit code") {
        // The child runs until a marker file appears, and the marker is created only once the poller has
        // seen the child alive, so the check does not depend on how quickly a loaded runner schedules
        // either side.
        const testing::temp_directory directory("core_process_poll");
        const std::filesystem::path marker = directory.path() / "release";
        result<process> started = process::start(child({"wait-for", utf8_of(marker)}));
        REQUIRE(started.has_value());
        process &running = started.value();

        std::atomic<bool> stop = false;
        std::atomic<int> polls = 0;
        std::thread poller([&running, &stop, &polls] {
            while (!stop.load()) {
                if (running.is_running()) {
                    polls.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
        std::thread releaser([&polls, &marker] {
            const steady_clock::time_point begin = steady_clock::now();
            while (polls.load() == 0 && seconds_since(begin) < 30.0) {
                std::this_thread::yield();
            }
            std::ofstream release(marker);
        });

        process_desc exitDesc = child({"exit", "9"});
        const result<process_result> finished = running.wait();
        stop.store(true);
        poller.join();
        releaser.join();

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(polls.load() > 0);   // the poller saw the child alive at least once
        CHECK_FALSE(running.is_running());

        // The same with a non-zero code, so a lost status (KILLED_EXIT_CODE) or a made-up 0 would be caught.
        result<process> nonZero = process::start(exitDesc);
        REQUIRE(nonZero.has_value());
        std::atomic<bool> stopSecond = false;
        std::thread secondPoller([&nonZero, &stopSecond] {
            while (!stopSecond.load()) {
                CONJURE_UNUSED(nonZero->is_running());
            }
        });
        const result<process_result> nonZeroResult = nonZero->wait();
        stopSecond.store(true);
        secondPoller.join();
        REQUIRE(nonZeroResult.has_value());
        CHECK(nonZeroResult->exitCode == 9);
    }

    TEST_CASE("process - the child is placed into a job object on Windows") {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            // Job assignment failing is logged as a warning; a clean run proves kill() reaches
            // the whole tree.
            testing::log_capture capture = {};
            const result<process_result> finished = process::run(child({"exit", "0"}));
            REQUIRE(finished.has_value());
            CHECK_FALSE(capture.contains("no job object"));
            CHECK(capture.count(log_level::WARN) == 0);
        }
    }

    TEST_CASE("process - wait enforces the timeout of a started process") {
        process_desc desc = child({"sleep", "10000"});
        desc.timeoutSeconds = 0.3;
        result<process> started = process::start(desc);
        REQUIRE(started.has_value());

        const result<process_result> finished = started->wait();
        REQUIRE(finished.has_value());
        CHECK(finished->timedOut);
        CHECK(finished->exitCode == process::KILLED_EXIT_CODE);
    }

    TEST_CASE("process - read_output drains incrementally and wait returns the remainder") {
        result<process> started = process::start(child({"trickle", "5", "20"}));
        REQUIRE(started.has_value());
        process &running = started.value();

        std::string collected = {};
        while (running.is_running()) {
            running.read_output(collected);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const result<process_result> finished = running.wait();
        REQUIRE(finished.has_value());

        const std::string all = without_carriage_returns(collected + finished->standardOutput);
        CHECK(all == "line0\nline1\nline2\nline3\nline4\n");
        CHECK(finished->exitCode == 0);

        // Everything was handed out exactly once; nothing is left to read.
        std::string leftovers = {};
        CHECK(running.read_output(leftovers) == 0);
        CHECK(running.read_error(leftovers) == 0);
        CHECK(leftovers.empty());
    }

    TEST_CASE("process - read_output and read_error are safe from another thread while the child floods") {
        constexpr usize BYTES_PER_STREAM = 1u << 18;
        result<process> started = process::start(child({"flood", std::to_string(BYTES_PER_STREAM)}));
        REQUIRE(started.has_value());
        process &running = started.value();

        std::string outputFromThread = {};
        std::string errorFromThread = {};
        std::atomic<bool> stop = false;
        std::thread reader([&running, &outputFromThread, &errorFromThread, &stop] {
            while (!stop.load()) {
                running.read_output(outputFromThread);
                running.read_error(errorFromThread);
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        });

        const result<process_result> finished = running.wait();
        stop.store(true);
        reader.join();

        REQUIRE(finished.has_value());
        CHECK(outputFromThread.size() + finished->standardOutput.size() == BYTES_PER_STREAM);
        CHECK(errorFromThread.size() + finished->standardError.size() == BYTES_PER_STREAM);
        CHECK(all_x(outputFromThread));
        CHECK(all_x(errorFromThread));
        CHECK(all_x(finished->standardOutput));
        CHECK(all_x(finished->standardError));
    }

    TEST_CASE("process - wait fails with INVALID_STATE when not started or already waited") {
        process inert = {};
        const result<process_result> neverStarted = inert.wait();
        REQUIRE_FALSE(neverStarted.has_value());
        CHECK(neverStarted.get_error().generic_code() == error_code::INVALID_STATE);

        result<process> started = process::start(child({"exit", "3"}));
        REQUIRE(started.has_value());
        const result<process_result> first = started->wait();
        REQUIRE(first.has_value());
        CHECK(first->exitCode == 3);

        const result<process_result> second = started->wait();
        REQUIRE_FALSE(second.has_value());
        CHECK(second.get_error().generic_code() == error_code::INVALID_STATE);
    }

    TEST_CASE("process - a default-constructed process is inert") {
        process inert = {};
        CHECK_FALSE(inert.is_running());
        CHECK(inert.process_id() == 0);
        inert.kill();

        std::string output = {};
        CHECK(inert.read_output(output) == 0);
        CHECK(inert.read_error(output) == 0);
        CHECK(output.empty());
    }

    TEST_CASE("process - destroying a running process kills it promptly") {
        const steady_clock::time_point begin = steady_clock::now();
        {
            result<process> started = process::start(child({"sleep", "10000"}));
            REQUIRE(started.has_value());
            CHECK(started->is_running());
        }
        CHECK(seconds_since(begin) < 5.0);
    }

    TEST_CASE("process - a process can be moved while running") {
        result<process> started = process::start(child({"sleep", "300"}));
        REQUIRE(started.has_value());

        process moved(std::move(started.value()));
        CHECK_FALSE(started->is_running());
        CHECK(started->process_id() == 0);
        CHECK(moved.is_running());

        process assigned = {};
        assigned = std::move(moved);
        CHECK_FALSE(moved.is_running());
        CHECK(assigned.is_running());

        const result<process_result> finished = assigned.wait();
        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
    }

    TEST_CASE("process - move assignment over a running process kills the old child") {
        result<process> longRunning = process::start(child({"sleep", "10000"}));
        REQUIRE(longRunning.has_value());
        result<process> quick = process::start(child({"exit", "5"}));
        REQUIRE(quick.has_value());

        const steady_clock::time_point begin = steady_clock::now();
        longRunning.value() = std::move(quick.value());
        CHECK(seconds_since(begin) < 5.0);

        const result<process_result> finished = longRunning->wait();
        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 5);
    }

    TEST_CASE("process - elapsedSeconds measures the run") {
        const result<process_result> finished = process::run(child({"sleep", "200"}));

        REQUIRE(finished.has_value());
        CHECK(finished->elapsedSeconds >= 0.15);
        CHECK(finished->elapsedSeconds < 30.0);
    }

    TEST_CASE("process - children started concurrently from two threads do not inherit each other's pipes") {
        // A pipe end leaks only while another start() is in flight: the slow child must not
        // receive the fast child's write end, or the fast child's reader never sees end-of-file
        // until the slow child dies. Both threads release the barrier and start at the same moment.
        // The slow child waits for a marker that is never created, so it outlives the fast one
        // however long the runner stalls; a watchdog kills it after 30 s so a leak fails the case
        // instead of hanging it.
        const testing::temp_directory directory("core_process_pipes");
        const std::string neverCreated = utf8_of(directory.path() / "never");
        std::barrier<> ready(2);
        std::optional<result<process>> slow = std::nullopt;
        std::optional<result<process>> fast = std::nullopt;

        std::thread slowStarter([&slow, &ready, &neverCreated] {
            ready.arrive_and_wait();
            slow.emplace(process::start(child({"wait-for", neverCreated})));
        });
        std::thread fastStarter([&fast, &ready] {
            ready.arrive_and_wait();
            fast.emplace(process::start(child({"echo", "fast"})));
        });
        slowStarter.join();
        fastStarter.join();

        REQUIRE(slow.has_value());
        REQUIRE(fast.has_value());
        REQUIRE(slow->has_value());
        REQUIRE(fast->has_value());

        process &slowProcess = slow->value();
        std::atomic<bool> done = false;
        std::atomic<bool> watchdogFired = false;
        std::thread watchdog([&slowProcess, &done, &watchdogFired] {
            const steady_clock::time_point begin = steady_clock::now();
            while (!done.load()) {
                if (seconds_since(begin) > 30.0) {
                    watchdogFired.store(true);
                    slowProcess.kill();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });

        const result<process_result> fastResult = fast->value().wait();
        const bool slowStillRunning = slowProcess.is_running();
        done.store(true);
        watchdog.join();

        REQUIRE(fastResult.has_value());
        CHECK(without_carriage_returns(fastResult->standardOutput) == "fast\n");
        CHECK_FALSE(watchdogFired.load());
        CHECK(slowStillRunning);

        slowProcess.kill();
        const result<process_result> slowResult = slowProcess.wait();
        REQUIRE(slowResult.has_value());
        CHECK(slowResult->exitCode == process::KILLED_EXIT_CODE);
    }

    TEST_CASE("process - children inherit no descriptor beyond the standard streams") {
        if constexpr (!CHILDREN_INHERIT_ONLY_STANDARD_STREAMS) {
            MESSAGE("glibc before 2.34 cannot close inherited descriptors in a spawned child");
            return;
        }

        // A descriptor without close-on-exec: libstdc++ opens file streams through fopen, which leaves
        // O_CLOEXEC off, so before the closefrom file action every Linux child inherited it. Windows limits
        // inheritance to the standard handles and the child reports 0 there by construction.
        const testing::temp_directory directory("core_process_fds");
        std::ofstream held(directory.path() / "held.txt");
        REQUIRE(held.is_open());

        const result<process_result> finished = process::run(child({"fds"}));
        held.close();

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "0\n");
    }

#if !CONJURE_PLATFORM_WINDOWS
    TEST_CASE("process - a child whose exit status is lost to an ignored SIGCHLD reports KILLED_EXIT_CODE") {
        // With SIGCHLD ignored the kernel reaps children itself and waitpid fails with ECHILD: the status is
        // gone, and it used to be reported as exit code 0, a success the child never claimed.
        testing::log_capture logs = {};
        const auto previous = std::signal(SIGCHLD, SIG_IGN);
        const result<process_result> finished = process::run(child({"exit", "3"}));
        std::signal(SIGCHLD, previous);

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == process::KILLED_EXIT_CODE);
        CHECK(logs.contains(log_level::WARN, "exit status was lost"));
    }
#endif

    TEST_CASE("process - a zero-length write by the child does not end the capture early") {
        const result<process_result> finished = process::run(child({"zero-write"}));

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "before\nafter\n");
    }

    TEST_CASE("process - standard error alone can be captured while standard output is inherited") {
        process_desc desc = child({"stderr", "only", "this"});
        desc.captureStdout = false;
        desc.captureStderr = true;
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(finished->standardOutput.empty());
        CHECK(without_carriage_returns(finished->standardError) == "only this\n");
    }

    TEST_CASE("process - environment names must be non-empty and free of '='") {
        process_desc emptyName = child({"exit", "0"});
        emptyName.environment = {{"", "value"}};
        const result<process_result> noName = process::run(emptyName);
        REQUIRE_FALSE(noName.has_value());
        CHECK(noName.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        process_desc equalsInName = child({"exit", "0"});
        equalsInName.environment = {{"NAME=1", "value"}};
        const result<process_result> badName = process::run(equalsInName);
        REQUIRE_FALSE(badName.has_value());
        CHECK(badName.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(badName.get_error().message().find("NAME=1") != std::string::npos);
    }

    TEST_CASE("process - removing every inherited variable leaves the child an empty environment") {
        const result<process_result> listed = process::run(child({"envlist"}));
        REQUIRE(listed.has_value());
        REQUIRE(listed->exitCode == 0);

        process_desc desc = child({"env", "PATH"});
        std::string name = {};
        for (const char character : without_carriage_returns(listed->standardOutput)) {
            if (character == '\n') {
                if (!name.empty()) {
                    desc.environment.emplace_back(name, "");
                }
                name.clear();
                continue;
            }
            name.push_back(character);
        }
        REQUIRE_FALSE(desc.environment.empty());

        const result<process_result> finished = process::run(desc);
        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "<unset>\n");
    }

    TEST_CASE("process - non-finite timeouts are rejected and huge ones are honoured") {
        process_desc notANumber = child({"exit", "0"});
        notANumber.timeoutSeconds = std::numeric_limits<f64>::quiet_NaN();
        const result<process_result> nan = process::run(notANumber);
        REQUIRE_FALSE(nan.has_value());
        CHECK(nan.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        process_desc infinite = child({"exit", "0"});
        infinite.timeoutSeconds = std::numeric_limits<f64>::infinity();
        const result<process_result> inf = process::run(infinite);
        REQUIRE_FALSE(inf.has_value());
        CHECK(inf.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        process_desc huge = child({"echo", "patient"});
        huge.timeoutSeconds = 1.0e300;
        const result<process_result> finished = process::run(huge);
        REQUIRE(finished.has_value());
        CHECK_FALSE(finished->timedOut);
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "patient\n");

        process_desc maximum = child({"echo", "max"});
        maximum.timeoutSeconds = std::numeric_limits<f64>::max();
        const result<process_result> atMax = process::run(maximum);
        REQUIRE(atMax.has_value());
        CHECK_FALSE(atMax->timedOut);
    }

    TEST_CASE("process - a relative executable path is resolved against the parent's directory") {
        // The header promises resolution against the parent's current directory, not against
        // workingDirectory, so the helper is addressed relative to its own directory while the
        // child runs somewhere else.
        const testing::temp_directory directory("core_process_relative");
        const scoped_current_path inChildDirectory(child_path().parent_path());

        process_desc desc = {};
        desc.executable = "./" + child_path().filename().generic_string();
        desc.arguments = {"cwd"};
        desc.workingDirectory = directory.path();
        const result<process_result> finished = process::run(desc);

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        std::string reported = without_carriage_returns(finished->standardOutput);
        REQUIRE_FALSE(reported.empty());
        reported.pop_back();   // the newline

        std::error_code ignored = {};
        CHECK(std::filesystem::equivalent(std::filesystem::path(reported), directory.path(), ignored));
    }

    TEST_CASE("process - process_id is the child's own identifier") {
        result<process> started = process::start(child({"pid"}));
        REQUIRE(started.has_value());
        const u32 reportedByParent = started->process_id();
        CHECK(reportedByParent != 0);

        const result<process_result> finished = started->wait();
        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);

        const std::string printed = without_carriage_returns(finished->standardOutput);
        REQUIRE_FALSE(printed.empty());
        CHECK(std::stoull(printed) == reportedByParent);
    }

    TEST_CASE("process - current_process_id is the calling process's operating system id") {
        const u32 reported = current_process_id();
        CHECK(reported != 0);
        CHECK(reported == operating_system_process_id());
    }

    TEST_CASE("process - children start with default signal dispositions") {
        // SIGINT is ignored for the duration of the spawn; the child must not inherit that.
        const auto previous = std::signal(SIGINT, SIG_IGN);
        const result<process_result> finished = process::run(child({"signals"}));
        std::signal(SIGINT, previous);

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "default\n");
    }

    TEST_CASE("process - a grandchild holding the output does not stall wait past the timeout") {
        process_desc desc = child({"spawn-sleeper", "60000"});
        desc.timeoutSeconds = 1.0;
        const steady_clock::time_point begin = steady_clock::now();
        const result<process_result> finished = process::run(desc);
        const f64 elapsed = seconds_since(begin);

        REQUIRE(finished.has_value());
        CHECK(elapsed < 10.0);
        CHECK(finished->timedOut);
        CHECK(finished->exitCode == 0);   // the child itself exited normally
        CHECK(without_carriage_returns(finished->standardOutput) == "spawned\n");
    }

    TEST_CASE("process - destroying a process whose grandchild holds the output returns promptly") {
        const steady_clock::time_point begin = steady_clock::now();
        {
            result<process> started = process::start(child({"spawn-sleeper", "60000"}));
            REQUIRE(started.has_value());
            wait_until_exited(started.value());
        }
        CHECK(seconds_since(begin) < 10.0);
    }

    TEST_CASE("process - kill reaches a grandchild after the child exited and unblocks wait") {
        result<process> started = process::start(child({"spawn-sleeper", "60000"}));
        REQUIRE(started.has_value());
        wait_until_exited(started.value());
        CHECK_FALSE(started->is_running());

        const steady_clock::time_point begin = steady_clock::now();
        started->kill();
        const result<process_result> finished = started->wait();

        REQUIRE(finished.has_value());
        CHECK(seconds_since(begin) < 10.0);
        CHECK_FALSE(finished->timedOut);
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "spawned\n");
    }

    TEST_CASE("regression - code sweep F233: a pipe reader runs on a named dedicated thread") {
        const std::array<std::string_view, 3> names = {
            detail::STANDARD_OUTPUT_READER_NAME,
            detail::STANDARD_ERROR_READER_NAME,
            detail::COMBINED_READER_NAME,
        };
        for (const std::string_view name : names) {
            CAPTURE(name);
            // Linux keeps 15 bytes of a thread name; a longer one would show up cut short there.
            CHECK_FALSE(name.empty());
            CHECK(name.size() <= jobs::detail::MAX_POSIX_THREAD_NAME_LENGTH);

            detail::completion_latch done = {};
            std::thread::id drainThread = {};
            std::string osName = {};
            {
                detail::pipe_reader reader = {};
                reader.start(name, done, [&drainThread, &osName] {
                    drainThread = std::this_thread::get_id();
                    osName = jobs::detail::current_thread_name();
                });
                reader.join();
            }

            CHECK(done.is_complete());
            CHECK(drainThread != std::thread::id());
            CHECK(drainThread != std::this_thread::get_id());
            // What a debugger or a profiler shows for the thread, as the operating system reports it.
            CHECK(osName == name);
        }
        CHECK(names[0] != names[1]);
        CHECK(names[0] != names[2]);
        CHECK(names[1] != names[2]);
    }

    TEST_CASE("regression - code sweep F233: a child's captured pipes are drained by pipe_reader threads only") {
        // The case above shows what a pipe_reader is; this one shows that the process backends use it.
        // A backend that went back to a thread of its own would still capture everything, but it would
        // start fewer pipe_readers than it has pipes to drain. Windows drains each captured pipe on its
        // own thread, POSIX drains both on one.
        constexpr u64 READERS_FOR_TWO_PIPES = CONJURE_PLATFORM_WINDOWS == 1 ? 2 : 1;

        u64 before = detail::started_pipe_reader_count();
        const result<process_result> both = process::run(child({"both"}));
        REQUIRE(both.has_value());
        CHECK(without_carriage_returns(both->standardOutput) == "out\n");
        CHECK(without_carriage_returns(both->standardError) == "err\n");
        CHECK(detail::started_pipe_reader_count() == before + READERS_FOR_TWO_PIPES);

        // One pipe, however it is asked for, is one reader on every platform.
        before = detail::started_pipe_reader_count();
        process_desc merged = child({"both"});
        merged.mergeStderr = true;
        REQUIRE(process::run(merged).has_value());
        CHECK(detail::started_pipe_reader_count() == before + 1);

        before = detail::started_pipe_reader_count();
        process_desc outputOnly = child({"echo", "quiet"});
        outputOnly.captureStderr = false;
        REQUIRE(process::run(outputOnly).has_value());
        CHECK(detail::started_pipe_reader_count() == before + 1);

        before = detail::started_pipe_reader_count();
        process_desc errorOnly = child({"stderr", "only", "this"});
        errorOnly.captureStdout = false;
        errorOnly.captureStderr = true;
        REQUIRE(process::run(errorOnly).has_value());
        CHECK(detail::started_pipe_reader_count() == before + 1);

        // Nothing captured means no pipe and no thread at all.
        before = detail::started_pipe_reader_count();
        process_desc uncaptured = child({"exit", "0"});
        uncaptured.captureStdout = false;
        uncaptured.captureStderr = false;
        REQUIRE(process::run(uncaptured).has_value());
        CHECK(detail::started_pipe_reader_count() == before);

        // start() creates the readers, wait() only joins them.
        before = detail::started_pipe_reader_count();
        result<process> started = process::start(child({"both"}));
        REQUIRE(started.has_value());
        CHECK(detail::started_pipe_reader_count() == before + READERS_FOR_TWO_PIPES);
        REQUIRE(started->wait().has_value());
        CHECK(detail::started_pipe_reader_count() == before + READERS_FOR_TWO_PIPES);
    }

    TEST_CASE("pipe_reader - join waits for the drain function and the latch completes when it returns") {
        detail::completion_latch done = {};
        detail::pipe_reader reader = {};
        std::binary_semaphore entered(0);
        std::binary_semaphore release(0);
        std::atomic<bool> drained = false;

        CHECK_FALSE(reader.joinable());
        CHECK(done.is_complete());

        reader.start("test-reader", done, [&entered, &release, &drained] {
            entered.acquire();
            release.acquire();
            drained.store(true);
        });

        // Registered with the latch before the thread runs its first instruction.
        CHECK_FALSE(done.is_complete());
        CHECK(reader.joinable());
        entered.release();
        CHECK_FALSE(done.is_complete());
        CHECK_FALSE(drained.load());

        release.release();
        done.wait();
        CHECK(drained.load());
        CHECK(done.is_complete());

        // The latch arriving does not join: the owner still has to.
        CHECK(reader.joinable());
        reader.join();
        CHECK_FALSE(reader.joinable());
        reader.join();
        CHECK_FALSE(reader.joinable());
    }

    TEST_CASE("pipe_reader - a reader that was never started joins as a no-op and leaves the latch alone") {
        detail::completion_latch done = {};
        {
            detail::pipe_reader reader = {};
            CHECK_FALSE(reader.joinable());
            reader.join();
            CHECK_FALSE(reader.joinable());
        }
        CHECK(done.is_complete());
        CHECK(done.wait_until(steady_clock::now()));
    }

    TEST_CASE("pipe_reader - destruction joins a reader whose drain function returns") {
        detail::completion_latch done = {};
        std::atomic<bool> drained = false;
        std::binary_semaphore release(0);
        {
            detail::pipe_reader reader = {};
            reader.start("test-reader", done, [&release, &drained] {
                release.acquire();
                drained.store(true);
            });
            // What an owner does before it lets go of a reader: make the pipe end.
            release.release();
        }
        CHECK(drained.load());
        CHECK(done.is_complete());
    }

    TEST_CASE("pipe_reader - two readers share one latch, which completes when both have returned") {
        detail::completion_latch done = {};
        detail::pipe_reader first = {};
        detail::pipe_reader second = {};
        std::binary_semaphore releaseFirst(0);
        std::binary_semaphore releaseSecond(0);
        std::binary_semaphore firstReturning(0);
        std::thread::id firstThread = {};
        std::thread::id secondThread = {};

        first.start(detail::STANDARD_OUTPUT_READER_NAME, done, [&releaseFirst, &firstReturning, &firstThread] {
            releaseFirst.acquire();
            firstThread = std::this_thread::get_id();
            firstReturning.release();
        });
        second.start(detail::STANDARD_ERROR_READER_NAME, done, [&releaseSecond, &secondThread] {
            releaseSecond.acquire();
            secondThread = std::this_thread::get_id();
        });
        CHECK_FALSE(done.is_complete());

        releaseFirst.release();
        firstReturning.acquire();
        first.join();
        CHECK_FALSE(first.joinable());
        // One reader is still draining, so an owner waiting with a deadline is told so.
        CHECK_FALSE(done.is_complete());
        CHECK_FALSE(done.wait_until(steady_clock::now()));

        releaseSecond.release();
        done.wait();
        second.join();
        CHECK(done.is_complete());
        CHECK(firstThread != secondThread);
        CHECK(firstThread != std::this_thread::get_id());
        CHECK(secondThread != std::this_thread::get_id());
    }

    TEST_CASE("pipe_reader - an empty drain function and a second start assert and start nothing") {
        detail::completion_latch done = {};
        detail::pipe_reader reader = {};

        CONJURE_CHECK_ASSERTS(reader.start("test-reader", done, std::function<void()>()));
        CHECK_FALSE(reader.joinable());
        // Nothing was registered: a latch left waiting for a reader that never runs would hang wait().
        CHECK(done.is_complete());

        std::atomic<int> runs = 0;
        reader.start("test-reader", done, [&runs] {
            runs.fetch_add(1);
        });
        CONJURE_CHECK_ASSERTS(reader.start("test-reader", done, [&runs] {
            runs.fetch_add(100);
        }));
        reader.join();
        CHECK(runs.load() == 1);
        CHECK(done.is_complete());

        // A reader is started once in its life, also after it was joined.
        CONJURE_CHECK_ASSERTS(reader.start("test-reader", done, [&runs] {
            runs.fetch_add(100);
        }));
        CHECK(runs.load() == 1);
        CHECK(done.is_complete());
        CHECK_FALSE(reader.joinable());
    }

    TEST_CASE("process - children run one after another each get their own readers and lose no output") {
        // Every run creates its reader threads and joins them in wait(); a reader left behind or a latch
        // left waiting would show up as a hang or as output of one child in the result of the next.
        for (int index = 0; index < 8; ++index) {
            CAPTURE(index);
            process_desc desc = child({"both"});
            desc.mergeStderr = index % 2 == 1;
            const result<process_result> finished = process::run(desc);

            REQUIRE(finished.has_value());
            CHECK(finished->exitCode == 0);
            if (desc.mergeStderr) {
                CHECK(without_carriage_returns(finished->standardOutput) == "out\nerr\n");
                CHECK(finished->standardError.empty());
            } else {
                CHECK(without_carriage_returns(finished->standardOutput) == "out\n");
                CHECK(without_carriage_returns(finished->standardError) == "err\n");
            }
        }
    }
}
