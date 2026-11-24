#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"

#include "conjure/renderer/limits.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// render_settings.h (docs/design/02_renderer.md 2.3, 3.4, 3.7, 3.8): the settings groups with their
// defaults and ranges, sanitise, the project.json "rendering.defaults" reader and writers, and the live
// project defaults.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] core::json::document parse_json(std::string_view text) {
        core::result<core::json::document> parsed = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(parsed);
        return std::move(parsed.value());
    }

    /// <summary>
    /// The snake_case spelling of a camelCase member name, as the reader expects the keys.
    /// </summary>
    [[nodiscard]] std::string snake_key(std::string_view camel) {
        std::string snake = {};
        for (const char character : camel) {
            if (character >= 'A' && character <= 'Z') {
                snake.push_back('_');
                snake.push_back(static_cast<char>(character - 'A' + 'a'));
                continue;
            }
            snake.push_back(character);
        }
        return snake;
    }

    /// <summary>
    /// The member names of a JSON object, in order.
    /// </summary>
    [[nodiscard]] std::vector<std::string> member_keys(const core::json::value &object) {
        std::vector<std::string> keys = {};
        for (const core::json::member &entry : object.members()) {
            keys.emplace_back(entry.key);
        }
        return keys;
    }

    /// <summary>
    /// Settings with every field away from its default and inside its range.
    /// </summary>
    [[nodiscard]] render_settings tuned_settings() {
        render_settings settings = {};
        settings.postProcess.manualEv100 = 13.5f;
        settings.postProcess.exposureCompensation = -1.25f;
        settings.postProcess.adaptationSpeedUp = 4.0f;
        settings.postProcess.adaptationSpeedDown = 0.5f;
        settings.postProcess.minEv100 = -4.0f;
        settings.postProcess.maxEv100 = 18.0f;
        settings.postProcess.bloomIntensity = 0.06f;
        settings.postProcess.reinhardWhitePoint = 6.0f;
        settings.postProcess.exposure = exposure_mode::MANUAL;
        settings.postProcess.tonemap = tonemap_operator::AGX;
        settings.postProcess.agxLook = agx_look::PUNCHY;
        settings.postProcess.bloomEnabled = false;
        settings.shadows.distance = 60.0f;
        settings.shadows.splitLambda = 0.5f;
        settings.shadows.casterPadding = 20.0f;
        settings.shadows.pcfRadiusTexels = 2.0f;
        settings.shadows.normalOffsetTexels = 1.0f;
        settings.shadows.cascadeCount = 3;
        settings.shadows.defaultFilter = shadow_filter::PCSS;
        settings.shadows.enabled = false;
        settings.ssao.radius = 0.75f;
        settings.ssao.intensity = 2.0f;
        settings.ssao.thickness = 0.25f;
        settings.ssao.enabled = false;
        return settings;
    }

    struct group_case {
        render_settings_group group = render_settings_group::COUNT;
        const core::reflect::type_info *type = nullptr;
    };

    [[nodiscard]] std::array<group_case, 3> every_group() {
        return {
          group_case{render_settings_group::POST_PROCESS, core::reflect::type_info_of<post_process_settings>()},
          group_case{render_settings_group::SHADOWS, core::reflect::type_info_of<shadow_settings>()},
          group_case{render_settings_group::SSAO, core::reflect::type_info_of<ssao_settings>()},
        };
    }

    /// <summary>
    /// Runs the group's sanitise on the group of 'settings'.
    /// </summary>
    [[nodiscard]] std::string_view sanitise_group(render_settings &settings, render_settings_group group) {
        switch (group) {
            case render_settings_group::POST_PROCESS: {
                return sanitise(settings.postProcess);
            }
            case render_settings_group::SHADOWS: {
                return sanitise(settings.shadows);
            }
            case render_settings_group::SSAO: {
                return sanitise(settings.ssao);
            }
            case render_settings_group::COUNT: {
                return "unknown group";
            }
        }
        return "unknown group";
    }

    [[nodiscard]] std::byte *group_bytes(render_settings &settings, render_settings_group group) {
        switch (group) {
            case render_settings_group::POST_PROCESS: {
                return reinterpret_cast<std::byte *>(&settings.postProcess);
            }
            case render_settings_group::SHADOWS: {
                return reinterpret_cast<std::byte *>(&settings.shadows);
            }
            case render_settings_group::SSAO: {
                return reinterpret_cast<std::byte *>(&settings.ssao);
            }
            case render_settings_group::COUNT: {
                return nullptr;
            }
        }
        return nullptr;
    }

    /// <summary>
    /// Stores a number into a reflected F32 or U32 field.
    /// </summary>
    void store_number(std::byte *group, const core::reflect::field_info &field, f64 number) {
        if (field.kind == core::reflect::field_kind::F32) {
            const f32 value = static_cast<f32>(number);
            std::memcpy(group + field.offset, &value, sizeof(value));
            return;
        }
        const u32 value = static_cast<u32>(number);
        std::memcpy(group + field.offset, &value, sizeof(value));
    }

    [[nodiscard]] f64 load_number(const std::byte *group, const core::reflect::field_info &field) {
        if (field.kind == core::reflect::field_kind::F32) {
            f32 value = 0.0f;
            std::memcpy(&value, group + field.offset, sizeof(value));
            return static_cast<f64>(value);
        }
        u32 value = 0;
        std::memcpy(&value, group + field.offset, sizeof(value));
        return static_cast<f64>(value);
    }

    [[nodiscard]] bool is_numeric(const core::reflect::field_info &field) {
        return field.kind == core::reflect::field_kind::F32 || field.kind == core::reflect::field_kind::U32;
    }
}

TEST_SUITE("renderer/render_settings") {

    TEST_CASE("post_process_settings - defaults are automatic exposure, bloom and the fitted ACES") {
        const post_process_settings settings = {};
        CHECK(settings.exposure == exposure_mode::AUTOMATIC);
        CHECK(settings.manualEv100 == doctest::Approx(12.0f));
        CHECK(settings.exposureCompensation == doctest::Approx(0.0f));
        CHECK(settings.adaptationSpeedUp == doctest::Approx(3.0f));
        CHECK(settings.adaptationSpeedDown == doctest::Approx(1.0f));
        CHECK(settings.minEv100 == doctest::Approx(-10.0f));
        CHECK(settings.maxEv100 == doctest::Approx(16.0f));
        CHECK(settings.bloomEnabled);
        CHECK(settings.bloomIntensity == doctest::Approx(0.04f));
        CHECK(settings.tonemap == tonemap_operator::ACES_FITTED);
        CHECK(settings.reinhardWhitePoint == doctest::Approx(4.0f));
        CHECK(settings.agxLook == agx_look::DEFAULT);
        post_process_settings other = settings;
        CHECK(other == settings);
        other.tonemap = tonemap_operator::AGX;
        CHECK_FALSE(other == settings);
    }

    TEST_CASE("shadow_settings - defaults are four PCF cascades over 100 metres") {
        const shadow_settings settings = {};
        CHECK(settings.enabled);
        CHECK(settings.distance == doctest::Approx(100.0f));
        CHECK(settings.splitLambda == doctest::Approx(0.7f));
        CHECK(settings.casterPadding == doctest::Approx(50.0f));
        CHECK(settings.pcfRadiusTexels == doctest::Approx(1.5f));
        CHECK(settings.normalOffsetTexels == doctest::Approx(1.5f));
        CHECK(settings.defaultFilter == shadow_filter::PCF);
        CHECK(settings.cascadeCount == MAX_SHADOW_CASCADES);
        shadow_settings other = settings;
        other.cascadeCount = 2;
        CHECK_FALSE(other == settings);
    }

    TEST_CASE("ssao_settings - defaults are a one metre radius at full intensity") {
        const ssao_settings settings = {};
        CHECK(settings.enabled);
        CHECK(settings.radius == doctest::Approx(1.0f));
        CHECK(settings.intensity == doctest::Approx(1.0f));
        CHECK(settings.thickness == doctest::Approx(0.5f));
        CHECK(settings == ssao_settings());
    }

    TEST_CASE("environment_settings - the default has no environment and unit intensity") {
        const environment_settings settings = {};
        CHECK_FALSE(settings.environment.is_valid());
        CHECK(settings.intensity == doctest::Approx(1.0f));
        CHECK(settings.rotationRadians == doctest::Approx(0.0f));
        environment_settings other = settings;
        other.environment.index = 3;
        other.environment.generation = 1;
        CHECK_FALSE(other == settings);
    }

    TEST_CASE("render_settings - the groups have their pinned sizes, offsets and defaults") {
        static_assert(sizeof(post_process_settings) == 36);
        static_assert(sizeof(shadow_settings) == 28);
        static_assert(sizeof(ssao_settings) == 16);
        static_assert(sizeof(render_settings) == 80);
        // Floats first, then the one-byte members: no padding between members.
        CHECK(offsetof(post_process_settings, manualEv100) == 0);
        CHECK(offsetof(post_process_settings, reinhardWhitePoint) == 28);
        CHECK(offsetof(post_process_settings, exposure) == 32);
        CHECK(offsetof(post_process_settings, tonemap) == 33);
        CHECK(offsetof(post_process_settings, agxLook) == 34);
        CHECK(offsetof(post_process_settings, bloomEnabled) == 35);
        CHECK(offsetof(shadow_settings, cascadeCount) == 20);
        CHECK(offsetof(shadow_settings, defaultFilter) == 24);
        CHECK(offsetof(shadow_settings, enabled) == 25);
        CHECK(offsetof(ssao_settings, enabled) == 12);
        CHECK(offsetof(render_settings, postProcess) == 0);
        CHECK(offsetof(render_settings, shadows) == 36);
        CHECK(offsetof(render_settings, ssao) == 64);

        const render_settings settings = {};
        CHECK(settings.postProcess == post_process_settings());
        CHECK(settings.shadows == shadow_settings());
        CHECK(settings.ssao == ssao_settings());
        render_settings other = settings;
        CHECK(other == settings);
        other.ssao.radius = 2.0f;
        CHECK_FALSE(other == settings);
        CHECK(static_cast<usize>(render_settings_group::COUNT) == 3);
    }

    TEST_CASE("render_settings - every numeric field carries its range and every field a tooltip") {
        for (const group_case &entry : every_group()) {
            REQUIRE(entry.type != nullptr);
            for (const core::reflect::field_info &field : entry.type->fields) {
                CAPTURE(entry.type->name);
                CAPTURE(field.name);
                CHECK_FALSE(field.attributes.tooltip.empty());
                if (!is_numeric(field)) {
                    CHECK_FALSE(field.attributes.min.has_value());
                    CHECK_FALSE(field.attributes.max.has_value());
                    continue;
                }
                REQUIRE(field.attributes.min.has_value());
                REQUIRE(field.attributes.max.has_value());
                CHECK(*field.attributes.min < *field.attributes.max);
            }
        }
        const core::reflect::field_info *radius = core::reflect::type_info_of<ssao_settings>()->find_field("radius");
        REQUIRE(radius != nullptr);
        CHECK(static_cast<f32>(*radius->attributes.min) == MIN_SSAO_RADIUS);
        CHECK(static_cast<f32>(*radius->attributes.max) == MAX_SSAO_RADIUS);
        const core::reflect::field_info *cascades = core::reflect::type_info_of<shadow_settings>()
          ->find_field("cascadeCount");
        REQUIRE(cascades != nullptr);
        CHECK(*cascades->attributes.min == doctest::Approx(1.0));
        CHECK(*cascades->attributes.max == doctest::Approx(static_cast<f64>(MAX_SHADOW_CASCADES)));
    }

    TEST_CASE("sanitise - every numeric field is clamped into its reflected range and named") {
        // Driven by reflection, so the ranges sanitise applies and the ranges the editor and the reader see
        // come from the same constants.
        for (const group_case &entry : every_group()) {
            REQUIRE(entry.type != nullptr);
            for (const core::reflect::field_info &field : entry.type->fields) {
                if (!is_numeric(field)) {
                    continue;
                }
                CAPTURE(entry.type->name);
                CAPTURE(field.name);
                const f64 minimum = *field.attributes.min;
                const f64 maximum = *field.attributes.max;

                // Above the maximum (each probe starts from fresh settings; doctest SUBCASEs do not repeat
                // inside a loop).
                render_settings above = {};
                std::byte *aboveGroup = group_bytes(above, entry.group);
                store_number(aboveGroup, field, maximum + 1.0);
                CHECK(sanitise_group(above, entry.group) == snake_key(field.name));
                CHECK(load_number(aboveGroup, field) <= maximum);
                CHECK(load_number(aboveGroup, field) >= minimum);
                CHECK(sanitise_group(above, entry.group).empty());

                // Below the minimum; an unsigned field with a minimum of 0 cannot go below it.
                if (field.kind == core::reflect::field_kind::F32 || minimum > 0.0) {
                    render_settings below = {};
                    std::byte *belowGroup = group_bytes(below, entry.group);
                    const bool isFloat = field.kind == core::reflect::field_kind::F32;
                    store_number(belowGroup, field, isFloat ? minimum - 1.0 : 0.0);
                    CHECK(sanitise_group(below, entry.group) == snake_key(field.name));
                    CHECK(load_number(belowGroup, field) >= minimum);
                    CHECK(load_number(belowGroup, field) <= maximum);
                }

                // The bounds themselves are valid; the exposure bounds also have to keep max >= min.
                render_settings bounds = {};
                std::byte *boundsGroup = group_bytes(bounds, entry.group);
                store_number(boundsGroup, field, minimum);
                if (field.name != "maxEv100") {
                    CHECK(sanitise_group(bounds, entry.group).empty());
                    CHECK(load_number(boundsGroup, field) == static_cast<f64>(static_cast<f32>(minimum)));
                }
                store_number(boundsGroup, field, maximum);
                if (field.name != "minEv100") {
                    CHECK(sanitise_group(bounds, entry.group).empty());
                    CHECK(load_number(boundsGroup, field) == static_cast<f64>(static_cast<f32>(maximum)));
                }
            }
        }
    }

    TEST_CASE("sanitise - exact clamps, NaN and infinities") {
        const f32 nan = std::numeric_limits<f32>::quiet_NaN();
        const f32 infinity = std::numeric_limits<f32>::infinity();

        SUBCASE("a NaN takes the field's default") {
            post_process_settings postProcess = {};
            postProcess.bloomIntensity = nan;
            CHECK(sanitise(postProcess) == "bloom_intensity");
            CHECK(postProcess.bloomIntensity == post_process_settings().bloomIntensity);
            shadow_settings shadows = {};
            shadows.distance = nan;
            CHECK(sanitise(shadows) == "distance");
            CHECK(shadows.distance == shadow_settings().distance);
            ssao_settings ssao = {};
            ssao.thickness = nan;
            CHECK(sanitise(ssao) == "thickness");
            CHECK(ssao.thickness == ssao_settings().thickness);
        }
        SUBCASE("an infinity takes the bound it passes") {
            post_process_settings postProcess = {};
            postProcess.manualEv100 = infinity;
            postProcess.exposureCompensation = -infinity;
            CHECK(sanitise(postProcess) == "manual_ev100");
            CHECK(postProcess.manualEv100 == MAX_EV100);
            CHECK(postProcess.exposureCompensation == -MAX_EXPOSURE_COMPENSATION);
            ssao_settings ssao = {};
            ssao.radius = -infinity;
            CHECK(sanitise(ssao) == "radius");
            CHECK(ssao.radius == MIN_SSAO_RADIUS);
        }
        SUBCASE("each bound clamps to itself") {
            shadow_settings shadows = {};
            shadows.distance = 0.0f;
            shadows.casterPadding = 5000.0f;
            shadows.cascadeCount = 0;
            CHECK(sanitise(shadows) == "distance");
            CHECK(shadows.distance == MIN_SHADOW_DISTANCE);
            CHECK(shadows.casterPadding == MAX_CASTER_PADDING);
            CHECK(shadows.cascadeCount == 1);
            shadows.cascadeCount = MAX_SHADOW_CASCADES + 1;
            CHECK(sanitise(shadows) == "cascade_count");
            CHECK(shadows.cascadeCount == MAX_SHADOW_CASCADES);
            post_process_settings postProcess = {};
            postProcess.reinhardWhitePoint = 0.0f;
            CHECK(sanitise(postProcess) == "reinhard_white_point");
            CHECK(postProcess.reinhardWhitePoint == MIN_REINHARD_WHITE_POINT);
        }
        SUBCASE("the first changed field in declaration order is the one named") {
            post_process_settings postProcess = {};
            postProcess.bloomIntensity = 2.0f;
            postProcess.adaptationSpeedUp = -1.0f;
            CHECK(sanitise(postProcess) == "adaptation_speed_up");
            CHECK(postProcess.bloomIntensity == MAX_BLOOM_INTENSITY);
            CHECK(postProcess.adaptationSpeedUp == 0.0f);
        }
    }

    TEST_CASE("sanitise - an inverted exposure range raises max_ev100 to min_ev100") {
        post_process_settings postProcess = {};
        postProcess.minEv100 = 10.0f;
        postProcess.maxEv100 = 4.0f;
        CHECK(sanitise(postProcess) == "max_ev100");
        CHECK(postProcess.minEv100 == 10.0f);
        CHECK(postProcess.maxEv100 == 10.0f);
        // An equal pair is a valid (fixed) range.
        CHECK(sanitise(postProcess).empty());
        // A NaN minimum takes its default first, then the maximum is compared with it.
        postProcess.minEv100 = std::numeric_limits<f32>::quiet_NaN();
        postProcess.maxEv100 = -12.0f;
        CHECK(sanitise(postProcess) == "min_ev100");
        CHECK(postProcess.minEv100 == post_process_settings().minEv100);
        CHECK(postProcess.maxEv100 == post_process_settings().minEv100);
    }

    TEST_CASE("sanitise - enumerators outside their enum and the DEFAULT filter take the default") {
        SUBCASE("exposure") {
            post_process_settings postProcess = {};
            postProcess.exposure = static_cast<exposure_mode>(7);
            CHECK(sanitise(postProcess) == "exposure");
            CHECK(postProcess.exposure == exposure_mode::AUTOMATIC);
        }
        SUBCASE("tonemap") {
            post_process_settings postProcess = {};
            postProcess.tonemap = static_cast<tonemap_operator>(4);
            CHECK(sanitise(postProcess) == "tonemap");
            CHECK(postProcess.tonemap == tonemap_operator::ACES_FITTED);
        }
        SUBCASE("agx look") {
            post_process_settings postProcess = {};
            postProcess.agxLook = static_cast<agx_look>(3);
            CHECK(sanitise(postProcess) == "agx_look");
            CHECK(postProcess.agxLook == agx_look::DEFAULT);
            postProcess.agxLook = agx_look::PUNCHY;
            CHECK(sanitise(postProcess).empty());
        }
        SUBCASE("default filter out of range") {
            shadow_settings shadows = {};
            shadows.defaultFilter = static_cast<shadow_filter>(9);
            CHECK(sanitise(shadows) == "default_filter");
            CHECK(shadows.defaultFilter == shadow_filter::PCF);
        }
        SUBCASE("default filter DEFAULT never survives") {
            shadow_settings shadows = {};
            shadows.defaultFilter = shadow_filter::DEFAULT;
            CHECK(sanitise(shadows) == "default_filter");
            CHECK(shadows.defaultFilter == shadow_filter::PCF);
            shadows.defaultFilter = shadow_filter::HARD;
            CHECK(sanitise(shadows).empty());
            CHECK(shadows.defaultFilter == shadow_filter::HARD);
        }
    }

    TEST_CASE("sanitise - valid values are left alone and give an empty name") {
        render_settings settings = tuned_settings();
        const render_settings before = settings;
        CHECK(sanitise(settings.postProcess).empty());
        CHECK(sanitise(settings.shadows).empty());
        CHECK(sanitise(settings.ssao).empty());
        CHECK(settings == before);
        render_settings defaults = {};
        CHECK(sanitise(defaults.postProcess).empty());
        CHECK(sanitise(defaults.shadows).empty());
        CHECK(sanitise(defaults.ssao).empty());
        CHECK(defaults == render_settings());
    }

    TEST_CASE("read_render_settings - null or absent defaults mean the engine defaults") {
        log_capture capture = {};
        SUBCASE("absent") {
            const render_settings_reading reading = read_render_settings(core::json::value());
            CHECK(reading.settings == render_settings());
            CHECK(reading.errors.empty());
            CHECK(reading.unknownKeys.empty());
        }
        SUBCASE("null") {
            const core::json::document document = parse_json(R"({"defaults": null})");
            const render_settings_reading reading = read_render_settings(document.root()["defaults"]);
            CHECK(reading.settings == render_settings());
            CHECK(reading.errors.empty());
        }
        SUBCASE("empty object") {
            const core::json::document document = parse_json("{}");
            const render_settings_reading reading = read_render_settings(document.root());
            CHECK(reading.settings == render_settings());
            CHECK(reading.errors.empty());
            CHECK(reading.unknownKeys.empty());
        }
        SUBCASE("empty groups") {
            const core::json::document document = parse_json(R"({"post_process": {}, "shadows": {}, "ssao": {}})");
            const render_settings_reading reading = read_render_settings(document.root());
            CHECK(reading.settings == render_settings());
            CHECK(reading.errors.empty());
        }
    }

    TEST_CASE("read_render_settings - a partial object changes only the keys it has") {
        const core::json::document document = parse_json(R"({
            "post_process": {"exposure": "manual", "manual_ev100": 13.0, "tonemap": "agx", "agx_look": "punchy",
                             "bloom_intensity": 0.06},
            "shadows": {"distance": 60.0, "cascade_count": 3, "default_filter": "pcss"},
            "ssao": {"radius": 0.75}
        })");
        const render_settings_reading reading = read_render_settings(document.root());
        CHECK(reading.errors.empty());
        CHECK(reading.unknownKeys.empty());
        render_settings expected = {};
        expected.postProcess.exposure = exposure_mode::MANUAL;
        expected.postProcess.manualEv100 = 13.0f;
        expected.postProcess.tonemap = tonemap_operator::AGX;
        expected.postProcess.agxLook = agx_look::PUNCHY;
        expected.postProcess.bloomIntensity = 0.06f;
        expected.shadows.distance = 60.0f;
        expected.shadows.cascadeCount = 3;
        expected.shadows.defaultFilter = shadow_filter::PCSS;
        expected.ssao.radius = 0.75f;
        CHECK(reading.settings == expected);
    }

    TEST_CASE("read_render_settings - the bounds themselves are accepted in their JSON spelling") {
        // 0.05 rounds to the f32 bound MIN_SSAO_RADIUS; a reader comparing in f64 would refuse it.
        const core::json::document document = parse_json(R"({
            "post_process": {"min_ev100": -16, "max_ev100": 24, "bloom_intensity": 1, "exposure_compensation": -10},
            "shadows": {"distance": 10000, "cascade_count": 1, "split_lambda": 0},
            "ssao": {"radius": 0.05, "intensity": 8, "thickness": 1}
        })");
        const render_settings_reading reading = read_render_settings(document.root());
        CHECK(reading.errors.empty());
        CHECK(reading.settings.postProcess.minEv100 == MIN_EV100);
        CHECK(reading.settings.postProcess.maxEv100 == MAX_EV100);
        CHECK(reading.settings.postProcess.exposureCompensation == -MAX_EXPOSURE_COMPENSATION);
        CHECK(reading.settings.shadows.distance == MAX_SHADOW_DISTANCE);
        CHECK(reading.settings.shadows.cascadeCount == 1);
        CHECK(reading.settings.ssao.radius == MIN_SSAO_RADIUS);
        CHECK(reading.settings.ssao.intensity == MAX_SSAO_INTENSITY);
    }

    TEST_CASE("read_render_settings - each kind of error names its slash path and keeps only that leaf's default") {
        struct error_case {
            std::string_view json = {};
            std::string_view path = {};
            std::string_view reasonPart = {};
        };
        const error_case cases[] = {
          {R"({"post_process": {"bloom_intensity": "high"}})", "rendering/defaults/post_process/bloom_intensity",
            "expected a number, got string"},
          {R"({"post_process": {"bloom_enabled": 1}})", "rendering/defaults/post_process/bloom_enabled",
            "expected true or false, got number"},
          {R"({"post_process": {"tonemap": 2}})", "rendering/defaults/post_process/tonemap", "got number"},
          {R"({"post_process": {"tonemap": null}})", "rendering/defaults/post_process/tonemap", "got null"},
          {R"({"post_process": {"tonemap": "filmic"}})", "rendering/defaults/post_process/tonemap",
            "'filmic' is not one of aces_fitted, reinhard_extended, agx, none"},
          {R"({"post_process": {"bloom_intensity": 1.5}})", "rendering/defaults/post_process/bloom_intensity",
            "1.5 is outside [0, 1]"},
          {R"({"post_process": {"manual_ev100": -17}})", "rendering/defaults/post_process/manual_ev100",
            "-17 is outside [-16, 24]"},
          {R"({"post_process": {"manual_ev100": 1e300}})", "rendering/defaults/post_process/manual_ev100",
            "is outside [-16, 24]"},
          {R"({"ssao": {"radius": 0.01}})", "rendering/defaults/ssao/radius", "0.01 is outside [0.05, 10]"},
          {R"({"shadows": {"cascade_count": 2.5}})", "rendering/defaults/shadows/cascade_count",
            "expected a whole number, got 2.5"},
          {R"({"shadows": {"cascade_count": 0}})", "rendering/defaults/shadows/cascade_count", "0 is outside [1, 4]"},
          {R"({"shadows": {"cascade_count": -3}})", "rendering/defaults/shadows/cascade_count", "is outside [1, 4]"},
          {R"({"shadows": {"default_filter": "default"}})", "rendering/defaults/shadows/default_filter",
            "'default' is not one of hard, pcf, pcss"},
          {R"({"shadows": {"enabled": "yes"}})", "rendering/defaults/shadows/enabled", "expected true or false"},
          {R"({"shadows": [1, 2]})", "rendering/defaults/shadows", "expected an object, got array"},
          {R"({"ssao": null})", "rendering/defaults/ssao", "expected an object, got null"},
        };
        for (const error_case &entry : cases) {
            CAPTURE(entry.json);
            // Every case also carries a valid leaf in another group, which must still apply.
            core::json::document document = parse_json(entry.json);
            const bool hasPostProcess = document.root().contains("post_process");
            const char *validLeaf = hasPostProcess ? "/ssao/intensity" : "/post_process/reinhard_white_point";
            REQUIRE(document.set_pointer(validLeaf, 3.0));
            const render_settings_reading reading = read_render_settings(document.root());
            REQUIRE(reading.errors.size() == 1);
            CAPTURE(reading.errors[0].reason);
            CHECK(reading.errors[0].path == entry.path);
            CHECK(reading.errors[0].reason.find(entry.reasonPart) != std::string::npos);
            render_settings expected = {};
            if (hasPostProcess) {
                expected.ssao.intensity = 3.0f;
            } else {
                expected.postProcess.reinhardWhitePoint = 3.0f;
            }
            CHECK(reading.settings == expected);
            CHECK(reading.unknownKeys.empty());
        }
    }

    TEST_CASE("read_render_settings - several bad leaves are all listed and a repeated key keeps its default") {
        const core::json::document document = parse_json(R"({
            "post_process": {"bloom_intensity": 0.5, "bloom_intensity": 9, "exposure": "manual"},
            "ssao": {"radius": -1, "thickness": 2, "enabled": false}
        })");
        const render_settings_reading reading = read_render_settings(document.root());
        REQUIRE(reading.errors.size() == 3);
        CHECK(reading.errors[0].path == "rendering/defaults/post_process/bloom_intensity");
        CHECK(reading.errors[1].path == "rendering/defaults/ssao/radius");
        CHECK(reading.errors[2].path == "rendering/defaults/ssao/thickness");
        render_settings expected = {};
        expected.postProcess.exposure = exposure_mode::MANUAL;
        expected.ssao.enabled = false;
        CHECK(reading.settings == expected);
    }

    TEST_CASE("read_render_settings - an inverted exposure range puts both bounds back to their defaults") {
        SUBCASE("both keys given: the error is at max_ev100") {
            const core::json::document document = parse_json(
              R"({"post_process": {"min_ev100": 8, "max_ev100": 2, "bloom_intensity": 0.1}})"
            );
            const render_settings_reading reading = read_render_settings(document.root());
            REQUIRE(reading.errors.size() == 1);
            CHECK(reading.errors[0].path == "rendering/defaults/post_process/max_ev100");
            CHECK(reading.errors[0].reason == "max_ev100 2 is below min_ev100 8");
            CHECK(reading.settings.postProcess.minEv100 == post_process_settings().minEv100);
            CHECK(reading.settings.postProcess.maxEv100 == post_process_settings().maxEv100);
            CHECK(reading.settings.postProcess.bloomIntensity == 0.1f);
        }
        SUBCASE("only min_ev100 given: the error is at min_ev100") {
            const core::json::document document = parse_json(R"({"post_process": {"min_ev100": 20}})");
            const render_settings_reading reading = read_render_settings(document.root());
            REQUIRE(reading.errors.size() == 1);
            CHECK(reading.errors[0].path == "rendering/defaults/post_process/min_ev100");
            CHECK(reading.settings.postProcess == post_process_settings());
        }
        SUBCASE("an equal pair is a valid range") {
            const core::json::document document = parse_json(R"({"post_process": {"min_ev100": 5, "max_ev100": 5}})");
            const render_settings_reading reading = read_render_settings(document.root());
            CHECK(reading.errors.empty());
            CHECK(reading.settings.postProcess.minEv100 == 5.0f);
            CHECK(reading.settings.postProcess.maxEv100 == 5.0f);
        }
    }

    TEST_CASE("read_render_settings - a reading always passes sanitise unchanged") {
        const std::string_view documents[] = {
          R"({"post_process": {"min_ev100": 20}, "shadows": {"default_filter": "default"}})",
          R"({"post_process": {"max_ev100": -16}, "ssao": {"radius": 0.05}})",
          R"({"shadows": {"cascade_count": 4, "default_filter": "HARD"}})",
        };
        for (const std::string_view text : documents) {
            CAPTURE(text);
            const core::json::document document = parse_json(text);
            render_settings_reading reading = read_render_settings(document.root());
            CHECK(sanitise(reading.settings.postProcess).empty());
            CHECK(sanitise(reading.settings.shadows).empty());
            CHECK(sanitise(reading.settings.ssao).empty());
        }
    }

    TEST_CASE("read_render_settings - unknown keys are listed with their full path and are not errors") {
        log_capture capture = {};
        const core::json::document document = parse_json(R"({
            "anti_aliasing": {"mode": "taa"},
            "post_process": {"bloomIntensity": 0.2, "bloom_intensity": 0.2, "vignette": true},
            "ssao": {"radius": 2}
        })");
        const render_settings_reading reading = read_render_settings(document.root());
        CHECK(reading.errors.empty());
        REQUIRE(reading.unknownKeys.size() == 3);
        CHECK(reading.unknownKeys[0] == "rendering/defaults/anti_aliasing");
        // Only the snake_case spelling is a key; the reflection (camelCase) spelling is unknown here.
        CHECK(reading.unknownKeys[1] == "rendering/defaults/post_process/bloomIntensity");
        CHECK(reading.unknownKeys[2] == "rendering/defaults/post_process/vignette");
        CHECK(reading.settings.postProcess.bloomIntensity == 0.2f);
        CHECK(reading.settings.ssao.radius == 2.0f);
        CHECK(capture.count(core::log_level::ERR) == 0);
        CHECK(capture.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("read_render_settings - enumerators are read without regard to ASCII case") {
        const std::string_view spellings[] = {"aces_fitted", "ACES_FITTED", "Aces_Fitted", "aCeS_fItTeD"};
        for (const std::string_view spelling : spellings) {
            CAPTURE(spelling);
            core::json::document document = core::json::document::object();
            REQUIRE(document.set_pointer("/post_process/tonemap", spelling));
            REQUIRE(document.set_pointer("/post_process/exposure", "MANUAL"));
            REQUIRE(document.set_pointer("/shadows/default_filter", "Pcss"));
            const render_settings_reading reading = read_render_settings(document.root());
            CHECK(reading.errors.empty());
            CHECK(reading.settings.postProcess.tonemap == tonemap_operator::ACES_FITTED);
            CHECK(reading.settings.postProcess.exposure == exposure_mode::MANUAL);
            CHECK(reading.settings.shadows.defaultFilter == shadow_filter::PCSS);
        }
        // Case folding does not make a near miss match.
        const core::json::document typo = parse_json(R"({"post_process": {"tonemap": "aces fitted"}})");
        CHECK(read_render_settings(typo.root()).errors.size() == 1);
    }

    TEST_CASE("read_render_settings - a defaults value that is not an object is an error at rendering/defaults") {
        const std::string_view documents[] = {R"([])", R"("defaults")", R"(3)", R"(true)"};
        for (const std::string_view text : documents) {
            CAPTURE(text);
            const core::json::document document = parse_json(text);
            const render_settings_reading reading = read_render_settings(document.root());
            REQUIRE(reading.errors.size() == 1);
            CHECK(reading.errors[0].path == "rendering/defaults");
            CHECK(reading.errors[0].reason.starts_with("expected an object, got "));
            CHECK(reading.settings == render_settings());
        }
    }

    TEST_CASE("render_settings_from_json - the strict form fails with INVALID_SETTINGS naming the first error") {
        SUBCASE("valid with unknown keys") {
            const core::json::document document = parse_json(R"({"ssao": {"radius": 2, "quality": "high"}})");
            const core::result<render_settings> settings = render_settings_from_json(document.root());
            CONJURE_REQUIRE_OK(settings);
            CHECK(settings.value().ssao.radius == 2.0f);
        }
        SUBCASE("null") {
            const core::result<render_settings> settings = render_settings_from_json(core::json::value());
            CONJURE_REQUIRE_OK(settings);
            CHECK(settings.value() == render_settings());
        }
        SUBCASE("invalid") {
            const core::json::document document = parse_json(
              R"({"shadows": {"distance": 0.5}, "ssao": {"radius": 100}})"
            );
            const core::result<render_settings> settings = render_settings_from_json(document.root());
            CONJURE_CHECK_ERROR(settings, renderer_error::INVALID_SETTINGS);
            REQUIRE_FALSE(settings.has_value());
            CHECK(settings.get_error().message() == "rendering/defaults/shadows/distance: 0.5 is outside [1, 10000]");
        }
        SUBCASE("not an object") {
            const core::json::document document = parse_json("[1]");
            const core::result<render_settings> settings = render_settings_from_json(document.root());
            REQUIRE_FALSE(settings.has_value());
            CHECK(settings.get_error().message() == "rendering/defaults: expected an object, got array");
        }
    }

    TEST_CASE("render_settings_to_json - writes every key in project.json form and reads back to the same settings") {
        const render_settings settings = tuned_settings();
        const core::json::document document = render_settings_to_json(settings);
        CHECK(document.root()["post_process"]["tonemap"].as_string() == "agx");
        CHECK(document.root()["post_process"]["agx_look"].as_string() == "punchy");
        CHECK(document.root()["post_process"]["exposure"].as_string() == "manual");
        CHECK(document.root()["shadows"]["default_filter"].as_string() == "pcss");
        CHECK(document.root()["shadows"]["cascade_count"].as_u64() == 3);
        CHECK_FALSE(document.root()["ssao"]["enabled"].as_bool(true));
        const core::result<render_settings> restored = render_settings_from_json(document.root());
        CONJURE_REQUIRE_OK(restored);
        CHECK(restored.value() == settings);
        const core::result<render_settings> defaults = render_settings_from_json(
          render_settings_to_json(render_settings()).root()
        );
        CONJURE_REQUIRE_OK(defaults);
        CHECK(defaults.value() == render_settings());
    }

    TEST_CASE("render_settings_to_json - an enumerator outside its enum is written as its number") {
        render_settings settings = {};
        settings.postProcess.tonemap = static_cast<tonemap_operator>(9);
        const core::json::document document = render_settings_to_json(settings);
        CHECK(document.root()["post_process"]["tonemap"].as_u64() == 9);
        CHECK(read_render_settings(document.root()).errors.size() == 1);
    }

    TEST_CASE("render_settings - the project.json keys are the pinned list") {
        const std::vector<std::string> postProcessKeys = {"manual_ev100", "exposure_compensation",
          "adaptation_speed_up", "adaptation_speed_down", "min_ev100", "max_ev100", "bloom_intensity",
          "reinhard_white_point", "exposure", "tonemap", "agx_look", "bloom_enabled"};
        const std::vector<std::string> shadowKeys = {"distance", "split_lambda", "caster_padding", "pcf_radius_texels",
          "normal_offset_texels", "cascade_count", "default_filter", "enabled"};
        const std::vector<std::string> ssaoKeys = {"radius", "intensity", "thickness", "enabled"};
        const core::json::document document = render_settings_to_json(render_settings());
        CHECK(member_keys(document.root()) == std::vector<std::string>{"post_process", "shadows", "ssao"});
        CHECK(member_keys(document.root()["post_process"]) == postProcessKeys);
        CHECK(member_keys(document.root()["shadows"]) == shadowKeys);
        CHECK(member_keys(document.root()["ssao"]) == ssaoKeys);

        // The list is the reflected fields in snake_case, so a new field cannot be missed by the reader.
        const std::array<const std::vector<std::string> *, 3> pinned = {&postProcessKeys, &shadowKeys, &ssaoKeys};
        const std::array<group_case, 3> groups = every_group();
        for (usize index = 0; index < groups.size(); ++index) {
            CAPTURE(index);
            REQUIRE(groups[index].type != nullptr);
            std::vector<std::string> reflected = {};
            for (const core::reflect::field_info &field : groups[index].type->fields) {
                reflected.push_back(snake_key(field.name));
            }
            CHECK(reflected == *pinned[index]);
        }
    }

    TEST_CASE("render_setting_leaf_to_json - one snake_case leaf with a lower-case enumerator") {
        SUBCASE("a float") {
            const core::json::document value = parse_json("0.06");
            const core::json::document leaf = render_setting_leaf_to_json(
              render_settings_group::POST_PROCESS,
              "bloomIntensity",
              value.root()
            );
            CHECK(leaf.to_canonical_string() == R"({"post_process":{"bloom_intensity":0.06}})");
        }
        SUBCASE("an enumerator") {
            const core::json::document value = parse_json(R"("ACES_FITTED")");
            const core::json::document leaf = render_setting_leaf_to_json(
              render_settings_group::POST_PROCESS,
              "tonemap",
              value.root()
            );
            CHECK(leaf.to_canonical_string() == R"({"post_process":{"tonemap":"aces_fitted"}})");
        }
        SUBCASE("a whole number and a bool") {
            const core::json::document count = parse_json("2");
            CHECK(
              render_setting_leaf_to_json(render_settings_group::SHADOWS, "cascadeCount", count.root())
                .to_canonical_string() == R"({"shadows":{"cascade_count":2}})"
            );
            const core::json::document flag = parse_json("false");
            CHECK(
              render_setting_leaf_to_json(render_settings_group::SSAO, "enabled", flag.root()).to_canonical_string() ==
              R"({"ssao":{"enabled":false}})"
            );
        }
        SUBCASE("null and an invalid value give a null leaf, the merge patch that removes the key") {
            const core::json::document value = parse_json("null");
            CHECK(
              render_setting_leaf_to_json(render_settings_group::SSAO, "radius", value.root()).to_canonical_string() ==
              R"({"ssao":{"radius":null}})"
            );
            CHECK(
              render_setting_leaf_to_json(render_settings_group::SHADOWS, "defaultFilter", core::json::value())
                .to_canonical_string() == R"({"shadows":{"default_filter":null}})"
            );
        }
        SUBCASE("the leaf is accepted by the reader after a merge") {
            const core::json::document value = parse_json(R"("PCSS")");
            core::json::document defaults = parse_json(R"({"shadows": {"distance": 40}})");
            REQUIRE(defaults.merge_patch(
              render_setting_leaf_to_json(render_settings_group::SHADOWS, "defaultFilter", value.root())
            ));
            const core::result<render_settings> settings = render_settings_from_json(defaults.root());
            CONJURE_REQUIRE_OK(settings);
            CHECK(settings.value().shadows.defaultFilter == shadow_filter::PCSS);
            CHECK(settings.value().shadows.distance == 40.0f);
        }
        SUBCASE("an unknown field or group asserts and gives an empty object") {
            const core::json::document value = parse_json("1");
            core::json::document leaf = core::json::document::object();
            CONJURE_CHECK_ASSERTS(
              leaf = render_setting_leaf_to_json(render_settings_group::SSAO, "bloomIntensity", value.root())
            );
            CHECK(leaf.to_canonical_string() == "{}");
            CONJURE_CHECK_ASSERTS(
              leaf = render_setting_leaf_to_json(render_settings_group::COUNT, "radius", value.root())
            );
            CHECK(leaf.to_canonical_string() == "{}");
        }
    }

    TEST_CASE("render_defaults - set sanitises and bumps the revision") {
        render_defaults defaults = {};
        CHECK(defaults.revision() == 0);
        CHECK(defaults.get() == render_settings());

        const render_settings tuned = tuned_settings();
        defaults.set(tuned);
        CHECK(defaults.revision() == 1);
        CHECK(defaults.get() == tuned);

        // Setting the same value still bumps the revision.
        defaults.set(tuned);
        CHECK(defaults.revision() == 2);

        render_settings invalid = tuned;
        invalid.postProcess.bloomIntensity = 5.0f;
        invalid.shadows.defaultFilter = shadow_filter::DEFAULT;
        invalid.ssao.radius = std::numeric_limits<f32>::quiet_NaN();
        defaults.set(invalid);
        CHECK(defaults.revision() == 3);
        CHECK(defaults.get().postProcess.bloomIntensity == MAX_BLOOM_INTENSITY);
        CHECK(defaults.get().shadows.defaultFilter == shadow_filter::PCF);
        CHECK(defaults.get().ssao.radius == ssao_settings().radius);
        CHECK(defaults.get().shadows.distance == tuned.shadows.distance);
    }

    TEST_CASE("render_settings - to_string names every operator, mode, look and group") {
        CHECK(to_string(tonemap_operator::ACES_FITTED) == "ACES_FITTED");
        CHECK(to_string(tonemap_operator::REINHARD_EXTENDED) == "REINHARD_EXTENDED");
        CHECK(to_string(tonemap_operator::AGX) == "AGX");
        CHECK(to_string(tonemap_operator::NONE) == "NONE");
        CHECK(to_string(exposure_mode::AUTOMATIC) == "AUTOMATIC");
        CHECK(to_string(exposure_mode::MANUAL) == "MANUAL");
        CHECK(to_string(agx_look::DEFAULT) == "DEFAULT");
        CHECK(to_string(agx_look::GOLDEN) == "GOLDEN");
        CHECK(to_string(agx_look::PUNCHY) == "PUNCHY");
        CHECK(to_string(render_settings_group::POST_PROCESS) == "post_process");
        CHECK(to_string(render_settings_group::SHADOWS) == "shadows");
        CHECK(to_string(render_settings_group::SSAO) == "ssao");
        CHECK(to_string(render_settings_group::COUNT) == "");
        CHECK(to_string(static_cast<render_settings_group>(99)) == "");
        CHECK(to_string(static_cast<agx_look>(99)) == "DEFAULT");
        CHECK_FALSE(to_string(static_cast<tonemap_operator>(99)).empty());
        CHECK_FALSE(to_string(static_cast<exposure_mode>(99)).empty());
    }

    TEST_CASE("view_settings_override_flags - the groups are single bits with bit 2 free") {
        CHECK(static_cast<u8>(view_settings_override_flags::NONE) == 0);
        CHECK(static_cast<u8>(view_settings_override_flags::POST_PROCESS) == 1);
        CHECK(static_cast<u8>(view_settings_override_flags::SSAO) == 2);
        const view_settings_override_flags both =
          view_settings_override_flags::POST_PROCESS | view_settings_override_flags::SSAO;
        CHECK(has_flag(both, view_settings_override_flags::POST_PROCESS));
        CHECK(has_flag(both, view_settings_override_flags::SSAO));
        CHECK((static_cast<u8>(both) & 0x4u) == 0);
    }
}
