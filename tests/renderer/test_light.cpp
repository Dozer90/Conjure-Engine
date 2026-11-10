#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/renderer/light.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <string_view>

// light.h (docs/design/02_renderer.md 2.4): the extraction contract's light record, glTF units and
// the atlas tile size rounding.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

TEST_SUITE("renderer/light") {

    TEST_CASE("light - the defaults are a 10 candela white point light without shadows") {
        const light value = {};
        CHECK(value.type == light_type::POINT);
        CONJURE_CHECK_APPROX(value.position, math::vec3(0.0f));
        CONJURE_CHECK_APPROX(value.direction, math::vec3(0.0f, -1.0f, 0.0f));
        CONJURE_CHECK_APPROX(value.colour, math::vec3(1.0f));
        CHECK(value.intensity == doctest::Approx(10.0f));
        CHECK(value.range == doctest::Approx(10.0f));
        CHECK(value.innerConeRadians == doctest::Approx(0.4f));
        CHECK(value.outerConeRadians == doctest::Approx(0.6f));
        CHECK(value.sourceRadius == doctest::Approx(0.05f));
        CHECK_FALSE(value.castsShadows);
        CHECK(value.filter == shadow_filter::DEFAULT);
        CHECK(value.shadowResolution == 512);
        CHECK(value == light());
    }

    TEST_CASE("light - clamp_shadow_resolution rounds to the nearest of 256, 512 and 1024") {
        CHECK(clamp_shadow_resolution(0) == 256);
        CHECK(clamp_shadow_resolution(1) == 256);
        CHECK(clamp_shadow_resolution(256) == 256);
        CHECK(clamp_shadow_resolution(300) == 256);
        CHECK(clamp_shadow_resolution(383) == 256);
        CHECK(clamp_shadow_resolution(400) == 512);
        CHECK(clamp_shadow_resolution(512) == 512);
        CHECK(clamp_shadow_resolution(700) == 512);
        CHECK(clamp_shadow_resolution(900) == 1024);
        CHECK(clamp_shadow_resolution(1024) == 1024);
        CHECK(clamp_shadow_resolution(4096) == 1024);
        CHECK(clamp_shadow_resolution(65535) == 1024);
        CHECK(MIN_SHADOW_TILE_SIZE == 256);
        CHECK(MAX_SHADOW_TILE_SIZE == 1024);
    }

    TEST_CASE("light - to_string names every light type and shadow filter") {
        CHECK(to_string(light_type::DIRECTIONAL) == "DIRECTIONAL");
        CHECK(to_string(light_type::POINT) == "POINT");
        CHECK(to_string(light_type::SPOT) == "SPOT");
        CHECK(to_string(shadow_filter::DEFAULT) == "DEFAULT");
        CHECK(to_string(shadow_filter::HARD) == "HARD");
        CHECK(to_string(shadow_filter::PCF) == "PCF");
        CHECK(to_string(shadow_filter::PCSS) == "PCSS");
        CHECK_FALSE(to_string(static_cast<light_type>(200)).empty());
        CHECK_FALSE(to_string(static_cast<shadow_filter>(200)).empty());
    }

    TEST_CASE("light - equality covers every field") {
        light a = {};
        light b = a;
        CHECK(a == b);
        b.shadowResolution = 1024;
        CHECK_FALSE(a == b);
        b = a;
        b.filter = shadow_filter::PCSS;
        CHECK_FALSE(a == b);
        b = a;
        b.direction.x = 0.1f;
        CHECK_FALSE(a == b);
        b = a;
        b.entityId = 7;
        CHECK_FALSE(a == b);
        CHECK(light().entityId == 0);
    }
}
