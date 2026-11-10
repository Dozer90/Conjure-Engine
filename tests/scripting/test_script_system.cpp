#include "conjure/testing/doctest.h"

#include "scripting_test_support.h"

#include "conjure/core/string_id.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/scripting/hot_reload.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/script_system.h"

#include <array>
#include <span>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    [[nodiscard]] const ecs::system_desc *find_system(ecs::scheduler &scheduler, std::string_view name) {
        for (ecs::system_phase phase = ecs::system_phase::FRAME_BEGIN; phase != ecs::system_phase::COUNT;
          phase = static_cast<ecs::system_phase>(static_cast<u8>(phase) + 1)) {
            for (const ecs::stage_info &stage : scheduler.stages(phase)) {
                for (const ecs::system_id id : stage.systems) {
                    const ecs::system_desc *desc = scheduler.system(id);
                    if (desc != nullptr && desc->name == core::string_id(name)) {
                        return desc;
                    }
                }
            }
        }
        return nullptr;
    }
}

TEST_SUITE("scripting/script_system") {

    TEST_CASE("register_script_systems - registers the six exclusive systems in their phases") {
        scripting_fixture fixture = {};
        ecs::scheduler scheduler(fixture.world, nullptr);
        CONJURE_REQUIRE_OK(register_script_systems(scheduler, fixture.runtime));
        CONJURE_REQUIRE_OK(scheduler.finalise());
        CHECK(scheduler.system_count() == 6);

        const std::array<std::pair<std::string_view, ecs::system_phase>, 6> expected = {{
          {SYSTEM_FRAME_BEGIN, ecs::system_phase::FRAME_BEGIN},
          {SYSTEM_COLLISION_DISPATCH, ecs::system_phase::FIXED_UPDATE},
          {SYSTEM_FIXED_UPDATE, ecs::system_phase::FIXED_UPDATE},
          {SYSTEM_UPDATE, ecs::system_phase::UPDATE},
          {SYSTEM_LATE_UPDATE, ecs::system_phase::LATE_UPDATE},
          {SYSTEM_FRAME_END, ecs::system_phase::FRAME_END},
        }};
        for (const auto &[name, phase] : expected) {
            CAPTURE(name);
            const ecs::system_desc *desc = find_system(scheduler, name);
            REQUIRE(desc != nullptr);
            CHECK(desc->phase == phase);
            CHECK(has_flag(desc->flags, ecs::system_flags::EXCLUSIVE));
        }

        // Collision dispatch runs in its own stage before fixed_update.
        const std::span<const ecs::stage_info> fixedStages = scheduler.stages(ecs::system_phase::FIXED_UPDATE);
        REQUIRE(fixedStages.size() == 2);
        CHECK(fixedStages[0].exclusive);
        CHECK(scheduler.system(fixedStages[0].systems[0])->name == core::string_id(SYSTEM_COLLISION_DISPATCH));
        CHECK(scheduler.system(fixedStages[1].systems[0])->name == core::string_id(SYSTEM_FIXED_UPDATE));
    }

    TEST_CASE("script systems - running the phases drives the runtime in frame order") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.Ticker", {script_callback::UPDATE})});
        fixture.runtime.set_mode(script_mode::PLAY);
        static_cast<void>(fixture.runtime.add_script(fixture.create_entity(), hash_of("Game.Ticker"), {}));
        ecs::scheduler scheduler(fixture.world, nullptr);
        CONJURE_REQUIRE_OK(register_script_systems(scheduler, fixture.runtime));
        CONJURE_REQUIRE_OK(scheduler.finalise());
        fixture.null_host().reset_recorder();

        ecs::frame_info frame = {};
        frame.deltaTime = 0.25f;
        frame.fixedDeltaTime = 0.125f;
        frame.time = 3.0;
        scheduler.run_phase(ecs::system_phase::FRAME_BEGIN, frame);
        CHECK(fixture.null_host().live_slot_count() == 1);     // frame_begin flushed the pending create
        scheduler.run_phase(ecs::system_phase::FIXED_UPDATE, frame);
        scheduler.run_phase(ecs::system_phase::UPDATE, frame);
        scheduler.run_phase(ecs::system_phase::LATE_UPDATE, frame);
        scheduler.run_phase(ecs::system_phase::FRAME_END, frame);

        const auto &phases = fixture.null_host().recorder().phases();
        REQUIRE(phases.size() == 3);
        CHECK(phases[0].phase == script_phase::FIXED_UPDATE);
        CHECK(phases[0].deltaTime == 0.125f);
        CHECK(phases[1].phase == script_phase::UPDATE);
        CHECK(phases[1].deltaTime == 0.25f);
        CHECK(phases[2].phase == script_phase::LATE_UPDATE);
        CHECK(phases[2].deltaTime == 0.25f);
        CHECK(fixture.null_host().recorder().count("dispatch_collisions") == 0);
    }

    TEST_CASE("script systems - collision_dispatch forwards the bound source before fixed_update") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.Hit", {script_callback::COLLISION_ENTER})});
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(a, hash_of("Game.Hit"), {}));
        fixture.runtime.flush_pending();

        std::vector<collision_event> events(1);
        events[0].a = a;
        events[0].b = b;
        events[0].kind = collision_kind::COLLISION_ENTER;
        u32 sourceCalls = 0;
        fixture.runtime.set_collision_source([&events, &sourceCalls]() -> std::span<const collision_event> {
            ++sourceCalls;
            return events;
        });
        CHECK(static_cast<bool>(fixture.runtime.collision_source()));

        ecs::scheduler scheduler(fixture.world, nullptr);
        CONJURE_REQUIRE_OK(register_script_systems(scheduler, fixture.runtime));
        fixture.null_host().reset_recorder();
        scheduler.run_phase(ecs::system_phase::FIXED_UPDATE, {});
        CHECK(sourceCalls == 1);
        const std::vector<std::string> &calls = fixture.null_host().recorder().calls();
        REQUIRE(calls.size() == 2);
        CHECK(calls[0] == "dispatch_collisions");
        CHECK(calls[1] == "run_phase");
        REQUIRE(fixture.null_host().recorder().collisions().size() == 1);
        CHECK(fixture.null_host().recorder().collisions()[0].other == b.to_u64());
    }

    TEST_CASE("script systems - frame_begin ticks the hot reload controller with the frame time") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.Reload", {})});
        hot_reload_controller reload(fixture.runtime, {});
        reload.set_debounce_seconds(0.0);
        ecs::scheduler scheduler(fixture.world, nullptr);
        CONJURE_REQUIRE_OK(register_script_systems(scheduler, fixture.runtime, &reload));
        reload.request();
        CHECK(reload.state() == reload_state::PENDING);
        ecs::frame_info frame = {};
        frame.time = 10.0;
        scheduler.run_phase(ecs::system_phase::FRAME_BEGIN, frame);
        CHECK(reload.state() == reload_state::IDLE);
        CHECK(reload.reload_count() == 1);
        CHECK(fixture.runtime.stats().reloadCount == 1);
    }
}
