#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/binary_asset.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

TEST_SUITE("assets/binary_asset") {

    TEST_CASE("binary_importer - identity, version and extensions") {
        const binary_importer importer = {};
        CHECK(importer.id() == "conjure.assets.binary");
        CHECK(importer.version() == 1);
        const std::vector<std::string_view> extensions(importer.extensions().begin(), importer.extensions().end());
        CHECK(extensions == std::vector<std::string_view>{".dat", ".bytes", ".raw"});
        CHECK(importer.default_settings().to_string() == "{}");
        CHECK(asset_type_traits<binary_asset>::NAME == "conjure.assets.binary");
    }

    TEST_CASE("binary_importer - claims .dat, .bytes and .raw in any case, never .bin") {
        importer_registry importers = {};
        CONJURE_REQUIRE_OK(importers.register_importer(std::make_unique<binary_importer>()));
        constexpr std::array<std::string_view, 6> CLAIMED = {".dat", ".bytes", ".raw", ".DAT", ".Bytes", ".RAW"};
        for (const std::string_view extension : CLAIMED) {
            CAPTURE(extension);
            const asset_importer *found = importers.find_for_extension(extension);
            REQUIRE(found != nullptr);
            CHECK(found->id() == binary_importer::ID);
        }
        // .bin is glTF's external buffer, a side file of the .gltf that reads it; .byte is a near miss.
        constexpr std::array<std::string_view, 3> UNCLAIMED = {".bin", ".BIN", ".byte"};
        for (const std::string_view extension : UNCLAIMED) {
            CAPTURE(extension);
            CHECK(importers.find_for_extension(extension) == nullptr);
        }
        CHECK(importers.find_for_path("data/loot.bin") == nullptr);
        const asset_importer *renamed = importers.find_for_path("data/loot.bin.bytes");
        REQUIRE(renamed != nullptr);
        CHECK(renamed->id() == binary_importer::ID);
    }

    TEST_CASE("register_binary_asset_type - registers the type and optionally the importer") {
        asset_type_registry types = {};
        importer_registry importers = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(types, &importers));
        REQUIRE(types.find(asset_type_traits<binary_asset>::id()) != nullptr);
        CHECK(types.find(asset_type_traits<binary_asset>::id())->version == 1);
        CHECK(importers.find_for_extension(".BYTES") != nullptr);
        CONJURE_CHECK_ERROR(register_binary_asset_type(types, &importers), asset_error_code::TYPE_DUPLICATE);

        asset_type_registry typesOnly = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(typesOnly));
        CHECK(typesOnly.size() == 1);
    }

    TEST_CASE("binary_asset - importer and loader round trip through the pipeline") {
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(fixture.manager->types(), &fixture.importers));
        fixture.write_source("data/blob.bytes", "\x01\x02\x03raw");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("data/blob.bytes");
        const std::optional<asset_record> record = fixture.database->find(id);
        REQUIRE(record.has_value());
        CHECK(record->type == asset_type_traits<binary_asset>::id());
        CHECK(record->importerId == "conjure.assets.binary");

        const asset_ref<binary_asset> ref = fixture.manager->bind<binary_asset>(id);
        asset_handle handle = fixture.manager->load(id);
        settle(*fixture.manager);
        REQUIRE(ref.get() != nullptr);
        CHECK(ref.get()->bytes == bytes_of("\x01\x02\x03raw"));
        CHECK(fixture.manager->fallback<binary_asset>() != nullptr);
        CHECK(fixture.manager->fallback<binary_asset>()->bytes.empty());
    }

    TEST_CASE("binary_asset - an empty file imports to an empty payload") {
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(fixture.manager->types(), &fixture.importers));
        fixture.write_source("empty.raw", "");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("empty.raw");
        core::result<asset_handle> handle = fixture.manager->load_blocking(id);
        CONJURE_REQUIRE_OK(handle);
        REQUIRE(handle.value().get<binary_asset>() != nullptr);
        CHECK(handle.value().get<binary_asset>()->bytes.empty());
    }

    TEST_CASE("binary_asset - a 64 MiB file round-trips byte for byte") {
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(fixture.manager->types(), &fixture.importers));
        std::string large(64 * MIB, '\0');
        for (usize index = 0; index < large.size(); index += 4099) {
            large[index] = static_cast<char>(index % 251);
        }
        fixture.write_source("large.dat", large);
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("large.dat");
        core::result<asset_handle> handle = fixture.manager->load_blocking(id);
        CONJURE_REQUIRE_OK(handle);
        const binary_asset *payload = handle.value().get<binary_asset>();
        REQUIRE(payload != nullptr);
        REQUIRE(payload->bytes.size() == large.size());
        CHECK(text_of(payload->bytes) == large);
        CHECK(fixture.manager->stats().bytesResident == large.size());
    }

    TEST_CASE("regression - code sweep F311 a .bin side file is not imported a second time as a binary asset") {
        // The shape of a .gltf with an external buffer: the owner reads robot.bin as a side file.
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(fixture.manager->types(), &fixture.importers));
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/models/robot.bin", bytes_of("buffer")));
        fixture.write_source("models/robot.ta", "side robot.bin\nrobot");
        fixture.scan_and_import();
        const asset_id owner = fixture.id_of("models/robot.ta");
        CHECK(fixture.database->owners_of_side_file("assets://models/robot.bin") == std::vector<asset_id>{owner});
        REQUIRE(fixture.database->find(owner).has_value());
        CHECK(fixture.database->find(owner)->status == asset_record_status::OK);

        // The buffer still gets a meta and a browsable record, but no importer, no import and no blob.
        const asset_id buffer = fixture.id_of("models/robot.bin");
        CHECK(fixture.fileSystem->exists(meta_path_of("assets/models/robot.bin")));
        const std::optional<asset_record> record = fixture.database->find(buffer);
        REQUIRE(record.has_value());
        CHECK(record->status == asset_record_status::NO_IMPORTER);
        CHECK(record->importerId.empty());
        CHECK(record->type == asset_type_id::INVALID);
        CHECK_FALSE(fixture.cache().contains(buffer));
        CONJURE_CHECK_ERROR(fixture.manager->load_blocking(buffer), asset_error_code::NO_IMPORTER);

        // The same bytes named .bytes are a raw data file and import as one.
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/data/robot.bytes", bytes_of("buffer")));
        fixture.scan_and_import();
        const asset_id data = fixture.id_of("data/robot.bytes");
        REQUIRE(fixture.database->find(data).has_value());
        CHECK(fixture.database->find(data)->type == asset_type_traits<binary_asset>::id());
        CHECK(fixture.cache().contains(data));
    }

    TEST_CASE("binary_loader - the fallback is an empty asset") {
        const binary_loader loader = {};
        core::result<asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        REQUIRE(fallback.value() != nullptr);
        CHECK(payload_as<binary_asset>(fallback.value())->bytes.empty());
    }
}
