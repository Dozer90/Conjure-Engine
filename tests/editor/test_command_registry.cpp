#include "conjure/testing/doctest.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <format>
#include <string>
#include <vector>

// command_registry.h: registration rules, lookup, description and the schema hash.

using namespace conjure;
using namespace conjure::editor;

namespace {
    command_descriptor make(std::string name, command_flags flags = command_flags::NONE, std::string owner = "editor") {
        command_descriptor descriptor = {};
        descriptor.name = std::move(name);
        descriptor.summary = "test command";
        descriptor.argsSchema = R"({"type":"object","properties":{"x":{"type":"integer"}}})";
        descriptor.flags = flags;
        descriptor.owner = std::move(owner);
        descriptor.handler = [](command_invocation &) -> core::result<command_outcome> {
            return command_outcome{};
        };
        return descriptor;
    }
}

TEST_SUITE("editor/command_registry") {

    TEST_CASE("command_registry - adds, finds and lists sorted by name") {
        command_registry registry = {};
        CONJURE_REQUIRE_OK(registry.add(make("zeta.run")));
        CONJURE_REQUIRE_OK(registry.add(make("alpha.run")));
        CHECK(registry.size() == 2);
        REQUIRE(registry.find("alpha.run") != nullptr);
        CHECK(registry.find("missing") == nullptr);
        const std::vector<std::string> names = registry.names();
        REQUIRE(names.size() == 2);
        CHECK(names[0] == "alpha.run");
        CHECK(names[1] == "zeta.run");
        CHECK(registry.args_schema("alpha.run") != nullptr);
        CHECK(registry.args_schema("missing") == nullptr);
    }

    TEST_CASE("command_registry - a duplicate name is rejected") {
        command_registry registry = {};
        CONJURE_REQUIRE_OK(registry.add(make("dup.one")));
        CONJURE_CHECK_ERROR(registry.add(make("dup.one")), protocol_error_code::INVALID_ARGS);
        CHECK(registry.size() == 1);
    }

    TEST_CASE("command_registry - a bad schema, an empty name and a missing handler are rejected") {
        command_registry registry = {};
        command_descriptor badSchema = make("bad.schema");
        badSchema.argsSchema = R"({"type":"object","patternProperties":{}})";
        CONJURE_CHECK_ERROR(registry.add(badSchema), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(registry.add(make("")), protocol_error_code::INVALID_ARGS);
        command_descriptor noHandler = make("no.handler");
        noHandler.handler = {};
        CONJURE_CHECK_ERROR(registry.add(noHandler), protocol_error_code::INVALID_ARGS);
        CHECK(registry.size() == 0);
    }

    TEST_CASE("command_registry - remove_owned_by drops a module's commands only") {
        command_registry registry = {};
        CONJURE_REQUIRE_OK(registry.add(make("core.keep")));
        CONJURE_REQUIRE_OK(registry.add(make("plugin.a", command_flags::NONE, "plugin")));
        CONJURE_REQUIRE_OK(registry.add(make("plugin.b", command_flags::NONE, "plugin")));
        CHECK(registry.remove_owned_by("plugin") == 2);
        CHECK(registry.size() == 1);
        CHECK(registry.find("core.keep") != nullptr);
        CHECK(registry.remove_owned_by("plugin") == 0);
        CHECK(registry.remove("core.keep"));
        CHECK_FALSE(registry.remove("core.keep"));
    }

    TEST_CASE("command_registry - async commands are never allowed inside a batch") {
        command_registry registry = {};
        CONJURE_REQUIRE_OK(registry.add(make("async.one", command_flags::ASYNC)));
        REQUIRE(registry.find("async.one") != nullptr);
        CHECK(has_command_flag(*registry.find("async.one"), command_flags::NOT_IN_BATCH));
    }

    TEST_CASE("command_registry - describe_all hides UI-only commands and reports events and codes") {
        command_registry registry = {};
        CONJURE_REQUIRE_OK(registry.add(make("public.one")));
        CONJURE_REQUIRE_OK(registry.add(make("ui.only", command_flags::UI_ONLY)));
        const core::json::document hidden = registry.describe_all(false);
        CHECK(hidden.root()["commands"].size() == 1);
        CHECK(hidden.root()["commands"][0]["name"].as_string() == "public.one");
        CHECK(hidden.root()["protocolVersion"].as_i64() == PROTOCOL_VERSION);
        CHECK(hidden.root()["events"].size() == protocol_event_names().size());
        CHECK(hidden.root()["errorCodes"].size() == all_error_codes().size());
        CHECK(hidden.root()["schemaHash"].as_string().size() == 16);
        const core::json::document shown = registry.describe_all(true);
        CHECK(shown.root()["commands"].size() == 2);
        const core::json::document one = registry.describe(*registry.find("public.one"));
        CHECK(one.root()["name"].as_string() == "public.one");
        CHECK(one.root()["owner"].as_string() == "editor");
        CHECK(one.root()["args"]["type"].as_string() == "object");
        CHECK(one.root()["flags"].is_array());
    }

    TEST_CASE("command_registry - the schema hash is stable and sensitive to the catalogue") {
        command_registry first = {};
        command_registry second = {};
        CONJURE_REQUIRE_OK(first.add(make("a.one")));
        CONJURE_REQUIRE_OK(first.add(make("b.two")));
        CONJURE_REQUIRE_OK(second.add(make("b.two")));
        CONJURE_REQUIRE_OK(second.add(make("a.one")));
        CHECK(first.schema_hash() == second.schema_hash());
        CHECK(first.schema_hash() != 0);
        const u64 before = first.schema_hash();
        CONJURE_REQUIRE_OK(first.add(make("c.three")));
        CHECK(first.schema_hash() != before);
        // UI-only commands do not take part in the agent-facing hash.
        const u64 withoutUi = first.schema_hash();
        CONJURE_REQUIRE_OK(first.add(make("ui.panel", command_flags::UI_ONLY)));
        CHECK(first.schema_hash() == withoutUi);
    }

    TEST_CASE("regression - F496 command_registry schema_hash follows changes made after an earlier read") {
        command_registry registry = {};
        const u64 empty = registry.schema_hash();
        CONJURE_REQUIRE_OK(registry.add(make("core.keep")));
        const u64 first = registry.schema_hash();
        CHECK(first != empty);
        CHECK(registry.describe_all(false).root()["schemaHash"].as_string() == std::format("{:016x}", first));
        CONJURE_REQUIRE_OK(registry.add(make("plugin.a", command_flags::NONE, "plugin")));
        const u64 withPlugin = registry.schema_hash();
        CHECK(withPlugin != first);
        CHECK(registry.describe_all(false).root()["schemaHash"].as_string() == std::format("{:016x}", withPlugin));
        CHECK(registry.remove("plugin.a"));
        CHECK(registry.schema_hash() == first);
        CONJURE_REQUIRE_OK(registry.add(make("plugin.b", command_flags::NONE, "plugin")));
        CHECK(registry.schema_hash() != first);
        CHECK(registry.remove_owned_by("plugin") == 1);
        CHECK(registry.schema_hash() == first);
        CHECK(registry.remove_owned_by("nobody") == 0);
        CHECK(registry.schema_hash() == first);
        CHECK(registry.remove("core.keep"));
        CHECK(registry.schema_hash() == empty);
    }

    TEST_CASE("command_registry - closest_names suggests the nearest spellings") {
        command_registry registry = {};
        register_builtin_commands(registry);
        const std::vector<std::string> closest = registry.closest_names("entity.craete", 3);
        REQUIRE_FALSE(closest.empty());
        CHECK(closest.front() == "entity.create");
        CHECK(registry.closest_names("x", 0).empty());
    }

    TEST_CASE("regression - F102 command_registry closest_names of an over-long name suggests nothing") {
        command_registry registry = {};
        register_builtin_commands(registry);
        // 128 bytes still gets suggestions; one byte more is no command's spelling.
        const std::string longest = "entity.create" + std::string(128 - 13, 'x');
        REQUIRE(longest.size() == 128);
        CHECK_FALSE(registry.closest_names(longest, 3).empty());
        CHECK(registry.closest_names(longest + "x", 3).empty());
        CHECK(registry.closest_names(std::string(1024 * 1024, 'e'), 3).empty());
    }

    TEST_CASE("protocol_event_names - lists every event once") {
        const std::span<const std::string_view> names = protocol_event_names();
        CHECK(names.size() == 15);
        bool sawLog = false;
        for (const std::string_view name : names) {
            sawLog = sawLog || name == "log";
        }
        CHECK(sawLog);
    }
}
