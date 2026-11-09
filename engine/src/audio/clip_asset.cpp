#include "conjure/audio/clip_asset.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/decoder.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/error.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <bit>
#include <cstring>
#include <format>
#include <memory>
#include <utility>

using namespace conjure;
using namespace conjure::audio;

static_assert(std::endian::native == std::endian::little, "clip blobs are written verbatim in little-endian order");

namespace {
    constexpr std::string_view KEY_STORAGE = "storage";
    constexpr std::string_view KEY_THRESHOLD = "streamThresholdSeconds";
    constexpr std::string_view KEY_FORCE_MONO = "forceMono";
    constexpr std::string_view KEY_SAMPLE_RATE = "sampleRate";
    constexpr std::string_view KEY_NORMALIZE = "normalize";

    [[nodiscard]] bool parse_storage(std::string_view text, clip_storage_setting &out) noexcept {
        if (text == "auto") {
            out = clip_storage_setting::AUTO;
            return true;
        }
        if (text == "pcm") {
            out = clip_storage_setting::PCM;
            return true;
        }
        if (text == "streamed") {
            out = clip_storage_setting::STREAMED;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool valid_storage(u8 value) noexcept {
        return value == static_cast<u8>(clip_storage::PCM) || value == static_cast<u8>(clip_storage::STREAMED);
    }

    [[nodiscard]] bool valid_encoding(u8 value) noexcept {
        return value <= static_cast<u8>(clip_encoding::FLAC);
    }

    void append_bytes(std::vector<byte> &out, const void *data, usize size) {
        const byte *bytes = static_cast<const byte *>(data);
        out.insert(out.end(), bytes, bytes + size);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// clip_import_settings

std::string_view conjure::audio::to_string(clip_storage_setting value) noexcept {
    switch (value) {
        case clip_storage_setting::AUTO:
            return "auto";
        case clip_storage_setting::PCM:
            return "pcm";
        case clip_storage_setting::STREAMED:
            return "streamed";
    }
    return "auto";
}

core::result<clip_import_settings> clip_import_settings::from_json(const core::json::value &source) {
    clip_import_settings settings = {};
    if (!source.is_valid() || source.is_null()) {
        return settings;
    }
    if (!source.is_object()) {
        return core::make_error(audio_error::INVALID_SETTINGS, "clip import settings must be a JSON object");
    }

    if (source.contains(KEY_STORAGE)) {
        const core::json::value storage = source[KEY_STORAGE];
        if (!storage.is_string() || !parse_storage(storage.as_string(), settings.storage)) {
            return core::make_error(
              audio_error::INVALID_SETTINGS,
              "clip import settings: storage must be \"auto\", \"pcm\" or \"streamed\""
            );
        }
    }
    if (source.contains(KEY_THRESHOLD)) {
        const core::json::value threshold = source[KEY_THRESHOLD];
        if (!threshold.is_number() || threshold.as_f64() < 0.0) {
            return core::make_error(
              audio_error::INVALID_SETTINGS,
              "clip import settings: streamThresholdSeconds must be a non-negative number"
            );
        }
        settings.streamThresholdSeconds = threshold.as_f64();
    }
    if (source.contains(KEY_FORCE_MONO)) {
        const core::json::value forceMono = source[KEY_FORCE_MONO];
        if (!forceMono.is_bool()) {
            return core::make_error(audio_error::INVALID_SETTINGS, "clip import settings: forceMono must be a boolean");
        }
        settings.forceMono = forceMono.as_bool();
    }
    if (source.contains(KEY_SAMPLE_RATE)) {
        const core::json::value sampleRate = source[KEY_SAMPLE_RATE];
        const u64 rate = sampleRate.as_u64();
        const bool rateInRange = rate == 0 || (rate >= MIN_SAMPLE_RATE && rate <= MAX_SAMPLE_RATE);
        const bool inRange = sampleRate.is_number() && rateInRange;
        if (!inRange || sampleRate.as_f64() < 0.0) {
            return core::make_error(
              audio_error::INVALID_SETTINGS,
              "clip import settings: sampleRate must be 0 or within [{}, {}]",
              MIN_SAMPLE_RATE,
              MAX_SAMPLE_RATE
            );
        }
        settings.sampleRate = static_cast<u32>(sampleRate.as_u64());
    }
    if (source.contains(KEY_NORMALIZE)) {
        const core::json::value normalize = source[KEY_NORMALIZE];
        if (!normalize.is_bool()) {
            return core::make_error(audio_error::INVALID_SETTINGS, "clip import settings: normalize must be a boolean");
        }
        settings.normalize = normalize.as_bool();
    }
    return settings;
}

void clip_import_settings::to_json(core::json::writer &out) const {
    out.begin_object();
    out.key(KEY_FORCE_MONO);
    out.value(forceMono);
    out.key(KEY_NORMALIZE);
    out.value(normalize);
    out.key(KEY_SAMPLE_RATE);
    out.value(sampleRate);
    out.key(KEY_STORAGE);
    out.value(to_string(storage));
    out.key(KEY_THRESHOLD);
    out.value(streamThresholdSeconds);
    out.end_object();
}

core::json::document clip_import_settings::to_document() const {
    core::json::document document = core::json::document::object();
    document.set(KEY_FORCE_MONO, forceMono);
    document.set(KEY_NORMALIZE, normalize);
    document.set(KEY_SAMPLE_RATE, sampleRate);
    document.set(KEY_STORAGE, to_string(storage));
    document.set(KEY_THRESHOLD, streamThresholdSeconds);
    return document;
}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

bool conjure::audio::is_clip_extension(std::string_view extension) noexcept {
    for (const std::string_view candidate : CLIP_EXTENSIONS) {
        if (core::str::iequals(candidate, extension)) {
            return true;
        }
    }
    return false;
}

core::result<std::vector<byte>> conjure::audio::cook_clip(
  std::span<const byte> encoded,
  const clip_import_settings &settings
) {
    CONJURE_TRY_ASSIGN(const clip_info probed, probe(encoded));

    clip_storage storage = clip_storage::PCM;
    switch (settings.storage) {
        case clip_storage_setting::PCM:
            storage = clip_storage::PCM;
            break;
        case clip_storage_setting::STREAMED:
            storage = clip_storage::STREAMED;
            break;
        case clip_storage_setting::AUTO:
            storage = probed.duration_seconds() > settings.streamThresholdSeconds
              ? clip_storage::STREAMED
              : clip_storage::PCM;
            break;
    }

    clip_blob_header header = {};
    header.storage = static_cast<u8>(storage);
    header.encoding = static_cast<u8>(probed.encoding);

    std::vector<byte> blob = {};
    if (storage == clip_storage::STREAMED) {
        header.channels = static_cast<u8>(probed.format.channels);
        header.sampleRate = probed.format.sampleRate;
        header.frameCount = probed.frameCount;
        header.dataBytes = encoded.size();
        blob.reserve(sizeof(clip_blob_header) + encoded.size());
        append_bytes(blob, &header, sizeof(header));
        append_bytes(blob, encoded.data(), encoded.size());
        return blob;
    }

    decode_options options = {};
    options.forceMono = settings.forceMono;
    options.sampleRate = settings.sampleRate;
    options.normalize = settings.normalize;
    CONJURE_TRY_ASSIGN(const pcm_buffer pcm, decode_to_pcm(encoded, options));

    header.channels = static_cast<u8>(pcm.format().channels);
    header.sampleRate = pcm.format().sampleRate;
    header.frameCount = pcm.frame_count();
    header.dataBytes = pcm.samples().size_bytes();
    blob.reserve(sizeof(clip_blob_header) + pcm.samples().size_bytes());
    append_bytes(blob, &header, sizeof(header));
    append_bytes(blob, pcm.samples().data(), pcm.samples().size_bytes());
    return blob;
}

core::result<clip_blob_header> conjure::audio::read_clip_blob_header(std::span<const byte> blob) {
    if (blob.size() < sizeof(clip_blob_header)) {
        return core::make_error(
          audio_error::INVALID_CLIP,
          "clip blob is {} bytes, shorter than the {}-byte header",
          blob.size(),
          sizeof(clip_blob_header)
        );
    }
    clip_blob_header header = {};
    std::memcpy(&header, blob.data(), sizeof(header));

    if (header.magic != clip_blob_header::expected_magic()) {
        return core::make_error(audio_error::INVALID_CLIP, "clip blob magic is not CJAC");
    }
    if (header.version != CLIP_BLOB_VERSION) {
        return core::make_error(
          audio_error::INVALID_CLIP,
          "clip blob version {} is not the supported version {}",
          header.version,
          CLIP_BLOB_VERSION
        );
    }
    if (!valid_storage(header.storage)) {
        return core::make_error(audio_error::INVALID_CLIP, "clip blob storage {} is unknown", header.storage);
    }
    if (!valid_encoding(header.encoding)) {
        return core::make_error(audio_error::INVALID_CLIP, "clip blob encoding {} is unknown", header.encoding);
    }
    if (header.channels != 1 && header.channels != 2) {
        return core::make_error(
          audio_error::INVALID_CLIP,
          "clip blob has {} channels; 1 or 2 expected",
          header.channels
        );
    }
    if (header.sampleRate < MIN_SAMPLE_RATE || header.sampleRate > MAX_SAMPLE_RATE) {
        return core::make_error(
          audio_error::INVALID_CLIP,
          "clip blob sample rate {} is out of range",
          header.sampleRate
        );
    }
    const u64 payloadBytes = blob.size() - sizeof(clip_blob_header);
    if (header.dataBytes != payloadBytes) {
        return core::make_error(
          audio_error::INVALID_CLIP,
          "clip blob declares {} payload bytes but carries {}",
          header.dataBytes,
          payloadBytes
        );
    }
    if (header.storage == static_cast<u8>(clip_storage::PCM)) {
        const u64 expected = header.frameCount * header.channels * sizeof(f32);
        if (expected != header.dataBytes) {
            return core::make_error(
              audio_error::INVALID_CLIP,
              "clip blob PCM payload is {} bytes; {} frames of {} channels need {}",
              header.dataBytes,
              header.frameCount,
              header.channels,
              expected
            );
        }
    }
    if (header.frameCount == 0 || header.dataBytes == 0) {
        return core::make_error(audio_error::INVALID_CLIP, "clip blob is empty");
    }
    return header;
}

core::result<audio_clip> conjure::audio::load_clip_blob(std::span<const byte> blob) {
    CONJURE_TRY_ASSIGN(const clip_blob_header header, read_clip_blob_header(blob));
    const std::span<const byte> payload = blob.subspan(sizeof(clip_blob_header));

    audio_clip clip = {};
    clip.info.format.sampleRate = header.sampleRate;
    clip.info.format.channels = header.channels;
    clip.info.format.format = sample_format::F32;
    clip.info.frameCount = header.frameCount;
    clip.info.encoding = static_cast<clip_encoding>(header.encoding);
    clip.info.storage = static_cast<clip_storage>(header.storage);

    if (clip.info.storage == clip_storage::STREAMED) {
        CONJURE_TRY_ASSIGN(const clip_info probed, probe(payload));
        if (probed.format.channels != header.channels || probed.format.sampleRate != header.sampleRate) {
            return core::make_error(
              audio_error::INVALID_CLIP,
              "streamed clip header ({} Hz, {} ch) disagrees with its payload ({} Hz, {} ch)",
              header.sampleRate,
              header.channels,
              probed.format.sampleRate,
              probed.format.channels
            );
        }
        clip.encoded = std::make_shared<const std::vector<byte>>(payload.begin(), payload.end());
        return clip;
    }

    std::vector<f32> samples(static_cast<usize>(header.frameCount) * header.channels, 0.0f);
    std::memcpy(samples.data(), payload.data(), payload.size());
    clip.pcm = std::make_shared<const pcm_buffer>(clip.info.format, std::move(samples));
    return clip;
}


/*===========================================================================*/
/*===========================================================================*/
// audio_clip_importer

audio_clip_importer::audio_clip_importer() {}

audio_clip_importer::~audio_clip_importer() {}

std::string_view audio_clip_importer::id() const noexcept {
    return CLIP_IMPORTER_ID;
}

u32 audio_clip_importer::version() const noexcept {
    return CLIP_IMPORTER_VERSION;
}

std::span<const std::string_view> audio_clip_importer::extensions() const noexcept {
    return CLIP_EXTENSIONS;
}

core::json::document audio_clip_importer::default_settings() const {
    return clip_import_settings{}.to_document();
}

core::result<assets::import_output> audio_clip_importer::import(assets::import_context &context) const {
    const core::result<clip_import_settings> settings = clip_import_settings::from_json(context.settings());
    if (!settings.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::IMPORT_FAILED, settings.get_error().to_string());
    }

    core::result<std::vector<byte>> blob = cook_clip(context.source_bytes(), settings.value());
    if (!blob.has_value()) {
        return assets::make_asset_error(
          assets::asset_error_code::IMPORT_FAILED,
          std::format("{}: {}", context.source_path(), blob.get_error().to_string())
        );
    }

    assets::imported_asset primary = {};
    primary.displayName = std::string(core::vfs::path_utils::stem(context.source_path()));
    primary.type = assets::asset_type_traits<audio_clip>::id();
    primary.payload = std::move(blob).value();

    assets::import_output output = {};
    output.assets.push_back(std::move(primary));
    return output;
}


/*===========================================================================*/
/*===========================================================================*/
// audio_clip_loader

audio_clip_loader::audio_clip_loader() {}

audio_clip_loader::~audio_clip_loader() {}

core::result<audio_clip> audio_clip_loader::load_cpu_typed(
  const assets::load_context &context,
  std::span<const std::byte> payloadBytes
) const {
    static_cast<void>(context);
    return load_clip_blob(payloadBytes);
}

core::result<audio_clip> audio_clip_loader::finalise_typed(
  const assets::finalise_context &context,
  audio_clip &&intermediate
) const {
    static_cast<void>(context);
    return std::move(intermediate);
}

std::optional<audio_clip> audio_clip_loader::create_fallback_typed() const {
    return make_fallback_clip();
}


/*===========================================================================*/
/*===========================================================================*/
// Registration

core::result<void> conjure::audio::register_asset_types(
  assets::asset_type_registry &types,
  assets::importer_registry &importers
) {
    assets::asset_type_desc desc = assets::make_asset_type_desc<audio_clip>(std::make_unique<audio_clip_loader>(), 1);
    CONJURE_TRY(types.register_type(std::move(desc)));
    CONJURE_TRY(importers.register_importer(std::make_unique<audio_clip_importer>()));
    return {};
}
