#include "audio/mixer.h"
#include "conjure/audio/clip.h"

#include "conjure/core/math.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr f32 STOP_SILENCE_THRESHOLD = 1.0e-4f;

    [[nodiscard]] inline u8 raw(voice_state state) noexcept {
        return static_cast<u8>(state);
    }

    [[nodiscard]] inline voice_state state_of(const mixer_voice &voice) noexcept {
        return static_cast<voice_state>(voice.state.load(std::memory_order_relaxed));
    }

    inline void set_state(mixer_voice &voice, voice_state state) noexcept {
        voice.state.store(raw(state), std::memory_order_release);
    }

    /// <summary>
    /// Constant-power pan gains for a mono signal.
    /// </summary>
    inline void constant_power_pan(f32 pan, f32 &left, f32 &right) noexcept {
        const f32 angle = (pan + 1.0f) * math::QUARTER_PI;
        left = std::cos(angle);
        right = std::sin(angle);
    }

    /// <summary>
    /// Balance gains for a stereo signal: unity at centre, the far channel fades linearly.
    /// </summary>
    inline void balance_pan(f32 pan, f32 &left, f32 &right) noexcept {
        left = pan <= 0.0f ? 1.0f : 1.0f - pan;
        right = pan >= 0.0f ? 1.0f : 1.0f + pan;
    }

    inline void start_ramp(f32 current, f32 target, u32 frames, f32 &step, u32 &remaining) noexcept {
        remaining = frames == 0 ? 1 : frames;
        step = (target - current) / static_cast<f32>(remaining);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

mixer::mixer(u32 sampleRate) :
    mSampleRate(sampleRate) {
    mGroupGain.fill(1.0f);
    mGroupPaused.fill(false);
}

mixer::~mixer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void mixer::render(f32 *out, u32 frameCount) noexcept {
    mRenderCalls.fetch_add(1, std::memory_order_relaxed);

    u32 rendered = 0;
    while (rendered < frameCount) {
        const u32 blockFrames = std::min(frameCount - rendered, MIX_BLOCK_FRAMES);
        render_block(blockFrames);
        std::memcpy(
          out + static_cast<usize>(rendered) * MIX_CHANNELS,
          mMixBlock.data(),
          static_cast<usize>(blockFrames) * MIX_CHANNELS * sizeof(f32)
        );
        rendered += blockFrames;
    }

    mFramesRendered.fetch_add(frameCount, std::memory_order_relaxed);
}

void mixer::render_callback(void *user, f32 *output, u32 frameCount) noexcept {
    static_cast<mixer *>(user)->render(output, frameCount);
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods: commands

void mixer::apply_commands() noexcept {
    voice_command command = {};
    u32 budget = command_ring::capacity();
    while (budget > 0 && mCommands.try_pop(command)) {
        apply_command(command);
        --budget;
    }
}

void mixer::apply_command(const voice_command &command) noexcept {
    mCommandsApplied.fetch_add(1, std::memory_order_relaxed);

    switch (command.kind) {
        case command_kind::SET_GROUP_GAIN:
            if (command.groupIndex < MAX_GROUPS) {
                mGroupGain[command.groupIndex] = command.payload.scalar;
            }
            return;
        case command_kind::SET_GROUP_PAUSED:
            if (command.groupIndex < MAX_GROUPS) {
                mGroupPaused[command.groupIndex] = command.payload.flag;
            }
            return;
        case command_kind::SET_MASTER_PAUSED:
            mMasterPaused = command.payload.flag;
            return;
        default:
            break;
    }

    if (command.voiceIndex >= MAX_VOICES) {
        mCommandsDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    mixer_voice &voice = mVoices[command.voiceIndex];

    if (command.kind == command_kind::PLAY) {
        apply_play(voice, command);
        return;
    }

    // Everything below addresses a live occupant: a mismatching generation is stale traffic.
    if (voice.generation.load(std::memory_order_relaxed) != command.generation) {
        mCommandsDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const voice_state current = state_of(voice);
    switch (command.kind) {
        case command_kind::STOP:
            if (current == voice_state::PAUSED) {
                // A paused voice is silent already; ramping it down would make it audible for the
                // length of the fade. It ends where it stands.
                finish_voice(voice, command.voiceIndex, event_kind::STOPPED);
                return;
            }
            if (current == voice_state::PLAYING || current == voice_state::STOPPING) {
                voice.gainTarget = 0.0f;
                start_ramp(voice.gainCurrent, 0.0f, command.fadeFrames, voice.gainStep, voice.gainRampFrames);
                set_state(voice, voice_state::STOPPING);
            }
            return;
        case command_kind::PAUSE:
            if (current == voice_state::PLAYING) {
                set_state(voice, voice_state::PAUSED);
                emit(event_kind::PAUSED, command.voiceIndex, voice, static_cast<u64>(voice.cursor));
            }
            return;
        case command_kind::RESUME:
            if (current == voice_state::PAUSED) {
                set_state(voice, voice_state::PLAYING);
                emit(event_kind::RESUMED, command.voiceIndex, voice, static_cast<u64>(voice.cursor));
            }
            return;
        case command_kind::SET_GAIN:
            if (current != voice_state::STOPPING) {
                voice.gainTarget = command.payload.scalar;
                start_ramp(
                  voice.gainCurrent,
                  voice.gainTarget,
                  command.fadeFrames,
                  voice.gainStep,
                  voice.gainRampFrames
                );
            }
            return;
        case command_kind::SET_PAN:
            voice.panTarget = command.payload.scalar;
            start_ramp(voice.panCurrent, voice.panTarget, command.fadeFrames, voice.panStep, voice.panRampFrames);
            return;
        case command_kind::SET_STEP:
            voice.step = command.payload.step;
            return;
        case command_kind::SET_LOOP:
            voice.loop = command.payload.flag;
            return;
        case command_kind::SEEK:
            if (voice.clipFrames > 0) {
                const u64 frame = std::min(command.payload.frame, voice.clipFrames - 1);
                voice.cursor = static_cast<f64>(frame);
                voice.publishedCursor.store(frame, std::memory_order_relaxed);
                voice.heldCount = 0;
                voice.heldLast = false;
                voice.streamFrac = 0.0;
            }
            return;
        case command_kind::SET_GROUP:
            voice.groupIndex = static_cast<u8>(command.groupIndex < MAX_GROUPS ? command.groupIndex : 0);
            return;
        case command_kind::SET_DOWNMIX:
            voice.downmix = command.payload.flag && voice.channels == 2;
            return;
        case command_kind::PLAY:
        case command_kind::SET_GROUP_GAIN:
        case command_kind::SET_GROUP_PAUSED:
        case command_kind::SET_MASTER_PAUSED:
            return;
    }
}

void mixer::apply_play(mixer_voice &voice, const voice_command &command) noexcept {
    const play_payload &play = command.payload.play;

    // Every plain field and the state are written first; the generation is published last with
    // release semantics (see the ordering contract on mixer_voice), so a main-thread reader that
    // sees the new occupant's generation never pairs it with the previous occupant's terminal state.
    voice.samples = play.pcm != nullptr ? play.pcm->samples().data() : nullptr;
    voice.clipFrames = play.pcm != nullptr ? play.pcm->frame_count() : play.clipFrames;
    voice.stream = play.stream;
    voice.channels = static_cast<u16>(play.channels == 2 ? 2 : 1);
    voice.loop = play.loop;
    voice.downmix = play.downmix && voice.channels == 2;
    voice.groupIndex = static_cast<u8>(command.groupIndex < MAX_GROUPS ? command.groupIndex : 0);
    voice.step = play.step;
    voice.gainCurrent = play.gain;
    voice.gainTarget = play.gain;
    voice.gainStep = 0.0f;
    voice.gainRampFrames = 0;
    voice.panCurrent = play.pan;
    voice.panTarget = play.pan;
    voice.panStep = 0.0f;
    voice.panRampFrames = 0;
    voice.heldCount = 0;
    voice.heldLast = false;
    voice.streamFrac = 0.0;
    voice.underrunReported = false;

    const u64 startFrame = voice.clipFrames > 0 ? std::min(play.startFrame, voice.clipFrames - 1) : 0;
    voice.cursor = static_cast<f64>(startFrame);
    voice.publishedCursor.store(startFrame, std::memory_order_relaxed);

    set_state(voice, play.startPaused ? voice_state::PAUSED : voice_state::PLAYING);
    voice.generation.store(command.generation, std::memory_order_release);
    emit(event_kind::STARTED, command.voiceIndex, voice, startFrame);
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods: rendering

void mixer::render_block(u32 frameCount) noexcept {
    apply_commands();
    std::fill_n(mMixBlock.data(), static_cast<usize>(frameCount) * MIX_CHANNELS, 0.0f);

    u32 active = 0;
    for (u32 index = 0; index < MAX_VOICES; ++index) {
        mixer_voice &voice = mVoices[index];
        const voice_state state = state_of(voice);
        if (state != voice_state::PLAYING && state != voice_state::PAUSED && state != voice_state::STOPPING) {
            continue;
        }
        ++active;
        if (state == voice_state::PAUSED || mMasterPaused || mGroupPaused[voice.groupIndex]) {
            continue;
        }
        render_voice(voice, index, frameCount);
    }
    mActiveVoices.store(active, std::memory_order_relaxed);

    for (usize sample = 0; sample < static_cast<usize>(frameCount) * MIX_CHANNELS; ++sample) {
        mMixBlock[sample] = std::clamp(mMixBlock[sample], -1.0f, 1.0f);
    }
}

void mixer::render_voice(mixer_voice &voice, u32 index, u32 frameCount) noexcept {
    advance_pan(voice, frameCount);
    voice.underrunReported = false;

    f32 leftPan = 1.0f;
    f32 rightPan = 1.0f;
    if (voice.channels == 2 && !voice.downmix) {
        balance_pan(voice.panCurrent, leftPan, rightPan);
    } else {
        constant_power_pan(voice.panCurrent, leftPan, rightPan);
    }

    if (voice.stream != nullptr) {
        render_stream_voice(voice, index, frameCount, leftPan, rightPan);
    } else {
        render_pcm_voice(voice, index, frameCount, leftPan, rightPan);
    }

    if (state_of(voice) == voice_state::STOPPING && voice.gainRampFrames == 0
      && voice.gainCurrent <= STOP_SILENCE_THRESHOLD) {
        finish_voice(voice, index, event_kind::STOPPED);
    }
    voice.publishedCursor.store(static_cast<u64>(voice.cursor), std::memory_order_relaxed);
}

void mixer::render_pcm_voice(mixer_voice &voice, u32 index, u32 frameCount, f32 leftPan, f32 rightPan) noexcept {
    if (voice.samples == nullptr || voice.clipFrames == 0) {
        finish_voice(voice, index, event_kind::FINISHED);
        return;
    }

    const f64 clipFrames = static_cast<f64>(voice.clipFrames);
    const f32 groupGain = mGroupGain[voice.groupIndex];
    const u16 channels = voice.channels;

    for (u32 frame = 0; frame < frameCount; ++frame) {
        advance_gain(voice);

        const u64 position = static_cast<u64>(voice.cursor);
        const f32 fraction = static_cast<f32>(voice.cursor - static_cast<f64>(position));
        u64 next = position + 1;
        if (next >= voice.clipFrames) {
            next = voice.loop ? 0 : position;
        }
        const f32 *current = voice.samples + position * channels;
        const f32 *following = voice.samples + next * channels;

        const f32 gain = voice.gainCurrent * groupGain;
        f32 left = 0.0f;
        f32 right = 0.0f;
        if (channels == 2) {
            const f32 sampleLeft = current[0] + (following[0] - current[0]) * fraction;
            const f32 sampleRight = current[1] + (following[1] - current[1]) * fraction;
            if (voice.downmix) {
                const f32 mono = 0.5f * (sampleLeft + sampleRight);
                left = mono;
                right = mono;
            } else {
                left = sampleLeft;
                right = sampleRight;
            }
        } else {
            const f32 mono = current[0] + (following[0] - current[0]) * fraction;
            left = mono;
            right = mono;
        }
        mMixBlock[static_cast<usize>(frame) * 2] += gain * leftPan * left;
        mMixBlock[static_cast<usize>(frame) * 2 + 1] += gain * rightPan * right;

        voice.cursor += voice.step;
        if (voice.cursor >= clipFrames) {
            if (!voice.loop) {
                voice.cursor = clipFrames;
                finish_voice(voice, index, event_kind::FINISHED);
                return;
            }
            while (voice.cursor >= clipFrames) {
                voice.cursor -= clipFrames;
            }
            mLoops.fetch_add(1, std::memory_order_relaxed);
            emit(event_kind::LOOPED, index, voice, static_cast<u64>(voice.cursor));
        }
    }
}

bool mixer::ensure_stream_frames(mixer_voice &voice) noexcept {
    stream_ring &ring = *voice.stream;
    while (voice.heldCount < 2) {
        if (!ring.pop_frame(voice.held[voice.heldCount].data())) {
            break;
        }
        ++voice.heldCount;
    }
    if (voice.heldCount == 2) {
        return true;
    }
    if (!ring.end_of_stream()) {
        return false;
    }
    if (voice.heldCount == 1) {
        voice.held[1] = voice.held[0];
        voice.heldCount = 2;
        voice.heldLast = true;
        return true;
    }
    return false;
}

void mixer::render_stream_voice(mixer_voice &voice, u32 index, u32 frameCount, f32 leftPan, f32 rightPan) noexcept {
    const f32 groupGain = mGroupGain[voice.groupIndex];
    const u16 channels = voice.channels;
    const f64 clipFrames = static_cast<f64>(voice.clipFrames);

    for (u32 frame = 0; frame < frameCount; ++frame) {
        if (!ensure_stream_frames(voice)) {
            if (voice.stream->end_of_stream() && voice.heldCount == 0) {
                finish_voice(voice, index, event_kind::FINISHED);
                return;
            }
            if (!voice.underrunReported) {
                voice.underrunReported = true;
                mUnderruns.fetch_add(1, std::memory_order_relaxed);
                emit(event_kind::UNDERRUN, index, voice, static_cast<u64>(voice.cursor));
            }
            return;   // Silence for the rest of the block; the cursor holds.
        }

        advance_gain(voice);
        const f32 fraction = static_cast<f32>(voice.streamFrac);
        const f32 gain = voice.gainCurrent * groupGain;
        f32 left = 0.0f;
        f32 right = 0.0f;
        if (channels == 2) {
            const f32 sampleLeft = voice.held[0][0] + (voice.held[1][0] - voice.held[0][0]) * fraction;
            const f32 sampleRight = voice.held[0][1] + (voice.held[1][1] - voice.held[0][1]) * fraction;
            if (voice.downmix) {
                const f32 mono = 0.5f * (sampleLeft + sampleRight);
                left = mono;
                right = mono;
            } else {
                left = sampleLeft;
                right = sampleRight;
            }
        } else {
            const f32 mono = voice.held[0][0] + (voice.held[1][0] - voice.held[0][0]) * fraction;
            left = mono;
            right = mono;
        }
        mMixBlock[static_cast<usize>(frame) * 2] += gain * leftPan * left;
        mMixBlock[static_cast<usize>(frame) * 2 + 1] += gain * rightPan * right;

        voice.streamFrac += voice.step;
        while (voice.streamFrac >= 1.0) {
            voice.streamFrac -= 1.0;
            voice.cursor += 1.0;
            if (voice.heldLast) {
                // The frame just consumed was the last one of the stream.
                voice.heldCount = 0;
                voice.heldLast = false;
                finish_voice(voice, index, event_kind::FINISHED);
                return;
            }
            voice.held[0] = voice.held[1];
            voice.heldCount = 1;
            if (!ensure_stream_frames(voice)) {
                break;   // Checked again at the top of the next frame.
            }
            if (voice.loop && clipFrames > 0.0 && voice.cursor >= clipFrames) {
                voice.cursor -= clipFrames;
                mLoops.fetch_add(1, std::memory_order_relaxed);
                emit(event_kind::LOOPED, index, voice, static_cast<u64>(voice.cursor));
            }
        }
    }
}

void mixer::advance_gain(mixer_voice &voice) noexcept {
    if (voice.gainRampFrames == 0) {
        return;
    }
    voice.gainCurrent += voice.gainStep;
    --voice.gainRampFrames;
    if (voice.gainRampFrames == 0) {
        voice.gainCurrent = voice.gainTarget;
    }
}

void mixer::advance_pan(mixer_voice &voice, u32 frameCount) noexcept {
    if (voice.panRampFrames == 0) {
        return;
    }
    const u32 advance = std::min(frameCount, voice.panRampFrames);
    voice.panCurrent += voice.panStep * static_cast<f32>(advance);
    voice.panRampFrames -= advance;
    if (voice.panRampFrames == 0) {
        voice.panCurrent = voice.panTarget;
    }
}

void mixer::finish_voice(mixer_voice &voice, u32 index, event_kind kind) noexcept {
    const voice_state state = kind == event_kind::STOPPED ? voice_state::STOPPED : voice_state::FINISHED;
    voice.publishedCursor.store(static_cast<u64>(voice.cursor), std::memory_order_relaxed);
    set_state(voice, state);
    emit(kind, index, voice, static_cast<u64>(voice.cursor));
}

void mixer::emit(event_kind kind, u32 index, const mixer_voice &voice, u64 frame) noexcept {
    voice_event event = {};
    event.kind = kind;
    event.voiceIndex = index;
    event.generation = voice.generation.load(std::memory_order_relaxed);
    event.frame = frame;
    if (!mEvents.try_push(event)) {
        mEventsDropped.fetch_add(1, std::memory_order_relaxed);
    }
}
