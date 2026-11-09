#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/audio/audio_backend.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/null_backend.h"

#include <memory>

using namespace conjure;
using namespace conjure::audio;

namespace {
    void counting_callback(void *user, f32 *output, u32 frameCount) noexcept {
        u32 *calls = static_cast<u32 *>(user);
        ++*calls;
        for (u32 sample = 0; sample < frameCount * MIX_CHANNELS; ++sample) {
            output[sample] = 0.0f;
        }
    }
}

TEST_SUITE("audio/audio_backend") {

    TEST_CASE("create_backend - NULL_BACKEND always succeeds and starts stopped") {
        core::result<std::unique_ptr<audio_backend>> created = create_backend(audio_backend_kind::NULL_BACKEND);
        CONJURE_REQUIRE_OK(created);
        audio_backend &backend = *created.value();
        CHECK(backend.kind() == audio_backend_kind::NULL_BACKEND);
        CHECK(backend.state() == device_state::STOPPED);
        CHECK(is_backend_compiled(audio_backend_kind::NULL_BACKEND));
        CHECK(is_backend_compiled(audio_backend_kind::AUTO));
    }

    TEST_CASE("create_backend - AUTO never fails and resolves to a concrete kind") {
        core::result<std::unique_ptr<audio_backend>> created = create_backend(audio_backend_kind::AUTO);
        CONJURE_REQUIRE_OK(created);
        const audio_backend_kind kind = created.value()->kind();
        CHECK(kind != audio_backend_kind::AUTO);
        CHECK((kind == audio_backend_kind::MINIAUDIO || kind == audio_backend_kind::NULL_BACKEND));
        if (is_backend_compiled(audio_backend_kind::MINIAUDIO)) {
            CHECK(kind == audio_backend_kind::MINIAUDIO);
        } else {
            CHECK(kind == audio_backend_kind::NULL_BACKEND);
        }
    }

    TEST_CASE("create_backend - MINIAUDIO is reported when it is compiled in") {
        core::result<std::unique_ptr<audio_backend>> created = create_backend(audio_backend_kind::MINIAUDIO);
        if (is_backend_compiled(audio_backend_kind::MINIAUDIO)) {
            CONJURE_REQUIRE_OK(created);
            CHECK(created.value()->kind() == audio_backend_kind::MINIAUDIO);
            CHECK(created.value()->state() == device_state::STOPPED);
        } else {
            CONJURE_CHECK_ERROR(created, core::error_code::UNSUPPORTED);
        }
    }

    TEST_CASE("audio_backend - start with a null callback fails with INVALID_SETTINGS") {
        null_backend backend = {};
        backend_desc desc = {};
        CONJURE_CHECK_ERROR(backend.start(desc), audio_error::INVALID_SETTINGS);
        CHECK(backend.state() == device_state::STOPPED);
    }

    TEST_CASE("audio_backend - start with an out-of-range sample rate fails with INVALID_SETTINGS") {
        null_backend backend = {};
        u32 calls = 0;
        backend_desc desc = {};
        desc.callback = &counting_callback;
        desc.user = &calls;
        desc.format.sampleRate = 1000;
        CONJURE_CHECK_ERROR(backend.start(desc), audio_error::INVALID_SETTINGS);
        desc.format.sampleRate = 500000;
        CONJURE_CHECK_ERROR(backend.start(desc), audio_error::INVALID_SETTINGS);
        CHECK(backend.state() == device_state::STOPPED);
    }

    TEST_CASE("audio_backend - state goes STOPPED, RUNNING, STOPPED and stop is idempotent") {
        null_backend backend = {};
        u32 calls = 0;
        backend_desc desc = {};
        desc.callback = &counting_callback;
        desc.user = &calls;
        CHECK(backend.state() == device_state::STOPPED);
        CONJURE_REQUIRE_OK(backend.start(desc));
        CHECK(backend.state() == device_state::RUNNING);
        CHECK(backend.output_format().sampleRate == 48000);
        CHECK(backend.output_format().channels == MIX_CHANNELS);
        backend.update(0.01f);
        CHECK(calls > 0);
        backend.stop();
        CHECK(backend.state() == device_state::STOPPED);
        backend.stop();
        CHECK(backend.state() == device_state::STOPPED);
        const u32 callsAfterStop = calls;
        backend.update(0.01f);
        CHECK(calls == callsAfterStop);
    }

    TEST_CASE("audio_backend - restarting resets the virtual clock") {
        null_backend backend = {};
        u32 calls = 0;
        backend_desc desc = {};
        desc.callback = &counting_callback;
        desc.user = &calls;
        CONJURE_REQUIRE_OK(backend.start(desc));
        backend.update(0.5f);
        CHECK(backend.virtual_time() == doctest::Approx(0.5));
        CONJURE_REQUIRE_OK(backend.start(desc));
        CHECK(backend.virtual_time() == doctest::Approx(0.0));
        CHECK(backend.state() == device_state::RUNNING);
    }

    TEST_CASE("audio_backend - the interface is used polymorphically through a unique_ptr") {
        core::result<std::unique_ptr<audio_backend>> created = create_backend(audio_backend_kind::NULL_BACKEND);
        CONJURE_REQUIRE_OK(created);
        std::unique_ptr<audio_backend> backend = std::move(created).value();
        u32 calls = 0;
        backend_desc desc = {};
        desc.callback = &counting_callback;
        desc.user = &calls;
        desc.format.sampleRate = 44100;
        CONJURE_REQUIRE_OK(backend->start(desc));
        backend->update(1.0f);
        CHECK(backend->output_format().sampleRate == 44100);
        CHECK(calls == (44100 + MIX_BLOCK_FRAMES - 1) / MIX_BLOCK_FRAMES);
        backend.reset();
    }
}
