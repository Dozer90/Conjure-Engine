#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/marshal.h"
#include "conjure/bindings/native_api_entries.h"
#include "conjure/core/math/math.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include <array>
#include <string>
#include <vector>

// transform_* (docs/design/15_bindings.md 10): local and world round trips, the parent chain, reparenting
// with and without keepWorld, the interpolated transform and the dead-entity path.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    [[nodiscard]] conjure_transform at(f32 x, f32 y, f32 z) noexcept {
        conjure_transform value = {};
        value.position = {x, y, z};
        value.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        value.scale = {1.0f, 1.0f, 1.0f};
        return value;
    }
}

TEST_SUITE("bindings/transform_bindings") {

    TEST_CASE("transform - get and set local roundtrip") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity player = fixture.create("player");
        conjure_transform local = {};
        CHECK(api.transform_get_local(player.to_u64(), &local) == 0);
        CHECK(local.scale.x == 1.0f);
        CHECK(local.rotation.w == 1.0f);
        local = at(1.0f, 2.0f, 3.0f);
        local.scale = {2.0f, 2.0f, 2.0f};
        CHECK(api.transform_set_local(player.to_u64(), &local) == 0);
        conjure_transform read = {};
        CHECK(api.transform_get_local(player.to_u64(), &read) == 0);
        CHECK(read.position.y == 2.0f);
        CHECK(read.scale.z == 2.0f);
        CHECK(fixture.world.get<scene::local_transform>(player)->position == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(api.transform_get_local(player.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.transform_set_local(player.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(call_count(native_function_id::transform_set_local) == 2);
        CHECK(call_count(native_function_id::transform_get_local) == 3);
    }

    TEST_CASE("transform - set_local stamps change detection") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity player = fixture.create("player");
        fixture.step();                                            // propagate once: dirty cleared
        const math::mat4 before = fixture.world.get<scene::world_transform>(player)->matrix;
        CHECK(math::vec3(before[3]) == math::vec3(0.0f));
        const conjure_transform moved = at(4.0f, 0.0f, 0.0f);
        CHECK(api.transform_set_local(player.to_u64(), &moved) == 0);
        fixture.step();
        const math::mat4 after = fixture.world.get<scene::world_transform>(player)->matrix;
        CHECK(math::vec3(after[3]) == math::vec3(4.0f, 0.0f, 0.0f));
    }

    TEST_CASE("transform - world matrix composes a parent chain of depth 8") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        ecs::entity parent = ecs::NULL_ENTITY;
        std::vector<ecs::entity> chain = {};
        for (u32 depth = 0; depth < 8; ++depth) {
            const ecs::entity link = fixture.create("link", parent);
            const conjure_transform local = at(1.0f, 0.0f, 0.0f);
            REQUIRE(api.transform_set_local(link.to_u64(), &local) == 0);
            chain.push_back(link);
            parent = link;
        }
        conjure_mat4 matrix = {};
        CHECK(api.transform_get_world_matrix(chain.back().to_u64(), &matrix) == 0);
        CONJURE_CHECK_APPROX(matrix.m[12], 8.0f);
        CONJURE_CHECK_APPROX(matrix.m[13], 0.0f);
        conjure_transform world = {};
        CHECK(api.transform_get_world(chain.back().to_u64(), &world) == 0);
        CONJURE_CHECK_APPROX(world.position.x, 8.0f);
        CHECK(
          api.transform_get_world_matrix(chain.back().to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(api.transform_get_world(chain.back().to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("transform - set_world computes the local against the parent") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity parent = fixture.create("parent");
        const ecs::entity child = fixture.create("child", parent);
        const conjure_transform parentLocal = at(10.0f, 0.0f, 0.0f);
        REQUIRE(api.transform_set_local(parent.to_u64(), &parentLocal) == 0);
        const conjure_transform target = at(12.0f, 5.0f, 0.0f);
        CHECK(api.transform_set_world(child.to_u64(), &target) == 0);
        conjure_transform local = {};
        CHECK(api.transform_get_local(child.to_u64(), &local) == 0);
        CONJURE_CHECK_APPROX(local.position.x, 2.0f);
        CONJURE_CHECK_APPROX(local.position.y, 5.0f);
        conjure_transform world = {};
        CHECK(api.transform_get_world(child.to_u64(), &world) == 0);
        CONJURE_CHECK_APPROX(world.position.x, 12.0f);
        CHECK(api.transform_set_world(child.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.transform_set_world(999, &target) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("transform - set_parent keeps the world pose by default") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity parent = fixture.create("parent");
        const ecs::entity child = fixture.create("child");
        const conjure_transform parentLocal = at(10.0f, 0.0f, 0.0f);
        const conjure_transform childLocal = at(3.0f, 0.0f, 0.0f);
        REQUIRE(api.transform_set_local(parent.to_u64(), &parentLocal) == 0);
        REQUIRE(api.transform_set_local(child.to_u64(), &childLocal) == 0);
        CHECK(api.transform_set_parent(child.to_u64(), parent.to_u64()) == 0);
        CHECK(fixture.active().tree().parent_of(child) == parent);
        conjure_transform local = {};
        CHECK(api.transform_get_local(child.to_u64(), &local) == 0);
        CONJURE_CHECK_APPROX(local.position.x, -7.0f);            // world x stays 3
        conjure_transform world = {};
        CHECK(api.transform_get_world(child.to_u64(), &world) == 0);
        CONJURE_CHECK_APPROX(world.position.x, 3.0f);
        CHECK(api.entity_get_parent(child.to_u64()) == parent.to_u64());
        // Detaching (parent 0) keeps the world pose as well.
        CHECK(api.transform_set_parent(child.to_u64(), 0) == 0);
        CHECK(api.entity_get_parent(child.to_u64()) == 0);
        CHECK(api.transform_get_local(child.to_u64(), &local) == 0);
        CONJURE_CHECK_APPROX(local.position.x, 3.0f);
        // Failure paths: self and a cycle are bad arguments (ArgumentException in C#), a dead parent or a
        // dead child is ENTITY_NOT_ALIVE.
        CHECK(api.transform_set_parent(child.to_u64(), child.to_u64()) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.transform_set_parent(child.to_u64(), parent.to_u64()) == 0);
        CHECK(api.transform_set_parent(parent.to_u64(), child.to_u64()) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(last_error_code() == bindings_error::INVALID_ARGUMENT);
        CHECK(api.transform_set_parent(child.to_u64(), 999) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(api.transform_set_parent(999, parent.to_u64()) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("transform - set_parent_ex places at an index and may drop the world pose") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity parent = fixture.create("parent");
        const ecs::entity first = fixture.create("first", parent);
        const ecs::entity second = fixture.create("second", parent);
        const ecs::entity newcomer = fixture.create("newcomer");
        const conjure_transform parentLocal = at(10.0f, 0.0f, 0.0f);
        const conjure_transform newcomerLocal = at(1.0f, 0.0f, 0.0f);
        REQUIRE(api.transform_set_local(parent.to_u64(), &parentLocal) == 0);
        REQUIRE(api.transform_set_local(newcomer.to_u64(), &newcomerLocal) == 0);
        CHECK(api.transform_set_parent_ex(newcomer.to_u64(), parent.to_u64(), 0, 1) == 0);
        const std::span<const ecs::entity> children = fixture.active().tree().children_of(parent);
        REQUIRE(children.size() == 3);
        CHECK(children[0] == first);
        CHECK(children[1] == newcomer);
        CHECK(children[2] == second);
        conjure_transform local = {};
        CHECK(api.transform_get_local(newcomer.to_u64(), &local) == 0);
        CONJURE_CHECK_APPROX(local.position.x, 1.0f);              // keepWorld 0: the local stays
        conjure_transform world = {};
        CHECK(api.transform_get_world(newcomer.to_u64(), &world) == 0);
        CONJURE_CHECK_APPROX(world.position.x, 11.0f);
        // A negative index appends.
        CHECK(api.transform_set_parent_ex(first.to_u64(), parent.to_u64(), 1, -1) == 0);
        CHECK(fixture.active().tree().children_of(parent).back() == first);
    }

    TEST_CASE("transform - interpolated is COMPONENT_MISSING without physics") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity player = fixture.create("player");
        conjure_transform value = {};
        CHECK(api.transform_get_interpolated(player.to_u64(), &value) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(last_error_message() == "transform_get_interpolated: interpolated_transform");
        scene::interpolated_transform interpolated = {};
        interpolated.current.position = math::vec3(0.0f, 7.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<scene::interpolated_transform>(player, interpolated));
        CHECK(api.transform_get_interpolated(player.to_u64(), &value) == 0);
        CHECK(value.position.y == 7.0f);
        CHECK(api.transform_get_interpolated(player.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("transform - a dead entity reports ENTITY_NOT_ALIVE and logs once per frame") {
        log_capture logs = {};
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity doomed = fixture.create("doomed");
        CONJURE_REQUIRE_OK(fixture.active().destroy_now(doomed));
        bindings::detail::clear_frame_failure_log();
        conjure_transform local = {};
        for (u32 attempt = 0; attempt < 50; ++attempt) {
            CHECK(api.transform_get_local(doomed.to_u64(), &local) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        }
        CHECK(api.transform_set_local(0, &local) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(last_error_message() == "transform_set_local");
        CHECK(last_error_code() == bindings_error::ENTITY_NOT_ALIVE);
#if !CONJURE_DISTRIBUTION
        u32 failureLines = 0;
        for (const std::string &message : logs.messages(core::log_level::DEBUG)) {
            if (message.find("first failure of this entry this frame") != std::string::npos) {
                ++failureLines;
            }
        }
        CHECK(failureLines == 2);                                 // one per entry per frame
#endif
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("transform - 10k set_local calls are allocation free") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity player = fixture.create("player");
        conjure_transform local = at(0.0f, 0.0f, 0.0f);
        CHECK(api.transform_set_local(player.to_u64(), &local) == 0);
        allocation_scope scope = {};
        for (u32 index = 0; index < 10000; ++index) {
            local.position.x = static_cast<f32>(index);
            static_cast<void>(api.transform_set_local(player.to_u64(), &local));
            static_cast<void>(api.transform_get_local(player.to_u64(), &local));
        }
        CHECK(scope.allocation_count() == 0);
        CHECK(call_count(native_function_id::transform_set_local) == 10001);
    }
}
