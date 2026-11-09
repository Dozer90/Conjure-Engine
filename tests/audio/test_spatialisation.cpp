#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/audio/spatialisation.h"

#include "conjure/core/math.h"

#include <array>
#include <limits>

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr std::array<attenuation_model, 4> EVERY_MODEL = {
        attenuation_model::NONE, attenuation_model::INVERSE, attenuation_model::LINEAR, attenuation_model::EXPONENTIAL,
    };

    attenuation_params params_for(attenuation_model model, f32 rolloff = 1.0f) {
        attenuation_params params = {};
        params.model = model;
        params.minDistance = 1.0f;
        params.maxDistance = 100.0f;
        params.rolloff = rolloff;
        return params;
    }
}

TEST_SUITE("audio/spatialisation") {

    TEST_CASE("listener_state - defaults look down -Z with +Y up") {
        const listener_state listener = {};
        CONJURE_CHECK_APPROX(listener.position, math::vec3(0.0f));
        CONJURE_CHECK_APPROX(listener.forward, math::vec3(0.0f, 0.0f, -1.0f));
        CONJURE_CHECK_APPROX(listener.up, math::vec3(0.0f, 1.0f, 0.0f));
        CONJURE_CHECK_APPROX(listener.velocity, math::vec3(0.0f));
        CHECK(listener.gain == doctest::Approx(1.0));
        CHECK(spatial_result{} == spatial_result{});
    }

    TEST_CASE("compute_attenuation - every model is 1 at minDistance and below") {
        for (usize index = 0; index < EVERY_MODEL.size(); ++index) {
            CAPTURE(index);
            const attenuation_params params = params_for(EVERY_MODEL[index]);
            CHECK(compute_attenuation(params, 1.0f) == doctest::Approx(1.0));
            CHECK(compute_attenuation(params, 0.0f) == doctest::Approx(1.0));
            CHECK(compute_attenuation(params, 0.5f) == doctest::Approx(1.0));
        }
    }

    TEST_CASE("compute_attenuation - LINEAR reaches 0 at maxDistance and NONE stays at 1") {
        CHECK(compute_attenuation(params_for(attenuation_model::LINEAR), 100.0f) == doctest::Approx(0.0));
        CHECK(compute_attenuation(params_for(attenuation_model::LINEAR), 50.5f) == doctest::Approx(0.5));
        CHECK(compute_attenuation(params_for(attenuation_model::NONE), 100.0f) == doctest::Approx(1.0));
        CHECK(compute_attenuation(params_for(attenuation_model::NONE), 1.0e6f) == doctest::Approx(1.0));
    }

    TEST_CASE("compute_attenuation - INVERSE and EXPONENTIAL are monotonic and match their formulas") {
        const attenuation_params inverse = params_for(attenuation_model::INVERSE);
        const attenuation_params exponential = params_for(attenuation_model::EXPONENTIAL);
        f32 previousInverse = 1.0f;
        f32 previousExponential = 1.0f;
        for (u32 step = 1; step <= 100; ++step) {
            CAPTURE(step);
            const f32 distance = static_cast<f32>(step);
            const f32 gainInverse = compute_attenuation(inverse, distance);
            const f32 gainExponential = compute_attenuation(exponential, distance);
            CHECK(gainInverse <= previousInverse + 1.0e-6f);
            CHECK(gainExponential <= previousExponential + 1.0e-6f);
            CHECK(gainInverse == doctest::Approx(static_cast<f64>(1.0f / (1.0f + (distance - 1.0f)))));
            CHECK(gainExponential == doctest::Approx(static_cast<f64>(1.0f / distance)));
            previousInverse = gainInverse;
            previousExponential = gainExponential;
        }
        CHECK(compute_attenuation(inverse, 100.0f) == doctest::Approx(0.01));
    }

    TEST_CASE("compute_attenuation - distances beyond maxDistance clamp to the value at maxDistance") {
        for (usize index = 0; index < EVERY_MODEL.size(); ++index) {
            CAPTURE(index);
            const attenuation_params params = params_for(EVERY_MODEL[index]);
            const f64 atMax = static_cast<f64>(compute_attenuation(params, 100.0f));
            CHECK(compute_attenuation(params, 1000.0f) == doctest::Approx(atMax));
            CHECK(compute_attenuation(params, std::numeric_limits<f32>::infinity()) == doctest::Approx(atMax));
            CHECK(compute_attenuation(params, std::numeric_limits<f32>::quiet_NaN()) == doctest::Approx(atMax));
        }
    }

    TEST_CASE("compute_attenuation - rolloff 0 gives 1 everywhere") {
        for (usize index = 0; index < EVERY_MODEL.size(); ++index) {
            CAPTURE(index);
            const attenuation_params params = params_for(EVERY_MODEL[index], 0.0f);
            CHECK(compute_attenuation(params, 50.0f) == doctest::Approx(1.0));
            CHECK(compute_attenuation(params, 100.0f) == doctest::Approx(1.0));
        }
    }

    TEST_CASE("compute_attenuation - a steeper rolloff attenuates more") {
        const f32 gentle = compute_attenuation(params_for(attenuation_model::INVERSE, 0.5f), 10.0f);
        const f32 steep = compute_attenuation(params_for(attenuation_model::INVERSE, 4.0f), 10.0f);
        CHECK(steep < gentle);
        CHECK(compute_attenuation(params_for(attenuation_model::LINEAR, 2.0f), 50.5f) == doctest::Approx(0.0));
    }

    TEST_CASE("clamp_attenuation - applies the A15 rules and replaces NaN with the defaults") {
        attenuation_params params = {};
        params.minDistance = -5.0f;
        params.maxDistance = 0.001f;
        params.rolloff = 99.0f;
        const attenuation_params clamped = clamp_attenuation(params);
        CHECK(clamped.minDistance == doctest::Approx(0.01));
        CHECK(clamped.maxDistance == doctest::Approx(0.01));
        CHECK(clamped.rolloff == doctest::Approx(10.0));

        params.minDistance = std::numeric_limits<f32>::quiet_NaN();
        params.maxDistance = std::numeric_limits<f32>::quiet_NaN();
        params.rolloff = std::numeric_limits<f32>::quiet_NaN();
        const attenuation_params sanitised = clamp_attenuation(params);
        CHECK(sanitised.minDistance == doctest::Approx(1.0));
        CHECK(sanitised.maxDistance == doctest::Approx(100.0));
        CHECK(sanitised.rolloff == doctest::Approx(1.0));
    }

    TEST_CASE("compute_pan - right is +1, left is -1, ahead and behind are 0") {
        const listener_state listener = {};
        CHECK(compute_pan(listener, math::vec3(5.0f, 0.0f, 0.0f)) == doctest::Approx(1.0));
        CHECK(compute_pan(listener, math::vec3(-5.0f, 0.0f, 0.0f)) == doctest::Approx(-1.0));
        CHECK(compute_pan(listener, math::vec3(0.0f, 0.0f, -5.0f)) == doctest::Approx(0.0));
        CHECK(compute_pan(listener, math::vec3(0.0f, 0.0f, 5.0f)) == doctest::Approx(0.0));
        CHECK(compute_pan(listener, math::vec3(0.0f, 3.0f, 0.0f)) == doctest::Approx(0.0));
        CHECK(compute_pan(listener, math::vec3(1.0f, 0.0f, -1.0f)) == doctest::Approx(0.70710678));
    }

    TEST_CASE("compute_pan - a coincident source is centred") {
        const listener_state listener = {};
        CHECK(compute_pan(listener, listener.position) == doctest::Approx(0.0));
        CHECK(compute_pan(listener, math::vec3(std::numeric_limits<f32>::quiet_NaN())) == doctest::Approx(0.0));
    }

    TEST_CASE("compute_pan - a listener rotated 90 degrees hears the world turned") {
        listener_state listener = {};
        listener.forward = math::vec3(1.0f, 0.0f, 0.0f);   // Facing +X: right is +Z.
        CHECK(compute_pan(listener, math::vec3(0.0f, 0.0f, 5.0f)) == doctest::Approx(1.0));
        CHECK(compute_pan(listener, math::vec3(0.0f, 0.0f, -5.0f)) == doctest::Approx(-1.0));
        CHECK(compute_pan(listener, math::vec3(5.0f, 0.0f, 0.0f)) == doctest::Approx(0.0));

        listener.position = math::vec3(10.0f, 0.0f, 10.0f);
        CHECK(compute_pan(listener, math::vec3(10.0f, 0.0f, 20.0f)) == doctest::Approx(1.0));
    }

    TEST_CASE("compute_doppler_ratio - approaching raises the pitch and receding lowers it") {
        const listener_state listener = {};
        const math::vec3 ahead = math::vec3(0.0f, 0.0f, -10.0f);
        const math::vec3 towards = math::vec3(0.0f, 0.0f, 34.3f);    // Moving +Z, towards the listener.
        const math::vec3 away = math::vec3(0.0f, 0.0f, -34.3f);
        const f32 approaching = compute_doppler_ratio(listener, ahead, towards, 1.0f, DEFAULT_SPEED_OF_SOUND);
        const f32 receding = compute_doppler_ratio(listener, ahead, away, 1.0f, DEFAULT_SPEED_OF_SOUND);
        CHECK(approaching > 1.0f);
        CHECK(receding < 1.0f);
        CHECK(approaching == doctest::Approx(1.0 / 0.9).epsilon(1.0e-3));
        CHECK(receding == doctest::Approx(1.0 / 1.1).epsilon(1.0e-3));
    }

    TEST_CASE("compute_doppler_ratio - a factor of 0 disables the effect") {
        const listener_state listener = {};
        const f32 ratio = compute_doppler_ratio(
          listener,
          math::vec3(0.0f, 0.0f, -10.0f),
          math::vec3(0.0f, 0.0f, 100.0f),
          0.0f,
          DEFAULT_SPEED_OF_SOUND
        );
        CHECK(ratio == doctest::Approx(1.0));
        const f32 coincident = compute_doppler_ratio(listener, listener.position, math::vec3(50.0f), 1.0f, 343.0f);
        CHECK(coincident == doctest::Approx(1.0));
    }

    TEST_CASE("compute_doppler_ratio - the ratio is clamped to [0.5, 2]") {
        const listener_state listener = {};
        const math::vec3 ahead = math::vec3(0.0f, 0.0f, -10.0f);
        const f32 extremeUp = compute_doppler_ratio(listener, ahead, math::vec3(0.0f, 0.0f, 330.0f), 10.0f, 343.0f);
        const f32 extremeDown = compute_doppler_ratio(listener, ahead, math::vec3(0.0f, 0.0f, -3000.0f), 10.0f, 343.0f);
        CHECK(extremeUp == doctest::Approx(2.0));
        CHECK(extremeDown == doctest::Approx(0.5));
    }

    TEST_CASE("compute_doppler_ratio - listener motion counts too") {
        listener_state listener = {};
        listener.velocity = math::vec3(0.0f, 0.0f, -34.3f);   // Moving towards a source ahead.
        const math::vec3 ahead = math::vec3(0.0f, 0.0f, -10.0f);
        const f32 ratio = compute_doppler_ratio(listener, ahead, math::vec3(0.0f), 1.0f, 343.0f);
        CHECK(ratio == doctest::Approx(1.1).epsilon(1.0e-3));
    }

    TEST_CASE("compute_spatial - distance 0 is full gain and centred, maxDistance is the floor") {
        const listener_state listener = {};
        const attenuation_params params = params_for(attenuation_model::INVERSE);
        const math::vec3 still = math::vec3(0.0f);
        const spatial_result atListener = compute_spatial(listener, listener.position, still, params, 0.0f, 343.0f);
        CHECK(atListener.gain == doctest::Approx(1.0));
        CHECK(atListener.pan == doctest::Approx(0.0));
        CHECK(atListener.pitchRatio == doctest::Approx(1.0));

        const math::vec3 farRight = math::vec3(100.0f, 0.0f, 0.0f);
        const spatial_result far = compute_spatial(listener, farRight, still, params, 0.0f, 343.0f);
        CHECK(far.gain == doctest::Approx(0.01));
        CHECK(far.pan == doctest::Approx(1.0));

        const math::vec3 farLeft = math::vec3(-5000.0f, 0.0f, 0.0f);
        const spatial_result beyond = compute_spatial(listener, farLeft, still, params, 0.0f, 343.0f);
        CHECK(beyond.gain == doctest::Approx(0.01));
        CHECK(beyond.pan == doctest::Approx(-1.0));
    }

    TEST_CASE("compute_spatial - a non-finite position counts as the listener position") {
        const listener_state listener = {};
        const attenuation_params params = params_for(attenuation_model::LINEAR);
        const spatial_result result = compute_spatial(
          listener,
          math::vec3(std::numeric_limits<f32>::infinity()),
          math::vec3(0.0f),
          params,
          1.0f,
          343.0f
        );
        CHECK(result.gain == doctest::Approx(1.0));
        CHECK(result.pan == doctest::Approx(0.0));
        CHECK(result.pitchRatio == doctest::Approx(1.0));
    }

    TEST_CASE("clamp_listener - normalises the basis and sanitises garbage") {
        listener_state listener = {};
        listener.forward = math::vec3(0.0f, 0.0f, -4.0f);
        listener.up = math::vec3(0.0f, 1.0f, -0.5f);   // Tilted towards forward: re-orthogonalised.
        listener.gain = 7.0f;
        const listener_state clamped = clamp_listener(listener);
        CONJURE_CHECK_APPROX(clamped.forward, math::vec3(0.0f, 0.0f, -1.0f));
        CONJURE_CHECK_APPROX(clamped.up, math::vec3(0.0f, 1.0f, 0.0f));
        CHECK(clamped.gain == doctest::Approx(2.0));

        listener_state broken = {};
        broken.position = math::vec3(std::numeric_limits<f32>::quiet_NaN());
        broken.forward = math::vec3(0.0f);
        broken.up = math::vec3(0.0f, 0.0f, -1.0f);   // Parallel to the default forward.
        broken.gain = std::numeric_limits<f32>::quiet_NaN();
        const listener_state sanitised = clamp_listener(broken);
        CONJURE_CHECK_APPROX(sanitised.position, math::vec3(0.0f));
        CONJURE_CHECK_APPROX(sanitised.forward, math::vec3(0.0f, 0.0f, -1.0f));
        CHECK(glm::length(sanitised.up) == doctest::Approx(1.0));
        CHECK(glm::dot(sanitised.up, sanitised.forward) == doctest::Approx(0.0));
        CHECK(sanitised.gain == doctest::Approx(1.0));
    }
}
