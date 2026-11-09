#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "fake_consumer.h"
#include "static_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_events.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/string_id.h"

#include <stop_token>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    bool sInsideFrame = false;

    bool inside_frame() {
        return sInsideFrame;
    }

    struct request_fixture {
        std::vector<std::string> log = {};
        fake_consumer ecs{"ecs", &log};
        static_module_source alpha{"alpha", manifest_json("alpha"), REGISTERING_ENTRY};
        static_module_source beta{"beta", manifest_json("beta", "1.0.0", {{"alpha"}}), REGISTERING_ENTRY};
        static_module_source zeta{"zeta", manifest_json("zeta"), REGISTERING_ENTRY};
        static_host host = {};
        module_id alphaId = {};
        module_id betaId = {};
        module_id zetaId = {};

        request_fixture() {
            host.manager.add_consumer(ecs);
            ecs.bind_registry(host.manager.registry());
            alphaId = host.manager.add_static_module(alpha.desc()).value();
            betaId = host.manager.add_static_module(beta.desc()).value();
            zetaId = host.manager.add_static_module(zeta.desc()).value();
        }
    };
}

TEST_SUITE("modules/module_requests") {

    TEST_CASE("module_manager - requests are applied in the order they were made") {
        request_fixture fixture = {};
        fixture.host.manager.request_load(fixture.zetaId);
        fixture.host.manager.request_load(fixture.betaId);
        fixture.host.manager.request_unload(fixture.zetaId, unload_policy::REMOVE_INSTANCES);
        CHECK(fixture.host.manager.pending_count() == 3);
        CHECK_FALSE(fixture.host.manager.any_loaded());

        fixture.host.manager.apply_pending();
        CHECK(fixture.host.manager.pending_count() == 0);
        CHECK(fixture.log == std::vector<std::string>{
            "validate:ecs:zeta", "attach:ecs:zeta",
            "validate:ecs:alpha", "attach:ecs:alpha",
            "validate:ecs:beta", "attach:ecs:beta",
            "can_detach(remove):ecs:zeta", "detach:ecs:zeta",
        });
        const module_registry &registry = fixture.host.manager.registry();
        REQUIRE(registry.load_order().size() == 2);
        CHECK(registry.load_order()[0] == fixture.alphaId);
        CHECK(registry.load_order()[1] == fixture.betaId);
        CHECK_FALSE(registry.is_valid(fixture.zetaId));
    }

    TEST_CASE("module_manager - a request for a stale or invalid id is dropped with a warning") {
        request_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.zetaId));
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.zetaId));
        fixture.log.clear();

        fixture.host.manager.request_load(fixture.zetaId);
        fixture.host.manager.request_unload(module_id{}, unload_policy::REFUSE_IF_IN_USE);
        fixture.host.manager.request_reload(module_id{99, 0}, reload_scope::FULL);
        fixture.host.manager.apply_pending();

        CHECK(fixture.log.empty());
        CHECK(logs.count(log_level::WARN) == 3);
        CHECK(logs.contains(log_level::WARN, "stale or invalid"));
        CHECK(fixture.host.manager.registry().state(fixture.host.manager.registry().find(string_id("zeta")))
          == module_state::UNLOADED);
    }

    TEST_CASE("module_manager - apply_pending inside a frame asserts and keeps the queue") {
        module_manager_desc desc = static_host::make_desc();
        desc.isInsideFrame = &inside_frame;
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_host host(desc);
        const module_id id = host.manager.add_static_module(alpha.desc()).value();

        sInsideFrame = true;
        host.manager.request_load(id);
        CONJURE_CHECK_ASSERTS(host.manager.apply_pending());
        CHECK(host.manager.pending_count() == 1);
        CHECK(host.manager.registry().state(id) == module_state::DISCOVERED);
        CONJURE_CHECK_ASSERTS(CHECK_FALSE(host.manager.load(id).has_value()));

        sInsideFrame = false;
        host.manager.apply_pending();
        CHECK(host.manager.pending_count() == 0);
        CHECK(host.manager.registry().state(id) == module_state::LOADED);
    }

    TEST_CASE("module_manager - a request made off the main thread asserts and is dropped") {
        request_fixture fixture = {};
        conjure::testing::log_capture logs = {};

        // The dedicated thread is never the main thread; the assert it raises is recorded by the
        // most recently created live scope, which is this one.
        conjure::testing::expect_assert guard = {};
        {
            jobs::dedicated_thread requester("modules-request", [&fixture](std::stop_token) {
                fixture.host.manager.request_load(fixture.alphaId);
            });
            requester.join();
        }
        CHECK(guard.count() == 1);
        CHECK(guard.last_message().find("main thread") != std::string::npos);
        CHECK(fixture.host.manager.pending_count() == 0);

        fixture.host.manager.apply_pending();
        CHECK(fixture.log.empty());
        CHECK(fixture.host.manager.registry().state(fixture.alphaId) == module_state::DISCOVERED);

        // The same request from the main thread is accepted.
        fixture.host.manager.request_load(fixture.alphaId);
        CHECK(fixture.host.manager.pending_count() == 1);
        fixture.host.manager.apply_pending();
        CHECK(fixture.host.manager.registry().state(fixture.alphaId) == module_state::LOADED);
    }

    TEST_CASE("module_manager - an idle apply_pending touches nothing") {
        request_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));
        fixture.log.clear();
        conjure::testing::log_capture logs = {};

        fixture.host.manager.apply_pending();
        fixture.host.manager.apply_pending();
        CHECK(fixture.host.manager.pending_count() == 0);
        CHECK(fixture.log.empty());
        CHECK(logs.count() == 0);
        CHECK(fixture.host.manager.registry().state(fixture.alphaId) == module_state::LOADED);
    }

    TEST_CASE("module_manager - a refused request is logged and stored as the module's last error") {
        request_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.betaId));

        fixture.host.manager.request_unload(fixture.alphaId, unload_policy::REMOVE_INSTANCES);
        fixture.host.manager.apply_pending();
        CHECK(fixture.host.manager.registry().state(fixture.alphaId) == module_state::LOADED);
        CHECK(fixture.host.manager.registry().last_error(fixture.alphaId).is(module_error::HAS_DEPENDENTS));
        CHECK(logs.contains(log_level::WARN, "refused"));
        CHECK(logs.count(log_level::ERR) == 0);
    }

    TEST_CASE("module_manager - requests made while applying wait for the next apply_pending") {
        request_fixture fixture = {};
        bool requestedAgain = false;
        const event_subscription onLoaded =
          fixture.host.events.subscribe<module_loaded_event>([&](const module_loaded_event &event) {
              if (event.id == fixture.zetaId && !requestedAgain) {
                  requestedAgain = true;
                  fixture.host.manager.request_unload(fixture.zetaId, unload_policy::REMOVE_INSTANCES);
              }
          });

        fixture.host.manager.request_load(fixture.zetaId);
        fixture.host.manager.apply_pending();
        CHECK(fixture.host.manager.registry().state(fixture.zetaId) == module_state::LOADED);
        CHECK(fixture.host.manager.pending_count() == 1);

        fixture.host.manager.apply_pending();
        CHECK(fixture.host.manager.pending_count() == 0);
        CHECK_FALSE(fixture.host.manager.registry().is_valid(fixture.zetaId));
    }
}
