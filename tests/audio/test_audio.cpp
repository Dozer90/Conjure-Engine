#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/audio/audio.h"

using namespace conjure;
using namespace conjure::audio;

TEST_SUITE("audio/audio") {

    TEST_CASE("audio - the umbrella header exposes every public type") {
        // Compilation of this file is the test; a few spot checks keep it meaningful.
        const audio_format format = {};
        const play_params params = {};
        const listener_state listener = {};
        const clip_import_settings settings = {};
        const backend_desc desc = {};
        const audio_listener listenerComponent = {};
        CHECK(format.sampleRate == 48000);
        CHECK(params.group == MASTER_GROUP);
        CHECK(listener.gain == doctest::Approx(1.0));
        CHECK(settings.storage == clip_storage_setting::AUTO);
        CHECK(desc.periodFrames == 480);
        CHECK(listenerComponent.active);
        CHECK(audio_error_name(audio_error::INVALID_CLIP) == "INVALID_CLIP");
    }

    TEST_CASE("audio - an engine built through the umbrella runs headless") {
        testing::log_capture logs = {};
        audio_settings settings = {};
        settings.backend = audio_backend_kind::NULL_BACKEND;
        audio_engine engine(settings);
        CHECK(engine.backend_kind() == audio_backend_kind::NULL_BACKEND);
        CHECK(engine.null_backend_instance() != nullptr);
        engine.update(1.0f / 60.0f);
        engine.end_frame();
        CHECK(engine.stats().framesRendered == 800);
    }
}
