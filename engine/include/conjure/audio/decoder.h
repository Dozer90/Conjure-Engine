#pragma once

#include "conjure/audio/clip.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <memory>
#include <span>

// Decoding (docs/design/12_audio.md 3.10). Everything here goes through miniaudio's decoders (WAV,
// MP3, FLAC built in, Ogg Vorbis through stb_vorbis compiled into the miniaudio translation unit)
// and is backend independent: the null backend decodes exactly like the real one. Safe to call
// from any thread; a streaming_decoder instance is used by one thread at a time.

namespace conjure::audio {

    /// <summary>
    /// Options of decode_to_pcm: downmix to mono, resample (0 keeps the source rate) and scale the
    /// peak to 1.
    /// </summary>
    struct decode_options {
        bool forceMono = false;
        u32 sampleRate = 0;
        bool normalize = false;

        [[nodiscard]] bool operator==(const decode_options &other) const noexcept = default;
    };

    /// <summary>
    /// Sniffs the container magic (RIFF/WAVE, OggS, ID3 / MP3 sync, fLaC). UNKNOWN when none matches.
    /// </summary>
    [[nodiscard]] CONJURE_API clip_encoding sniff_encoding(std::span<const byte> encoded) noexcept;

    /// <summary>
    /// Opens a decoder to read format and length. Errors: INVALID_CLIP (empty), UNSUPPORTED_FORMAT
    /// (unknown magic), DECODE_FAILED (miniaudio refused the data; the message names its result).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<clip_info> probe(std::span<const byte> encoded);

    /// <summary>
    /// Decodes a whole file to interleaved f32 at its native channel count and rate, then applies
    /// the options in order: forceMono, sampleRate (miniaudio linear resampler), normalize.
    /// Same errors as probe().
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<pcm_buffer> decode_to_pcm(
      std::span<const byte> encoded,
      const decode_options &options
    );

    /// <summary>
    /// Incremental decoder over caller-owned encoded bytes, used by streamed voices.
    /// </summary>
    class CONJURE_API streaming_decoder {
        private:
            struct state;

            std::unique_ptr<state> mState = nullptr;   // the miniaudio decoder; null while closed
            clip_info mInfo = {};
            u64 mPosition = 0;

        public:
            streaming_decoder();
            ~streaming_decoder();

            streaming_decoder(const streaming_decoder &) = delete;
            streaming_decoder &operator=(const streaming_decoder &) = delete;
            streaming_decoder(streaming_decoder &&) noexcept;
            streaming_decoder &operator=(streaming_decoder &&) noexcept;

        public:
            /// <summary>
            /// Opens a decoder over caller-owned bytes, which must outlive the decoder (or close()).
            /// An open decoder is closed first. A container that does not report its length (Ogg
            /// Vorbis) is decoded once to count it, which costs a full decode on the calling thread;
            /// use the overload below when the length is already known. Same errors as probe().
            /// </summary>
            [[nodiscard]] core::result<void> open(std::span<const byte> encoded);

            /// <summary>
            /// open() with a length the caller already knows (a probed clip's info.frameCount): the
            /// container's own length wins when it reports one, otherwise knownFrameCount is used
            /// and nothing is decoded before the first read(). 0 counts as unknown.
            /// </summary>
            [[nodiscard]] core::result<void> open(std::span<const byte> encoded, u64 knownFrameCount);

            /// <summary>
            /// True between a successful open() and close().
            /// </summary>
            [[nodiscard]] bool is_open() const noexcept {
                return mState != nullptr;
            }

            /// <summary>
            /// Format, length and encoding of the open stream (storage STREAMED).
            /// </summary>
            [[nodiscard]] const clip_info &info() const noexcept {
                return mInfo;
            }

            /// <summary>
            /// Frame index the next read() starts at.
            /// </summary>
            [[nodiscard]] u64 position() const noexcept {
                return mPosition;
            }

            /// <summary>
            /// Decodes up to out.size() / channels frames into interleaved f32; returns the number
            /// of frames written (0 at the end of the stream or when closed).
            /// </summary>
            [[nodiscard]] usize read(std::span<f32> out) noexcept;

            /// <summary>
            /// Positions the next read at 'frame'. INVALID_STATE when closed, DECODE_FAILED when the
            /// decoder refuses the position.
            /// </summary>
            [[nodiscard]] core::result<void> seek(u64 frame) noexcept;

            /// <summary>
            /// Releases the decoder. Safe to call twice.
            /// </summary>
            void close() noexcept;
    };
}
