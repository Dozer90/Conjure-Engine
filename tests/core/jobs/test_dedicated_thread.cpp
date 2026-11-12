#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "jobs_test_support.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/platform_detect.h"
#include "core/jobs/thread_name.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::jobs;

TEST_SUITE("core/dedicated_thread") {

    TEST_CASE("dedicated_thread - the body runs on a separate thread carrying the given name") {
        std::atomic<bool> ran = false;
        std::thread::id bodyThread = {};
        std::string osName = {};

        {
            dedicated_thread thread("io-reader", [&](std::stop_token) {
                bodyThread = std::this_thread::get_id();
                osName = jobs::detail::current_thread_name();
                ran.store(true);
            });
            CHECK(thread.name() == "io-reader");
            CHECK(thread.joinable());
            CHECK(thread.get_id() != std::this_thread::get_id());
            const std::thread::id idBeforeJoin = thread.get_id();
            thread.join();
            CHECK(idBeforeJoin == bodyThread);
            CHECK_FALSE(thread.joinable());
        }

        CHECK(ran.load());
        CHECK(bodyThread != std::this_thread::get_id());
        // Linux keeps only the first 15 bytes of a thread name; the others keep it whole.
        CHECK(osName == std::string("io-reader").substr(0, jobs::detail::MAX_POSIX_THREAD_NAME_LENGTH));
    }

    TEST_CASE("dedicated_thread - long names are truncated only where the platform requires it") {
        const std::string longName = "a-rather-long-dedicated-thread-name";
        std::string osName = {};
        {
            dedicated_thread thread(longName, [&osName](std::stop_token) {
                osName = jobs::detail::current_thread_name();
            });
        }
        CHECK(longName.starts_with(osName));
        CHECK(osName.size() >= jobs::detail::MAX_POSIX_THREAD_NAME_LENGTH);
    }

    TEST_CASE("dedicated_thread - name truncation never splits a UTF-8 code point") {
        // The two-byte character occupies bytes 14 and 15, so a plain 15-byte cut keeps only its lead byte.
        const std::string name = "worker-thread-\xC3\xA4-streaming";
#if !CONJURE_PLATFORM_WINDOWS
        CHECK(std::string(jobs::detail::prepare_thread_name(name).characters.data()) == "worker-thread-");
#endif

        std::string osName = {};
        {
            dedicated_thread thread(name, [&osName](std::stop_token) {
                osName = jobs::detail::current_thread_name();
            });
        }
        REQUIRE_FALSE(osName.empty());
        CHECK(name.starts_with(osName));
        CHECK(static_cast<unsigned char>(osName.back()) < 0xC0);
    }

    TEST_CASE("dedicated_thread - a name with invalid UTF-8 keeps its valid characters") {
        // Windows replaces the stray byte with U+FFFD; Linux and macOS keep it as it is.
        std::string osName = {};
        {
            dedicated_thread thread("\xFFworker", [&osName](std::stop_token) {
                osName = jobs::detail::current_thread_name();
            });
        }
        CHECK_FALSE(osName.empty());
        CHECK(osName.ends_with("worker"));
    }

    TEST_CASE("dedicated_thread - destruction requests a stop and joins") {
        std::atomic<bool> started = false;
        std::atomic<bool> sawStop = false;
        std::atomic<int> iterations = 0;

        {
            dedicated_thread thread("looper", [&](std::stop_token token) {
                started.store(true);
                while (!token.stop_requested()) {
                    iterations.fetch_add(1);
                    std::this_thread::yield();
                }
                sawStop.store(true);
            });
            REQUIRE(testing::spin_until([&started] { return started.load(); }));
            CHECK_FALSE(thread.stop_requested());
        }

        CHECK(sawStop.load());
        CHECK(iterations.load() > 0);
    }

    TEST_CASE("dedicated_thread - request_stop signals the token without joining") {
        std::atomic<bool> exited = false;
        dedicated_thread thread("stoppable", [&exited](std::stop_token token) {
            while (!token.stop_requested()) {
                std::this_thread::yield();
            }
            exited.store(true);
        });

        thread.request_stop();
        CHECK(thread.stop_requested());
        CHECK(thread.joinable());

        REQUIRE(testing::spin_until([&exited] { return exited.load(); }));
        thread.join();
        CHECK_FALSE(thread.joinable());
        CHECK(thread.get_id() == std::thread::id());

        // Joining again is a no-op.
        thread.join();
        CHECK_FALSE(thread.joinable());
    }

    TEST_CASE("dedicated_thread - join waits for a body that finishes on its own") {
        std::atomic<bool> finished = false;
        dedicated_thread thread("short-lived", [&finished](std::stop_token) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            finished.store(true);
        });

        thread.join();

        CHECK(finished.load());
        CHECK_FALSE(thread.joinable());
        CHECK_FALSE(thread.stop_requested());
    }

    TEST_CASE("dedicated_thread - an empty body asserts and leaves the thread non-joinable") {
        testing::expect_assert asserts = {};

        dedicated_thread thread("empty", std::function<void(std::stop_token)>());

        CHECK(asserts.count() == 1);
        CHECK_FALSE(thread.joinable());
        CHECK(thread.name() == "empty");
        thread.request_stop();
        thread.join();
        CHECK_FALSE(thread.joinable());
    }

    TEST_CASE("dedicated_thread - a dedicated thread is neither a worker nor the main thread") {
        set_main_thread();
        std::atomic<u32> threadIndex = 99;
        std::atomic<job_system *> current = reinterpret_cast<job_system *>(1);
        std::atomic<bool> isMain = true;

        {
            dedicated_thread thread("probe", [&](std::stop_token) {
                threadIndex.store(job_system::thread_index());
                current.store(job_system::current());
                isMain.store(is_main_thread());
            });
        }

        CHECK(threadIndex.load() == 0);
        CHECK(current.load() == nullptr);
        CHECK_FALSE(isMain.load());
    }

    TEST_CASE("dedicated_thread - move transfers ownership of the running thread") {
        std::atomic<bool> released = false;
        dedicated_thread original("movable", [&released](std::stop_token token) {
            while (!token.stop_requested()) {
                std::this_thread::yield();
            }
            released.store(true);
        });
        const std::thread::id id = original.get_id();

        dedicated_thread moved = std::move(original);
        CHECK(moved.joinable());
        CHECK(moved.get_id() == id);
        CHECK(moved.name() == "movable");

        dedicated_thread assigned("placeholder", [](std::stop_token) {});
        assigned = std::move(moved);
        CHECK(assigned.joinable());
        CHECK(assigned.get_id() == id);
        CHECK(assigned.name() == "movable");

        assigned.request_stop();
        assigned.join();
        CHECK(released.load());
    }

    TEST_CASE("dedicated_thread - a dedicated thread can drive a job system") {
        testing::scoped_job_system fixture(8);
        job_system &system = fixture.system();
        std::atomic<int> ran = 0;
        std::atomic<bool> handleComplete = false;

        {
            dedicated_thread driver("driver", [&](std::stop_token) {
                const job_handle handle = system.schedule([&ran] {
                    ran.fetch_add(1);
                });
                system.wait(handle);
                handleComplete.store(handle.is_complete());
                system.parallel_for(100, 10, [&ran](usize begin, usize end) {
                    ran.fetch_add(static_cast<int>(end - begin));
                });
            });
        }

        CHECK(ran.load() == 101);
        CHECK(handleComplete.load());
    }
}
