#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/test_environment.h"
#include "conjure/testing/world_builder.h"
#include "integration_engine_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_time.h"
#include "conjure/scene/scene.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
#endif

#include <filesystem>
#include <utility>
#include <vector>

// docs/design/08_testing.md 6.1 test_script_lifecycle [scripting]: a LifecycleProbe entity in a headless
// application over the real CLR runs ten frames and is destroyed: OnCreate ran once, OnUpdate ten times (once
// per frame, after OnStart), OnDestroy ran exactly once and the destroy removes the script entry. The probe
// reports through its public fields, read back with collect_entry_fields; OnDestroy reports through static
// counters a second, long-lived LifecycleWitness entity copies into its own fields every update
// (Conjure.TestScripts/LifecycleProbe.cs). Both destroy paths of 03_scripting.md 3.4 are driven: Entity.Destroy()
// from a script (script_runtime::request_entity_destroy; OnDestroy sees the entity alive) and a native
// scene::destroy (OnDestroy runs at the next flush with the entity already gone). With physics, a Collider on a box
// resting on static ground gets OnCollisionStay once per fixed step while the box is awake, and OnCollisionExit
// when it falls asleep (code sweep W3-CS-L4, F408).

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr std::string_view LIFECYCLE_PROBE = "Conjure.TestScripts.LifecycleProbe";
    constexpr std::string_view LIFECYCLE_WITNESS = "Conjure.TestScripts.LifecycleWitness";
    constexpr u32 FRAMES = 10;

    [[nodiscard]] i64 field_i64(
      scripting::script_runtime &runtime,
      scripting::script_entry_id id,
      std::string_view name
    ) {
        core::result<core::json::document> fields = runtime.collect_entry_fields(id);
        CONJURE_REQUIRE_OK(fields);
        return fields.value().root()[name].as_i64(-1);
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    constexpr std::string_view COLLIDER = "Conjure.TestScripts.Collider";

    [[nodiscard]] f64 field_f64(
      scripting::script_runtime &runtime,
      scripting::script_entry_id id,
      std::string_view name
    ) {
        core::result<core::json::document> fields = runtime.collect_entry_fields(id);
        CONJURE_REQUIRE_OK(fields);
        return fields.value().root()[name].as_f64(-1.0);
    }
#endif

    [[nodiscard]] scripting::script_entry_id only_script(scripting::script_runtime &runtime, ecs::entity entity) {
        const std::vector<scripting::script_entry_id> scripts = runtime.scripts_of(entity);
        REQUIRE(scripts.size() == 1);
        return scripts.front();
    }
}

TEST_SUITE("integration/script_lifecycle") {

    TEST_CASE("script_lifecycle - a LifecycleProbe runs ten frames and is destroyed with its entity [scripting]"
      * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        const std::filesystem::path assembly = integration_test::test_scripts_assembly("v1");
        REQUIRE(std::filesystem::is_regular_file(assembly));

        headless_engine_options options = integration_test::quiet_options();
        options.enableScripting = true;
        options.useNullScriptHost = false;
        options.gameAssembly = &assembly;
        headless_engine engine(options);
        scripting::script_runtime *runtime = engine.context().scripting;
        REQUIRE(runtime != nullptr);
        REQUIRE(runtime->is_assembly_loaded());
        scene::scene &draft = engine.active_scene();

        const core::result<ecs::entity> witnessEntity = spawn_script_entity(draft, "Witness", LIFECYCLE_WITNESS);
        CONJURE_REQUIRE_OK(witnessEntity);
        const scripting::script_entry_id witness = only_script(*runtime, witnessEntity.value());
        const core::result<ecs::entity> probe = spawn_script_entity(draft, "Probe", LIFECYCLE_PROBE);
        CONJURE_REQUIRE_OK(probe);
        const scripting::script_entry_id id = only_script(*runtime, probe.value());

        engine.step_frames(FRAMES);
        CHECK(field_i64(*runtime, id, "Created") == 1);
        CHECK(field_i64(*runtime, id, "Started") == 1);
        CHECK(field_i64(*runtime, id, "Updates") == FRAMES);
        CHECK(field_i64(*runtime, id, "FixedUpdates") == FRAMES);
        CHECK(field_i64(*runtime, id, "LateUpdates") == FRAMES);
        CHECK(field_i64(*runtime, id, "StartedBeforeUpdate") == 1);
        CHECK(field_i64(*runtime, id, "UpdateBeforeLate") == 1);
        REQUIRE(runtime->entry(id) != nullptr);
        CHECK(runtime->entry(id)->is_live());
        CHECK(field_i64(*runtime, witness, "ProbeDestroyed") == 0);

        // Entity.Destroy() from a script: the entity goes at the next flush, after OnDestroy saw it alive.
        runtime->request_entity_destroy(probe.value());
        engine.step_frames(1);
        CHECK_FALSE(engine.world().is_alive(probe.value()));
        CHECK(runtime->entry(id) == nullptr);
        CHECK(runtime->scripts_of(probe.value()).empty());
        engine.step_frames(1);
        CHECK(field_i64(*runtime, witness, "ProbeDestroyed") == 1);
        CHECK(field_i64(*runtime, witness, "ProbeDestroyedAlive") == 1);

        // A native destroy of a scripted entity: OnDestroy runs once more, with the entity already dead.
        const core::result<ecs::entity> second = spawn_script_entity(draft, "Probe", LIFECYCLE_PROBE);
        CONJURE_REQUIRE_OK(second);
        const scripting::script_entry_id secondId = only_script(*runtime, second.value());
        engine.step_frames(1);
        CHECK(field_i64(*runtime, secondId, "Created") == 1);
        draft.destroy(second.value());
        engine.step_frames(1);
        CHECK_FALSE(engine.world().is_alive(second.value()));
        CHECK(runtime->entry(secondId) == nullptr);
        engine.step_frames(1);
        CHECK(field_i64(*runtime, witness, "ProbeDestroyed") == 2);
        CHECK(field_i64(*runtime, witness, "ProbeDestroyedAlive") == 1);

        CHECK(engine.world().is_alive(witnessEntity.value()));
        CHECK(runtime->stats().liveInstances == 1);
        CHECK(engine.logs().count(core::log_level::ERR) == 0);
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("script_lifecycle - OnCollisionStay counts every fixed step and stops at sleep [scripting]"
      * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        const std::filesystem::path assembly = integration_test::test_scripts_assembly("v1");
        REQUIRE(std::filesystem::is_regular_file(assembly));

        headless_engine_options options = integration_test::physics_options();
        options.enableScripting = true;
        options.useNullScriptHost = false;
        options.gameAssembly = &assembly;
        headless_engine engine(options);
        scripting::script_runtime *runtime = engine.context().scripting;
        REQUIRE(runtime != nullptr);
        REQUIRE(runtime->is_assembly_loaded());
        scene::scene &draft = engine.active_scene();

        // A unit box resting on a static unit box: in contact from the first step, with nothing to bounce.
        CONJURE_REQUIRE_OK(spawn_box_body(draft, math::vec3(0.0f, -0.5f, 0.0f), physics::motion_type::STATIC));
        const core::result<ecs::entity> box =
          spawn_box_body(draft, math::vec3(0.0f, 0.5f, 0.0f), physics::motion_type::DYNAMIC);
        CONJURE_REQUIRE_OK(box);
        const physics::rigid_body *defaults = std::as_const(engine.world()).get<physics::rigid_body>(box.value());
        REQUIRE(defaults != nullptr);
        REQUIRE(defaults->flags == physics::rigid_body_flags::ALLOW_SLEEPING);     // what the second subcase uses
        const core::json::document fields = core::json::document::object();
        const scripting::script_entry_id id =
          runtime->add_script(box.value(), scripting::script_name_hash(COLLIDER), fields.root());
        REQUIRE(id.is_valid());
        const auto fixed_steps = [&engine]() {
            return engine.context().time->fixedStepIndex;
        };

        SUBCASE("kept awake: one Stay per fixed step after the Enter") {
            // Before the first frame creates the Jolt body (the script moved the entity: the component is read again).
            physics::rigid_body *body = engine.world().get<physics::rigid_body>(box.value());
            REQUIRE(body != nullptr);
            body->flags = physics::rigid_body_flags::NONE;
            engine.step_frames(2);
            REQUIRE(field_i64(*runtime, id, "Enters") == 1);
            const i64 staysBefore = field_i64(*runtime, id, "Stays");
            const u64 stepsBefore = fixed_steps();
            constexpr u32 STEPS = 30;
            engine.step_frames(STEPS);
            const u64 steps = fixed_steps() - stepsBefore;
            REQUIRE(steps >= STEPS);
            CHECK(field_i64(*runtime, id, "Stays") - staysBefore == static_cast<i64>(steps));
            CHECK(field_i64(*runtime, id, "Enters") == 1);
            CHECK(field_i64(*runtime, id, "Exits") == 0);
            CHECK(field_f64(*runtime, id, "LastStayImpulse") == 0.0);
        }
        SUBCASE("allowed to sleep: an Exit when the box sleeps and no Stay after it") {
            // timeBeforeSleepSeconds is 0.5 s; one and a half seconds leaves a margin.
            constexpr u32 SETTLE_FRAMES = 90;
            engine.step_frames(SETTLE_FRAMES);
            CHECK(field_i64(*runtime, id, "Enters") == 1);
            REQUIRE(field_i64(*runtime, id, "Exits") == 1);
            const i64 stays = field_i64(*runtime, id, "Stays");
            CHECK(stays > 0);
            engine.step_frames(30);
            CHECK(field_i64(*runtime, id, "Stays") == stays);
            CHECK(field_i64(*runtime, id, "Exits") == 1);
            CHECK(field_i64(*runtime, id, "Enters") == 1);
        }
        CHECK(engine.logs().count(core::log_level::ERR) == 0);
    }
#endif
}
