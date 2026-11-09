#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_meta.h"
#include "conjure/core/json/document.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    constexpr std::string_view FULL_META = R"({
  "schema": 1,
  "uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01",
  "importer": "conjure.renderer.gltf",
  "importerVersion": 3,
  "settings": { "scale": 1.0, "generateTangents": true },
  "subAssets": {
    "mesh/Body": "a1b2c3d4-0000-4000-8000-000000000001",
    "material/Skin": "a1b2c3d4-0000-4000-8000-000000000002"
  },
  "custom": { "note": "kept" }
})";
}

TEST_SUITE("assets/asset_meta") {

    TEST_CASE("asset_meta - parse and serialise round trip") {
        core::result<asset_meta> parsed = parse_asset_meta(FULL_META);
        CONJURE_REQUIRE_OK(parsed);
        const asset_meta &meta = parsed.value();
        CHECK(meta.id.to_string() == "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01");
        CHECK(meta.importerId == "conjure.renderer.gltf");
        CHECK(meta.importerVersion == 3);
        CHECK(meta.settings.root()["scale"].as_f64() == 1.0);
        CHECK(meta.settings.root()["generateTangents"].as_bool());
        REQUIRE(meta.subAssets.size() == 2);
        CHECK(meta.subAssets[0].subPath == "material/Skin");
        CHECK(meta.subAssets[1].subPath == "mesh/Body");
        REQUIRE(meta.find_sub_asset("mesh/Body") != nullptr);
        CHECK(meta.find_sub_asset("mesh/Body")->id.to_string() == "a1b2c3d4-0000-4000-8000-000000000001");
        CHECK(meta.find_sub_asset("mesh/Nope") == nullptr);

        const std::string text = serialize_asset_meta(meta);
        core::result<asset_meta> again = parse_asset_meta(text);
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value().id == meta.id);
        CHECK(again.value().importerId == meta.importerId);
        CHECK(again.value().importerVersion == meta.importerVersion);
        CHECK(again.value().settings == meta.settings);
        CHECK(again.value().subAssets == meta.subAssets);
        CHECK(serialize_asset_meta(again.value()) == text);
        CHECK(text.back() == '\n');
    }

    TEST_CASE("asset_meta - optional fields may be missing") {
        core::result<asset_meta> parsed = parse_asset_meta(R"({"uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01"})");
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().importerId.empty());
        CHECK(parsed.value().importerVersion == 0);
        CHECK(parsed.value().settings.root().is_object());
        CHECK(parsed.value().settings.root().size() == 0);
        CHECK(parsed.value().subAssets.empty());
        CHECK(parsed.value().unknown.root().size() == 0);
    }

    TEST_CASE("asset_meta - corrupt content is META_INVALID") {
        CONJURE_CHECK_ERROR(parse_asset_meta("not json"), asset_error_code::META_INVALID);
        CONJURE_CHECK_ERROR(parse_asset_meta("[]"), asset_error_code::META_INVALID);
        CONJURE_CHECK_ERROR(parse_asset_meta("{}"), asset_error_code::META_INVALID);
        CONJURE_CHECK_ERROR(parse_asset_meta(R"({"uuid": "nope"})"), asset_error_code::META_INVALID);
        CONJURE_CHECK_ERROR(
          parse_asset_meta(R"({"uuid": "00000000-0000-0000-0000-000000000000"})"),
          asset_error_code::META_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_asset_meta(R"({"uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01", "schema": 7})"),
          asset_error_code::META_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_asset_meta(R"({"uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01", "importer": 3})"),
          asset_error_code::META_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_asset_meta(R"({"uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01", "settings": []})"),
          asset_error_code::META_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_asset_meta(R"({"uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01", "subAssets": {"a": 1}})"),
          asset_error_code::META_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_asset_meta(R"({"uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01", "subAssets": {"a": "bad"}})"),
          asset_error_code::META_INVALID
        );
    }

    TEST_CASE("read_asset_meta - a corrupt file is reported and left untouched") {
        core::vfs::memory_file_system fileSystem = {};
        CONJURE_REQUIRE_OK(fileSystem.write_file("a.png.meta", bytes_of("{ corrupt")));
        const u64 writes = fileSystem.write_counter();
        CONJURE_CHECK_ERROR(read_asset_meta(fileSystem, "a.png.meta"), asset_error_code::META_INVALID);
        CHECK(fileSystem.write_counter() == writes);
        core::result<std::string> content = fileSystem.read_text_file("a.png.meta");
        CONJURE_REQUIRE_OK(content);
        CHECK(content.value() == "{ corrupt");
        CONJURE_CHECK_ERROR(read_asset_meta(fileSystem, "missing.meta"), asset_error_code::NOT_FOUND);
    }

    TEST_CASE("asset_meta - unknown keys survive a read/write round trip") {
        core::result<asset_meta> parsed = parse_asset_meta(FULL_META);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().unknown.root()["custom"]["note"].as_string() == "kept");
        const std::string text = serialize_asset_meta(parsed.value());
        CHECK(text.find("\"custom\"") != std::string::npos);
        CHECK(text.find("\"kept\"") != std::string::npos);
    }

    TEST_CASE("write_asset_meta - sub-assets are sorted by sub path on write") {
        core::vfs::memory_file_system fileSystem = {};
        asset_meta meta = {};
        meta.id = test_id(1);
        meta.importerId = "conjure.test.text";
        meta.subAssets.push_back(sub_asset_entry{"zeta", test_id(2)});
        meta.subAssets.push_back(sub_asset_entry{"alpha", test_id(3)});
        meta.subAssets.push_back(sub_asset_entry{"mid", test_id(4)});
        CONJURE_REQUIRE_OK(write_asset_meta(fileSystem, "dir/x.ta.meta", meta));

        core::result<std::string> text = fileSystem.read_text_file("dir/x.ta.meta");
        CONJURE_REQUIRE_OK(text);
        const usize alpha = text.value().find("\"alpha\"");
        const usize mid = text.value().find("\"mid\"");
        const usize zeta = text.value().find("\"zeta\"");
        CHECK(alpha < mid);
        CHECK(mid < zeta);
        CHECK(text.value().find("\"schema\": 1") != std::string::npos);

        core::result<asset_meta> back = read_asset_meta(fileSystem, "dir/x.ta.meta");
        CONJURE_REQUIRE_OK(back);
        CHECK(back.value().subAssets[0].subPath == "alpha");
        CHECK(back.value().subAssets[2].id == test_id(2));
    }

    TEST_CASE("write_asset_meta - a read-only file system is IO_ERROR") {
        core::vfs::memory_file_system fileSystem = {};
        fileSystem.set_read_only(true);
        asset_meta meta = {};
        meta.id = test_id(1);
        CONJURE_CHECK_ERROR(write_asset_meta(fileSystem, "x.meta", meta), asset_error_code::IO_ERROR);
    }

    TEST_CASE("canonical_settings_hash - is independent of key order and whitespace") {
        const core::json::document a = core::json::object({{"scale", 1.0}, {"generateTangents", true}});
        const core::json::document b = core::json::object({{"generateTangents", true}, {"scale", 1.0}});
        core::result<core::json::document> c = core::json::document::parse(
          "{ \"scale\" : 1.0 ,\n \"generateTangents\" : true }"
        );
        CONJURE_REQUIRE_OK(c);
        CHECK(canonical_settings_hash(a.root()) == canonical_settings_hash(b.root()));
        CHECK(canonical_settings_hash(a.root()) == canonical_settings_hash(c.value().root()));

        const core::json::document d = core::json::object({{"scale", 2.0}, {"generateTangents", true}});
        CHECK(canonical_settings_hash(a.root()) != canonical_settings_hash(d.root()));

        const core::json::document empty = core::json::document::object();
        CHECK(canonical_settings_hash(core::json::value()) == canonical_settings_hash(empty.root()));
    }

    TEST_CASE("meta_path_of - appends .meta and is_meta_path recognises it") {
        CHECK(meta_path_of("assets/a.png") == "assets/a.png.meta");
        CHECK(is_meta_path("assets/a.png.meta"));
        CHECK_FALSE(is_meta_path("assets/a.png"));
        CHECK_FALSE(is_meta_path("meta"));
    }
}
