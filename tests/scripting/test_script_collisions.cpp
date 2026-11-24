#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/scripting/script_runtime.h"

#include <array>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view COLLIDER = "Conjure.TestScripts.Collider";
    constexpr std::string_view ENTER_ONLY = "Conjure.TestScripts.EnterOnlyCollider";

    constexpr f32 IMPULSE = 2.5f;          // what LastImpulse and LastStayImpulse read back
}

TEST_SUITE("scripting/script_collisions") {

    TEST_CASE("regression - code sweep W3-CS-L4 a STAY event reaches only the classes that override OnCollisionStay") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({
          make_class("Game.Stayer", {script_callback::COLLISION_STAY}),
          make_class("Game.Bumper", {script_callback::COLLISION_ENTER}),
        });
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id stayer = fixture.runtime.add_script(a, hash_of("Game.Stayer"), {});
        REQUIRE(stayer.is_valid());
        REQUIRE(fixture.runtime.add_script(b, hash_of("Game.Bumper"), {}).is_valid());
        fixture.runtime.flush_pending();
        REQUIRE(fixture.runtime.stats().liveInstances == 2);

        const std::array<collision_event, 1> events = {
          collision_between(a, b, collision_kind::COLLISION_STAY, IMPULSE),
        };
        fixture.runtime.dispatch_collision_events(events);
        const std::vector<null_host_recorder::collision_record> &records = fixture.null_host().recorder().collisions();
        REQUIRE(records.size() == 1);
        CHECK(records[0].slot == fixture.runtime.entry(stayer)->managedSlot);
        CHECK(records[0].kind == collision_kind::COLLISION_STAY);
        CHECK(records[0].other == b.to_u64());
    }

    TEST_CASE("script_callback - the stay and activity callbacks are named and appended") {
        // The round trip of every name is test_native_api's "names round trip"; these are the new names and values.
        CHECK(to_string(script_callback::COLLISION_STAY) == "COLLISION_STAY");
        CHECK(to_string(script_callback::ENABLE) == "ENABLE");
        CHECK(to_string(script_callback::DISABLE) == "DISABLE");
        CHECK(parse_script_callback("COLLISION_STAY") == script_callback::COLLISION_STAY);
        CHECK(parse_script_callback("ENABLE") == script_callback::ENABLE);
        CHECK(parse_script_callback("DISABLE") == script_callback::DISABLE);
        CHECK(parse_script_callback("STAY") == script_callback::COUNT);
        CHECK(parse_script_callback("") == script_callback::COUNT);
        // Appended: the older bits keep their values, and every bit fits the 16-bit callback mask.
        CHECK(static_cast<u8>(script_callback::RELOAD) == 10);
        CHECK(static_cast<u8>(script_callback::COLLISION_STAY) == 11);
        CHECK(static_cast<u8>(script_callback::ENABLE) == 12);
        CHECK(static_cast<u8>(script_callback::DISABLE) == 13);
        CHECK(callback_bit(script_callback::DISABLE) == 0x2000);
        CHECK(static_cast<u8>(collision_kind::TRIGGER_EXIT) == 3);
        CHECK(static_cast<u8>(collision_kind::COLLISION_STAY) == 4);
        CHECK(CREATE_FLAG_INACTIVE == 4);
    }

    TEST_CASE("script_runtime - a warm collision dispatch allocates nothing") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.Bumper", {script_callback::COLLISION_ENTER})});
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        REQUIRE(fixture.runtime.add_script(a, hash_of("Game.Bumper"), {}).is_valid());
        REQUIRE(fixture.runtime.add_script(b, hash_of("Game.Bumper"), {}).is_valid());
        fixture.runtime.flush_pending();
        REQUIRE(fixture.runtime.stats().liveInstances == 2);

        // The recorder allocates per record, so it is off; a counting hook shows the records still arrive.
        u32 received = 0;
        fixture.null_host().set_recording(false);
        fixture.null_host().set_collision_hook([&received](const conjure_collision_record &) {
            ++received;
        });
        const std::array<collision_event, 1> events = {
          collision_between(a, b, collision_kind::COLLISION_ENTER, IMPULSE),
        };
        fixture.runtime.dispatch_collision_events(events);         // warm-up: the record buffer grows once
        REQUIRE(received == 2);
        CONJURE_CHECK_NO_ALLOCATIONS(fixture.runtime.dispatch_collision_events(events));
        CHECK(received == 4);
        fixture.null_host().set_collision_hook({});
    }

    TEST_CASE("script collisions - events reach only the overriding callbacks [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id full = fixture.add_script(a, COLLIDER);
        const script_entry_id enterOnly = fixture.add_script(b, ENTER_ONLY);

        const std::array<collision_event, 4> events = {
          collision_between(a, b, collision_kind::COLLISION_ENTER, IMPULSE),
          collision_between(a, b, collision_kind::COLLISION_EXIT, IMPULSE),
          collision_between(a, b, collision_kind::TRIGGER_ENTER, IMPULSE),
          collision_between(a, b, collision_kind::TRIGGER_EXIT, IMPULSE),
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

        const std::array<collision_event, 1> events = {
          collision_between(a, b, collision_kind::COLLISION_ENTER, IMPULSE),
        };
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

    TEST_CASE("script collisions - OnCollisionStay runs once per stay event [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        REQUIRE(fixture.runtime.any_class_has(script_callback::COLLISION_STAY));
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id full = fixture.add_script(a, COLLIDER);
        const script_entry_id enterOnly = fixture.add_script(b, ENTER_ONLY);

        const std::array<collision_event, 1> entered = {
          collision_between(a, b, collision_kind::COLLISION_ENTER, IMPULSE),
        };
        const std::array<collision_event, 1> stayed = {
          collision_between(a, b, collision_kind::COLLISION_STAY, IMPULSE),
        };
        fixture.runtime.dispatch_collision_events(entered);
        constexpr i64 STEPS = 3;
        for (i64 step = 0; step < STEPS; ++step) {
            fixture.runtime.dispatch_collision_events(stayed);
        }

        CHECK(fixture.field_i64(full, "Enters") == 1);
        CHECK(fixture.field_i64(full, "Stays") == STEPS);
        // The dispatch hands the event's impulse through; that a physics STAY carries 0 is the conversion's rule.
        CHECK(fixture.field_f64(full, "LastStayImpulse") == doctest::Approx(2.5));
        CHECK(fixture.fields_of(full).root()["LastOther"].as_u64() == b.to_u64());
        CHECK(fixture.field_i64(enterOnly, "Enters") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
