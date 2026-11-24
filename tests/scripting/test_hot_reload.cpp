#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "scripting_test_support.h"

#include "conjure/core/delegate.h"
#include "conjure/scripting/hot_reload.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    /// <summary>
    /// Moves a file's write time 'seconds' later, as a rebuild would.
    /// </summary>
    void bump_write_time(const std::filesystem::path &file, i64 seconds) {
        std::error_code probe = {};
        const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(file, probe);
        REQUIRE_FALSE(probe);
        std::filesystem::last_write_time(file, stamp + std::chrono::seconds(seconds), probe);
        REQUIRE_FALSE(probe);
    }
}

TEST_SUITE("scripting/hot_reload") {

    TEST_CASE("hot_reload_controller - requests inside the debounce window coalesce into one reload") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.A", {})});
        hot_reload_controller reload(fixture.runtime, {});
        CHECK(reload.state() == reload_state::IDLE);
        CHECK(reload.debounce_seconds() == doctest::Approx(0.2));
        CHECK(reload.request_count() == 0);

        reload.request();
        reload.request();
        reload.request();
        CHECK(reload.request_count() == 3);
        CHECK(reload.state() == reload_state::PENDING);
        CHECK(reload.reload_count() == 0);

        reload.tick(1.0);       // stamps the window
        CHECK(reload.state() == reload_state::PENDING);
        reload.tick(1.1);
        CHECK(reload.state() == reload_state::PENDING);
        CHECK(reload.reload_count() == 0);
        reload.tick(1.25);
        CHECK(reload.state() == reload_state::IDLE);
        CHECK(reload.reload_count() == 1);
        CHECK(fixture.runtime.stats().reloadCount == 1);
        reload.tick(2.0);
        CHECK(reload.reload_count() == 1);
    }

    TEST_CASE("hot_reload_controller - the reload runs only inside tick") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.A", {})});
        hot_reload_controller reload(fixture.runtime, {});
        reload.set_debounce_seconds(0.0);
        reload.request();
        CHECK(fixture.runtime.stats().reloadCount == 0);
        CHECK(fixture.null_host().recorder().count("load_game_assembly") == 1);
        reload.tick(0.0);
        CHECK(fixture.runtime.stats().reloadCount == 1);
        CHECK(fixture.null_host().recorder().count("load_game_assembly") == 2);
    }

    TEST_CASE("hot_reload_controller - state goes IDLE, PENDING, RELOADING, IDLE") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.A", {})});
        hot_reload_controller reload(fixture.runtime, {});
        reload.set_debounce_seconds(0.0);
        reload_state observed = reload_state::IDLE;
        hot_reload_controller *observedController = &reload;
        fixture.runtime.on_reloaded = core::delegate<void()>::bind([&observed, observedController]() {
            observed = observedController->state();
        });
        CHECK(reload.state() == reload_state::IDLE);
        reload.request();
        CHECK(reload.state() == reload_state::PENDING);
        reload.tick(5.0);
        CHECK(observed == reload_state::RELOADING);
        CHECK(reload.state() == reload_state::IDLE);
        CHECK(to_string(reload_state::RELOADING) == "RELOADING");
        CHECK(to_string(reload_state::PENDING) == "PENDING");
    }

    TEST_CASE("hot_reload_controller - an immediate request bypasses the debounce window") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.A", {})});
        hot_reload_controller reload(fixture.runtime, {});
        reload.request(true);
        reload.tick(0.0);
        CHECK(reload.reload_count() == 1);
        CHECK(reload.state() == reload_state::IDLE);

        // A plain request after the immediate one waits again.
        reload.request();
        reload.tick(1.0);
        CHECK(reload.reload_count() == 1);
        reload.request(true);
        reload.tick(1.05);
        CHECK(reload.reload_count() == 2);
    }

    TEST_CASE("hot_reload_controller - a request made during the reload is honoured by the next tick") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.A", {})});
        hot_reload_controller reload(fixture.runtime, {});
        reload.set_debounce_seconds(0.0);
        hot_reload_controller *controller = &reload;
        fixture.runtime.on_reloaded = core::delegate<void()>::bind([controller]() {
            if (controller->reload_count() == 0) {
                controller->request(true);
            }
        });
        reload.request();
        reload.tick(0.0);
        CHECK(reload.reload_count() == 1);
        CHECK(reload.state() == reload_state::PENDING);
        reload.tick(0.0);
        CHECK(reload.reload_count() == 2);
        CHECK(reload.state() == reload_state::IDLE);
    }

    TEST_CASE("hot_reload_controller - a failed reload is recorded and warned, the state returns to IDLE") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.A", {})});
        hot_reload_controller reload(fixture.runtime, {});
        reload.set_debounce_seconds(0.0);
        CHECK(reload.last_result().has_value());
        fixture.null_host().set_load_result(status_of(scripting_error::ASSEMBLY_LOAD_FAILED));
        reload.request();
        reload.tick(0.0);
        CHECK(reload.state() == reload_state::IDLE);
        CHECK(reload.reload_count() == 1);
        CONJURE_CHECK_ERROR(reload.last_result(), scripting_error::ASSEMBLY_LOAD_FAILED);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(fixture.runtime.classes().find("Game.A") != nullptr);
    }

    TEST_CASE("hot_reload_controller - watching the assembly file requests a reload when it changes") {
        scripting_fixture fixture = {};
        temp_directory directory = {};
        const std::filesystem::path assembly = directory.file("Game.dll");
        hot_reload_controller reload(fixture.runtime, assembly);
        CHECK(reload.assembly_path() == assembly);
        CHECK(reload.watch_interval_seconds() == 0.0);
        reload.set_watch_interval_seconds(1.0);
        reload.set_debounce_seconds(0.0);
        CHECK(reload.watch_interval_seconds() == 1.0);

        reload.tick(0.0);                                      // file does not exist yet: nothing
        CHECK(reload.state() == reload_state::IDLE);
        CONJURE_REQUIRE_OK(directory.write_text("Game.dll", "v1"));
        reload.tick(1.0);                                      // baseline stamp
        CHECK(reload.state() == reload_state::IDLE);
        std::error_code probe = {};
        const std::filesystem::file_time_type later =
          std::filesystem::last_write_time(assembly, probe) + std::chrono::seconds(5);
        std::filesystem::last_write_time(assembly, later, probe);
        REQUIRE_FALSE(probe);
        reload.tick(1.5);                                      // inside the interval: not polled yet
        CHECK(reload.state() == reload_state::IDLE);
        reload.tick(2.0);                                      // polled, changed: requested and (no debounce) reloaded
        CHECK(reload.state() == reload_state::IDLE);
        CHECK(reload.request_count() == 1);
        CHECK(reload.reload_count() == 1);
        reload.tick(3.0);                                      // unchanged since: nothing
        CHECK(reload.request_count() == 1);

        reload.set_assembly_path(directory.file("Other.dll"));
        CHECK(reload.assembly_path() == directory.file("Other.dll"));
        reload.set_watch_interval_seconds(-1.0);
        CHECK(reload.watch_interval_seconds() == 0.0);
        reload.set_debounce_seconds(-3.0);
        CHECK(reload.debounce_seconds() == 0.0);
    }

    TEST_CASE("hot_reload_controller - a watched reload that fails to load is retried up to three times") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        temp_directory directory = {};
        CONJURE_REQUIRE_OK(directory.write_text("Game.dll", "v1"));
        const std::filesystem::path assembly = directory.file("Game.dll");
        hot_reload_controller reload(fixture.runtime, assembly);
        reload.set_watch_interval_seconds(1.0);
        reload.set_debounce_seconds(0.0);
        fixture.null_host().set_load_result(status_of(scripting_error::ASSEMBLY_LOAD_FAILED));
        reload.tick(1.0);                                      // baseline stamp
        bump_write_time(assembly, 5);

        // The change is seen at 2.0; each failed load is tried again on the next tick (no debounce).
        const std::array<f64, 5> ticks = {2.0, 2.1, 2.2, 2.3, 2.4};
        const std::array<u32, 5> expectedReloads = {1, 2, 3, 4, 4};
        for (usize index = 0; index < ticks.size(); ++index) {
            CAPTURE(index);
            reload.tick(ticks[index]);
            CHECK(reload.reload_count() == expectedReloads[index]);
        }
        CHECK(reload.state() == reload_state::IDLE);
        CHECK(reload.request_count() == 1);
        CONJURE_CHECK_ERROR(reload.last_result(), scripting_error::ASSEMBLY_LOAD_FAILED);
        CHECK(logs.count(core::log_level::WARN) == 4);
        CHECK(logs.contains(core::log_level::WARN, "retrying (0 left)"));

        // A reload asked for with request() that fails is not retried.
        reload.request();
        reload.tick(3.5);
        CHECK(reload.reload_count() == 5);
        CHECK(reload.state() == reload_state::IDLE);
        reload.tick(3.6);
        CHECK(reload.reload_count() == 5);
        CHECK(logs.count(core::log_level::WARN) == 5);
    }

    TEST_CASE("hot_reload_controller - a change of the .pdb alone requests a reload") {
        scripting_fixture fixture = {};
        temp_directory directory = {};
        CONJURE_REQUIRE_OK(directory.write_text("Game.dll", "v1"));
        CONJURE_REQUIRE_OK(directory.write_text("Game.pdb", "symbols v1"));
        hot_reload_controller reload(fixture.runtime, directory.file("Game.dll"));
        reload.set_watch_interval_seconds(1.0);
        reload.set_debounce_seconds(0.0);
        reload.tick(1.0);                                      // baseline: the newer of the two stamps
        CHECK(reload.request_count() == 0);

        bump_write_time(directory.file("Game.pdb"), 5);
        reload.tick(2.0);
        CHECK(reload.request_count() == 1);
        CHECK(reload.reload_count() == 1);
        CONJURE_CHECK_OK(reload.last_result());
        reload.tick(3.0);                                      // unchanged since
        CHECK(reload.request_count() == 1);
    }
}
