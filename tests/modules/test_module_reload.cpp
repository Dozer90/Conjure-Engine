#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "fake_consumer.h"
#include "static_fixtures.h"

#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_events.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/string_id.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    struct reload_fixture {
        std::vector<std::string> log = {};
        fake_consumer ecs{"ecs", &log};
        fake_consumer scripting{"scripting", &log};
        static_module_source alpha{"alpha", manifest_json("alpha", "1.0.0"), REGISTERING_ENTRY};
        static_module_source beta{"beta", manifest_json("beta", "1.0.0", {{"alpha"}}), REGISTERING_ENTRY};
        static_host host = {};
        module_id alphaId = {};
        module_id betaId = {};

        reload_fixture() {
            scripting.participatesInManagedReload = true;
            host.manager.add_consumer(ecs);
            host.manager.add_consumer(scripting);
            ecs.bind_registry(host.manager.registry());
            scripting.bind_registry(host.manager.registry());
            alphaId = host.manager.add_static_module(alpha.desc()).value();
            betaId = host.manager.add_static_module(beta.desc()).value();
            REQUIRE(host.manager.load_all().has_value());
            log.clear();
        }
    };
}

TEST_SUITE("modules/module_reload") {

    TEST_CASE("module_manager - a FULL reload detaches dependents first and attaches dependencies first") {
        reload_fixture fixture = {};
        std::vector<std::string> events = {};
        const event_subscription onUnloading =
          fixture.host.events.subscribe<module_unloading_event>([&](const module_unloading_event &event) {
              events.push_back("unloading:" + fixture.host.manager.registry().manifest(event.id).name
                + (event.reloading ? ":reloading" : ""));
          });
        const event_subscription onLoaded =
          fixture.host.events.subscribe<module_loaded_event>([&](const module_loaded_event &event) {
              events.push_back("loaded:" + fixture.host.manager.registry().manifest(event.id).name);
          });
        const event_subscription onUnloaded =
          fixture.host.events.subscribe<module_unloaded_event>([&](const module_unloaded_event &) {
              events.push_back("unloaded");
          });

        CONJURE_REQUIRE_OK(fixture.host.manager.reload(fixture.alphaId, reload_scope::FULL));

        CHECK(fixture.log == std::vector<std::string>{
            "can_detach(remove):ecs:beta", "can_detach(remove):scripting:beta",
            "can_detach(remove):ecs:alpha", "can_detach(remove):scripting:alpha",
            "detach(reload):scripting:beta", "detach(reload):ecs:beta",
            "detach(reload):scripting:alpha", "detach(reload):ecs:alpha",
            "validate:ecs:alpha", "validate:scripting:alpha", "attach:ecs:alpha", "attach:scripting:alpha",
            "restore:ecs:alpha", "restore:scripting:alpha",
            "validate:ecs:beta", "validate:scripting:beta", "attach:ecs:beta", "attach:scripting:beta",
            "restore:ecs:beta", "restore:scripting:beta",
        });
        CHECK(events == std::vector<std::string>{
            "unloading:beta:reloading", "unloading:alpha:reloading", "loaded:alpha", "loaded:beta",
        });

        const module_registry &registry = fixture.host.manager.registry();
        CHECK(registry.is_valid(fixture.alphaId));
        CHECK(registry.is_valid(fixture.betaId));
        CHECK(registry.state(fixture.alphaId) == module_state::LOADED);
        CHECK(registry.state(fixture.betaId) == module_state::LOADED);
        CHECK(registry.load_count(fixture.alphaId) == 2);
        CHECK(registry.load_count(fixture.betaId) == 2);
        REQUIRE(registry.load_order().size() == 2);
        CHECK(registry.load_order()[0] == fixture.alphaId);
        CHECK(registry.load_order()[1] == fixture.betaId);
        REQUIRE(registry.dependents(fixture.alphaId).size() == 1);
        CHECK(registry.dependents(fixture.alphaId)[0] == fixture.betaId);
        CHECK(counters().entryRuns == 4);
        CHECK(counters().activated == 4);
        CHECK(counters().deactivating == 2);
    }

    TEST_CASE("module_manager - reloading a leaf touches only that module") {
        reload_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.reload(fixture.betaId, reload_scope::FULL));
        CHECK(fixture.log == std::vector<std::string>{
            "can_detach(remove):ecs:beta", "can_detach(remove):scripting:beta",
            "detach(reload):scripting:beta", "detach(reload):ecs:beta",
            "validate:ecs:beta", "validate:scripting:beta", "attach:ecs:beta", "attach:scripting:beta",
            "restore:ecs:beta", "restore:scripting:beta",
        });
        CHECK(fixture.host.manager.registry().load_count(fixture.alphaId) == 1);
        CHECK(fixture.host.manager.registry().load_count(fixture.betaId) == 2);
    }

    TEST_CASE("module_manager - consumer blobs round trip through the reload_context") {
        reload_fixture fixture = {};
        fixture.ecs.snapshotPayload = "ten thousand spinners";
        fixture.scripting.snapshotPayload = "";

        CONJURE_REQUIRE_OK(fixture.host.manager.reload(fixture.alphaId, reload_scope::FULL));
        CHECK(fixture.ecs.restoreCount == 2);
        CHECK(fixture.ecs.restoredPayload == "ten thousand spinners");
        CHECK(fixture.scripting.restoreCount == 2);
        CHECK(fixture.scripting.restoredPayload.empty());
    }

    TEST_CASE("module_manager - MANAGED_ONLY touches participating consumers only and keeps the native side") {
        reload_fixture fixture = {};
        fixture.scripting.snapshotPayload = "script fields";
        std::vector<std::string> events = {};
        const event_subscription onUnloading =
          fixture.host.events.subscribe<module_unloading_event>([&](const module_unloading_event &event) {
              events.push_back(event.reloading ? "unloading:reloading" : "unloading");
          });
        const event_subscription onLoaded =
          fixture.host.events.subscribe<module_loaded_event>([&](const module_loaded_event &) {
              events.push_back("loaded");
          });

        CONJURE_REQUIRE_OK(fixture.host.manager.reload(fixture.alphaId, reload_scope::MANAGED_ONLY));
        CHECK(fixture.log == std::vector<std::string>{
            "can_detach(remove):scripting:beta", "can_detach(remove):scripting:alpha",
            "detach(reload):scripting:beta", "detach(reload):scripting:alpha",
            "attach:scripting:alpha", "restore:scripting:alpha",
            "attach:scripting:beta", "restore:scripting:beta",
        });
        CHECK(events == std::vector<std::string>{"unloading:reloading", "unloading:reloading", "loaded", "loaded"});
        CHECK(counters().entryRuns == 2);

        // The lifecycle callbacks bracket a managed reload like a full one (07_modules.md 3.5); only
        // the entry is not run again.
        CHECK(counters().activated == 4);
        CHECK(counters().deactivating == 2);
        CHECK(counters().lastDeactivatingId == fixture.alphaId);
        CHECK(counters().lastActivatedId == fixture.betaId);
        CHECK(fixture.scripting.restoredPayload == "script fields");
        CHECK(fixture.ecs.restoreCount == 0);
        CHECK(fixture.ecs.is_attached(fixture.alphaId));
        CHECK(fixture.host.manager.registry().load_count(fixture.alphaId) == 1);
        CHECK(fixture.host.manager.registry().state(fixture.alphaId) == module_state::LOADED);
    }

    TEST_CASE("module_manager - a failed reload keeps the snapshot and restores it on the next successful load") {
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fake_consumer ecs("ecs");
        static_module_source alpha("alpha", manifest_json("alpha"), SECOND_RUN_FAILING_ENTRY);
        static_module_source beta("beta", manifest_json("beta", "1.0.0", {{"alpha"}}), REGISTERING_ENTRY);
        static_host host = {};
        host.manager.add_consumer(ecs);
        ecs.bind_registry(host.manager.registry());
        const module_id alphaId = host.manager.add_static_module(alpha.desc()).value();
        const module_id betaId = host.manager.add_static_module(beta.desc()).value();
        CONJURE_REQUIRE_OK(host.manager.load_all());
        ecs.snapshotPayload = "keep me";

        const result<void> failed = host.manager.reload(alphaId, reload_scope::FULL);
        CONJURE_CHECK_ERROR(failed, module_error::ENTRY_FAILED);
        CHECK(host.manager.registry().is_valid(alphaId));
        CHECK(host.manager.registry().state(alphaId) == module_state::FAILED);
        CHECK(host.manager.registry().state(betaId) == module_state::UNLOADED);
        CHECK(host.manager.registry().load_order().empty());
        CHECK(ecs.restoreCount == 0);
        CHECK(ecs.attached().empty());
        const std::string &lastMessage = host.manager.registry().last_error(alphaId).message();
        CHECK(lastMessage.find("rebuilt module is broken") != std::string::npos);

        // The fixed build: alpha succeeds again, beta follows, both snapshots restore.
        ecs.snapshotPayload = "overwritten if detach ran again";
        CONJURE_REQUIRE_OK(host.manager.load_all());
        CHECK(ecs.restoreCount == 2);
        CHECK(ecs.restoredPayload == "keep me");
        CHECK(host.manager.registry().state(alphaId) == module_state::LOADED);
        CHECK(host.manager.registry().state(betaId) == module_state::LOADED);
        CHECK(host.manager.registry().load_count(alphaId) == 2);
    }

    TEST_CASE("module_manager - a fault inside the reload cascade stops it and leaves the rest as they were") {
        reload_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.ecs.liveUsesAfterDetach = 1;    // beta detaches first and faults

        conjure::testing::expect_assert guard = {};
        const result<void> faulted = fixture.host.manager.reload(fixture.alphaId, reload_scope::FULL);
        CHECK(guard.count() == 1);
        CONJURE_CHECK_ERROR(faulted, module_error::FAULTED);
        const module_registry &registry = fixture.host.manager.registry();
        CHECK(registry.state(fixture.betaId) == module_state::FAULTED);
        CHECK(registry.state(fixture.alphaId) == module_state::LOADED);
        REQUIRE(registry.load_order().size() == 1);
        CHECK(registry.load_order()[0] == fixture.alphaId);
        CHECK(registry.dependents(fixture.alphaId).empty());
        CHECK(fixture.ecs.is_attached(fixture.alphaId));
        CHECK_FALSE(fixture.ecs.is_attached(fixture.betaId));
        CHECK(registry.load_count(fixture.alphaId) == 1);
        CHECK(counters().deactivating == 1);
        CHECK(counters().entryRuns == 2);
        CHECK(fixture.log == std::vector<std::string>{
            "can_detach(remove):ecs:beta", "can_detach(remove):scripting:beta",
            "can_detach(remove):ecs:alpha", "can_detach(remove):scripting:alpha",
            "detach(reload):scripting:beta", "detach(reload):ecs:beta",
        });

        // Alpha has no dependents left and unloads normally once the consumer behaves again.
        fixture.ecs.liveUsesAfterDetach = 0;
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.alphaId));
        CONJURE_CHECK_ERROR(fixture.host.manager.load(fixture.betaId), module_error::FAULTED);
    }

    TEST_CASE("module_manager - a consumer veto anywhere in the cascade reloads nothing") {
        reload_fixture fixture = {};
        fixture.ecs.canDetachResult = make_module_error(module_error::UNLOAD_BLOCKED, "play mode");
        const result<void> blocked = fixture.host.manager.reload(fixture.alphaId, reload_scope::FULL);
        CONJURE_CHECK_ERROR(blocked, module_error::UNLOAD_BLOCKED);
        CHECK(fixture.log == std::vector<std::string>{"can_detach(remove):ecs:beta"});
        CHECK(fixture.host.manager.registry().load_count(fixture.alphaId) == 1);
        CHECK(fixture.host.manager.registry().state(fixture.betaId) == module_state::LOADED);
        CHECK(counters().deactivating == 0);
    }

    TEST_CASE("module_manager - request_reload does nothing until apply_pending runs") {
        reload_fixture fixture = {};
        fixture.host.manager.request_reload(fixture.betaId, reload_scope::FULL);
        CHECK(fixture.host.manager.pending_count() == 1);
        CHECK(fixture.log.empty());
        CHECK(fixture.host.manager.registry().load_count(fixture.betaId) == 1);
        CHECK(counters().entryRuns == 2);

        fixture.host.manager.apply_pending();
        CHECK(fixture.host.manager.pending_count() == 0);
        CHECK(fixture.host.manager.registry().load_count(fixture.betaId) == 2);
        CHECK(counters().entryRuns == 3);
        CHECK_FALSE(fixture.log.empty());
    }

    TEST_CASE("module_manager - reload of an unloaded module is NOT_LOADED") {
        reload_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.betaId));
        const module_id freshBeta = fixture.host.manager.registry().find(string_id("beta"));
        CONJURE_CHECK_ERROR(fixture.host.manager.reload(freshBeta, reload_scope::FULL), module_error::NOT_LOADED);
        CONJURE_CHECK_ERROR(fixture.host.manager.reload(fixture.betaId, reload_scope::FULL), module_error::STALE_ID);
    }
}
