#include "conjure/testing/doctest.h"

#include "conjure/audio/audio_error.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"

#include <array>
#include <string>

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr std::array<audio_error, 18> EVERY_CODE = {
        audio_error::BACKEND_INIT_FAILED, audio_error::DEVICE_NOT_FOUND, audio_error::DEVICE_START_FAILED,
        audio_error::DEVICE_LOST, audio_error::UNSUPPORTED_FORMAT, audio_error::DECODE_FAILED,
        audio_error::INVALID_CLIP, audio_error::CLIP_NOT_LOADED, audio_error::VOICE_LIMIT_REACHED,
        audio_error::INVALID_VOICE, audio_error::STREAM_SLOT_LIMIT, audio_error::INVALID_GROUP,
        audio_error::GROUP_LIMIT_REACHED, audio_error::GROUP_CYCLE, audio_error::DUPLICATE_GROUP,
        audio_error::COMMAND_QUEUE_FULL, audio_error::INVALID_SETTINGS, audio_error::NOT_INITIALISED,
    };
}

TEST_SUITE("audio/audio_error") {

    TEST_CASE("audio_error - every code has a non-empty, unique name") {
        for (usize index = 0; index < EVERY_CODE.size(); ++index) {
            CAPTURE(index);
            const std::string_view name = audio_error_name(EVERY_CODE[index]);
            CHECK_FALSE(name.empty());
            for (usize other = index + 1; other < EVERY_CODE.size(); ++other) {
                CHECK(name != audio_error_name(EVERY_CODE[other]));
            }
        }
        CHECK(audio_error_name(audio_error::DECODE_FAILED) == "DECODE_FAILED");
        CHECK(audio_error_name(audio_error::BACKEND_INIT_FAILED) == "BACKEND_INIT_FAILED");
        CHECK(audio_error_name(static_cast<audio_error>(999)).empty());
    }

    TEST_CASE("audio_error - the first code is 1 so a zero code never means an audio error") {
        CHECK(static_cast<u16>(audio_error::BACKEND_INIT_FAILED) == 1);
        CHECK(static_cast<u16>(audio_error::NOT_INITIALISED) == EVERY_CODE.size());
    }

    TEST_CASE("audio_error - make_error produces the audio category") {
        const core::error failure = core::make_error(audio_error::VOICE_LIMIT_REACHED, "256 voices busy");
        CHECK(failure.category_name() == "audio");
        CHECK(failure.code() == static_cast<u32>(audio_error::VOICE_LIMIT_REACHED));
        CHECK(failure.code_name() == "VOICE_LIMIT_REACHED");
        CHECK(failure.message() == "256 voices busy");
        CHECK(failure.to_string() == "audio.VOICE_LIMIT_REACHED: 256 voices busy");
        CHECK(failure.is_category<audio_error>());
        CHECK_FALSE(failure.is_category<core::error_code>());
    }

    TEST_CASE("audio_error - is() matches exactly one code") {
        const core::error failure = core::make_error(audio_error::INVALID_GROUP, "group {} unknown", 7);
        CHECK(failure.is(audio_error::INVALID_GROUP));
        CHECK_FALSE(failure.is(audio_error::INVALID_VOICE));
        CHECK_FALSE(failure.is(core::error_code::INVALID_ARGUMENT));
        CHECK(failure.message() == "group 7 unknown");
    }

    TEST_CASE("audio_error - results carry audio errors through the test macros") {
        const core::result<int> failed = core::make_error(audio_error::CLIP_NOT_LOADED);
        CONJURE_CHECK_ERROR(failed, audio_error::CLIP_NOT_LOADED);
        CHECK(failed.get_error().message().empty());
    }
}
