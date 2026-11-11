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
//   engine.audio_finish_one_shots (FRAME_END, MAIN_THREAD): clears isPlaying of the voices that ended this
//     frame and destroys the entities of oneShot sources.

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
            void start_voice(audio::audio_source &source);
            [[nodiscard]] audio::group_id group_of(const audio::audio_source &source) const noexcept;
    };
}

#endif
