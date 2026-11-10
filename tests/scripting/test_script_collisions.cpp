#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/scripting/script_runtime.h"

#include <array>
#include <string>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view COLLIDER = "Conjure.TestScripts.Collider";
    constexpr std::string_view ENTER_ONLY = "Conjure.TestScripts.EnterOnlyCollider";

    [[nodiscard]] collision_event event_between(ecs::entity a, ecs::entity b, collision_kind kind) {
        collision_event event = {};
        event.a = a;
        event.b = b;
        event.normal = math::vec3(1.0f, 0.0f, 0.0f);
        event.impulse = 2.5f;
        event.kind = kind;
        return event;
    }
}

TEST_SUITE("scripting/script_collisions") {

    TEST_CASE("script collisions - events reach only the overriding callbacks [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id full = fixture.add_script(a, COLLIDER);
        const script_entry_id enterOnly = fixture.add_script(b, ENTER_ONLY);

        const std::array<collision_event, 4> events = {
          event_between(a, b, collision_kind::COLLISION_ENTER),
          event_between(a, b, collision_kind::COLLISION_EXIT),
          event_between(a, b, collision_kind::TRIGGER_ENTER),
          event_between(a, b, collision_kind::TRIGGER_EXIT),
        };
        fixture.runtime.dispatch_collision_events(events);

        CHECK(fixture.field_i64(full, "Enters") == 1);
        CHECK(fixture.field_i64(full, "Exits") == 1);
        CHECK(fixture.field_i64(full, "TriggerEnters") == 1);
        CHECK(fixture.field_i64(full, "TriggerExits") == 1);
        CHECK(fixture.fields_of(full).root()["LastOther"].as_u64() == b.to_u64());
        CHECK(fixture.field_f64(full, "LastImpulse") == doctest::Approx(2.5));
        CHECK(fixture.field_i64(enterOnly, "Enters") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("script collisions - one record per instance per pair, flipped for b [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const ecs::entity bystander = fixture.create_entity();
        const script_entry_id first = fixture.add_script(a, COLLIDER);
        const script_entry_id second = fixture.add_script(a, COLLIDER);     // two scripts on a
        const script_entry_id other = fixture.add_script(b, COLLIDER);
        const script_entry_id uninvolved = fixture.add_script(bystander, COLLIDER);

        const std::array<collision_event, 1> events = {event_between(a, b, collision_kind::COLLISION_ENTER)};
        fixture.runtime.dispatch_collision_events(events);
        CHECK(fixture.field_i64(first, "Enters") == 1);
        CHECK(fixture.field_i64(second, "Enters") == 1);
        CHECK(fixture.field_i64(other, "Enters") == 1);
        CHECK(fixture.field_i64(uninvolved, "Enters") == 0);
        CHECK(fixture.field_f64(first, "LastNormalX") == doctest::Approx(1.0));
        CHECK(fixture.field_f64(other, "LastNormalX") == doctest::Approx(-1.0));
        CHECK(fixture.fields_of(other).root()["LastOther"].as_u64() == a.to_u64());

        // The same pair again counts again (one record per event), an empty batch does nothing.
        fixture.runtime.dispatch_collision_events(events);
        fixture.runtime.dispatch_collision_events({});
        CHECK(fixture.field_i64(first, "Enters") == 2);
        CHECK(fixture.field_i64(other, "Enters") == 2);
    }
}
