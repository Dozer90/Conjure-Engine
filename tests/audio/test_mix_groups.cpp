#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "audio_test_support.h"
#include "wav_builder.h"

#include "conjure/audio/audio_engine.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/mix_groups.h"

#include "conjure/core/string_id.h"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::audio;
using namespace conjure::core::literals;

namespace {
    group_desc named(std::string_view name, group_id parent = MASTER_GROUP, f32 gain = 1.0f) {
        group_desc desc = {};
        desc.name = core::string_id(name);
        desc.parent = parent;
        desc.gain = gain;
        return desc;
    }

    std::optional<null_group_stats> group_stats(audio_engine &engine, group_id id) {
        for (const null_group_stats &entry : engine.null_backend_instance()->stats().groups) {
            if (entry.id == id) {
                return entry;
            }
        }
        return std::nullopt;
    }
}

TEST_SUITE("audio/mix_groups") {

    TEST_CASE("mix_groups - clamp_group_gain applies [0, 2] and NaN becomes 1") {
        CHECK(clamp_group_gain(-1.0f) == doctest::Approx(0.0));
        CHECK(clamp_group_gain(0.5f) == doctest::Approx(0.5));
        CHECK(clamp_group_gain(3.0f) == doctest::Approx(2.0));
        CHECK(clamp_group_gain(std::numeric_limits<f32>::quiet_NaN()) == doctest::Approx(1.0));
        CHECK(clamp_group_gain(std::numeric_limits<f32>::infinity()) == doctest::Approx(1.0));
    }

    TEST_CASE("mix_groups - the master group always exists") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const std::optional<group_info> master = engine.group(MASTER_GROUP);
        REQUIRE(master.has_value());
        CHECK(master->id == MASTER_GROUP);
        CHECK(master->name.is_null());
        CHECK_FALSE(master->parent.is_valid());
        CHECK(master->gain == doctest::Approx(1.0));
        CHECK(master->effectiveGain == doctest::Approx(1.0));
        CHECK_FALSE(master->paused);
        CHECK(engine.groups().size() == 1);
        CHECK(engine.find_group(core::string_id()) == MASTER_GROUP);
        CHECK_FALSE(engine.find_group("nothing"_sid).is_valid());
    }

    TEST_CASE("mix_groups - create_group by name with a parent and find_group") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<group_id> music = engine.create_group(named("Music", MASTER_GROUP, 0.5f));
        CONJURE_REQUIRE_OK(music);
        const core::result<group_id> ambience = engine.create_group(named("Ambience", music.value(), 0.5f));
        CONJURE_REQUIRE_OK(ambience);
        CHECK(music.value().is_valid());
        CHECK(music.value() != MASTER_GROUP);
        CHECK(engine.find_group("Music"_sid) == music.value());
        CHECK(engine.find_group("Ambience"_sid) == ambience.value());
        CHECK(engine.groups().size() == 3);

        const std::optional<group_info> info = engine.group(ambience.value());
        REQUIRE(info.has_value());
        CHECK(info->parent == music.value());
        CHECK(info->gain == doctest::Approx(0.5));
        CHECK(info->effectiveGain == doctest::Approx(0.25));

        // Parents may also be named, which is how settings declare them.
        group_desc byName = {};
        byName.name = core::string_id("Voices");
        byName.parentName = core::string_id("Ambience");
        const core::result<group_id> voices = engine.create_group(byName);
        CONJURE_REQUIRE_OK(voices);
        CHECK(engine.group(voices.value())->parent == ambience.value());
        CHECK(engine.group(voices.value())->effectiveGain == doctest::Approx(0.25));
    }

    TEST_CASE("mix_groups - effective gain is the product up the chain and a parent change updates children") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<group_id> music = engine.create_group(named("Music", MASTER_GROUP, 0.5f));
        const core::result<group_id> inner = engine.create_group(named("Inner", music.value(), 0.5f));
        CONJURE_REQUIRE_OK(music);
        CONJURE_REQUIRE_OK(inner);
        engine.set_master_gain(0.5f);
        CHECK(engine.group(inner.value())->effectiveGain == doctest::Approx(0.125));
        CHECK(engine.group(music.value())->effectiveGain == doctest::Approx(0.25));

        engine.set_group_gain(music.value(), 1.0f);
        CHECK(engine.group(music.value())->gain == doctest::Approx(1.0));
        CHECK(engine.group(music.value())->effectiveGain == doctest::Approx(0.5));
        CHECK(engine.group(inner.value())->effectiveGain == doctest::Approx(0.25));

        engine.update(testing::FRAME_SECONDS);
        const std::optional<null_group_stats> innerStats = group_stats(engine, inner.value());
        REQUIRE(innerStats.has_value());
        CHECK(innerStats->gain == doctest::Approx(0.5));
        CHECK(innerStats->effectiveGain == doctest::Approx(0.25));
        CHECK(innerStats->name == "Inner"_sid);
        const std::optional<null_group_stats> masterStats = group_stats(engine, MASTER_GROUP);
        REQUIRE(masterStats.has_value());
        CHECK(masterStats->gain == doctest::Approx(0.5));

        // Gains are clamped like every other API-boundary value.
        engine.set_group_gain(music.value(), 9.0f);
        CHECK(engine.group(music.value())->gain == doctest::Approx(2.0));
    }

    TEST_CASE("mix_groups - a voice at volume 0.5 in Music (0.5) under master (0.5) renders at 0.125") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const core::result<group_id> music = engine.create_group(named("Music", MASTER_GROUP, 0.5f));
        CONJURE_REQUIRE_OK(music);
        engine.set_master_gain(0.5f);

        play_params params = {};
        params.volume = 0.5f;
        params.group = music.value();
        params.loop = true;
        params.retain = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(1000.0, 0.5, 1.0f, 2), params);
        CONJURE_REQUIRE_OK(voice);

        testing::step_frames(engine, 6);   // group gain ramps settle
        backend.set_capture_frames(4800);
        testing::step_frames(engine, 6);
        CHECK(testing::peak_of(backend.captured()) == doctest::Approx(0.125).epsilon(0.01));
        CHECK(engine.voice(voice.value())->effectiveGain == doctest::Approx(0.125));
    }

    TEST_CASE("mix_groups - GROUP_LIMIT_REACHED at the 33rd group") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        for (u32 index = 1; index < MAX_GROUPS; ++index) {
            CAPTURE(index);
            CONJURE_REQUIRE_OK(engine.create_group(named("group" + std::to_string(index))));
        }
        CHECK(engine.groups().size() == MAX_GROUPS);
        CONJURE_CHECK_ERROR(engine.create_group(named("one_too_many")), audio_error::GROUP_LIMIT_REACHED);
    }

    TEST_CASE("mix_groups - DUPLICATE_GROUP for a taken or null name") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        CONJURE_REQUIRE_OK(engine.create_group(named("Music")));
        CONJURE_CHECK_ERROR(engine.create_group(named("Music")), audio_error::DUPLICATE_GROUP);
        group_desc unnamed = {};
        CONJURE_CHECK_ERROR(engine.create_group(unnamed), audio_error::DUPLICATE_GROUP);
    }

    TEST_CASE("mix_groups - INVALID_GROUP for an unknown parent and GROUP_CYCLE beyond the depth limit") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        group_id stale = {};
        stale.index = 5;
        stale.generation = 3;
        CONJURE_CHECK_ERROR(engine.create_group(named("Orphan", stale)), audio_error::INVALID_GROUP);
        group_desc unknownParent = named("Orphan");
        unknownParent.parentName = core::string_id("Missing");
        CONJURE_CHECK_ERROR(engine.create_group(unknownParent), audio_error::INVALID_GROUP);

        group_id parent = MASTER_GROUP;
        for (u32 depth = 1; depth <= MAX_GROUP_DEPTH; ++depth) {
            CAPTURE(depth);
            const core::result<group_id> created = engine.create_group(named("depth" + std::to_string(depth), parent));
            CONJURE_REQUIRE_OK(created);
            parent = created.value();
        }
        CONJURE_CHECK_ERROR(engine.create_group(named("too_deep", parent)), audio_error::GROUP_CYCLE);
    }

    TEST_CASE("mix_groups - remove_group moves children and voices to the parent and retires the id") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<group_id> music = engine.create_group(named("Music", MASTER_GROUP, 0.5f));
        const core::result<group_id> child = engine.create_group(named("Child", music.value(), 0.5f));
        CONJURE_REQUIRE_OK(music);
        CONJURE_REQUIRE_OK(child);

        play_params params = {};
        params.group = music.value();
        params.retain = true;
        params.loop = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.1), params);
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 0).groupIndex == music.value().index);

        CONJURE_REQUIRE_OK(engine.remove_group(music.value()));
        CHECK_FALSE(engine.group(music.value()).has_value());
        CHECK_FALSE(engine.find_group("Music"_sid).is_valid());
        CHECK(engine.group(child.value())->parent == MASTER_GROUP);
        CHECK(engine.group(child.value())->effectiveGain == doctest::Approx(0.5));
        CHECK(engine.voice(voice.value())->group == MASTER_GROUP);
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 0).groupIndex == MASTER_GROUP.index);

        CONJURE_CHECK_ERROR(engine.remove_group(music.value()), audio_error::INVALID_GROUP);
        engine.set_group_gain(music.value(), 0.1f);   // stale: no-op
        engine.set_group(voice.value(), music.value());
        CHECK(engine.voice(voice.value())->group == MASTER_GROUP);

        // The slot is reused with a new generation, so the old id stays stale.
        const core::result<group_id> reused = engine.create_group(named("Reused"));
        CONJURE_REQUIRE_OK(reused);
        CHECK(reused.value().index == music.value().index);
        CHECK(reused.value().generation != music.value().generation);
        CHECK_FALSE(engine.group(music.value()).has_value());
    }

    TEST_CASE("mix_groups - remove_group(MASTER_GROUP) is refused") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        CONJURE_CHECK_ERROR(engine.remove_group(MASTER_GROUP), audio_error::INVALID_GROUP);
        CHECK(engine.groups().size() == 1);
    }

    TEST_CASE("mix_groups - set_group_paused freezes the cursors of its voices and renders silence") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const core::result<group_id> music = engine.create_group(named("Music"));
        CONJURE_REQUIRE_OK(music);
        play_params params = {};
        params.group = music.value();
        params.loop = true;
        params.retain = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        const u64 cursorBefore = engine.voice(voice.value())->cursorFrames;
        CHECK(cursorBefore > 0);

        engine.set_group_paused(music.value(), true);
        CHECK(engine.group(music.value())->paused);
        backend.set_capture_frames(1600);
        testing::step_frames(engine, 3);
        CHECK(engine.voice(voice.value())->cursorFrames == cursorBefore);
        CHECK(testing::peak_of(backend.captured()) == doctest::Approx(0.0));
        CHECK(engine.is_playing(voice.value()));

        engine.set_group_paused(music.value(), false);
        testing::step_frames(engine, 2);
        CHECK(engine.voice(voice.value())->cursorFrames > cursorBefore);
        CHECK(testing::peak_of(backend.captured()) > 0.1f);
    }

    TEST_CASE("mix_groups - groups declared in settings are created with their named parents") {
        testing::log_capture logs = {};
        audio_settings settings = testing::null_settings();
        group_desc child = {};
        child.name = core::string_id("Child");
        child.parentName = core::string_id("Parent");
        child.gain = 0.5f;
        group_desc parent = {};
        parent.name = core::string_id("Parent");
        parent.gain = 0.5f;
        settings.groups = {child, parent};   // Child listed first: creation order follows the tree.
        settings.masterGain = 0.5f;
        CONJURE_REQUIRE_OK(settings.validate());

        audio_engine engine(settings);
        CHECK(engine.last_backend_error() == nullptr);
        CHECK(engine.groups().size() == 3);
        const group_id childId = engine.find_group("Child"_sid);
        REQUIRE(childId.is_valid());
        CHECK(engine.group(childId)->parent == engine.find_group("Parent"_sid));
        CHECK(engine.group(childId)->effectiveGain == doctest::Approx(0.125));
        CHECK(engine.group(MASTER_GROUP)->gain == doctest::Approx(0.5));
    }
}
