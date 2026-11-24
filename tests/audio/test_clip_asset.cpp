#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "wav_builder.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/assets_error.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/clip_asset.h"
#include "conjure/audio/decoder.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

namespace {
    std::vector<byte> tone_wav(f64 seconds, u16 channels = 1, u32 sampleRate = 48000) {
        testing::wav_spec spec = {};
        spec.channels = channels;
        spec.sampleRate = sampleRate;
        spec.frameCount = static_cast<u64>(seconds * sampleRate);
        spec.amplitude = 0.25f;
        return testing::build_wav(spec);
    }

    clip_blob_header header_of(std::span<const byte> blob) {
        clip_blob_header header = {};
        std::memcpy(&header, blob.data(), sizeof(header));
        return header;
    }

    /// <summary>
    /// Imports 'wav' with the given settings JSON through audio_clip_importer and returns the
    /// warnings the import reported.
    /// </summary>
    std::vector<std::string> import_warnings(std::span<const byte> wav, const char *settingsText) {
        const core::result<core::json::document> settings = core::json::document::parse(settingsText);
        REQUIRE(settings.has_value());
        core::vfs::memory_file_system fileSystem = {};
        assets::sub_asset_table subAssets = {};
        assets::import_context context(
          fileSystem,
          "assets://music/theme.wav",
          "music/theme.wav",
          wav,
          settings.value().root(),
          "windows",
          core::uuid::generate(),
          subAssets,
          {}
        );
        const audio_clip_importer importer = {};
        const core::result<assets::import_output> output = importer.import(context);
        REQUIRE(output.has_value());
        return std::vector<std::string>(context.warnings().begin(), context.warnings().end());
    }

    assets::cooked_blob wrap_blob(const core::uuid &id, std::vector<byte> payload) {
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<audio_clip>::id();
        header.typeVersion = 1;
        header.importerVersion = CLIP_IMPORTER_VERSION;
        header.id = id;
        return assets::make_cooked_blob(header, {}, std::move(payload));
    }

    struct manager_fixture {
        core::jobs::job_system jobs = {};
        assets::memory_asset_source *source = nullptr;
        std::unique_ptr<assets::asset_manager> manager = nullptr;

        manager_fixture() {
            std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
            source = owned.get();
            assets::asset_manager_desc desc = {};
            desc.jobs = &jobs;
            desc.source = std::move(owned);
            manager = std::make_unique<assets::asset_manager>(std::move(desc));
            assets::importer_registry importers = {};
            const core::result<void> registered = register_asset_types(manager->types(), importers);
            REQUIRE(registered.has_value());
        }
    };
}

TEST_SUITE("audio/clip_asset") {

    TEST_CASE("clip_blob_header - 32 bytes, CJAC magic, version 1") {
        const clip_blob_header header = {};
        CHECK(sizeof(clip_blob_header) == 32);
        CHECK(header.magic == clip_blob_header::expected_magic());
        CHECK(std::string(header.magic.data(), 4) == "CJAC");
        CHECK(header.version == CLIP_BLOB_VERSION);
        CHECK(CLIP_BLOB_VERSION == 1);
        CHECK(header == clip_blob_header{});
    }

    TEST_CASE("clip_import_settings - defaults round trip through JSON") {
        const clip_import_settings defaults = {};
        CHECK(defaults.storage == clip_storage_setting::AUTO);
        CHECK(defaults.streamThresholdSeconds == doctest::Approx(10.0));
        CHECK_FALSE(defaults.forceMono);
        CHECK(defaults.sampleRate == 0);
        CHECK_FALSE(defaults.normalize);

        const core::json::document document = defaults.to_document();
        CHECK(document.to_canonical_string()
          == R"({"forceMono":false,"normalize":false,"sampleRate":0,"storage":"auto","streamThresholdSeconds":10.0})");
        core::json::writer out = {};
        defaults.to_json(out);
        CHECK(out.finish() == document.to_canonical_string());

        const core::result<clip_import_settings> parsed = clip_import_settings::from_json(document.root());
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == defaults);
    }

    TEST_CASE("clip_import_settings - every field parses and missing keys keep their defaults") {
        const core::result<core::json::document> document = core::json::document::parse(
          R"({"storage":"streamed","streamThresholdSeconds":2.5,"forceMono":true,"sampleRate":22050,"normalize":true})"
        );
        CONJURE_REQUIRE_OK(document);
        const core::result<clip_import_settings> parsed = clip_import_settings::from_json(document.value().root());
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().storage == clip_storage_setting::STREAMED);
        CHECK(parsed.value().streamThresholdSeconds == doctest::Approx(2.5));
        CHECK(parsed.value().forceMono);
        CHECK(parsed.value().sampleRate == 22050);
        CHECK(parsed.value().normalize);

        const core::result<core::json::document> partial = core::json::document::parse(R"({"storage":"pcm"})");
        CONJURE_REQUIRE_OK(partial);
        const core::result<clip_import_settings> partialParsed =
          clip_import_settings::from_json(partial.value().root());
        CONJURE_REQUIRE_OK(partialParsed);
        CHECK(partialParsed.value().storage == clip_storage_setting::PCM);
        CHECK(partialParsed.value().streamThresholdSeconds == doctest::Approx(10.0));

        const core::result<clip_import_settings> fromInvalid = clip_import_settings::from_json(core::json::value{});
        CONJURE_REQUIRE_OK(fromInvalid);
        CHECK(fromInvalid.value() == clip_import_settings{});
        CHECK(to_string(clip_storage_setting::AUTO) == "auto");
        CHECK(to_string(clip_storage_setting::PCM) == "pcm");
        CHECK(to_string(clip_storage_setting::STREAMED) == "streamed");
    }

    TEST_CASE("clip_import_settings - bad values fail with INVALID_SETTINGS") {
        const std::array<const char *, 6> badTexts = {
            R"({"storage":"disk"})",
            R"({"storage":7})",
            R"({"streamThresholdSeconds":-1})",
            R"({"forceMono":"yes"})",
            R"({"sampleRate":12})",
            R"({"normalize":1})",
        };
        for (usize index = 0; index < badTexts.size(); ++index) {
            CAPTURE(index);
            const core::result<core::json::document> document = core::json::document::parse(badTexts[index]);
            CONJURE_REQUIRE_OK(document);
            const core::result<clip_import_settings> parsed = clip_import_settings::from_json(document.value().root());
            CONJURE_CHECK_ERROR(parsed, audio_error::INVALID_SETTINGS);
        }
        const core::result<core::json::document> array = core::json::document::parse("[1,2]");
        CONJURE_REQUIRE_OK(array);
        CONJURE_CHECK_ERROR(clip_import_settings::from_json(array.value().root()), audio_error::INVALID_SETTINGS);
    }

    TEST_CASE("cook_clip - a .wav becomes a PCM blob with a correct header") {
        const std::vector<byte> wav = tone_wav(0.5, 2, 44100);
        const core::result<std::vector<byte>> blob = cook_clip(wav, clip_import_settings{});
        CONJURE_REQUIRE_OK(blob);
        const clip_blob_header header = header_of(blob.value());
        CHECK(header.magic == clip_blob_header::expected_magic());
        CHECK(header.version == 1);
        CHECK(header.storage == static_cast<u8>(clip_storage::PCM));
        CHECK(header.channels == 2);
        CHECK(header.encoding == static_cast<u8>(clip_encoding::WAV));
        CHECK(header.sampleRate == 44100);
        CHECK(header.frameCount == 22050);
        CHECK(header.dataBytes == 22050 * 2 * sizeof(f32));
        CHECK(blob.value().size() == sizeof(clip_blob_header) + header.dataBytes);

        const core::result<clip_blob_header> validated = read_clip_blob_header(blob.value());
        CONJURE_REQUIRE_OK(validated);
        CHECK(validated.value() == header);
    }

    TEST_CASE("cook_clip - streamed storage keeps the original bytes byte-identical") {
        const std::vector<byte> wav = tone_wav(0.25);
        clip_import_settings settings = {};
        settings.storage = clip_storage_setting::STREAMED;
        const core::result<std::vector<byte>> blob = cook_clip(wav, settings);
        CONJURE_REQUIRE_OK(blob);
        const clip_blob_header header = header_of(blob.value());
        CHECK(header.storage == static_cast<u8>(clip_storage::STREAMED));
        CHECK(header.frameCount == 12000);
        CHECK(header.channels == 1);
        CHECK(header.sampleRate == 48000);
        CHECK(header.dataBytes == wav.size());
        REQUIRE(blob.value().size() == sizeof(clip_blob_header) + wav.size());
        CHECK(std::memcmp(blob.value().data() + sizeof(clip_blob_header), wav.data(), wav.size()) == 0);
    }

    TEST_CASE("cook_clip - auto storage switches to streamed above the threshold") {
        clip_import_settings settings = {};
        settings.streamThresholdSeconds = 0.3;
        const core::result<std::vector<byte>> shortBlob = cook_clip(tone_wav(0.2), settings);
        const core::result<std::vector<byte>> longBlob = cook_clip(tone_wav(0.4), settings);
        CONJURE_REQUIRE_OK(shortBlob);
        CONJURE_REQUIRE_OK(longBlob);
        CHECK(header_of(shortBlob.value()).storage == static_cast<u8>(clip_storage::PCM));
        CHECK(header_of(longBlob.value()).storage == static_cast<u8>(clip_storage::STREAMED));

        settings.storage = clip_storage_setting::PCM;
        const core::result<std::vector<byte>> forcedPcm = cook_clip(tone_wav(0.4), settings);
        CONJURE_REQUIRE_OK(forcedPcm);
        CHECK(header_of(forcedPcm.value()).storage == static_cast<u8>(clip_storage::PCM));
    }

    TEST_CASE("cook_clip - forceMono, sampleRate and normalize apply to PCM storage") {
        clip_import_settings settings = {};
        settings.storage = clip_storage_setting::PCM;
        settings.forceMono = true;
        settings.sampleRate = 24000;
        settings.normalize = true;
        const core::result<std::vector<byte>> blob = cook_clip(tone_wav(0.5, 2, 48000), settings);
        CONJURE_REQUIRE_OK(blob);
        const clip_blob_header header = header_of(blob.value());
        CHECK(header.channels == 1);
        CHECK(header.sampleRate == 24000);
        CHECK(static_cast<f64>(header.frameCount) == doctest::Approx(12000.0).epsilon(0.01));

        const core::result<audio_clip> clip = load_clip_blob(blob.value());
        CONJURE_REQUIRE_OK(clip);
        REQUIRE(clip.value().pcm != nullptr);
        CHECK(testing::peak_of(clip.value().pcm->samples()) == doctest::Approx(1.0).epsilon(1.0e-4));
    }

    TEST_CASE("cook_clip - corrupt input fails with a message and unknown extensions are not claimed") {
        const std::vector<byte> garbage(64, byte{0x5A});
        const core::result<std::vector<byte>> cooked = cook_clip(garbage, clip_import_settings{});
        CONJURE_CHECK_ERROR(cooked, audio_error::UNSUPPORTED_FORMAT);
        CHECK_FALSE(cooked.get_error().message().empty());

        std::vector<byte> truncated = tone_wav(0.1);
        truncated.resize(30);
        CONJURE_CHECK_ERROR(cook_clip(truncated, clip_import_settings{}), audio_error::DECODE_FAILED);
        CONJURE_CHECK_ERROR(cook_clip({}, clip_import_settings{}), audio_error::INVALID_CLIP);

        CHECK(is_clip_extension(".wav"));
        CHECK(is_clip_extension(".OGG"));
        CHECK(is_clip_extension(".mp3"));
        CHECK(is_clip_extension(".flac"));
        CHECK_FALSE(is_clip_extension(".txt"));
        CHECK_FALSE(is_clip_extension("wav"));
        CHECK_FALSE(is_clip_extension(""));
        CHECK(CLIP_EXTENSIONS.size() == 4);
    }

    TEST_CASE("cook_clip - two imports of the same input are byte-identical") {
        const std::vector<byte> wav = tone_wav(0.3, 2);
        clip_import_settings settings = {};
        settings.sampleRate = 32000;
        const core::result<std::vector<byte>> first = cook_clip(wav, settings);
        const core::result<std::vector<byte>> second = cook_clip(wav, settings);
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        CHECK(first.value() == second.value());
    }

    TEST_CASE("load_clip_blob - a PCM blob becomes a clip with its own buffer") {
        const std::vector<byte> wav = tone_wav(0.1);
        const core::result<std::vector<byte>> blob = cook_clip(wav, clip_import_settings{});
        CONJURE_REQUIRE_OK(blob);
        const core::result<audio_clip> clip = load_clip_blob(blob.value());
        CONJURE_REQUIRE_OK(clip);
        CHECK(clip.value().is_valid());
        CHECK_FALSE(clip.value().is_streamed());
        CHECK(clip.value().info.frameCount == 4800);
        CHECK(clip.value().info.encoding == clip_encoding::WAV);
        CHECK(clip.value().duration_seconds() == doctest::Approx(0.1));
        REQUIRE(clip.value().pcm != nullptr);
        const core::result<pcm_buffer> reference = decode_to_pcm(wav, decode_options{});
        CONJURE_REQUIRE_OK(reference);
        CHECK(clip.value().pcm->samples().size() == reference.value().samples().size());
        CHECK(clip.value().pcm->samples()[100] == doctest::Approx(static_cast<f64>(reference.value().samples()[100])));

        // Loading again publishes a distinct buffer while the first stays valid (hot reload, A6).
        const core::result<audio_clip> again = load_clip_blob(blob.value());
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value().pcm.get() != clip.value().pcm.get());
        CHECK(clip.value().pcm->frame_count() == 4800);
    }

    TEST_CASE("load_clip_blob - a streamed blob keeps the encoded bytes and probes them") {
        clip_import_settings settings = {};
        settings.storage = clip_storage_setting::STREAMED;
        const std::vector<byte> wav = tone_wav(0.2, 2);
        const core::result<std::vector<byte>> blob = cook_clip(wav, settings);
        CONJURE_REQUIRE_OK(blob);
        const core::result<audio_clip> clip = load_clip_blob(blob.value());
        CONJURE_REQUIRE_OK(clip);
        CHECK(clip.value().is_streamed());
        CHECK(clip.value().is_valid());
        REQUIRE(clip.value().encoded != nullptr);
        CHECK(*clip.value().encoded == wav);
        CHECK(clip.value().info.frameCount == 9600);
        CHECK(clip.value().info.format.channels == 2);
    }

    TEST_CASE("load_clip_blob - bad magic, version, size and channels fail with INVALID_CLIP") {
        const core::result<std::vector<byte>> cooked = cook_clip(tone_wav(0.05), clip_import_settings{});
        CONJURE_REQUIRE_OK(cooked);
        const std::vector<byte> &blob = cooked.value();

        std::vector<byte> badMagic = blob;
        badMagic[0] = byte{'X'};
        CONJURE_CHECK_ERROR(load_clip_blob(badMagic), audio_error::INVALID_CLIP);

        std::vector<byte> badVersion = blob;
        badVersion[4] = byte{9};
        CONJURE_CHECK_ERROR(load_clip_blob(badVersion), audio_error::INVALID_CLIP);

        std::vector<byte> badSize = blob;
        badSize.pop_back();
        CONJURE_CHECK_ERROR(load_clip_blob(badSize), audio_error::INVALID_CLIP);

        std::vector<byte> badChannels = blob;
        badChannels[9] = byte{6};
        CONJURE_CHECK_ERROR(load_clip_blob(badChannels), audio_error::INVALID_CLIP);

        std::vector<byte> badStorage = blob;
        badStorage[8] = byte{5};
        CONJURE_CHECK_ERROR(load_clip_blob(badStorage), audio_error::INVALID_CLIP);

        std::vector<byte> tooShort(blob.begin(), blob.begin() + 10);
        CONJURE_CHECK_ERROR(load_clip_blob(tooShort), audio_error::INVALID_CLIP);
        CONJURE_CHECK_ERROR(load_clip_blob({}), audio_error::INVALID_CLIP);

        // A streamed header over bytes that do not decode fails on the probe.
        clip_import_settings settings = {};
        settings.storage = clip_storage_setting::STREAMED;
        const core::result<std::vector<byte>> streamed = cook_clip(tone_wav(0.05), settings);
        CONJURE_REQUIRE_OK(streamed);
        std::vector<byte> scrambled = streamed.value();
        for (usize index = sizeof(clip_blob_header); index < scrambled.size(); ++index) {
            scrambled[index] = byte{0x33};
        }
        CHECK_FALSE(load_clip_blob(scrambled).has_value());
    }

    TEST_CASE("load_clip_blob - a streamed payload whose format disagrees with the header is INVALID_CLIP") {
        clip_import_settings settings = {};
        settings.storage = clip_storage_setting::STREAMED;
        const core::result<std::vector<byte>> cooked = cook_clip(tone_wav(0.05), settings);
        CONJURE_REQUIRE_OK(cooked);
        CONJURE_REQUIRE_OK(load_clip_blob(cooked.value()));

        std::vector<byte> mismatched = cooked.value();
        clip_blob_header header = header_of(mismatched);
        header.sampleRate = 44100;
        std::memcpy(mismatched.data(), &header, sizeof(header));
        CONJURE_REQUIRE_OK(read_clip_blob_header(mismatched));
        const core::result<audio_clip> loaded = load_clip_blob(mismatched);
        CONJURE_CHECK_ERROR(loaded, audio_error::INVALID_CLIP);
        CHECK(loaded.get_error().message().find("disagrees with its payload") != std::string::npos);

        header = header_of(cooked.value());
        header.channels = 2;
        std::memcpy(mismatched.data(), &header, sizeof(header));
        CONJURE_CHECK_ERROR(load_clip_blob(mismatched), audio_error::INVALID_CLIP);
    }

    TEST_CASE("regression - code sweep F183 a PCM frameCount that wraps the size check fails with INVALID_CLIP") {
        // frameCount * channels * 4 overflows to exactly the real payload size for these offsets.
        u16 channels = 1;
        u64 wrappingOffset = 0;
        SUBCASE("mono") {
            channels = 1;
            wrappingOffset = 1ull << 62;
        }
        SUBCASE("stereo") {
            channels = 2;
            wrappingOffset = 1ull << 61;
        }
        const core::result<std::vector<byte>> cooked = cook_clip(tone_wav(0.1, channels), clip_import_settings{});
        CONJURE_REQUIRE_OK(cooked);
        std::vector<byte> forged = cooked.value();
        clip_blob_header header = header_of(forged);
        REQUIRE(header.storage == static_cast<u8>(clip_storage::PCM));
        header.frameCount += wrappingOffset;
        std::memcpy(forged.data(), &header, sizeof(header));

        CONJURE_CHECK_ERROR(read_clip_blob_header(forged), audio_error::INVALID_CLIP);
        CONJURE_CHECK_ERROR(load_clip_blob(forged), audio_error::INVALID_CLIP);
        CONJURE_REQUIRE_OK(load_clip_blob(cooked.value()));
    }

    TEST_CASE("audio_clip_importer - identity, extensions and default settings") {
        const audio_clip_importer importer = {};
        CHECK(importer.id() == CLIP_IMPORTER_ID);
        CHECK(importer.id() == "conjure.audio.clip");
        CHECK(importer.version() == 1);
        CHECK(importer.extensions().size() == 4);
        CHECK(importer.extensions()[0] == ".wav");
        const std::string expectedDefaults = clip_import_settings{}.to_document().to_canonical_string();
        CHECK(importer.default_settings().to_canonical_string() == expectedDefaults);
        CHECK(assets::asset_type_traits<audio_clip>::NAME == "conjure.audio.clip");
        CHECK(assets::asset_type_traits<audio_clip>::id() == assets::make_asset_type_id("conjure.audio.clip"));
    }

    TEST_CASE("audio_clip_importer - import produces one primary clip asset and reports bad input") {
        core::vfs::memory_file_system fileSystem = {};
        assets::sub_asset_table subAssets = {};
        const core::uuid id = core::uuid::generate();
        const std::vector<byte> wav = tone_wav(0.1, 2);
        const core::json::document settings = clip_import_settings{}.to_document();
        assets::import_context context(
          fileSystem,
          "assets://sounds/beep.wav",
          "sounds/beep.wav",
          wav,
          settings.root(),
          "windows",
          id,
          subAssets,
          {}
        );
        const audio_clip_importer importer = {};
        const core::result<assets::import_output> output = importer.import(context);
        CONJURE_REQUIRE_OK(output);
        REQUIRE(output.value().assets.size() == 1);
        const assets::imported_asset &primary = output.value().assets[0];
        CHECK(primary.subPath.empty());
        CHECK(primary.displayName == "beep");
        CHECK(primary.type == assets::asset_type_traits<audio_clip>::id());
        CHECK(primary.dependencies.empty());
        const core::result<std::vector<byte>> reference = cook_clip(wav, clip_import_settings{});
        CONJURE_REQUIRE_OK(reference);
        CHECK(primary.payload == reference.value());

        const std::vector<byte> garbage(100, byte{1});
        assets::import_context badContext(
          fileSystem,
          "assets://sounds/bad.wav",
          "sounds/bad.wav",
          garbage,
          settings.root(),
          "windows",
          id,
          subAssets,
          {}
        );
        const core::result<assets::import_output> failed = importer.import(badContext);
        CONJURE_CHECK_ERROR(failed, assets::assets_error::IMPORT_FAILED);
        CHECK(failed.get_error().message().find("UNSUPPORTED_FORMAT") != std::string::npos);
        CHECK(failed.get_error().message().find("bad.wav") != std::string::npos);
    }

    TEST_CASE("audio_clip_importer - PCM-only settings on a streamed clip report an import warning") {
        const std::vector<byte> wav = tone_wav(0.1, 2);

        const std::vector<std::string> forcedMono = import_warnings(wav, R"({"storage":"streamed","forceMono":true})");
        REQUIRE(forcedMono.size() == 1);
        CHECK(forcedMono[0].find("PCM storage only") != std::string::npos);

        // "auto" picks STREAMED above the threshold, which is the case the author is least likely to notice.
        const char *autoStreamed = R"({"storage":"auto","streamThresholdSeconds":0.05,"sampleRate":24000})";
        CHECK(import_warnings(wav, autoStreamed).size() == 1);
        CHECK(import_warnings(wav, R"({"storage":"streamed","normalize":true})").size() == 1);

        // PCM storage applies the settings, and a streamed clip without them has nothing to report.
        CHECK(import_warnings(wav, R"({"storage":"pcm","forceMono":true})").empty());
        CHECK(import_warnings(wav, R"({"storage":"auto","forceMono":true,"normalize":true})").empty());
        CHECK(import_warnings(wav, R"({"storage":"streamed"})").empty());
    }

    TEST_CASE("register_asset_types - registers the clip type and importer once") {
        assets::asset_type_registry types = {};
        assets::importer_registry importers = {};
        CONJURE_REQUIRE_OK(register_asset_types(types, importers));
        const assets::asset_type_desc *desc = types.find(assets::asset_type_traits<audio_clip>::id());
        REQUIRE(desc != nullptr);
        CHECK(desc->name == "conjure.audio.clip");
        CHECK(desc->version == 1);
        CHECK(desc->loader != nullptr);
        CHECK(importers.find(CLIP_IMPORTER_ID) != nullptr);
        CHECK(importers.find_for_extension(".ogg") == importers.find(CLIP_IMPORTER_ID));
        CHECK_FALSE(register_asset_types(types, importers).has_value());
    }

    TEST_CASE("audio_clip_loader - a blob loads through memory_asset_source and load_blocking") {
        testing::log_capture logs = {};
        manager_fixture fixture = {};
        const core::uuid id = core::uuid::generate();
        const core::result<std::vector<byte>> blob = cook_clip(tone_wav(0.1), clip_import_settings{});
        CONJURE_REQUIRE_OK(blob);
        fixture.source->add(wrap_blob(id, blob.value()), "assets://tone.wav");

        const assets::asset_ref<audio_clip> ref = fixture.manager->bind<audio_clip>(id);
        CHECK(ref.get() == nullptr);
        const core::result<assets::asset_handle> handle = fixture.manager->load_blocking(id);
        CONJURE_REQUIRE_OK(handle);
        const audio_clip *clip = ref.get();
        REQUIRE(clip != nullptr);
        CHECK(clip->is_valid());
        CHECK(clip->info.frameCount == 4800);
        CHECK(ref.version() > 0);
    }

    TEST_CASE("audio_clip_loader - a corrupt blob fails the slot and the fallback is the silent clip") {
        testing::log_capture logs = {};
        logs.allow_errors();
        manager_fixture fixture = {};
        const core::uuid id = core::uuid::generate();
        std::vector<byte> corrupt(48, byte{0});
        fixture.source->add(wrap_blob(id, corrupt), "assets://broken.wav");

        const assets::asset_ref<audio_clip> ref = fixture.manager->bind<audio_clip>(id);
        const core::result<assets::asset_handle> handle = fixture.manager->load_blocking(id);
        CHECK_FALSE(handle.has_value());
        CHECK(ref.get() == nullptr);
        CHECK(ref.state() == assets::asset_state::FAILED);
        const core::error *failure = fixture.manager->last_error(id);
        REQUIRE(failure != nullptr);
        CHECK(failure->message().find("INVALID_CLIP") != std::string::npos);

        const audio_clip *fallback = fixture.manager->get_or_fallback(ref);
        REQUIRE(fallback != nullptr);
        CHECK(fallback->info.frameCount == 4800);
        CHECK(fallback->info.format.channels == 1);
        CHECK(fallback->info.format.sampleRate == 48000);
        REQUIRE(fallback->pcm != nullptr);
        CHECK(testing::peak_of(fallback->pcm->samples()) == doctest::Approx(0.0));
    }

    TEST_CASE("audio_clip_loader - reload publishes a new buffer while the old shared_ptr stays valid") {
        testing::log_capture logs = {};
        manager_fixture fixture = {};
        const core::uuid id = core::uuid::generate();
        const core::result<std::vector<byte>> blob = cook_clip(tone_wav(0.1), clip_import_settings{});
        CONJURE_REQUIRE_OK(blob);
        fixture.source->add(wrap_blob(id, blob.value()), "assets://tone.wav");
        const assets::asset_ref<audio_clip> ref = fixture.manager->bind<audio_clip>(id);
        CONJURE_REQUIRE_OK(fixture.manager->load_blocking(id));
        REQUIRE(ref.get() != nullptr);
        const std::shared_ptr<const pcm_buffer> held = ref.get()->pcm;
        const u32 firstVersion = ref.version();

        const core::result<std::vector<byte>> replacement = cook_clip(tone_wav(0.2), clip_import_settings{});
        CONJURE_REQUIRE_OK(replacement);
        fixture.source->add(wrap_blob(id, replacement.value()), "assets://tone.wav");
        fixture.manager->reload(id);
        fixture.manager->flush();
        for (u32 frame = 0; frame < 8; ++frame) {
            fixture.manager->begin_frame(frame + 1);
            fixture.manager->update();
            fixture.manager->end_frame();
        }
        REQUIRE(ref.get() != nullptr);
        CHECK(ref.version() > firstVersion);
        CHECK(ref.get()->info.frameCount == 9600);
        CHECK(ref.get()->pcm.get() != held.get());
        CHECK(held->frame_count() == 4800);
        CHECK(held.use_count() == 1);
    }
}
