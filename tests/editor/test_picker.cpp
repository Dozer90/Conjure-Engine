#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/math/ray.h"
#include "conjure/editor/picker.h"

#include <vector>

// picker.h: broad phase ordering, ties, misses and the narrow phase over scene geometry.

using namespace conjure;
using namespace conjure::editor;

namespace {
    pick_candidate box(u8 seed, const math::vec3 &center, const math::vec3 &halfExtents) {
        pick_candidate candidate = {};
        candidate.guid = core::uuid(seed, seed);
        candidate.worldBounds.min = center - halfExtents;
        candidate.worldBounds.max = center + halfExtents;
        candidate.entity = ecs::entity{seed, 1};
        return candidate;
    }

    math::ray forward_ray(const math::vec3 &origin = math::vec3(0.0f, 0.0f, 10.0f)) {
        math::ray ray = {};
        ray.origin = origin;
        ray.direction = math::vec3(0.0f, 0.0f, -1.0f);
        return ray;
    }
}

TEST_SUITE("editor/picker") {

    TEST_CASE("pick_mode - names round trip") {
        CHECK(to_string(pick_mode::MESH) == "mesh");
        CHECK(parse_pick_mode("bounds") == pick_mode::BOUNDS);
        CHECK(parse_pick_mode("physics") == pick_mode::PHYSICS);
        CHECK_FALSE(parse_pick_mode("laser").has_value());
    }

    TEST_CASE("pick - the nearest box along the ray wins") {
        const std::vector<pick_candidate> candidates = {
            box(1, math::vec3(0.0f, 0.0f, -5.0f), math::vec3(1.0f)),
            box(2, math::vec3(0.0f, 0.0f, 2.0f), math::vec3(1.0f)),
            box(3, math::vec3(0.0f, 0.0f, -20.0f), math::vec3(1.0f)),
        };
        const pick_result result = pick(forward_ray(), candidates, pick_mode::BOUNDS);
        REQUIRE(result.hit);
        CHECK(result.guid == candidates[1].guid);
        CONJURE_CHECK_APPROX(result.distance, 7.0f);
        CONJURE_CHECK_APPROX(result.point, math::vec3(0.0f, 0.0f, 3.0f));
        const core::json::document json = result.to_json();
        CHECK(json.root()["hit"].as_bool());
        CHECK(json.root()["guid"].as_string() == candidates[1].guid.to_string());
        CHECK(json.root()["point"].size() == 3);
    }

    TEST_CASE("pick - a ray that misses every box reports no hit") {
        const std::vector<pick_candidate> candidates = {box(1, math::vec3(5.0f, 0.0f, 0.0f), math::vec3(1.0f))};
        const pick_result result = pick(forward_ray(), candidates, pick_mode::BOUNDS);
        CHECK_FALSE(result.hit);
        CHECK(result.guid.is_null());
        CHECK_FALSE(result.to_json().root()["hit"].as_bool());
        CHECK_FALSE(pick(forward_ray(), {}, pick_mode::MESH).hit);
    }

    TEST_CASE("pick - a tie at the same entry distance goes to the smaller volume") {
        const std::vector<pick_candidate> candidates = {
            box(1, math::vec3(0.0f, 0.0f, 0.0f), math::vec3(5.0f, 5.0f, 1.0f)),
            box(2, math::vec3(0.0f, 0.0f, 0.0f), math::vec3(0.5f, 0.5f, 1.0f)),
        };
        const pick_result result = pick(forward_ray(), candidates, pick_mode::BOUNDS);
        REQUIRE(result.hit);
        CHECK(result.guid == candidates[1].guid);
    }

    TEST_CASE("pick - a ray starting inside a box hits it at distance zero") {
        const std::vector<pick_candidate> candidates = {box(1, math::vec3(0.0f), math::vec3(2.0f))};
        const pick_result result = pick(forward_ray(math::vec3(0.0f)), candidates, pick_mode::BOUNDS);
        REQUIRE(result.hit);
        CHECK(result.distance == doctest::Approx(0.0f));
    }

    TEST_CASE("pick - maxDistance limits the broad phase") {
        const std::vector<pick_candidate> candidates = {box(1, math::vec3(0.0f, 0.0f, -50.0f), math::vec3(1.0f))};
        pick_context context = {};
        context.maxDistance = 10.0f;
        CHECK_FALSE(pick(forward_ray(), candidates, pick_mode::BOUNDS, context).hit);
        context.maxDistance = 100.0f;
        CHECK(pick(forward_ray(), candidates, pick_mode::BOUNDS, context).hit);
    }

    TEST_CASE("pick - the narrow phase lets a ray pass through a sprite's empty bounds corner") {
        editor_test::headless_editor editor = {};
        // A large sprite in front, a small one behind: the ray goes through the front bounds but misses the
        // front quad, so the back sprite wins in MESH mode while BOUNDS mode stops at the front.
        const std::string front = editor.create_entity("front");
        const std::string back = editor.create_entity("back");
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", front}, {"position", core::json::array({0.0f, 0.0f, 0.0f})}})
        );
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", back}, {"position", core::json::array({0.0f, 0.0f, -5.0f})}})
        );
        (void) editor.ok(
          "component.add",
          core::json::object({
            {"entity", front},
            {"type", "sprite"},
            {"value", core::json::object({{"size", core::json::array({1.0f, 1.0f})}})},
          })
        );
        (void) editor.ok(
          "component.add",
          core::json::object({
            {"entity", back},
            {"type", "sprite"},
            {"value", core::json::object({{"size", core::json::array({4.0f, 4.0f})}})},
          })
        );
        editor.pump(2);
        std::vector<pick_candidate> candidates = gather_pick_candidates(editor.scene());
        REQUIRE(candidates.size() == 2);
        // Widen the front candidate's broad-phase box by hand so the ray enters it but misses the 1 x 1 quad.
        for (pick_candidate &candidate : candidates) {
            if (candidate.guid.to_string() == front) {
                candidate.worldBounds.min = math::vec3(-3.0f, -3.0f, -0.01f);
                candidate.worldBounds.max = math::vec3(3.0f, 3.0f, 0.01f);
            }
        }
        math::ray ray = forward_ray(math::vec3(1.5f, 1.5f, 10.0f));
        pick_context context = {};
        context.scene = &editor.scene();
        const pick_result mesh = pick(ray, candidates, pick_mode::MESH, context);
        REQUIRE(mesh.hit);
        CHECK(mesh.guid.to_string() == back);
        const pick_result bounds = pick(ray, candidates, pick_mode::BOUNDS, context);
        REQUIRE(bounds.hit);
        CHECK(bounds.guid.to_string() == front);
        // Through the centre both modes agree on the front sprite.
        const pick_result centre = pick(forward_ray(), candidates, pick_mode::MESH, context);
        REQUIRE(centre.hit);
        CHECK(centre.guid.to_string() == front);
        CONJURE_CHECK_APPROX(centre.distance, 10.0f);
    }

    TEST_CASE("pick - physics mode delegates to the physics world and misses without bodies") {
        editor_test::headless_editor editor = {};
        pick_context context = {};
        context.scene = &editor.scene();
        context.physics = editor.context().physics();
        const std::vector<pick_candidate> candidates = {box(1, math::vec3(0.0f), math::vec3(1.0f))};
        // The candidates are ignored in PHYSICS mode: no body exists, so nothing is hit.
        CHECK_FALSE(pick(forward_ray(), candidates, pick_mode::PHYSICS, context).hit);
        pick_context noPhysics = {};
        noPhysics.scene = &editor.scene();
        CHECK_FALSE(pick(forward_ray(), candidates, pick_mode::PHYSICS, noPhysics).hit);
    }
}
