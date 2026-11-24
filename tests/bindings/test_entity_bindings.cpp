#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/marshal.h"
#include "conjure/core/hash.h"
#include "conjure/core/string_id.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_runtime.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

// The hierarchy half of entity_* (docs/design/15_bindings.md 10): names, parents and children, enabled state,
// tags, layers, guids, duplication and bulk creation.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

TEST_SUITE("bindings/entity_bindings") {

    TEST_CASE("entity - set_name and the core find roundtrip") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity hero = fixture.create("placeholder");
        const utf8_argument name = utf8("hero");
        CHECK(api.entity_set_name(hero.to_u64(), name.data, name.length) == 0);
        CHECK(fixture.active().name_of(hero) == core::string_id("hero"));
        // The core entity_find_by_name / entity_get_name read the runtime's table, kept in step.
        CHECK(api.entity_find_by_name(name.data, name.length) == hero.to_u64());
        const std::string back = read_string([&](char *buffer, int32_t capacity) {
            return api.entity_get_name(hero.to_u64(), buffer, capacity);
        });
        CHECK(back == "hero");
        CHECK(api.entity_set_name(hero.to_u64(), nullptr, 0) == 0);              // "" clears the name
        CHECK(fixture.active().name_of(hero).is_null());
        CHECK(api.entity_set_name(hero.to_u64(), nullptr, 3) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_set_name(999, name.data, name.length) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("entity - parent, root and children follow the count protocol with a small buffer") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity root = fixture.create("root");
        const ecs::entity middle = fixture.create("middle", root);
        std::vector<ecs::entity> leaves = {};
        for (u32 index = 0; index < 5; ++index) {
            leaves.push_back(fixture.create("leaf", middle));
        }
        CHECK(api.entity_get_parent(leaves[0].to_u64()) == middle.to_u64());
        CHECK(api.entity_get_parent(root.to_u64()) == 0);
        CHECK(api.entity_get_root(leaves[3].to_u64()) == root.to_u64());
        CHECK(api.entity_get_root(root.to_u64()) == root.to_u64());
        std::array<uint64_t, 2> small = {};
        CHECK(api.entity_get_children(middle.to_u64(), small.data(), 2) == 5);
        CHECK(small[0] == leaves[0].to_u64());
        CHECK(small[1] == leaves[1].to_u64());
        CHECK(api.entity_get_children(middle.to_u64(), nullptr, 0) == 5);
        std::array<uint64_t, 8> wide = {};
        CHECK(api.entity_get_children(middle.to_u64(), wide.data(), 8) == 5);
        CHECK(wide[4] == leaves[4].to_u64());
        CHECK(wide[5] == 0);
        CHECK(api.entity_get_children(leaves[0].to_u64(), wide.data(), 8) == 0);
        CHECK(api.entity_get_children(middle.to_u64(), wide.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_get_children(999, wide.data(), 8) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(api.entity_get_parent(999) == 0);
        CHECK(api.entity_get_root(999) == 0);
        CHECK(last_error_code() == bindings_error::ENTITY_NOT_ALIVE);
    }

    TEST_CASE("entity - enabled toggles the disabled tag") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity parent = fixture.create("parent");
        const ecs::entity child = fixture.create("child", parent);
        CHECK(api.entity_is_enabled(parent.to_u64()) == 1);
        CHECK(api.entity_set_enabled(parent.to_u64(), 0) == 0);
        CHECK(api.entity_is_enabled(parent.to_u64()) == 0);
        CHECK(fixture.world.has<scene::disabled>(parent));
        CHECK(fixture.world.has<scene::inactive>(child));
        CHECK(api.entity_set_enabled(parent.to_u64(), 0) == 0);                 // no-op
        CHECK(api.entity_set_enabled(parent.to_u64(), 1) == 0);
        CHECK(api.entity_is_enabled(parent.to_u64()) == 1);
        CHECK_FALSE(fixture.world.has<scene::inactive>(child));
        CHECK(api.entity_is_enabled(999) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(api.entity_set_enabled(999, 1) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("entity - tag hash equals fnv1a64 and set_tag adds or rewrites the component") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity enemy = fixture.create("enemy");
        CHECK(api.entity_get_tag(enemy.to_u64()) == 0);
        const utf8_argument tag = utf8("Enemy");
        CHECK(api.entity_set_tag(enemy.to_u64(), tag.data, tag.length) == 0);
        CHECK(api.entity_get_tag(enemy.to_u64()) == core::hash::fnv1a64("Enemy"));
        CHECK(fixture.active().tag_of(enemy) == core::string_id("Enemy"));
        const utf8_argument other = utf8("Boss");
        CHECK(api.entity_set_tag(enemy.to_u64(), other.data, other.length) == 0);    // present: rewritten in place
        CHECK(api.entity_get_tag(enemy.to_u64()) == core::hash::fnv1a64("Boss"));
        CHECK(api.entity_set_tag(enemy.to_u64(), nullptr, 0) == 0);
        CHECK(api.entity_get_tag(enemy.to_u64()) == 0);
        CHECK(api.entity_set_tag(enemy.to_u64(), nullptr, 4) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_set_tag(999, tag.data, tag.length) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(api.entity_get_tag(999) == 0);
    }

    TEST_CASE("entity - get_tag_text returns the tag's text through the needed-length protocol") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity enemy = fixture.create("enemy");
        const uint64_t packed = enemy.to_u64();
        const auto readTag = [&api, packed](char *buffer, int32_t capacity) {
            return api.entity_get_tag_text(packed, buffer, capacity);
        };
        SUBCASE("an untagged entity answers the empty text") {
            CHECK(api.entity_get_tag_text(packed, nullptr, 0) == 0);
            CHECK(read_string(readTag).empty());
        }
        SUBCASE("a tagged entity answers its text, also into an exact buffer and as a length query") {
            const utf8_argument tag = utf8("Enemy");
            REQUIRE(api.entity_set_tag(packed, tag.data, tag.length) == 0);
            CHECK(read_string(readTag) == "Enemy");
            std::array<char, 5> exact = {};
            CHECK(api.entity_get_tag_text(packed, exact.data(), 5) == 5);
            CHECK(std::string_view(exact.data(), exact.size()) == "Enemy");
            CHECK(api.entity_get_tag_text(packed, nullptr, 0) == 5);
            CHECK(api.entity_get_tag_text(packed, nullptr, 8) == 5);
        }
        SUBCASE("a buffer that is too small gets the leading bytes and the full length") {
            const utf8_argument tag = utf8("Enemy");
            REQUIRE(api.entity_set_tag(packed, tag.data, tag.length) == 0);
            std::array<char, 4> small = {'-', '-', '-', '-'};
            CHECK(api.entity_get_tag_text(packed, small.data(), 2) == 5);
            CHECK(std::string_view(small.data(), small.size()) == "En--");
        }
        SUBCASE("a text longer than the 256-byte first read comes back whole on the second") {
            const std::string longTag = std::string(300, 'x') + "!";
            const utf8_argument tag = utf8(longTag);
            REQUIRE(api.entity_set_tag(packed, tag.data, tag.length) == 0);
            CHECK(read_string(readTag) == longTag);
        }
        SUBCASE("a removed tag answers the empty text again") {
            const utf8_argument tag = utf8("Enemy");
            REQUIRE(api.entity_set_tag(packed, tag.data, tag.length) == 0);
            REQUIRE(api.entity_set_tag(packed, nullptr, 0) == 0);
            CHECK(api.entity_get_tag_text(packed, nullptr, 0) == 0);
        }
        SUBCASE("a tag hash without interned text is NOT_FOUND, while entity_get_tag still has the hash") {
            // from_hash interns nothing; the text is spelled nowhere else in the engine or its tests.
            const u64 hash = core::hash::fnv1a64("entity_bindings.tag_text.never_interned");
            REQUIRE(core::string_id::from_hash(hash).view().empty());
            REQUIRE(fixture.active().set_tag(enemy, core::string_id::from_hash(hash)).has_value());
            std::array<char, 8> buffer = {};
            CHECK(api.entity_get_tag_text(packed, buffer.data(), 8) == to_status(bindings_error::NOT_FOUND));
            CHECK(last_error_code() == bindings_error::NOT_FOUND);
            CHECK(api.entity_get_tag(packed) == hash);
        }
        SUBCASE("a negative capacity is INVALID_ARGUMENT and a dead entity ENTITY_NOT_ALIVE") {
            std::array<char, 8> buffer = {};
            CHECK(api.entity_get_tag_text(packed, buffer.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
            CHECK(api.entity_get_tag_text(999, buffer.data(), 8) == to_status(bindings_error::ENTITY_NOT_ALIVE));
            CHECK(api.entity_get_tag_text(0, nullptr, 0) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        }
    }

    TEST_CASE("entity - layer set, get and find") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity a = fixture.create("a");
        const ecs::entity b = fixture.create("b");
        const ecs::entity c = fixture.create("c");
        CHECK(api.entity_get_layer(a.to_u64()) == 0);
        CHECK(api.entity_set_layer(a.to_u64(), 5) == 0);
        CHECK(api.entity_set_layer(b.to_u64(), 5) == 0);
        CHECK(api.entity_set_layer(c.to_u64(), 7) == 0);
        CHECK(api.entity_set_layer(c.to_u64(), 9) == 0);          // present: rewritten
        CHECK(api.entity_get_layer(a.to_u64()) == 5);
        CHECK(api.entity_get_layer(c.to_u64()) == 9);
        std::array<uint64_t, 4> found = {};
        CHECK(api.entity_find_by_layer(5, found.data(), 4) == 2);
        CHECK(api.entity_find_by_layer(9, found.data(), 4) == 1);
        CHECK(found[0] == c.to_u64());
        CHECK(api.entity_find_by_layer(11, found.data(), 4) == 0);
        CHECK(api.entity_set_layer(a.to_u64(), 32) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_set_layer(a.to_u64(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_find_by_layer(32, found.data(), 4) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_find_by_layer(5, found.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_get_layer(999) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("entity - guid get and find") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity marked = fixture.create("marked");
        conjure_uuid guid = {};
        CHECK(api.entity_get_guid(marked.to_u64(), &guid) == 0);
        CHECK((guid.high != 0 || guid.low != 0));
        CHECK(api.entity_find_by_guid(&guid) == marked.to_u64());
        const conjure_uuid unknown = uuid_of(test_uuid(77));
        CHECK(api.entity_find_by_guid(&unknown) == 0);
        CHECK(api.entity_find_by_guid(nullptr) == 0);
        CHECK(last_error_code() == bindings_error::INVALID_ARGUMENT);
        CHECK(api.entity_get_guid(marked.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_get_guid(999, &guid) == to_status(bindings_error::ENTITY_NOT_ALIVE));

        // A raw ECS entity without a guid: the getter is a read and reports COMPONENT_MISSING instead of
        // growing one (C# Entity.Guid then reads AssetId.Null); scene::ensure_guid is the way to mint one.
        const core::result<ecs::entity> raw = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(raw);
        CHECK(api.entity_get_guid(raw.value().to_u64(), &guid) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(last_error_message() == "entity_get_guid: guid");
        CHECK_FALSE(fixture.world.has<scene::guid>(raw.value()));
        CONJURE_REQUIRE_OK(fixture.active().ensure_guid(raw.value()));
        CHECK(api.entity_get_guid(raw.value().to_u64(), &guid) == 0);
        CHECK(api.entity_find_by_guid(&guid) == raw.value().to_u64());
    }

    TEST_CASE("regression - find_by_tag hashes the queried string without interning it") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        constexpr std::string_view NEVER_INTERNED = "never_interned_tag_7f3a";
        const utf8_argument query = utf8(NEVER_INTERNED);
        std::array<uint64_t, 2> found = {};
        CHECK(api.entity_find_by_tag(query.data, query.length, found.data(), 2) == 0);
        CHECK(core::string_id::from_hash(core::hash::fnv1a64(NEVER_INTERNED)).view().empty());
        // set_tag interns (tools must see the text), and the lookup then finds it by hash alone.
        const ecs::entity tagged = fixture.create("tagged");
        REQUIRE(api.entity_set_tag(tagged.to_u64(), query.data, query.length) == 0);
        CHECK(core::string_id::from_hash(core::hash::fnv1a64(NEVER_INTERNED)).view() == NEVER_INTERNED);
        CHECK(api.entity_find_by_tag(query.data, query.length, found.data(), 2) == 1);
        CHECK(found[0] == tagged.to_u64());
    }

    TEST_CASE("entity - find_by_tag counts without a buffer") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const utf8_argument tag = utf8("Pickup");
        std::vector<ecs::entity> pickups = {};
        for (u32 index = 0; index < 7; ++index) {
            const ecs::entity pickup = fixture.create("pickup");
            REQUIRE(api.entity_set_tag(pickup.to_u64(), tag.data, tag.length) == 0);
            pickups.push_back(pickup);
        }
        const ecs::entity other = fixture.create("other");
        const utf8_argument otherTag = utf8("Other");
        REQUIRE(api.entity_set_tag(other.to_u64(), otherTag.data, otherTag.length) == 0);
        CHECK(api.entity_find_by_tag(tag.data, tag.length, nullptr, 0) == 7);
        std::array<uint64_t, 3> some = {};
        CHECK(api.entity_find_by_tag(tag.data, tag.length, some.data(), 3) == 7);
        CHECK(some[2] != 0);
        const utf8_argument nobody = utf8("Nobody");
        CHECK(api.entity_find_by_tag(nobody.data, nobody.length, some.data(), 3) == 0);
        CHECK(api.entity_find_by_tag(nullptr, 2, some.data(), 3) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.entity_find_by_tag(tag.data, tag.length, some.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT)
        );
    }

    TEST_CASE("entity - duplicate copies the subtree and deep-copies scripts") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        scripting::script_class_info info = {};
        info.fullName = "Game.Marker";
        info.hash = scripting::script_name_hash(info.fullName);
        fixture.null_host().add_fake_class(info);
        CONJURE_REQUIRE_OK(fixture.runtime->load_game_assembly({}));

        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const core::json::document fields = core::json::document::object();
        REQUIRE(fixture.runtime->add_script(child, info.hash, fields.root()).is_valid());
        fixture.runtime->flush_pending();
        const utf8_argument tag = utf8("Copyable");
        REQUIRE(api.entity_set_tag(root.to_u64(), tag.data, tag.length) == 0);

        const uint64_t copyPacked = api.entity_duplicate(root.to_u64());
        REQUIRE(copyPacked != 0);
        const ecs::entity copy = marshal::to_entity(copyPacked);
        CHECK(copy != root);
        CHECK(fixture.active().tree().parent_of(copy).is_null());
        REQUIRE(fixture.active().tree().children_of(copy).size() == 1);
        const ecs::entity copiedChild = fixture.active().tree().children_of(copy)[0];
        CHECK(api.entity_get_tag(copyPacked) == core::hash::fnv1a64("Copyable"));
        fixture.runtime->flush_pending();
        CHECK(fixture.runtime->scripts_of(copiedChild).size() == 1);
        CHECK(fixture.runtime->scripts_of(copiedChild)[0] != fixture.runtime->scripts_of(child)[0]);
        conjure_uuid original = {};
        conjure_uuid duplicate = {};
        REQUIRE(api.entity_get_guid(root.to_u64(), &original) == 0);
        REQUIRE(api.entity_get_guid(copyPacked, &duplicate) == 0);
        CHECK((original.high != duplicate.high || original.low != duplicate.low));
        CHECK(api.entity_duplicate(999) == 0);
        CHECK(last_error_code() == bindings_error::ENTITY_NOT_ALIVE);
    }

    TEST_CASE("entity - create_many parents every entity") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity parent = fixture.create("parent");
        std::array<uint64_t, 6> created = {};
        CHECK(api.entity_create_many(6, parent.to_u64(), created.data()) == 6);
        for (const uint64_t packed : created) {
            CAPTURE(packed);
            REQUIRE(packed != 0);
            CHECK(api.entity_get_parent(packed) == parent.to_u64());
            CHECK(fixture.world.has<scene::local_transform>(marshal::to_entity(packed)));
        }
        CHECK(fixture.active().tree().children_of(parent).size() == 6);
        std::array<uint64_t, 2> roots = {};
        CHECK(api.entity_create_many(2, 0, roots.data()) == 2);
        CHECK(api.entity_get_parent(roots[0]) == 0);
        CHECK(api.entity_create_many(0, 0, roots.data()) == 0);
        CHECK(api.entity_create_many(-1, 0, roots.data()) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_create_many(2, 0, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.entity_create_many(2, 999, roots.data()) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }
}
