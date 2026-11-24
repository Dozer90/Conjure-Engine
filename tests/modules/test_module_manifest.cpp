#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    constexpr std::string_view FULL_MANIFEST = R"({
      "schema": 1,
      "name": "com.studio.space_shooter",
      "displayName": "Space Shooter",
      "description": "The everything game",
      "version": "0.3.1",
      "kind": "game",
      "engine": "^0.1.0",
      "platforms": ["windows", "linux", "macos"],
      "requires": ["renderer", "physics", "scripting"],
      "binary": "space_shooter",
      "dependencies": [
        { "name": "conjure.physics", "version": "*" },
        { "name": "com.studio.gameplay_common", "version": ">=1.0.0 <2.0.0", "optional": false },
        { "name": "com.studio.optional_bits", "version": "~2.1.0", "optional": true }
      ],
      "managed": ["managed/SpaceShooter.dll"],
      "content": ["assets", "levels"],
      "editorOnly": true
    })";

    constexpr std::string_view MINIMAL_MANIFEST = R"({"schema": 1, "name": "tiny", "version": "1.0.0", "engine": "*"})";

    // Every member a validation test needs, minus the closing brace so a case can append one member.
    const std::string BASE = R"({"schema": 1, "name": "a", "version": "1.0.0", "engine": "*")";

    module_manifest parse_or_fail(std::string_view text) {
        result<module_manifest> parsed = parse_module_manifest(text, "test.json");
        INFO("parse failure: ", conjure::testing::describe_error(parsed));
        REQUIRE(parsed.has_value());
        return std::move(parsed).value();
    }

    std::string message_of(std::string_view text) {
        const result<module_manifest> parsed = parse_module_manifest(text, "bad.json");
        REQUIRE_FALSE(parsed.has_value());
        return parsed.get_error().message();
    }
}

TEST_SUITE("modules/module_manifest") {

    TEST_CASE("module_manifest - a full manifest parses every member") {
        const module_manifest manifest = parse_or_fail(FULL_MANIFEST);

        CHECK(manifest.schemaVersion == 1);
        CHECK(manifest.name == "com.studio.space_shooter");
        CHECK(manifest.displayName == "Space Shooter");
        CHECK(manifest.description == "The everything game");
        CHECK(manifest.version == semantic_version{0, 3, 1});
        CHECK(manifest.kind == module_kind::GAME);
        CHECK(manifest.engineVersion == version_range{{0, 1, 0}, {0, 2, 0}});
        CHECK(manifest.platforms == module_platform_flags::ALL);
        CHECK(manifest.requiredFeatures
          == (module_feature_flags::RENDERER | module_feature_flags::PHYSICS | module_feature_flags::SCRIPTING));
        CHECK(manifest.binary == "space_shooter");
        REQUIRE(manifest.dependencies.size() == 3);
        CHECK(manifest.dependencies[0].name == "conjure.physics");
        CHECK(manifest.dependencies[0].version == version_range{});
        CHECK_FALSE(manifest.dependencies[0].optional);
        CHECK(manifest.dependencies[1].version == version_range{{1, 0, 0}, {2, 0, 0}});
        CHECK(manifest.dependencies[2].name == "com.studio.optional_bits");
        CHECK(manifest.dependencies[2].version == version_range{{2, 1, 0}, {2, 2, 0}});
        CHECK(manifest.dependencies[2].optional);
        CHECK(manifest.managedAssemblies == std::vector<std::string>{"managed/SpaceShooter.dll"});
        CHECK(manifest.contentDirectories == std::vector<std::string>{"assets", "levels"});
        CHECK(manifest.editorOnly);
    }

    TEST_CASE("module_manifest - a minimal manifest gets the documented defaults") {
        const module_manifest manifest = parse_or_fail(MINIMAL_MANIFEST);

        CHECK(manifest.name == "tiny");
        CHECK(manifest.displayName.empty());
        CHECK(manifest.kind == module_kind::PLUGIN);
        CHECK(manifest.platforms == module_platform_flags::ALL);
        CHECK(manifest.requiredFeatures == module_feature_flags::NONE);
        CHECK(manifest.binary.empty());
        CHECK(manifest.dependencies.empty());
        CHECK(manifest.managedAssemblies.empty());
        CHECK(manifest.contentDirectories.empty());
        CHECK_FALSE(manifest.editorOnly);
        CHECK(manifest.engineVersion.is_unbounded());
    }

    TEST_CASE("module_manifest - each required member missing is MANIFEST_INVALID naming the pointer") {
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"name": "a", "version": "1.0.0", "engine": "*"})"),
          module_error::MANIFEST_INVALID
        );
        CHECK(message_of(R"({"name": "a", "version": "1.0.0", "engine": "*"})").find("/schema") != std::string::npos);
        CHECK(message_of(R"({"schema": 1, "version": "1.0.0", "engine": "*"})").find("/name") != std::string::npos);
        CHECK(message_of(R"({"schema": 1, "name": "a", "engine": "*"})").find("/version") != std::string::npos);
        CHECK(message_of(R"({"schema": 1, "name": "a", "version": "1.0.0"})").find("/engine") != std::string::npos);
        CHECK(message_of(R"({"schema": 1, "name": "a", "version": "1.0.0"})").find("bad.json") == 0);
    }

    TEST_CASE("module_manifest - unsupported schema, bad JSON and a non-object root are MANIFEST_INVALID") {
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": 2, "name": "a", "version": "1.0.0", "engine": "*"})"),
          module_error::MANIFEST_INVALID
        );
        CHECK(message_of(R"({"schema": 2, "name": "a", "version": "1.0.0", "engine": "*"})").find("schema 2")
          != std::string::npos);
        CONJURE_CHECK_ERROR(parse_module_manifest("{ not json"), module_error::MANIFEST_INVALID);
        CONJURE_CHECK_ERROR(parse_module_manifest("[1, 2]"), module_error::MANIFEST_INVALID);
        CONJURE_CHECK_ERROR(parse_module_manifest(""), module_error::MANIFEST_INVALID);
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": "1", "name": "a", "version": "1.0.0", "engine": "*"})"),
          module_error::MANIFEST_INVALID
        );
    }

    TEST_CASE("module_manifest - bad name characters and a 65 character name are INVALID_NAME") {
        const std::string tooLong(65, 'a');
        const std::string justRight(64, 'a');
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": 1, "name": "Has.Upper", "version": "1.0.0", "engine": "*"})"),
          module_error::INVALID_NAME
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": 1, "name": "has space", "version": "1.0.0", "engine": "*"})"),
          module_error::INVALID_NAME
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": 1, "name": "has-dash", "version": "1.0.0", "engine": "*"})"),
          module_error::INVALID_NAME
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": 1, "name": "", "version": "1.0.0", "engine": "*"})"),
          module_error::INVALID_NAME
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(
            R"({"schema": 1, "name": ")" + tooLong + R"(", "version": "1.0.0", "engine": "*"})"
          ),
          module_error::INVALID_NAME
        );
        CONJURE_CHECK_OK(
          parse_module_manifest(
            R"({"schema": 1, "name": ")" + justRight + R"(", "version": "1.0.0", "engine": "*"})"
          )
        );
        CHECK(is_valid_module_name("a.b_c9"));
        CHECK_FALSE(is_valid_module_name("A"));
        CHECK_FALSE(is_valid_module_name(""));
        CHECK_FALSE(is_valid_module_name(tooLong));
        CHECK(is_valid_module_name(justRight));
    }

    TEST_CASE("module_manifest - an unknown key warns and is otherwise ignored") {
        conjure::testing::log_capture logs = {};
        const module_manifest manifest =
          parse_or_fail(R"({"schema": 1, "name": "a", "version": "1.0.0", "engine": "*", "colour": "blue"})");
        CHECK(manifest.name == "a");
        CHECK(logs.contains(log_level::WARN, "colour"));
        CHECK(logs.count(log_level::WARN) == 1);
    }

    TEST_CASE("regression - sweep-2026-11-12 F342 an unknown key inside a dependency warns and is otherwise ignored") {
        conjure::testing::log_capture logs = {};
        const module_manifest manifest =
          parse_or_fail(BASE + R"(, "dependencies": [{"name": "b", "versoin": "^2.0.0", "optinal": true}]})");
        REQUIRE(manifest.dependencies.size() == 1);
        CHECK(manifest.dependencies[0].name == "b");
        CHECK(manifest.dependencies[0].version.is_unbounded());
        CHECK_FALSE(manifest.dependencies[0].optional);
        CHECK(logs.contains(log_level::WARN, "versoin"));
        CHECK(logs.contains(log_level::WARN, "optinal"));
        CHECK(logs.contains(log_level::WARN, "/dependencies/0"));
        CHECK(logs.count(log_level::WARN) == 2);
    }

    TEST_CASE("regression - sweep-2026-11-12 F338 path members that leave the module root are MANIFEST_INVALID") {
        const std::array<std::string_view, 6> badBinaries = {"../x", "sub/x", R"(a\\b)", "C:x", ".", ".."};
        for (const std::string_view binary : badBinaries) {
            CAPTURE(binary);
            const std::string text = BASE + R"(, "binary": ")" + std::string(binary) + R"("})";
            CONJURE_CHECK_ERROR(parse_module_manifest(text), module_error::MANIFEST_INVALID);
            CHECK(message_of(text).find("/binary") != std::string::npos);
        }

        const std::array<std::string_view, 6> badManaged = {
            R"(["../../secrets.dll"])", R"(["/abs/x.dll"])", R"(["C:/x.dll"])", R"(["managed\\..\\..\\x.dll"])",
            R"(["\\\\server\\share\\x.dll"])", R"([""])",
        };
        for (const std::string_view managed : badManaged) {
            CAPTURE(managed);
            const std::string text = BASE + R"(, "managed": )" + std::string(managed) + "}";
            CONJURE_CHECK_ERROR(parse_module_manifest(text), module_error::MANIFEST_INVALID);
            CHECK(message_of(text).find("/managed/0") != std::string::npos);
        }

        const std::array<std::string_view, 3> badContent = {R"([".."])", R"(["assets/../../x"])", R"(["ok", "a/.."])"};
        for (const std::string_view content : badContent) {
            CAPTURE(content);
            const std::string text = BASE + R"(, "content": )" + std::string(content) + "}";
            CONJURE_CHECK_ERROR(parse_module_manifest(text), module_error::MANIFEST_INVALID);
            CHECK(message_of(text).find("/content/") != std::string::npos);
        }
        CHECK(message_of(BASE + R"(, "content": ["ok", "a/.."]})").find("/content/1") != std::string::npos);

        const std::string acceptedText = BASE + R"(, "binary": "space_shooter", "managed": ["managed/Beta.dll"],)"
          + R"( "content": ["assets", "assets/./sub"]})";
        const module_manifest accepted = parse_or_fail(acceptedText);
        CHECK(accepted.binary == "space_shooter");
        CHECK(accepted.managedAssemblies == std::vector<std::string>{"managed/Beta.dll"});
        CHECK(accepted.contentDirectories == std::vector<std::string>{"assets", "assets/./sub"});
        CONJURE_CHECK_OK(parse_module_manifest(BASE + R"(, "binary": "", "content": ["..assets", "a..b/c"]})"));
    }

    TEST_CASE("module_manifest - unknown platform, feature and kind entries are MANIFEST_INVALID") {
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "platforms": ["amiga"]})"),
          module_error::MANIFEST_INVALID
        );
        CHECK(message_of(BASE + R"(, "platforms": ["amiga"]})").find("/platforms/0") != std::string::npos);
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "requires": ["renderer", "teleport"]})"),
          module_error::MANIFEST_INVALID
        );
        CHECK(message_of(BASE + R"(, "requires": ["renderer", "teleport"]})").find("/requires/1") != std::string::npos);
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "kind": "dlc"})"),
          module_error::MANIFEST_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "platforms": "windows"})"),
          module_error::MANIFEST_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "editorOnly": "yes"})"),
          module_error::MANIFEST_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": 1, "name": "a", "version": "x", "engine": "*"})"),
          module_error::MANIFEST_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(R"({"schema": 1, "name": "a", "version": "1.0.0", "engine": ">=2.0.0 <1.0.0"})"),
          module_error::MANIFEST_INVALID
        );
    }

    TEST_CASE("module_manifest - an empty platforms list means no platform") {
        const module_manifest manifest =
          parse_or_fail(R"({"schema": 1, "name": "a", "version": "1.0.0", "engine": "*", "platforms": []})");
        CHECK(manifest.platforms == module_platform_flags::NONE);
    }

    TEST_CASE("module_manifest - self-dependency and duplicate dependencies are MANIFEST_INVALID") {
        const std::string self =
          R"({"schema": 1, "name": "a", "version": "1.0.0", "engine": "*", "dependencies": [{"name": "a"}]})";
        CONJURE_CHECK_ERROR(parse_module_manifest(self), module_error::MANIFEST_INVALID);
        CHECK(message_of(self).find("itself") != std::string::npos);

        const std::string duplicate = R"({"schema": 1, "name": "a", "version": "1.0.0", "engine": "*",
          "dependencies": [{"name": "b"}, {"name": "b", "version": "^1.0.0"}]})";
        CONJURE_CHECK_ERROR(parse_module_manifest(duplicate), module_error::MANIFEST_INVALID);
        CHECK(message_of(duplicate).find("/dependencies/1/name") != std::string::npos);

        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "dependencies": [{"name": "B!"}]})"),
          module_error::MANIFEST_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "dependencies": ["b"]})"),
          module_error::MANIFEST_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "dependencies": [{"name": "b", "version": "nope"}]})"),
          module_error::MANIFEST_INVALID
        );
        CONJURE_CHECK_ERROR(
          parse_module_manifest(BASE + R"(, "dependencies": [{"name": "b", "optional": 1}]})"),
          module_error::MANIFEST_INVALID
        );
    }

    TEST_CASE("module_manifest - write then parse reproduces the manifest") {
        const module_manifest full = parse_or_fail(FULL_MANIFEST);
        const module_manifest minimal = parse_or_fail(MINIMAL_MANIFEST);

        const module_manifest fullAgain = parse_or_fail(write_module_manifest(full).to_string());
        CHECK(fullAgain == full);
        const module_manifest minimalAgain = parse_or_fail(write_module_manifest(minimal).to_string(true));
        CHECK(minimalAgain == minimal);

        const json::document withoutEditorOnly = write_module_manifest(full, false);
        CHECK_FALSE(withoutEditorOnly.root().contains("editorOnly"));
        const module_manifest exported = parse_or_fail(withoutEditorOnly.to_string());
        CHECK_FALSE(exported.editorOnly);
        CHECK(exported.name == full.name);
        CHECK(write_module_manifest(full).root()["editorOnly"].as_bool() == true);
    }

    TEST_CASE("module_manifest - binary paths derive from the triplet and decorate per platform") {
        CHECK(decorate_binary_name("game", module_platform::WINDOWS) == "game.dll");
        CHECK(decorate_binary_name("game", module_platform::LINUX) == "libgame.so");
        CHECK(decorate_binary_name("game", module_platform::MACOS) == "libgame.dylib");
        CHECK(decorate_binary_name("", module_platform::LINUX).empty());

        const module_manifest manifest = parse_or_fail(FULL_MANIFEST);
        const std::filesystem::path root = std::filesystem::path("proj") / "modules" / "shooter";

        const binary_triplet windowsDebug = {module_platform::WINDOWS, module_architecture::X64, build_config::DEBUG};
        CHECK(module_binary_path(manifest, root, windowsDebug).generic_string()
          == "proj/modules/shooter/bin/windows-x64-debug/space_shooter.dll");
        const binary_triplet linuxDev = {module_platform::LINUX, module_architecture::X64, build_config::DEVELOPMENT};
        CHECK(module_binary_path(manifest, root, linuxDev).generic_string()
          == "proj/modules/shooter/bin/linux-x64-development/libspace_shooter.so");
        const binary_triplet macDist = {module_platform::MACOS, module_architecture::ARM64, build_config::DISTRIBUTION};
        CHECK(module_binary_path(manifest, root, macDist).generic_string()
          == "proj/modules/shooter/bin/macos-arm64-distribution/libspace_shooter.dylib");
        CHECK(module_binary_directory(root, macDist).generic_string()
          == "proj/modules/shooter/bin/macos-arm64-distribution");

        const module_manifest noBinary = parse_or_fail(MINIMAL_MANIFEST);
        CHECK(module_binary_path(noBinary, root, windowsDebug).empty());
    }

    TEST_CASE("module_manifest - load_module_manifest reads a file and reports a missing one") {
        const conjure::testing::temp_directory directory("modules_manifest");
        CONJURE_REQUIRE_OK(directory.write_text("module.json", MINIMAL_MANIFEST));
        const result<module_manifest> loaded = load_module_manifest(directory.file("module.json"));
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded->name == "tiny");

        CONJURE_CHECK_ERROR(load_module_manifest(directory.file("missing.json")), module_error::MANIFEST_NOT_FOUND);
        CONJURE_CHECK_ERROR(load_module_manifest(directory.path()), module_error::MANIFEST_NOT_FOUND);

        CONJURE_REQUIRE_OK(directory.write_text("broken.json", "{"));
        const result<module_manifest> broken = load_module_manifest(directory.file("broken.json"));
        CONJURE_CHECK_ERROR(broken, module_error::MANIFEST_INVALID);
        CHECK(broken.get_error().message().find("broken.json") != std::string::npos);
    }

    TEST_CASE("regression - code sweep FU-LONGPATH load_module_manifest reads a manifest past MAX_PATH") {
        // Past Windows' MAX_PATH the file is read through the extended-length form; Linux and macOS have no such
        // limit, and the classic form runs the same calls there.
        const conjure::testing::temp_directory directory("modules_manifest_long");
        const std::filesystem::path deep = conjure::testing::long_directory(directory, conjure::testing::PAST_MAX_PATH);
        const std::string relative = deep.lexically_relative(directory.path()).generic_string();
        CONJURE_REQUIRE_OK(directory.write_text(relative + "/module.json", MINIMAL_MANIFEST));
        CONJURE_REQUIRE_OK(directory.write_text(relative + "/broken.json", "{"));

        SUBCASE("in the classic form") {
            const result<module_manifest> loaded = load_module_manifest(deep / "module.json");
            CONJURE_REQUIRE_OK(loaded);
            CHECK(loaded->name == "tiny");
            const std::filesystem::path brokenPath = deep / "broken.json";
            const result<module_manifest> broken = load_module_manifest(brokenPath);
            CONJURE_CHECK_ERROR(broken, module_error::MANIFEST_INVALID);
            CHECK(broken.get_error().message().find(brokenPath.generic_string()) == 0);
        }
        SUBCASE("in the generic device form //?/, which is not prefixed a second time") {
            if constexpr (CONJURE_PLATFORM_WINDOWS == 0) {
                MESSAGE("the //?/ device form exists on Windows only");
                return;
            }
            const std::filesystem::path device = "//?/" + (deep / "module.json").generic_string();
            const result<module_manifest> loaded = load_module_manifest(device);
            CONJURE_REQUIRE_OK(loaded);
            CHECK(loaded->name == "tiny");
        }
    }

    TEST_CASE("module_manifest - load_module_manifest names the file and the pointer of an invalid member") {
        const conjure::testing::temp_directory directory("modules_manifest");
        CONJURE_REQUIRE_OK(directory.write_text("module.json", BASE + R"(, "platforms": ["amiga"]})"));
        const std::filesystem::path path = directory.file("module.json");

        const result<module_manifest> loaded = load_module_manifest(path);
        CONJURE_CHECK_ERROR(loaded, module_error::MANIFEST_INVALID);
        REQUIRE_FALSE(loaded.has_value());
        const std::string message = loaded.get_error().message();
        CHECK(message.find(path.generic_string()) == 0);
        CHECK(message.find("/platforms/0") != std::string::npos);
        CHECK(message.find("amiga") != std::string::npos);
    }
}
