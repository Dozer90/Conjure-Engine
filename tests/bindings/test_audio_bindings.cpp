#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/audio/audio_engine.h"
#include "conjure/audio/clip.h"
#include "conjure/audio/clip_asset.h"
#include "conjure/audio/components.h"
#include "conjure/audio/null_backend.h"
#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/marshal.h"
#include "conjure/core/hash.h"
#include "conjure/core/string_id.h"
#include "conjure/engine/application.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "engine/subsystems/audio_components.h"

#include <memory>
#include <vector>

// audio_* (docs/design/15_bindings.md 10, B8, R50): the entity entries write playbackRequest, engine.audio_sync
// turns it into voices (asserted through the headless application), one-shots need a resident clip, group
// and master gains round-trip and clip_duration reads the payload.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    struct audio_fixture {
        bare_fixture base;
        assets::asset_id clip = {};

        audio_fixture() :
            base(make_options()) {
            clip = base.publish_clip(3, "assets://audio/tone.wav");
        }

        [[nodiscard]] static bare_fixture_options make_options() noexcept {
            bare_fixture_options options = {};
            options.audio = true;
            return options;
        }

        [[nodiscard]] ecs::entity spawn_source() {
            const ecs::entity entity = base.create("speaker");
            audio::audio_source source = {};
            source.playOnStart = false;
            source.spatial = false;
            source.clip = base.assets.bind<audio::audio_clip>(clip);
            CONJURE_REQUIRE_OK(base.world.add<audio::audio_source>(entity, source));
            return entity;
        }
    };
}

TEST_SUITE("bindings/audio_bindings") {

    TEST_CASE("audio - play sets the request and the engine's sync starts a voice") {
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        REQUIRE(app->context().audio != nullptr);
        REQUIRE(app->context().scripting != nullptr);
        conjure_native_api &api = app->context().scripting->native_api();

        // The clip lands in a secondary memory source of the application's manager.
        std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
        const std::vector<std::byte> wav = make_test_wav();
        const core::result<std::vector<std::byte>> cooked = audio::cook_clip(wav, audio::clip_import_settings{});
        CONJURE_REQUIRE_OK(cooked);
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<audio::audio_clip>::id();
        header.typeVersion = 1;
        header.importerVersion = 1;
        header.id = test_uuid(3);
        owned->add(assets::make_cooked_blob(header, {}, cooked.value()), "assets://audio/tone.wav");
        app->context().assets->add_source(std::move(owned));
        {
            // The handle may not outlive the manager: pinning keeps the clip resident instead.
            const core::result<assets::asset_handle> loaded = app->context().assets->load_blocking(test_uuid(3));
            CONJURE_REQUIRE_OK(loaded);
            app->context().assets->pin(test_uuid(3), true);
        }

        const core::result<ecs::entity> speaker =
          app->context().scenes->active().create_entity(core::string_id("speaker"));
        CONJURE_REQUIRE_OK(speaker);
        audio::audio_source source = {};
        source.playOnStart = false;
        source.spatial = false;
        source.clip = app->context().assets->bind<audio::audio_clip>(test_uuid(3));
        CONJURE_REQUIRE_OK(app->context().world->add<audio::audio_source>(speaker.value(), source));

        CHECK(api.audio_is_playing(speaker.value().to_u64()) == 0);
        CHECK(api.audio_play(speaker.value().to_u64()) == 0);
        CHECK(
          app->context().world->get<audio::audio_source>(speaker.value())->playbackRequest
          == audio::audio_playback_request::PLAY
        );
        CONJURE_REQUIRE_OK(app->step_frame());
        const audio::audio_source *synced = app->context().world->get<audio::audio_source>(speaker.value());
        CHECK(synced->playbackRequest == audio::audio_playback_request::NONE);
        CHECK(synced->isPlaying);
        CHECK(api.audio_is_playing(speaker.value().to_u64()) == 1);
        CHECK(app->context().audio->active_voice_count() == 1);

        CHECK(api.audio_stop(speaker.value().to_u64()) == 0);
        CONJURE_REQUIRE_OK(app->step_frame());
        CONJURE_REQUIRE_OK(app->step_frame());
        CHECK(api.audio_is_playing(speaker.value().to_u64()) == 0);
        app->shutdown();
    }

    TEST_CASE("audio - stop, pause and resume write their requests") {
        audio_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity speaker = fixture.spawn_source();
        audio::audio_source *source = fixture.base.world.get<audio::audio_source>(speaker);
        CHECK(api.audio_play(speaker.to_u64()) == 0);
        CHECK(source->playbackRequest == audio::audio_playback_request::PLAY);
        CHECK(api.audio_pause(speaker.to_u64()) == 0);
        CHECK(source->playbackRequest == audio::audio_playback_request::PAUSE);
        CHECK(api.audio_resume(speaker.to_u64()) == 0);
        CHECK(source->playbackRequest == audio::audio_playback_request::RESUME);
        CHECK(api.audio_stop(speaker.to_u64()) == 0);
        CHECK(source->playbackRequest == audio::audio_playback_request::STOP);
        CHECK(api.audio_play(999) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(api.audio_stop(0) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("audio - is_playing reflects the previous sync") {
        audio_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity speaker = fixture.spawn_source();
        CHECK(api.audio_is_playing(speaker.to_u64()) == 0);
        fixture.base.world.get<audio::audio_source>(speaker)->isPlaying = true;      // what engine.audio_sync writes
        CHECK(api.audio_is_playing(speaker.to_u64()) == 1);
        CHECK(api.audio_is_playing(999) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("audio - a one-shot requires a loaded clip") {
        audio_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid clip = uuid_of(fixture.clip);
        CHECK(api.audio_play_one_shot(&clip, nullptr, 1.0f, 1.0f, 0) == to_status(bindings_error::ASSET_NOT_LOADED));
        fixture.base.load_now(fixture.clip);
        CHECK(api.audio_play_one_shot(&clip, nullptr, 0.8f, 1.0f, 0) == 0);
        CHECK(fixture.base.audio->active_voice_count() == 1);
        const conjure_vec3 position = {1.0f, 0.0f, 0.0f};
        CHECK(api.audio_play_one_shot(&clip, &position, 1.0f, 1.0f, core::hash::fnv1a64("music")) == 0);
        CHECK(fixture.base.audio->active_voice_count() == 2);
        CHECK(
          api.audio_play_one_shot(&clip, nullptr, 1.0f, 1.0f, core::hash::fnv1a64("nope"))
          == to_status(bindings_error::NOT_FOUND)
        );
        CHECK(api.audio_play_one_shot(nullptr, nullptr, 1.0f, 1.0f, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid null = {};
        CHECK(api.audio_play_one_shot(&null, nullptr, 1.0f, 1.0f, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid unknown = uuid_of(test_uuid(80));
        const u32 slotsBefore = fixture.base.assets.stats().slots;
        CHECK(api.audio_play_one_shot(&unknown, nullptr, 1.0f, 1.0f, 0) == to_status(bindings_error::ASSET_NOT_LOADED));
        float seconds = 0.0f;
        CHECK(api.audio_clip_duration(&unknown, &seconds) == to_status(bindings_error::ASSET_NOT_LOADED));
        CHECK(fixture.base.assets.stats().slots == slotsBefore);       // a read never binds a slot (05)
        // The voices are one-shots: the engine releases them when they end.
        for (u32 frame = 0; frame < 12; ++frame) {
            fixture.base.audio->update(1.0f / 60.0f);
            fixture.base.audio->end_frame();
        }
        CHECK(fixture.base.audio->active_voice_count() == 0);
    }

    TEST_CASE("audio - group and master gain roundtrip") {
        audio_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        float gain = 0.0f;
        CHECK(api.audio_get_master_gain(&gain) == 0);
        CHECK(gain == 1.0f);
        CHECK(api.audio_set_master_gain(0.25f) == 0);
        CHECK(api.audio_get_master_gain(&gain) == 0);
        CHECK(gain == 0.25f);
        const uint64_t music = core::hash::fnv1a64("music");
        CHECK(api.audio_get_group_gain(music, &gain) == 0);
        CHECK(gain == 0.5f);
        CHECK(api.audio_set_group_gain(music, 0.75f) == 0);
        CHECK(api.audio_get_group_gain(music, &gain) == 0);
        CHECK(gain == 0.75f);
        CHECK(api.audio_get_group_gain(0, &gain) == 0);                // 0 = master
        CHECK(gain == 0.25f);
        CHECK(api.audio_set_group_gain(core::hash::fnv1a64("nope"), 1.0f) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.audio_get_group_gain(core::hash::fnv1a64("nope"), &gain) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.audio_get_group_gain(music, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.audio_get_master_gain(nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.audio_set_master_gain(std::numeric_limits<float>::quiet_NaN())
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
    }

    TEST_CASE("audio - clip_duration comes from the payload") {
        audio_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid clip = uuid_of(fixture.clip);
        float seconds = 0.0f;
        CHECK(api.audio_clip_duration(&clip, &seconds) == to_status(bindings_error::ASSET_NOT_LOADED));
        fixture.base.load_now(fixture.clip);
        CHECK(api.audio_clip_duration(&clip, &seconds) == 0);
        CONJURE_CHECK_APPROX(seconds, 0.1f);
        CHECK(api.audio_clip_duration(&clip, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.audio_clip_duration(nullptr, &seconds) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("audio - an entity without a source is COMPONENT_MISSING") {
        audio_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity plain = fixture.base.create("plain");
        CHECK(api.audio_play(plain.to_u64()) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(last_error_message() == "audio_play: audio_source");
        CHECK(api.audio_is_playing(plain.to_u64()) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(api.audio_pause(plain.to_u64()) == to_status(bindings_error::COMPONENT_MISSING));
    }

    TEST_CASE("audio - without an audio engine every entry is NOT_SUPPORTED") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        float gain = 0.0f;
        CHECK(api.audio_get_master_gain(&gain) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.audio_play(1) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(last_error_message() == "audio_play: subsystem unavailable");
    }
}
