#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/audio/audio_engine.h"
#include "conjure/audio/audio_types.h"
#include "conjure/audio/clip.h"
#include "conjure/audio/clip_asset.h"
#include "conjure/audio/components.h"
#include "conjure/audio/voice.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "engine/subsystems/audio_components.h"
#include "engine/subsystems/audio_subsystem.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// audio_subsystem (docs/design/06_engine_loop.md 3.4 step 11, 09 R50/R52): the settings mapping, the
// component registration whatever the enabled flag says, and engine.audio_sync consuming playback requests,
// publishing the listener and clearing stale state on the null backend, and the ownership of component
// voices: every voice a source started is given back when it ends, is replaced or loses its component.

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

    void append_u16(std::vector<std::byte> &out, u16 value) {
        out.push_back(static_cast<std::byte>(value & 0xFFu));
        out.push_back(static_cast<std::byte>(value >> 8));
    }

    void append_u32(std::vector<std::byte> &out, u32 value) {
        append_u16(out, static_cast<u16>(value & 0xFFFFu));
        append_u16(out, static_cast<u16>(value >> 16));
    }

    void append_tag(std::vector<std::byte> &out, std::string_view tag) {
        for (const char character : tag) {
            out.push_back(static_cast<std::byte>(character));
        }
    }

    /// <summary>
    /// A canonical WAV (RIFF + fmt + data): 16-bit PCM, mono, 48 kHz, 4800 frames (0.1 s) of a 440 Hz sine.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> make_wav() {
        constexpr u32 SAMPLE_RATE = 48000;
        constexpr u32 FRAMES = 4800;
        constexpr u32 DATA_BYTES = FRAMES * 2;
        std::vector<std::byte> bytes = {};
        bytes.reserve(44 + DATA_BYTES);
        append_tag(bytes, "RIFF");
        append_u32(bytes, 36 + DATA_BYTES);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32(bytes, 16);
        append_u16(bytes, 1);                   // PCM
        append_u16(bytes, 1);                   // mono
        append_u32(bytes, SAMPLE_RATE);
        append_u32(bytes, SAMPLE_RATE * 2);     // byte rate
        append_u16(bytes, 2);                   // block align
        append_u16(bytes, 16);                  // bits per sample
        append_tag(bytes, "data");
        append_u32(bytes, DATA_BYTES);
        for (u32 frame = 0; frame < FRAMES; ++frame) {
            const f64 phase = static_cast<f64>(frame) * 440.0 * math::TWO_PI / static_cast<f64>(SAMPLE_RATE);
            append_u16(bytes, static_cast<u16>(static_cast<i16>(std::sin(phase) * 16000.0)));
        }
        return bytes;
    }

    /// <summary>
    /// A headless application on the null audio backend with one loaded clip, for the cases that need a
    /// source to really play. Destroyed in the order the asset manager asks for: handle, then application.
    /// </summary>
    struct audio_fixture {
        application app;                            // no default: built from the test configuration
        audio_subsystem *subsystem = nullptr;
        assets::asset_id clipId = core::uuid(0xC11Bu, 0x1u);
        assets::asset_handle clip = {};

        audio_fixture() :
            app(engine_test::make_desc(engine_test::inline_config())) {
            CONJURE_REQUIRE_OK(app.initialise());
            REQUIRE(app.context().audio != nullptr);
            subsystem = find_audio_subsystem(app);
            REQUIRE(subsystem != nullptr);

            const std::vector<std::byte> wav = make_wav();
            const core::result<std::vector<std::byte>> cooked = audio::cook_clip(wav, audio::clip_import_settings{});
            CONJURE_REQUIRE_OK(cooked);
            assets::cooked_blob_header header = {};
            header.type = assets::asset_type_traits<audio::audio_clip>::id();
            header.typeVersion = 1;
            header.importerVersion = 1;
            header.id = clipId;
            std::unique_ptr<assets::memory_asset_source> source = std::make_unique<assets::memory_asset_source>();
            source->add(assets::make_cooked_blob(header, {}, cooked.value()), "assets://sounds/beep.wav");
            app.context().assets->add_source(std::move(source));
            clip = app.context().assets->load(clipId);
            for (u32 frame = 0; frame < 16 && clip.state() != assets::asset_state::LOADED; ++frame) {
                CONJURE_REQUIRE_OK(app.step_frame());
            }
            REQUIRE(clip.state() == assets::asset_state::LOADED);
        }

        ~audio_fixture() {
            // The manager asserts on live handles at shutdown; the sources' references go with the world.
            app.context().world->clear();
            clip = assets::asset_handle{};
            app.shutdown();
        }

        audio_fixture(const audio_fixture &) = delete;
        audio_fixture &operator=(const audio_fixture &) = delete;

        /// <summary>
        /// A named entity with an audio_source over the clip (non-spatial, playOnStart), adjusted by 'edit'.
        /// </summary>
        template <typename EditFunction>
        [[nodiscard]] ecs::entity add_source(std::string_view name, EditFunction &&edit) {
            const core::result<ecs::entity> entity = app.context().scenes->active().create_entity(
              core::string_id::intern(name)
            );
            CONJURE_REQUIRE_OK(entity);
            audio::audio_source source = {};
            source.clip = app.context().assets->bind<audio::audio_clip>(clipId);
            source.spatial = false;
            source.playOnStart = true;
            edit(source);
            CONJURE_REQUIRE_OK(app.context().world->add<audio::audio_source>(entity.value(), source));
            return entity.value();
        }

        [[nodiscard]] audio::audio_source &source(ecs::entity entity) {
            audio::audio_source *found = app.context().world->get<audio::audio_source>(entity);
            REQUIRE(found != nullptr);
            return *found;
        }
    };
}

TEST_SUITE("engine/audio_subsystem") {

    TEST_CASE("audio_subsystem - make_audio_settings maps the engine configuration one to one") {
        engine_config config = engine_config::defaults();
        config.audio.backend = audio_backend_choice::MINIAUDIO;
        config.audio.sampleRate = 44100;
        config.audio.deviceName = "Speakers";
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

    TEST_CASE("regression - code sweep F144 six hundred replayed component sounds never exhaust the voice table") {
        log_capture logs = {};
        audio_fixture fixture = {};
        audio::audio_engine &engine = *fixture.app.context().audio;
        const ecs::entity speaker = fixture.add_source("speaker", [](audio::audio_source &source) {
            source.playOnStart = false;
            source.loop = true;     // never ends by itself: only the replay can give the voice back
        });

        // Every PLAY replaces a voice that is still sounding. Each one used to stay in the engine's table (a
        // retained voice that was only stopped), so the 513th play found it full and started nothing.
        for (u32 replay = 0; replay < 600; ++replay) {
            CAPTURE(replay);
            fixture.source(speaker).playbackRequest = audio::audio_playback_request::PLAY;
            CONJURE_REQUIRE_OK(fixture.app.step_frame());
            REQUIRE(fixture.subsystem->stats().voicesStarted == 1);
            REQUIRE(engine.is_playing(fixture.source(speaker).handle.voice));
            REQUIRE(fixture.subsystem->owned_voice_count() == 1);
        }
        CHECK(engine.stats().voicesDropped == 0);
        // The replaced voices fade out and go: a handful may still be ramping down, never hundreds.
        for (u32 frame = 0; frame < 8; ++frame) {
            CONJURE_REQUIRE_OK(fixture.app.step_frame());
        }
        CHECK(engine.active_voice_count() == 1);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("regression - code sweep F144 destroying an entity with a looping source releases its voice") {
        log_capture logs = {};
        audio_fixture fixture = {};
        audio::audio_engine &engine = *fixture.app.context().audio;
        ecs::world &world = *fixture.app.context().world;
        const auto looping = [](audio::audio_source &source) {
            source.loop = true;
        };
        const ecs::entity destroyed = fixture.add_source("destroyed", looping);
        const ecs::entity stripped = fixture.add_source("stripped", looping);
        const ecs::entity kept = fixture.add_source("kept", looping);
        CONJURE_REQUIRE_OK(fixture.app.step_frame());
        CONJURE_REQUIRE_OK(fixture.app.step_frame());
        const audio::voice_id destroyedVoice = fixture.source(destroyed).handle.voice;
        const audio::voice_id strippedVoice = fixture.source(stripped).handle.voice;
        const audio::voice_id keptVoice = fixture.source(kept).handle.voice;
        REQUIRE(engine.is_playing(destroyedVoice));
        REQUIRE(engine.is_playing(strippedVoice));
        REQUIRE(engine.is_playing(keptVoice));
        CHECK(fixture.subsystem->owned_voice_count() == 3);

        // The entity goes, or only its component: either way the loop must stop sounding and its entry in
        // the voice table must come back. It used to play on for ever.
        CONJURE_REQUIRE_OK(world.destroy_entity(destroyed));
        CONJURE_REQUIRE_OK(world.remove<audio::audio_source>(stripped));
        CHECK(fixture.subsystem->owned_voice_count() == 1);
        for (u32 frame = 0; frame < 8; ++frame) {
            CONJURE_REQUIRE_OK(fixture.app.step_frame());
        }
        CHECK_FALSE(engine.is_valid(destroyedVoice));
        CHECK_FALSE(engine.is_valid(strippedVoice));
        CHECK(engine.is_playing(keptVoice));
        CHECK(engine.active_voice_count() == 1);

        // world::clear is what a scene restore does to every entity at once.
        world.clear();
        CHECK(fixture.subsystem->owned_voice_count() == 0);
        for (u32 frame = 0; frame < 8; ++frame) {
            CONJURE_REQUIRE_OK(fixture.app.step_frame());
        }
        CHECK_FALSE(engine.is_valid(keptVoice));
        CHECK(engine.active_voice_count() == 0);
    }

    TEST_CASE("audio_subsystem - a finished voice is given back and a one-shot source takes its entity with it") {
        log_capture logs = {};
        audio_fixture fixture = {};
        audio::audio_engine &engine = *fixture.app.context().audio;
        ecs::world &world = *fixture.app.context().world;
        const ecs::entity once = fixture.add_source("once", [](audio::audio_source &source) {
            source.oneShot = true;
        });
        const ecs::entity plain = fixture.add_source("plain", [](audio::audio_source &) {});
        CONJURE_REQUIRE_OK(fixture.app.step_frame());
        const audio::voice_id onceVoice = fixture.source(once).handle.voice;
        const audio::voice_id plainVoice = fixture.source(plain).handle.voice;
        REQUIRE(engine.is_valid(onceVoice));
        REQUIRE(engine.is_valid(plainVoice));

        // The clip lasts 0.1 s: well within these frames both voices end.
        for (u32 frame = 0; frame < 30; ++frame) {
            CONJURE_REQUIRE_OK(fixture.app.step_frame());
        }
        CHECK_FALSE(world.is_alive(once));
        CHECK_FALSE(engine.is_valid(onceVoice));
        // The plain source stays, stopped, with no voice and nothing left in the engine's table for it.
        REQUIRE(world.is_alive(plain));
        CHECK_FALSE(fixture.source(plain).isPlaying);
        CHECK_FALSE(fixture.source(plain).handle.voice.is_valid());
        CHECK(fixture.source(plain).handle.started_once());
        CHECK_FALSE(engine.is_valid(plainVoice));
        CHECK(fixture.subsystem->owned_voice_count() == 0);
        // playOnStart was consumed: the source does not start again by itself.
        CONJURE_REQUIRE_OK(fixture.app.step_frame());
        CHECK(fixture.subsystem->stats().voicesStarted == 0);
    }

    TEST_CASE("audio_subsystem - a copied source gets its own voice and never touches the original's") {
        log_capture logs = {};
        audio_fixture fixture = {};
        audio::audio_engine &engine = *fixture.app.context().audio;
        ecs::world &world = *fixture.app.context().world;
        const ecs::entity original = fixture.add_source("original", [](audio::audio_source &source) {
            source.loop = true;
        });
        CONJURE_REQUIRE_OK(fixture.app.step_frame());
        const audio::voice_id originalVoice = fixture.source(original).handle.voice;
        REQUIRE(engine.is_playing(originalVoice));

        // What entity.duplicate, a prefab made from a playing source and a restored snapshot all produce: the
        // component bytes, voice handle included, on another entity.
        const core::result<ecs::entity> copy = fixture.app.context().scenes->active().create_entity(
          core::string_id("copy")
        );
        CONJURE_REQUIRE_OK(copy);
        CONJURE_REQUIRE_OK(world.add<audio::audio_source>(copy.value(), fixture.source(original)));
        REQUIRE(fixture.source(copy.value()).handle.voice == originalVoice);

        CONJURE_REQUIRE_OK(fixture.app.step_frame());
        const audio::voice_id copyVoice = fixture.source(copy.value()).handle.voice;
        CHECK(copyVoice != originalVoice);
        CHECK(engine.is_playing(copyVoice));
        CHECK(fixture.source(original).handle.voice == originalVoice);
        CHECK(engine.is_playing(originalVoice));
        CHECK(fixture.subsystem->owned_voice_count() == 2);

        // Destroying the copy gives back the copy's voice only.
        CONJURE_REQUIRE_OK(world.destroy_entity(copy.value()));
        for (u32 frame = 0; frame < 8; ++frame) {
            CONJURE_REQUIRE_OK(fixture.app.step_frame());
        }
        CHECK_FALSE(engine.is_valid(copyVoice));
        CHECK(engine.is_playing(originalVoice));
        CHECK(fixture.subsystem->owned_voice_count() == 1);
    }
}
