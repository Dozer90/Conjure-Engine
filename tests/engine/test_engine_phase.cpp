#include "conjure/testing/doctest.h"

#include "conjure/core/types.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/engine/engine_phase.h"

// engine_phase.h (docs/design/06_engine_loop.md 2.4, R1): the fixed-step grouping and the phase helpers over
// the ecs phase list.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/engine_phase") {

    TEST_CASE("engine_phase - the phase list is the documented ten in order") {
        CHECK(ecs::SYSTEM_PHASE_COUNT == 10);
        CHECK(phase_index(ecs::system_phase::FRAME_BEGIN) == 0);
        CHECK(phase_index(ecs::system_phase::FIXED_UPDATE) == 1);
        CHECK(phase_index(ecs::system_phase::PHYSICS_STEP) == 2);
        CHECK(phase_index(ecs::system_phase::FIXED_END) == 3);
        CHECK(phase_index(ecs::system_phase::UPDATE) == 4);
        CHECK(phase_index(ecs::system_phase::LATE_UPDATE) == 5);
        CHECK(phase_index(ecs::system_phase::TRANSFORM) == 6);
        CHECK(phase_index(ecs::system_phase::SYNC) == 7);
        CHECK(phase_index(ecs::system_phase::EXTRACT) == 8);
        CHECK(phase_index(ecs::system_phase::FRAME_END) == 9);
        CHECK(phase_index(ecs::system_phase::COUNT) == ecs::SYSTEM_PHASE_COUNT);
    }

    TEST_CASE("engine_phase - the fixed phases are the middle three") {
        REQUIRE(FIXED_PHASES.size() == 3);
        CHECK(FIXED_PHASES[0] == ecs::system_phase::FIXED_UPDATE);
        CHECK(FIXED_PHASES[1] == ecs::system_phase::PHYSICS_STEP);
        CHECK(FIXED_PHASES[2] == ecs::system_phase::FIXED_END);
        for (u32 index = 0; index < ecs::SYSTEM_PHASE_COUNT; ++index) {
            const ecs::system_phase phase = static_cast<ecs::system_phase>(index);
            const bool expected = index >= 1 && index <= 3;
            CHECK(is_fixed_phase(phase) == expected);
        }
        static_assert(is_fixed_phase(ecs::system_phase::PHYSICS_STEP));
        static_assert(!is_fixed_phase(ecs::system_phase::UPDATE));
    }

    TEST_CASE("engine_phase - to_string names every phase like the ecs does") {
        for (u32 index = 0; index < ecs::SYSTEM_PHASE_COUNT; ++index) {
            const ecs::system_phase phase = static_cast<ecs::system_phase>(index);
            CHECK(conjure::engine::to_string(phase) == ecs::to_string(phase));
            CHECK_FALSE(conjure::engine::to_string(phase).empty());
        }
        CHECK(conjure::engine::to_string(ecs::system_phase::UPDATE) == "UPDATE");
        CHECK(conjure::engine::to_string(ecs::system_phase::COUNT) == "COUNT");
    }
}
