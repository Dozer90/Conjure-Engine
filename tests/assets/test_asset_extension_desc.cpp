#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_extension_desc.h"

#include <memory>
#include <type_traits>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    std::unique_ptr<asset_importer> create_test_importer() {
        return std::make_unique<test_importer>();
    }

    std::unique_ptr<asset_importer> create_nothing() {
        return nullptr;
    }

    std::unique_ptr<asset_loader> create_test_loader() {
        return std::make_unique<test_loader>();
    }

    std::unique_ptr<asset_loader> create_no_loader() {
        return nullptr;
    }
}

TEST_SUITE("assets/asset_extension_desc") {

    TEST_CASE("extension records - are trivially copyable and name their extension points") {
        static_assert(std::is_trivially_copyable_v<importer_extension_desc>);
        static_assert(std::is_trivially_copyable_v<asset_type_extension_desc>);
        CHECK(importer_extension_desc::EXTENSION_POINT == core::string_id("assets.importer"));
        CHECK(asset_type_extension_desc::EXTENSION_POINT == core::string_id("assets.type"));
    }

    TEST_CASE("register_importer_extension - creates and registers the importer") {
        importer_registry importers = {};
        importer_extension_desc desc = {};
        desc.id = "conjure.test.text";
        desc.create = &create_test_importer;
        CONJURE_REQUIRE_OK(register_importer_extension(importers, desc));
        CHECK(importers.find("conjure.test.text") != nullptr);
        CONJURE_CHECK_ERROR(register_importer_extension(importers, desc), core::error_code::ALREADY_EXISTS);
    }

    TEST_CASE("register_importer_extension - rejects a missing factory, a null importer and an id mismatch") {
        importer_registry importers = {};
        importer_extension_desc noFactory = {};
        noFactory.id = "conjure.test.text";
        CONJURE_CHECK_ERROR(register_importer_extension(importers, noFactory), core::error_code::INVALID_ARGUMENT);

        importer_extension_desc nothing = {};
        nothing.id = "conjure.test.text";
        nothing.create = &create_nothing;
        CONJURE_CHECK_ERROR(register_importer_extension(importers, nothing), core::error_code::INVALID_ARGUMENT);

        importer_extension_desc mismatch = {};
        mismatch.id = "conjure.test.other-id";
        mismatch.create = &create_test_importer;
        CONJURE_CHECK_ERROR(register_importer_extension(importers, mismatch), core::error_code::INVALID_ARGUMENT);
        CHECK(importers.importers().empty());
    }

    TEST_CASE("register_asset_type_extension - creates the loader and registers the type with its flags") {
        asset_type_registry types = {};
        asset_type_extension_desc desc = {};
        desc.name = "conjure.test.asset";
        desc.version = 4;
        desc.flags = asset_type_flags::RELOAD_WITH_DEPENDENCIES;
        desc.createLoader = &create_test_loader;
        CONJURE_REQUIRE_OK(register_asset_type_extension(types, desc));
        const asset_type_desc *registered = types.find(asset_type_traits<test_asset>::id());
        REQUIRE(registered != nullptr);
        CHECK(registered->version == 4);
        CHECK(registered->flags == asset_type_flags::RELOAD_WITH_DEPENDENCIES);
        CHECK(registered->loader != nullptr);
        CONJURE_CHECK_ERROR(register_asset_type_extension(types, desc), asset_error_code::TYPE_DUPLICATE);
    }

    TEST_CASE("register_asset_type_extension - rejects a missing factory, a null loader and a name mismatch") {
        asset_type_registry types = {};
        asset_type_extension_desc noFactory = {};
        noFactory.name = "conjure.test.asset";
        CONJURE_CHECK_ERROR(register_asset_type_extension(types, noFactory), core::error_code::INVALID_ARGUMENT);

        asset_type_extension_desc nothing = {};
        nothing.name = "conjure.test.asset";
        nothing.createLoader = &create_no_loader;
        CONJURE_CHECK_ERROR(register_asset_type_extension(types, nothing), core::error_code::INVALID_ARGUMENT);

        asset_type_extension_desc mismatch = {};
        mismatch.name = "conjure.test.other";
        mismatch.createLoader = &create_test_loader;
        CONJURE_CHECK_ERROR(register_asset_type_extension(types, mismatch), core::error_code::INVALID_ARGUMENT);
        CHECK(types.size() == 0);
    }
}
