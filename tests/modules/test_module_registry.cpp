#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "static_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/string_id.h"

#include <span>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    struct registry_fixture {
        static_module_source alpha{"alpha", manifest_json("alpha", "1.2.0"), REGISTERING_ENTRY};
        static_module_source beta{"beta", manifest_json("beta", "0.1.0", {{"alpha", "^1.0.0"}}), REGISTERING_ENTRY};
        static_host host = {};
        module_id alphaId = {};
        module_id betaId = {};

        registry_fixture() {
            alphaId = host.manager.add_static_module(alpha.desc()).value();
            betaId = host.manager.add_static_module(beta.desc()).value();
        }
    };
}

TEST_SUITE("modules/module_registry") {

    TEST_CASE("module_registry - find, is_valid and the per-module getters describe registered modules") {
        const registry_fixture fixture = {};
        const module_registry &registry = fixture.host.manager.registry();

        CHECK(registry.size() == 2);
        CHECK(registry.find(string_id("alpha")) == fixture.alphaId);
        CHECK(registry.find(string_id("beta")) == fixture.betaId);
        CHECK_FALSE(registry.find(string_id("gamma")).is_valid());
        CHECK(registry.is_valid(fixture.alphaId));
        CHECK_FALSE(registry.is_valid(module_id{}));
        CHECK_FALSE(registry.is_valid(module_id{7, 0}));
        CHECK_FALSE(registry.is_valid(module_id{fixture.alphaId.index, 5}));

        CHECK(registry.state(fixture.alphaId) == module_state::DISCOVERED);
        CHECK(registry.manifest(fixture.alphaId).name == "alpha");
        CHECK(registry.manifest(fixture.betaId).dependencies.size() == 1);
        CHECK(registry.registrations(fixture.alphaId).extensions.empty());
        CHECK(registry.last_error(fixture.alphaId) == error{});
        CHECK(registry.root_directory(fixture.alphaId).empty());
        CHECK(registry.binary_path(fixture.alphaId).empty());
        CHECK(registry.load_count(fixture.alphaId) == 0);
        CHECK(registry.all().size() == 2);
        CHECK(registry.all()[0] == fixture.alphaId);
        CHECK(registry.all()[1] == fixture.betaId);
        CHECK(registry.load_order().empty());
        CHECK(registry.dependents(fixture.alphaId).empty());
        CHECK(registry.dependencies(fixture.betaId).empty());
    }

    TEST_CASE("module_registry - load order, dependents and registrations follow loads and unloads") {
        registry_fixture fixture = {};
        module_registry &registry = fixture.host.manager.registry();

        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.betaId));
        CHECK(registry.state(fixture.alphaId) == module_state::LOADED);
        CHECK(registry.state(fixture.betaId) == module_state::LOADED);
        REQUIRE(registry.load_order().size() == 2);
        CHECK(registry.load_order()[0] == fixture.alphaId);
        CHECK(registry.load_order()[1] == fixture.betaId);
        REQUIRE(registry.dependents(fixture.alphaId).size() == 1);
        CHECK(registry.dependents(fixture.alphaId)[0] == fixture.betaId);
        REQUIRE(registry.dependencies(fixture.betaId).size() == 1);
        CHECK(registry.dependencies(fixture.betaId)[0] == fixture.alphaId);
        CHECK(registry.registrations(fixture.alphaId).extensions.size() == 2);
        CHECK(registry.load_count(fixture.alphaId) == 1);

        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.betaId));
        CHECK(registry.dependents(fixture.alphaId).empty());
        CHECK(registry.load_order().size() == 1);
        CHECK_FALSE(registry.is_valid(fixture.betaId));
        const module_id newBeta = registry.find(string_id("beta"));
        CHECK(newBeta.index == fixture.betaId.index);
        CHECK(newBeta.generation == fixture.betaId.generation + 1);
        CHECK(registry.state(newBeta) == module_state::UNLOADED);
        CHECK(registry.registrations(newBeta).extensions.empty());
        CHECK(registry.all()[1] == newBeta);
    }

    TEST_CASE("module_registry - ownership notes answer owner_of until the owner is forgotten") {
        registry_fixture fixture = {};
        module_registry &registry = fixture.host.manager.registry();
        const string_id point("test.component");

        registry.note_ownership(point, string_id("spinner"), fixture.alphaId);
        registry.note_ownership(point, string_id("panel"), fixture.betaId);
        registry.note_ownership(string_id("test.system"), string_id("spinner"), fixture.betaId);
        CHECK(registry.owner_of(point, string_id("spinner")) == fixture.alphaId);
        CHECK(registry.owner_of(point, string_id("panel")) == fixture.betaId);
        CHECK(registry.owner_of(string_id("test.system"), string_id("spinner")) == fixture.betaId);
        CHECK_FALSE(registry.owner_of(point, string_id("unknown")).is_valid());

        registry.note_ownership(point, string_id("spinner"), fixture.betaId);
        CHECK(registry.owner_of(point, string_id("spinner")) == fixture.betaId);

        registry.forget_ownership(fixture.betaId);
        CHECK_FALSE(registry.owner_of(point, string_id("spinner")).is_valid());
        CHECK_FALSE(registry.owner_of(point, string_id("panel")).is_valid());
        CHECK_FALSE(registry.owner_of(string_id("test.system"), string_id("spinner")).is_valid());

        CONJURE_CHECK_ASSERTS(registry.note_ownership(point, string_id("x"), module_id{}));
        CHECK_FALSE(registry.owner_of(point, string_id("x")).is_valid());
    }

    TEST_CASE("module_registry - unloading a module drops the ownership it noted") {
        registry_fixture fixture = {};
        module_registry &registry = fixture.host.manager.registry();
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.alphaId));
        registry.note_ownership(string_id("test.component"), string_id("alpha"), fixture.alphaId);
        CHECK(registry.owner_of(string_id("test.component"), string_id("alpha")) == fixture.alphaId);

        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.alphaId));
        CHECK_FALSE(registry.owner_of(string_id("test.component"), string_id("alpha")).is_valid());
    }

    TEST_CASE("module_registry - getters on an invalid or stale id assert and return empty values") {
        registry_fixture fixture = {};
        const module_registry &registry = fixture.host.manager.registry();
        const module_id stale = {fixture.alphaId.index, 9};

        conjure::testing::expect_assert guard = {};
        CHECK(registry.state(module_id{}) == module_state::UNLOADED);
        CHECK(registry.manifest(stale).name.empty());
        CHECK(registry.registrations(stale).extensions.empty());
        CHECK(registry.last_error(module_id{}) == error{});
        CHECK(registry.root_directory(stale).empty());
        CHECK(registry.binary_path(stale).empty());
        CHECK(registry.load_count(stale) == 0);
        CHECK(registry.dependents(stale).empty());
        CHECK(registry.dependencies(module_id{}).empty());
        CHECK(guard.count() == 9);
    }

    TEST_CASE("module_registry - the const and mutable views of the manager are the same object") {
        registry_fixture fixture = {};
        const module_manager &constManager = fixture.host.manager;
        CHECK(&constManager.registry() == &fixture.host.manager.registry());
    }
}
