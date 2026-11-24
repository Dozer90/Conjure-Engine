#include "conjure/testing/doctest.h"

#include "scripting_test_support.h"

#include "conjure/core/json/writer.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Inactive scripts sleep (code sweep W3-CS-L4, owner option A), over the null host. The scripting module has no
// scene, so the fixture's test components stand in for the two activity tags: test_position plays scene.disabled
// (the self state) and test_velocity plays scene.inactive (what the scene puts on every descendant of a disabled
// entity). set_active calls and create flags are read back from the null host's recorder.

using namespace conjure;
using namespace conjure::scripting;
using namespace scripting_test;

namespace {

    constexpr std::string_view SLEEPER = "Game.Sleeper";
    constexpr std::string_view STAYER = "Game.Stayer";

    /// <summary>
    /// A scripting_fixture with the two stand-in activity tags set and Game.Sleeper loaded (update, collision and
    /// activity callbacks), in EDIT mode.
    /// </summary>
    struct activity_fixture : scripting_fixture {
        ecs::component_id disabledTag = {};
        ecs::component_id inactiveTag = {};

        activity_fixture() {
            disabledTag = registry.id_of<test_position>();
            inactiveTag = registry.id_of<test_velocity>();
            use_test_activity_tags(registry, runtime);
            load_fake_classes({make_class(
              SLEEPER,
              {script_callback::UPDATE,
               script_callback::COLLISION_ENTER,
               script_callback::ENABLE,
               script_callback::DISABLE,
               script_callback::DESTROY}
            )});
        }

        /// <summary>
        /// Adds a Sleeper to the entity and flushes, so in PLAY mode its instance exists.
        /// </summary>
        [[nodiscard]] script_entry_id add_sleeper(ecs::entity entity) {
            const script_entry_id id = runtime.add_script(entity, hash_of(SLEEPER), {});
            REQUIRE(id.is_valid());
            runtime.flush_pending();
            return id;
        }

        [[nodiscard]] i32 slot_of(script_entry_id id) const {
            const script_entry *entryValue = runtime.entry(id);
            REQUIRE(entryValue != nullptr);
            return entryValue->managedSlot;
        }

        [[nodiscard]] bool entry_has(script_entry_id id, script_entry_flags flag) const {
            const script_entry *entryValue = runtime.entry(id);
            REQUIRE(entryValue != nullptr);
            return entryValue->has_flag(flag);
        }

        void add_tag(ecs::entity entity, ecs::component_id tag) {
            CONJURE_REQUIRE_OK(world.add_raw(entity, tag));
        }

        void remove_tag(ecs::entity entity, ecs::component_id tag) {
            CONJURE_REQUIRE_OK(world.remove_raw(entity, tag));
        }

        [[nodiscard]] const null_host_recorder &recorder() {
            return null_host().recorder();
        }

        [[nodiscard]] u32 pending_changes() const noexcept {
            return runtime.stats().pendingActivityChanges;
        }

        /// <summary>
        /// The script_component JSON of the entity's set, as the scene serializer writes it.
        /// </summary>
        [[nodiscard]] std::string saved_json(ecs::entity entity) {
            const script_component *component = std::as_const(world).get<script_component>(entity);
            REQUIRE(component != nullptr);
            core::json::writer out = {};
            runtime.write_set_json(component->setHandle, out);
            return out.finish();
        }
    };

    /// <summary>
    /// Position of the first recorded call with this name (calls().size() when there is none).
    /// </summary>
    [[nodiscard]] usize first_call(const null_host_recorder &recorder, std::string_view call) {
        const std::vector<std::string> &calls = recorder.calls();
        return static_cast<usize>(std::find(calls.begin(), calls.end(), call) - calls.begin());
    }
}

TEST_SUITE("scripting/script_activity") {

    TEST_CASE("script_runtime - set_activity_tags keeps each valid id once") {
        activity_fixture fixture = {};
        const std::array<ecs::component_id, 4> tags = {
          fixture.disabledTag,
          ecs::component_id{},
          fixture.disabledTag,
          fixture.inactiveTag,
        };
        fixture.runtime.set_activity_tags(tags);
        REQUIRE(fixture.runtime.activity_tags().size() == 2);
        CHECK(fixture.runtime.activity_tags()[0] == fixture.disabledTag);
        CHECK(fixture.runtime.activity_tags()[1] == fixture.inactiveTag);

        fixture.runtime.set_activity_tags({});
        CHECK(fixture.runtime.activity_tags().empty());
    }

    TEST_CASE("script_runtime - is_entity_active reads the activity tags") {
        activity_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        CHECK(fixture.runtime.is_entity_active(entity));

        SUBCASE("the disabled tag") {
            fixture.add_tag(entity, fixture.disabledTag);
            CHECK_FALSE(fixture.runtime.is_entity_active(entity));
        }
        SUBCASE("the inactive tag") {
            fixture.add_tag(entity, fixture.inactiveTag);
            CHECK_FALSE(fixture.runtime.is_entity_active(entity));
        }
        SUBCASE("no tags set: a tagged entity is active") {
            fixture.add_tag(entity, fixture.disabledTag);
            fixture.runtime.set_activity_tags({});
            CHECK(fixture.runtime.is_entity_active(entity));
        }
        SUBCASE("a dead entity") {
            CONJURE_REQUIRE_OK(fixture.world.destroy_entity(entity));
            CHECK_FALSE(fixture.runtime.is_entity_active(entity));
        }
        SUBCASE("no attached world") {
            fixture.runtime.attach_world(nullptr);
            CHECK_FALSE(fixture.runtime.is_entity_active(entity));
            fixture.runtime.attach_world(&fixture.world);
        }
    }

    TEST_CASE("regression - code sweep W3-CS-L4 disabling a scripted entity tells its instance once") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_sleeper(entity);
        const i32 slot = fixture.slot_of(id);
        REQUIRE(fixture.null_host().is_slot_active(slot));
        fixture.null_host().reset_recorder();

        fixture.add_tag(entity, fixture.disabledTag);
        CHECK(fixture.pending_changes() == 1);
        fixture.runtime.flush_pending();
        CHECK(fixture.pending_changes() == 0);
        REQUIRE(fixture.recorder().activities().size() == 1);
        CHECK(fixture.recorder().activities()[0].slot == slot);
        CHECK_FALSE(fixture.recorder().activities()[0].active);
        CHECK(fixture.recorder().activities()[0].changed);
        CHECK_FALSE(fixture.null_host().is_slot_active(slot));
        CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));

        // A steady frame costs nothing.
        fixture.runtime.run_phase(script_phase::UPDATE, 0.016f);
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 1);

        fixture.remove_tag(entity, fixture.disabledTag);
        fixture.runtime.flush_pending();
        REQUIRE(fixture.recorder().activities().size() == 2);
        CHECK(fixture.recorder().activities()[1].slot == slot);
        CHECK(fixture.recorder().activities()[1].active);
        CHECK(fixture.null_host().is_slot_active(slot));
        CHECK_FALSE(fixture.entry_has(id, script_entry_flags::INACTIVE));
        CHECK(fixture.recorder().count("set_active") == 2);
    }

    TEST_CASE("script_runtime - an entity stays inactive while either activity tag remains") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_sleeper(entity);
        fixture.add_tag(entity, fixture.disabledTag);
        fixture.add_tag(entity, fixture.inactiveTag);
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 1);

        fixture.remove_tag(entity, fixture.disabledTag);
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 1);
        CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));

        fixture.remove_tag(entity, fixture.inactiveTag);
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 2);
        CHECK(fixture.null_host().is_slot_active(fixture.slot_of(id)));
    }

    TEST_CASE("script_runtime - the inactive tag of a disabled ancestor reaches the child's scripts") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity parent = fixture.create_entity();
        const ecs::entity child = fixture.create_entity();
        const script_entry_id childScript = fixture.add_sleeper(child);
        fixture.null_host().reset_recorder();

        // What scene::set_enabled(parent, false) does: the self tag on the parent, the inactive tag below it.
        fixture.add_tag(parent, fixture.disabledTag);
        fixture.add_tag(child, fixture.inactiveTag);
        CHECK(fixture.pending_changes() == 1);          // the parent carries no script_component: not recorded
        fixture.runtime.flush_pending();
        REQUIRE(fixture.recorder().activities().size() == 1);
        CHECK(fixture.recorder().activities()[0].slot == fixture.slot_of(childScript));
        CHECK_FALSE(fixture.recorder().activities()[0].active);
    }

    TEST_CASE("script_runtime - an instance of an inactive entity is created with CREATE_FLAG_INACTIVE") {
        activity_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();

        SUBCASE("created by set_mode(PLAY)") {
            fixture.add_tag(entity, fixture.disabledTag);
            const script_entry_id id = fixture.runtime.add_script(entity, hash_of(SLEEPER), {});
            REQUIRE(id.is_valid());
            fixture.runtime.set_mode(script_mode::PLAY);
            REQUIRE(fixture.recorder().creates().size() == 1);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) != 0);
            CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));
            CHECK_FALSE(fixture.null_host().is_slot_active(fixture.slot_of(id)));
            CHECK(fixture.recorder().count("set_active") == 0);
        }
        SUBCASE("created in PLAY mode under a disabled ancestor") {
            fixture.runtime.set_mode(script_mode::PLAY);
            fixture.add_tag(entity, fixture.inactiveTag);
            const script_entry_id id = fixture.add_sleeper(entity);
            REQUIRE(fixture.recorder().creates().size() == 1);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) != 0);
            CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));
            CHECK(fixture.recorder().count("set_active") == 0);
        }
        SUBCASE("an active entity sends no flag") {
            fixture.runtime.set_mode(script_mode::PLAY);
            const script_entry_id id = fixture.add_sleeper(entity);
            REQUIRE(fixture.recorder().creates().size() == 1);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) == 0);
            CHECK_FALSE(fixture.entry_has(id, script_entry_flags::INACTIVE));
        }
    }

    TEST_CASE("script_runtime - a disable between frames takes effect before the next run_phase call") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.add_sleeper(entity));
        fixture.null_host().reset_recorder();

        // A native system outside any script phase; no flush runs between this change and the run_phase call below.
        fixture.add_tag(entity, fixture.disabledTag);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.016f);
        const usize setActive = first_call(fixture.recorder(), "set_active");
        const usize runPhase = first_call(fixture.recorder(), "run_phase");
        REQUIRE(setActive < fixture.recorder().calls().size());
        REQUIRE(runPhase < fixture.recorder().calls().size());
        CHECK(setActive < runPhase);
    }

    TEST_CASE(
      "regression - code sweep W3-CS-L4 a disable before the collision dispatch takes effect first and gets no records"
    ) {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id first = fixture.add_sleeper(a);
        const script_entry_id second = fixture.add_sleeper(b);
        fixture.runtime.run_phase(script_phase::FIXED_UPDATE, 0.02f);
        fixture.null_host().reset_recorder();

        // A native system disables a after the last phase; the next fixed step dispatches a contact of a and b.
        fixture.add_tag(a, fixture.disabledTag);
        const std::array<collision_event, 1> events = {collision_between(a, b, collision_kind::COLLISION_ENTER)};
        fixture.runtime.dispatch_collision_events(events);

        const usize setActive = first_call(fixture.recorder(), "set_active");
        const usize dispatched = first_call(fixture.recorder(), "dispatch_collisions");
        REQUIRE(setActive < fixture.recorder().calls().size());
        REQUIRE(dispatched < fixture.recorder().calls().size());
        CHECK(setActive < dispatched);
        CHECK_FALSE(fixture.null_host().is_slot_active(fixture.slot_of(first)));
        REQUIRE(fixture.recorder().collisions().size() == 1);
        CHECK(fixture.recorder().collisions()[0].slot == fixture.slot_of(second));
        CHECK(fixture.recorder().collisions()[0].other == a.to_u64());

        // Both inactive: no record at all, so no managed call.
        fixture.add_tag(b, fixture.inactiveTag);
        fixture.null_host().reset_recorder();
        fixture.runtime.dispatch_collision_events(events);
        CHECK(fixture.recorder().count("set_active") == 1);
        CHECK(fixture.recorder().count("dispatch_collisions") == 0);
        CHECK(fixture.runtime.current_phase() == script_phase::COUNT);
    }

    TEST_CASE("script_runtime - a collision dispatch without events still takes a recorded change") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_sleeper(entity);
        fixture.add_tag(entity, fixture.disabledTag);
        fixture.runtime.dispatch_collision_events({});
        CHECK(fixture.recorder().count("set_active") == 1);
        CHECK(fixture.recorder().count("dispatch_collisions") == 0);
        CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));
        CHECK(fixture.runtime.current_phase() == script_phase::COUNT);
    }

    TEST_CASE("script_runtime - a hot reload makes no set_active call") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity awake = fixture.create_entity();
        const ecs::entity asleep = fixture.create_entity();
        fixture.add_tag(asleep, fixture.disabledTag);
        const script_entry_id awakeScript = fixture.add_sleeper(awake);
        const script_entry_id asleepScript = fixture.add_sleeper(asleep);
        fixture.null_host().reset_recorder();

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 0);
        REQUIRE(fixture.recorder().creates().size() == 2);
        for (const null_host_recorder::create_record &record : fixture.recorder().creates()) {
            CAPTURE(record.entity);
            CHECK((record.flags & CREATE_FLAG_ALREADY_CREATED) != 0);
            const bool inactive = (record.flags & CREATE_FLAG_INACTIVE) != 0;
            CHECK(inactive == (record.entity == asleep.to_u64()));
        }
        CHECK_FALSE(fixture.entry_has(awakeScript, script_entry_flags::INACTIVE));
        CHECK(fixture.entry_has(asleepScript, script_entry_flags::INACTIVE));
        CHECK(fixture.null_host().is_slot_active(fixture.slot_of(awakeScript)));
        CHECK_FALSE(fixture.null_host().is_slot_active(fixture.slot_of(asleepScript)));
    }

    TEST_CASE("regression - code sweep W3-CS-L4 an activity change pending at a hot reload takes effect before it") {
        // The reload recreates the instance from its entity's tags: a change still pending would find the new
        // instance already in that state, and the script would stop or start updating without OnDisable or OnEnable.
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();

        SUBCASE("a disable reaches the old instance as OnDisable") {
            const script_entry_id id = fixture.add_sleeper(entity);
            const i32 slot = fixture.slot_of(id);
            fixture.null_host().reset_recorder();
            fixture.add_tag(entity, fixture.disabledTag);
            REQUIRE(fixture.pending_changes() == 1);

            CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
            CHECK(fixture.pending_changes() == 0);
            REQUIRE(fixture.recorder().activities().size() == 1);
            CHECK(fixture.recorder().activities()[0].slot == slot);
            CHECK_FALSE(fixture.recorder().activities()[0].active);
            CHECK(fixture.recorder().activities()[0].changed);
            CHECK(first_call(fixture.recorder(), "set_active") < first_call(fixture.recorder(), "create_instances"));
            REQUIRE(fixture.recorder().creates().size() == 1);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_ALREADY_CREATED) != 0);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) != 0);
            CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));
        }
        SUBCASE("an enable reaches the old instance as OnEnable") {
            fixture.add_tag(entity, fixture.disabledTag);
            const script_entry_id id = fixture.add_sleeper(entity);
            const i32 slot = fixture.slot_of(id);
            REQUIRE_FALSE(fixture.null_host().is_slot_active(slot));
            fixture.null_host().reset_recorder();
            fixture.remove_tag(entity, fixture.disabledTag);

            CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
            REQUIRE(fixture.recorder().activities().size() == 1);
            CHECK(fixture.recorder().activities()[0].slot == slot);
            CHECK(fixture.recorder().activities()[0].active);
            CHECK(first_call(fixture.recorder(), "set_active") < first_call(fixture.recorder(), "create_instances"));
            REQUIRE(fixture.recorder().creates().size() == 1);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) == 0);
            CHECK_FALSE(fixture.entry_has(id, script_entry_flags::INACTIVE));
        }
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 1);
    }

    TEST_CASE("regression - code sweep W3-CS-L4 an activity change pending at a snapshot restore takes effect first") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const i32 slot = fixture.slot_of(fixture.add_sleeper(entity));
        const core::json::document snapshot = json(fixture.saved_json(entity));
        fixture.null_host().reset_recorder();
        fixture.add_tag(entity, fixture.disabledTag);
        REQUIRE(fixture.pending_changes() == 1);

        const script_component *component = std::as_const(fixture.world).get<script_component>(entity);
        REQUIRE(component != nullptr);
        CONJURE_REQUIRE_OK(fixture.runtime.read_set_json(component->setHandle, snapshot.root()));
        CHECK(fixture.pending_changes() == 0);
        REQUIRE(fixture.recorder().activities().size() == 1);
        CHECK(fixture.recorder().activities()[0].slot == slot);
        CHECK_FALSE(fixture.recorder().activities()[0].active);
        // The instance is then recreated without callbacks, in the state OnDisable left it.
        REQUIRE(fixture.recorder().destroys().size() == 1);
        CHECK_FALSE(fixture.recorder().destroys()[0].callOnDestroy);
        fixture.runtime.flush_pending();
        CHECK(first_call(fixture.recorder(), "set_active") < first_call(fixture.recorder(), "create_instances"));
        REQUIRE(fixture.recorder().creates().size() == 1);
        CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_ALREADY_CREATED) != 0);
        CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) != 0);
        CHECK(fixture.recorder().count("set_active") == 1);
    }

    TEST_CASE("script_runtime - an inactive entry is not marked started until its first activation") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        fixture.add_tag(entity, fixture.disabledTag);
        const script_entry_id id = fixture.add_sleeper(entity);

        fixture.runtime.run_phase(script_phase::FIXED_UPDATE, 0.02f);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.016f);
        CHECK_FALSE(fixture.entry_has(id, script_entry_flags::STARTED));

        fixture.remove_tag(entity, fixture.disabledTag);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.016f);
        CHECK(fixture.entry_has(id, script_entry_flags::STARTED));
    }

    TEST_CASE("script_runtime - the saved script JSON of a disabled entity is unchanged") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.add_sleeper(entity));
        const std::string expected = "{\"scripts\":[{\"class\":\"Game.Sleeper\",\"hash\":"
          + std::to_string(hash_of(SLEEPER)) + ",\"created\":true,\"fields\":{}}]}";
        CHECK(fixture.saved_json(entity) == expected);

        fixture.add_tag(entity, fixture.disabledTag);
        fixture.runtime.flush_pending();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.016f);
        CHECK(fixture.saved_json(entity) == expected);
    }

    TEST_CASE("script_runtime - EDIT mode records no activity change") {
        activity_fixture fixture = {};
        constexpr usize ENTITY_COUNT = 1000;
        std::vector<ecs::entity> entities = {};
        entities.reserve(ENTITY_COUNT);
        for (usize index = 0; index < ENTITY_COUNT; ++index) {
            CAPTURE(index);
            const ecs::entity entity = fixture.create_entity();
            REQUIRE(fixture.runtime.add_script(entity, hash_of(SLEEPER), {}).is_valid());
            fixture.add_tag(entity, fixture.disabledTag);
            entities.push_back(entity);
        }
        CHECK(fixture.pending_changes() == 0);

        // Play creates every instance from the tags its entity has now: no activity call is needed.
        fixture.runtime.set_mode(script_mode::PLAY);
        CHECK(fixture.recorder().count("set_active") == 0);
        CHECK(fixture.recorder().creates().size() == ENTITY_COUNT);
        usize inactiveCreates = 0;
        for (const null_host_recorder::create_record &record : fixture.recorder().creates()) {
            if ((record.flags & CREATE_FLAG_INACTIVE) != 0) {
                ++inactiveCreates;
            }
        }
        CHECK(inactiveCreates == ENTITY_COUNT);
    }

    TEST_CASE("script_runtime - set_mode and attach_world forget the recorded activity changes") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_sleeper(entity);
        fixture.add_tag(entity, fixture.disabledTag);
        REQUIRE(fixture.pending_changes() == 1);

        SUBCASE("set_mode(EDIT), then PLAY again") {
            fixture.runtime.set_mode(script_mode::EDIT);
            CHECK(fixture.pending_changes() == 0);
            fixture.null_host().reset_recorder();
            fixture.runtime.set_mode(script_mode::PLAY);
            CHECK(fixture.pending_changes() == 0);
            CHECK(fixture.recorder().count("set_active") == 0);
            REQUIRE(fixture.recorder().creates().size() == 1);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) != 0);
        }
        SUBCASE("detached and attached again: the live instances are checked again") {
            fixture.runtime.attach_world(nullptr);
            CHECK(fixture.pending_changes() == 0);
            fixture.runtime.attach_world(&fixture.world);
            CHECK(fixture.pending_changes() == 1);
            fixture.runtime.flush_pending();
            CHECK(fixture.recorder().count("set_active") == 1);
            CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));
        }
    }

    TEST_CASE("script_runtime - any_class_has follows the loaded classes") {
        {
            scripting_fixture bare = {};
            CHECK_FALSE(bare.runtime.any_class_has(script_callback::UPDATE));
        }
        activity_fixture fixture = {};
        CHECK(fixture.runtime.any_class_has(script_callback::UPDATE));
        CHECK(fixture.runtime.any_class_has(script_callback::ENABLE));
        CHECK_FALSE(fixture.runtime.any_class_has(script_callback::COLLISION_STAY));
        CHECK_FALSE(fixture.runtime.any_class_has(script_callback::COUNT));

        fixture.null_host().add_fake_class(make_class(STAYER, {script_callback::COLLISION_STAY}));
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        CHECK(fixture.runtime.any_class_has(script_callback::COLLISION_STAY));

        REQUIRE(fixture.null_host().remove_fake_class(hash_of(STAYER)));
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        CHECK_FALSE(fixture.runtime.any_class_has(script_callback::COLLISION_STAY));

        fixture.runtime.shutdown();
        CHECK_FALSE(fixture.runtime.any_class_has(script_callback::UPDATE));
    }

    TEST_CASE("script_runtime - a change undone before it takes effect calls nothing") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_sleeper(entity);
        fixture.add_tag(entity, fixture.disabledTag);
        fixture.remove_tag(entity, fixture.disabledTag);
        CHECK(fixture.pending_changes() == 2);
        fixture.runtime.flush_pending();
        CHECK(fixture.pending_changes() == 0);
        CHECK(fixture.recorder().count("set_active") == 0);
        CHECK_FALSE(fixture.entry_has(id, script_entry_flags::INACTIVE));
    }

    TEST_CASE("script_runtime - a destroy in the phase that disabled the entity leaves OnDisable to the destroy") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const i32 slot = fixture.slot_of(fixture.add_sleeper(entity));
        fixture.null_host().reset_recorder();

        fixture.add_tag(entity, fixture.disabledTag);
        fixture.runtime.request_entity_destroy(entity);
        fixture.runtime.flush_pending();
        CHECK_FALSE(fixture.world.is_alive(entity));
        CHECK(fixture.recorder().count("set_active") == 0);
        REQUIRE(fixture.recorder().destroys().size() == 1);
        CHECK(fixture.recorder().destroys()[0].slot == slot);
        CHECK(fixture.recorder().destroys()[0].callOnDestroy);
    }

    TEST_CASE("script_runtime - changing the activity tags in PLAY checks the live instances again") {
        activity_fixture fixture = {};
        fixture.runtime.set_activity_tags({});
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_sleeper(entity);
        fixture.add_tag(entity, fixture.disabledTag);
        CHECK(fixture.pending_changes() == 0);          // no tags, no observers

        const std::array<ecs::component_id, 1> tags = {fixture.disabledTag};
        fixture.runtime.set_activity_tags(tags);
        CHECK(fixture.pending_changes() == 1);
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 1);
        CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));
    }

    TEST_CASE("script_runtime - a change made by a callback takes effect in the same flush") {
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();

        SUBCASE("OnCreate disables its own entity") {
            fixture.null_host().set_create_hook([&fixture, entity](const conjure_create_request &, i32) {
                fixture.add_tag(entity, fixture.disabledTag);
            });
            const script_entry_id id = fixture.add_sleeper(entity);
            fixture.null_host().set_create_hook({});
            REQUIRE(fixture.recorder().creates().size() == 1);
            CHECK((fixture.recorder().creates()[0].flags & CREATE_FLAG_INACTIVE) == 0);
            REQUIRE(fixture.recorder().activities().size() == 1);
            CHECK_FALSE(fixture.recorder().activities()[0].active);
            CHECK(fixture.entry_has(id, script_entry_flags::INACTIVE));
        }
        SUBCASE("OnDisable disables another entity") {
            const ecs::entity other = fixture.create_entity();
            const script_entry_id first = fixture.add_sleeper(entity);
            const script_entry_id second = fixture.add_sleeper(other);
            const i32 firstSlot = fixture.slot_of(first);
            fixture.null_host().set_activity_hook([&fixture, other, firstSlot](i32 slot, bool active) {
                if (slot == firstSlot && !active) {
                    fixture.add_tag(other, fixture.disabledTag);
                }
            });
            fixture.add_tag(entity, fixture.disabledTag);
            fixture.runtime.flush_pending();
            fixture.null_host().set_activity_hook({});
            CHECK(fixture.pending_changes() == 0);
            REQUIRE(fixture.recorder().activities().size() == 2);
            CHECK(fixture.recorder().activities()[0].slot == firstSlot);
            CHECK(fixture.recorder().activities()[1].slot == fixture.slot_of(second));
            CHECK(fixture.entry_has(second, script_entry_flags::INACTIVE));
        }
    }

    TEST_CASE("regression - code sweep W3-CS-L4 a snapshot restore gives a script it brings back no OnDisable") {
        // The ordering table: a snapshot restore is OnReload only, like a hot reload. A live instance the JSON brings
        // back as created is destroyed without callbacks; one the JSON does not bring back is really removed.
        activity_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_sleeper(entity);
        const i32 slot = fixture.slot_of(id);
        const std::string created = "{\"class\":\"Game.Sleeper\",\"created\":true,\"fields\":{}}";
        const std::string fresh = "{\"class\":\"Game.Sleeper\",\"fields\":{}}";
        fixture.null_host().reset_recorder();

        SUBCASE("a snapshot restore through the serializer hook") {
            const core::json::document snapshot = json(fixture.saved_json(entity));
            const script_component *component = std::as_const(fixture.world).get<script_component>(entity);
            REQUIRE(component != nullptr);
            CONJURE_REQUIRE_OK(fixture.runtime.read_set_json(component->setHandle, snapshot.root()));
            REQUIRE(fixture.recorder().destroys().size() == 1);
            CHECK(fixture.recorder().destroys()[0].slot == slot);
            CHECK_FALSE(fixture.recorder().destroys()[0].callOnDestroy);
        }
        SUBCASE("an entry without \"created\" replaces the script") {
            CONJURE_REQUIRE_OK(
              fixture.runtime.deserialize_entity_scripts(entity, json("{\"scripts\":[" + fresh + "]}").root())
            );
            REQUIRE(fixture.recorder().destroys().size() == 1);
            CHECK(fixture.recorder().destroys()[0].callOnDestroy);
        }
        SUBCASE("two live scripts of the class, one brought back") {
            const i32 secondSlot = fixture.slot_of(fixture.add_sleeper(entity));
            fixture.null_host().reset_recorder();
            CONJURE_REQUIRE_OK(
              fixture.runtime.deserialize_entity_scripts(entity, json("{\"scripts\":[" + created + "]}").root())
            );
            REQUIRE(fixture.recorder().destroys().size() == 2);
            CHECK(fixture.recorder().destroys()[0].slot == slot);
            CHECK_FALSE(fixture.recorder().destroys()[0].callOnDestroy);
            CHECK(fixture.recorder().destroys()[1].slot == secondSlot);
            CHECK(fixture.recorder().destroys()[1].callOnDestroy);
        }
        fixture.runtime.flush_pending();
        CHECK(fixture.recorder().count("set_active") == 0);
        CHECK(fixture.runtime.stats().liveInstances == 1);
    }
}
