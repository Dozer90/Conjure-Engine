#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/entity_ref.h"

#include <string>
#include <vector>

// entity_ref.h (docs/design/04_editor.md 3.5): guids and hierarchy paths.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/entity_ref") {

    TEST_CASE("entity_ref::resolve - a guid string names its entity") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Named");
        const core::json::document reference = core::json::document::from_value(
          core::json::object({{"v", guid}}).root()["v"]
        );
        const core::result<ecs::entity> entity = entity_ref::resolve(reference.root(), editor.scene());
        CONJURE_REQUIRE_OK(entity);
        CHECK(editor.scene().name_of(entity.value()).view() == "Named");
        CHECK(entity_ref::looks_like_guid(guid));
        CHECK_FALSE(entity_ref::looks_like_guid("Root/Child"));
        const core::result<core::uuid> back = entity_ref::guid_of(editor.scene(), entity.value());
        CONJURE_REQUIRE_OK(back);
        CHECK(back.value().to_string() == guid);
        CHECK(entity_ref::to_json(back.value()) == guid);
    }

    TEST_CASE("entity_ref::resolve - a path walks names from the roots") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string child = editor.create_entity("Child", root);
        const std::string grandchild = editor.create_entity("Leaf", child);
        const core::json::document reference = core::json::document::from_value(
          core::json::object({{"v", "Root/Child/Leaf"}}).root()["v"]
        );
        const core::result<ecs::entity> entity = entity_ref::resolve(reference.root(), editor.scene());
        CONJURE_REQUIRE_OK(entity);
        CHECK(entity_ref::guid_of(editor.scene(), entity.value()).value().to_string() == grandchild);
        CHECK(entity_ref::path_of(editor.scene(), entity.value()) == "Root/Child/Leaf");
        const core::json::document missing = core::json::document::from_value(
          core::json::object({{"v", "Root/Nope"}}).root()["v"]
        );
        CONJURE_CHECK_ERROR(entity_ref::resolve(missing.root(), editor.scene()), protocol_error_code::ENTITY_NOT_FOUND);
        const core::json::document number = core::json::document::from_value(
          core::json::object({{"v", 12}}).root()["v"]
        );
        CONJURE_CHECK_ERROR(entity_ref::resolve(number.root(), editor.scene()), protocol_error_code::INVALID_ARGS);
        const core::json::document empty = core::json::document::from_value(
          core::json::object({{"v", ""}}).root()["v"]
        );
        CONJURE_CHECK_ERROR(entity_ref::resolve(empty.root(), editor.scene()), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("entity_ref::resolve - an ambiguous path lists the candidates") {
        editor_test::headless_editor editor = {};
        const std::string first = editor.create_entity("Twin");
        const std::string second = editor.create_entity("Twin");
        const core::json::document reference = core::json::document::from_value(
          core::json::object({{"v", "Twin"}}).root()["v"]
        );
        core::json::document details = core::json::document::object();
        const core::result<ecs::entity> entity = entity_ref::resolve(reference.root(), editor.scene(), &details);
        CONJURE_CHECK_ERROR(entity, protocol_error_code::INVALID_ARGS);
        REQUIRE(details.root()["candidates"].size() == 2);
        CHECK(details.root()["candidates"][0].as_string() == first);
        CHECK(details.root()["candidates"][1].as_string() == second);
        // Through the protocol the details reach the response.
        const protocol_response response = editor.send("entity.get", core::json::object({{"entity", "Twin"}}));
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorDetails.root()["candidates"].size() == 2);
    }

    TEST_CASE("entity_ref::resolve - a stale guid is ENTITY_NOT_FOUND") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Gone");
        (void) editor.ok("entity.destroy", core::json::object({{"entity", guid}}));
        const core::json::document reference = core::json::document::from_value(
          core::json::object({{"v", guid}}).root()["v"]
        );
        CONJURE_CHECK_ERROR(
          entity_ref::resolve(reference.root(), editor.scene()),
          protocol_error_code::ENTITY_NOT_FOUND
        );
        const core::json::document unknown = core::json::document::from_value(
          core::json::object({{"v", core::uuid::generate().to_string()}}).root()["v"]
        );
        CONJURE_CHECK_ERROR(entity_ref::resolve(unknown.root(), editor.scene()), protocol_error_code::ENTITY_NOT_FOUND);
    }

    TEST_CASE("entity_ref::resolve_many - resolves arrays and rejects anything else") {
        editor_test::headless_editor editor = {};
        const std::string a = editor.create_entity("A");
        const std::string b = editor.create_entity("B");
        const core::json::document list = core::json::array({a, "B"});
        const core::result<std::vector<ecs::entity>> entities = entity_ref::resolve_many(list.root(), editor.scene());
        CONJURE_REQUIRE_OK(entities);
        REQUIRE(entities.value().size() == 2);
        CHECK(entity_ref::guid_of(editor.scene(), entities.value()[1]).value().to_string() == b);
        const core::json::document notList = core::json::object({{"x", 1}});
        CONJURE_CHECK_ERROR(
          entity_ref::resolve_many(notList.root(), editor.scene()),
          protocol_error_code::INVALID_ARGS
        );
        const core::json::document withMissing = core::json::array({a, "Nobody"});
        CONJURE_CHECK_ERROR(
          entity_ref::resolve_many(withMissing.root(), editor.scene()),
          protocol_error_code::ENTITY_NOT_FOUND
        );
    }

    TEST_CASE("entity_ref::guid_of - a dead entity is ENTITY_NOT_FOUND") {
        editor_test::headless_editor editor = {};
        CONJURE_CHECK_ERROR(
          entity_ref::guid_of(editor.scene(), ecs::NULL_ENTITY),
          protocol_error_code::ENTITY_NOT_FOUND
        );
    }
}
