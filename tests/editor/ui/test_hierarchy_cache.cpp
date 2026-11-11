#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/selection_set.h"
#include "conjure/editor/ui/hierarchy_cache.h"

#include <string>

// hierarchy_cache.h (docs/design/04_editor.md D11, 2.5).

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

namespace {
    ecs::entity entity_of(editor_test::headless_editor &editor, const std::string &guid) {
        return editor.scene().find_guid(*core::uuid::parse(guid));
    }
}

TEST_SUITE("editor/ui/hierarchy_cache") {

    TEST_CASE("hierarchy_cache - rebuilds only when the structure version, expansion or filter changes") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        (void) editor.create_entity("Child", root);
        hierarchy_cache cache = {};
        selection_set selection = {};
        CHECK(cache.refresh_if_dirty(editor.scene(), selection));
        CHECK(cache.rebuild_count() == 1);
        CHECK(cache.structure_version() == editor.scene().structure_version());
        // Collapsed roots show one row.
        REQUIRE(cache.rows().size() == 1);
        CHECK(has_flag(cache.rows()[0].flags, hierarchy_row_flags::HAS_CHILDREN));
        CHECK_FALSE(has_flag(cache.rows()[0].flags, hierarchy_row_flags::EXPANDED));
        CHECK_FALSE(cache.refresh_if_dirty(editor.scene(), selection));
        CHECK(cache.rebuild_count() == 1);
        (void) editor.create_entity("Sibling");
        CHECK(cache.refresh_if_dirty(editor.scene(), selection));
        CHECK(cache.rebuild_count() == 2);
        CHECK(cache.rows().size() == 2);
        cache.set_expanded(entity_of(editor, root), true);
        CHECK(cache.is_expanded(entity_of(editor, root)));
        CHECK(cache.refresh_if_dirty(editor.scene(), selection));
        REQUIRE(cache.rows().size() == 3);
        CHECK(cache.rows()[1].depth == 1);
        CHECK(cache.rows()[1].nameId.view() == "Child");
        cache.set_expanded(entity_of(editor, root), true);
        CHECK_FALSE(cache.refresh_if_dirty(editor.scene(), selection));
        cache.invalidate();
        CHECK(cache.refresh_if_dirty(editor.scene(), selection));
    }

    TEST_CASE("hierarchy_cache - the filter keeps the ancestors of matching entities and expands them") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string branch = editor.create_entity("Branch", root);
        (void) editor.create_entity("needle", branch);
        (void) editor.create_entity("Other", root);
        (void) editor.create_entity("Unrelated");
        hierarchy_cache cache = {};
        selection_set selection = {};
        cache.set_filter("NEEDLE");
        CHECK(cache.filter() == "NEEDLE");
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        REQUIRE(cache.rows().size() == 3);
        CHECK(cache.rows()[0].nameId.view() == "Root");
        CHECK(cache.rows()[1].nameId.view() == "Branch");
        CHECK(cache.rows()[2].nameId.view() == "needle");
        CHECK(cache.rows()[2].depth == 2);
        CHECK(has_flag(cache.rows()[0].flags, hierarchy_row_flags::EXPANDED));
        cache.set_filter("  ");
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        CHECK(cache.rows().size() == 2);
        cache.set_filter("zzz");
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        CHECK(cache.rows().empty());
    }

    TEST_CASE("hierarchy_cache - 10,000 entities produce a flat row list with the selection and state flags") {
        editor_test::headless_editor editor = {};
        const core::json::document created = editor.ok("entity.create_many", core::json::object({{"count", 10000}}));
        const std::string first = std::string(created.root()["guids"][0].as_string());
        (void) editor.ok("entity.set_enabled", core::json::object({{"entity", first}, {"enabled", false}}));
        selection_set selection = {};
        const std::array<core::uuid, 1> guids = {*core::uuid::parse(first)};
        selection.set(guids);
        selection.rebuild(editor.scene());
        hierarchy_cache cache = {};
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        REQUIRE(cache.rows().size() == 10000);
        CHECK(cache.rows()[0].depth == 0);
        CHECK(cache.rows()[9999].depth == 0);
        CHECK(has_flag(cache.rows()[0].flags, hierarchy_row_flags::SELECTED));
        CHECK(has_flag(cache.rows()[0].flags, hierarchy_row_flags::DISABLED));
        CHECK_FALSE(has_flag(cache.rows()[1].flags, hierarchy_row_flags::SELECTED));
        CHECK_FALSE(has_flag(cache.rows()[0].flags, hierarchy_row_flags::HAS_CHILDREN));
        CHECK(cache.rebuild_count() == 1);
    }

    TEST_CASE("hierarchy_cache - reveal expands every ancestor and prefab instances are flagged") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string mid = editor.create_entity("Mid", root);
        const std::string leaf = editor.create_entity("Leaf", mid);
        (void) editor.ok("prefab.create", core::json::object({{"entity", root}, {"path", "prefabs/tree"}}));
        hierarchy_cache cache = {};
        selection_set selection = {};
        cache.reveal(editor.scene(), entity_of(editor, leaf));
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        REQUIRE(cache.rows().size() == 3);
        CHECK(has_flag(cache.rows()[0].flags, hierarchy_row_flags::PREFAB_INSTANCE));
        CHECK(cache.rows()[2].nameId.view() == "Leaf");
        cache.set_expanded(ecs::NULL_ENTITY, true);
        CHECK_FALSE(cache.is_expanded(ecs::NULL_ENTITY));
    }
}
