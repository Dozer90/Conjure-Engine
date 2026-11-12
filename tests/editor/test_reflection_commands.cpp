#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <filesystem>
#include <fstream>
#include <string>

// reflection.* and editor.schema (docs/design/04_editor.md D15): the agent's self-description, and the dump of the
// catalogue into docs/generated/editor_protocol.json that this test keeps in sync.

#ifndef CONJURE_EDITOR_PROTOCOL_DOCUMENT
    #define CONJURE_EDITOR_PROTOCOL_DOCUMENT ""
#endif

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/reflection_commands") {

    TEST_CASE("reflection.components - every editable component is listed with its fields") {
        editor_test::headless_editor editor = {};
        const core::json::document result = editor.ok("reflection.components");
        REQUIRE(result.root()["components"].size() > 0);
        CHECK(result.root()["count"].as_u64() == result.root()["components"].size());
        bool sprite = false;
        bool transform = false;
        for (const core::json::value component : result.root()["components"]) {
            CHECK(component["name"].is_string());
            CHECK(component["module"].is_string());
            CHECK(component["fields"].is_array());
            CHECK(component["aliases"].is_array());
            CHECK(component["editable"].as_bool());
            if (component["name"].as_string() == "conjure.renderer.sprite") {
                sprite = true;
                bool size = false;
                for (const core::json::value field : component["fields"]) {
                    if (field["name"].as_string() == "size") {
                        size = true;
                        CHECK(field["type"].as_string() == "VEC2");
                        CHECK(field.contains("default"));
                        CHECK(field["attributes"].is_object());
                    }
                }
                CHECK(size);
                CHECK(component["aliases"].size() >= 1);
            }
            if (component["name"].as_string() == "conjure.scene.local_transform") {
                transform = true;
            }
        }
        CHECK(sprite);
        CHECK(transform);
        const core::json::document noFields = editor.ok(
          "reflection.components",
          core::json::object({{"fields", false}})
        );
        CHECK_FALSE(noFields.root()["components"][0].contains("fields"));
        const core::json::document hidden = editor.ok(
          "reflection.components",
          core::json::object({{"includeHidden", true}})
        );
        CHECK(hidden.root()["count"].as_u64() >= result.root()["count"].as_u64());
    }

    TEST_CASE("reflection.type - resolves components by alias, structs and enumerations") {
        editor_test::headless_editor editor = {};
        const core::json::document camera = editor.ok("reflection.type", core::json::object({{"name", "camera"}}));
        CHECK(camera.root()["name"].as_string() == "conjure.renderer.camera");
        CHECK(camera.root()["type"]["fields"].size() > 0);
        bool projection = false;
        for (const core::json::value field : camera.root()["type"]["fields"]) {
            if (field["name"].as_string() == "cameraData") {
                for (const core::json::value sub : field["fields"]) {
                    if (sub["name"].as_string() == "projection") {
                        projection = true;
                        CHECK(sub["type"].as_string() == "ENUM");
                        CHECK(sub["enum"]["values"].size() == 2);
                    }
                }
            }
        }
        CHECK(projection);
        const core::json::document enumeration = editor.ok(
          "reflection.type",
          core::json::object({{"name", "conjure.renderer.projection_type"}})
        );
        CHECK(enumeration.root()["enum"]["values"].size() == 2);
        const core::json::document structure = editor.ok(
          "reflection.type",
          core::json::object({{"name", "conjure.renderer.camera_data"}})
        );
        CHECK(structure.root()["type"]["fields"].size() >= 5);
        CHECK(editor.fail(
          "reflection.type",
          core::json::object({{"name", "no.such.type"}})
        ) == protocol_error_code::TYPE_NOT_FOUND);
    }

    TEST_CASE("reflection.asset_types - lists the registered types and the importers seen in the project") {
        editor_test::headless_editor editor = {};
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "t.png");
        (void) editor.ok("asset.import", core::json::object({{"source", "t.png"}}));
        const core::json::document result = editor.ok("reflection.asset_types");
        REQUIRE(result.root()["types"].size() > 0);
        bool texture = false;
        for (const core::json::value type : result.root()["types"]) {
            CHECK(type["name"].is_string());
            CHECK(type["extensions"].is_array());
            CHECK(type["importOptionsSchema"]["type"].as_string() == "object");
            if (type["name"].as_string() == "conjure.renderer.texture") {
                texture = true;
                CHECK(type["importers"].size() == 1);
                CHECK(type["extensions"][0].as_string() == ".png");
            }
        }
        CHECK(texture);
        CHECK(result.root()["importers"].size() >= 1);
    }

    TEST_CASE("editor.schema - every command has a schema and the dump matches docs/generated/editor_protocol.json") {
        editor_test::headless_editor editor = {};
        const core::json::document schema = editor.ok("editor.schema");
        CHECK(schema.root()["protocolVersion"].as_i64() == PROTOCOL_VERSION);
        CHECK(schema.root()["commands"].size() == builtin_command_count());
        for (const core::json::value command : schema.root()["commands"]) {
            CHECK_MESSAGE(command["args"]["type"].as_string() == "object", command["name"].as_string());
            CHECK_MESSAGE(!command["summary"].as_string().empty(), command["name"].as_string());
            CHECK(command["flags"].is_array());
        }
        CHECK(schema.root()["events"].size() == 15);
        CHECK(schema.root()["errorCodes"].size() == all_error_codes().size());
        const core::json::document one = editor.ok("editor.schema", core::json::object({{"command", "entity.create"}}));
        CHECK(one.root()["name"].as_string() == "entity.create");
        CHECK(editor.fail(
          "editor.schema",
          core::json::object({{"command", "entity.nope"}})
        ) == protocol_error_code::UNKNOWN_COMMAND);

        const std::filesystem::path document = std::filesystem::path(CONJURE_EDITOR_PROTOCOL_DOCUMENT);
        REQUIRE_FALSE(document.empty());
        const std::string text = schema.to_string(true) + "\n";
        if (testing::environment().updateGoldens || !std::filesystem::exists(document)) {
            std::error_code ignored = {};
            std::filesystem::create_directories(document.parent_path(), ignored);
            std::ofstream output(document, std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << text;
            MESSAGE("wrote " << document.generic_string());
        }
        const std::string committed = editor_test::read_text_file(document);
        const core::result<core::json::document> parsed = core::json::document::parse(committed);
        CONJURE_REQUIRE_OK(parsed);
        CHECK_MESSAGE(
          parsed.value() == schema,
          "docs/generated/editor_protocol.json is stale: rerun conjure_editor_tests with --conjure-update-golden"
        );
        CHECK(parsed.value().root()["schemaHash"].as_string() == schema.root()["schemaHash"].as_string());
    }

    TEST_CASE("session.hello - reports the protocol version, engine version and schema hash") {
        editor_test::headless_editor editor = {};
        const core::json::document hello = editor.ok("session.hello", core::json::object({{"clientName", "test"}}));
        CHECK(hello.root()["protocolVersion"].as_i64() == PROTOCOL_VERSION);
        CHECK_FALSE(hello.root()["engineVersion"].as_string().empty());
        CHECK(hello.root()["schemaHash"].as_string() == editor.ok("editor.schema").root()["schemaHash"].as_string());
        CHECK(hello.root()["clientId"].as_u64() == editor.client_id());
        CHECK(hello.root()["headless"].as_bool());
        // The headless editor's client is in process (like an --exec file): --ai-token gates only the transports,
        // so a wrong or missing token does not fail its hello (F477).
        editor.context().set_token("secret");
        (void) editor.ok("session.hello", core::json::object({{"token", "wrong"}}));
        (void) editor.ok("session.hello");
        (void) editor.ok("session.hello", core::json::object({{"token", "secret"}}));
        const core::json::document pong = editor.ok("session.ping");
        CHECK(pong.root().contains("frame"));
        const core::json::document subscribed = editor.ok(
          "session.subscribe",
          core::json::object({{"events", core::json::array({"play.state"})}, {"minLogLevel", "warn"}})
        );
        CHECK(subscribed.root()["subscribed"].size() == 1);
        CHECK(editor.fail(
          "session.subscribe",
          core::json::object({{"events", core::json::array({"nope"})}})
        ) == protocol_error_code::INVALID_ARGS);
    }
}
