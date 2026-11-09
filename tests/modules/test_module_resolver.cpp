#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "static_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_resolver.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    struct resolver_fixture {
        std::vector<std::unique_ptr<static_module_source>> sources = {};
        static_host host = {};

        module_id add(
          std::string_view name,
          std::string_view version = "1.0.0",
          const std::vector<dependency_spec> &dependencies = {},
          std::string_view extra = {},
          std::string_view engine = "*"
        ) {
            sources.push_back(std::make_unique<static_module_source>(
              std::string(name), manifest_json(name, version, dependencies, extra, engine), nullptr));
            const result<module_id> added = host.manager.add_static_module(sources.back()->desc());
            INFO("add_static_module failed: ", conjure::testing::describe_error(added));
            REQUIRE(added.has_value());
            return added.value();
        }

        [[nodiscard]] resolution_result resolve(
          semantic_version engineVersion = host_engine_version(),
          module_feature_flags engineFeatures = module_feature_flags::NONE
        ) const {
            const module_registry &registry = host.manager.registry();
            std::vector<module_id> candidates = {};
            for (const module_id id : registry.all()) {
                const module_state state = registry.state(id);
                if (state != module_state::LOADED && state != module_state::SKIPPED && state != module_state::FAULTED) {
                    candidates.push_back(id);
                }
            }
            resolution_input input = {};
            input.candidates = candidates;
            input.loaded = registry.load_order();
            input.registry = &registry;
            input.engineVersion = engineVersion;
            input.engineFeatures = engineFeatures;
            return resolve_load_order(input);
        }

        [[nodiscard]] std::vector<std::string> names(const std::vector<module_id> &ids) const {
            std::vector<std::string> result = {};
            for (const module_id id : ids) {
                result.push_back(host.manager.registry().manifest(id).name);
            }
            return result;
        }

        [[nodiscard]] const resolution_failure *failure_of(
          const resolution_result &resolved,
          std::string_view name
        ) const {
            for (const resolution_failure &failure : resolved.failures) {
                if (host.manager.registry().manifest(failure.id).name == name) {
                    return &failure;
                }
            }
            return nullptr;
        }
    };
}

TEST_SUITE("modules/module_resolver") {

    TEST_CASE("resolve_load_order - a linear chain loads dependencies first") {
        resolver_fixture fixture = {};
        fixture.add("a", "1.0.0", {{"b"}});
        fixture.add("b", "1.0.0", {{"c"}});
        fixture.add("c");

        const resolution_result resolved = fixture.resolve();
        CHECK(resolved.failures.empty());
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"c", "b", "a"});
    }

    TEST_CASE("resolve_load_order - a diamond resolves once per module with name tie-breaks") {
        resolver_fixture fixture = {};
        fixture.add("top", "1.0.0", {{"left"}, {"right"}});
        fixture.add("right", "1.0.0", {{"base"}});
        fixture.add("left", "1.0.0", {{"base"}});
        fixture.add("base");

        const resolution_result resolved = fixture.resolve();
        CHECK(resolved.failures.empty());
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"base", "left", "right", "top"});
    }

    TEST_CASE("resolve_load_order - independent modules sort by name whatever their registration order") {
        resolver_fixture fixture = {};
        fixture.add("zeta");
        fixture.add("alpha");
        fixture.add("mid");

        const resolution_result resolved = fixture.resolve();
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"alpha", "mid", "zeta"});
    }

    TEST_CASE("resolve_load_order - a missing dependency fails the dependent unless it is optional") {
        resolver_fixture fixture = {};
        fixture.add("needs_ghost", "1.0.0", {{"ghost"}});
        fixture.add("tolerates_ghost", "1.0.0", {{"ghost", "*", true}});

        const resolution_result resolved = fixture.resolve();
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"tolerates_ghost"});
        REQUIRE(resolved.failures.size() == 1);
        const resolution_failure *failure = fixture.failure_of(resolved, "needs_ghost");
        REQUIRE(failure != nullptr);
        CHECK(failure->error.is(module_error::DEPENDENCY_MISSING));
        CHECK(failure->error.message().find("'ghost'") != std::string::npos);
    }

    TEST_CASE("resolve_load_order - a dependency outside the version range is DEPENDENCY_VERSION") {
        resolver_fixture fixture = {};
        fixture.add("app", "1.0.0", {{"lib", "^2.0.0"}});
        fixture.add("lib", "1.5.0");

        const resolution_result resolved = fixture.resolve();
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"lib"});
        const resolution_failure *failure = fixture.failure_of(resolved, "app");
        REQUIRE(failure != nullptr);
        CHECK(failure->error.is(module_error::DEPENDENCY_VERSION));
        CHECK(failure->error.message().find("1.5.0") != std::string::npos);
        CHECK(failure->error.message().find(">=2.0.0 <3.0.0") != std::string::npos);
    }

    TEST_CASE("resolve_load_order - two and three node cycles are DEPENDENCY_CYCLE with the cycle in the message") {
        resolver_fixture fixture = {};
        fixture.add("x", "1.0.0", {{"y"}});
        fixture.add("y", "1.0.0", {{"x"}});
        fixture.add("p", "1.0.0", {{"q"}});
        fixture.add("q", "1.0.0", {{"r"}});
        fixture.add("r", "1.0.0", {{"p"}});
        fixture.add("free");

        const resolution_result resolved = fixture.resolve();
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"free"});
        CHECK(resolved.failures.size() == 5);

        u32 cycles = 0;
        u32 propagated = 0;
        for (const resolution_failure &failure : resolved.failures) {
            if (failure.error.is(module_error::DEPENDENCY_CYCLE)) {
                ++cycles;
                CHECK(failure.error.message().find("->") != std::string::npos);
            } else {
                CHECK(failure.error.is(module_error::DEPENDENCY_FAILED));
                ++propagated;
            }
        }
        CHECK(cycles == 2);
        CHECK(propagated == 3);

        bool sawTwoCycle = false;
        bool sawThreeCycle = false;
        for (const resolution_failure &failure : resolved.failures) {
            const std::string &message = failure.error.message();
            if (message.find("x -> y -> x") != std::string::npos || message.find("y -> x -> y") != std::string::npos) {
                sawTwoCycle = true;
            }
            if (message.find("p -> q -> r -> p") != std::string::npos
              || message.find("q -> r -> p -> q") != std::string::npos
              || message.find("r -> p -> q -> r") != std::string::npos) {
                sawThreeCycle = true;
            }
        }
        CHECK(sawTwoCycle);
        CHECK(sawThreeCycle);
    }

    TEST_CASE("resolve_load_order - a failure propagates to dependents naming the root cause") {
        resolver_fixture fixture = {};
        fixture.add("root", "1.0.0", {}, R"(, "requires": ["renderer"])");
        fixture.add("middle", "1.0.0", {{"root"}});
        fixture.add("leaf", "1.0.0", {{"middle"}});
        fixture.add("optional_user", "1.0.0", {{"root", "*", true}});

        const resolution_result resolved = fixture.resolve(host_engine_version(), module_feature_flags::NONE);
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"optional_user"});
        REQUIRE(resolved.failures.size() == 3);
        CHECK(fixture.failure_of(resolved, "root")->error.is(module_error::FEATURE_UNAVAILABLE));
        const resolution_failure *leaf = fixture.failure_of(resolved, "leaf");
        REQUIRE(leaf != nullptr);
        CHECK(leaf->error.is(module_error::DEPENDENCY_FAILED));
        CHECK(leaf->error.message().find("'middle'") != std::string::npos);
        CHECK(leaf->error.message().find("renderer") != std::string::npos);
        CHECK(fixture.failure_of(resolved, "middle")->error.is(module_error::DEPENDENCY_FAILED));
    }

    TEST_CASE("resolve_load_order - the game module is forced last and a second one is rejected") {
        resolver_fixture fixture = {};
        fixture.add("aaa_game", "1.0.0", {}, R"(, "kind": "game")");
        fixture.add("zzz_plugin");
        fixture.add("mmm_plugin");

        const resolution_result resolved = fixture.resolve();
        CHECK(resolved.failures.empty());
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"mmm_plugin", "zzz_plugin", "aaa_game"});

        fixture.add("bbb_game", "1.0.0", {}, R"(, "kind": "game")");
        const resolution_result twoGames = fixture.resolve();
        REQUIRE(twoGames.failures.size() == 1);
        CHECK(twoGames.failures[0].error.is(module_error::MULTIPLE_GAME_MODULES));
        CHECK(fixture.host.manager.registry().manifest(twoGames.failures[0].id).name == "bbb_game");
        CHECK(twoGames.failures[0].error.message().find("'aaa_game'") != std::string::npos);
        CHECK(fixture.names(twoGames.order) == std::vector<std::string>{"mmm_plugin", "zzz_plugin", "aaa_game"});
    }

    TEST_CASE("resolve_load_order - a loaded game module rejects every candidate game module") {
        resolver_fixture fixture = {};
        const module_id game = fixture.add("the_game", "1.0.0", {}, R"(, "kind": "game")");
        CONJURE_REQUIRE_OK(fixture.host.manager.load(game));
        fixture.add("another_game", "1.0.0", {}, R"(, "kind": "game")");

        const resolution_result resolved = fixture.resolve();
        CHECK(resolved.order.empty());
        REQUIRE(resolved.failures.size() == 1);
        CHECK(resolved.failures[0].error.is(module_error::MULTIPLE_GAME_MODULES));
    }

    TEST_CASE("resolve_load_order - loaded modules satisfy dependencies and are never reordered") {
        resolver_fixture fixture = {};
        const module_id base = fixture.add("base", "2.1.0");
        CONJURE_REQUIRE_OK(fixture.host.manager.load(base));
        fixture.add("user", "1.0.0", {{"base", "^2.0.0"}});
        fixture.add("picky", "1.0.0", {{"base", "^3.0.0"}});

        const resolution_result resolved = fixture.resolve();
        CHECK(fixture.names(resolved.order) == std::vector<std::string>{"user"});
        REQUIRE(resolved.failures.size() == 1);
        CHECK(fixture.failure_of(resolved, "picky")->error.is(module_error::DEPENDENCY_VERSION));
    }

    TEST_CASE("resolve_load_order - a SKIPPED dependency counts as missing") {
        resolver_fixture fixture = {};
        fixture.add("elsewhere", "1.0.0", {}, R"(, "platforms": [])");
        fixture.add("needs_elsewhere", "1.0.0", {{"elsewhere"}});
        CHECK(fixture.host.manager.registry().state(fixture.host.manager.registry().find(string_id("elsewhere")))
          == module_state::SKIPPED);

        const resolution_result resolved = fixture.resolve();
        CHECK(resolved.order.empty());
        REQUIRE(resolved.failures.size() == 1);
        CHECK(resolved.failures[0].error.is(module_error::DEPENDENCY_MISSING));

        // The message names the real cause, not a missing module.json.
        CHECK(resolved.failures[0].error.message().find("'elsewhere', which is SKIPPED on this platform")
          != std::string::npos);
        CHECK(resolved.failures[0].error.message().find("not registered") == std::string::npos);
    }

    TEST_CASE("resolve_load_order - engine version and feature requirements are checked per module") {
        resolver_fixture fixture = {};
        fixture.add("old_engine", "1.0.0", {}, {}, ">=99.0.0");
        fixture.add("needs_physics", "1.0.0", {}, R"(, "requires": ["physics", "audio"])");
        fixture.add("fits");

        const resolution_result partial = fixture.resolve(host_engine_version(), module_feature_flags::PHYSICS);
        CHECK(fixture.names(partial.order) == std::vector<std::string>{"fits"});
        REQUIRE(partial.failures.size() == 2);
        const resolution_failure *oldEngine = fixture.failure_of(partial, "old_engine");
        REQUIRE(oldEngine != nullptr);
        CHECK(oldEngine->error.is(module_error::ENGINE_VERSION_MISMATCH));
        CHECK(oldEngine->error.message().find(">=99.0.0") != std::string::npos);
        const resolution_failure *physics = fixture.failure_of(partial, "needs_physics");
        REQUIRE(physics != nullptr);
        CHECK(physics->error.is(module_error::FEATURE_UNAVAILABLE));
        CHECK(physics->error.message().find("feature(s) audio ") != std::string::npos);
        CHECK(physics->error.message().find("physics, audio") == std::string::npos);

        const resolution_result all = fixture.resolve(
          semantic_version{99, 0, 0}, module_feature_flags::PHYSICS | module_feature_flags::AUDIO);
        CHECK(all.failures.empty());
        CHECK(all.order.size() == 3);
    }

    TEST_CASE("resolve_load_order - failures are reported in name order and a null registry asserts") {
        resolver_fixture fixture = {};
        fixture.add("zzz", "1.0.0", {{"nope"}});
        fixture.add("aaa", "1.0.0", {{"nope"}});
        const resolution_result resolved = fixture.resolve();
        REQUIRE(resolved.failures.size() == 2);
        CHECK(fixture.host.manager.registry().manifest(resolved.failures[0].id).name == "aaa");
        CHECK(fixture.host.manager.registry().manifest(resolved.failures[1].id).name == "zzz");

        resolution_input input = {};
        conjure::testing::expect_assert guard = {};
        const resolution_result empty = resolve_load_order(input);
        CHECK(guard.count() == 1);
        CHECK(empty.order.empty());
        CHECK(empty.failures.empty());
    }

    TEST_CASE("resolve_load_order - an empty candidate list resolves to nothing") {
        resolver_fixture fixture = {};
        const resolution_result resolved = fixture.resolve();
        CHECK(resolved.order.empty());
        CHECK(resolved.failures.empty());
    }
}
