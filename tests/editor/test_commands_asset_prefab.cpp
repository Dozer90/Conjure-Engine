#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/testing/log_capture.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// asset.* and prefab.* (docs/design/04_editor.md 4.4, R35).

using namespace conjure;
using namespace conjure::editor;
using editor_test::TEST_SCENE;
using editor_test::test_scene_file;

namespace {
    // One triangle with an embedded buffer: three u16 indices, then three POSITION vectors.
    constexpr std::string_view TRIANGLE_GLTF = R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0, "name": "triangle"}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 1}, "indices": 0}]}],
  "buffers": [{
    "uri": "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA=",
    "byteLength": 44
  }],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 6, "target": 34963},
    {"buffer": 0, "byteOffset": 8, "byteLength": 36, "target": 34962}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5123, "count": 3, "type": "SCALAR", "max": [2], "min": [0]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3", "max": [1, 1, 0], "min": [0, 0, 0]}
  ]
})";

    void write_file(const std::filesystem::path &path, std::string_view text) {
        std::error_code ignored = {};
        std::filesystem::create_directories(path.parent_path(), ignored);
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        REQUIRE(output.is_open());
        output << text;
    }

    /// <summary>
    /// The names directly under the project root, to show that a refused command created nothing there.
    /// </summary>
    std::vector<std::string> project_root_entries(const std::filesystem::path &root) {
        std::vector<std::string> names = {};
        std::error_code ignored = {};
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root, ignored)) {
            names.push_back(entry.path().filename().generic_string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    /// <summary>
    /// Holds the asset manager inside a frame (at its own frame index) for the scope's lifetime. The editor
    /// dispatches commands inside a frame, where asset_manager::load_blocking logs its once-per-frame hitch WARN;
    /// the test harness dispatches between frames, where it does not.
    /// </summary>
    class asset_frame_scope {
        private:
            assets::asset_manager *mManager = nullptr;

        public:
            explicit asset_frame_scope(assets::asset_manager &manager) :
                mManager(&manager) {
                mManager->begin_frame(mManager->frame_index());
            }

            ~asset_frame_scope() {
                mManager->end_frame();
            }

            asset_frame_scope(const asset_frame_scope &) = delete;
            asset_frame_scope &operator=(const asset_frame_scope &) = delete;
    };

    /// <summary>
    /// The first component of the entity's sprite size, read through component.get.
    /// </summary>
    f32 sprite_width(editor_test::headless_editor &editor, const std::string &entity) {
        const core::json::document size = editor.ok(
          "component.get",
          core::json::object({{"entity", entity}, {"path", "sprite.size"}})
        );
        return size.root()["value"][0].as_f32();
    }
}

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
        ) == protocol_error_code::PATH_NOT_FOUND);
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
        // The catalogue's file snapshot cannot see an empty folder (asset_create_folder.json and
        // asset_delete_folder.json opt out of its change check), so the folder round trip is checked here.
        (void) editor.ok("history.redo");
        CHECK(std::filesystem::is_directory(editor.project_directory() / "assets" / "props"));
        const core::json::document deleted = editor.ok("asset.delete", core::json::object({{"asset", "props"}}));
        CHECK(deleted.root()["folder"].as_bool());
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "props"));
        (void) editor.ok("history.undo");
        CHECK(std::filesystem::is_directory(editor.project_directory() / "assets" / "props"));
        (void) editor.ok("history.redo");
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

    TEST_CASE("regression - code sweep F137 asset.import makes a glTF a render model, collision only when asked") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path models = editor.project_directory() / "assets" / "models";
        write_file(models / "crate.gltf", TRIANGLE_GLTF);
        write_file(models / "crate_collision.gltf", TRIANGLE_GLTF);

        // By extension a glTF is a render model, whatever else can read it.
        const core::json::document model = editor.ok(
          "asset.import",
          core::json::object({{"source", "models/crate.gltf"}})
        );
        CHECK(model.root()["type"].as_string() == "conjure.renderer.model");
        CHECK(model.root()["importer"].as_string() == "conjure.renderer.gltf");
        CHECK(model.root()["status"].as_string() == "OK");
        CHECK(model.root()["subAssets"].size() >= 1);

        // "Import as": the importer is named, so the same format becomes collision geometry.
        const core::json::document collision = editor.ok(
          "asset.import",
          core::json::object({
            {"source", "models/crate_collision.gltf"},
            {"importer", "conjure.physics.collision_mesh_importer"},
            {"options", core::json::object({{"weld_distance", 0.01}})},
          })
        );
        const std::string collisionUuid = std::string(collision.root()["uuid"].as_string());
        CHECK(collision.root()["type"].as_string() == "conjure.physics.collision_mesh");
        CHECK(collision.root()["importer"].as_string() == "conjure.physics.collision_mesh_importer");
        CHECK(collision.root()["status"].as_string() == "OK");
        // The options are settings of the importer that ran, on top of its defaults.
        const core::json::document info = editor.ok("asset.info", core::json::object({{"asset", collisionUuid}}));
        CHECK(info.root()["settings"]["weld_distance"].as_f64(0.0) == doctest::Approx(0.01));

        // "Reimport as" switches an asset back and keeps its uuid.
        const core::json::document switched = editor.ok(
          "asset.reimport",
          core::json::object({{"asset", collisionUuid}, {"importer", "conjure.renderer.gltf"}})
        );
        CHECK(switched.root()["uuid"].as_string() == collisionUuid);
        CHECK(switched.root()["type"].as_string() == "conjure.renderer.model");
        CHECK(switched.root()["importer"].as_string() == "conjure.renderer.gltf");
        // A plain reimport afterwards keeps the importer the .meta now names.
        const core::json::document again = editor.ok("asset.reimport", core::json::object({{"asset", collisionUuid}}));
        CHECK(again.root()["importer"].as_string() == "conjure.renderer.gltf");

        // An importer that is not registered is the caller's mistake and changes nothing.
        CHECK(editor.fail(
          "asset.reimport",
          core::json::object({{"asset", collisionUuid}, {"importer", "conjure.nope"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "asset.import",
          core::json::object({{"source", "models/crate.gltf"}, {"importer", 7}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.ok(
          "asset.info",
          core::json::object({{"asset", collisionUuid}})
        ).root()["importer"].as_string() == "conjure.renderer.gltf");
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

    TEST_CASE("regression - code sweep F736 prefab commands use a resident prefab without a blocking load") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        (void) editor.ok(
          "component.add",
          core::json::object({
            {"entity", root},
            {"type", "sprite"},
            {"value", core::json::object({{"size", core::json::array({0.9f, 0.9f})}})},
          })
        );
        const std::string uuid = std::string(editor.ok(
          "prefab.create",
          core::json::object({{"entity", root}, {"path", "prefabs/cell"}})
        ).root()["uuid"].as_string());
        const std::optional<core::uuid> id = core::uuid::parse(uuid);
        REQUIRE(id.has_value());
        assets::asset_manager *manager = editor.context().assets();
        REQUIRE(manager != nullptr);

        SUBCASE("prefab.instantiate and prefab.apply of a resident prefab do not block") {
            // prefab.create loaded the prefab, and nothing unloads it before collect() runs.
            REQUIRE(manager->state_of(*id) == assets::asset_state::LOADED);
            const asset_frame_scope frame(*manager);
            const testing::log_capture logs = {};
            const core::json::document made = editor.ok("prefab.instantiate", core::json::object({{"prefab", uuid}}));
            REQUIRE(made.root()["guids"].size() == 1);
            const std::string copy = std::string(made.root()["guids"][0].as_string());
            (void) editor.ok(
              "component.set",
              core::json::object({
                {"entity", copy},
                {"path", "sprite.size"},
                {"value", core::json::array({2.0f, 2.0f})},
              })
            );
            (void) editor.ok("prefab.apply", core::json::object({{"entity", copy}}));
            CHECK_FALSE(logs.contains(core::log_level::WARN, "load_blocking"));
        }

        SUBCASE("a prefab reloading after prefab.apply is waited for, so the next instance holds the applied state") {
            const asset_frame_scope frame(*manager);
            const testing::log_capture logs = {};
            (void) editor.ok(
              "component.set",
              core::json::object({
                {"entity", root},
                {"path", "sprite.size"},
                {"value", core::json::array({2.0f, 2.0f})},
              })
            );
            (void) editor.ok("prefab.apply", core::json::object({{"entity", root}}));
            REQUIRE(manager->state_of(*id) == assets::asset_state::RELOADING);
            CHECK_FALSE(logs.contains(core::log_level::WARN, "load_blocking"));
            const core::json::document made = editor.ok("prefab.instantiate", core::json::object({{"prefab", uuid}}));
            REQUIRE(made.root()["guids"].size() == 1);
            CHECK(sprite_width(editor, std::string(made.root()["guids"][0].as_string())) == doctest::Approx(2.0f));
            CHECK(manager->state_of(*id) == assets::asset_state::LOADED);
            CHECK(logs.contains(core::log_level::WARN, "load_blocking"));
        }

        SUBCASE("a prefab that is no longer resident is loaded synchronously and instantiated") {
            // No command or instance holds a handle on the prefab, so collect() unloads it.
            (void) manager->collect();
            REQUIRE(manager->state_of(*id) == assets::asset_state::UNLOADED);
            const asset_frame_scope frame(*manager);
            const testing::log_capture logs = {};
            const core::json::document made = editor.ok("prefab.instantiate", core::json::object({{"prefab", uuid}}));
            REQUIRE(made.root()["guids"].size() == 1);
            CHECK(sprite_width(editor, std::string(made.root()["guids"][0].as_string())) == doctest::Approx(0.9f));
            CHECK(manager->state_of(*id) == assets::asset_state::LOADED);
            CHECK(logs.contains(core::log_level::WARN, "load_blocking"));
        }

        SUBCASE("a resident prefab is refused once the asset manager is shut down") {
            // shutdown() leaves the LOADED slot LOADED; the command still goes through load_blocking, which
            // refuses with SHUTTING_DOWN (an internal error on the protocol) instead of binding the payload.
            manager->shutdown();
            REQUIRE(manager->state_of(*id) == assets::asset_state::LOADED);
            CHECK(
              editor.fail("prefab.instantiate", core::json::object({{"prefab", uuid}})) ==
              protocol_error_code::INTERNAL
            );
        }
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

    TEST_CASE("regression - F648 prefab.instantiate with a name containing '/' is INVALID_ARGS and instantiates none") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        const std::string uuid = std::string(editor.ok(
          "prefab.create",
          core::json::object({{"entity", root}, {"path", "prefabs/cell"}})
        ).root()["uuid"].as_string());
        const usize entries = editor.context().undo().size();
        CHECK(editor.fail(
          "prefab.instantiate",
          core::json::object({{"prefab", uuid}, {"name", "Cells/One"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "prefab.instantiate",
          core::json::object({{"prefab", uuid}, {"name", ""}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.scene().world().entity_count() == 1);
        CHECK(editor.context().undo().size() == entries);
        const core::json::document made = editor.ok(
          "prefab.instantiate",
          core::json::object({{"prefab", uuid}, {"name", "Cell Two"}})
        );
        CHECK(made.root()["guids"].size() == 1);
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

    TEST_CASE("regression - asset paths with '..' are refused and write nothing") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path project = editor.project_directory();
        const std::string projectJson = editor_test::read_text_file(project / "project.json");
        const std::filesystem::path outside = editor_test::write_tiny_png(project / "incoming" / "far.png");
        const std::vector<std::string> rootBefore = project_root_entries(project);
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const usize entries = editor.context().undo().size();

        CHECK(editor.fail(
          "asset.restore",
          core::json::object({{"path", "assets://../project.json"}, {"contentBase64", "e30="}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "asset.restore",
          core::json::object({{"path", "assets://a/../../x.json"}, {"contentBase64", "e30="}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail("asset.import", core::json::object({{"source", "../project.json"}})) ==
          protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "asset.import",
          core::json::object({{"source", outside.generic_string()}, {"destination", "../x.png"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "asset.import",
          core::json::object({{"source", outside.generic_string()}, {"destination", "assets://..\\x.png"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail("asset.create_folder", core::json::object({{"path", "../escaped"}})) ==
          protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail("asset.create_material", core::json::object({{"path", "../escaped"}})) ==
          protocol_error_code::INVALID_ARGS);

        CHECK(editor_test::read_text_file(project / "project.json") == projectJson);
        CHECK(project_root_entries(project) == rootBefore);
        CHECK(editor.context().undo().size() == entries);
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        // Dot segments that stay inside the mount are folded away, not refused.
        const core::json::document folded = editor.ok(
          "asset.create_folder",
          core::json::object({{"path", "props/../kept"}})
        );
        CHECK(folded.root()["path"].as_string() == "assets://kept");
        CHECK(std::filesystem::is_directory(project / "assets" / "kept"));
    }

    TEST_CASE("regression - prefab.create refuses a path that leaves the assets root and writes nothing") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        const std::filesystem::path project = editor.project_directory();
        const std::vector<std::string> rootBefore = project_root_entries(project);
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const std::array<std::string_view, 4> escaping = {"../outside", "a/../../b", "assets://..\\outside", "./x"};
        for (const std::string_view path : escaping) {
            CAPTURE(path);
            CHECK(editor.fail(
              "prefab.create",
              core::json::object({{"entity", root}, {"path", path}})
            ) == protocol_error_code::INVALID_ARGS);
        }
        CHECK_FALSE(std::filesystem::exists(project / "outside.prefab.json"));
        CHECK_FALSE(std::filesystem::exists(project / "outside.prefab.json.meta"));
        CHECK_FALSE(std::filesystem::exists(project / "b.prefab.json"));
        CHECK(project_root_entries(project) == rootBefore);
        CHECK(editor.fail(
          "asset.info",
          core::json::object({{"asset", "assets://../outside.prefab.json"}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
    }

    TEST_CASE("regression - asset.create_material refuses a baseColor component outside the f32 range") {
        editor_test::headless_editor editor = {};
        const core::result<core::json::document> args = core::json::document::parse(
          R"({"path":"materials/huge","baseColor":[1e39,1,1]})"
        );
        CONJURE_REQUIRE_OK(args);
        CHECK(editor.fail("asset.create_material", core::json::document::from_value(args.value().root())) ==
          protocol_error_code::INVALID_ARGS);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "materials" / "huge.cmat"));
        const core::result<core::json::document> emissive = core::json::document::parse(
          R"({"path":"materials/glow","emissive":[0,1e39,0]})"
        );
        CONJURE_REQUIRE_OK(emissive);
        CHECK(editor.fail("asset.create_material", core::json::document::from_value(emissive.value().root())) ==
          protocol_error_code::INVALID_ARGS);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "materials" / "glow.cmat"));
    }

    TEST_CASE("regression - asset.import of a directory is refused and writes nothing") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path directory = editor.project_directory() / "incoming" / "folder";
        write_file(directory / "inside.txt", "not imported");
        const usize records = editor.ok("asset.list").root()["count"].as_u64();
        const protocol_response response = editor.send(
          "asset.import",
          core::json::object({{"source", directory.generic_string()}})
        );
        REQUIRE_FALSE(response.ok);
        // The directory exists and cannot be read as a file: IO_ERROR, not a "not found" code (code sweep F656).
        CHECK(response.errorCode == protocol_error_code::IO_ERROR);
        CHECK(response.errorMessage.find("not a regular file") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "folder"));
        CHECK(editor.ok("asset.list").root()["count"].as_u64() == records);
        // A source that does not exist at all says so rather than calling it "not a regular file".
        const protocol_response missing = editor.send(
          "asset.import",
          core::json::object({{"source", (directory / "absent.png").generic_string()}})
        );
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(missing.errorMessage.find("no such file") != std::string::npos);
    }

    TEST_CASE("regression - code sweep F187 asset.import reads a relative source against the project root") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path root = editor.context().project()->root_directory();
        (void) editor_test::write_tiny_png(root / "incoming" / "relative.png");
        // Not an asset path (nothing is at assets://incoming/relative.png), so a file outside the assets root:
        // the one under the project root, never one in the editor's working directory.
        const core::json::document imported = editor.ok(
          "asset.import",
          core::json::object({{"source", "incoming/relative.png"}})
        );
        CHECK(imported.root()["path"].as_string() == "assets://relative.png");
        CHECK(imported.root()["copied"].as_bool(false));
        CHECK(std::filesystem::is_regular_file(root / "assets" / "relative.png"));

        // A relative source that names nothing is looked for in the project, and the answer says where.
        const protocol_response missing = editor.send(
          "asset.import",
          core::json::object({{"source", "incoming/none.png"}})
        );
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        const std::string expected = (root / "incoming" / "none.png").generic_string();
        CHECK_MESSAGE(missing.errorMessage.find(expected) != std::string::npos, missing.errorMessage);
        CHECK_FALSE(std::filesystem::exists(root / "assets" / "none.png"));

        // An asset path still comes first: a source that names an asset is that asset, not a file to copy.
        const core::json::document again = editor.ok(
          "asset.import",
          core::json::object({{"source", "relative.png"}})
        );
        CHECK(again.root()["uuid"].as_string() == imported.root()["uuid"].as_string());
        CHECK_FALSE(again.root()["copied"].as_bool(true));
    }

    TEST_CASE("asset.import - without a project there is nothing to import into and no file is read") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        const protocol_error_code refused = editor.fail(
          "asset.import",
          core::json::object({{"source", "conjure_f187_no_such_folder/none.png"}})
        );
        CHECK(refused == protocol_error_code::PROJECT_NOT_OPEN);
    }

    TEST_CASE("regression - asset.import of an absolute path inside the assets directory registers the file in place") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path file = editor_test::write_tiny_png(
          editor.project_directory() / "assets" / "data" / "notes.png"
        );
        const usize entries = editor.context().undo().size();
        const core::json::document imported = editor.ok(
          "asset.import",
          core::json::object({{"source", file.generic_string()}})
        );
        CHECK(imported.root()["path"].as_string() == "assets://data/notes.png");
        CHECK_FALSE(imported.root()["copied"].as_bool(true));
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "notes.png"));
        CHECK(editor.context().undo().size() == entries);
        // A spelling with redundant segments names the same file.
        const std::filesystem::path redundant = editor.project_directory() / "assets" / "data" / ".." / "data" /
          "notes.png";
        const core::json::document again = editor.ok(
          "asset.import",
          core::json::object({{"source", redundant.generic_string()}})
        );
        CHECK(again.root()["uuid"].as_string() == imported.root()["uuid"].as_string());
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "notes.png"));
        // An explicit destination still asks for a copy.
        const core::json::document copied = editor.ok(
          "asset.import",
          core::json::object({{"source", file.generic_string()}, {"destination", "copies/notes.png"}})
        );
        CHECK(copied.root()["path"].as_string() == "assets://copies/notes.png");
        CHECK(copied.root()["copied"].as_bool(false));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "copies" / "notes.png"));
    }

    TEST_CASE("regression - code sweep F656 asset.delete refuses the open scene unless closeOpenScene is passed") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        REQUIRE(editor.context().scene_path() == TEST_SCENE);
        REQUIRE(std::filesystem::is_regular_file(test_scene_file(editor)));
        const std::string uuid = std::string(editor.ok(
          "asset.info",
          core::json::object({{"asset", TEST_SCENE}})
        ).root()["uuid"].as_string());

        // By path, by uuid and with an explicit false: each one names the open scene and is refused.
        const protocol_response refused = editor.send("asset.delete", core::json::object({{"asset", TEST_SCENE}}));
        REQUIRE_FALSE(refused.ok);
        CHECK(refused.errorCode == protocol_error_code::INVALID_ARGS);
        CHECK(refused.errorMessage.find("closeOpenScene") != std::string::npos);
        CHECK(refused.errorMessage.find(TEST_SCENE) != std::string::npos);
        CHECK(editor.fail("asset.delete", core::json::object({{"asset", uuid}})) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "asset.delete",
          core::json::object({{"asset", "scenes/test.scene.json"}, {"closeOpenScene", false}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "asset.delete",
          core::json::object({{"asset", uuid}, {"closeOpenScene", "yes"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().has_scene());
        CHECK(editor.context().scene_path() == TEST_SCENE);
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));

        // With the flag the file goes and the scene is closed, unsaved changes included.
        (void) editor.create_entity("Unsaved");
        REQUIRE(editor.context().scene_dirty());
        const core::json::document deleted = editor.ok(
          "asset.delete",
          core::json::object({{"asset", uuid}, {"closeOpenScene", true}})
        );
        CHECK(deleted.root()["uuid"].as_string() == uuid);
        CHECK(deleted.root()["path"].as_string() == TEST_SCENE);
        CHECK(deleted.root()["closedScene"].as_bool());
        CHECK_FALSE(deleted.root()["startupScene"].as_bool(true));
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        CHECK_FALSE(editor.context().has_scene());
        CHECK(editor.context().scene_path().empty());
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.fail("scene.info") == protocol_error_code::SCENE_NOT_OPEN);
        CHECK(editor.fail("asset.info", core::json::object({{"asset", uuid}})) == protocol_error_code::ASSET_NOT_FOUND);

        // Closing cleared the history, so the delete is its only entry: undo restores the file with its uuid and
        // leaves the scene closed; the restored file opens again.
        REQUIRE(editor.context().undo().size() == 1);
        (void) editor.ok("history.undo");
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.ok("asset.info", core::json::object({{"asset", TEST_SCENE}})).root()["uuid"].as_string() == uuid);
        CHECK_FALSE(editor.context().has_scene());
        const core::json::document reopened = editor.ok("scene.open", core::json::object({{"path", "scenes/test"}}));
        CHECK(reopened.root()["entityCount"].as_i64() == 1);
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
    }

    TEST_CASE("asset.delete - closeOpenScene changes nothing for an asset that is not the open scene") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        // The open scene moves to another file, so scenes/test is a scene file like any other asset.
        (void) editor.ok("scene.save_as", core::json::object({{"path", "scenes/other"}}));
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "plain.png");
        (void) editor.ok("asset.import", core::json::object({{"source", "plain.png"}}));
        const usize entries = editor.context().undo().size();

        const core::json::document scene = editor.ok("asset.delete", core::json::object({{"asset", TEST_SCENE}}));
        CHECK_FALSE(scene.root()["closedScene"].as_bool(true));
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        const core::json::document png = editor.ok(
          "asset.delete",
          core::json::object({{"asset", "plain.png"}, {"closeOpenScene", true}})
        );
        CHECK_FALSE(png.root()["closedScene"].as_bool(true));
        CHECK_FALSE(png.root()["startupScene"].as_bool(true));
        CHECK(editor.context().has_scene());
        CHECK(editor.context().scene_path() == "assets://scenes/other.scene.json");
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
        // Nothing was closed, so nothing cleared the history: both deletes sit on top of what was there.
        CHECK(editor.context().undo().size() == entries + 2);
        (void) editor.ok("history.undo");
        (void) editor.ok("history.undo");
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));

        // An unsaved scene has no file behind it, so no asset is "the open scene".
        (void) editor.ok("scene.new", core::json::object({{"name", "fresh"}}));
        REQUIRE(editor.context().scene_path().empty());
        const core::json::document other = editor.ok(
          "asset.delete",
          core::json::object({{"asset", "scenes/other.scene.json"}})
        );
        CHECK_FALSE(other.root()["closedScene"].as_bool(true));
        CHECK(editor.context().has_scene());
    }

    TEST_CASE("regression - code sweep F656 asset.delete with closeOpenScene is refused in a batch, a transaction "
      "and during play") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const core::json::document request = core::json::object({{"asset", TEST_SCENE}, {"closeOpenScene", true}});

        // Closing clears the undo history a batch is collecting into.
        const protocol_response batched = editor.send(
          "editor.batch",
          core::json::object({
            {"commands", core::json::array({core::json::object({{"cmd", "asset.delete"}, {"args", request}})})},
          })
        );
        REQUIRE_FALSE(batched.ok);
        CHECK(batched.errorCode == protocol_error_code::INVALID_ARGS);
        CHECK(batched.errorDetails.root()["failedIndex"].as_i64() == 0);
        CHECK(batched.errorMessage.find("batch") != std::string::npos);

        // And the one an open transaction is recording into.
        (void) editor.ok("editor.begin_transaction", core::json::object({{"label", "Open"}}));
        (void) editor.create_entity("In transaction");
        CHECK(editor.fail("asset.delete", core::json::document::from_value(request.root())) ==
          protocol_error_code::INVALID_ARGS);
        REQUIRE(editor.context().undo().has_open_transaction());
        (void) editor.ok("editor.end_transaction");
        CHECK(editor.context().undo().size() == 2);

        // scene.close is an editing-state command, and so is the delete that needs it.
        (void) editor.ok("play.start");
        CHECK(editor.fail("asset.delete", core::json::object({{"asset", TEST_SCENE}})) ==
          protocol_error_code::INVALID_ARGS);
        const protocol_response playing = editor.send("asset.delete", core::json::document::from_value(request.root()));
        REQUIRE_FALSE(playing.ok);
        CHECK(playing.errorCode == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(playing.errorDetails.root()["state"].as_string() == "playing");
        (void) editor.ok("play.stop");

        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().has_scene());
        CHECK(editor.context().scene_path() == TEST_SCENE);
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
        // Outside all three the same request goes through.
        CHECK(editor.ok(
          "asset.delete",
          core::json::document::from_value(request.root())
        ).root()["closedScene"].as_bool());
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
    }

    TEST_CASE("regression - code sweep F656 an undo that would delete the open scene fails and keeps the file") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        // A scene file copied in from outside: the import's undo is an asset.delete of it.
        (void) editor.ok("scene.save");
        const std::filesystem::path incoming = editor.project_directory() / "incoming" / "copied.scene.json";
        write_file(incoming, editor_test::read_text_file(test_scene_file(editor)));
        (void) editor.ok(
          "asset.import",
          core::json::object({{"source", incoming.generic_string()}, {"destination", "scenes/copied.scene.json"}})
        );
        // scene.save_as keeps the history, so the open scene now sits on the file that import's undo deletes.
        (void) editor.ok("scene.save_as", core::json::object({{"path", "scenes/copied"}}));
        REQUIRE(editor.context().scene_path() == "assets://scenes/copied.scene.json");
        const std::filesystem::path copied = editor.project_directory() / "assets" / "scenes" / "copied.scene.json";
        editor.log().allow_errors();
        const protocol_response undone = editor.send("history.undo");
        REQUIRE_FALSE(undone.ok);
        // The refusal of the delete itself, not some other failure of the step.
        CHECK(undone.errorMessage.find("closeOpenScene") != std::string::npos);
        CHECK(std::filesystem::is_regular_file(copied));
        CHECK(editor.context().has_scene());
        CHECK(editor.context().scene_path() == "assets://scenes/copied.scene.json");
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
    }

    TEST_CASE("regression - code sweep F656 a redo of asset.delete with closeOpenScene is refused when its file "
      "became the open scene") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::filesystem::path incoming = editor.project_directory() / "incoming" / "copied.scene.json";
        write_file(incoming, editor_test::read_text_file(test_scene_file(editor)));
        const core::json::document imported = editor.ok(
          "asset.import",
          core::json::object({{"source", incoming.generic_string()}, {"destination", "scenes/copied.scene.json"}})
        );
        const std::string uuid = std::string(imported.root()["uuid"].as_string());
        const std::filesystem::path copied = editor.project_directory() / "assets" / "scenes" / "copied.scene.json";

        // Not the open scene yet, so the flag changes nothing: a plain delete whose redo carries the flag along.
        const core::json::document deleted = editor.ok(
          "asset.delete",
          core::json::object({{"asset", uuid}, {"closeOpenScene", true}})
        );
        CHECK_FALSE(deleted.root()["closedScene"].as_bool(true));
        (void) editor.ok("history.undo");
        REQUIRE(std::filesystem::is_regular_file(copied));
        REQUIRE(editor.context().undo().can_redo());
        // scene.save_as records nothing, so the redo tail survives while the open scene moves onto that file.
        (void) editor.ok("scene.save_as", core::json::object({{"path", "scenes/copied"}}));
        REQUIRE(editor.context().scene_path() == "assets://scenes/copied.scene.json");
        REQUIRE(editor.context().undo().can_redo());

        // The redo is the delete with closeOpenScene: closing would clear the history that is being replayed.
        editor.log().allow_errors();
        const protocol_response redone = editor.send("history.redo");
        REQUIRE_FALSE(redone.ok);
        CHECK(redone.errorMessage.find("undo or redo step") != std::string::npos);
        CHECK(std::filesystem::is_regular_file(copied));
        CHECK(editor.context().has_scene());
        CHECK(editor.context().scene_path() == "assets://scenes/copied.scene.json");
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
        // The failed step is dropped; what lies below it is still there to undo.
        CHECK(editor.context().undo().can_undo());
        CHECK_FALSE(editor.context().undo().can_redo());
        CHECK(editor.context().undo().size() == 2);
    }

    TEST_CASE("regression - code sweep F656 asset.delete that cannot remove the file leaves the open scene as it "
      "was") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        (void) editor.create_entity("Unsaved");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({"Unsaved"})}}));
        REQUIRE(editor.context().scene_dirty());
        const usize entries = editor.context().undo().size();
        REQUIRE(entries == 2);

        // A remove the file system refuses, on every platform: the asset's .meta is a folder that is not empty.
        // (The database removes the source before it reaches the .meta, so the file itself is gone afterwards;
        // what this case pins is that the editor's own state survives a delete that reports failure.)
        std::filesystem::path meta = test_scene_file(editor);
        meta += ".meta";
        std::error_code ignored = {};
        REQUIRE(std::filesystem::remove(meta, ignored));
        write_file(meta / "blocker.txt", "keeps the folder from being removed");

        const protocol_response refused = editor.send(
          "asset.delete",
          core::json::object({{"asset", TEST_SCENE}, {"closeOpenScene", true}})
        );
        REQUIRE_FALSE(refused.ok);
        CHECK(refused.errorCode == protocol_error_code::IO_ERROR);
        // The delete failed before the scene was closed: nothing of the session is lost.
        CHECK(editor.context().has_scene());
        CHECK(editor.context().scene_path() == TEST_SCENE);
        CHECK(editor.context().scene_dirty());
        CHECK(editor.context().undo().size() == entries);
        CHECK(editor.ok("selection.get").root()["count"].as_u64() == 1);
        CHECK(editor.scene().world().entity_count() == 2);
        (void) editor.ok("entity.get", core::json::object({{"entity", "Unsaved"}}));
        // The unsaved work is still there and can be undone step by step.
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 1);
    }

    TEST_CASE("asset.delete - a scene.close that fails after the delete puts the file back and leaves the scene "
      "open") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::string uuid = std::string(editor.ok(
          "asset.info",
          core::json::object({{"asset", TEST_SCENE}})
        ).root()["uuid"].as_string());
        const std::string content = editor_test::read_text_file(test_scene_file(editor));
        const usize entries = editor.context().undo().size();
        REQUIRE_FALSE(editor.context().scene_dirty());

        // The guard of asset.delete makes sure scene.close is allowed, so only a stand-in can make it fail.
        command_registry &registry = editor.context().registry();
        REQUIRE(registry.remove("scene.close"));
        command_descriptor refusing = {};
        refusing.name = "scene.close";
        refusing.summary = "A stand-in that never closes the scene.";
        refusing.argsSchema = R"({"type":"object","additionalProperties":true})";
        refusing.resultSchema = R"({"type":"object"})";
        refusing.owner = "test";
        refusing.handler = [](command_invocation &) -> core::result<command_outcome> {
            return make_editor_error(protocol_error_code::BUSY, "the stand-in never closes the scene");
        };
        CONJURE_REQUIRE_OK(registry.add(std::move(refusing)));

        editor.clear_events();
        const protocol_response failed = editor.send(
          "asset.delete",
          core::json::object({{"asset", uuid}, {"closeOpenScene", true}})
        );
        REQUIRE_FALSE(failed.ok);
        CHECK(failed.errorCode == protocol_error_code::BUSY);
        // The file is back byte for byte under its uuid, and the session is as it was.
        REQUIRE(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor_test::read_text_file(test_scene_file(editor)) == content);
        CHECK(editor.ok("asset.info", core::json::object({{"asset", TEST_SCENE}})).root()["uuid"].as_string() == uuid);
        CHECK(editor.context().has_scene());
        CHECK(editor.context().scene_path() == TEST_SCENE);
        CHECK_FALSE(editor.context().scene_dirty());
        CHECK(editor.context().undo().size() == entries);
        CHECK(editor.warnings("STARTUP_SCENE_DELETED").empty());
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
    }

    TEST_CASE("regression - code sweep F656 asset.delete knows the open scene under another spelling of its file") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::string uuid = std::string(editor.ok(
          "asset.info",
          core::json::object({{"asset", TEST_SCENE}})
        ).root()["uuid"].as_string());

        const protocol_response opened = editor.send("scene.open", core::json::object({{"path", "Scenes/TEST"}}));
        if (!opened.ok) {
            // A volume that tells the two spellings apart: the other one names no file, so the scene cannot be
            // open under it and there is nothing to guard.
            CHECK(opened.errorCode == protocol_error_code::PATH_NOT_FOUND);
            CHECK(editor.context().scene_path() == TEST_SCENE);
            MESSAGE(
              "the volume tells the two spellings apart: the spelling guard is not exercised here (the hard-link "
              "cases cover it)"
            );
            return;
        }
        // The volume ignores case: the scene is open under a spelling the database does not register.
        REQUIRE(editor.context().scene_path() == "assets://Scenes/TEST.scene.json");
        (void) editor.ok(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"startup_scene", "SCENES/test.scene.json"}})}})
        );

        // The guard decides by file, not by spelling: by path and by uuid the delete is refused.
        const protocol_response refused = editor.send("asset.delete", core::json::object({{"asset", TEST_SCENE}}));
        REQUIRE_FALSE(refused.ok);
        CHECK(refused.errorCode == protocol_error_code::INVALID_ARGS);
        CHECK(refused.errorMessage.find("closeOpenScene") != std::string::npos);
        CHECK(editor.fail("asset.delete", core::json::object({{"asset", uuid}})) == protocol_error_code::INVALID_ARGS);
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().has_scene());
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));

        // With the flag it is the open scene and the startup scene, whatever either was spelled like.
        editor.clear_events();
        const core::json::document deleted = editor.ok(
          "asset.delete",
          core::json::object({{"asset", uuid}, {"closeOpenScene", true}})
        );
        CHECK(deleted.root()["closedScene"].as_bool());
        CHECK(deleted.root()["startupScene"].as_bool());
        CHECK(editor.warnings("STARTUP_SCENE_DELETED").size() == 1);
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        CHECK_FALSE(editor.context().has_scene());
    }

    TEST_CASE("regression - code sweep F656 asset.delete of the startup scene succeeds with a warning event") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        (void) editor.ok("scene.save_as", core::json::object({{"path", "scenes/other"}}));
        (void) editor.ok(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"startup_scene", "scenes/test.scene.json"}})}})
        );
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "plain.png");
        (void) editor.ok("asset.import", core::json::object({{"source", "plain.png"}}));

        // An ordinary asset raises no warning.
        editor.clear_events();
        const core::json::document png = editor.ok("asset.delete", core::json::object({{"asset", "plain.png"}}));
        CHECK_FALSE(png.root()["startupScene"].as_bool(true));
        CHECK(editor.warnings("STARTUP_SCENE_DELETED").empty());

        // The startup scene is not the open one here: no flag is needed, the delete succeeds and says so.
        editor.clear_events();
        const core::json::document deleted = editor.ok("asset.delete", core::json::object({{"asset", TEST_SCENE}}));
        CHECK(deleted.root()["startupScene"].as_bool());
        CHECK_FALSE(deleted.root()["closedScene"].as_bool(true));
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        const std::vector<protocol_event> warnings = editor.warnings("STARTUP_SCENE_DELETED");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.front().data.root()["path"].as_string() == TEST_SCENE);
        CHECK(warnings.front().data.root()["message"].as_string().find("startup scene") != std::string::npos);
        // project.json still names it: the warning is the only trace, by decision.
        CHECK(editor.ok("project.info").root()["startupScene"].as_string() == "scenes/test.scene.json");

        // Undo brings the file back; deleting it again (the redo) warns again.
        (void) editor.ok("history.undo");
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        editor.clear_events();
        (void) editor.ok("history.redo");
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        CHECK(editor.warnings("STARTUP_SCENE_DELETED").size() == 1);
    }

    TEST_CASE("asset.delete - a startup scene that is also the open scene needs closeOpenScene and still warns") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("scene.save");
        // A scheme-qualified startup scene names the same asset as one relative to the assets root.
        (void) editor.ok(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"startup_scene", TEST_SCENE}})}})
        );
        editor.clear_events();
        CHECK(editor.fail("asset.delete", core::json::object({{"asset", TEST_SCENE}})) ==
          protocol_error_code::INVALID_ARGS);
        // A refused delete deleted nothing, so it warns about nothing.
        CHECK(editor.warnings("STARTUP_SCENE_DELETED").empty());
        const core::json::document deleted = editor.ok(
          "asset.delete",
          core::json::object({{"asset", TEST_SCENE}, {"closeOpenScene", true}})
        );
        CHECK(deleted.root()["closedScene"].as_bool());
        CHECK(deleted.root()["startupScene"].as_bool());
        CHECK(editor.warnings("STARTUP_SCENE_DELETED").size() == 1);
        CHECK_FALSE(editor.context().has_scene());
    }

    TEST_CASE("regression - code sweep F656 an asset whose source file vanished answers PATH_NOT_FOUND, an unknown "
      "reference ASSET_NOT_FOUND") {
        editor_test::headless_editor editor = {};
        const core::json::document created = editor.ok(
          "asset.create_material",
          core::json::object({{"path", "materials/gone"}})
        );
        const std::string uuid = std::string(created.root()["uuid"].as_string());
        // Removed behind the database's back: the record still resolves, the file it names does not exist.
        std::error_code ignored = {};
        REQUIRE(std::filesystem::remove(editor.project_directory() / "assets" / "materials" / "gone.cmat", ignored));
        const protocol_response set = editor.send(
          "asset.set",
          core::json::object({{"asset", uuid}, {"path", "anything"}, {"value", 1}})
        );
        REQUIRE_FALSE(set.ok);
        CHECK(set.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(set.errorMessage.find("gone.cmat") != std::string::npos);
        CHECK(editor.fail(
          "asset.delete",
          core::json::object({{"asset", uuid}})
        ) == protocol_error_code::PATH_NOT_FOUND);
        // asset.reimport hands the asset to the importer, whose report of the failed read carries no code: the
        // command looks first, and a reimport that cannot read its source does not switch the importer either.
        const protocol_response reimported = editor.send(
          "asset.reimport",
          core::json::object({{"asset", uuid}, {"importer", "conjure.renderer.gltf"}})
        );
        REQUIRE_FALSE(reimported.ok);
        CHECK(reimported.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(reimported.errorMessage.find("gone.cmat") != std::string::npos);
        CHECK(editor.ok("asset.info", core::json::object({{"asset", uuid}})).root()["importer"].as_string() !=
          "conjure.renderer.gltf");
        // Something is there, but it is not a file that can be read: IO_ERROR.
        REQUIRE(std::filesystem::create_directory(
          editor.project_directory() / "assets" / "materials" / "gone.cmat",
          ignored
        ));
        CHECK(editor.fail(
          "asset.reimport",
          core::json::object({{"asset", uuid}})
        ) == protocol_error_code::IO_ERROR);

        // prefab.apply rewrites the prefab's source file, which has to be there to be kept for the undo.
        const std::string root = editor.create_entity("Cell");
        (void) editor.ok("prefab.create", core::json::object({{"entity", root}, {"path", "prefabs/cell"}}));
        const std::filesystem::path prefab = editor.project_directory() / "assets" / "prefabs" / "cell.prefab.json";
        REQUIRE(std::filesystem::remove(prefab, ignored));
        const protocol_response applied = editor.send("prefab.apply", core::json::object({{"entity", root}}));
        REQUIRE_FALSE(applied.ok);
        CHECK(applied.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(applied.errorMessage.find("cell.prefab.json") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(prefab));

        // A reference that names no asset at all keeps its own code, also where a folder would be accepted.
        CHECK(editor.fail(
          "asset.set",
          core::json::object({{"asset", "materials/never.cmat"}, {"path", "anything"}, {"value", 1}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
        CHECK(editor.fail(
          "asset.delete",
          core::json::object({{"asset", "materials/never.cmat"}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
        CHECK(editor.fail(
          "asset.info",
          core::json::object({{"asset", "00000000-0000-4000-8000-000000000001"}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
    }

    TEST_CASE("regression - code sweep F089 asset commands run while playing are undone after play.stop") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path incoming = editor.project_directory() / "incoming";
        const std::filesystem::path textures = editor.project_directory() / "assets" / "textures";
        // An asset of before the session, deleted while playing.
        const core::json::document old = editor.ok(
          "asset.import",
          core::json::object({
            {"source", editor_test::write_tiny_png(incoming / "old.png").generic_string()},
            {"destination", "textures/old.png"},
          })
        );
        const std::string oldUuid = std::string(old.root()["uuid"].as_string());
        const usize historyBefore = editor.context().undo().size();

        (void) editor.ok("play.start");
        const core::json::document live = editor.ok(
          "asset.import",
          core::json::object({
            {"source", editor_test::write_tiny_png(incoming / "live.png").generic_string()},
            {"destination", "textures/live.png"},
          })
        );
        const std::string liveUuid = std::string(live.root()["uuid"].as_string());
        (void) editor.ok("asset.create_folder", core::json::object({{"path", "live_folder"}}));
        (void) editor.ok("asset.delete", core::json::object({{"asset", oldUuid}}));
        CHECK(editor.context().undo().size() == historyBefore);
        CHECK(editor.context().play().kept_file_edits() == 3);

        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 3);
        // The files are as the session left them.
        CHECK(std::filesystem::is_regular_file(textures / "live.png"));
        CHECK(std::filesystem::is_directory(editor.project_directory() / "assets" / "live_folder"));
        CHECK_FALSE(std::filesystem::exists(textures / "old.png"));
        REQUIRE(editor.context().undo().size() == historyBefore + 1);
        CHECK(editor.context().undo().entries().back()->label == play_controller::FILE_EDITS_LABEL);

        (void) editor.ok("history.undo");
        CHECK_FALSE(std::filesystem::exists(textures / "live.png"));
        CHECK_FALSE(std::filesystem::exists(textures / "live.png.meta"));
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "assets" / "live_folder"));
        CHECK(std::filesystem::is_regular_file(textures / "old.png"));
        CHECK(editor.ok(
          "asset.info",
          core::json::object({{"asset", "textures/old.png"}})
        ).root()["uuid"].as_string() == oldUuid);
        CHECK(editor.fail("asset.info", core::json::object({{"asset", liveUuid}})) ==
          protocol_error_code::ASSET_NOT_FOUND);

        (void) editor.ok("history.redo");
        CHECK_FALSE(std::filesystem::exists(textures / "old.png"));
        CHECK(std::filesystem::is_directory(editor.project_directory() / "assets" / "live_folder"));
        // The redo restores the import with the uuid it had while playing.
        CHECK(editor.ok(
          "asset.info",
          core::json::object({{"asset", "textures/live.png"}})
        ).root()["uuid"].as_string() == liveUuid);
    }

    TEST_CASE("regression - code sweep F089 prefab.apply while playing is kept and its redo writes the applied bytes") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        (void) editor.ok(
          "component.add",
          core::json::object({
            {"entity", root},
            {"type", "sprite"},
            {"value", core::json::object({{"size", core::json::array({0.9f, 0.9f})}})},
          })
        );
        (void) editor.ok("prefab.create", core::json::object({{"entity", root}, {"path", "prefabs/live"}}));
        const std::filesystem::path file = editor.project_directory() / "assets" / "prefabs" / "live.prefab.json";
        const std::string before = editor_test::read_text_file(file);
        const usize historyBefore = editor.context().undo().size();

        (void) editor.ok("play.start");
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", root}, {"path", "sprite.size"}, {"value", core::json::array({2.0f, 2.0f})}})
        );
        (void) editor.ok("prefab.apply", core::json::object({{"entity", root}}));
        const std::string applied = editor_test::read_text_file(file);
        REQUIRE(applied != before);
        CHECK(applied.find("2.0") != std::string::npos);
        CHECK(editor.context().undo().size() == historyBefore);
        CHECK(editor.context().play().kept_file_edits() == 1);

        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 1);
        // The scene is back at the snapshot, the prefab file is not.
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", root}, {"path", "sprite.size"}})
        ).root()["value"][0].as_f32() == doctest::Approx(0.9f));
        CHECK(editor_test::read_text_file(file) == applied);
        REQUIRE(editor.context().undo().size() == historyBefore + 1);

        (void) editor.ok("history.undo");
        CHECK(editor_test::read_text_file(file) == before);
        // The redo cannot apply again: the instance holds the snapshot's 0.9 now. It writes the applied bytes.
        (void) editor.ok("history.redo");
        CHECK(editor_test::read_text_file(file) == applied);
        (void) editor.ok("history.undo");
        CHECK(editor_test::read_text_file(file) == before);
    }

    TEST_CASE("prefab.apply - while editing the redo applies the instance again") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        (void) editor.ok("component.add", core::json::object({{"entity", root}, {"type", "sprite"}}));
        (void) editor.ok("prefab.create", core::json::object({{"entity", root}, {"path", "prefabs/edited"}}));
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", root}, {"path", "sprite.size"}, {"value", core::json::array({3.0f, 3.0f})}})
        );
        (void) editor.ok("prefab.apply", core::json::object({{"entity", root}}));
        const undo_entry *entry = editor.context().undo().entries().back();
        REQUIRE(entry->redo.size() == 1);
        CHECK(entry->redo[0].name == "prefab.apply");
        const std::filesystem::path file = editor.project_directory() / "assets" / "prefabs" / "edited.prefab.json";
        const std::string applied = editor_test::read_text_file(file);
        (void) editor.ok("history.undo");
        CHECK(editor_test::read_text_file(file) != applied);
        (void) editor.ok("history.redo");
        CHECK(editor_test::read_text_file(file) == applied);
    }

    TEST_CASE("regression - code sweep F089 prefab.create answers WRONG_PLAY_STATE while playing and writes nothing") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Cell");
        const core::json::document request = core::json::object({{"entity", root}, {"path", "prefabs/played"}});
        const std::filesystem::path file = editor.project_directory() / "assets" / "prefabs" / "played.prefab.json";
        (void) editor.ok("play.start");
        const protocol_response playing = editor.send(
          "prefab.create",
          core::json::document::from_value(request.root())
        );
        REQUIRE_FALSE(playing.ok);
        CHECK(playing.errorCode == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(playing.errorDetails.root()["state"].as_string() == "playing");
        (void) editor.ok("play.pause");
        const protocol_response paused = editor.send(
          "prefab.create",
          core::json::document::from_value(request.root())
        );
        REQUIRE_FALSE(paused.ok);
        CHECK(paused.errorCode == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(paused.errorDetails.root()["state"].as_string() == "paused");
        CHECK_FALSE(std::filesystem::exists(file));
        CHECK(editor.context().play().kept_file_edits() == 0);
        // The entity carries no prefab marker that play.stop would have to throw away.
        CHECK_FALSE(editor.ok(
          "entity.get",
          core::json::object({{"entity", root}})
        ).root()["components"].contains("conjure.scene.prefab_instance"));
        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 0);
        (void) editor.ok("prefab.create", core::json::document::from_value(request.root()));
        CHECK(std::filesystem::is_regular_file(file));
    }
}
