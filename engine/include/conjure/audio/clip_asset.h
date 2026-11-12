#pragma once

#include "conjure/audio/audio_types.h"
#include "conjure/audio/clip.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// The conjure.audio.clip cooked blob, import settings, importer and loader (docs/design/12_audio.md
// 2.2, 4.2). The pure functions cook_clip and load_clip_blob hold the whole import and load logic;
// audio_clip_importer and audio_clip_loader are the thin assets-module adapters over them. The
// loader needs no audio_engine: the clip is a self-contained CPU payload (A6, R43).

namespace conjure::audio {

    /// <summary>
    /// Importer identity and the extensions it claims.
    /// </summary>
    inline constexpr std::string_view CLIP_IMPORTER_ID = "conjure.audio.clip";
    inline constexpr u32 CLIP_IMPORTER_VERSION = 1;
    inline constexpr std::array<std::string_view, 4> CLIP_EXTENSIONS = {".wav", ".ogg", ".mp3", ".flac"};

    /// <summary>
    /// Blob format version written into clip_blob_header::version.
    /// </summary>
    inline constexpr u32 CLIP_BLOB_VERSION = 1;

    /// <summary>
    /// 32-byte little-endian header of a cooked clip, written verbatim and followed by dataBytes of
    /// interleaved f32 samples (PCM) or the original encoded file (STREAMED).
    /// </summary>
    struct clip_blob_header {
        std::array<char, 4> magic = {'C', 'J', 'A', 'C'};
        u32 version = CLIP_BLOB_VERSION;
        u8 storage = 0;                     // clip_storage
        u8 channels = 0;                    // 1 or 2
        u8 encoding = 0;                    // clip_encoding
        u8 reserved = 0;
        u32 sampleRate = 0;
        u64 frameCount = 0;
        u64 dataBytes = 0;

        /// <summary>
        /// The magic every valid header carries.
        /// </summary>
        [[nodiscard]] static constexpr std::array<char, 4> expected_magic() noexcept {
            return {'C', 'J', 'A', 'C'};
        }

        [[nodiscard]] bool operator==(const clip_blob_header &other) const noexcept = default;
    };

    static_assert(sizeof(clip_blob_header) == 32, "clip_blob_header is 32 bytes on disk");
    static_assert(std::is_trivially_copyable_v<clip_blob_header>, "clip_blob_header is written verbatim");

    /// <summary>
    /// The storage choice of an import: AUTO picks STREAMED above streamThresholdSeconds.
    /// </summary>
    enum class clip_storage_setting : u8 {
        AUTO,
        PCM,
        STREAMED
    };

    /// <summary>
    /// Lower-case JSON spelling ("auto", "pcm", "streamed").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(clip_storage_setting value) noexcept;

    /// <summary>
    /// Importer settings, mirrored by the JSON object
    /// { "storage": "auto", "streamThresholdSeconds": 10.0, "forceMono": false, "sampleRate": 0, "normalize": false }.
    /// forceMono, sampleRate and normalize apply to PCM storage only; STREAMED keeps the file, and
    /// the importer reports a warning when one of them is set on a clip it streams.
    /// </summary>
    struct CONJURE_API clip_import_settings {
        clip_storage_setting storage = clip_storage_setting::AUTO;
        f64 streamThresholdSeconds = 10.0;
        bool forceMono = false;
        u32 sampleRate = 0;                 // 0 keeps the source rate
        bool normalize = false;

        /// <summary>
        /// Reads the settings object; missing keys keep their defaults. Fails with INVALID_SETTINGS
        /// for an unknown storage name, a negative threshold or a sample rate outside
        /// 0 or [MIN_SAMPLE_RATE, MAX_SAMPLE_RATE]. An invalid (missing) value yields the defaults.
        /// </summary>
        [[nodiscard]] static core::result<clip_import_settings> from_json(const core::json::value &source);

        /// <summary>
        /// Writes the settings object with keys in sorted order (canonical form).
        /// </summary>
        void to_json(core::json::writer &out) const;

        /// <summary>
        /// The settings as an owning document.
        /// </summary>
        [[nodiscard]] core::json::document to_document() const;

        [[nodiscard]] bool operator==(const clip_import_settings &other) const noexcept = default;
    };

    /// <summary>
    /// True when the lower-case extension (".wav") is one the importer claims.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_clip_extension(std::string_view extension) noexcept;

    /// <summary>
    /// Imports a source file into a cooked blob (header + payload). PCM storage decodes with
    /// decode_options built from the settings; STREAMED storage copies the file byte for byte and
    /// records the probed format. Deterministic for the same input and build. Errors are those of
    /// probe() / decode_to_pcm().
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<std::vector<byte>> cook_clip(
      std::span<const byte> encoded,
      const clip_import_settings &settings
    );

    /// <summary>
    /// Validates and returns the header of a blob: magic, version, storage and encoding values,
    /// channels 1 or 2, sample rate in range, dataBytes equal to the payload size and (PCM) equal to
    /// frameCount * channels * 4. Fails with INVALID_CLIP naming the mismatch.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<clip_blob_header> read_clip_blob_header(std::span<const byte> blob);

    /// <summary>
    /// Turns a blob into a self-contained clip: PCM samples are copied into a fresh pcm_buffer,
    /// STREAMED bytes are copied and opened with the header's frameCount (no full decode) to check
    /// that their format matches the header. Errors: INVALID_CLIP for a bad header or a streamed
    /// payload whose format disagrees with it, DECODE_FAILED / UNSUPPORTED_FORMAT when streamed
    /// bytes do not decode.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<audio_clip> load_clip_blob(std::span<const byte> blob);

    /// <summary>
    /// The conjure.audio.clip importer: .wav .ogg .mp3 .flac -> one cooked clip blob (cook_clip).
    /// Deterministic and stateless; runs on a worker thread.
    /// </summary>
    class CONJURE_API audio_clip_importer final : public assets::asset_importer {
        public:
            audio_clip_importer();
            ~audio_clip_importer() override;

        public:
            /// <summary>
            /// "conjure.audio.clip".
            /// </summary>
            [[nodiscard]] std::string_view id() const noexcept override;

            /// <summary>
            /// CLIP_IMPORTER_VERSION.
            /// </summary>
            [[nodiscard]] u32 version() const noexcept override;

            /// <summary>
            /// CLIP_EXTENSIONS.
            /// </summary>
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;

            /// <summary>
            /// clip_import_settings{} as JSON.
            /// </summary>
            [[nodiscard]] core::json::document default_settings() const override;

            /// <summary>
            /// Reads the settings, cooks the source bytes and returns the primary asset. Decode or
            /// settings failures come back as IMPORT_FAILED with the audio error's message. A clip
            /// cooked as STREAMED while forceMono, sampleRate or normalize is set adds an import
            /// warning, since those settings apply to PCM storage only.
            /// </summary>
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };

    /// <summary>
    /// The conjure.audio.clip loader: blob -> audio_clip on the load job; finalise returns the clip
    /// unchanged; the fallback is the silent clip.
    /// </summary>
    class CONJURE_API audio_clip_loader final : public assets::typed_asset_loader<audio_clip, audio_clip> {
        public:
            audio_clip_loader();
            ~audio_clip_loader() override;

        protected:
            [[nodiscard]] core::result<audio_clip> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;

            [[nodiscard]] core::result<audio_clip> finalise_typed(
              const assets::finalise_context &context,
              audio_clip &&intermediate
            ) const override;

            /// <summary>
            /// A 0.1 s silent mono 48 kHz clip (the "silent clip" fallback of 05_assets.md D14).
            /// </summary>
            [[nodiscard]] std::optional<audio_clip> create_fallback_typed() const override;
    };

    /// <summary>
    /// Registers conjure.audio.clip (with its loader) and the clip importer. Main thread, from
    /// audio_subsystem::startup. Errors are those of the two registries (TYPE_DUPLICATE, ...).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_asset_types(
      assets::asset_type_registry &types,
      assets::importer_registry &importers
    );
}
