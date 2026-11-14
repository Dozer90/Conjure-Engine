#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO

#include "conjure/audio/audio_engine.h"
#include "conjure/audio/components.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/system.h"

#include <string_view>
#include <vector>

// Startup step 11 (docs/design/06_engine_loop.md 3.4, 09 R50/R52): the audio engine (miniaudio or null; a
// device failure falls back to null with a WARN inside the engine) mapped one-to-one from
// engine::audio_settings, the audio component types registered into the ECS (even when audio is disabled, so
// scenes with sources still load) and the clip asset type. Owns two systems:
//   engine.audio_sync (SYNC, MAIN_THREAD | RUNS_WHILE_PAUSED): consumes playbackRequest, starts playOnStart
//     sources once, mirrors authored fields and world positions into the voices, writes isPlaying and the
//     listener;
//   engine.audio_finish_one_shots (FRAME_END, MAIN_THREAD): releases the voices that ended this frame,
//     clears isPlaying and destroys the entities of oneShot sources.
//
// Voice ownership (code sweep F144). A component voice is retained: the audio engine keeps it until
// audio_engine::release, so the subsystem is what gives it back. It records which voice each entity owns
// and releases it when the voice ends, when a PLAY request replaces it and, through a REMOVED observer on
// audio_source, when the component or its entity goes (scene unload and the editor's play.stop restore
// included), so a destroyed emitter stops sounding and the voice table never fills up. The record, not the
// component, is the truth: source_handle is plain bytes that a duplicated entity or a restored snapshot
// carries along, and a component whose handle names a voice its entity does not own is a copy. Such a
// component starts over as a fresh one (its own voice, playOnStart applies) and never touches the
// original's voice.

namespace conjure::engine {

    struct audio_sync_stats {
        u32 voicesStarted = 0;
        u32 requestsHandled = 0;
        u32 oneShotsFinished = 0;
        u32 positionsPushed = 0;
        bool listenerSeen = false;
    };

    class audio_subsystem final : public engine_subsystem {
        public:
            static constexpr std::string_view SYSTEM_SYNC = "engine.audio_sync";
            static constexpr std::string_view SYSTEM_FINISH_ONE_SHOTS = "engine.audio_finish_one_shots";

        private:
            engine_services *mServices = nullptr;
            audio_sync_stats mStats = {};
            std::vector<audio::voice_transform> mPositions = {};
            core::flat_hash_map<ecs::entity, audio::voice_id> mVoices = {};    // the voice each source entity owns
            std::vector<ecs::observer_id> mObservers = {};                     // on the world, until shutdown
            bool mEnabled = false;

        public:
            explicit audio_subsystem(engine_services &services);
            ~audio_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void register_systems(ecs::scheduler &scheduler, engine_context &context) override;
            void begin_frame(engine_context &context) override;
            void end_frame(engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// Counters of the last frame's sync pass (tests, debug overlay).
            /// </summary>
            [[nodiscard]] const audio_sync_stats &stats() const noexcept {
                return mStats;
            }

            /// <summary>
            /// Voices the subsystem currently holds for audio_source components: one per entity whose source
            /// has started and not yet ended. Everything else in the engine's voice table was played directly.
            /// </summary>
            [[nodiscard]] u32 owned_voice_count() const noexcept {
                return static_cast<u32>(mVoices.size());
            }

            /// <summary>
            /// audio::audio_settings built from the engine configuration (R52): NULL_DEVICE maps to
            /// NULL_BACKEND, headless always picks the null backend, groups keep their parent names.
            /// </summary>
            [[nodiscard]] static audio::audio_settings make_audio_settings(
              const engine_config &config,
              core::jobs::job_system *jobs
            );

        private:
            void sync(ecs::system_context &context);
            void finish_one_shots(ecs::system_context &context);
            void start_voice(ecs::entity entity, audio::audio_source &source);

            /// <summary>
            /// Resets a component whose handle does not name the voice its entity owns (a copy, a restored
            /// snapshot, an overwritten component) to a fresh one, and gives back a voice the entity owns
            /// that the component no longer names.
            /// </summary>
            void adopt_or_reset(ecs::entity entity, audio::audio_source &source);

            /// <summary>
            /// Releases the voice the entity owns, if any, and forgets it. Safe inside an ECS observer: it
            /// touches the audio engine and this subsystem only.
            /// </summary>
            void release_voice_of(ecs::entity entity);
            [[nodiscard]] audio::group_id group_of(const audio::audio_source &source) const noexcept;
    };
}

#endif
