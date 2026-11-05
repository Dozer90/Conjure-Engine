#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/process.h"

#include <atomic>
#include <barrier>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    using steady_clock = std::chrono::steady_clock;

    // Built next to the test executable by tests/core/system/CMakeLists.txt.
    const std::filesystem::path CHILD_PATH = CONJURE_PROCESS_CHILD_PATH;

    process_desc child(std::vector<std::string> arguments) {
        process_desc desc = {};
        desc.executable = CHILD_PATH.generic_string();
        desc.arguments = std::move(arguments);
        return desc;
    }

    // The child prints in text mode, which the Windows runtime turns into "\r\n" line endings.
    std::string without_carriage_returns(std::string text) {
        std::erase(text, '\r');
        return text;
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
        result<process> started = process::start(child({"sleep", "150"}));
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

        process_desc exitDesc = child({"exit", "9"});
        const result<process_result> finished = running.wait();
        stop.store(true);
        poller.join();

        REQUIRE(finished.has_value());
        CHECK(finished->exitCode == 0);
        CHECK(polls.load() > 0);   // the poller saw the child alive at least once
        CHECK_FALSE(running.is_running());

        // The same with a non-zero code, so a lost status (reported as 0) would be caught.
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
        // until the sleeper dies. Both threads release the barrier and start at the same moment.
        std::barrier<> ready(2);
        std::optional<result<process>> slow = std::nullopt;
        std::optional<result<process>> fast = std::nullopt;

        std::thread slowStarter([&slow, &ready] {
            ready.arrive_and_wait();
            slow.emplace(process::start(child({"sleep", "2000"})));
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

        const steady_clock::time_point begin = steady_clock::now();
        const result<process_result> fastResult = fast->value().wait();
        const f64 fastSeconds = seconds_since(begin);

        REQUIRE(fastResult.has_value());
        CHECK(without_carriage_returns(fastResult->standardOutput) == "fast\n");
        CHECK(fastSeconds < 1.5);
        CHECK(slow->value().is_running());

        slow->value().kill();
        const result<process_result> slowResult = slow->value().wait();
        REQUIRE(slowResult.has_value());
        CHECK(slowResult->exitCode == process::KILLED_EXIT_CODE);
    }

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
        const scoped_current_path inChildDirectory(CHILD_PATH.parent_path());

        process_desc desc = {};
        desc.executable = "./" + CHILD_PATH.filename().generic_string();
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
}
