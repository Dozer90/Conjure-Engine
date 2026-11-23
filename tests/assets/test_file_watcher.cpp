#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/file_watcher.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    struct watcher_fixture {
        core::vfs::memory_file_system fileSystem = {};
        core::jobs::job_system jobs = {};
        polling_file_watcher watcher;

        watcher_fixture(f64 intervalSeconds = 3600.0) :
            watcher(fileSystem, jobs, intervalSeconds) {
            CONJURE_REQUIRE_OK(jobs.start(core::jobs::job_system_desc{}));
            CONJURE_REQUIRE_OK(fileSystem.write_file("assets/a.ta", bytes_of("a")));
            CONJURE_REQUIRE_OK(fileSystem.write_file("assets/sub/deep/b.ta", bytes_of("b")));
            watcher.add_root("assets");
        }

        ~watcher_fixture() {
            jobs.stop();
        }

        std::vector<file_change> poll() {
            std::vector<file_change> changes = {};
            watcher.poll(changes);
            return changes;
        }
    };
}

TEST_SUITE("assets/file_watcher") {

    TEST_CASE("polling_file_watcher - the baseline emits nothing") {
        watcher_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CHECK(fixture.watcher.tracked_file_count() == 2);
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());
        CHECK(fixture.watcher.interval_seconds() == 3600.0);
    }

    TEST_CASE("polling_file_watcher - add, modify and remove are reported after one stable pass") {
        watcher_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());

        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/new.ta", bytes_of("new")));
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/a.ta", bytes_of("changed")));
        CONJURE_REQUIRE_OK(fixture.fileSystem.remove("assets/sub/deep/b.ta"));

        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());   // seen once: not yet stable

        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        const std::vector<file_change> changes = fixture.poll();
        REQUIRE(changes.size() == 3);
        CHECK(changes[0] == file_change{"assets/a.ta", file_change_kind::MODIFIED});
        CHECK(changes[1] == file_change{"assets/new.ta", file_change_kind::ADDED});
        CHECK(changes[2] == file_change{"assets/sub/deep/b.ta", file_change_kind::REMOVED});
        CHECK(fixture.watcher.tracked_file_count() == 2);

        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());
    }

    TEST_CASE("polling_file_watcher - a file written in two steps is reported once, after it is stable") {
        watcher_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());

        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/a.ta", bytes_of("step one")));
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());

        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/a.ta", bytes_of("step two is longer")));
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());   // changed again: debounce restarts

        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        const std::vector<file_change> changes = fixture.poll();
        REQUIRE(changes.size() == 1);
        CHECK(changes[0] == file_change{"assets/a.ta", file_change_kind::MODIFIED});

        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());
    }

    TEST_CASE("polling_file_watcher - a change reverted before it is stable is never reported") {
        watcher_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/temp.ta", bytes_of("t")));
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.fileSystem.remove("assets/temp.ta"));
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());
    }

    TEST_CASE("polling_file_watcher - nested directories and several roots are watched") {
        watcher_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("engine/builtin/x.ta", bytes_of("x")));
        fixture.watcher.add_root("engine");
        fixture.watcher.add_root("engine");   // duplicates are ignored
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CHECK(fixture.watcher.tracked_file_count() == 3);

        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/sub/deep/deeper/c.ta", bytes_of("c")));
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("engine/builtin/x.ta", bytes_of("xx")));
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        const std::vector<file_change> changes = fixture.poll();
        REQUIRE(changes.size() == 2);
        CHECK(changes[0].path == "assets/sub/deep/deeper/c.ta");
        CHECK(changes[0].kind == file_change_kind::ADDED);
        CHECK(changes[1].path == "engine/builtin/x.ta");
        CHECK(changes[1].kind == file_change_kind::MODIFIED);
    }

    TEST_CASE("regression - atomic-write temporaries are never tracked or reported") {
        watcher_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/old.ta.tmp", bytes_of("leftover")));
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CHECK(fixture.watcher.tracked_file_count() == 2);

        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/a.ta.meta.tmp-0123456789abcdef", bytes_of("{}")));
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/b.ta.tmp", bytes_of("half")));
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());
        CONJURE_REQUIRE_OK(fixture.fileSystem.remove("assets/old.ta.tmp"));
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().empty());
        CHECK(fixture.watcher.tracked_file_count() == 2);
    }

    TEST_CASE("polling_file_watcher - add_root while watching waits for the pass in flight") {
        watcher_fixture fixture(0.0);
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("engine/builtin/x.ta", bytes_of("x")));
        CHECK(fixture.poll().empty());   // a pass is scheduled (and, inline, already done)
        fixture.watcher.add_root("engine");
        CHECK(fixture.poll().empty());
        const std::vector<file_change> changes = fixture.poll();
        REQUIRE(changes.size() == 1);
        CHECK(changes[0] == file_change{"engine/builtin/x.ta", file_change_kind::ADDED});
        CHECK(fixture.watcher.tracked_file_count() == 3);
    }

    TEST_CASE("polling_file_watcher - an unreadable root returns an error") {
        watcher_fixture fixture = {};
        fixture.watcher.add_root("missing");
        CONJURE_CHECK_ERROR(fixture.watcher.snapshot(), asset_error_code::IO_ERROR);
        CONJURE_CHECK_ERROR(fixture.watcher.scan_now(), asset_error_code::IO_ERROR);
    }

    TEST_CASE("polling_file_watcher - poll runs the stat pass on the job system when the interval is due") {
        watcher_fixture fixture(0.0);
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/a.ta", bytes_of("polled")));

        // Interval 0 and an inline job system: every poll runs one pass; the second pass confirms.
        CHECK(fixture.poll().empty());
        const std::vector<file_change> changes = fixture.poll();
        REQUIRE(changes.size() == 1);
        CHECK(changes[0].path == "assets/a.ta");
        CHECK(fixture.poll().empty());
    }

    TEST_CASE("polling_file_watcher - a failing pass warns once until the failure changes or clears") {
        watcher_fixture fixture(0.0);
        conjure::testing::log_capture logs = {};
        fixture.watcher.add_root("missing");

        // Interval 0 and an inline job system: every poll runs (and finishes) one failing pass.
        for (u32 pass = 0; pass < 3; ++pass) {
            CAPTURE(pass);
            CHECK(fixture.poll().empty());
        }
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "file watcher pass failed"));

        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("missing/x.ta", bytes_of("x")));
        CHECK(fixture.poll().empty());
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::INFO, "succeeded again"));

        CONJURE_REQUIRE_OK(fixture.fileSystem.remove_all("missing"));
        static_cast<void>(fixture.poll());
        static_cast<void>(fixture.poll());
        CHECK(logs.count(core::log_level::WARN) == 2);
    }

    TEST_CASE("polling_file_watcher - the interval is in seconds, 0.5 by default, clamped to [0, 1e9]") {
        core::vfs::memory_file_system fileSystem = {};
        core::jobs::job_system jobs = {};
        const polling_file_watcher byDefault(fileSystem, jobs);
        CHECK(byDefault.interval_seconds() == DEFAULT_WATCH_INTERVAL_SECONDS);

        struct interval_case {
            f64 given = 0.0;
            f64 expected = 0.0;
        };
        constexpr std::array<interval_case, 8> CASES = {{
            {0.25, 0.25},
            {0.0, 0.0},
            {1.0e9, 1.0e9},
            {-1.0, 0.0},
            {-std::numeric_limits<f64>::infinity(), 0.0},
            {std::numeric_limits<f64>::quiet_NaN(), 0.0},
            {std::numeric_limits<f64>::infinity(), 1.0e9},
            {1.0e15, 1.0e9},
        }};
        for (const interval_case &entry : CASES) {
            CAPTURE(entry.given);
            const polling_file_watcher watcher(fileSystem, jobs, entry.given);
            CHECK(watcher.interval_seconds() == entry.expected);
        }
    }

    TEST_CASE("polling_file_watcher - poll starts no pass before the interval has elapsed") {
        watcher_fixture fixture(3600.0);
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/a.ta", bytes_of("polled")));

        // The snapshot counts as a pass: an hour has not passed, so no poll stats the files.
        for (u32 attempt = 0; attempt < 3; ++attempt) {
            CAPTURE(attempt);
            CHECK(fixture.poll().empty());
        }
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CONJURE_REQUIRE_OK(fixture.watcher.scan_now());
        CHECK(fixture.poll().size() == 1);
    }

    TEST_CASE("polling_file_watcher - a NaN or negative interval runs a pass on every poll, like 0") {
        f64 intervalSeconds = 0.0;
        SUBCASE("NaN") {
            intervalSeconds = std::numeric_limits<f64>::quiet_NaN();
        }
        SUBCASE("negative") {
            intervalSeconds = -1.0;
        }
        SUBCASE("negative infinity") {
            intervalSeconds = -std::numeric_limits<f64>::infinity();
        }
        CAPTURE(intervalSeconds);

        watcher_fixture fixture(intervalSeconds);
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        CONJURE_REQUIRE_OK(fixture.fileSystem.write_file("assets/a.ta", bytes_of("polled")));

        // An inline job system: every poll runs one pass; the second pass confirms.
        CHECK(fixture.poll().empty());
        const std::vector<file_change> changes = fixture.poll();
        REQUIRE(changes.size() == 1);
        CHECK(changes[0] == file_change{"assets/a.ta", file_change_kind::MODIFIED});
    }

    TEST_CASE("polling_file_watcher - the first poll without a snapshot only records the baseline") {
        watcher_fixture fixture(0.0);
        CHECK(fixture.poll().empty());
        CHECK(fixture.watcher.tracked_file_count() == 2);
        CONJURE_REQUIRE_OK(fixture.fileSystem.remove("assets/a.ta"));
        CHECK(fixture.poll().empty());
        const std::vector<file_change> changes = fixture.poll();
        REQUIRE(changes.size() == 1);
        CHECK(changes[0].kind == file_change_kind::REMOVED);
    }
}
