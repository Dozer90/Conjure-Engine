#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "renderer_test_support.h"

#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/assets_error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// material_asset.h (docs/design/02_renderer.md R7, 4.2): the one PBR material model with glTF
// defaults, its canonical JSON form, the magenta fallback, the .cmat importer and the loader.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] material_asset sample_material() {
        material_desc desc = {};
        desc.baseColourFactor = math::vec4(0.5f, 0.25f, 1.0f, 0.75f);
        desc.emissiveFactor = math::vec3(1.0f, 0.5f, 0.0f);
        desc.emissiveStrength = 4.0f;
        desc.metallicFactor = 0.0f;
        desc.roughnessFactor = 0.3f;
        desc.normalScale = 0.8f;
        desc.occlusionStrength = 0.6f;
        desc.alphaCutoff = 0.4f;
        desc.alphaMode = alpha_mode::MASK;
        desc.doubleSided = true;
        desc.unlit = false;
        desc.textures[static_cast<usize>(material_texture::BASE_COLOUR)] = assets::asset_ref<texture_asset>(
          core::uuid(1, 1)
        );
        desc.textures[static_cast<usize>(material_texture::NORMAL)] = assets::asset_ref<texture_asset>(
          core::uuid(2, 2)
        );
        desc.textureUvSets = {0, 0, 1, 0, 0};
        return material_asset(desc, "sample");
    }

    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }
}

TEST_SUITE("renderer/material_asset") {

    TEST_CASE("material_desc - the defaults are the glTF metallic-roughness defaults") {
        const material_desc desc = {};
        CONJURE_CHECK_APPROX(desc.baseColourFactor, math::vec4(1.0f));
        CONJURE_CHECK_APPROX(desc.emissiveFactor, math::vec3(0.0f));
        CHECK(desc.emissiveStrength == doctest::Approx(1.0f));
        CHECK(desc.metallicFactor == doctest::Approx(1.0f));
        CHECK(desc.roughnessFactor == doctest::Approx(1.0f));
        CHECK(desc.normalScale == doctest::Approx(1.0f));
        CHECK(desc.occlusionStrength == doctest::Approx(1.0f));
        CHECK(desc.alphaCutoff == doctest::Approx(0.5f));
        CHECK(desc.alphaMode == alpha_mode::SOLID);
        CHECK_FALSE(desc.doubleSided);
        CHECK_FALSE(desc.unlit);
        for (const assets::asset_ref<texture_asset> &texture : desc.textures) {
            CHECK(texture.id().is_null());
        }
        for (const u8 uvSet : desc.textureUvSets) {
            CHECK(uvSet == 0);
        }
        CHECK(desc == material_desc());
        CHECK(material_asset().texture_flags() == 0);
        CHECK(to_string(alpha_mode::SOLID) == "SOLID");
        CHECK(to_string(alpha_mode::MASK) == "MASK");
        CHECK(to_string(alpha_mode::BLEND) == "BLEND");
        CHECK(to_string(material_texture::BASE_COLOUR) == "BASE_COLOUR");
        CHECK(to_string(material_texture::EMISSIVE) == "EMISSIVE");
    }

    TEST_CASE("material_asset - texture_flags sets one bit per referenced slot") {
        const material_asset material = sample_material();
        CHECK(material.texture_flags() == ((1u << 0) | (1u << 2)));
        material_asset all = material;
        for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
            all.desc().textures[slot] = assets::asset_ref<texture_asset>(core::uuid(9, slot + 1));
        }
        CHECK(all.texture_flags() == 0x1Fu);
        CHECK(material.name() == "sample");
        all.set_name("all");
        CHECK(all.name() == "all");
        CHECK_FALSE(all == material);
    }

    TEST_CASE("material_asset - to_json is canonical and parse_json restores every field") {
        const material_asset material = sample_material();
        const std::string json = material.to_json();
        CHECK(json == material.to_json());
        // Sorted keys: alphaCutoff before alphaMode before baseColourFactor ... before unlit.
        CHECK(json.find("\"alphaCutoff\"") < json.find("\"alphaMode\""));
        CHECK(json.find("\"alphaMode\"") < json.find("\"baseColourFactor\""));
        CHECK(json.find("\"textures\"") < json.find("\"unlit\""));
        CHECK(json.find("\"mask\"") != std::string::npos);
        CHECK(json.find(core::uuid(1, 1).to_string()) != std::string::npos);
        CHECK(json.find(core::uuid(2, 2).to_string()) != std::string::npos);
        core::result<material_asset> parsed = material_asset::parse_json(json);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == material);
        CHECK(parsed.value().name() == "sample");
        CHECK(parsed.value().desc().textures[2].id() == core::uuid(2, 2));
        CHECK_FALSE(parsed.value().desc().textures[2].is_bound());
        CHECK(parsed.value().desc().textureUvSets[2] == 1);
        CHECK(parsed.value().texture_flags() == material.texture_flags());
        // The blob form is the JSON bytes.
        const std::vector<std::byte> blob = material.write_blob();
        CHECK(blob.size() == json.size());
        core::result<material_asset> fromBlob = material_asset::parse_blob(blob);
        CONJURE_REQUIRE_OK(fromBlob);
        CHECK(fromBlob.value() == material);
    }

    TEST_CASE("material_asset - parse_json applies defaults for missing keys and accepts every alpha mode") {
        core::result<material_asset> empty = material_asset::parse_json("{}");
        CONJURE_REQUIRE_OK(empty);
        CHECK(empty.value().desc() == material_desc());
        CHECK(empty.value().name().empty());
        core::result<material_asset> blend = material_asset::parse_json(
          R"({"alphaMode": "blend", "roughnessFactor": 0.1, "name": "glass"})"
        );
        CONJURE_REQUIRE_OK(blend);
        CHECK(blend.value().desc().alphaMode == alpha_mode::BLEND);
        CHECK(blend.value().desc().roughnessFactor == doctest::Approx(0.1f));
        CHECK(blend.value().name() == "glass");
        core::result<material_asset> opaque = material_asset::parse_json(
          R"({"alphaMode": "opaque", "unlit": true, "textures": {"emissive": null}})"
        );
        CONJURE_REQUIRE_OK(opaque);
        CHECK(opaque.value().desc().alphaMode == alpha_mode::SOLID);
        CHECK(opaque.value().desc().unlit);
        CHECK(opaque.value().texture_flags() == 0);
    }

    TEST_CASE(
      "material_asset - parse_json fails with PARSE_FAILURE for bad JSON and INVALID_ARGUMENT for bad members"
    ) {
        CONJURE_CHECK_ERROR(material_asset::parse_json("{"), core::error_code::PARSE_FAILURE);
        CONJURE_CHECK_ERROR(material_asset::parse_json(""), core::error_code::PARSE_FAILURE);
        CONJURE_CHECK_ERROR(material_asset::parse_json("[1, 2, 3]"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(material_asset::parse_json(R"({"name": 5})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"baseColourFactor": [1, 2]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"emissiveFactor": "red"})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"metallicFactor": "high"})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(material_asset::parse_json(R"({"doubleSided": 1})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"alphaMode": "cutout"})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(material_asset::parse_json(R"({"alphaMode": 2})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(material_asset::parse_json(R"({"textures": []})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"textures": {"normal": 7}})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"textures": {"normal": "not-a-uuid"}})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"textureUvSets": [0, 1]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          material_asset::parse_json(R"({"textureUvSets": [0, 0, 0, 0, 7]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(material_asset::parse_blob(bytes_of("nonsense")), core::error_code::PARSE_FAILURE);
    }

    TEST_CASE("material_asset - the fallback is an unlit magenta material") {
        const material_asset fallback = material_asset::fallback();
        CHECK(fallback.desc().unlit);
        CHECK(fallback.desc().baseColourFactor.x == doctest::Approx(1.0f));
        CHECK(fallback.desc().baseColourFactor.y == doctest::Approx(0.0f));
        CHECK(fallback.desc().baseColourFactor.z == doctest::Approx(1.0f));
        CHECK(fallback.desc().baseColourFactor.w == doctest::Approx(1.0f));
        CHECK(fallback.desc().alphaMode == alpha_mode::SOLID);
        CHECK(fallback.texture_flags() == 0);
        CHECK_FALSE(fallback.name().empty());
        core::result<material_asset> parsed = material_asset::parse_json(fallback.to_json());
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == fallback);
    }

    TEST_CASE("material_importer - validates the JSON, cooks it canonically and lists texture dependencies") {
        const material_importer importer = {};
        CHECK(importer.id() == MATERIAL_IMPORTER_ID);
        CHECK(importer.version() == MATERIAL_IMPORTER_VERSION);
        REQUIRE(importer.extensions().size() == 1);
        CHECK(importer.extensions()[0] == ".cmat");
        CHECK(importer.default_settings().to_string() == "{}");

        core::vfs::memory_file_system fileSystem = {};
        assets::sub_asset_table subAssets = {};
        // Unsorted, duplicated references: the dependency list comes back sorted and unique.
        const std::string normalId = core::uuid(2, 2).to_string();
        const std::string colourId = core::uuid(1, 1).to_string();
        const std::string source = std::string(R"({"name": "floor", "textures": {"normal": ")") + normalId +
          R"(", "baseColour": ")" + colourId + R"(", "emissive": ")" + colourId + R"("}, "roughnessFactor": 0.5})";
        const std::vector<std::byte> bytes = bytes_of(source);
        assets::import_context context(
          fileSystem,
          "assets://floor.cmat",
          "assets/floor.cmat",
          bytes,
          {},
          "windows",
          renderer_support::test_asset_id(7),
          subAssets,
          {}
        );
        core::result<assets::import_output> imported = importer.import(context);
        CONJURE_REQUIRE_OK(imported);
        REQUIRE(imported.value().assets.size() == 1);
        const assets::imported_asset &primary = imported.value().assets[0];
        CHECK(primary.subPath.empty());
        CHECK(primary.type == assets::asset_type_traits<material_asset>::id());
        CHECK(primary.displayName == "floor");
        REQUIRE(primary.dependencies.size() == 2);
        CHECK(primary.dependencies[0] == core::uuid(1, 1));
        CHECK(primary.dependencies[1] == core::uuid(2, 2));
        core::result<material_asset> cooked = material_asset::parse_blob(primary.payload);
        CONJURE_REQUIRE_OK(cooked);
        CHECK(cooked.value().desc().roughnessFactor == doctest::Approx(0.5f));
        CHECK(cooked.value().texture_flags() == ((1u << 0) | (1u << 2) | (1u << 4)));
        const std::string payloadText(reinterpret_cast<const char *>(primary.payload.data()), primary.payload.size());
        CHECK(cooked.value().to_json() == payloadText);

        const std::vector<std::byte> broken = bytes_of("{\"alphaMode\": \"glass\"}");
        assets::import_context badContext(
          fileSystem,
          "assets://b.cmat",
          "assets/b.cmat",
          broken,
          {},
          "windows",
          renderer_support::test_asset_id(8),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(badContext), assets::assets_error::IMPORT_FAILED);
        const std::vector<std::byte> garbage = bytes_of("{{{");
        assets::import_context garbageContext(
          fileSystem,
          "assets://c.cmat",
          "assets/c.cmat",
          garbage,
          {},
          "windows",
          renderer_support::test_asset_id(9),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(garbageContext), assets::assets_error::IMPORT_FAILED);
    }

    TEST_CASE("material_importer - unknown keys such as baseColorFactor are reported as import warnings") {
        const material_importer importer = {};
        core::vfs::memory_file_system fileSystem = {};
        assets::sub_asset_table subAssets = {};
        const std::vector<std::byte> gltfSpelling = bytes_of(
          R"({"baseColorFactor": [1, 0, 0, 1], "roughnessFactor": 0.5, "textures": {"albedo": null}})"
        );
        assets::import_context context(
          fileSystem,
          "assets://red.cmat",
          "assets/red.cmat",
          gltfSpelling,
          {},
          "windows",
          renderer_support::test_asset_id(11),
          subAssets,
          {}
        );
        core::result<assets::import_output> imported = importer.import(context);
        CONJURE_REQUIRE_OK(imported);
        REQUIRE(context.warnings().size() == 2);
        CHECK(context.warnings()[0].find("'baseColorFactor'") != std::string::npos);
        CHECK(context.warnings()[0].find("did you mean 'baseColourFactor'") != std::string::npos);
        CHECK(context.warnings()[1].find("'albedo'") != std::string::npos);
        // The keys are still ignored: the import keeps the default white.
        core::result<material_asset> cooked = material_asset::parse_blob(imported.value().assets[0].payload);
        CONJURE_REQUIRE_OK(cooked);
        CHECK(cooked.value().desc().baseColourFactor == math::vec4(1.0f));
        CHECK(cooked.value().desc().roughnessFactor == doctest::Approx(0.5f));

        // A spelling is suggested only where it is a key: baseColour exists only inside "textures"
        // and baseColourFactor only at the top level.
        const std::vector<std::byte> misplaced = bytes_of(
          R"({"baseColor": [1, 0, 0, 1], "textures": {"baseColor": null, "baseColorFactor": null}})"
        );
        assets::import_context misplacedContext(
          fileSystem,
          "assets://misplaced.cmat",
          "assets/misplaced.cmat",
          misplaced,
          {},
          "windows",
          renderer_support::test_asset_id(13),
          subAssets,
          {}
        );
        CONJURE_REQUIRE_OK(importer.import(misplacedContext));
        REQUIRE(misplacedContext.warnings().size() == 3);
        CHECK(misplacedContext.warnings()[0].find("unknown key 'baseColor'") != std::string::npos);
        CHECK(misplacedContext.warnings()[0].find("did you mean") == std::string::npos);
        CHECK(misplacedContext.warnings()[1].find("'baseColor'") != std::string::npos);
        CHECK(misplacedContext.warnings()[1].find("did you mean 'baseColour'") != std::string::npos);
        CHECK(misplacedContext.warnings()[2].find("'baseColorFactor'") != std::string::npos);
        CHECK(misplacedContext.warnings()[2].find("did you mean") == std::string::npos);

        // The canonical form uses only known keys.
        const std::vector<std::byte> canonical = material_asset::fallback().write_blob();
        assets::import_context canonicalContext(
          fileSystem,
          "assets://magenta.cmat",
          "assets/magenta.cmat",
          canonical,
          {},
          "windows",
          renderer_support::test_asset_id(12),
          subAssets,
          {}
        );
        CONJURE_REQUIRE_OK(importer.import(canonicalContext));
        CHECK(canonicalContext.warnings().empty());
    }

    TEST_CASE("material_loader - loads the JSON payload and falls back to the magenta material") {
        const material_loader loader = {};
        CHECK(loader.type() == assets::asset_type_traits<material_asset>::id());
        const material_asset source = sample_material();
        const std::vector<std::byte> blob = source.write_blob();
        assets::load_context context = {};
        context.id = renderer_support::test_asset_id(10);
        context.type = loader.type();
        core::result<assets::asset_payload> loaded = loader.load_cpu(context, blob);
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(assets::payload_as<material_asset>(loaded.value()) != nullptr);
        CHECK(*assets::payload_as<material_asset>(loaded.value()) == source);
        core::result<assets::asset_payload> finalised = loader.finalise(
          assets::finalise_context{},
          std::move(loaded).value()
        );
        CONJURE_REQUIRE_OK(finalised);
        CHECK(*assets::payload_as<material_asset>(finalised.value()) == source);
        CONJURE_CHECK_ERROR(loader.load_cpu(context, bytes_of("{")), core::error_code::PARSE_FAILURE);
        core::result<assets::asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        REQUIRE(assets::payload_as<material_asset>(fallback.value()) != nullptr);
        CHECK(*assets::payload_as<material_asset>(fallback.value()) == material_asset::fallback());
        CHECK(MATERIAL_ASSET_TYPE_NAME == "conjure.renderer.material");
    }
}
