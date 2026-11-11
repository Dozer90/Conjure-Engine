#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/audio/audio_engine.h"
#include "conjure/audio/audio_types.h"
#include "conjure/audio/components.h"
#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "engine/subsystems/audio_components.h"
#include "engine/subsystems/audio_subsystem.h"

#include <memory>
#include <string>

// audio_subsystem (docs/design/06_engine_loop.md 3.4 step 11, 09 R50/R52): the settings mapping, the
// component registration whatever the enabled flag says, and engine.audio_sync consuming playback requests,
// publishing the listener and clearing stale state on the null backend.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] audio_subsystem *find_audio_subsystem(application &app) {
        for (const std::unique_ptr<engine_subsystem> &subsystem : app.subsystems()) {
            if (subsystem->name() == "audio") {
                return dynamic_cast<audio_subsystem *>(subsystem.get());
            }
        }
        return nullptr;
    }
}

TEST_SUITE("engine/audio_subsystem") {

    TEST_CASE("audio_subsystem - make_audio_settings maps the engine configuration one to one") {
        engine_config config = engine_config::defaults();
        config.audio.backend = audio_backend_choice::MINIAUDIO;
        config.audio.sampleRate = 44100;
        config.audio.device = "Speakers";
        config.audio.masterGain = 0.5f;
        config.audio.dopplerFactor = 1.0f;
        config.audio.speedOfSound = 340.0f;
        audio_group_setting music = {};
        music.name = "music";
        music.gain = 0.8f;
        audio_group_setting voice = {};
        voice.name = "voice";
        voice.parent = "music";
        voice.gain = 0.9f;
        config.audio.groups = {music, voice};

        const audio::audio_settings settings = audio_subsystem::make_audio_settings(config, nullptr);
        CHECK(settings.backend == audio::audio_backend_kind::MINIAUDIO);
        CHECK(settings.sampleRate == 44100);
        CHECK(settings.deviceName == "Speakers");
        CHECK(settings.masterGain == 0.5f);
        CHECK(settings.dopplerFactor == 1.0f);
        CHECK(settings.speedOfSound == 340.0f);
        CHECK(settings.jobs == nullptr);
        REQUIRE(settings.groups.size() == 2);
        CHECK(settings.groups[0].name == core::string_id("music"));
        CHECK(settings.groups[0].parentName.is_null());
        CHECK(settings.groups[1].parentName == core::string_id("music"));
        CHECK(settings.groups[1].gain == 0.9f);

        config.audio.backend = audio_backend_choice::NULL_DEVICE;
        CHECK(audio_subsystem::make_audio_settings(config, nullptr).backend == audio::audio_backend_kind::NULL_BACKEND);
        config.audio.backend = audio_backend_choice::AUTO;
        CHECK(audio_subsystem::make_audio_settings(config, nullptr).backend == audio::audio_backend_kind::AUTO);
        config.headless = true;
        CHECK(audio_subsystem::make_audio_settings(config, nullptr).backend == audio::audio_backend_kind::NULL_BACKEND);
    }

    TEST_CASE("audio_subsystem - components and the clip type are registered even when audio is disabled") {
        engine_config config = engine_test::inline_config();
        config.audio.enabled = false;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.context().audio == nullptr);
        CHECK(app.context().components->find(core::string_id(audio::AUDIO_SOURCE_TYPE_NAME)).is_valid());
        CHECK(app.context().components->find(core::string_id(audio::AUDIO_LISTENER_TYPE_NAME)).is_valid());
        CHECK(app.context().assets->types().find("conjure.audio.clip") != nullptr);
        app.shutdown();
    }

    TEST_CASE("audio_subsystem - the null backend engine runs and the sync pass handles sources and listeners") {
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        audio_group_setting music = {};
        music.name = "music";
        config.audio.groups = {music};
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().audio != nullptr);
        CHECK(app.context().audio->backend_kind() == audio::audio_backend_kind::NULL_BACKEND);
        CHECK(app.context().audio->find_group(core::string_id("music")).is_valid());
        audio_subsystem *subsystem = find_audio_subsystem(app);
        REQUIRE(subsystem != nullptr);

        scene::scene &active = app.context().scenes->active();
        ecs::world &world = *app.context().world;
        const core::result<ecs::entity> listener = active.create_entity(core::string_id("ears"));
        CONJURE_REQUIRE_OK(listener);
        CONJURE_REQUIRE_OK(world.add<audio::audio_listener>(listener.value()));
        const core::result<ecs::entity> speaker = active.create_entity(core::string_id("speaker"));
        CONJURE_REQUIRE_OK(speaker);
        audio::audio_source source = {};
        source.playOnStart = true;
        source.oneShot = true;
        source.group = core::string_id("music");
        source.playbackRequest = audio::audio_playback_request::PLAY;
        source.isPlaying = true;   // stale state as a copied component would carry it
        CONJURE_REQUIRE_OK(world.add<audio::audio_source>(speaker.value(), source));

        CONJURE_REQUIRE_OK(app.step_frame());
        const audio::audio_source *synced = world.get<audio::audio_source>(speaker.value());
        REQUIRE(synced != nullptr);
        // No clip is loaded: the request is consumed, nothing plays, the stale flag is cleared.
        CHECK(synced->playbackRequest == audio::audio_playback_request::NONE);
        CHECK_FALSE(synced->isPlaying);
        CHECK_FALSE(synced->handle.voice.is_valid());
        CHECK(subsystem->stats().requestsHandled == 1);
        CHECK(subsystem->stats().voicesStarted == 0);
        CHECK(subsystem->stats().listenerSeen);
        CHECK(subsystem->stats().oneShotsFinished == 0);
        CHECK(world.is_alive(speaker.value()));

        // Later frames keep retrying playOnStart silently and keep the listener published.
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->stats().requestsHandled == 0);
        CHECK(subsystem->stats().listenerSeen);
        CHECK(app.context().audio->active_voice_count() == 0);
        app.shutdown();
    }
}
