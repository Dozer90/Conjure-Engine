#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "fake_consumer.h"
#include "fixture_descriptors.h"
#include "static_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_events.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_service.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/event_bus.h"
#include "conjure/core/string_id.h"

#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    class world_service {
        public:
            CONJURE_MODULE_SERVICE(conjure::test::world_service);
    };

    /// <summary>
    /// Three static modules: gamma depends on beta depends on alpha, every one registering.
    /// </summary>
    struct chain_fixture {
        std::vector<std::string> log = {};
        fake_consumer consumer{"ecs", &log};
        static_module_source alpha{"alpha", manifest_json("alpha", "1.0.0"), REGISTERING_ENTRY};
        static_module_source beta{"beta", manifest_json("beta", "1.0.0", {{"alpha"}}), REGISTERING_ENTRY};
        static_module_source gamma{"gamma", manifest_json("gamma", "1.0.0", {{"beta"}}), REGISTERING_ENTRY};
        static_host host = {};
        module_id alphaId = {};
        module_id betaId = {};
        module_id gammaId = {};

        chain_fixture() {
            host.manager.add_consumer(consumer);
            consumer.bind_registry(host.manager.registry());
            alphaId = host.manager.add_static_module(alpha.desc()).value();
            betaId = host.manager.add_static_module(beta.desc()).value();
            gammaId = host.manager.add_static_module(gamma.desc()).value();
        }

        [[nodiscard]] module_state state(module_id id) const noexcept {
            return host.manager.registry().state(id);
        }
    };
}

TEST_SUITE("modules/module_manager_static") {

    TEST_CASE("module_manager - load runs the entry, validates, attaches and records the registrations") {
        chain_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));

        CHECK(fixture.state(fixture.alphaId) == module_state::LOADED);
        CHECK(fixture.log == std::vector<std::string>{"validate:ecs:alpha", "attach:ecs:alpha"});
        CHECK(counters().entryRuns == 1);
        CHECK(counters().activated == 1);
        CHECK(counters().lastActivatedId == fixture.alphaId);

        const module_registration_set &set = fixture.host.manager.registry().registrations(fixture.alphaId);
        const small_vector<const fixtures::component_desc *, 8> components =
          extensions_of<fixtures::component_desc>(set);
        REQUIRE(components.size() == 1);
        CHECK(std::string_view(components[0]->name) == "alpha");
        CHECK(extensions_of<fixtures::system_desc>(set).size() == 1);
        CHECK(set.lifecycle.onActivated != nullptr);
        CHECK(fixture.host.manager.registry().load_count(fixture.alphaId) == 1);
        CHECK(logs.contains(log_level::INFO, "loaded module 'alpha'"));
    }

    TEST_CASE("module_manager - load of a dependent loads the closure in order and unload_all reverses it") {
        chain_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.gammaId));
        CHECK(fixture.state(fixture.alphaId) == module_state::LOADED);
        CHECK(fixture.state(fixture.betaId) == module_state::LOADED);
        CHECK(fixture.state(fixture.gammaId) == module_state::LOADED);
        CHECK(fixture.log == std::vector<std::string>{
            "validate:ecs:alpha", "attach:ecs:alpha",
            "validate:ecs:beta", "attach:ecs:beta",
            "validate:ecs:gamma", "attach:ecs:gamma",
        });
        REQUIRE(fixture.host.manager.registry().load_order().size() == 3);
        CHECK(fixture.host.manager.registry().load_order()[2] == fixture.gammaId);

        fixture.log.clear();
        fixture.host.manager.unload_all();
        CHECK(fixture.log == std::vector<std::string>{"detach:ecs:gamma", "detach:ecs:beta", "detach:ecs:alpha"});
        CHECK(counters().deactivating == 3);
        CHECK_FALSE(fixture.host.manager.any_loaded());
        CHECK(fixture.state(fixture.host.manager.registry().find(string_id("alpha"))) == module_state::UNLOADED);
    }

    TEST_CASE("module_manager - load_all resolves everything and continues past failures returning the first") {
        chain_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        static_module_source orphan("orphan", manifest_json("orphan", "1.0.0", {{"nobody"}}), REGISTERING_ENTRY);
        static_module_source brokenEntry("broken", manifest_json("broken"), FAILING_ENTRY);
        const module_id orphanId = fixture.host.manager.add_static_module(orphan.desc()).value();
        const module_id brokenId = fixture.host.manager.add_static_module(brokenEntry.desc()).value();

        const result<void> all = fixture.host.manager.load_all();
        REQUIRE_FALSE(all.has_value());
        CHECK(all.get_error().is_category<module_error>());
        CHECK(fixture.state(fixture.alphaId) == module_state::LOADED);
        CHECK(fixture.state(fixture.betaId) == module_state::LOADED);
        CHECK(fixture.state(fixture.gammaId) == module_state::LOADED);
        CHECK(fixture.state(orphanId) == module_state::FAILED);
        CHECK(fixture.host.manager.registry().last_error(orphanId).is(module_error::DEPENDENCY_MISSING));
        CHECK(fixture.state(brokenId) == module_state::FAILED);
        CHECK(fixture.host.manager.registry().last_error(brokenId).is(module_error::ENTRY_FAILED));
        CHECK(logs.contains(log_level::ERR, "orphan"));
        CHECK(logs.contains(log_level::ERR, "broken"));
        CHECK(fixture.host.manager.registry().load_order().size() == 3);
    }

    TEST_CASE("module_manager - a validate veto leaves the registry untouched and reports REGISTRATION_CONFLICT") {
        // Declared before the fixture: it must outlive the manager that calls it.
        std::vector<std::string> sharedLog = {};
        fake_consumer second("assets", &sharedLog);
        chain_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.host.manager.add_consumer(second);
        second.validateResult = make_error(error_code::ALREADY_EXISTS, "component 'alpha' already registered");

        const result<void> loaded = fixture.host.manager.load(fixture.alphaId);
        CONJURE_CHECK_ERROR(loaded, module_error::REGISTRATION_CONFLICT);
        CHECK(loaded.get_error().message().find("'assets'") != std::string::npos);
        CHECK(loaded.get_error().message().find("already registered") != std::string::npos);
        CHECK(fixture.state(fixture.alphaId) == module_state::FAILED);
        CHECK(fixture.host.manager.registry().last_error(fixture.alphaId) == loaded.get_error());
        CHECK(fixture.log == std::vector<std::string>{"validate:ecs:alpha"});
        CHECK(sharedLog == std::vector<std::string>{"validate:assets:alpha"});
        CHECK(fixture.consumer.attached().empty());
        CHECK(second.attached().empty());
        CHECK(fixture.host.manager.registry().registrations(fixture.alphaId).extensions.empty());
        CHECK(fixture.host.manager.registry().load_order().empty());
        CHECK(counters().activated == 0);
        CHECK_FALSE(fixture.host.manager.any_loaded());

        second.validateResult = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));
        CHECK(fixture.state(fixture.alphaId) == module_state::LOADED);
    }

    TEST_CASE("module_manager - REFUSE_IF_IN_USE reports IN_USE with the count and REMOVE_INSTANCES proceeds") {
        chain_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));
        fixture.consumer.liveUses = 12;
        fixture.log.clear();

        const result<void> refused = fixture.host.manager.unload(fixture.alphaId);
        CONJURE_CHECK_ERROR(refused, module_error::IN_USE);
        CHECK(refused.get_error().message().find("12") != std::string::npos);
        CHECK(fixture.state(fixture.alphaId) == module_state::LOADED);
        CHECK(fixture.log == std::vector<std::string>{"can_detach(refuse):ecs:alpha"});
        CHECK(counters().deactivating == 0);

        fixture.log.clear();
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.alphaId, unload_policy::REMOVE_INSTANCES));
        CHECK(fixture.log == std::vector<std::string>{"can_detach(remove):ecs:alpha", "detach:ecs:alpha"});
        CHECK(counters().deactivating == 1);
        CHECK_FALSE(fixture.host.manager.registry().is_valid(fixture.alphaId));
    }

    TEST_CASE("module_manager - a consumer veto is returned unchanged and nothing detaches") {
        chain_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));
        fixture.consumer.canDetachResult = core::make_error(module_error::UNLOAD_BLOCKED, "play mode is active");

        const result<void> blocked = fixture.host.manager.unload(fixture.alphaId, unload_policy::REMOVE_INSTANCES);
        CONJURE_CHECK_ERROR(blocked, module_error::UNLOAD_BLOCKED);
        CHECK(blocked.get_error().message() == "play mode is active");
        CHECK(fixture.state(fixture.alphaId) == module_state::LOADED);
        CHECK(fixture.consumer.is_attached(fixture.alphaId));
        CHECK(counters().deactivating == 0);
    }

    TEST_CASE("module_manager - a module with loaded dependents cannot unload") {
        chain_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.betaId));

        const result<void> refused = fixture.host.manager.unload(fixture.alphaId);
        CONJURE_CHECK_ERROR(refused, module_error::HAS_DEPENDENTS);
        CHECK(refused.get_error().message().find("beta") != std::string::npos);
        CHECK(fixture.state(fixture.alphaId) == module_state::LOADED);

        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.betaId));
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.alphaId));
        CHECK_FALSE(fixture.host.manager.any_loaded());
    }

    TEST_CASE("module_manager - unload bumps the generation, rejects the stale id and reloads under the new one") {
        chain_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.alphaId));

        CONJURE_CHECK_ERROR(fixture.host.manager.load(fixture.alphaId), module_error::STALE_ID);
        CONJURE_CHECK_ERROR(fixture.host.manager.unload(fixture.alphaId), module_error::STALE_ID);
        CHECK_FALSE(fixture.host.manager.registry().is_valid(fixture.alphaId));

        const module_id fresh = fixture.host.manager.registry().find(string_id("alpha"));
        CHECK(fresh.index == fixture.alphaId.index);
        CHECK(fresh.generation == 1);
        CHECK(fixture.state(fresh) == module_state::UNLOADED);
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fresh));
        CHECK(fixture.state(fresh) == module_state::LOADED);
        CHECK(fixture.host.manager.registry().load_count(fresh) == 2);
        CHECK(counters().entryRuns == 2);
    }

    TEST_CASE("module_manager - an entry failure is ENTRY_FAILED listing every reason and a retry may succeed") {
        chain_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        static_module_source broken("broken", manifest_json("broken"), FAILING_ENTRY);
        static_module_source silent("silent", manifest_json("silent"), STATUS_FAILING_ENTRY);
        static_module_source flaky("flaky", manifest_json("flaky"), FLAKY_ENTRY);
        const module_id brokenId = fixture.host.manager.add_static_module(broken.desc()).value();
        const module_id silentId = fixture.host.manager.add_static_module(silent.desc()).value();
        const module_id flakyId = fixture.host.manager.add_static_module(flaky.desc()).value();

        const result<void> failed = fixture.host.manager.load(brokenId);
        CONJURE_CHECK_ERROR(failed, module_error::ENTRY_FAILED);
        CHECK(failed.get_error().message().find("first reason") != std::string::npos);
        CHECK(failed.get_error().message().find("second reason") != std::string::npos);
        CHECK(fixture.state(brokenId) == module_state::FAILED);
        CHECK(fixture.consumer.calls().empty());
        CHECK(fixture.host.manager.registry().registrations(brokenId).errors.empty());

        const result<void> statusOnly = fixture.host.manager.load(silentId);
        CONJURE_CHECK_ERROR(statusOnly, module_error::ENTRY_FAILED);
        CHECK(statusOnly.get_error().message().find("status 1") != std::string::npos);

        CONJURE_CHECK_ERROR(fixture.host.manager.load(flakyId), module_error::ENTRY_FAILED);
        CHECK(fixture.state(flakyId) == module_state::FAILED);
        CONJURE_REQUIRE_OK(fixture.host.manager.load(flakyId));
        CHECK(fixture.state(flakyId) == module_state::LOADED);
        CHECK(fixture.host.manager.registry().last_error(flakyId) == error{});
        CHECK(logs.count(log_level::ERR) == 3);
    }

    TEST_CASE("module_manager - load stops at the first failure of the closure and leaves earlier successes loaded") {
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_module_source beta("beta", manifest_json("beta", "1.0.0", {{"alpha"}}), FAILING_ENTRY);
        static_module_source gamma("gamma", manifest_json("gamma", "1.0.0", {{"beta"}}), REGISTERING_ENTRY);
        static_host host = {};
        const module_id alphaId = host.manager.add_static_module(alpha.desc()).value();
        const module_id betaId = host.manager.add_static_module(beta.desc()).value();
        const module_id gammaId = host.manager.add_static_module(gamma.desc()).value();

        CONJURE_CHECK_ERROR(host.manager.load(gammaId), module_error::ENTRY_FAILED);
        CHECK(host.manager.registry().state(alphaId) == module_state::LOADED);
        CHECK(host.manager.registry().state(betaId) == module_state::FAILED);

        // Never attempted, so never RESOLVED: the module panel must not show a stale "about to load".
        CHECK(host.manager.registry().state(gammaId) == module_state::DISCOVERED);
        CHECK(host.manager.registry().load_order().size() == 1);
        CHECK(counters().entryRuns == 2);
    }

    TEST_CASE("module_manager - load_all does not load a dependent whose dependency failed to load") {
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        std::vector<std::string> log = {};
        fake_consumer consumer("ecs", &log);
        static_module_source alpha("alpha", manifest_json("alpha"), FAILING_ENTRY);
        static_module_source beta("beta", manifest_json("beta", "1.0.0", {{"alpha"}}), REGISTERING_ENTRY);
        static_module_source gamma("gamma", manifest_json("gamma", "1.0.0", {{"alpha", "*", true}}), REGISTERING_ENTRY);
        static_host host = {};
        host.manager.add_consumer(consumer);
        consumer.bind_registry(host.manager.registry());
        const module_id alphaId = host.manager.add_static_module(alpha.desc()).value();
        const module_id betaId = host.manager.add_static_module(beta.desc()).value();
        const module_id gammaId = host.manager.add_static_module(gamma.desc()).value();
        std::vector<std::string> failedEvents = {};
        const event_subscription onFailed =
          host.events.subscribe<module_load_failed_event>([&](const module_load_failed_event &event) {
              failedEvents.push_back(host.manager.registry().manifest(event.id).name);
          });

        // Resolution passes (alpha is registered); alpha then fails at load time, which must take
        // beta down with it while gamma, depending only optionally, still loads.
        const result<void> all = host.manager.load_all();
        CONJURE_CHECK_ERROR(all, module_error::ENTRY_FAILED);
        const module_registry &registry = host.manager.registry();
        CHECK(registry.state(alphaId) == module_state::FAILED);
        CHECK(registry.last_error(alphaId).is(module_error::ENTRY_FAILED));
        CHECK(registry.state(betaId) == module_state::FAILED);
        CHECK(registry.last_error(betaId).is(module_error::DEPENDENCY_FAILED));
        CHECK(registry.last_error(betaId).message().find("'alpha'") != std::string::npos);
        CHECK(registry.last_error(betaId).message().find("first reason") != std::string::npos);
        CHECK(registry.state(gammaId) == module_state::LOADED);
        CHECK(registry.dependencies(betaId).empty());
        CHECK(registry.dependents(alphaId).empty());
        REQUIRE(registry.load_order().size() == 1);
        CHECK(registry.load_order()[0] == gammaId);
        CHECK(counters().entryRuns == 2);
        CHECK(log == std::vector<std::string>{"validate:ecs:gamma", "attach:ecs:gamma"});
        CHECK(failedEvents == std::vector<std::string>{"alpha", "beta"});
        CHECK(logs.contains(log_level::ERR, "which failed to load"));
    }

    TEST_CASE("module_manager - an optional dependency loaded later is linked to the module already using it") {
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_module_source beta("beta", manifest_json("beta", "1.0.0", {{"alpha", "*", true}}), REGISTERING_ENTRY);
        static_host host = {};
        const module_id betaId = host.manager.add_static_module(beta.desc()).value();
        CONJURE_REQUIRE_OK(host.manager.load(betaId));
        CHECK(host.manager.registry().dependencies(betaId).empty());

        const module_id alphaId = host.manager.add_static_module(alpha.desc()).value();
        CONJURE_REQUIRE_OK(host.manager.load(alphaId));
        const module_registry &registry = host.manager.registry();
        REQUIRE(registry.dependents(alphaId).size() == 1);
        CHECK(registry.dependents(alphaId)[0] == betaId);
        REQUIRE(registry.dependencies(betaId).size() == 1);
        CHECK(registry.dependencies(betaId)[0] == alphaId);

        // The late link protects alpha and pulls beta into alpha's reload cascade.
        CONJURE_CHECK_ERROR(host.manager.unload(alphaId), module_error::HAS_DEPENDENTS);
        CONJURE_REQUIRE_OK(host.manager.reload(alphaId, reload_scope::FULL));
        CHECK(registry.load_count(betaId) == 2);
        CHECK(registry.load_count(alphaId) == 2);
        REQUIRE(registry.dependents(alphaId).size() == 1);
        CHECK(registry.dependents(alphaId)[0] == betaId);

        CONJURE_REQUIRE_OK(host.manager.unload(betaId));
        CHECK(registry.dependents(alphaId).empty());
        CONJURE_REQUIRE_OK(host.manager.unload(alphaId));
        CHECK_FALSE(host.manager.any_loaded());
    }

    TEST_CASE("module_manager - unload_all logs a fault and still empties the load order") {
        chain_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.betaId));
        fixture.consumer.liveUsesAfterDetach = 2;

        conjure::testing::expect_assert guard = {};
        fixture.host.manager.unload_all();
        CHECK(guard.count() == 2);
        CHECK_FALSE(fixture.host.manager.any_loaded());
        CHECK(fixture.host.manager.registry().load_order().empty());
        CHECK(fixture.state(fixture.betaId) == module_state::FAULTED);
        CHECK(fixture.state(fixture.alphaId) == module_state::FAULTED);
        CHECK(fixture.host.manager.registry().last_error(fixture.alphaId).is(module_error::FAULTED));
        CHECK(logs.contains(log_level::ERR, "unload_all:"));
        CHECK(fixture.log == std::vector<std::string>{
            "validate:ecs:alpha", "attach:ecs:alpha", "validate:ecs:beta", "attach:ecs:beta",
            "detach:ecs:beta", "detach:ecs:alpha",
        });
    }

    TEST_CASE("module_manager - a module needing a shared library is refused when they are disabled") {
        static_host host = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        const conjure::testing::temp_directory directory("modules_static_shared_disabled");
        const std::string nativeManifest = manifest_json("native", "1.0.0", {}, R"(, "binary": "native")");
        CONJURE_REQUIRE_OK(directory.write_text("native/module.json", nativeManifest));
        const module_id id = host.manager.add_manifest(directory.file("native/module.json")).value();

        const result<void> loaded = host.manager.load(id);
        CONJURE_CHECK_ERROR(loaded, module_error::SHARED_LIBRARIES_DISABLED);
        CHECK(loaded.get_error().message().find("native") != std::string::npos);
        CHECK(host.manager.registry().state(id) == module_state::FAILED);
    }

    TEST_CASE("module_manager - live uses surviving detach fault the module for good") {
        chain_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));
        fixture.consumer.liveUsesAfterDetach = 3;

        conjure::testing::expect_assert guard = {};
        const result<void> faulted = fixture.host.manager.unload(fixture.alphaId, unload_policy::REMOVE_INSTANCES);
        CHECK(guard.count() == 1);
        CONJURE_CHECK_ERROR(faulted, module_error::FAULTED);
        CHECK(fixture.state(fixture.alphaId) == module_state::FAULTED);
        CHECK(fixture.host.manager.registry().is_valid(fixture.alphaId));
        CHECK(fixture.host.manager.registry().load_order().empty());
        CHECK(logs.contains(log_level::ERR, "faulted"));

        CONJURE_CHECK_ERROR(fixture.host.manager.load(fixture.alphaId), module_error::FAULTED);
        CONJURE_CHECK_ERROR(fixture.host.manager.unload(fixture.alphaId), module_error::FAULTED);
        CONJURE_CHECK_ERROR(fixture.host.manager.reload(fixture.alphaId, reload_scope::FULL), module_error::FAULTED);
        fixture.consumer.liveUsesAfterDetach = 0;
        CONJURE_CHECK_ERROR(fixture.host.manager.load_all(), module_error::DEPENDENCY_MISSING);
        CHECK(fixture.state(fixture.alphaId) == module_state::FAULTED);
        CHECK(fixture.state(fixture.betaId) == module_state::FAILED);
        const error &betaError = fixture.host.manager.registry().last_error(fixture.betaId);
        CHECK(betaError.is(module_error::DEPENDENCY_MISSING));
        CHECK(betaError.message().find("'alpha', which is FAULTED") != std::string::npos);
        CHECK(betaError.message().find("live use(s)") != std::string::npos);
        CHECK(betaError.message().find("not registered") == std::string::npos);
    }

    TEST_CASE("module_manager - consumers and services are fixed once a module is loaded") {
        chain_fixture fixture = {};
        fake_consumer late("late");
        world_service world = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));

        CONJURE_CHECK_ASSERTS(fixture.host.manager.add_consumer(late));
        CONJURE_CHECK_ASSERTS(fixture.host.manager.remove_consumer(fixture.consumer));
        CONJURE_CHECK_ASSERTS(fixture.host.manager.provide_service(world));
        CHECK(fixture.host.manager.services().size() == 0);

        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.alphaId));
        fixture.host.manager.provide_service(world);
        CHECK(fixture.host.manager.services().find<world_service>() == &world);
        fixture.host.manager.add_consumer(late);
        CONJURE_CHECK_ASSERTS(fixture.host.manager.add_consumer(late));
        fixture.host.manager.remove_consumer(late);
        fixture.host.manager.remove_consumer(late);
    }

    TEST_CASE("module_manager - a provided service reaches the module context") {
        world_service world = {};
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_host host = {};
        host.manager.provide_service(world);
        const module_id id = host.manager.add_static_module(alpha.desc()).value();
        CONJURE_REQUIRE_OK(host.manager.load(id));
        CHECK(host.manager.services().find<world_service>() == &world);
        CHECK(host.manager.services().contains(world_service::SERVICE_ID));
    }
}
