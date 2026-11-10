#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/debug_draw.h"
#include "conjure/renderer/limits.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>

// debug_draw.h (docs/design/02_renderer.md 3.10): line vertices packed as the GPU consumes them,
// shape helpers with known vertex counts, the per-list cap, and the facade with timed entries.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {
    constexpr math::vec4 RED = math::vec4(1.0f, 0.0f, 0.0f, 1.0f);

    [[nodiscard]] math::aabb unit_box() {
        math::aabb box = {};
        box.min = math::vec3(-1.0f);
        box.max = math::vec3(1.0f);
        return box;
    }
}

TEST_SUITE("renderer/debug_draw") {

    TEST_CASE("pack_debug_colour - packs RGBA8 with red in the low byte and clamps to [0, 1]") {
        CHECK(pack_debug_colour(math::vec4(1.0f, 0.0f, 0.0f, 1.0f)) == 0xFF0000FFu);
        CHECK(pack_debug_colour(math::vec4(0.0f, 1.0f, 0.0f, 1.0f)) == 0xFF00FF00u);
        CHECK(pack_debug_colour(math::vec4(0.0f, 0.0f, 1.0f, 1.0f)) == 0xFFFF0000u);
        CHECK(pack_debug_colour(math::vec4(0.0f, 0.0f, 0.0f, 0.0f)) == 0u);
        CHECK(pack_debug_colour(math::vec4(2.0f, -1.0f, 0.5f, 1.0f)) == 0xFF8000FFu);
        const u32 half = pack_debug_colour(math::vec4(0.5f, 0.5f, 0.5f, 0.5f));
        CHECK((half & 0xFF) == 128);
        CHECK(((half >> 24) & 0xFF) == 128);
    }

    TEST_CASE("debug_vertex - mirrors the 16 byte GPU vertex") {
        static_assert(sizeof(debug_vertex) == 16, "16 bytes");
        debug_vertex vertex = {};
        CHECK(vertex.colour == 0xFFFFFFFFu);
        CONJURE_CHECK_APPROX(vertex.position, math::vec3(0.0f));
        debug_vertex other = vertex;
        CHECK(other == vertex);
        other.colour = 1;
        CHECK_FALSE(other == vertex);
    }

    TEST_CASE("debug_draw_buffer - every shape appends its documented number of vertices") {
        debug_draw_buffer buffer = {};
        CHECK(buffer.empty());
        buffer.line(math::vec3(0.0f), math::vec3(1.0f), RED);
        CHECK(buffer.depth_tested().size() == 2);
        buffer.box(unit_box(), RED);
        CHECK(buffer.depth_tested().size() == 2 + 24);
        buffer.sphere(math::vec3(0.0f), 1.0f, RED, true, 24);
        CHECK(buffer.depth_tested().size() == 2 + 24 + 3 * 24 * 2);
        buffer.frustum(clip_from_view(camera(), 1.0f), RED);
        CHECK(buffer.depth_tested().size() == 2 + 24 + 144 + 24);
        buffer.axes(math::mat4(1.0f), 2.0f);
        CHECK(buffer.depth_tested().size() == 2 + 24 + 144 + 24 + 6);
        buffer.grid(math::vec3(0.0f), 2, 1.0f, RED);
        CHECK(buffer.depth_tested().size() == 2 + 24 + 144 + 24 + 6 + 2 * 5 * 2);
        CHECK(buffer.overlay().empty());
        CHECK(buffer.dropped_count() == 0);
        CHECK_FALSE(buffer.empty());
        // A sphere with fewer than three segments still draws three.
        debug_draw_buffer coarse = {};
        coarse.sphere(math::vec3(0.0f), 1.0f, RED, true, 1);
        CHECK(coarse.depth_tested().size() == 3 * 3 * 2);
    }

    TEST_CASE("debug_draw_buffer - shapes carry their colour and geometry") {
        debug_draw_buffer buffer = {};
        buffer.line(math::vec3(1.0f, 2.0f, 3.0f), math::vec3(4.0f, 5.0f, 6.0f), RED, false);
        REQUIRE(buffer.overlay().size() == 2);
        CONJURE_CHECK_APPROX(buffer.overlay()[0].position, math::vec3(1.0f, 2.0f, 3.0f));
        CONJURE_CHECK_APPROX(buffer.overlay()[1].position, math::vec3(4.0f, 5.0f, 6.0f));
        CHECK(buffer.overlay()[0].colour == 0xFF0000FFu);
        CHECK(buffer.depth_tested().empty());

        buffer.box(unit_box(), RED);
        for (const debug_vertex &vertex : buffer.depth_tested()) {
            CHECK(math::absolute(vertex.position.x) == doctest::Approx(1.0f));
            CHECK(math::absolute(vertex.position.y) == doctest::Approx(1.0f));
            CHECK(math::absolute(vertex.position.z) == doctest::Approx(1.0f));
        }

        debug_draw_buffer axes = {};
        axes.axes(glm::translate(math::mat4(1.0f), math::vec3(10.0f, 0.0f, 0.0f)), 3.0f);
        REQUIRE(axes.depth_tested().size() == 6);
        CONJURE_CHECK_APPROX(axes.depth_tested()[0].position, math::vec3(10.0f, 0.0f, 0.0f));
        CONJURE_CHECK_APPROX(axes.depth_tested()[1].position, math::vec3(13.0f, 0.0f, 0.0f));
        CONJURE_CHECK_APPROX(axes.depth_tested()[3].position, math::vec3(10.0f, 3.0f, 0.0f));
        CONJURE_CHECK_APPROX(axes.depth_tested()[5].position, math::vec3(10.0f, 0.0f, 3.0f));
        CHECK((axes.depth_tested()[0].colour & 0xFF) == 0xFF);           // X is red
        CHECK(((axes.depth_tested()[2].colour >> 8) & 0xFF) == 0xFF);    // Y is green
        CHECK(((axes.depth_tested()[4].colour >> 16) & 0xFF) == 0xFF);   // Z is blue

        debug_draw_buffer sphere = {};
        sphere.sphere(math::vec3(1.0f, 2.0f, 3.0f), 2.0f, RED);
        for (const debug_vertex &vertex : sphere.depth_tested()) {
            CHECK(glm::length(vertex.position - math::vec3(1.0f, 2.0f, 3.0f)) == doctest::Approx(2.0f).epsilon(1.0e-3));
        }
    }

    TEST_CASE("debug_draw_buffer - an invalid box adds nothing and a frustum spans the view volume") {
        debug_draw_buffer buffer = {};
        buffer.box(math::aabb(), RED);
        CHECK(buffer.empty());
        camera value = {};
        value.nearPlane = 1.0f;
        value.farPlane = 10.0f;
        buffer.frustum(clip_from_view(value, 1.0f), RED);
        REQUIRE(buffer.depth_tested().size() == 24);
        f32 nearest = 1.0e9f;
        f32 farthest = 0.0f;
        for (const debug_vertex &vertex : buffer.depth_tested()) {
            nearest = std::min(nearest, -vertex.position.z);
            farthest = std::max(farthest, -vertex.position.z);
        }
        CHECK(nearest == doctest::Approx(1.0f).epsilon(1.0e-3));
        CHECK(farthest == doctest::Approx(10.0f).epsilon(1.0e-3));
    }

    TEST_CASE("debug_draw_buffer - each list is capped at MAX_DEBUG_VERTICES and the excess is counted") {
        debug_draw_buffer buffer = {};
        const u32 lines = MAX_DEBUG_VERTICES / 2;
        for (u32 index = 0; index < lines; ++index) {
            buffer.line(math::vec3(0.0f), math::vec3(1.0f), RED);
        }
        CHECK(buffer.depth_tested().size() == MAX_DEBUG_VERTICES);
        CHECK(buffer.dropped_count() == 0);
        buffer.line(math::vec3(0.0f), math::vec3(1.0f), RED);
        CHECK(buffer.depth_tested().size() == MAX_DEBUG_VERTICES);
        CHECK(buffer.dropped_count() == 2);
        buffer.box(unit_box(), RED);
        CHECK(buffer.dropped_count() == 2 + 24);
        // The overlay list has its own cap.
        buffer.line(math::vec3(0.0f), math::vec3(1.0f), RED, false);
        CHECK(buffer.overlay().size() == 2);
        CHECK(buffer.dropped_count() == 26);
        buffer.clear();
        CHECK(buffer.empty());
        CHECK(buffer.dropped_count() == 0);
    }

    TEST_CASE("debug_draw_buffer - append copies both lists and clear keeps nothing") {
        debug_draw_buffer a = {};
        a.line(math::vec3(0.0f), math::vec3(1.0f), RED);
        a.line(math::vec3(0.0f), math::vec3(2.0f), RED, false);
        debug_draw_buffer b = {};
        b.box(unit_box(), RED);
        b.append(a);
        CHECK(b.depth_tested().size() == 26);
        CHECK(b.overlay().size() == 2);
        CONJURE_CHECK_APPROX(b.overlay()[1].position, math::vec3(2.0f));
        b.append(a);
        CHECK(b.depth_tested().size() == 28);
        b.clear();
        CHECK(b.empty());
        CHECK(a.depth_tested().size() == 2);
    }

    TEST_CASE("debug_draw - immediate shapes are drawn once and timed shapes live for their duration") {
        debug_draw facade = {};
        facade.line(math::vec3(0.0f), math::vec3(1.0f), RED);
        facade.box(unit_box(), RED, 1.0f);
        facade.sphere(math::vec3(0.0f), 1.0f, RED, 2.5f);
        facade.axes(math::mat4(1.0f), 1.0f, 0.0f);
        CHECK(facade.timed_entry_count() == 2);

        debug_draw_buffer frame = {};
        facade.append_to(frame);
        CHECK(frame.depth_tested().size() == 2 + 24 + 144 + 6);
        // The immediate shapes are gone, the timed ones stay.
        frame.clear();
        facade.append_to(frame);
        CHECK(frame.depth_tested().size() == 24 + 144);

        facade.tick(1.0f);
        CHECK(facade.timed_entry_count() == 1);
        frame.clear();
        facade.append_to(frame);
        CHECK(frame.depth_tested().size() == 144);
        facade.tick(2.0f);
        CHECK(facade.timed_entry_count() == 0);
        frame.clear();
        facade.append_to(frame);
        CHECK(frame.empty());
    }

    TEST_CASE("debug_draw - submit adds a prepared buffer, clear drops everything") {
        debug_draw facade = {};
        debug_draw_buffer shape = {};
        shape.line(math::vec3(0.0f), math::vec3(1.0f), RED, false);
        facade.submit(shape);
        facade.submit(shape, 5.0f);
        CHECK(facade.timed_entry_count() == 1);
        debug_draw_buffer frame = {};
        facade.append_to(frame);
        CHECK(frame.overlay().size() == 4);
        frame.clear();
        facade.append_to(frame);
        CHECK(frame.overlay().size() == 2);
        facade.clear();
        CHECK(facade.timed_entry_count() == 0);
        frame.clear();
        facade.append_to(frame);
        CHECK(frame.empty());
    }
}
