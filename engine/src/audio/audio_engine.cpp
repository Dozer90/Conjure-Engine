#include "conjure/audio/audio_engine.h"
#include "audio/commands.h"
#include "audio/group_table.h"
#include "audio/mixer.h"
#include "audio/stream_ring.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/decoder.h"
#include "conjure/audio/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/error.h"
#include "conjure/core/jobs/main_thread.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::audio;

/*===========================================================================*/
/*===========================================================================*/
// Private types

namespace conjure::audio::detail {
    // Voice entries outnumber mixer slots so a voice waiting for a stolen slot has an id of its own.
    constexpr u32 VOICE_TABLE_CAPACITY = MAX_VOICES * 2;
    constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();
    constexpr f32 GAIN_EPSILON = 1.0e-4f;
    constexpr f32 PAN_EPSILON = 1.0e-3f;
    constexpr f64 STEP_EPSILON = 1.0e-3;

    /// <summary>
    /// Where a streamed voice is in a seek: PAUSE sent and awaiting the acknowledgement, ring and
    /// decoder reset pending (no decode job may be in flight), or the primed chunk awaited.
    /// </summary>
    enum class seek_phase : u8 {
        NONE,
        WAIT_PAUSE,
        WAIT_RESET,
        WAIT_PRIME
    };

    /// <summary>
    /// One of the MAX_STREAMING_VOICES decode pipelines: the ring the mixer reads, the decoder a job
    /// drives, and the flags that make "one producer at a time" true.
    /// </summary>
    struct stream_slot {
        stream_ring ring{STREAM_RING_FRAMES};
        streaming_decoder decoder = {};
        std::vector<f32> scratch = std::vector<f32>(static_cast<usize>(STREAM_CHUNK_FRAMES) * MIX_CHANNELS, 0.0f);
        std::atomic<bool> jobInFlight = false;
        std::atomic<bool> loop = false;
        core::jobs::job_handle job = {};
        std::shared_ptr<const std::vector<byte>> encoded = nullptr;   // keeps the decoder's bytes alive
        bool inUse = false;
        bool releasing = false;              // freed once the job in flight has finished
        u32 ownerEntry = INVALID_INDEX;
        u64 clipFrames = 0;
        u16 channels = 1;
    };

    /// <summary>
    /// Main-thread record of one voice.
    /// </summary>
    struct voice_slot {
        voice_info info = {};
        std::shared_ptr<const pcm_buffer> pcm = nullptr;
        stream_slot *stream = nullptr;
        play_params params = {};
        attenuation_params attenuation = {};
        math::vec3 position = math::vec3(0.0f);
        math::vec3 velocity = math::vec3(0.0f);
        bool hasPosition = false;
        u32 mixerIndex = INVALID_INDEX;
        u32 mixerGeneration = 0;
        bool hasMixerSlot = false;           // PLAY sent; the slot is owned until the mixer acknowledges the end
        bool awaitingSlot = false;           // waiting for a stolen slot to be acknowledged
        bool releaseRequested = false;
        bool stolen = false;
        bool reported = false;               // appeared in finished_voices()
        bool userPaused = false;
        seek_phase seekPhase = seek_phase::NONE;
        u64 seekFrame = 0;
        f32 lastGain = -1.0f;
        f32 lastPan = 0.0f;
        f64 lastStep = 0.0;
        f32 attenuationGain = 1.0f;
        f32 pitchRatio = 1.0f;
        u32 clipRate = 48000;
        u64 clipFrames = 0;
        u64 streamStartFrame = 0;            // where the streamed voice's decoder was positioned at play()
        u16 channels = 1;
    };

    /// <summary>
    /// What audio_engine::voice() reads from any thread: a copy of the main-thread mirror taken
    /// at the end of play() and of every update(), published through a per-entry seqlock.
    /// </summary>
    struct voice_mirror {
        voice_info info = {};
        u32 mixerIndex = INVALID_INDEX;
        u32 mixerGeneration = 0;
        bool hasMixerSlot = false;
    };

    constexpr u32 MIRROR_READ_ATTEMPTS = 64;

    struct voice_entry {
        std::atomic<u32> generation = 0;
        std::atomic<u8> state = static_cast<u8>(voice_state::FREE);
        bool live = false;
        u32 nextFree = INVALID_INDEX;
        voice_slot slot = {};
        std::atomic<u32> mirrorSequence = 0;   // seqlock of 'mirror': odd while the main thread writes it
        voice_mirror mirror = {};
    };

    [[nodiscard]] u32 seconds_to_frames(f32 seconds, u32 sampleRate) noexcept {
        if (!(seconds > 0.0f)) {
            return 1;
        }
        const f64 frames = std::round(static_cast<f64>(seconds) * static_cast<f64>(sampleRate));
        return static_cast<u32>(std::clamp(frames, 1.0, static_cast<f64>(std::numeric_limits<u32>::max())));
    }
}

using namespace conjure::audio::detail;

struct audio_engine::state {
    audio_settings settings = {};
    std::unique_ptr<audio_backend> backend = nullptr;
    null_backend *nullBackend = nullptr;
    std::optional<core::error> backendError = std::nullopt;
    backend_desc desc = {};
    mixer mix{48000};
    std::array<voice_entry, VOICE_TABLE_CAPACITY> voices = {};
    u32 freeHead = INVALID_INDEX;
    u32 liveVoices = 0;
    std::array<u32, MAX_VOICES> slotOwner = {};           // voice entry owning each mixer slot
    std::array<u32, MAX_VOICES> slotGeneration = {};      // last generation issued per mixer slot
    std::array<stream_slot, MAX_STREAMING_VOICES> streams = {};
    group_table groups = {};
    listener_state listener = {};
    std::vector<voice_command> spill = {};
    usize spillHead = 0;
    std::vector<voice_id> finished = {};
    std::vector<voice_id> finishedPublished = {};
    bool masterPaused = false;
    bool recovering = false;                              // LOST observed; retries driven by the engine
    f32 recoveryTimer = 0.0f;
    u32 recoveryAttempts = 0;
    u32 activeVoices = 0;                                 // live voices that have not ended
    std::atomic<u32> voicesDropped = 0;
    std::atomic<u32> peakVoices = 0;
    std::atomic<u32> streamingVoices = 0;
    std::atomic<u64> commandsSent = 0;
    std::atomic<u64> commandsSpilled = 0;
    std::atomic<u64> eventsReceived = 0;
    std::atomic<u32> deviceRestarts = 0;
    std::atomic<bool> fellBackToNull = false;
    std::atomic<u8> deviceStateMirror = static_cast<u8>(device_state::STOPPED);
};

/*===========================================================================*/
/*===========================================================================*/
// Private helpers

struct conjure::audio::detail::audio_engine_helpers {
    using engine_state = audio_engine::state;

    /*-----------------------------------------------------------------------*/
    // Commands

    static void push_command(engine_state &state, const voice_command &command) {
        state.commandsSent.fetch_add(1, std::memory_order_relaxed);
        if (state.spillHead < state.spill.size() || !state.mix.commands().try_push(command)) {
            state.spill.push_back(command);
            state.commandsSpilled.fetch_add(1, std::memory_order_relaxed);
        }
    }

    static void flush_spill(engine_state &state) {
        while (state.spillHead < state.spill.size()) {
            if (!state.mix.commands().try_push(state.spill[state.spillHead])) {
                break;
            }
            ++state.spillHead;
        }
        if (state.spillHead >= state.spill.size()) {
            state.spill.clear();
            state.spillHead = 0;
        }
    }

    [[nodiscard]] static voice_command voice_message(const voice_slot &slot, command_kind kind) noexcept {
        voice_command command = {};
        command.kind = kind;
        command.voiceIndex = slot.mixerIndex;
        command.generation = slot.mixerGeneration;
        return command;
    }

    /*-----------------------------------------------------------------------*/
    // Voice table

    [[nodiscard]] static voice_entry *resolve(engine_state &state, voice_id id) noexcept {
        const u32 index = id.key.index;
        if (!id.is_valid() || index >= VOICE_TABLE_CAPACITY) {
            return nullptr;
        }
        voice_entry &entry = state.voices[index];
        if (!entry.live || entry.generation.load(std::memory_order_acquire) != id.key.generation) {
            return nullptr;
        }
        return &entry;
    }

    [[nodiscard]] static const voice_entry *resolve(const engine_state &state, voice_id id) noexcept {
        const u32 index = id.key.index;
        if (!id.is_valid() || index >= VOICE_TABLE_CAPACITY) {
            return nullptr;
        }
        const voice_entry &entry = state.voices[index];
        if (!entry.live || entry.generation.load(std::memory_order_acquire) != id.key.generation) {
            return nullptr;
        }
        return &entry;
    }

    [[nodiscard]] static voice_id id_of(const engine_state &state, u32 entryIndex) noexcept {
        voice_id id = {};
        id.key.index = entryIndex;
        id.key.generation = state.voices[entryIndex].generation.load(std::memory_order_relaxed);
        return id;
    }

    [[nodiscard]] static u32 allocate_entry(engine_state &state) {
        if (state.freeHead == INVALID_INDEX) {
            return INVALID_INDEX;
        }
        const u32 index = state.freeHead;
        voice_entry &entry = state.voices[index];
        state.freeHead = entry.nextFree;
        entry.nextFree = INVALID_INDEX;
        entry.live = true;
        entry.slot = {};
        u32 generation = entry.generation.load(std::memory_order_relaxed) + 1;
        if (generation == 0) {
            generation = 1;
        }
        entry.generation.store(generation, std::memory_order_release);
        ++state.liveVoices;
        return index;
    }

    static void set_mirror_state(voice_entry &entry, voice_state value) noexcept {
        entry.slot.info.state = value;
        entry.state.store(static_cast<u8>(value), std::memory_order_release);
    }

    /*-----------------------------------------------------------------------*/
    // Any-thread mirror (seqlock; one writer, the main thread)

    /// <summary>
    /// Copies the main-thread mirror into the snapshot voice() reads. The sequence is odd while
    /// the copy is in progress and even once it is complete; a reader that sees a change retries.
    /// </summary>
    static void publish_mirror(voice_entry &entry) noexcept {
        const u32 sequence = entry.mirrorSequence.load(std::memory_order_relaxed);
        entry.mirrorSequence.store(sequence + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        entry.mirror.info = entry.slot.info;
        entry.mirror.mixerIndex = entry.slot.mixerIndex;
        entry.mirror.mixerGeneration = entry.slot.mixerGeneration;
        entry.mirror.hasMixerSlot = entry.slot.hasMixerSlot;
        entry.mirrorSequence.store(sequence + 2, std::memory_order_release);
    }

    static void publish_mirrors(engine_state &state) noexcept {
        for (voice_entry &entry : state.voices) {
            if (entry.live) {
                publish_mirror(entry);
            }
        }
    }

    /// <summary>
    /// Reads a consistent snapshot (the classic seqlock loop). The writer holds the sequence odd
    /// for a few dozen stores at most, so the bounded retry practically always succeeds; if it
    /// does not, the last copy is returned, which only affects the non-atomic fields of voice().
    /// </summary>
    [[nodiscard]] static voice_mirror read_mirror(const voice_entry &entry) noexcept {
        voice_mirror copy = {};
        for (u32 attempt = 0; attempt < MIRROR_READ_ATTEMPTS; ++attempt) {
            const u32 before = entry.mirrorSequence.load(std::memory_order_acquire);
            if ((before & 1u) != 0) {
                continue;
            }
            copy = entry.mirror;
            std::atomic_thread_fence(std::memory_order_acquire);
            if (entry.mirrorSequence.load(std::memory_order_relaxed) == before) {
                break;
            }
        }
        return copy;
    }

    static void release_stream_slot(engine_state &state, voice_slot &slot) {
        if (slot.stream == nullptr) {
            return;
        }
        stream_slot &stream = *slot.stream;
        slot.stream = nullptr;
        stream.ownerEntry = INVALID_INDEX;
        if (stream.jobInFlight.load(std::memory_order_acquire)) {
            stream.releasing = true;
            return;
        }
        free_stream_slot(state, stream);
    }

    static void free_stream_slot(engine_state &state, stream_slot &stream) {
        stream.decoder.close();
        stream.encoded.reset();
        stream.job.reset();
        stream.inUse = false;
        stream.releasing = false;
        stream.ownerEntry = INVALID_INDEX;
        stream.clipFrames = 0;
        if (state.streamingVoices.load(std::memory_order_relaxed) > 0) {
            state.streamingVoices.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    static void free_entry(engine_state &state, u32 entryIndex) {
        voice_entry &entry = state.voices[entryIndex];
        if (!entry.live) {
            return;
        }
        if (entry.slot.hasMixerSlot && entry.slot.mixerIndex < MAX_VOICES
          && state.slotOwner[entry.slot.mixerIndex] == entryIndex) {
            state.slotOwner[entry.slot.mixerIndex] = INVALID_INDEX;
        }
        release_stream_slot(state, entry.slot);
        entry.slot = {};
        entry.live = false;
        set_mirror_state(entry, voice_state::FREE);
        entry.nextFree = state.freeHead;
        state.freeHead = entryIndex;
        --state.liveVoices;
    }

    /*-----------------------------------------------------------------------*/
    // Gain, pan and step of a voice as the mixer should see them

    [[nodiscard]] static f64 step_of(const engine_state &state, const voice_slot &slot) noexcept {
        const f64 mixRate = static_cast<f64>(state.mix.sample_rate());
        return static_cast<f64>(slot.params.pitch) * static_cast<f64>(slot.clipRate) / mixRate
          * static_cast<f64>(slot.pitchRatio);
    }

    [[nodiscard]] static f32 gain_of(const engine_state &state, const voice_slot &slot) noexcept {
        return slot.params.volume * slot.attenuationGain * state.listener.gain;
    }

    [[nodiscard]] static f32 pan_of(const engine_state &state, const voice_slot &slot) noexcept {
        if (!slot.params.spatial) {
            return slot.params.pan;
        }
        const math::vec3 position = slot.hasPosition ? slot.position : state.listener.position;
        return compute_pan(state.listener, position);
    }

    static void refresh_spatial(engine_state &state, voice_slot &slot) noexcept {
        if (!slot.params.spatial) {
            slot.attenuationGain = 1.0f;
            slot.pitchRatio = 1.0f;
            return;
        }
        const math::vec3 position = slot.hasPosition ? slot.position : state.listener.position;
        const spatial_result spatial = compute_spatial(
          state.listener,
          position,
          slot.velocity,
          slot.attenuation,
          state.settings.dopplerFactor,
          state.settings.speedOfSound
        );
        slot.attenuationGain = spatial.gain;
        slot.pitchRatio = spatial.pitchRatio;
    }

    static void send_gain(engine_state &state, voice_slot &slot, u32 rampFrames) {
        const f32 gain = gain_of(state, slot);
        voice_command command = voice_message(slot, command_kind::SET_GAIN);
        command.fadeFrames = rampFrames;
        command.payload.scalar = gain;
        push_command(state, command);
        slot.lastGain = gain;
    }

    static void send_pan(engine_state &state, voice_slot &slot) {
        const f32 pan = pan_of(state, slot);
        voice_command command = voice_message(slot, command_kind::SET_PAN);
        command.fadeFrames = seconds_to_frames(DEFAULT_RAMP_SECONDS, state.mix.sample_rate());
        command.payload.scalar = pan;
        push_command(state, command);
        slot.lastPan = pan;
    }

    static void send_step(engine_state &state, voice_slot &slot) {
        const f64 step = step_of(state, slot);
        voice_command command = voice_message(slot, command_kind::SET_STEP);
        command.payload.step = step;
        push_command(state, command);
        slot.lastStep = step;
    }

    static void update_effective_gain(engine_state &state, voice_slot &slot) noexcept {
        slot.info.effectiveGain = gain_of(state, slot) * state.groups.effective_gain(slot.params.group.index);
    }

    /// <summary>
    /// Pushes whatever changed since the last send (used by the setters and the spatial pass).
    /// </summary>
    static void sync_voice(engine_state &state, voice_slot &slot, bool force) {
        if (!slot.hasMixerSlot) {
            update_effective_gain(state, slot);
            return;
        }
        refresh_spatial(state, slot);
        const u32 ramp = seconds_to_frames(DEFAULT_RAMP_SECONDS, state.mix.sample_rate());
        if (force || std::abs(gain_of(state, slot) - slot.lastGain) > GAIN_EPSILON) {
            send_gain(state, slot, ramp);
        }
        if (force || std::abs(pan_of(state, slot) - slot.lastPan) > PAN_EPSILON) {
            send_pan(state, slot);
        }
        if (force || std::abs(step_of(state, slot) - slot.lastStep) > STEP_EPSILON * std::max(1.0, slot.lastStep)) {
            send_step(state, slot);
        }
        update_effective_gain(state, slot);
    }

    /*-----------------------------------------------------------------------*/
    // Streaming

    static void decode_chunk(stream_slot &stream) noexcept {
        const u16 channels = stream.channels;
        const usize chunkSamples = static_cast<usize>(STREAM_CHUNK_FRAMES) * channels;
        std::span<f32> scratch(stream.scratch.data(), chunkSamples);

        usize frames = stream.decoder.read(scratch);
        if (frames < STREAM_CHUNK_FRAMES && stream.loop.load(std::memory_order_acquire)) {
            if (stream.decoder.seek(0).has_value()) {
                const std::span<f32> remainder = scratch.subspan(frames * channels);
                frames += stream.decoder.read(remainder);
            }
        }
        if (frames == 0) {
            // Either the stream ended (no loop) or the decoder cannot produce more: the voice ends.
            stream.ring.set_end_of_stream(true);
        }
        stream.ring.push_frames(scratch.first(frames * channels));
        stream.jobInFlight.store(false, std::memory_order_release);
    }

    static void schedule_decode(engine_state &state, stream_slot &stream) {
        stream.jobInFlight.store(true, std::memory_order_release);
        if (state.settings.jobs == nullptr) {
            decode_chunk(stream);
            return;
        }
        stream_slot *target = &stream;
        stream.job = state.settings.jobs->schedule([target]() {
            decode_chunk(*target);
        });
    }

    /// <summary>
    /// Fills the ring as far as it goes inline, or schedules one job.
    /// </summary>
    static void feed_stream(engine_state &state, stream_slot &stream) {
        if (stream.jobInFlight.load(std::memory_order_acquire) || stream.ring.end_of_stream()) {
            return;
        }
        if (state.settings.jobs == nullptr) {
            while (!stream.ring.end_of_stream() && stream.ring.free_frames() >= STREAM_CHUNK_FRAMES) {
                schedule_decode(state, stream);
            }
            return;
        }
        if (stream.ring.free_frames() >= STREAM_CHUNK_FRAMES) {
            schedule_decode(state, stream);
        }
    }

    [[nodiscard]] static bool stream_primed(const stream_slot &stream) noexcept {
        return stream.ring.available_frames() >= STREAM_CHUNK_FRAMES || stream.ring.end_of_stream();
    }

    [[nodiscard]] static stream_slot *acquire_stream_slot(engine_state &state) noexcept {
        for (stream_slot &stream : state.streams) {
            if (!stream.inUse && !stream.jobInFlight.load(std::memory_order_acquire)) {
                stream.inUse = true;
                stream.releasing = false;
                state.streamingVoices.fetch_add(1, std::memory_order_relaxed);
                return &stream;
            }
        }
        return nullptr;
    }

    /*-----------------------------------------------------------------------*/
    // Mixer slot lifecycle

    [[nodiscard]] static u32 find_free_mixer_slot(const engine_state &state) noexcept {
        for (u32 index = 0; index < MAX_VOICES; ++index) {
            if (state.slotOwner[index] == INVALID_INDEX) {
                return index;
            }
        }
        return INVALID_INDEX;
    }

    static void start_mixer_voice(engine_state &state, u32 entryIndex, u32 mixerIndex) {
        voice_entry &entry = state.voices[entryIndex];
        voice_slot &slot = entry.slot;

        u32 generation = state.slotGeneration[mixerIndex] + 1;
        if (generation == 0) {
            generation = 1;
        }
        state.slotGeneration[mixerIndex] = generation;
        state.slotOwner[mixerIndex] = entryIndex;
        slot.mixerIndex = mixerIndex;
        slot.mixerGeneration = generation;
        slot.hasMixerSlot = true;
        slot.awaitingSlot = false;

        refresh_spatial(state, slot);

        voice_command command = {};
        command.kind = command_kind::PLAY;
        command.voiceIndex = mixerIndex;
        command.generation = generation;
        command.groupIndex = slot.params.group.index;
        play_payload &play = command.payload.play;
        play.pcm = slot.pcm.get();
        play.stream = slot.stream != nullptr ? &slot.stream->ring : nullptr;
        play.gain = gain_of(state, slot);
        play.pan = pan_of(state, slot);
        play.step = step_of(state, slot);
        play.clipFrames = slot.clipFrames;
        play.loop = slot.params.loop;
        play.startPaused = slot.params.startPaused || slot.userPaused;
        play.downmix = slot.params.spatial && slot.channels == 2;
        play.channels = slot.channels;

        const f64 startFrames = slot.params.startSeconds * static_cast<f64>(slot.clipRate);
        play.startFrame = slot.stream != nullptr
          ? slot.streamStartFrame
          : static_cast<u64>(std::clamp(startFrames, 0.0, static_cast<f64>(slot.clipFrames)));
        push_command(state, command);

        slot.lastGain = play.gain;
        slot.lastPan = play.pan;
        slot.lastStep = play.step;
        slot.info.cursorFrames = play.startFrame;
        update_effective_gain(state, slot);
        set_mirror_state(entry, play.startPaused ? voice_state::PAUSED : voice_state::PLAYING);
    }

    /// <summary>
    /// Marks a voice ended (FINISHED or STOPPED), publishes it once and frees its mixer slot. Stolen
    /// and released voices are erased right away; one-shots wait for end_frame().
    /// </summary>
    static void on_voice_ended(engine_state &state, u32 entryIndex, voice_state terminal, u64 cursor) {
        voice_entry &entry = state.voices[entryIndex];
        voice_slot &slot = entry.slot;
        if (!entry.live || is_terminal_state(slot.info.state)) {
            return;
        }
        slot.info.cursorFrames = cursor;
        set_mirror_state(entry, terminal);
        if (state.activeVoices > 0) {
            --state.activeVoices;
        }
        if (!slot.reported) {
            slot.reported = true;
            state.finished.push_back(id_of(state, entryIndex));
        }
        if (slot.hasMixerSlot && slot.mixerIndex < MAX_VOICES && state.slotOwner[slot.mixerIndex] == entryIndex) {
            state.slotOwner[slot.mixerIndex] = INVALID_INDEX;
        }
        slot.hasMixerSlot = false;
        slot.awaitingSlot = false;
        release_stream_slot(state, slot);
        if (slot.stolen || slot.releaseRequested) {
            free_entry(state, entryIndex);
            return;
        }
        publish_mirror(entry);
    }

    static void drain_events(engine_state &state) {
        voice_event event = {};
        while (state.mix.events().try_pop(event)) {
            state.eventsReceived.fetch_add(1, std::memory_order_relaxed);
            if (event.voiceIndex >= MAX_VOICES) {
                continue;
            }
            const u32 owner = state.slotOwner[event.voiceIndex];
            if (owner == INVALID_INDEX) {
                continue;
            }
            voice_entry &entry = state.voices[owner];
            voice_slot &slot = entry.slot;
            if (!entry.live || slot.mixerGeneration != event.generation) {
                continue;
            }
            switch (event.kind) {
                case event_kind::STARTED:
                    if (slot.info.state == voice_state::PRIMING) {
                        set_mirror_state(entry, slot.userPaused ? voice_state::PAUSED : voice_state::PLAYING);
                    }
                    break;
                case event_kind::FINISHED:
                    on_voice_ended(state, owner, voice_state::FINISHED, event.frame);
                    break;
                case event_kind::STOPPED:
                    on_voice_ended(state, owner, voice_state::STOPPED, event.frame);
                    break;
                case event_kind::PAUSED:
                    if (slot.info.state == voice_state::PLAYING || slot.info.state == voice_state::PRIMING) {
                        set_mirror_state(entry, voice_state::PAUSED);
                    }
                    if (slot.seekPhase == seek_phase::WAIT_PAUSE) {
                        slot.seekPhase = seek_phase::WAIT_RESET;
                    }
                    break;
                case event_kind::RESUMED:
                    if (slot.info.state == voice_state::PAUSED) {
                        set_mirror_state(entry, voice_state::PLAYING);
                    }
                    break;
                case event_kind::LOOPED:
                case event_kind::UNDERRUN:
                    break;
            }
        }
    }

    /// <summary>
    /// Covers dropped events: the mixer's published slot state is authoritative.
    /// </summary>
    static void reconcile(engine_state &state) {
        for (u32 mixerIndex = 0; mixerIndex < MAX_VOICES; ++mixerIndex) {
            const u32 owner = state.slotOwner[mixerIndex];
            if (owner == INVALID_INDEX) {
                continue;
            }
            voice_entry &entry = state.voices[owner];
            if (!entry.live || !entry.slot.hasMixerSlot) {
                continue;
            }
            // A command still waiting in the ring or spill means the slot may not have seen PLAY yet.
            if (state.mix.slot_generation(mixerIndex) != entry.slot.mixerGeneration) {
                continue;
            }
            const voice_state mixerState = state.mix.slot_state(mixerIndex);
            if (is_terminal_state(mixerState)) {
                on_voice_ended(state, owner, mixerState, state.mix.slot_cursor(mixerIndex));
                continue;
            }
            entry.slot.info.cursorFrames = state.mix.slot_cursor(mixerIndex);
            if (entry.slot.info.state == voice_state::PRIMING && mixerState == voice_state::PLAYING) {
                set_mirror_state(entry, voice_state::PLAYING);
            }
            if (entry.slot.info.state == voice_state::PRIMING && mixerState == voice_state::PAUSED) {
                set_mirror_state(entry, voice_state::PAUSED);
            }
        }
    }

    /// <summary>
    /// Gives stolen slots to the voices waiting for them and starts primed streamed voices.
    /// </summary>
    static void start_pending_voices(engine_state &state) {
        for (u32 entryIndex = 0; entryIndex < VOICE_TABLE_CAPACITY; ++entryIndex) {
            voice_entry &entry = state.voices[entryIndex];
            voice_slot &slot = entry.slot;
            if (!entry.live || slot.hasMixerSlot || slot.info.state != voice_state::PRIMING) {
                continue;
            }
            // A pending seek repositions the stream first (streaming_pass); starting now would
            // play the chunk decoded from the old position.
            if (slot.stream != nullptr && (slot.seekPhase != seek_phase::NONE || !stream_primed(*slot.stream))) {
                continue;
            }
            u32 mixerIndex = INVALID_INDEX;
            if (slot.awaitingSlot) {
                if (slot.mixerIndex < MAX_VOICES && state.slotOwner[slot.mixerIndex] == INVALID_INDEX) {
                    mixerIndex = slot.mixerIndex;
                }
            } else {
                mixerIndex = find_free_mixer_slot(state);
            }
            if (mixerIndex == INVALID_INDEX) {
                continue;
            }
            start_mixer_voice(state, entryIndex, mixerIndex);
        }
    }

    /// <summary>
    /// True when nothing reads the voice's stream ring: the voice has no mixer slot yet, or the
    /// mixer has published PAUSED for this very occupant (acquire; set at the top of a block, so
    /// no pop of an earlier block is still in flight). The mirror alone is not enough: a PAUSE
    /// still travelling through the command ring leaves the device thread reading.
    /// </summary>
    [[nodiscard]] static bool stream_reader_idle(const engine_state &state, const voice_slot &slot) noexcept {
        if (!slot.hasMixerSlot) {
            return true;
        }
        if (slot.mixerIndex >= MAX_VOICES || state.mix.slot_generation(slot.mixerIndex) != slot.mixerGeneration) {
            return false;
        }
        return state.mix.slot_state(slot.mixerIndex) == voice_state::PAUSED;
    }

    /// <summary>
    /// The streamed-voice seek state machine and the chunk refills.
    /// </summary>
    static void streaming_pass(engine_state &state) {
        for (stream_slot &stream : state.streams) {
            if (stream.releasing) {
                if (!stream.jobInFlight.load(std::memory_order_acquire)) {
                    free_stream_slot(state, stream);
                }
                continue;
            }
            if (!stream.inUse || stream.ownerEntry == INVALID_INDEX) {
                continue;
            }
            voice_entry &entry = state.voices[stream.ownerEntry];
            voice_slot &slot = entry.slot;
            if (!entry.live) {
                continue;
            }

            const bool awaitingReset =
              slot.seekPhase == seek_phase::WAIT_PAUSE || slot.seekPhase == seek_phase::WAIT_RESET;
            if (awaitingReset) {
                if (stream.jobInFlight.load(std::memory_order_acquire) || !stream_reader_idle(state, slot)) {
                    continue;
                }
                // No job writes and nothing pops: both ring ends are ours for the reset.
                stream.ring.reset(stream.channels);
                const u64 lastFrame = stream.clipFrames > 0 ? stream.clipFrames - 1 : 0;
                if (!stream.decoder.seek(std::min(slot.seekFrame, lastFrame)).has_value()) {
                    CONJURE_LOG_WARN(LOG_AUDIO, "streamed voice seek failed; continuing from the current position");
                }
                if (slot.hasMixerSlot) {
                    slot.seekPhase = seek_phase::WAIT_PRIME;
                } else {
                    // No PLAY sent yet: the voice starts from the decoder's new position once primed.
                    slot.streamStartFrame = stream.decoder.position();
                    slot.info.cursorFrames = slot.streamStartFrame;
                    slot.seekPhase = seek_phase::NONE;
                }
                feed_stream(state, stream);
            }

            if (slot.seekPhase == seek_phase::WAIT_PRIME && stream_primed(stream)) {
                voice_command seekCommand = voice_message(slot, command_kind::SEEK);
                seekCommand.payload.frame = slot.seekFrame;
                push_command(state, seekCommand);
                slot.info.cursorFrames = seekCommand.payload.frame;
                if (!slot.userPaused) {
                    push_command(state, voice_message(slot, command_kind::RESUME));
                    set_mirror_state(entry, voice_state::PLAYING);
                }
                slot.seekPhase = seek_phase::NONE;
            }

            if (slot.seekPhase == seek_phase::NONE || slot.seekPhase == seek_phase::WAIT_PRIME) {
                feed_stream(state, stream);
            }
        }
    }

    static void spatial_pass(engine_state &state) {
        for (u32 entryIndex = 0; entryIndex < VOICE_TABLE_CAPACITY; ++entryIndex) {
            voice_entry &entry = state.voices[entryIndex];
            voice_slot &slot = entry.slot;
            if (!entry.live) {
                continue;
            }
            if (!slot.hasMixerSlot || is_terminal_state(slot.info.state) || slot.info.state == voice_state::STOPPING) {
                continue;
            }
            sync_voice(state, slot, false);
        }
    }

    static void count_started_voice(engine_state &state) noexcept {
        ++state.activeVoices;
        if (state.activeVoices > state.peakVoices.load(std::memory_order_relaxed)) {
            state.peakVoices.store(state.activeVoices, std::memory_order_relaxed);
        }
    }

    static void write_null_stats(engine_state &state) {
        if (state.nullBackend == nullptr) {
            return;
        }
        null_backend_stats &stats = state.nullBackend->mutable_stats();
        stats.listener = state.listener;
        stats.activeVoices = state.mix.active_voices();
        stats.underruns = state.mix.underruns();
        stats.commandsApplied = state.mix.commands_applied();
        for (u32 index = 0; index < MAX_VOICES; ++index) {
            const mixer_voice &voice = state.mix.slot(index);
            null_voice_stats &entry = stats.voices[index];
            entry.state = state.mix.slot_state(index);
            entry.cursorFrames = state.mix.slot_cursor(index);
            entry.gain = voice.gainCurrent;
            entry.pan = voice.panCurrent;
            entry.step = voice.step;
            entry.loop = voice.loop;
            entry.groupIndex = voice.groupIndex;
        }
        stats.groups.clear();
        for (const group_info &group : state.groups.infos()) {
            null_group_stats entry = {};
            entry.id = group.id;
            entry.name = group.name;
            entry.gain = group.gain;
            entry.effectiveGain = group.effectiveGain;
            entry.paused = group.paused;
            stats.groups.push_back(entry);
        }
    }

    /*-----------------------------------------------------------------------*/
    // Backend

    static void adopt_null_backend(engine_state &state) {
        std::unique_ptr<null_backend> fallback = std::make_unique<null_backend>();
        state.nullBackend = fallback.get();
        state.backend = std::move(fallback);
        // The null backend accepts any rate inside the supported range, which the desc already has.
        const core::result<void> started = state.backend->start(state.desc);
        CONJURE_ASSERT(started.has_value(), "the null backend cannot refuse a validated desc");
        static_cast<void>(started);
    }

    /// <summary>
    /// Device loss (docs/design/12_audio.md 3.8, A14). The backend reports LOST once; from then on
    /// the engine owns the retry loop, whatever state the backend publishes after a failed
    /// restart (a failed start() leaves it STOPPED), until a start succeeds or the attempts run
    /// out and the null backend takes over so voices keep finishing.
    /// </summary>
    static void recover_device(engine_state &state, f32 deltaSeconds) {
        if (!state.recovering) {
            if (state.backend->state() != device_state::LOST) {
                state.recoveryTimer = 0.0f;
                return;
            }
            state.recovering = true;
            state.recoveryAttempts = 0;
            state.recoveryTimer = 0.0f;
            CONJURE_LOG_WARN(
              LOG_AUDIO,
              "audio device lost; retrying every {} s, up to {} time(s)",
              state.settings.recoveryBackoffSeconds,
              state.settings.maxRecoveryAttempts
            );
        }

        state.recoveryTimer += deltaSeconds;
        if (state.recoveryTimer < state.settings.recoveryBackoffSeconds) {
            return;
        }
        state.recoveryTimer = 0.0f;
        ++state.recoveryAttempts;
        state.backend->stop();
        const core::result<void> restarted = state.backend->start(state.desc);
        if (restarted.has_value()) {
            state.recovering = false;
            state.deviceRestarts.fetch_add(1, std::memory_order_relaxed);
            CONJURE_LOG_INFO(LOG_AUDIO, "audio device recovered after {} attempt(s)", state.recoveryAttempts);
            state.recoveryAttempts = 0;
            return;
        }
        if (state.recoveryAttempts < state.settings.maxRecoveryAttempts) {
            CONJURE_LOG_DEBUG(
              LOG_AUDIO,
              "audio device restart {}/{} failed: {}",
              state.recoveryAttempts,
              state.settings.maxRecoveryAttempts,
              restarted.get_error()
            );
            return;
        }
        CONJURE_LOG_WARN(
          LOG_AUDIO,
          "audio device could not be recovered after {} attempt(s) ({}); falling back to the null backend",
          state.recoveryAttempts,
          restarted.get_error()
        );
        state.backendError = restarted.get_error();
        state.fellBackToNull.store(true, std::memory_order_relaxed);
        state.recovering = false;
        state.recoveryAttempts = 0;
        adopt_null_backend(state);
    }

    /*-----------------------------------------------------------------------*/
    // Voice stealing

    [[nodiscard]] static u32 choose_victim(const engine_state &state, u8 priority) noexcept {
        u32 victim = INVALID_INDEX;
        u8 victimPriority = 0;
        f32 victimGain = 0.0f;
        for (u32 entryIndex = 0; entryIndex < VOICE_TABLE_CAPACITY; ++entryIndex) {
            const voice_entry &entry = state.voices[entryIndex];
            const voice_slot &slot = entry.slot;
            if (!entry.live || !slot.hasMixerSlot || slot.stolen || is_terminal_state(slot.info.state)
              || slot.info.state == voice_state::STOPPING) {
                continue;
            }
            if (slot.params.loop && slot.params.retain) {
                continue;
            }
            if (slot.params.priority > priority) {
                continue;
            }
            const bool better = victim == INVALID_INDEX || slot.params.priority < victimPriority
              || (slot.params.priority == victimPriority && slot.info.effectiveGain < victimGain);
            if (better) {
                victim = entryIndex;
                victimPriority = slot.params.priority;
                victimGain = slot.info.effectiveGain;
            }
        }
        return victim;
    }
};

using helpers = conjure::audio::detail::audio_engine_helpers;

/*===========================================================================*/
/*===========================================================================*/
// audio_settings

core::result<void> audio_settings::validate() const {
    if (sampleRate < MIN_SAMPLE_RATE || sampleRate > MAX_SAMPLE_RATE) {
        return core::make_error(
          audio_error::INVALID_SETTINGS,
          "sample rate {} outside [{}, {}]",
          sampleRate,
          MIN_SAMPLE_RATE,
          MAX_SAMPLE_RATE
        );
    }
    if (periodFrames == 0) {
        return core::make_error(audio_error::INVALID_SETTINGS, "periodFrames must be positive");
    }
    if (!std::isfinite(masterGain) || !std::isfinite(dopplerFactor) || !std::isfinite(speedOfSound)) {
        return core::make_error(
          audio_error::INVALID_SETTINGS,
          "master gain, Doppler factor and speed of sound must be finite"
        );
    }
    if (dopplerFactor < 0.0f || speedOfSound <= 0.0f) {
        return core::make_error(
          audio_error::INVALID_SETTINGS,
          "Doppler factor must be >= 0 and the speed of sound positive"
        );
    }
    if (groups.size() > MAX_GROUPS - 1) {
        return core::make_error(
          audio_error::INVALID_SETTINGS,
          "{} groups listed; at most {} besides master",
          groups.size(),
          MAX_GROUPS - 1
        );
    }

    core::flat_hash_map<core::string_id, usize> byName = {};
    for (usize index = 0; index < groups.size(); ++index) {
        const group_desc &desc = groups[index];
        if (desc.name.is_null()) {
            return core::make_error(audio_error::INVALID_SETTINGS, "group {} has no name", index);
        }
        if (byName.contains(desc.name)) {
            return core::make_error(audio_error::INVALID_SETTINGS, "group '{}' is listed twice", desc.name);
        }
        byName.insert_or_assign(desc.name, index);
    }
    for (const group_desc &desc : groups) {
        if (!desc.parentName.is_null() && !byName.contains(desc.parentName)) {
            return core::make_error(
              audio_error::INVALID_SETTINGS,
              "group '{}' names an unknown parent '{}'",
              desc.name,
              desc.parentName
            );
        }
        if (desc.parentName.is_null() && desc.parent != MASTER_GROUP) {
            return core::make_error(
              audio_error::INVALID_SETTINGS,
              "group '{}' must name its parent (ids do not exist before construction)",
              desc.name
            );
        }
    }
    for (const group_desc &desc : groups) {
        u32 depth = 1;
        core::string_id current = desc.parentName;
        while (!current.is_null()) {
            ++depth;
            if (depth > MAX_GROUP_DEPTH) {
                return core::make_error(
                  audio_error::INVALID_SETTINGS,
                  "group '{}' is part of a cycle or deeper than {}",
                  desc.name,
                  MAX_GROUP_DEPTH
                );
            }
            const auto parentIndex = byName.find(current);
            if (parentIndex == byName.end()) {
                break;
            }
            current = groups[parentIndex->second].parentName;
        }
    }
    return {};
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

audio_engine::audio_engine(audio_settings settings) :
    mState(std::make_unique<state>()) {
    state &engine = *mState;
    engine.settings = std::move(settings);

    for (u32 index = 0; index < VOICE_TABLE_CAPACITY; ++index) {
        engine.voices[index].nextFree = index + 1 < VOICE_TABLE_CAPACITY ? index + 1 : INVALID_INDEX;
    }
    engine.freeHead = 0;
    engine.slotOwner.fill(INVALID_INDEX);
    engine.slotGeneration.fill(0);

    bool settingsValid = true;
    const core::result<void> validated = engine.settings.validate();
    if (!validated.has_value()) {
        CONJURE_LOG_ERROR(LOG_AUDIO, "invalid audio settings, using the null backend: {}", validated.get_error());
        engine.backendError = validated.get_error();
        settingsValid = false;
        engine.settings.sampleRate = std::clamp(engine.settings.sampleRate, MIN_SAMPLE_RATE, MAX_SAMPLE_RATE);
        engine.settings.periodFrames = std::max<u32>(engine.settings.periodFrames, 1);
        engine.settings.dopplerFactor = std::isfinite(engine.settings.dopplerFactor)
          ? std::max(engine.settings.dopplerFactor, 0.0f)
          : 0.0f;
        const bool speedUsable = std::isfinite(engine.settings.speedOfSound) && engine.settings.speedOfSound > 0.0f;
        engine.settings.speedOfSound = speedUsable ? engine.settings.speedOfSound : DEFAULT_SPEED_OF_SOUND;
        engine.settings.masterGain = clamp_group_gain(engine.settings.masterGain);
        engine.settings.groups.clear();
    }
    engine.settings.dopplerFactor = std::min(engine.settings.dopplerFactor, 10.0f);

    engine.mix.set_sample_rate(engine.settings.sampleRate);
    engine.desc.format.sampleRate = engine.settings.sampleRate;
    engine.desc.format.channels = static_cast<u16>(MIX_CHANNELS);
    engine.desc.format.format = sample_format::F32;
    engine.desc.periodFrames = engine.settings.periodFrames;
    engine.desc.deviceName = engine.settings.deviceName;
    engine.desc.callback = &mixer::render_callback;
    engine.desc.user = &engine.mix;

    const audio_backend_kind requested = settingsValid ? engine.settings.backend : audio_backend_kind::NULL_BACKEND;
    bool started = false;
    if (requested != audio_backend_kind::NULL_BACKEND) {
        core::result<std::unique_ptr<audio_backend>> created = create_backend(requested);
        if (created.has_value()) {
            engine.backend = std::move(created).value();
            if (engine.backend->kind() == audio_backend_kind::NULL_BACKEND) {
                engine.nullBackend = static_cast<null_backend *>(engine.backend.get());
            }
            const core::result<void> startResult = engine.backend->start(engine.desc);
            if (startResult.has_value()) {
                started = true;
            } else {
                CONJURE_LOG_WARN(
                  LOG_AUDIO,
                  "audio backend {} failed to start, using the null backend: {}",
                  requested,
                  startResult.get_error()
                );
                engine.backendError = startResult.get_error();
                engine.backend.reset();
                engine.nullBackend = nullptr;
            }
        } else {
            CONJURE_LOG_WARN(
              LOG_AUDIO,
              "audio backend {} unavailable, using the null backend: {}",
              requested,
              created.get_error()
            );
            engine.backendError = created.get_error();
        }
        if (!started) {
            engine.fellBackToNull.store(true, std::memory_order_relaxed);
        }
    }
    if (!started) {
        helpers::adopt_null_backend(engine);
    }
    engine.deviceStateMirror.store(static_cast<u8>(engine.backend->state()), std::memory_order_relaxed);

    // Groups from settings: parents are named, so create them in dependency order.
    std::vector<bool> created(engine.settings.groups.size(), false);
    bool progress = true;
    while (progress) {
        progress = false;
        for (usize index = 0; index < engine.settings.groups.size(); ++index) {
            if (created[index]) {
                continue;
            }
            const group_desc &desc = engine.settings.groups[index];
            if (!desc.parentName.is_null() && !engine.groups.find(desc.parentName).is_valid()) {
                continue;
            }
            const core::result<group_id> group = create_group(desc);
            if (!group.has_value()) {
                CONJURE_LOG_WARN(LOG_AUDIO, "settings group '{}' not created: {}", desc.name, group.get_error());
            }
            created[index] = true;
            progress = true;
        }
    }
    // validate() rules out unknown parents, so this only reports a parent whose own creation
    // failed at runtime; its subtree must not vanish silently.
    for (usize index = 0; index < engine.settings.groups.size(); ++index) {
        if (!created[index]) {
            const group_desc &desc = engine.settings.groups[index];
            CONJURE_LOG_WARN(
              LOG_AUDIO,
              "settings group '{}' not created: its parent '{}' does not exist",
              desc.name,
              desc.parentName
            );
        }
    }

    set_master_gain(engine.settings.masterGain);
    CONJURE_LOG_INFO(
      LOG_AUDIO,
      "audio engine ready on the {} backend at {} Hz ({} voices, {} groups)",
      engine.backend->kind(),
      engine.settings.sampleRate,
      MAX_VOICES,
      engine.groups.live_count()
    );
}

audio_engine::~audio_engine() {
    state &engine = *mState;

    // Stop the device first: afterwards the callback is not running and the rings are ours.
    engine.backend->stop();

    u32 retained = 0;
    for (voice_entry &entry : engine.voices) {
        if (entry.live && entry.slot.params.retain && !is_terminal_state(entry.slot.info.state)) {
            ++retained;
        }
    }
    if (retained > 0) {
        CONJURE_LOG_INFO(LOG_AUDIO, "audio engine destroyed with {} live retained voice(s)", retained);
    }

    // Decode jobs reference the stream slots; let every one finish before the slots go away.
    for (stream_slot &stream : engine.streams) {
        if (engine.settings.jobs != nullptr && stream.job.valid()) {
            engine.settings.jobs->wait(stream.job);
        }
        stream.decoder.close();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods: backend and device

audio_backend_kind audio_engine::backend_kind() const noexcept {
    return mState->backend->kind();
}

const core::error *audio_engine::last_backend_error() const noexcept {
    return mState->backendError.has_value() ? &*mState->backendError : nullptr;
}

null_backend *audio_engine::null_backend_instance() noexcept {
    return mState->nullBackend;
}

audio_format audio_engine::output_format() const noexcept {
    return mState->desc.format;
}

core::result<std::vector<audio_device_info>> audio_engine::enumerate_devices() const {
    return mState->backend->enumerate_devices();
}

core::result<void> audio_engine::select_device(std::string_view deviceName) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "audio_engine::select_device is main-thread only");
    state &engine = *mState;
    const std::string previous = engine.desc.deviceName;
    engine.desc.deviceName = std::string(deviceName);
    engine.backend->stop();
    engine.recovering = false;
    engine.recoveryAttempts = 0;
    const core::result<void> started = engine.backend->start(engine.desc);
    if (started.has_value()) {
        engine.settings.deviceName = engine.desc.deviceName;
        engine.deviceRestarts.fetch_add(1, std::memory_order_relaxed);
        return {};
    }
    engine.desc.deviceName = previous;
    const core::result<void> restored = engine.backend->start(engine.desc);
    if (!restored.has_value()) {
        CONJURE_LOG_WARN(LOG_AUDIO, "previous audio device could not be restored either: {}", restored.get_error());
        engine.fellBackToNull.store(true, std::memory_order_relaxed);
        helpers::adopt_null_backend(engine);
    }
    return started.get_error();
}

const audio_settings &audio_engine::settings() const noexcept {
    return mState->settings;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods: frame

void audio_engine::update(f32 deltaSeconds) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "audio_engine::update is main-thread only");
    state &engine = *mState;
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
        deltaSeconds = 0.0f;
    }

    helpers::recover_device(engine, deltaSeconds);
    helpers::flush_spill(engine);
    engine.backend->update(deltaSeconds);
    const device_state published = engine.recovering ? device_state::LOST : engine.backend->state();
    engine.deviceStateMirror.store(static_cast<u8>(published), std::memory_order_relaxed);

    helpers::drain_events(engine);
    helpers::reconcile(engine);
    helpers::start_pending_voices(engine);
    helpers::spatial_pass(engine);
    helpers::streaming_pass(engine);
    helpers::start_pending_voices(engine);
    helpers::flush_spill(engine);
    helpers::write_null_stats(engine);
    helpers::publish_mirrors(engine);
}

void audio_engine::end_frame() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "audio_engine::end_frame is main-thread only");
    state &engine = *mState;

    for (u32 entryIndex = 0; entryIndex < VOICE_TABLE_CAPACITY; ++entryIndex) {
        voice_entry &entry = engine.voices[entryIndex];
        if (entry.live && !entry.slot.params.retain && is_terminal_state(entry.slot.info.state)) {
            helpers::free_entry(engine, entryIndex);
        }
    }
    for (stream_slot &stream : engine.streams) {
        if (stream.releasing && !stream.jobInFlight.load(std::memory_order_acquire)) {
            helpers::free_stream_slot(engine, stream);
        }
    }

    engine.finishedPublished.swap(engine.finished);
    engine.finished.clear();
}

std::span<const voice_id> audio_engine::finished_voices() const noexcept {
    return mState->finishedPublished;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods: voices

core::result<voice_id> audio_engine::play(const audio_clip &clip, const play_params &params, u32 clipVersion) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "audio_engine::play is main-thread only");
    state &engine = *mState;
    if (!clip.is_valid()) {
        return core::make_error(audio_error::INVALID_CLIP, "the clip carries no samples for its storage kind");
    }

    const play_params clamped = clamp_play_params(params);
    if (!engine.groups.is_valid(clamped.group)) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "play() with an invalid or stale group id");
        return core::make_error(
          audio_error::INVALID_GROUP,
          "play(): group {}:{} does not exist",
          clamped.group.index,
          clamped.group.generation
        );
    }

    stream_slot *stream = nullptr;
    if (clip.is_streamed()) {
        stream = helpers::acquire_stream_slot(engine);
        if (stream == nullptr) {
            engine.voicesDropped.fetch_add(1, std::memory_order_relaxed);
            return core::make_error(
              audio_error::STREAM_SLOT_LIMIT,
              "all {} streaming voices are in use",
              MAX_STREAMING_VOICES
            );
        }
    }

    const u32 entryIndex = helpers::allocate_entry(engine);
    if (entryIndex == INVALID_INDEX) {
        if (stream != nullptr) {
            helpers::free_stream_slot(engine, *stream);
        }
        engine.voicesDropped.fetch_add(1, std::memory_order_relaxed);
        return core::make_error(audio_error::VOICE_LIMIT_REACHED, "the voice table is full");
    }

    voice_entry &entry = engine.voices[entryIndex];
    voice_slot &slot = entry.slot;
    slot.params = clamped;
    slot.attenuation.model = clamped.attenuation;
    slot.attenuation.minDistance = clamped.minDistance;
    slot.attenuation.maxDistance = clamped.maxDistance;
    slot.attenuation.rolloff = clamped.rolloff;
    slot.userPaused = clamped.startPaused;
    slot.clipRate = clip.info.format.sampleRate;
    slot.clipFrames = clip.info.frameCount;
    slot.channels = clip.info.format.channels;
    slot.info.volume = clamped.volume;
    slot.info.pitch = clamped.pitch;
    slot.info.group = slot.params.group;
    slot.info.loop = clamped.loop;
    slot.info.spatial = clamped.spatial;
    slot.info.retain = clamped.retain;
    slot.info.streamed = clip.is_streamed();
    slot.info.clipVersion = clipVersion;
    slot.info.clipFrames = clip.info.frameCount;

    if (stream != nullptr) {
        // The slot is attached to the entry before anything can fail, so free_entry() releases it
        // on every error path; otherwise a clip that does not open would leak its stream slot.
        slot.stream = stream;
        stream->ownerEntry = entryIndex;
        stream->encoded = clip.encoded;
        stream->channels = clip.info.format.channels;
        stream->clipFrames = clip.info.frameCount;
        stream->loop.store(clamped.loop, std::memory_order_release);
        stream->ring.reset(stream->channels);
        // The loader probed the clip, so its length is known: a Vorbis stream is not decoded
        // once more just to count it on the main thread.
        const core::result<void> opened = stream->decoder.open(*stream->encoded, clip.info.frameCount);
        if (!opened.has_value()) {
            helpers::free_entry(engine, entryIndex);
            engine.voicesDropped.fetch_add(1, std::memory_order_relaxed);
            return opened.get_error();
        }
        if (stream->decoder.info().frameCount > 0) {
            stream->clipFrames = stream->decoder.info().frameCount;
            slot.clipFrames = stream->clipFrames;
            slot.info.clipFrames = stream->clipFrames;
        }
        const f64 startFrames = clamped.startSeconds * static_cast<f64>(slot.clipRate);
        const u64 startFrame = static_cast<u64>(std::clamp(startFrames, 0.0, static_cast<f64>(slot.clipFrames)));
        if (startFrame > 0 && !stream->decoder.seek(startFrame).has_value()) {
            CONJURE_LOG_WARN(LOG_AUDIO, "streamed voice could not seek to its start; playing from the beginning");
        }
        slot.streamStartFrame = stream->decoder.position();
        helpers::feed_stream(engine, *stream);
    } else {
        slot.pcm = clip.pcm;
    }

    helpers::set_mirror_state(entry, voice_state::PRIMING);
    helpers::count_started_voice(engine);

    const u32 mixerIndex = helpers::find_free_mixer_slot(engine);
    if (mixerIndex == INVALID_INDEX) {
        const u32 victim = helpers::choose_victim(engine, clamped.priority);
        if (victim == INVALID_INDEX) {
            --engine.activeVoices;
            helpers::free_entry(engine, entryIndex);
            engine.voicesDropped.fetch_add(1, std::memory_order_relaxed);
            CONJURE_LOG_TRACE(
              LOG_AUDIO,
              "voice limit reached: {} voices busy and nothing stealable at priority {}",
              MAX_VOICES,
              clamped.priority
            );
            return core::make_error(
              audio_error::VOICE_LIMIT_REACHED,
              "{} voices are busy and none can be stolen",
              MAX_VOICES
            );
        }
        voice_slot &victimSlot = engine.voices[victim].slot;
        victimSlot.stolen = true;
        slot.awaitingSlot = true;
        slot.mixerIndex = victimSlot.mixerIndex;
        stop(helpers::id_of(engine, victim), STEAL_FADE_SECONDS);
        helpers::publish_mirror(entry);
        return helpers::id_of(engine, entryIndex);
    }

    if (stream == nullptr || helpers::stream_primed(*stream)) {
        helpers::start_mixer_voice(engine, entryIndex, mixerIndex);
    }
    helpers::publish_mirror(entry);
    return helpers::id_of(engine, entryIndex);
}

core::result<voice_id> audio_engine::play(const assets::asset_ref<audio_clip> &clip, const play_params &params) {
    const audio_clip *payload = clip.get();
    if (payload == nullptr) {
        return core::make_error(audio_error::CLIP_NOT_LOADED, "clip {} is not loaded", clip.id());
    }
    return play(*payload, params, clip.version());
}

core::result<voice_id> audio_engine::play(std::shared_ptr<const pcm_buffer> pcm, const play_params &params) {
    if (pcm == nullptr || pcm->frame_count() == 0) {
        return core::make_error(audio_error::INVALID_CLIP, "play() needs a non-empty PCM buffer");
    }
    return play(make_pcm_clip(std::move(pcm)), params, 0);
}

void audio_engine::stop(voice_id voice, f32 fadeSeconds) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "stop() on an invalid voice id");
        return;
    }
    voice_slot &slot = entry->slot;
    if (is_terminal_state(slot.info.state) || slot.info.state == voice_state::STOPPING) {
        return;
    }
    if (!slot.hasMixerSlot) {
        // Never reached the mixer (priming or waiting for a slot): it ends here.
        const u32 entryIndex = voice.key.index;
        helpers::on_voice_ended(engine, entryIndex, voice_state::STOPPED, slot.info.cursorFrames);
        return;
    }
    voice_command command = helpers::voice_message(slot, command_kind::STOP);
    command.fadeFrames = seconds_to_frames(fadeSeconds, engine.mix.sample_rate());
    helpers::push_command(engine, command);
    helpers::set_mirror_state(*entry, voice_state::STOPPING);
}

void audio_engine::stop_all(f32 fadeSeconds) {
    state &engine = *mState;
    for (u32 entryIndex = 0; entryIndex < VOICE_TABLE_CAPACITY; ++entryIndex) {
        if (engine.voices[entryIndex].live) {
            stop(helpers::id_of(engine, entryIndex), fadeSeconds);
        }
    }
}

void audio_engine::pause(voice_id voice) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "pause() on an invalid voice id");
        return;
    }
    voice_slot &slot = entry->slot;
    slot.userPaused = true;
    if (slot.info.state != voice_state::PLAYING) {
        return;
    }
    if (slot.hasMixerSlot) {
        helpers::push_command(engine, helpers::voice_message(slot, command_kind::PAUSE));
    }
    helpers::set_mirror_state(*entry, voice_state::PAUSED);
}

void audio_engine::resume(voice_id voice) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "resume() on an invalid voice id");
        return;
    }
    voice_slot &slot = entry->slot;
    slot.userPaused = false;
    if (slot.info.state != voice_state::PAUSED || slot.seekPhase != seek_phase::NONE) {
        return;
    }
    if (slot.hasMixerSlot) {
        helpers::push_command(engine, helpers::voice_message(slot, command_kind::RESUME));
    }
    helpers::set_mirror_state(*entry, voice_state::PLAYING);
}

void audio_engine::release(voice_id voice) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "release() on an invalid voice id");
        return;
    }
    if (is_terminal_state(entry->slot.info.state)) {
        helpers::free_entry(engine, voice.key.index);
        return;
    }
    entry->slot.releaseRequested = true;
    stop(voice, DEFAULT_RAMP_SECONDS);
}

void audio_engine::set_volume(voice_id voice, f32 volume) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_volume() on an invalid voice id");
        return;
    }
    CONJURE_ASSERT(std::isfinite(volume), "set_volume: volume must be finite");
    entry->slot.params.volume = clamp_volume(volume);
    entry->slot.info.volume = entry->slot.params.volume;
    if (entry->slot.hasMixerSlot) {
        helpers::send_gain(engine, entry->slot, seconds_to_frames(DEFAULT_RAMP_SECONDS, engine.mix.sample_rate()));
    }
    helpers::update_effective_gain(engine, entry->slot);
    helpers::publish_mirror(*entry);
}

void audio_engine::set_pitch(voice_id voice, f32 pitch) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_pitch() on an invalid voice id");
        return;
    }
    CONJURE_ASSERT(std::isfinite(pitch), "set_pitch: pitch must be finite");
    entry->slot.params.pitch = clamp_pitch(pitch);
    entry->slot.info.pitch = entry->slot.params.pitch;
    if (entry->slot.hasMixerSlot) {
        helpers::send_step(engine, entry->slot);
    }
    helpers::publish_mirror(*entry);
}

void audio_engine::set_pan(voice_id voice, f32 pan) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_pan() on an invalid voice id");
        return;
    }
    CONJURE_ASSERT(std::isfinite(pan), "set_pan: pan must be finite");
    entry->slot.params.pan = clamp_pan(pan);
    if (entry->slot.hasMixerSlot && !entry->slot.params.spatial) {
        helpers::send_pan(engine, entry->slot);
    }
}

void audio_engine::set_loop(voice_id voice, bool loop) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_loop() on an invalid voice id");
        return;
    }
    entry->slot.params.loop = loop;
    entry->slot.info.loop = loop;
    if (entry->slot.stream != nullptr) {
        entry->slot.stream->loop.store(loop, std::memory_order_release);
    }
    if (entry->slot.hasMixerSlot) {
        voice_command command = helpers::voice_message(entry->slot, command_kind::SET_LOOP);
        command.payload.flag = loop;
        helpers::push_command(engine, command);
    }
    helpers::publish_mirror(*entry);
}

void audio_engine::set_spatial(voice_id voice, bool spatial) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_spatial() on an invalid voice id");
        return;
    }
    if (entry->slot.params.spatial == spatial) {
        return;
    }
    entry->slot.params.spatial = spatial;
    entry->slot.info.spatial = spatial;
    if (entry->slot.hasMixerSlot && entry->slot.channels == 2) {
        // A stereo clip is downmixed to mono before the constant-power pan only while spatial (A10).
        voice_command command = helpers::voice_message(entry->slot, command_kind::SET_DOWNMIX);
        command.payload.flag = spatial;
        helpers::push_command(engine, command);
    }
    helpers::sync_voice(engine, entry->slot, true);
    helpers::publish_mirror(*entry);
}

void audio_engine::set_priority(voice_id voice, u8 priority) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_priority() on an invalid voice id");
        return;
    }
    entry->slot.params.priority = priority;
}

void audio_engine::set_group(voice_id voice, group_id group) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_group() on an invalid voice id");
        return;
    }
    if (!engine.groups.is_valid(group)) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_group() with an invalid group id");
        return;
    }
    entry->slot.params.group = group;
    entry->slot.info.group = group;
    if (entry->slot.hasMixerSlot) {
        voice_command command = helpers::voice_message(entry->slot, command_kind::SET_GROUP);
        command.groupIndex = group.index;
        helpers::push_command(engine, command);
    }
    helpers::update_effective_gain(engine, entry->slot);
    helpers::publish_mirror(*entry);
}

void audio_engine::set_position(voice_id voice, math::vec3 position, math::vec3 velocity) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_position() on an invalid voice id");
        return;
    }
    const bool finite = std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z)
      && std::isfinite(velocity.x) && std::isfinite(velocity.y) && std::isfinite(velocity.z);
    CONJURE_ASSERT(finite, "set_position: position and velocity must be finite");
    if (!finite) {
        return;
    }
    entry->slot.position = position;
    entry->slot.velocity = velocity;
    entry->slot.hasPosition = true;
}

void audio_engine::set_positions(std::span<const voice_transform> transforms) {
    for (const voice_transform &transform : transforms) {
        set_position(transform.voice, transform.position, transform.velocity);
    }
}

void audio_engine::set_attenuation(voice_id voice, const attenuation_params &params) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_attenuation() on an invalid voice id");
        return;
    }
    entry->slot.attenuation = clamp_attenuation(params);
    entry->slot.params.attenuation = entry->slot.attenuation.model;
    entry->slot.params.minDistance = entry->slot.attenuation.minDistance;
    entry->slot.params.maxDistance = entry->slot.attenuation.maxDistance;
    entry->slot.params.rolloff = entry->slot.attenuation.rolloff;
}

void audio_engine::apply_params(voice_id voice, const play_params &params) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "apply_params() on an invalid voice id");
        return;
    }
    const play_params clamped = clamp_play_params(params);
    set_volume(voice, clamped.volume);
    set_pitch(voice, clamped.pitch);
    set_pan(voice, clamped.pan);
    set_loop(voice, clamped.loop);
    set_priority(voice, clamped.priority);
    if (engine.groups.is_valid(clamped.group)) {
        set_group(voice, clamped.group);
    }
    attenuation_params attenuation = {};
    attenuation.model = clamped.attenuation;
    attenuation.minDistance = clamped.minDistance;
    attenuation.maxDistance = clamped.maxDistance;
    attenuation.rolloff = clamped.rolloff;
    set_attenuation(voice, attenuation);
    set_spatial(voice, clamped.spatial);
}

void audio_engine::apply_source(voice_id voice, const audio_source &source, group_id group) {
    apply_params(voice, play_params_from(source, group));
}

void audio_engine::seek(voice_id voice, f64 seconds) {
    state &engine = *mState;
    voice_entry *entry = helpers::resolve(engine, voice);
    if (entry == nullptr) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "seek() on an invalid voice id");
        return;
    }
    voice_slot &slot = entry->slot;
    if (is_terminal_state(slot.info.state) || slot.info.state == voice_state::STOPPING) {
        return;
    }
    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    const f64 frames = std::round(seconds * static_cast<f64>(slot.clipRate));
    const u64 frame = slot.clipFrames == 0
      ? 0
      : static_cast<u64>(std::clamp(frames, 0.0, static_cast<f64>(slot.clipFrames - 1)));

    if (slot.stream != nullptr) {
        // Every streamed seek goes through streaming_pass(), which resets ring and decoder only
        // once no decode job is in flight and nothing pops from the ring (3.7). A seek issued
        // while one is pending simply retargets it.
        slot.seekFrame = frame;
        if (slot.seekPhase != seek_phase::NONE) {
            return;
        }
        if (!slot.hasMixerSlot || slot.info.state == voice_state::PAUSED) {
            slot.seekPhase = seek_phase::WAIT_RESET;
            return;
        }
        slot.seekPhase = seek_phase::WAIT_PAUSE;
        helpers::push_command(engine, helpers::voice_message(slot, command_kind::PAUSE));
        return;
    }

    if (!slot.hasMixerSlot) {
        slot.params.startSeconds = seconds;
        return;
    }
    voice_command command = helpers::voice_message(slot, command_kind::SEEK);
    command.payload.frame = frame;
    helpers::push_command(engine, command);
    slot.info.cursorFrames = frame;
    helpers::publish_mirror(*entry);
}

bool audio_engine::is_valid(voice_id voice) const noexcept {
    const u32 index = voice.key.index;
    if (!voice.is_valid() || index >= VOICE_TABLE_CAPACITY) {
        return false;
    }
    const voice_entry &entry = mState->voices[index];
    if (entry.generation.load(std::memory_order_acquire) != voice.key.generation) {
        return false;
    }
    return static_cast<voice_state>(entry.state.load(std::memory_order_acquire)) != voice_state::FREE;
}

bool audio_engine::is_playing(voice_id voice) const noexcept {
    const u32 index = voice.key.index;
    if (!voice.is_valid() || index >= VOICE_TABLE_CAPACITY) {
        return false;
    }
    const voice_entry &entry = mState->voices[index];
    if (entry.generation.load(std::memory_order_acquire) != voice.key.generation) {
        return false;
    }
    return is_active_state(static_cast<voice_state>(entry.state.load(std::memory_order_acquire)));
}

std::optional<voice_info> audio_engine::voice(voice_id voice) const noexcept {
    if (!is_valid(voice)) {
        return std::nullopt;
    }
    const voice_entry &entry = mState->voices[voice.key.index];
    const voice_mirror mirror = helpers::read_mirror(entry);
    if (entry.generation.load(std::memory_order_acquire) != voice.key.generation) {
        return std::nullopt;   // released while the snapshot was taken
    }
    voice_info info = mirror.info;
    info.state = static_cast<voice_state>(entry.state.load(std::memory_order_acquire));
    const bool mixerOwnsCursor = mirror.hasMixerSlot && mirror.mixerIndex < MAX_VOICES
      && !is_terminal_state(info.state)
      && mState->mix.slot_generation(mirror.mixerIndex) == mirror.mixerGeneration;
    if (mixerOwnsCursor) {
        info.cursorFrames = mState->mix.slot_cursor(mirror.mixerIndex);
    }
    return info;
}

u32 audio_engine::active_voice_count() const noexcept {
    u32 count = 0;
    for (const voice_entry &entry : mState->voices) {
        const voice_state state = static_cast<voice_state>(entry.state.load(std::memory_order_acquire));
        if (state != voice_state::FREE && !is_terminal_state(state)) {
            ++count;
        }
    }
    return count;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods: listener

void audio_engine::set_listener(const listener_state &listener) {
    state &engine = *mState;
    engine.listener = clamp_listener(listener);
}

const listener_state &audio_engine::listener() const noexcept {
    return mState->listener;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods: groups

core::result<group_id> audio_engine::create_group(const group_desc &desc) {
    state &engine = *mState;
    CONJURE_TRY_ASSIGN(const group_id created, engine.groups.create(desc));
    voice_command command = {};
    command.kind = command_kind::SET_GROUP_GAIN;
    command.groupIndex = created.index;
    command.payload.scalar = engine.groups.effective_gain(created.index);
    helpers::push_command(engine, command);
    voice_command pausedCommand = {};
    pausedCommand.kind = command_kind::SET_GROUP_PAUSED;
    pausedCommand.groupIndex = created.index;
    pausedCommand.payload.flag = false;
    helpers::push_command(engine, pausedCommand);
    return created;
}

core::result<void> audio_engine::remove_group(group_id group) {
    state &engine = *mState;
    CONJURE_TRY_ASSIGN(const group_removal removal, engine.groups.remove(group));
    const group_id parent = engine.groups.id_of(removal.parentIndex);

    for (u32 entryIndex = 0; entryIndex < VOICE_TABLE_CAPACITY; ++entryIndex) {
        voice_entry &entry = engine.voices[entryIndex];
        if (!entry.live || entry.slot.params.group.index != group.index) {
            continue;
        }
        entry.slot.params.group = parent;
        entry.slot.info.group = parent;
        if (entry.slot.hasMixerSlot) {
            voice_command command = helpers::voice_message(entry.slot, command_kind::SET_GROUP);
            command.groupIndex = parent.index;
            helpers::push_command(engine, command);
        }
        helpers::update_effective_gain(engine, entry.slot);
        helpers::publish_mirror(entry);
    }
    for (const u8 index : removal.changedGains) {
        voice_command command = {};
        command.kind = command_kind::SET_GROUP_GAIN;
        command.groupIndex = index;
        command.payload.scalar = engine.groups.effective_gain(index);
        helpers::push_command(engine, command);
    }
    voice_command reset = {};
    reset.kind = command_kind::SET_GROUP_PAUSED;
    reset.groupIndex = group.index;
    reset.payload.flag = false;
    helpers::push_command(engine, reset);
    return {};
}

group_id audio_engine::find_group(core::string_id name) const noexcept {
    return mState->groups.find(name);
}

void audio_engine::set_group_gain(group_id group, f32 gain) {
    state &engine = *mState;
    CONJURE_ASSERT(std::isfinite(gain), "set_group_gain: gain must be finite");
    if (!engine.groups.is_valid(group)) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_group_gain() with an invalid group id");
        return;
    }
    const group_change_list changed = engine.groups.set_gain(group, gain);
    for (const u8 index : changed) {
        voice_command command = {};
        command.kind = command_kind::SET_GROUP_GAIN;
        command.groupIndex = index;
        command.payload.scalar = engine.groups.effective_gain(index);
        helpers::push_command(engine, command);
    }
    for (voice_entry &entry : engine.voices) {
        if (entry.live) {
            helpers::update_effective_gain(engine, entry.slot);
            helpers::publish_mirror(entry);
        }
    }
}

void audio_engine::set_group_paused(group_id group, bool paused) {
    state &engine = *mState;
    if (!engine.groups.is_valid(group)) {
        CONJURE_LOG_TRACE(LOG_AUDIO, "set_group_paused() with an invalid group id");
        return;
    }
    engine.groups.set_paused(group, paused);
    voice_command command = {};
    command.kind = command_kind::SET_GROUP_PAUSED;
    command.groupIndex = group.index;
    command.payload.flag = paused;
    helpers::push_command(engine, command);
}

std::optional<group_info> audio_engine::group(group_id group) const noexcept {
    return mState->groups.info(group);
}

std::span<const group_info> audio_engine::groups() const noexcept {
    return mState->groups.infos();
}

void audio_engine::set_master_gain(f32 gain) {
    set_group_gain(MASTER_GROUP, gain);
}

void audio_engine::set_paused(bool paused) {
    state &engine = *mState;
    engine.masterPaused = paused;
    voice_command command = {};
    command.kind = command_kind::SET_MASTER_PAUSED;
    command.payload.flag = paused;
    helpers::push_command(engine, command);
}

bool audio_engine::is_paused() const noexcept {
    return mState->masterPaused;
}

audio_engine_stats audio_engine::stats() const noexcept {
    const state &engine = *mState;
    audio_engine_stats stats = {};
    stats.activeVoices = engine.mix.active_voices();
    stats.peakVoices = engine.peakVoices.load(std::memory_order_relaxed);
    stats.streamingVoices = engine.streamingVoices.load(std::memory_order_relaxed);
    stats.voicesDropped = engine.voicesDropped.load(std::memory_order_relaxed);
    stats.commandsSent = engine.commandsSent.load(std::memory_order_relaxed);
    stats.commandsSpilled = engine.commandsSpilled.load(std::memory_order_relaxed);
    stats.eventsReceived = engine.eventsReceived.load(std::memory_order_relaxed);
    stats.eventsDropped = engine.mix.events_dropped();
    stats.renderCalls = engine.mix.render_calls();
    stats.framesRendered = engine.mix.frames_rendered();
    stats.underruns = engine.mix.underruns();
    stats.loops = engine.mix.loops();
    stats.deviceRestarts = engine.deviceRestarts.load(std::memory_order_relaxed);
    stats.fellBackToNull = engine.fellBackToNull.load(std::memory_order_relaxed);
    stats.outputFormat = engine.desc.format;
    stats.deviceState = static_cast<device_state>(engine.deviceStateMirror.load(std::memory_order_relaxed));
    return stats;
}
