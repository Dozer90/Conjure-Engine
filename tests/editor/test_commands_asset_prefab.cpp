#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <filesystem>
#include <string>

// asset.* and prefab.* (docs/design/04_editor.md 4.4, R35).

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/commands_asset_prefab") {

    TEST_CASE("asset.import - copies a file into assets://, imports it and undo deletes it with the same uuid") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path source = editor_test::write_tiny_png(
          editor.project_directory() / "incoming" / "cell.png"
        );
        editor.clear_events();
        const core::json::document imported = editor.ok(
          "asset.import",
          core::json::object({{"source", source.generic_string()}, {"destination", "textures/cell.png"}})
        );
        const std::string uuid = std::string(imported.root()["uuid"].as_string());
        CHECK(core::uuid::parse(uuid).has_value());
        CHECK(imported.root()["path"].as_string() == "assets://textures/cell.png");
        CHECK(imported.root()["type"].as_string() == "conjure.renderer.texture");
        CHECK(imported.root()["status"].as_string() == "OK");
        CHECK(imported.root()["subAssets"].is_array());
        CHECK(editor.saw_event("asset.imported"));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "textures" / "cell.png"));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "textures" / "cell.png.meta"));
        const core::json::document info = editor.ok("asset.info", core::json::object({{"asset", uuid}}));
        CHECK(info.root()["path"].as_string() == "assets://textures/cell.png");
        CHECK(info.root()["dependencies"].is_array());
        const core::json::document byPath = editor.ok(
          "asset.info",
          core::json::object({{"asset", "textures/cell.png"}})
        );
        CHECK(byPath.root()["uuid"].as_string() == uuid);
        const editor_test::editor_snapshot withAsset = editor_test::capture_snapshot(editor);

        (void) editor.ok("history.undo");
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "textures" / "cell.png"));
        CHECK(editor.fail("asset.info", core::json::object({{"asset", uuid}})) == protocol_error_code::ASSET_NOT_FOUND);
        (void) editor.ok("history.redo");
        const editor_test::editor_snapshot redone = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(withAsset == redone, editor_test::describe_difference(withAsset, redone));
        CHECK(editor.ok("asset.info", core::json::object({{"asset", uuid}})).root()["uuid"].as_string() == uuid);
        CHECK(editor.fail(
          "asset.import",
          core::json::object({{"source", (editor.project_directory() / "nope.png").generic_string()}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
        CHECK(imported.root()["copied"].as_bool());
    }

    TEST_CASE("regression - asset.import of a file already inside the project records nothing to undo") {
        editor_test::headless_editor editor = {};
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "textures" / "here.png");
        const usize entries = editor.context().undo().size();
        const u64 revision = editor.context().revision();
        const core::json::document imported = editor.ok(
          "asset.import",
          core::json::object({{"source", "textures/here.png"}})
        );
        CHECK_FALSE(imported.root()["copied"].as_bool(true));
        CHECK(imported.root()["path"].as_string() == "assets://textures/here.png");
        CHECK(editor.context().revision() == revision + 1);
        CHECK(editor.context().undo().size() == entries);
        CHECK_FALSE(editor.context().undo().can_undo());
        // Importing again (a re-cook) is the same: the user's file never disappears through undo.
        (void) editor.ok("asset.import", core::json::object({{"source", "textures/here.png"}}));
        CHECK(editor.context().undo().size() == entries);
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "textures" / "here.png"));
        // A copied import still records its inverse.
        const std::filesystem::path outside = editor_test::write_tiny_png(
          editor.project_directory() / "incoming" / "far.png"
        );
        const core::json::document copied = editor.ok(
          "asset.import",
          core::json::object({{"source", outside.generic_string()}})
        );
        CHECK(copied.root()["copied"].as_bool());
        CHECK(editor.context().undo().size() == entries + 1);
    }

    TEST_CASE("asset.list - lists imported assets and filters by path and type") {
        editor_test::headless_editor editor = {};
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "a.png");
        (void) editor.ok("asset.import", core::json::object({{"source", "a.png"}}));
        (void) editor.ok("scene.save");
        const core::json::document all = editor.ok("asset.list");
        CHECK(all.root()["count"].as_i64() >= 2);
        const core::json::document textures = editor.ok(
          "asset.list",
          core::json::object({{"type", "conjure.renderer.texture"}})
        );
        REQUIRE(textures.root()["count"].as_i64() == 1);
        CHECK(textures.root()["assets"][0]["path"].as_string() == "assets://a.png");
        const core::json::document filtered = editor.ok("asset.list", core::json::object({{"filter", "scenes/"}}));
        CHECK(filtered.root()["count"].as_i64() == 1);
    }

    TEST_CASE("asset.create_material / asset.set - a material is written, edited by path and undone") {
        editor_test::headless_editor editor = {};
        const core::json::document created = editor.ok(
          "asset.create_material",
          core::json::object({
            {"path", "materials/cell"},
            {"baseColor", core::json::array({0.2f, 0.4f, 0.6f, 1.0f})},
            {"metallic", 0.0f},
            {"roughness", 0.5f},
          })
        );
        const std::string uuid = std::string(created.root()["uuid"].as_string());
        CHECK(created.root()["path"].as_string() == "assets://materials/cell.cmat");
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "materials" / "cell.cmat"));
        const std::string before = editor_test::read_text_file(
          editor.project_directory() / "assets" / "materials" / "cell.cmat"
        );
        const core::json::document materialJson = core::json::document::parse(before).value();
        REQUIRE(materialJson.root().is_object());
        // Pick the first numeric member of the material document as the edit target: the format belongs to
        // the renderer, the command only needs a path that exists.
        std::string key = "";
        for (const core::json::member entry : materialJson.root().members()) {
            if (entry.item.is_number()) {
                key = std::string(entry.key);
                break;
            }
        }
        REQUIRE_FALSE(key.empty());
        const core::json::document set = editor.ok(
          "asset.set",
          core::json::object({{"asset", uuid}, {"path", key}, {"value", 0.25}})
        );
        CHECK(set.root()["previous"].is_number());
        const std::string after = editor_test::read_text_file(
          editor.project_directory() / "assets" / "materials" / "cell.cmat"
        );
        CHECK(core::json::document::parse(after).value().root()[key].as_f64() == doctest::Approx(0.25));
        (void) editor.ok("history.undo");
        CHECK(
          core::json::document::parse(
            editor_test::read_text_file(editor.project_directory() / "assets" / "materials" / "cell.cmat")
          ).value() == materialJson
        );
        (void) editor.ok("history.undo");
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "materials" / "cell.cmat"));
        (void) editor.ok("history.redo");
        CHECK(editor.ok("asset.info", core::json::object({{"asset", uuid}})).root()["uuid"].as_string() == uuid);
        CHECK(editor.fail(
          "asset.create_material",
          core::json::object({{"path", "materials/cell"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "asset.set",
          core::json::object({{"asset", uuid}, {"path", "no.such.member"}, {"value", 1}})
        ) == protocol_error_code::PATH_NOT_FOUND);
    }

    TEST_CASE("asset.create_folder / asset.delete - folders and assets are removed and restored") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("asset.create_folder", core::json::object({{"path", "props"}}));
        CHECK(std::filesystem::is_directory(editor.project_directory() / "assets" / "props"));
        CHECK(editor.fail(
          "asset.create_folder",
          core::json::object({{"path", "props"}})
        ) == protocol_error_code::INVALID_ARGS);
        (void) editor.ok("history.undo");
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "props"));
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "del.png");
        const std::string uuid = std::string(editor.ok(
          "asset.import",
          core::json::object({{"source", "del.png"}})
        ).root()["uuid"].as_string());
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        (void) editor.ok("asset.delete", core::json::object({{"asset", uuid}}));
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "del.png"));
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot restored = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == restored, editor_test::describe_difference(before, restored));
        CHECK(editor.ok("asset.info", core::json::object({{"asset", uuid}})).root()["uuid"].as_string() == uuid);
        CHECK(editor.fail(
          "asset.delete",
          core::json::object({{"asset", "nothing/here.png"}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
    }

    TEST_CASE("asset.reimport - cooks again and answers the record") {
        editor_test::headless_editor editor = {};
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "re.png");
        const std::string uuid = std::string(editor.ok(
          "asset.import",
          core::json::object({{"source", "re.png"}})
        ).root()["uuid"].as_string());
        const core::json::document result = editor.ok("asset.reimport", core::json::object({{"asset", uuid}}));
        CHECK(result.root()["uuid"].as_string() == uuid);
        CHECK(result.root()["status"].as_string() == "OK");
    }

    TEST_CASE("prefab.* - create, instantiate, apply and unpack with undo at every step") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        const std::string child = editor.create_entity("Marker", root);
        (void) editor.ok(
          "component.add",
          core::json::object({
            {"entity", root},
            {"type", "sprite"},
            {"value", core::json::object({{"size", core::json::array({0.9f, 0.9f})}})},
          })
        );
        const editor_test::editor_snapshot plain = editor_test::capture_snapshot(editor);

        const core::json::document created = editor.ok(
          "prefab.create",
          core::json::object({{"entity", root}, {"path", "prefabs/cell"}})
        );
        const std::string uuid = std::string(created.root()["uuid"].as_string());
        CHECK(created.root()["path"].as_string() == "assets://prefabs/cell.prefab.json");
        CHECK(created.root()["guid"].as_string() == root);
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "prefabs" / "cell.prefab.json"));
        core::json::document info = editor.ok("entity.get", core::json::object({{"entity", root}}));
        CHECK(info.root()["components"].contains("conjure.scene.prefab_instance"));
        const editor_test::editor_snapshot marked = editor_test::capture_snapshot(editor);
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot unmarked = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(plain == unmarked, editor_test::describe_difference(plain, unmarked));
        (void) editor.ok("history.redo");
        const editor_test::editor_snapshot remarked = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(marked == remarked, editor_test::describe_difference(marked, remarked));

        const core::json::document instantiated = editor.ok(
          "prefab.instantiate",
          core::json::object({
            {"prefab", uuid},
            {"count", 3},
            {"name", "Copy"},
            {"transform", core::json::object({{"position", core::json::array({5.0f, 0.0f, 0.0f})}})},
          })
        );
        REQUIRE(instantiated.root()["guids"].size() == 3);
        CHECK(editor.scene().world().entity_count() == 8);
        const std::string copy = std::string(instantiated.root()["guids"][0].as_string());
        info = editor.ok("entity.get", core::json::object({{"entity", copy}}));
        CHECK(info.root()["name"].as_string() == "Copy");
        CHECK(info.root()["children"].size() == 1);
        CHECK(info.root()["components"].contains("conjure.scene.prefab_instance"));
        CHECK(
          info.root()["components"]["conjure.scene.local_transform"]["position"][0].as_f32() == doctest::Approx(5.0f)
        );
        const editor_test::editor_snapshot withCopies = editor_test::capture_snapshot(editor);
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 2);
        (void) editor.ok("history.redo");
        const editor_test::editor_snapshot copiesAgain = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(withCopies == copiesAgain, editor_test::describe_difference(withCopies, copiesAgain));
        (void) editor.ok("entity.get", core::json::object({{"entity", copy}}));

        // Apply: a change to the instance is written into the asset file.
        const std::string fileBefore = editor_test::read_text_file(
          editor.project_directory() / "assets" / "prefabs" / "cell.prefab.json"
        );
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", copy}, {"path", "sprite.size"}, {"value", core::json::array({2.0f, 2.0f})}})
        );
        const core::json::document applied = editor.ok("prefab.apply", core::json::object({{"entity", copy}}));
        CHECK(applied.root()["uuid"].as_string() == uuid);
        const std::string fileAfter = editor_test::read_text_file(
          editor.project_directory() / "assets" / "prefabs" / "cell.prefab.json"
        );
        CHECK(fileAfter != fileBefore);
        CHECK(fileAfter.find("2.0") != std::string::npos);
        (void) editor.ok("history.undo");
        CHECK(
          editor_test::read_text_file(
            editor.project_directory() / "assets" / "prefabs" / "cell.prefab.json"
          ) == fileBefore
        );
        CHECK(editor.fail(
          "prefab.apply",
          core::json::object({{"entity", editor.create_entity("Unlinked")}})
        ) == protocol_error_code::INVALID_ARGS);

        // Unpack: the markers go and come back.
        const editor_test::editor_snapshot packed = editor_test::capture_snapshot(editor);
        const core::json::document unpacked = editor.ok("prefab.unpack", core::json::object({{"entity", copy}}));
        CHECK(unpacked.root()["entities"].as_i64() == 2);
        info = editor.ok("entity.get", core::json::object({{"entity", copy}}));
        CHECK_FALSE(info.root()["components"].contains("conjure.scene.prefab_instance"));
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot repacked = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(packed == repacked, editor_test::describe_difference(packed, repacked));
        CHECK(editor.fail(
          "prefab.unpack",
          core::json::object({{"entity", editor.create_entity("Loose")}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "prefab.instantiate",
          core::json::object({{"prefab", "prefabs/missing"}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
        CHECK(editor.fail(
          "prefab.create",
          core::json::object({{"entity", root}, {"path", "prefabs/cell"}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("regression - prefab.instantiate with a guid listed twice fails up front and leaves nothing behind") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        const std::string uuid = std::string(editor.ok(
          "prefab.create",
          core::json::object({{"entity", root}, {"path", "prefabs/cell"}})
        ).root()["uuid"].as_string());
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const usize entries = editor.context().undo().size();
        const std::string twice = core::uuid::generate().to_string();
        CHECK(editor.fail(
          "prefab.instantiate",
          core::json::object({{"prefab", uuid}, {"count", 2}, {"guids", core::json::array({twice, twice})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.scene().world().entity_count() == 1);
        CHECK(editor.context().undo().size() == entries);
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        // A guid that is already live is refused the same way.
        CHECK(editor.fail(
          "prefab.instantiate",
          core::json::object({{"prefab", uuid}, {"guids", core::json::array({root})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.scene().world().entity_count() == 1);
        // And distinct fresh guids are honoured.
        const std::string wanted = core::uuid::generate().to_string();
        const core::json::document made = editor.ok(
          "prefab.instantiate",
          core::json::object({{"prefab", uuid}, {"guids", core::json::array({wanted})}})
        );
        CHECK(made.root()["guids"][0].as_string() == wanted);
    }

    TEST_CASE("regression - a prefab.create that fails after writing its file leaves no file behind") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        // The chosen uuid already belongs to another asset: add_file refuses the duplicate after the file and
        // its .meta were written, and both must be removed again.
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "taken.png");
        const std::string taken = std::string(editor.ok(
          "asset.import",
          core::json::object({{"source", "taken.png"}})
        ).root()["uuid"].as_string());
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const protocol_response response = editor.send(
          "prefab.create",
          core::json::object({{"entity", root}, {"path", "prefabs/dup"}, {"uuid", taken}})
        );
        CHECK_FALSE(response.ok);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "prefabs" / "dup.prefab.json"));
        CHECK_FALSE(
          std::filesystem::exists(editor.project_directory() / "assets" / "prefabs" / "dup.prefab.json.meta")
        );
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        CHECK(editor.ok("asset.info", core::json::object({{"asset", taken}})).root()["path"].as_string() ==
          "assets://taken.png");
    }
}
