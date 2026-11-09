#include "conjure/testing/doctest.h"

#include "conjure/ecs/system_phase.h"

#include <string_view>
#include <type_traits>

using namespace conjure;
using namespace conjure::ecs;

TEST_SUITE("ecs/system_phase") {

    TEST_CASE("system_phase - exact list and order of R1") {
        static_assert(static_cast<u8>(system_phase::FRAME_BEGIN) == 0);
        static_assert(static_cast<u8>(system_phase::FIXED_UPDATE) == 1);
        static_assert(static_cast<u8>(system_phase::PHYSICS_STEP) == 2);
        static_assert(static_cast<u8>(system_phase::FIXED_END) == 3);
        static_assert(static_cast<u8>(system_phase::UPDATE) == 4);
        static_assert(static_cast<u8>(system_phase::LATE_UPDATE) == 5);
        static_assert(static_cast<u8>(system_phase::TRANSFORM) == 6);
        static_assert(static_cast<u8>(system_phase::SYNC) == 7);
        static_assert(static_cast<u8>(system_phase::EXTRACT) == 8);
        static_assert(static_cast<u8>(system_phase::FRAME_END) == 9);
        static_assert(static_cast<u8>(system_phase::COUNT) == 10);
        static_assert(SYSTEM_PHASE_COUNT == 10);
        static_assert(std::is_same_v<std::underlying_type_t<system_phase>, u8>);
        CHECK(true);
    }

    TEST_CASE("system_phase - to_string names every phase") {
        CHECK(to_string(system_phase::FRAME_BEGIN) == "FRAME_BEGIN");
        CHECK(to_string(system_phase::FIXED_UPDATE) == "FIXED_UPDATE");
        CHECK(to_string(system_phase::PHYSICS_STEP) == "PHYSICS_STEP");
        CHECK(to_string(system_phase::FIXED_END) == "FIXED_END");
        CHECK(to_string(system_phase::UPDATE) == "UPDATE");
        CHECK(to_string(system_phase::LATE_UPDATE) == "LATE_UPDATE");
        CHECK(to_string(system_phase::TRANSFORM) == "TRANSFORM");
        CHECK(to_string(system_phase::SYNC) == "SYNC");
        CHECK(to_string(system_phase::EXTRACT) == "EXTRACT");
        CHECK(to_string(system_phase::FRAME_END) == "FRAME_END");
        CHECK(to_string(system_phase::COUNT) == "COUNT");
        CHECK(to_string(static_cast<system_phase>(200)) == "COUNT");
    }

    TEST_CASE("frame_info - defaults are zero and not in a fixed step") {
        const frame_info frame = {};
        CHECK(frame.deltaTime == 0.0f);
        CHECK(frame.fixedDeltaTime == 0.0f);
        CHECK(frame.interpolationAlpha == 0.0f);
        CHECK(frame.time == 0.0);
        CHECK(frame.frameIndex == 0);
        CHECK_FALSE(frame.inFixedStep);
        static_assert(std::is_trivially_copyable_v<frame_info>);
    }
}
