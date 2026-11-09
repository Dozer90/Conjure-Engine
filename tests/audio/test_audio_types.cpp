#include "conjure/testing/doctest.h"

#include "conjure/audio/audio_types.h"
#include "conjure/audio/mix_groups.h"
#include "conjure/audio/voice.h"

#include <format>
#include <string>
#include <type_traits>

using namespace conjure;
using namespace conjure::audio;

TEST_SUITE("audio/audio_types") {

    TEST_CASE("audio_types - constants match the design") {
        CHECK(MAX_VOICES == 256);
        CHECK(MAX_GROUPS == 32);
        CHECK(MAX_GROUP_DEPTH == 8);
        CHECK(MAX_STREAMING_VOICES == 32);
        CHECK(MIX_CHANNELS == 2);
        CHECK(MIX_BLOCK_FRAMES == 256);
        CHECK(STREAM_RING_FRAMES == 16384);
        CHECK(STREAM_CHUNK_FRAMES == 4096);
        CHECK(COMMAND_RING_CAPACITY == 8192);
        CHECK(DEFAULT_RAMP_SECONDS == doctest::Approx(0.005));
        CHECK(DEFAULT_SPEED_OF_SOUND == doctest::Approx(343.0));
        CHECK(STREAM_RING_FRAMES > STREAM_CHUNK_FRAMES * 2);
    }

    TEST_CASE("audio_format - defaults and equality") {
        const audio_format defaults = {};
        CHECK(defaults.sampleRate == 48000);
        CHECK(defaults.channels == MIX_CHANNELS);
        CHECK(defaults.format == sample_format::F32);

        audio_format other = {};
        CHECK(other == defaults);
        other.sampleRate = 44100;
        CHECK(other != defaults);
        other.sampleRate = 48000;
        other.channels = 1;
        CHECK(other != defaults);
    }

    TEST_CASE("audio_types - to_string names every enumerator") {
        CHECK(to_string(sample_format::F32) == "F32");

        CHECK(to_string(clip_storage::PCM) == "PCM");
        CHECK(to_string(clip_storage::STREAMED) == "STREAMED");

        CHECK(to_string(clip_encoding::UNKNOWN) == "UNKNOWN");
        CHECK(to_string(clip_encoding::WAV) == "WAV");
        CHECK(to_string(clip_encoding::OGG_VORBIS) == "OGG_VORBIS");
        CHECK(to_string(clip_encoding::MP3) == "MP3");
        CHECK(to_string(clip_encoding::FLAC) == "FLAC");

        CHECK(to_string(audio_backend_kind::AUTO) == "AUTO");
        CHECK(to_string(audio_backend_kind::MINIAUDIO) == "MINIAUDIO");
        CHECK(to_string(audio_backend_kind::NULL_BACKEND) == "NULL_BACKEND");

        CHECK(to_string(device_state::STOPPED) == "STOPPED");
        CHECK(to_string(device_state::STARTING) == "STARTING");
        CHECK(to_string(device_state::RUNNING) == "RUNNING");
        CHECK(to_string(device_state::LOST) == "LOST");

        CHECK(to_string(voice_state::FREE) == "FREE");
        CHECK(to_string(voice_state::PRIMING) == "PRIMING");
        CHECK(to_string(voice_state::PLAYING) == "PLAYING");
        CHECK(to_string(voice_state::PAUSED) == "PAUSED");
        CHECK(to_string(voice_state::STOPPING) == "STOPPING");
        CHECK(to_string(voice_state::FINISHED) == "FINISHED");
        CHECK(to_string(voice_state::STOPPED) == "STOPPED");

        CHECK(to_string(attenuation_model::NONE) == "NONE");
        CHECK(to_string(attenuation_model::INVERSE) == "INVERSE");
        CHECK(to_string(attenuation_model::LINEAR) == "LINEAR");
        CHECK(to_string(attenuation_model::EXPONENTIAL) == "EXPONENTIAL");
    }

    TEST_CASE("audio_types - out-of-range values print as UNKNOWN") {
        CHECK(to_string(static_cast<voice_state>(200)) == "UNKNOWN");
        CHECK(to_string(static_cast<clip_storage>(7)) == "UNKNOWN");
        CHECK(to_string(static_cast<device_state>(99)) == "UNKNOWN");
    }

    TEST_CASE("audio_types - enumerators format through std::format") {
        CHECK(std::format("{}", voice_state::PLAYING) == "PLAYING");
        CHECK(std::format("{}", audio_backend_kind::NULL_BACKEND) == "NULL_BACKEND");
        CHECK(std::format("{:>6}", clip_storage::PCM) == "   PCM");
    }

    TEST_CASE("audio_types - active and terminal state predicates") {
        CHECK(is_active_state(voice_state::PRIMING));
        CHECK(is_active_state(voice_state::PLAYING));
        CHECK(is_active_state(voice_state::PAUSED));
        CHECK_FALSE(is_active_state(voice_state::STOPPING));
        CHECK_FALSE(is_active_state(voice_state::FINISHED));
        CHECK_FALSE(is_active_state(voice_state::FREE));

        CHECK(is_terminal_state(voice_state::FINISHED));
        CHECK(is_terminal_state(voice_state::STOPPED));
        CHECK_FALSE(is_terminal_state(voice_state::STOPPING));
        CHECK_FALSE(is_terminal_state(voice_state::FREE));
    }

    TEST_CASE("voice_id - null, valid and to_u64 round trip") {
        const voice_id null = {};
        CHECK_FALSE(null.is_valid());
        CHECK(null == voice_id::null());
        CHECK(null.to_u64() == 0);

        voice_id valid = {};
        valid.key.index = 17;
        valid.key.generation = 3;
        CHECK(valid.is_valid());
        CHECK(valid != null);
        CHECK(voice_id::from_u64(valid.to_u64()) == valid);
        CHECK(valid.to_u64() == ((u64{3} << 32) | 17));
        CHECK(sizeof(voice_id) == 8);
        CHECK(std::is_trivially_copyable_v<voice_id>);
    }

    TEST_CASE("group_id - MASTER_GROUP is valid and the null id is not") {
        CHECK(MASTER_GROUP.is_valid());
        CHECK(MASTER_GROUP.index == 0);
        CHECK(MASTER_GROUP.generation == 1);
        const group_id null = {};
        CHECK_FALSE(null.is_valid());
        CHECK(null != MASTER_GROUP);
        CHECK(sizeof(group_id) == 2);
    }
}
