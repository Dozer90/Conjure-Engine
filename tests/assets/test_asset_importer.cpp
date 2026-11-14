#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/memory_file_system.h"

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

namespace {
    /// <summary>
    /// An importer that claims one extension, with or without its dot, and never imports.
    /// </summary>
    class suffix_importer final : public asset_importer {
        private:
            std::string mId = {};
            std::array<std::string_view, 1> mExtensions = {};

        public:
            suffix_importer(std::string id, std::string_view extension) :
                mId(std::move(id)),
                mExtensions({extension}) {}

            [[nodiscard]] std::string_view id() const noexcept override {
                return mId;
            }
            [[nodiscard]] u32 version() const noexcept override {
                return 1;
            }
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
                return mExtensions;
            }
            [[nodiscard]] core::result<import_output> import(import_context &context) const override {
                return core::error(core::error_code::UNSUPPORTED, std::string(context.source_path()));
            }
    };
}

TEST_SUITE("assets/asset_importer") {

    TEST_CASE("importer_registry - register and find by id and by extension ignoring case") {
        importer_registry registry = {};
        CHECK(registry.importers().empty());
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<test_importer>()));
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<failing_importer>()));

        REQUIRE(registry.find("conjure.test.text") != nullptr);
        CHECK(registry.find("conjure.test.text")->version() == 1);
        CHECK(registry.find("conjure.test.missing") == nullptr);
        CHECK(registry.find_for_extension(".ta") == registry.find("conjure.test.text"));
        CHECK(registry.find_for_extension("TA") == registry.find("conjure.test.text"));
        CHECK(registry.find_for_extension(".TXT") == registry.find("conjure.test.text"));
        CHECK(registry.find_for_extension(".bad") == registry.find("conjure.test.failing"));
        CHECK(registry.find_for_extension(".png") == nullptr);
        CHECK(registry.find_for_extension("") == nullptr);
        CHECK(registry.find_for_extension(".") == nullptr);
        CHECK(registry.importers().size() == 2);
        CHECK(registry.importers()[0]->id() == "conjure.test.text");
    }

    TEST_CASE("importer_registry - find_for_path picks the longest registered suffix") {
        // A ".scene.json" importer next to a ".json" one (R45): the longer suffix wins for "x.scene.json"
        // and the shorter one serves plain "x.json"; a suffix has to start at a dot of the file name.
        importer_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<suffix_importer>("conjure.test.json", ".json")));
        CONJURE_REQUIRE_OK(
          registry.register_importer(std::make_unique<suffix_importer>("conjure.test.scene_json", ".scene.json"))
        );
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<test_importer>()));
        const asset_importer *json = registry.find("conjure.test.json");
        const asset_importer *sceneJson = registry.find("conjure.test.scene_json");
        CHECK(registry.find_for_path("scenes/main.scene.json") == sceneJson);
        CHECK(registry.find_for_path("scenes/MAIN.Scene.JSON") == sceneJson);
        CHECK(registry.find_for_path("settings.json") == json);
        CHECK(registry.find_for_path("notscene.json") == json);
        CHECK(registry.find_for_path("a.notscene.json") == json);
        CHECK(registry.find_for_path("dir.scene.json/readme.ta") == registry.find("conjure.test.text"));
        // A leading dot is part of the name, not an extension (path_utils): ".scene.json" is a hidden
        // ".scene" file with a ".json" extension, so the suffix never consumes the whole name.
        CHECK(registry.find_for_path(".scene.json") == json);
        CHECK(registry.find_for_path("scene.json") == json);
        CHECK(registry.find_for_path("archive.tar.gz") == nullptr);
        CHECK(registry.find_for_path("") == nullptr);
        // The single-extension lookup is unchanged: it knows the last extension only.
        CHECK(registry.find_for_extension(".json") == json);
        CHECK(registry.find_for_extension(".scene.json") == sceneJson);
    }

    TEST_CASE("regression - review 3 find_for_path matches an extension registered without the dot") {
        // "gz" and "scene.json" registered bare next to a dotted ".json": the bare spelling matches the
        // same names as the dotted one, the dot boundary still applies and the longest suffix wins across
        // both spellings. A hidden file whose whole name is the extension (".gz", ".json") matches nothing
        // in either spelling, as find_for_extension(path_utils::extension()) never matched it.
        importer_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<suffix_importer>("conjure.test.json", ".json")));
        CONJURE_REQUIRE_OK(
          registry.register_importer(std::make_unique<suffix_importer>("conjure.test.scene_json", "scene.json"))
        );
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<suffix_importer>("conjure.test.gz", "gz")));
        const asset_importer *json = registry.find("conjure.test.json");
        const asset_importer *sceneJson = registry.find("conjure.test.scene_json");
        const asset_importer *gz = registry.find("conjure.test.gz");
        CHECK(registry.find_for_path("a.scene.json") == sceneJson);
        CHECK(registry.find_for_path("A.SCENE.JSON") == sceneJson);
        CHECK(registry.find_for_path("ascene.json") == json);
        CHECK(registry.find_for_path("a.json") == json);
        CHECK(registry.find_for_path("archive.tar.gz") == gz);
        CHECK(registry.find_for_path("dir.gz/plain.GZ") == gz);
        CHECK(registry.find_for_path("targz") == nullptr);
        CHECK(registry.find_for_path("gz") == nullptr);
        CHECK(registry.find_for_path(".gz") == nullptr);
        CHECK(registry.find_for_path(".json") == nullptr);
        CHECK(registry.find_for_path(".scene.json") == json);
        CHECK(registry.find_for_path("x..gz") == gz);
        CHECK(registry.find_for_extension("gz") == gz);
        CHECK(registry.find_for_extension(".GZ") == gz);
    }

    TEST_CASE("regression - code sweep F137 a second importer for a claimed extension is reported and stays second") {
        conjure::testing::log_capture logs = {};
        importer_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<suffix_importer>("conjure.test.model", ".glb")));
        CHECK(logs.count(core::log_level::WARN) == 0);

        // Accepted, but never silently: a whole file type must not go to whichever module registered first.
        CONJURE_REQUIRE_OK(
          registry.register_importer(std::make_unique<suffix_importer>("conjure.test.collision", "GLB"))
        );
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(
          core::log_level::WARN,
          "importer 'conjure.test.collision' claims 'GLB', which 'conjure.test.model' already imports"
        ));
        const asset_importer *model = registry.find("conjure.test.model");
        CHECK(registry.find_for_path("props/crate.glb") == model);
        CHECK(registry.find_for_extension(".glb") == model);
        // The second importer is still there for a .meta that names it.
        CHECK(registry.find("conjure.test.collision") != nullptr);

        // A longer suffix is a different claim, not a clash (R45).
        CONJURE_REQUIRE_OK(
          registry.register_importer(std::make_unique<suffix_importer>("conjure.test.special", ".collision.glb"))
        );
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(registry.find_for_path("props/crate.collision.glb") == registry.find("conjure.test.special"));
    }

    TEST_CASE("importer_registry - a duplicate id, a null importer and an unknown unregister fail") {
        importer_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<test_importer>()));
        CONJURE_CHECK_ERROR(
          registry.register_importer(std::make_unique<test_importer>(2)),
          core::error_code::ALREADY_EXISTS
        );
        CONJURE_CHECK_ERROR(registry.register_importer(nullptr), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(registry.unregister_importer("conjure.test.nope"), core::error_code::NOT_FOUND);
        CHECK(registry.importers().size() == 1);
    }

    TEST_CASE("importer_registry - unregister is refused while imports are in flight") {
        importer_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_importer(std::make_unique<test_importer>()));
        registry.begin_imports();
        CHECK(registry.imports_in_flight() == 1);
        CONJURE_CHECK_ERROR(registry.unregister_importer("conjure.test.text"), core::error_code::INVALID_STATE);
        registry.end_imports();
        CHECK(registry.imports_in_flight() == 0);
        CONJURE_REQUIRE_OK(registry.unregister_importer("conjure.test.text"));
        CHECK(registry.find("conjure.test.text") == nullptr);
        CHECK(registry.find_for_extension(".ta") == nullptr);
    }

    TEST_CASE("asset_importer - default settings are an empty object unless overridden") {
        const failing_importer plain = {};
        CHECK(plain.default_settings().to_string() == "{}");
        const test_importer custom = {};
        CHECK(custom.default_settings().root()["upper"].is_bool());
    }

    TEST_CASE("sub_asset_table - existing ids are kept, new ones allocated once and sorted on output") {
        const asset_id known = test_id(1);
        sub_asset_table table(std::vector<sub_asset_entry>{sub_asset_entry{"mesh/Body", known}});
        CHECK_FALSE(table.changed());
        CHECK(table.id_for("mesh/Body") == known);
        CHECK_FALSE(table.changed());

        const asset_id allocated = table.id_for("material/Skin");
        CHECK_FALSE(allocated.is_null());
        CHECK(allocated != known);
        CHECK(table.changed());
        CHECK(table.id_for("material/Skin") == allocated);

        const std::vector<sub_asset_entry> entries = table.entries();
        REQUIRE(entries.size() == 2);
        CHECK(entries[0].subPath == "material/Skin");
        CHECK(entries[1].subPath == "mesh/Body");
    }

    TEST_CASE("import_context - exposes the source, settings, platform and primary id") {
        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> bytes = bytes_of("content");
        const core::json::document settings = core::json::object({{"upper", true}});
        sub_asset_table subAssets = {};
        import_context context(
          fileSystem,
          "assets://models/robot.gltf",
          "assets/models/robot.gltf",
          bytes,
          settings.root(),
          "windows",
          test_id(1),
          subAssets,
          {}
        );
        CHECK(context.source_path() == "assets://models/robot.gltf");
        CHECK(text_of(context.source_bytes()) == "content");
        CHECK(context.settings()["upper"].as_bool());
        CHECK(context.platform() == "windows");
        CHECK(context.primary_id() == test_id(1));
        CHECK(context.side_files().empty());
        CHECK(context.warnings().empty());
        CHECK_FALSE(context.find_asset_by_path("textures/a.png").has_value());
        context.add_warning("careful");
        REQUIRE(context.warnings().size() == 1);
        CHECK(context.warnings()[0] == "careful");
    }

    TEST_CASE("import_context - side files are read next to the source and recorded by asset path") {
        core::vfs::memory_file_system fileSystem = {};
        CONJURE_REQUIRE_OK(fileSystem.write_file("assets/models/robot.bin", bytes_of("binary")));
        const std::vector<std::byte> bytes = {};
        sub_asset_table subAssets = {};
        import_context context(
          fileSystem, "assets://models/robot.gltf", "assets/models/robot.gltf", bytes, {}, "linux", test_id(
            2
          ), subAssets, {}
        );
        core::result<std::vector<std::byte>> side = context.read_side_file("robot.bin");
        CONJURE_REQUIRE_OK(side);
        CHECK(text_of(side.value()) == "binary");
        REQUIRE(context.side_files().size() == 1);
        CHECK(context.side_files()[0] == "assets://models/robot.bin");

        CONJURE_REQUIRE_OK(context.read_side_file("./robot.bin"));
        CHECK(context.side_files().size() == 1);
        CONJURE_CHECK_ERROR(context.read_side_file("missing.bin"), asset_error_code::IO_ERROR);

        // A side file that cannot be read is recorded too, so its appearance can be watched for.
        REQUIRE(context.side_files().size() == 2);
        CHECK(context.side_files()[0] == "assets://models/missing.bin");
        CHECK(context.side_files()[1] == "assets://models/robot.bin");
    }

    TEST_CASE("import_context - sub-asset ids are stable and paths resolve through the resolver") {
        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> bytes = {};
        sub_asset_table subAssets(std::vector<sub_asset_entry>{sub_asset_entry{"mesh/Body", test_id(5)}});
        std::vector<std::string> asked = {};
        const asset_path_resolver resolver = [&asked](std::string_view path) -> std::optional<asset_id> {
            asked.emplace_back(path);
            if (path == "assets://textures/a.png") {
                return test_id(9);
            }
            return std::nullopt;
        };
        import_context context(
          fileSystem, "assets://models/robot.gltf", "assets/models/robot.gltf", bytes, {}, "macos", test_id(
            1
          ), subAssets, resolver
        );
        CHECK(context.sub_asset_id("mesh/Body") == test_id(5));
        const asset_id fresh = context.sub_asset_id("mesh/Head");
        CHECK(context.sub_asset_id("mesh/Head") == fresh);
        CHECK(subAssets.changed());

        CHECK(context.find_asset_by_path("../textures/a.png") == test_id(9));
        CHECK(context.find_asset_by_path("assets://textures/a.png") == test_id(9));
        CHECK_FALSE(context.find_asset_by_path("missing.png").has_value());
        REQUIRE(asked.size() == 3);
        CHECK(asked[0] == "assets://textures/a.png");
        CHECK(asked[2] == "assets://models/missing.png");
    }

    TEST_CASE("test_importer - produces one primary plus the declared sub-assets") {
        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> bytes = bytes_of("sub Body\nsub Head\nhello");
        sub_asset_table subAssets = {};
        import_context context(
          fileSystem, "assets://robot.ta", "assets/robot.ta", bytes, {}, "windows", test_id(1), subAssets, {}
        );
        const test_importer importer = {};
        core::result<import_output> output = importer.import(context);
        CONJURE_REQUIRE_OK(output);
        REQUIRE(output.value().assets.size() == 3);
        CHECK(output.value().assets[0].subPath.empty());
        CHECK(text_of(output.value().assets[0].payload) == "hello");
        CHECK(output.value().assets[1].subPath == "Body");
        CHECK(text_of(output.value().assets[1].payload) == "Body:hello");
        CHECK(subAssets.entries().size() == 2);
    }
}
