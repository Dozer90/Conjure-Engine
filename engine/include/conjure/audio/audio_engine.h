#pragma once

#include "conjure/audio/audio_backend.h"
#include "conjure/audio/audio_device.h"
#include "conjure/audio/audio_types.h"
#include "conjure/audio/clip.h"
#include "conjure/audio/components.h"
#include "conjure/audio/mix_groups.h"
#include "conjure/audio/null_backend.h"
#include "conjure/audio/spatialisation.h"
#include "conjure/audio/voice.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The audio engine (docs/design/12_audio.md 4.4): voices, groups, listener and backend behind one
// object owned by engine::application. Every member is main-thread only (every mutating one asserts
// core::jobs::is_main_thread()) except is_valid(), is_playing(), voice(), stats() and output_format(),
// which read atomics refreshed by update().

namespace conjure::audio {

    namespace detail {
        struct audio_engine_helpers;
    }

    /// <summary>
    /// Construction settings; validate() mirrors the checks construction performs.
    /// </summary>
    struct CONJURE_API audio_settings {
        audio_backend_kind backend = audio_backend_kind::AUTO;
        u32 sampleRate = 48000;                       // Mix rate; MIN_SAMPLE_RATE..MAX_SAMPLE_RATE
        u32 periodFrames = 480;
        std::string deviceName = {};
        f32 masterGain = 1.0f;
        f32 dopplerFactor = 0.0f;                     // 0 = off; [0, 10]
        f32 speedOfSound = DEFAULT_SPEED_OF_SOUND;
        u32 maxRecoveryAttempts = 5;
        f32 recoveryBackoffSeconds = 1.0f;
        std::vector<group_desc> groups = {};          // Created at construction; parents resolved by name
        core::jobs::job_system *jobs = nullptr;       // nullptr: streaming decodes run inline in update()

        /// <summary>
        /// INVALID_SETTINGS when the sample rate is outside [MIN_SAMPLE_RATE, MAX_SAMPLE_RATE], the
        /// period is 0, a group name is null or duplicated, a parent name is unknown, the group
        /// tree has a cycle or exceeds MAX_GROUP_DEPTH, or more than MAX_GROUPS - 1 groups are listed.
        /// </summary>
        [[nodiscard]] core::result<void> validate() const;
    };

    /// <summary>
    /// One entry of set_positions().
    /// </summary>
    struct voice_transform {
        voice_id voice = {};
        math::vec3 position = math::vec3(0.0f);
        math::vec3 velocity = math::vec3(0.0f);
    };

    /// <summary>
    /// Counters for the profiler; stats() is a lock-free read.
    /// </summary>
    struct audio_engine_stats {
        u32 activeVoices = 0;
        u32 peakVoices = 0;
        u32 streamingVoices = 0;
        u32 voicesDropped = 0;
        u64 commandsSent = 0;
        u64 commandsSpilled = 0;
        u64 eventsReceived = 0;
        u64 eventsDropped = 0;
        u64 renderCalls = 0;
        u64 framesRendered = 0;
        u32 underruns = 0;
        u32 loops = 0;
        u32 deviceRestarts = 0;
        bool fellBackToNull = false;
        audio_format outputFormat = {};
        device_state deviceState = device_state::STOPPED;
    };

    class CONJURE_API audio_engine {
        private:
            friend struct detail::audio_engine_helpers;   // the main-thread algorithms of audio_engine.cpp

            struct state;

            std::unique_ptr<state> mState = nullptr;   // mixer, rings, tables, backend

        public:
            /// <summary>
            /// Never fails: an invalid setting or a backend that cannot start leaves the engine on
            /// the null backend with last_backend_error() set.
            /// </summary>
            explicit audio_engine(audio_settings settings);

            /// <summary>
            /// Stops every voice, stops the backend and drains the acknowledgements.
            /// </summary>
            ~audio_engine();

            audio_engine(const audio_engine &) = delete;
            audio_engine &operator=(const audio_engine &) = delete;

        public:
            /*-------------------------------------------------------------------*/
            // Backend and device

            /// <summary>
            /// The backend in use (never AUTO).
            /// </summary>
            [[nodiscard]] audio_backend_kind backend_kind() const noexcept;

            /// <summary>
            /// Why the requested backend was replaced by null, or nullptr when it was not.
            /// </summary>
            [[nodiscard]] const core::error *last_backend_error() const noexcept;

            /// <summary>
            /// The null backend when it is the one in use, nullptr on miniaudio.
            /// </summary>
            [[nodiscard]] null_backend *null_backend_instance() noexcept;

            /// <summary>
            /// The format the mixer renders at (A2).
            /// </summary>
            [[nodiscard]] audio_format output_format() const noexcept;

            /// <summary>
            /// Playback devices of the current backend.
            /// </summary>
            [[nodiscard]] core::result<std::vector<audio_device_info>> enumerate_devices() const;

            /// <summary>
            /// Restarts the backend on the named device (empty = default). Voices are frozen during
            /// the restart and continue afterwards. On failure the previous device is kept. An
            /// unknown non-empty name falls back to the default device with a WARN (12_audio.md 3.9)
            /// and counts as success. If neither the new nor the previous device starts, the engine
            /// continues on the null backend with last_backend_error() set and stats().fellBackToNull
            /// true; the error of the new device is returned.
            /// </summary>
            [[nodiscard]] core::result<void> select_device(std::string_view deviceName);

            /// <summary>
            /// The settings the engine was constructed from.
            /// </summary>
            [[nodiscard]] const audio_settings &settings() const noexcept;

            /*-------------------------------------------------------------------*/
            // Frame

            /// <summary>
            /// Once per frame: backend update, event drain, spatial pass, streaming pass, spill
            /// flush, stats (docs/design/12_audio.md 3.2). Main thread.
            /// </summary>
            void update(f32 deltaSeconds);

            /// <summary>
            /// Erases every acknowledged one-shot and publishes the ids that ended this frame
            /// through finished_voices(). Main thread.
            /// </summary>
            void end_frame();

            /// <summary>
            /// Voices that reached FINISHED or STOPPED during the last frame, each reported once.
            /// Valid until the next end_frame().
            /// </summary>
            [[nodiscard]] std::span<const voice_id> finished_voices() const noexcept;

            /*-------------------------------------------------------------------*/
            // Voices

            /// <summary>
            /// Starts a voice on a loaded clip asset (CLIP_NOT_LOADED while get() is null), recording
            /// the ref's version for reload detection. Other errors as play(const audio_clip &).
            /// </summary>
            [[nodiscard]] core::result<voice_id> play(
              const assets::asset_ref<audio_clip> &clip,
              const play_params &params = {}
            );

            /// <summary>
            /// Starts a voice on a clip payload, sharing its PCM or encoded bytes (A6). clipVersion
            /// is recorded in voice_info for reload detection. Errors: INVALID_CLIP (clip without
            /// its buffer), INVALID_GROUP (params.group stale or unknown), STREAM_SLOT_LIMIT,
            /// VOICE_LIMIT_REACHED, DECODE_FAILED (streamed clip that does not open; its stream
            /// slot is released again).
            /// </summary>
            [[nodiscard]] core::result<voice_id> play(
              const audio_clip &clip,
              const play_params &params = {},
              u32 clipVersion = 0
            );

            /// <summary>
            /// Starts a voice on raw PCM (tests, tools: no asset manager). INVALID_CLIP for a null
            /// or empty buffer.
            /// </summary>
            [[nodiscard]] core::result<voice_id> play(
              std::shared_ptr<const pcm_buffer> pcm,
              const play_params &params = {}
            );

            /// <summary>
            /// Fades the voice out over fadeSeconds, then it reports STOPPED. No-op for an invalid id
            /// or an already ended voice. A voice that is paused, or whose group is frozen (paused
            /// itself or through an ancestor group, group_info::effectivePaused), or while the master
            /// is paused, ends at once without a fade.
            /// </summary>
            void stop(voice_id voice, f32 fadeSeconds = DEFAULT_RAMP_SECONDS);

            /// <summary>
            /// stop() for every live voice.
            /// </summary>
            void stop_all(f32 fadeSeconds = DEFAULT_RAMP_SECONDS);

            /// <summary>
            /// Holds the cursor (the voice stays is_playing()); no-op on an invalid id or a voice
            /// that is not PLAYING. A PRIMING voice starts paused.
            /// </summary>
            void pause(voice_id voice);

            /// <summary>
            /// Continues a paused voice from its cursor; no-op on an invalid id or a voice that is
            /// not PAUSED. During a pending streamed seek the resume takes effect once it lands. On a
            /// voice still PRIMING it cancels startPaused or an earlier pause(): the voice starts
            /// playing.
            /// </summary>
            void resume(voice_id voice);

            /// <summary>
            /// Gives a retained voice back: stops it if it is still playing and erases it once the
            /// mixer acknowledged. One-shot voices are released by end_frame() instead (calling
            /// release on one is harmless).
            /// </summary>
            void release(voice_id voice);

            /// <summary>
            /// Volume clamped to [0, 2], ramped over DEFAULT_RAMP_SECONDS in the mixer. NaN asserts
            /// and becomes 1. No-op with a TRACE on a stale id. Main thread, like every setter.
            /// </summary>
            void set_volume(voice_id voice, f32 volume);

            /// <summary>
            /// Pitch clamped to [0.25, 4]; the mixer step changes at the next block. NaN asserts
            /// and becomes 1. No-op with a TRACE on a stale id.
            /// </summary>
            void set_pitch(voice_id voice, f32 pitch);

            /// <summary>
            /// Pan clamped to [-1, 1], used only while the voice is not spatial (a spatial voice
            /// pans from its position). NaN asserts and becomes 0. No-op with a TRACE on a stale id.
            /// </summary>
            void set_pan(voice_id voice, f32 pan);

            /// <summary>
            /// Loops at the clip end (streamed voices rewind their decoder). No-op on a stale id.
            /// </summary>
            void set_loop(voice_id voice, bool loop);

            /// <summary>
            /// Switches between flat playback (pan, unity gain) and 3D playback (attenuation and
            /// pan from set_position(); a stereo clip is downmixed to mono, A10). No-op on a stale id.
            /// </summary>
            void set_spatial(voice_id voice, bool spatial);

            /// <summary>
            /// Voice-stealing priority; lower is stolen first. No-op on a stale id.
            /// </summary>
            void set_priority(voice_id voice, u8 priority);

            /// <summary>
            /// Moves the voice to a live group; an invalid or stale group id is a no-op with a
            /// TRACE, as is a stale voice id.
            /// </summary>
            void set_group(voice_id voice, group_id group);

            /// <summary>
            /// World position and velocity of a spatial voice; applied by the next update().
            /// </summary>
            void set_position(voice_id voice, math::vec3 position, math::vec3 velocity = math::vec3(0.0f));

            /// <summary>
            /// set_position() for every entry; one call per frame from engine.audio_sync.
            /// </summary>
            void set_positions(std::span<const voice_transform> transforms);

            /// <summary>
            /// Attenuation model and distances of a spatial voice, clamped per A15 (minDistance
            /// >= 0.01, maxDistance >= minDistance, rolloff in [0, 10]); applied by the next
            /// update(). No-op on a stale id.
            /// </summary>
            void set_attenuation(voice_id voice, const attenuation_params &params);

            /// <summary>
            /// Applies volume, pitch, pan, loop, spatial flag, priority, group and attenuation of
            /// 'params' at once (what apply_source forwards to). An invalid group leaves the
            /// voice's group unchanged.
            /// </summary>
            void apply_params(voice_id voice, const play_params &params);

            /// <summary>
            /// Applies every authored field of an audio_source component to its voice at once
            /// (engine.audio_sync calls it for changed<audio_source> chunks, 12_audio.md 8):
            /// apply_params(voice, play_params_from(source, group)) with 'group' the resolution of
            /// source.group through find_group(). No-op on a stale voice id.
            /// </summary>
            void apply_source(voice_id voice, const audio_source &source, group_id group);

            /// <summary>
            /// Moves the cursor to 'seconds' (clamped to the clip length). A PCM voice jumps at the
            /// next block; a streamed voice pauses, refills its ring from the new position and
            /// resumes a few frames later (a later seek before that lands retargets it). When the
            /// decoder cannot reach the position, the voice continues from where its decoder stands
            /// and voice() reports that frame. No-op on a stale or ended voice.
            /// </summary>
            void seek(voice_id voice, f64 seconds);

            /// <summary>
            /// True while the id addresses a voice the engine still holds (any state but FREE).
            /// Any thread.
            /// </summary>
            [[nodiscard]] bool is_valid(voice_id voice) const noexcept;

            /// <summary>
            /// True for PRIMING, PLAYING or PAUSED. Any thread.
            /// </summary>
            [[nodiscard]] bool is_playing(voice_id voice) const noexcept;

            /// <summary>
            /// The voice's mirror, or nullopt for an invalid id. Any thread: state and cursor come
            /// from atomics the mixer writes; the other fields are a snapshot the main thread
            /// publishes (seqlock) whenever play(), a setter or update() changes them.
            /// </summary>
            [[nodiscard]] std::optional<voice_info> voice(voice_id voice) const noexcept;

            /// <summary>
            /// Voices in PRIMING, PLAYING, PAUSED or STOPPING.
            /// </summary>
            [[nodiscard]] u32 active_voice_count() const noexcept;

            /*-------------------------------------------------------------------*/
            // Listener

            /// <summary>
            /// The single listener (clamp_listener applied: basis normalised, gain in [0, 2],
            /// non-finite values sanitised). Spatial voices pick it up at the next update().
            /// </summary>
            void set_listener(const listener_state &listener);

            /// <summary>
            /// The listener as last set (after clamping). Main thread.
            /// </summary>
            [[nodiscard]] const listener_state &listener() const noexcept;

            /*-------------------------------------------------------------------*/
            // Groups

            /// <summary>
            /// Creates a group under desc.parent (or desc.parentName). Its own paused flag starts
            /// clear, but a group created under a paused ancestor is frozen from the start.
            /// Errors: GROUP_LIMIT_REACHED, DUPLICATE_GROUP (name taken or null), INVALID_GROUP
            /// (unknown parent id or name), GROUP_CYCLE (depth would exceed MAX_GROUP_DEPTH).
            /// </summary>
            [[nodiscard]] core::result<group_id> create_group(const group_desc &desc);

            /// <summary>
            /// Reparents children and moves voices to the parent before retiring the id. A pause
            /// the group held goes with it: its children and voices follow the parent's effective
            /// paused flag from then on. INVALID_GROUP for MASTER_GROUP or a stale id.
            /// </summary>
            [[nodiscard]] core::result<void> remove_group(group_id group);

            /// <summary>
            /// The group named 'name'; MASTER_GROUP for the null name; the null id when unknown.
            /// </summary>
            [[nodiscard]] group_id find_group(core::string_id name) const noexcept;

            /// <summary>
            /// Gain clamped to [0, 2]; the effective gains of the group and its descendants are
            /// recomputed and sent. NaN asserts and becomes 1. Stale id: no-op with a TRACE.
            /// </summary>
            void set_group_gain(group_id group, f32 gain);

            /// <summary>
            /// Sets the group's own paused flag. Pause is hierarchical, like gain: a paused group
            /// freezes its own voices and those of every descendant group (cursors hold, output is
            /// silence), and a group plays again only once neither it nor any ancestor is paused, so
            /// releasing a child under a paused parent changes group_info::paused but not
            /// group_info::effectivePaused. set_group_paused(MASTER_GROUP, true) therefore freezes
            /// every voice; it is a flag of its own, independent of set_paused() and is_paused().
            /// The flags reach the mixer as one command per group, so groups of one subtree may
            /// freeze or resume at different times: normally at most one MIX_BLOCK_FRAMES block
            /// apart, and with no upper bound when the main thread is interrupted in the middle of
            /// the call or the command ring spills. Voices that must stay sample-synchronised belong
            /// in the same group, or are held with set_paused(). Stale id: no-op with a TRACE.
            /// </summary>
            void set_group_paused(group_id group, bool paused);

            /// <summary>
            /// Name, parent, gain, effective gain, own paused flag and effective paused flag (own or
            /// any ancestor's) of a live group; nullopt for a stale id. Main thread.
            /// </summary>
            [[nodiscard]] std::optional<group_info> group(group_id group) const noexcept;

            /// <summary>
            /// Every live group, master first, in slot order. Valid until the next group change.
            /// </summary>
            [[nodiscard]] std::span<const group_info> groups() const noexcept;

            /// <summary>
            /// set_group_gain(MASTER_GROUP, gain).
            /// </summary>
            void set_master_gain(f32 gain);

            /// <summary>
            /// Pauses or resumes the whole mixer; cursors hold, output is silence. Independent of
            /// the group flags: group_info::effectivePaused does not report it.
            /// </summary>
            void set_paused(bool paused);

            /// <summary>
            /// True after set_paused(true). Main thread.
            /// </summary>
            [[nodiscard]] bool is_paused() const noexcept;

            /// <summary>
            /// Counters; lock-free, any thread (outputFormat is fixed at construction, the rest are
            /// relaxed atomics).
            /// </summary>
            [[nodiscard]] audio_engine_stats stats() const noexcept;
    };
}
