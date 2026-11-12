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
        cache.set_expanded(editor.scene(), entity_of(editor, root), true);
        CHECK(cache.is_expanded(editor.scene(), entity_of(editor, root)));
        CHECK(cache.refresh_if_dirty(editor.scene(), selection));
        REQUIRE(cache.rows().size() == 3);
        CHECK(cache.rows()[1].depth == 1);
        CHECK(cache.rows()[1].nameId.view() == "Child");
        cache.set_expanded(editor.scene(), entity_of(editor, root), true);
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
        cache.set_expanded(editor.scene(), ecs::NULL_ENTITY, true);
        CHECK_FALSE(cache.is_expanded(editor.scene(), ecs::NULL_ENTITY));
    }

    TEST_CASE("hierarchy_cache - refresh_labels picks up a rename and a disable without a rebuild") {
        editor_test::headless_editor editor = {};
        const std::string first = editor.create_entity("First");
        (void) editor.create_entity("Second");
        hierarchy_cache cache = {};
        selection_set selection = {};
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        REQUIRE(cache.rows().size() == 2);
        const u32 rebuilds = cache.rebuild_count();
        (void) editor.ok("entity.rename", core::json::object({{"entity", first}, {"name", "Renamed"}}));
        (void) editor.ok("entity.set_enabled", core::json::object({{"entity", first}, {"enabled", false}}));
        // Neither edit is structural: the cache keeps its rows until the labels are refreshed.
        CHECK_FALSE(cache.refresh_if_dirty(editor.scene(), selection));
        CHECK(cache.rows()[0].nameId.view() == "First");
        cache.refresh_labels(editor.scene(), 0, 1);
        CHECK(cache.rows()[0].nameId.view() == "Renamed");
        CHECK(has_flag(cache.rows()[0].flags, hierarchy_row_flags::DISABLED));
        (void) editor.ok("entity.set_enabled", core::json::object({{"entity", first}, {"enabled", true}}));
        // An end past the row count is clamped.
        cache.refresh_labels(editor.scene(), 0, 100);
        CHECK_FALSE(has_flag(cache.rows()[0].flags, hierarchy_row_flags::DISABLED));
        CHECK(cache.rows()[1].nameId.view() == "Second");
        CHECK(cache.rebuild_count() == rebuilds);
    }

    TEST_CASE("regression - expansion follows the guid and never leaks onto an entity that reuses the index") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("Parent");
        (void) editor.create_entity("Child", parent);
        hierarchy_cache cache = {};
        selection_set selection = {};
        const ecs::entity parentEntity = entity_of(editor, parent);
        const u32 parentIndex = parentEntity.index;
        cache.set_expanded(editor.scene(), parentEntity, true);
        REQUIRE(cache.is_expanded(editor.scene(), parentEntity));
        (void) editor.ok("entity.destroy", core::json::object({{"entity", parent}}));
        // Create until the ECS hands the parent's index out again, then give the newcomer a child.
        std::string reused = "";
        for (int attempt = 0; attempt < 64 && reused.empty(); ++attempt) {
            const std::string created = editor.create_entity("Newcomer");
            if (entity_of(editor, created).index == parentIndex) {
                reused = created;
            }
        }
        REQUIRE_FALSE(reused.empty());
        (void) editor.create_entity("NewChild", reused);
        CHECK_FALSE(cache.is_expanded(editor.scene(), entity_of(editor, reused)));
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        bool found = false;
        for (const hierarchy_row &row : cache.rows()) {
            if (row.entity == entity_of(editor, reused)) {
                found = true;
                CHECK(has_flag(row.flags, hierarchy_row_flags::HAS_CHILDREN));
                CHECK_FALSE(has_flag(row.flags, hierarchy_row_flags::EXPANDED));
            }
        }
        CHECK(found);
    }

    TEST_CASE("hierarchy_cache - expansion survives a play and stop snapshot restore") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("Parent");
        (void) editor.create_entity("Child", parent);
        hierarchy_cache cache = {};
        selection_set selection = {};
        cache.set_expanded(editor.scene(), entity_of(editor, parent), true);
        (void) editor.ok("play.start");
        (void) editor.ok("play.stop");
        CHECK(cache.is_expanded(editor.scene(), entity_of(editor, parent)));
        (void) cache.refresh_if_dirty(editor.scene(), selection);
        REQUIRE(cache.rows().size() == 2);
        CHECK(cache.rows()[0].nameId.view() == "Parent");
        CHECK(has_flag(cache.rows()[0].flags, hierarchy_row_flags::EXPANDED));
        CHECK(cache.rows()[1].nameId.view() == "Child");
    }
}
