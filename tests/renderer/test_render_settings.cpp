#include "conjure/testing/doctest.h"

#include "conjure/renderer/limits.h"
#include "conjure/renderer/render_settings.h"

#include "conjure/core/types.h"

#include <string_view>

// render_settings.h (docs/design/02_renderer.md 2.3, 3.4, 3.7, 3.8): the per-scene settings with
// the design's defaults.

using namespace conjure;
using namespace conjure::renderer;

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
        CHECK(settings.agxLook == doctest::Approx(0.0f));
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

    TEST_CASE("render_settings - to_string names every operator and exposure mode") {
        CHECK(to_string(tonemap_operator::ACES_FITTED) == "ACES_FITTED");
        CHECK(to_string(tonemap_operator::REINHARD_EXTENDED) == "REINHARD_EXTENDED");
        CHECK(to_string(tonemap_operator::AGX) == "AGX");
        CHECK(to_string(tonemap_operator::NONE) == "NONE");
        CHECK(to_string(exposure_mode::AUTOMATIC) == "AUTOMATIC");
        CHECK(to_string(exposure_mode::MANUAL) == "MANUAL");
        CHECK_FALSE(to_string(static_cast<tonemap_operator>(99)).empty());
        CHECK_FALSE(to_string(static_cast<exposure_mode>(99)).empty());
    }
}
