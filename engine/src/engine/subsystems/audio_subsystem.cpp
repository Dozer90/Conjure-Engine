#include "engine/subsystems/audio_subsystem.h"

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO

#include "engine/subsystems/audio_components.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/log_category.h"

#include "conjure/audio/clip_asset.h"
#include "conjure/audio/spatialisation.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/view.h"
#include "conjure/scene/components.h"

#include <algorithm>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] audio::audio_backend_kind to_backend_kind(audio_backend_choice choice, bool headless) noexcept {
        if (headless) {
            return audio::audio_backend_kind::NULL_BACKEND;
        }
        switch (choice) {
            case audio_backend_choice::AUTO:
                return audio::audio_backend_kind::AUTO;
            case audio_backend_choice::MINIAUDIO:
                return audio::audio_backend_kind::MINIAUDIO;
            case audio_backend_choice::NULL_DEVICE:
                break;
        }
        return audio::audio_backend_kind::NULL_BACKEND;
    }

    [[nodiscard]] math::vec3 position_of(const math::mat4 &matrix) noexcept {
        return math::vec3(matrix[3]);
    }

    [[nodiscard]] math::vec3 direction_of(const math::mat4 &matrix, const math::vec3 &local) noexcept {
        const math::vec3 rotated = math::vec3(matrix * math::vec4(local, 0.0f));
        const f32 length = glm::length(rotated);
        return length > 1.0e-6f ? rotated / length : local;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

audio_subsystem::audio_subsystem(engine_services &services) :
    mServices(&services) {}

audio_subsystem::~audio_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view audio_subsystem::name() const noexcept {
    return "audio";
}

core::result<void> audio_subsystem::startup(engine_context &context) {
    const engine_config &config = *context.config;
    mStats = {};

    // Components and asset types exist whatever the enabled flag says: a scene with sources still loads.
    CONJURE_TRY(mServices->components->register_component<audio::audio_source>());
    CONJURE_TRY(mServices->components->register_component<audio::audio_listener>());
    CONJURE_TRY(audio::register_asset_types(mServices->assetManager->types(), mServices->importers));

    mEnabled = config.audio.enabled;
    if (!mEnabled) {
        CONJURE_LOG_INFO(LOG_ENGINE, "audio: disabled by configuration");
        return core::result<void>::success();
    }

    mServices->audio = std::make_unique<audio::audio_engine>(make_audio_settings(config, &mServices->jobs));
    if (const core::error *failure = mServices->audio->last_backend_error(); failure != nullptr) {
        CONJURE_LOG_WARN(
          LOG_ENGINE,
          "audio: device unavailable, running on the null backend: {}",
          failure->to_string()
        );
    }
    context.audio = mServices->audio.get();
    CONJURE_LOG_INFO(LOG_ENGINE, "audio: {} backend", audio::to_string(mServices->audio->backend_kind()));
    return core::result<void>::success();
}

void audio_subsystem::register_systems(ecs::scheduler &scheduler, engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->audio == nullptr) {
        return;
    }

    using listener = const audio::audio_listener;
    using world_pose = const scene::world_transform;
    ecs::system_desc sync = ecs::make_system<audio::audio_source, listener, world_pose>(
      core::string_id::intern(SYSTEM_SYNC),
      ecs::system_phase::SYNC,
      [this](ecs::system_context &systemContext) {
          this->sync(systemContext);
      }
    );
    sync.flags = ecs::system_flags::MAIN_THREAD | ecs::system_flags::RUNS_WHILE_PAUSED;
    core::result<ecs::system_id> added = scheduler.add_system(std::move(sync));
    if (!added.has_value()) {
        CONJURE_LOG_ERROR(LOG_ENGINE, "audio: {} could not be added: {}", SYSTEM_SYNC, added.get_error().to_string());
    }

    ecs::system_desc finish = ecs::make_system<audio::audio_source>(
      core::string_id::intern(SYSTEM_FINISH_ONE_SHOTS),
      ecs::system_phase::FRAME_END,
      [this](ecs::system_context &systemContext) {
          this->finish_one_shots(systemContext);
      }
    );
    finish.flags = ecs::system_flags::MAIN_THREAD;
    finish.before.push_back(core::string_id::intern("scene.destroy_pending"));
    added = scheduler.add_system(std::move(finish));
    if (!added.has_value()) {
        CONJURE_LOG_ERROR(
          LOG_ENGINE,
          "audio: {} could not be added: {}",
          SYSTEM_FINISH_ONE_SHOTS,
          added.get_error().to_string()
        );
    }
}

void audio_subsystem::begin_frame(engine_context &context) {
    if (mServices->audio == nullptr) {
        return;
    }
    const f32 delta = context.time != nullptr ? context.time->unscaledDeltaTime : 0.0f;
    mServices->audio->update(delta);
}

void audio_subsystem::end_frame(engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->audio != nullptr) {
        mServices->audio->end_frame();
    }
}

void audio_subsystem::shutdown(engine_context &context) {
    context.audio = nullptr;
    mServices->audio.reset();
    mPositions.clear();
    mEnabled = false;
}

audio::audio_settings audio_subsystem::make_audio_settings(const engine_config &config, core::jobs::job_system *jobs) {
    audio::audio_settings settings = {};
    settings.backend = to_backend_kind(config.audio.backend, config.headless);
    settings.sampleRate = config.audio.sampleRate;
    settings.deviceName = config.audio.device;
    settings.masterGain = config.audio.masterGain;
    settings.dopplerFactor = config.audio.dopplerFactor;
    settings.speedOfSound = config.audio.speedOfSound;
    settings.jobs = jobs;
    for (const audio_group_setting &group : config.audio.groups) {
        audio::group_desc desc = {};
        desc.name = core::string_id::intern(group.name);
        if (!group.parent.empty()) {
            desc.parentName = core::string_id::intern(group.parent);
        }
        desc.gain = group.gain;
        settings.groups.push_back(desc);
    }
    return settings;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void audio_subsystem::sync(ecs::system_context &context) {
    audio::audio_engine &engine = *mServices->audio;
    mStats.voicesStarted = 0;
    mStats.requestsHandled = 0;
    mStats.positionsPushed = 0;
    mStats.listenerSeen = false;
    mPositions.clear();

    context.view<audio::audio_source, ecs::maybe<const scene::world_transform>>().for_each(
      [&](ecs::entity, audio::audio_source &source, const scene::world_transform *transform) {
          bool valid = engine.is_valid(source.handle.voice);
          if (!valid && source.isPlaying) {
              // The voice ended (or the component was copied with a dead handle): the state catches up.
              source.isPlaying = false;
              source.handle.voice = {};
          }

          switch (source.playbackRequest) {
              case audio::audio_playback_request::PLAY:
                  if (valid) {
                      engine.stop(source.handle.voice, 0.0f);
                  }
                  start_voice(source);
                  valid = engine.is_valid(source.handle.voice);
                  ++mStats.requestsHandled;
                  break;
              case audio::audio_playback_request::STOP:
                  if (valid) {
                      engine.stop(source.handle.voice);
                  }
                  source.handle.flags |= audio::source_handle::FLAG_STARTED_ONCE;
                  ++mStats.requestsHandled;
                  break;
              case audio::audio_playback_request::PAUSE:
                  if (valid) {
                      engine.pause(source.handle.voice);
                  }
                  ++mStats.requestsHandled;
                  break;
              case audio::audio_playback_request::RESUME:
                  if (valid) {
                      engine.resume(source.handle.voice);
                  }
                  ++mStats.requestsHandled;
                  break;
              case audio::audio_playback_request::NONE:
                  break;
          }
          source.playbackRequest = audio::audio_playback_request::NONE;

          if (!valid && source.playOnStart && !source.handle.started_once()) {
              start_voice(source);
              valid = engine.is_valid(source.handle.voice);
          }

          if (valid) {
              engine.apply_source(source.handle.voice, source, group_of(source));
              if (source.spatial && transform != nullptr) {
                  audio::voice_transform entry = {};
                  entry.voice = source.handle.voice;
                  entry.position = position_of(transform->matrix);
                  mPositions.push_back(entry);
              }
          }
          source.isPlaying = valid && engine.is_playing(source.handle.voice);
      }
    );

    if (!mPositions.empty()) {
        engine.set_positions(mPositions);
        mStats.positionsPushed = static_cast<u32>(mPositions.size());
    }

    context.view<const audio::audio_listener, ecs::maybe<const scene::world_transform>>().for_each(
      [&](ecs::entity, const audio::audio_listener &listener, const scene::world_transform *transform) {
          if (mStats.listenerSeen || !listener.active) {
              return;
          }
          audio::listener_state state = {};
          state.gain = listener.gain;
          if (transform != nullptr) {
              state.position = position_of(transform->matrix);
              state.forward = direction_of(transform->matrix, math::vec3(0.0f, 0.0f, -1.0f));
              state.up = direction_of(transform->matrix, math::vec3(0.0f, 1.0f, 0.0f));
          }
          engine.set_listener(state);
          mStats.listenerSeen = true;
      }
    );
}

void audio_subsystem::finish_one_shots(ecs::system_context &context) {
    audio::audio_engine &engine = *mServices->audio;
    const std::span<const audio::voice_id> finished = engine.finished_voices();
    mStats.oneShotsFinished = 0;
    if (finished.empty()) {
        return;
    }

    ecs::command_buffer &commands = context.commands();
    context.view<audio::audio_source>().for_each([&](ecs::entity entity, audio::audio_source &source) {
        if (!source.handle.voice.is_valid()) {
            return;
        }
        const bool ended = std::find(finished.begin(), finished.end(), source.handle.voice) != finished.end();
        if (!ended) {
            return;
        }
        source.isPlaying = false;
        source.handle.voice = {};
        if (source.oneShot) {
            commands.destroy_entity(entity);
            ++mStats.oneShotsFinished;
        }
    });
}

void audio_subsystem::start_voice(audio::audio_source &source) {
    audio::audio_engine &engine = *mServices->audio;
    audio::play_params params = audio::play_params_from(source, group_of(source));
    core::result<audio::voice_id> started = engine.play(source.clip, params);
    if (!started.has_value()) {
        // Typically CLIP_NOT_LOADED: the clip is still in flight, the next frame tries again.
        return;
    }
    source.handle.voice = started.value();
    source.handle.clipVersion = source.clip.version();
    source.handle.flags |= audio::source_handle::FLAG_STARTED_ONCE;
    ++mStats.voicesStarted;
}

audio::group_id audio_subsystem::group_of(const audio::audio_source &source) const noexcept {
    if (source.group.is_null()) {
        return audio::MASTER_GROUP;
    }
    const audio::group_id found = mServices->audio->find_group(source.group);
    return found.is_valid() ? found : audio::MASTER_GROUP;
}

#endif
