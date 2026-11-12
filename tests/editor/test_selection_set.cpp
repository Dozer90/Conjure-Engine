#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/uuid.h"
#include "conjure/editor/selection_set.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

// selection_set.h: guid set operations, the change event and the rebuild against a live scene.

using namespace conjure;
using namespace conjure::editor;

namespace {
    core::uuid make_guid(u8 seed) {
        return core::uuid(
          static_cast<u64>(seed) * 0x0101010101010101ull,
          static_cast<u64>(seed) * 0x0101010101010101ull
        );
    }
}

TEST_SUITE("editor/selection_set") {

    TEST_CASE("selection_set - set, add, remove and clear keep a sorted unique set") {
        selection_set selection = {};
        CHECK(selection.empty());
        CHECK(selection.primary().is_null());
        const std::array<core::uuid, 3> guids = {make_guid(3), make_guid(1), make_guid(2)};
        selection.set(guids);
        REQUIRE(selection.size() == 3);
        CHECK(selection.guids()[0] == make_guid(1));
        CHECK(selection.primary() == make_guid(2));
        CHECK(selection.contains(make_guid(2)));
        const std::array<core::uuid, 1> extra = {make_guid(9)};
        selection.add(extra);
        CHECK(selection.size() == 4);
        const std::array<core::uuid, 2> gone = {make_guid(1), make_guid(9)};
        selection.remove(gone);
        CHECK(selection.size() == 2);
        CHECK_FALSE(selection.contains(make_guid(1)));
        selection.clear();
        CHECK(selection.empty());
    }

    TEST_CASE("selection_set - duplicates and null guids are ignored") {
        selection_set selection = {};
        const std::array<core::uuid, 4> guids = {make_guid(4), make_guid(4), core::uuid::null(), make_guid(5)};
        selection.set(guids);
        CHECK(selection.size() == 2);
        selection.add(guids);
        CHECK(selection.size() == 2);
    }

    TEST_CASE("selection_set - on_changed fires once per effective change") {
        selection_set selection = {};
        int changes = 0;
        core::event_subscription token = selection.on_changed().subscribe([&changes]() {
            ++changes;
        });
        const std::array<core::uuid, 2> guids = {make_guid(1), make_guid(2)};
        selection.set(guids);
        CHECK(changes == 1);
        selection.set(guids);
        CHECK(changes == 1);
        selection.add(guids);
        CHECK(changes == 1);
        const std::array<core::uuid, 1> missing = {make_guid(7)};
        selection.remove(missing);
        CHECK(changes == 1);
        selection.remove(guids);
        CHECK(changes == 2);
        selection.clear();
        CHECK(changes == 2);
        token.release();
    }

    TEST_CASE("regression - F657 selection_set primary is the guid named last, else the lowest one left") {
        selection_set selection = {};
        int changes = 0;
        core::event_subscription token = selection.on_changed().subscribe([&changes]() {
            ++changes;
        });
        const std::array<core::uuid, 3> guids = {make_guid(3), make_guid(1), make_guid(2)};
        selection.set(guids);
        CHECK(selection.primary() == make_guid(2));
        const std::array<core::uuid, 2> extra = {make_guid(9), core::uuid::null()};
        selection.add(extra);
        CHECK(selection.primary() == make_guid(9));
        const std::array<core::uuid, 1> nine = {make_guid(9)};
        selection.remove(nine);
        CHECK(selection.primary() == make_guid(1));

        // The same guids with another one named last: only the primary changes, and that is a change.
        changes = 0;
        const std::array<core::uuid, 3> reordered = {make_guid(1), make_guid(2), make_guid(3)};
        selection.set(reordered);
        CHECK(selection.primary() == make_guid(3));
        CHECK(changes == 1);
        selection.set(reordered);
        CHECK(changes == 1);

        // Naming a guid that is already selected makes it the primary without adding anything.
        const std::array<core::uuid, 1> one = {make_guid(1)};
        selection.add(one);
        CHECK(selection.primary() == make_guid(1));
        CHECK(selection.size() == 3);
        CHECK(changes == 2);

        // Removing a guid that is not the primary keeps the primary.
        const std::array<core::uuid, 1> two = {make_guid(2)};
        selection.remove(two);
        CHECK(selection.primary() == make_guid(1));
        selection.clear();
        CHECK(selection.primary().is_null());
        token.release();
    }

    TEST_CASE("selection_set - a rebuild that drops the primary falls back to the lowest live guid") {
        editor_test::headless_editor editor = {};
        const std::optional<core::uuid> first = core::uuid::parse(editor.create_entity("first"));
        const std::optional<core::uuid> second = core::uuid::parse(editor.create_entity("second"));
        REQUIRE(first.has_value());
        REQUIRE(second.has_value());
        selection_set selection = {};
        const std::array<core::uuid, 3> guids = {*first, *second, make_guid(0x42)};
        selection.set(guids);
        CHECK(selection.primary() == make_guid(0x42));
        selection.rebuild(editor.scene());
        CHECK(selection.size() == 2);
        CHECK(selection.primary() == std::min(*first, *second));
    }

    TEST_CASE("selection_set - rebuild marks live entities and drops dead guids") {
        editor_test::headless_editor editor = {};
        const std::string alive = editor.create_entity("alive");
        const std::string doomed = editor.create_entity("doomed");
        const std::optional<core::uuid> aliveGuid = core::uuid::parse(alive);
        const std::optional<core::uuid> doomedGuid = core::uuid::parse(doomed);
        REQUIRE(aliveGuid.has_value());
        REQUIRE(doomedGuid.has_value());
        selection_set selection = {};
        const std::array<core::uuid, 3> guids = {*aliveGuid, *doomedGuid, make_guid(0x42)};
        selection.set(guids);
        selection.rebuild(editor.scene());
        CHECK(selection.size() == 2);
        CHECK(selection.contains(editor.scene().find_guid(*aliveGuid)));
        CHECK_FALSE(selection.contains(ecs::NULL_ENTITY));
        (void) editor.ok("entity.destroy", core::json::object({{"entity", doomed}}));
        int changes = 0;
        core::event_subscription token = selection.on_changed().subscribe([&changes]() {
            ++changes;
        });
        selection.rebuild(editor.scene());
        CHECK(selection.size() == 1);
        CHECK(changes == 1);
        CHECK(selection.contains(*aliveGuid));
        token.release();
    }
}
