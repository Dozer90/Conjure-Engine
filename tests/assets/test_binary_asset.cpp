#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/binary_asset.h"

#include <memory>
#include <string>
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
        CHECK(importer.extensions().size() == 4);
        CHECK(importer.extensions()[0] == ".bin");
        CHECK(importer.default_settings().to_string() == "{}");
        CHECK(asset_type_traits<binary_asset>::NAME == "conjure.assets.binary");
    }

    TEST_CASE("register_binary_asset_type - registers the type and optionally the importer") {
        asset_type_registry types = {};
        importer_registry importers = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(types, &importers));
        REQUIRE(types.find(asset_type_traits<binary_asset>::id()) != nullptr);
        CHECK(types.find(asset_type_traits<binary_asset>::id())->version == 1);
        CHECK(importers.find_for_extension(".BIN") != nullptr);
        CONJURE_CHECK_ERROR(register_binary_asset_type(types, &importers), asset_error_code::TYPE_DUPLICATE);

        asset_type_registry typesOnly = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(typesOnly));
        CHECK(typesOnly.size() == 1);
    }

    TEST_CASE("binary_asset - importer and loader round trip through the pipeline") {
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(register_binary_asset_type(fixture.manager->types(), &fixture.importers));
        fixture.write_source("data/blob.bin", "\x01\x02\x03raw");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("data/blob.bin");
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
        fixture.write_source("empty.bin", "");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("empty.bin");
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

    TEST_CASE("binary_loader - the fallback is an empty asset") {
        const binary_loader loader = {};
        core::result<asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        REQUIRE(fallback.value() != nullptr);
        CHECK(payload_as<binary_asset>(fallback.value())->bytes.empty());
    }
}
