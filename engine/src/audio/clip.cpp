#include "conjure/audio/clip.h"

#include "conjure/core/assert.h"

#include <utility>

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr u32 FALLBACK_SAMPLE_RATE = 48000;
    constexpr u64 FALLBACK_FRAMES = FALLBACK_SAMPLE_RATE / 10;   // 0.1 s
}

/*===========================================================================*/
/*===========================================================================*/
// pcm_buffer

pcm_buffer::pcm_buffer() {}

pcm_buffer::pcm_buffer(audio_format format, std::vector<f32> samples) :
    mFormat(format),
    mSamples(std::move(samples)) {
    if (mFormat.channels == 0) {
        CONJURE_ASSERT(false, "pcm_buffer: a channel count of 0 is not a format; treating it as mono");
        mFormat.channels = 1;
    }
    const usize remainder = mSamples.size() % mFormat.channels;
    if (remainder != 0) {
        CONJURE_ASSERT(
          false,
          "pcm_buffer: {} samples are not a multiple of {} channels",
          mSamples.size(),
          mFormat.channels
        );
        mSamples.resize(mSamples.size() - remainder);
    }
    mFrameCount = mSamples.size() / mFormat.channels;
}

pcm_buffer::~pcm_buffer() {}

f64 pcm_buffer::duration_seconds() const noexcept {
    if (mFormat.sampleRate == 0) {
        return 0.0;
    }
    return static_cast<f64>(mFrameCount) / static_cast<f64>(mFormat.sampleRate);
}


/*===========================================================================*/
/*===========================================================================*/
// clip_info and audio_clip

f64 clip_info::duration_seconds() const noexcept {
    if (format.sampleRate == 0) {
        return 0.0;
    }
    return static_cast<f64>(frameCount) / static_cast<f64>(format.sampleRate);
}

bool audio_clip::is_valid() const noexcept {
    if (info.storage == clip_storage::STREAMED) {
        return encoded != nullptr && !encoded->empty() && info.frameCount > 0;
    }
    return pcm != nullptr && pcm->frame_count() > 0 && pcm->frame_count() == info.frameCount
      && pcm->format() == info.format;
}

audio_clip conjure::audio::make_pcm_clip(std::shared_ptr<const pcm_buffer> pcm, clip_encoding encoding) {
    audio_clip clip = {};
    if (pcm != nullptr) {
        clip.info.format = pcm->format();
        clip.info.frameCount = pcm->frame_count();
    }
    clip.info.encoding = encoding;
    clip.info.storage = clip_storage::PCM;
    clip.pcm = std::move(pcm);
    return clip;
}

audio_clip conjure::audio::make_fallback_clip() {
    audio_format format = {};
    format.sampleRate = FALLBACK_SAMPLE_RATE;
    format.channels = 1;
    std::vector<f32> silence(static_cast<usize>(FALLBACK_FRAMES), 0.0f);
    return make_pcm_clip(std::make_shared<const pcm_buffer>(format, std::move(silence)), clip_encoding::WAV);
}
