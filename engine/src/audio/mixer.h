#pragma once

#include "audio/commands.h"
#include "audio/spsc_ring.h"
#include "audio/stream_ring.h"
#include "conjure/audio/audio_types.h"

#include "conjure/core/types.h"

#include <array>
#include <atomic>

// The mixer (docs/design/12_audio.md 3.3): one backend-agnostic render() invoked by the miniaudio
// data callback on the device thread or by null_backend::update() on the main thread. It owns the
// MAX_VOICES slots, the group gain table and both rings. render() never allocates, locks, logs,
// touches assets or walks the group tree: every policy decision arrives as a command.
//
// Pan laws: a mono (or downmixed spatial) voice uses constant power, left = cos((pan + 1) * PI / 4),
// right = sin(...), so centre is -3 dB per channel. A stereo voice played flat keeps unity at centre
// and attenuates the far channel linearly (left = min(1, 1 - pan), right = min(1, 1 + pan)), so a
// stereo clip at gain g renders at amplitude g.

namespace conjure::audio {

    /// <summary>
    /// One mixer slot. The main thread reads the atomics from any thread and the plain fields only
    /// on the null backend, where the callback runs on the main thread too.
    ///
    /// Ordering contract: PLAY writes every plain field, the published cursor and the state before
    /// it stores the new generation (release). A reader that loads generation (acquire) and finds
    /// the occupant it expects therefore also sees that occupant's state and cursor, never the
    /// previous occupant's FINISHED / STOPPED. Readers compare the generation first for this reason.
    /// </summary>
    struct mixer_voice {
        std::atomic<u8> state = static_cast<u8>(voice_state::FREE);
        std::atomic<u64> publishedCursor = 0;       // integer cursor after the last block
        std::atomic<u32> generation = 0;            // of the occupant that PLAY installed; stored last
        const f32 *samples = nullptr;               // PCM storage (interleaved)
        stream_ring *stream = nullptr;              // STREAMED storage
        u64 clipFrames = 0;
        f64 cursor = 0.0;                           // in clip frames
        f64 step = 1.0;                             // pitch * clipRate / mixRate
        f32 gainCurrent = 0.0f;
        f32 gainTarget = 0.0f;
        f32 gainStep = 0.0f;                        // per frame while gainRampFrames > 0
        u32 gainRampFrames = 0;
        f32 panCurrent = 0.0f;
        f32 panTarget = 0.0f;
        f32 panStep = 0.0f;                         // per frame, applied per block
        u32 panRampFrames = 0;
        bool loop = false;
        bool downmix = false;
        u8 groupIndex = 0;
        u16 channels = 1;
        std::array<std::array<f32, 2>, 2> held = {};   // stream: frame at the cursor and the next one
        u32 heldCount = 0;
        bool heldLast = false;                      // held[1] duplicates the final frame of the stream
        f64 streamFrac = 0.0;
        bool underrunReported = false;              // one UNDERRUN event per block
    };

    class mixer {
        public:
            using command_ring = spsc_ring<voice_command, COMMAND_RING_CAPACITY>;
            using event_ring = spsc_ring<voice_event, EVENT_RING_CAPACITY>;

        private:
            u32 mSampleRate = 48000;
            std::array<mixer_voice, MAX_VOICES> mVoices = {};
            std::array<f32, MAX_GROUPS> mGroupGain = {};
            std::array<bool, MAX_GROUPS> mGroupPaused = {};
            bool mMasterPaused = false;
            command_ring mCommands = {};
            event_ring mEvents = {};
            std::array<f32, MIX_BLOCK_FRAMES * MIX_CHANNELS> mMixBlock = {};
            std::atomic<u64> mRenderCalls = 0;
            std::atomic<u64> mFramesRendered = 0;
            std::atomic<u32> mUnderruns = 0;
            std::atomic<u32> mLoops = 0;
            std::atomic<u32> mCommandsApplied = 0;
            std::atomic<u32> mCommandsDropped = 0;
            std::atomic<u64> mEventsDropped = 0;
            std::atomic<u32> mActiveVoices = 0;

        public:
            explicit mixer(u32 sampleRate);
            ~mixer();

            mixer(const mixer &) = delete;
            mixer &operator=(const mixer &) = delete;

        public:
            /// <summary>
            /// The callback: drains commands, mixes every audible voice into 'out' (interleaved
            /// stereo f32, hard-clamped to [-1, 1]) in blocks of at most MIX_BLOCK_FRAMES.
            /// </summary>
            void render(f32 *out, u32 frameCount) noexcept;

            /// <summary>
            /// mix_callback adapter; 'user' is the mixer.
            /// </summary>
            static void render_callback(void *user, f32 *output, u32 frameCount) noexcept;

            /// <summary>
            /// Mix rate the step values are relative to.
            /// </summary>
            [[nodiscard]] u32 sample_rate() const noexcept {
                return mSampleRate;
            }

            /// <summary>
            /// Changes the mix rate. Only while the callback is not running (device stopped).
            /// </summary>
            void set_sample_rate(u32 sampleRate) noexcept {
                mSampleRate = sampleRate;
            }

            /// <summary>
            /// Producer side of the command ring (main thread).
            /// </summary>
            [[nodiscard]] command_ring &commands() noexcept {
                return mCommands;
            }

            /// <summary>
            /// Consumer side of the event ring (main thread).
            /// </summary>
            [[nodiscard]] event_ring &events() noexcept {
                return mEvents;
            }

            /// <summary>
            /// Slot inspection. The atomics are safe from any thread; the plain fields only when
            /// the callback is not running concurrently (null backend, or device stopped).
            /// </summary>
            [[nodiscard]] const mixer_voice &slot(u32 index) const noexcept {
                return mVoices[index];
            }

            /// <summary>
            /// Published state of a slot (atomic).
            /// </summary>
            [[nodiscard]] voice_state slot_state(u32 index) const noexcept {
                return static_cast<voice_state>(mVoices[index].state.load(std::memory_order_acquire));
            }

            /// <summary>
            /// Published integer cursor of a slot (atomic).
            /// </summary>
            [[nodiscard]] u64 slot_cursor(u32 index) const noexcept {
                return mVoices[index].publishedCursor.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Generation of the occupant the slot last received PLAY for (atomic).
            /// </summary>
            [[nodiscard]] u32 slot_generation(u32 index) const noexcept {
                return mVoices[index].generation.load(std::memory_order_acquire);
            }

            [[nodiscard]] f32 group_gain(u32 index) const noexcept {
                return mGroupGain[index];
            }

            [[nodiscard]] bool group_paused(u32 index) const noexcept {
                return mGroupPaused[index];
            }

            [[nodiscard]] bool master_paused() const noexcept {
                return mMasterPaused;
            }

            [[nodiscard]] u64 render_calls() const noexcept {
                return mRenderCalls.load(std::memory_order_relaxed);
            }

            [[nodiscard]] u64 frames_rendered() const noexcept {
                return mFramesRendered.load(std::memory_order_relaxed);
            }

            [[nodiscard]] u32 underruns() const noexcept {
                return mUnderruns.load(std::memory_order_relaxed);
            }

            [[nodiscard]] u32 loops() const noexcept {
                return mLoops.load(std::memory_order_relaxed);
            }

            [[nodiscard]] u32 commands_applied() const noexcept {
                return mCommandsApplied.load(std::memory_order_relaxed);
            }

            [[nodiscard]] u32 commands_dropped() const noexcept {
                return mCommandsDropped.load(std::memory_order_relaxed);
            }

            [[nodiscard]] u64 events_dropped() const noexcept {
                return mEventsDropped.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Voices in PLAYING, PAUSED or STOPPING after the last block.
            /// </summary>
            [[nodiscard]] u32 active_voices() const noexcept {
                return mActiveVoices.load(std::memory_order_relaxed);
            }

        private:
            void apply_commands() noexcept;
            void apply_command(const voice_command &command) noexcept;
            void apply_play(mixer_voice &voice, const voice_command &command) noexcept;
            void render_block(u32 frameCount) noexcept;
            void render_voice(mixer_voice &voice, u32 index, u32 frameCount) noexcept;
            void render_pcm_voice(mixer_voice &voice, u32 index, u32 frameCount, f32 leftPan, f32 rightPan) noexcept;
            void render_stream_voice(mixer_voice &voice, u32 index, u32 frameCount, f32 leftPan, f32 rightPan) noexcept;
            [[nodiscard]] bool ensure_stream_frames(mixer_voice &voice) noexcept;
            void advance_gain(mixer_voice &voice) noexcept;
            void advance_pan(mixer_voice &voice, u32 frameCount) noexcept;
            void finish_voice(mixer_voice &voice, u32 index, event_kind kind) noexcept;
            void emit(event_kind kind, u32 index, const mixer_voice &voice, u64 frame) noexcept;
    };
}
