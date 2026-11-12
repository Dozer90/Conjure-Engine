#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"

#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/world.h"
#include "conjure/ecs/world_hash.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace hash_test {
    /// <summary>
    /// A trivially copyable component with three padding bytes between its fields.
    /// </summary>
    struct padded_component {
        u8 flag = 0;
        u32 value = 0;
    };

    /// <summary>
    /// The layout of assets::asset_ref (R25): the uuid, then the slot pointer the manager binds.
    /// </summary>
    struct fake_asset_ref {
        core::uuid id = {};
        const void *slot = nullptr;
    };

    /// <summary>
    /// One reference, an array of two and a plain value after them.
    /// </summary>
    struct sprite_like_component {
        fake_asset_ref texture = {};
        std::array<fake_asset_ref, 2> layers = {};
        u32 tint = 0;
    };

    /// <summary>
    /// A nested struct that holds a reference.
    /// </summary>
    struct material_slot {
        fake_asset_ref material = {};
        u32 pass = 0;
    };

    /// <summary>
    /// The nested struct as a field and as an array of structs.
    /// </summary>
    struct mesh_like_component {
        material_slot primary = {};
        std::array<material_slot, 2> extras = {};
    };

    /// <summary>
    /// An asset reference handle that is the uuid and nothing else.
    /// </summary>
    struct bare_asset_ref {
        core::uuid id = {};
    };

    struct bare_ref_component {
        bare_asset_ref clip = {};
    };

    /// <summary>
    /// Registered by hand without TRIVIAL, so world_hash hashes its serialised text.
    /// </summary>
    struct long_text_component {
        u32 value = 0;
    };
}

template <>
struct conjure::core::reflect::reflect_kind_override<hash_test::fake_asset_ref> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ASSET_REF;
    static constexpr std::string_view NAME = "test.hash_asset_ref";
};

template <>
struct conjure::core::reflect::reflect_kind_override<hash_test::bare_asset_ref> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ASSET_REF;
    static constexpr std::string_view NAME = "test.hash_bare_asset_ref";
};

CONJURE_REFLECT_STRUCT(hash_test::padded_component, "test.padded", CONJURE_FIELD(flag), CONJURE_FIELD(value));
CONJURE_COMPONENT(hash_test::padded_component, "test.padded");
CONJURE_REFLECT_STRUCT(
  hash_test::sprite_like_component,
  "test.sprite_like",
  CONJURE_FIELD(texture),
  CONJURE_FIELD(layers),
  CONJURE_FIELD(tint)
);
CONJURE_COMPONENT(hash_test::sprite_like_component, "test.sprite_like");
CONJURE_REFLECT_STRUCT(hash_test::material_slot, "test.material_slot", CONJURE_FIELD(material), CONJURE_FIELD(pass));
CONJURE_REFLECT_STRUCT(hash_test::mesh_like_component, "test.mesh_like", CONJURE_FIELD(primary), CONJURE_FIELD(extras));
CONJURE_COMPONENT(hash_test::mesh_like_component, "test.mesh_like");
CONJURE_REFLECT_STRUCT(hash_test::bare_ref_component, "test.bare_ref", CONJURE_FIELD(clip));
CONJURE_COMPONENT(hash_test::bare_ref_component, "test.bare_ref");

namespace {
    /// <summary>
    /// Builds the same content in two worlds through different archetype creation orders. Both worlds
    /// run the same create/destroy/clear sequence first so their entity ids match exactly; only the
    /// masks of the scratch entities differ, which is what orders the archetypes differently.
    /// </summary>
    void populate(ecs_fixture &fixture, bool velocityFirst) {
        world &world = fixture.world;
        const component_mask firstScratch = velocityFirst ? fixture.mask<velocity>() : fixture.mask<position>();
        const component_mask secondScratch = fixture.mask<position, velocity>();
        const core::result<entity> scratch = world.create_entity(firstScratch);
        REQUIRE(scratch.has_value());
        REQUIRE(world.destroy_entity(scratch.value()).has_value());
        const core::result<entity> scratch2 = world.create_entity(secondScratch);
        REQUIRE(scratch2.has_value());
        REQUIRE(world.destroy_entity(scratch2.value()).has_value());
        world.clear();
        for (u32 index = 0; index < 100; ++index) {
            const core::result<entity> created = world.create_entity(fixture.mask<position>());
            REQUIRE(created.has_value());
            REQUIRE(world.set(created.value(), position{static_cast<f32>(index), 0.0f, 0.0f}).has_value());
        }
        for (u32 index = 0; index < 50; ++index) {
            const core::result<entity> created = world.create_entity(fixture.mask<position, velocity>());
            REQUIRE(created.has_value());
            REQUIRE(world.set(created.value(), velocity{1.0f, static_cast<f32>(index), 0.0f}).has_value());
        }
        for (u32 index = 0; index < 20; ++index) {
            const core::result<entity> created = world.create_entity(fixture.mask<velocity, player_tag>());
            REQUIRE(created.has_value());
        }
    }
}

TEST_SUITE("ecs/world_hash") {

    TEST_CASE("world_hash - empty world hashes to a constant and content changes it") {
        ecs_fixture fixture = {};
        const u64 empty = world_hash(fixture.world);
        ecs_fixture other = {};
        CHECK(world_hash(other.world) == empty);
        CONJURE_REQUIRE_OK(fixture.world.create_entity());
        CHECK(world_hash(fixture.world) != empty);
    }

    TEST_CASE("world_hash - independent of archetype creation order") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        populate(a, false);
        populate(b, true);
        const bool layoutsDiffer = a.world.archetypes().size() != b.world.archetypes().size() ||
          a.world.archetypes()[1]->mask() != b.world.archetypes()[1]->mask();
        CHECK(layoutsDiffer);
        CHECK(world_hash(a.world) == world_hash(b.world));
    }

    TEST_CASE("world_hash - one float bit flips it") {
        ecs_fixture fixture = {};
        populate(fixture, false);
        const u64 before = world_hash(fixture.world);
        CHECK(world_hash(fixture.world) == before);

        position *target = nullptr;
        fixture.world.view<position>().for_each([&](entity, position &value) {
            if (target == nullptr) {
                target = &value;
            }
        });
        REQUIRE(target != nullptr);
        const u32 bits = std::bit_cast<u32>(target->z);
        target->z = std::bit_cast<f32>(bits ^ 1u);
        CHECK(world_hash(fixture.world) != before);
        target->z = std::bit_cast<f32>(bits);
        CHECK(world_hash(fixture.world) == before);

        // Negative zero differs from zero in the bits, so it differs in the hash.
        target->z = -0.0f;
        CHECK(world_hash(fixture.world) != before);
    }

    TEST_CASE("world_hash - entity ids and tags take part") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        CONJURE_REQUIRE_OK(a.world.create_entity(a.mask<position>()));
        CONJURE_REQUIRE_OK(b.world.create_entity(b.mask<position>()));
        CHECK(world_hash(a.world) == world_hash(b.world));

        // Same data, different id (generation 2 instead of 1).
        const core::result<entity> extra = b.world.create_entity(b.mask<position>());
        CONJURE_REQUIRE_OK(extra);
        CONJURE_REQUIRE_OK(b.world.destroy_entity(extra.value()));
        CONJURE_REQUIRE_OK(b.world.create_entity(b.mask<position>()));
        const core::result<entity> extraA = a.world.create_entity(a.mask<position>());
        CONJURE_REQUIRE_OK(extraA);
        CHECK(a.world.entity_count() == b.world.entity_count());
        CHECK(world_hash(a.world) != world_hash(b.world));

        // A tag changes the archetype and therefore the hash.
        ecs_fixture c = {};
        ecs_fixture d = {};
        CONJURE_REQUIRE_OK(c.world.create_entity(c.mask<position>()));
        CONJURE_REQUIRE_OK(d.world.create_entity(d.mask<position, player_tag>()));
        CHECK(world_hash(c.world) != world_hash(d.world));
    }

    TEST_CASE("world_hash - non trivial components hash their content") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        const core::result<entity> first = a.world.create_entity(a.mask<name_component>());
        const core::result<entity> second = b.world.create_entity(b.mask<name_component>());
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        CONJURE_REQUIRE_OK(a.world.set(first.value(), name_component{"same"}));
        CONJURE_REQUIRE_OK(b.world.set(second.value(), name_component{"same"}));
        CHECK(world_hash(a.world) == world_hash(b.world));
        CONJURE_REQUIRE_OK(b.world.set(second.value(), name_component{"other"}));
        CHECK(world_hash(a.world) != world_hash(b.world));
    }

    TEST_CASE("regression - review 13 padding bytes do not affect the hash") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        CONJURE_REQUIRE_OK(a.registry.register_component<hash_test::padded_component>());
        CONJURE_REQUIRE_OK(b.registry.register_component<hash_test::padded_component>());
        const component_id paddedA = a.id<hash_test::padded_component>();
        const component_id paddedB = b.id<hash_test::padded_component>();
        const core::result<entity> first = a.world.create_entity();
        const core::result<entity> second = b.world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);

        // Same fields, different padding bytes: the raw copy brings the padding into the cell.
        std::array<std::byte, sizeof(hash_test::padded_component)> dirty = {};
        dirty.fill(std::byte{0xFF});
        std::array<std::byte, sizeof(hash_test::padded_component)> clean = {};
        const u8 flag = 1;
        const u32 value = 7;
        for (std::array<std::byte, sizeof(hash_test::padded_component)> *image : {&dirty, &clean}) {
            std::memcpy(image->data() + offsetof(hash_test::padded_component, flag), &flag, sizeof(flag));
            std::memcpy(image->data() + offsetof(hash_test::padded_component, value), &value, sizeof(value));
        }
        CONJURE_REQUIRE_OK(a.world.add_raw(first.value(), paddedA, dirty));
        CONJURE_REQUIRE_OK(b.world.add_raw(second.value(), paddedB, clean));
        CHECK(std::memcmp(a.world.get_raw(first.value(), paddedA), b.world.get_raw(second.value(), paddedB), 8) != 0);
        CHECK(world_hash(a.world) == world_hash(b.world));

        // The fields themselves still count.
        hash_test::padded_component changed = {};
        changed.flag = 1;
        changed.value = 8;
        CONJURE_REQUIRE_OK(b.world.set(second.value(), changed));
        CHECK(world_hash(a.world) != world_hash(b.world));
    }

    TEST_CASE("regression - demo-review P1 a bound and an unbound asset reference hash equal") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        CONJURE_REQUIRE_OK(a.registry.register_component<hash_test::sprite_like_component>());
        CONJURE_REQUIRE_OK(b.registry.register_component<hash_test::sprite_like_component>());
        const core::result<entity> first = a.world.create_entity();
        const core::result<entity> second = b.world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);

        const std::optional<core::uuid> texture = core::uuid::parse("11111111-2222-4333-8444-555555555555");
        const std::optional<core::uuid> layer = core::uuid::parse("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
        REQUIRE(texture.has_value());
        REQUIRE(layer.has_value());

        // The same three references: unbound in one world, bound to two different "slots" in the other
        // (what scene load and a script's AddComponent do, at addresses that differ in every process).
        const int slotStorage[2] = {0, 0};
        hash_test::sprite_like_component unbound = {};
        unbound.texture.id = *texture;
        unbound.layers[1].id = *layer;
        unbound.tint = 7;
        hash_test::sprite_like_component bound = unbound;
        bound.texture.slot = &slotStorage[0];
        bound.layers[0].slot = &slotStorage[1];
        bound.layers[1].slot = &slotStorage[0];
        CONJURE_REQUIRE_OK(a.world.add(first.value(), unbound));
        CONJURE_REQUIRE_OK(b.world.add(second.value(), bound));
        CHECK(world_hash(a.world) == world_hash(b.world));

        SUBCASE("the uuid of a single reference still counts") {
            hash_test::sprite_like_component other = bound;
            other.texture.id = *layer;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
        }

        SUBCASE("the uuid of every array element still counts") {
            hash_test::sprite_like_component other = bound;
            other.layers[1].id = *texture;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
            other = bound;
            other.layers[0].id = *layer;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
        }

        SUBCASE("the field after the references still counts") {
            hash_test::sprite_like_component other = bound;
            other.tint = 8;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
        }
    }

    TEST_CASE("regression - demo-review P1 asset references inside nested structs hash by uuid alone") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        CONJURE_REQUIRE_OK(a.registry.register_component<hash_test::mesh_like_component>());
        CONJURE_REQUIRE_OK(b.registry.register_component<hash_test::mesh_like_component>());
        const core::result<entity> first = a.world.create_entity();
        const core::result<entity> second = b.world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);

        const std::optional<core::uuid> stone = core::uuid::parse("11111111-2222-4333-8444-555555555555");
        const std::optional<core::uuid> moss = core::uuid::parse("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
        REQUIRE(stone.has_value());
        REQUIRE(moss.has_value());

        // A struct field and both elements of an array of structs, each bound in one world only.
        const int slotStorage[2] = {0, 0};
        hash_test::mesh_like_component unbound = {};
        unbound.primary.material.id = *stone;
        unbound.primary.pass = 1;
        unbound.extras[0].material.id = *moss;
        unbound.extras[1].material.id = *stone;
        unbound.extras[1].pass = 2;
        hash_test::mesh_like_component bound = unbound;
        bound.primary.material.slot = &slotStorage[0];
        bound.extras[0].material.slot = &slotStorage[1];
        bound.extras[1].material.slot = &slotStorage[0];
        CONJURE_REQUIRE_OK(a.world.add(first.value(), unbound));
        CONJURE_REQUIRE_OK(b.world.add(second.value(), bound));
        CHECK(world_hash(a.world) == world_hash(b.world));

        SUBCASE("the uuid inside the struct field still counts") {
            hash_test::mesh_like_component other = bound;
            other.primary.material.id = *moss;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
        }

        SUBCASE("the uuid inside every struct of the array still counts") {
            hash_test::mesh_like_component other = bound;
            other.extras[0].material.id = *stone;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
            other = bound;
            other.extras[1].material.id = *moss;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
        }

        SUBCASE("the field after the reference in a nested struct still counts") {
            hash_test::mesh_like_component other = bound;
            other.extras[1].pass = 3;
            CONJURE_REQUIRE_OK(b.world.set(second.value(), other));
            CHECK(world_hash(a.world) != world_hash(b.world));
        }
    }

    TEST_CASE("world_hash - an asset reference handle no wider than a uuid hashes every byte") {
        static_assert(sizeof(hash_test::bare_asset_ref) == sizeof(core::uuid));
        ecs_fixture a = {};
        ecs_fixture b = {};
        CONJURE_REQUIRE_OK(a.registry.register_component<hash_test::bare_ref_component>());
        CONJURE_REQUIRE_OK(b.registry.register_component<hash_test::bare_ref_component>());
        const core::result<entity> first = a.world.create_entity();
        const core::result<entity> second = b.world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);

        // Two uuids that differ in their last byte only.
        const std::optional<core::uuid> clip = core::uuid::parse("11111111-2222-4333-8444-555555555555");
        const std::optional<core::uuid> lastByte = core::uuid::parse("11111111-2222-4333-8444-555555555556");
        REQUIRE(clip.has_value());
        REQUIRE(lastByte.has_value());

        hash_test::bare_ref_component value = {};
        value.clip.id = *clip;
        CONJURE_REQUIRE_OK(a.world.add(first.value(), value));
        CONJURE_REQUIRE_OK(b.world.add(second.value(), value));
        CHECK(world_hash(a.world) == world_hash(b.world));

        value.clip.id = *lastByte;
        CONJURE_REQUIRE_OK(b.world.set(second.value(), value));
        CHECK(world_hash(a.world) != world_hash(b.world));
    }

    TEST_CASE("world_hash - row order within an archetype is part of the hash") {
        // Same entities with the same bytes, but one world moved e0 out of its archetype and back: the
        // swap-remove filled its row with the last one, so the rows are e2, e1, e0 instead of e0, e1, e2.
        ecs_fixture a = {};
        ecs_fixture b = {};
        std::array<entity, 3> first = {};
        std::array<entity, 3> second = {};
        CONJURE_REQUIRE_OK(a.world.create_entities(a.mask<position>(), first));
        CONJURE_REQUIRE_OK(b.world.create_entities(b.mask<position>(), second));
        for (u32 index = 0; index < 3; ++index) {
            const position value{static_cast<f32>(index), 1.0f, 2.0f};
            CONJURE_REQUIRE_OK(a.world.set(first[index], value));
            CONJURE_REQUIRE_OK(b.world.set(second[index], value));
        }
        CHECK(first == second);
        CHECK(world_hash(a.world) == world_hash(b.world));

        CONJURE_REQUIRE_OK(b.world.add<velocity>(second[0]));
        CONJURE_REQUIRE_OK(b.world.remove<velocity>(second[0]));
        CHECK(b.world.mask_of(second[0]) == a.world.mask_of(first[0]));
        CHECK(*b.world.get<const position>(second[0]) == position{0.0f, 1.0f, 2.0f});
        CHECK(world_hash(a.world) != world_hash(b.world));
        CONJURE_CHECK_OK(b.world.validate());
    }

    TEST_CASE("world_hash - non-trivial components share one json writer") {
        // Every non-trivial component used to get a fresh writer and a fresh string. This hook allocates
        // in a fresh writer in every configuration: the object pushes a frame on the writer's stack and
        // the 40-character string outgrows any small-string buffer.
        static constexpr auto writeHook = [](const void *, core::json::writer &out) -> core::result<void> {
            out.begin_object();
            out.key("text");
            out.value(std::string_view("a forty character string for the writers"));
            out.end_object();
            return core::result<void>::success();
        };
        static constexpr auto readHook = [](void *, const core::json::value &) -> core::result<void> {
            return core::result<void>::success();
        };
        ecs_fixture fixture = {};
        component_type_desc desc = {};
        desc.name = "test.hash_long_text";
        desc.size = sizeof(hash_test::long_text_component);
        desc.alignment = alignof(hash_test::long_text_component);
        desc.serializer.writeJson = writeHook;
        desc.serializer.readJson = readHook;
        const core::result<component_id> id = fixture.registry.register_described(desc);
        CONJURE_REQUIRE_OK(id);
        REQUIRE_FALSE(fixture.registry.info(id.value()).is_trivial());
        component_mask mask = {};
        mask.set(id.value());
        std::vector<entity> ids(1000);
        CONJURE_REQUIRE_OK(fixture.world.create_entities(mask, ids));

        const u64 warmUp = world_hash(fixture.world);
        testing::allocation_scope allocations = {};
        const u64 hash = world_hash(fixture.world);
        const u64 allocated = allocations.allocation_count();
        CAPTURE(allocated);
        CHECK(allocated <= 8);
        CHECK(hash == warmUp);
    }

    TEST_CASE("world_hash - allocation free for trivial worlds") {
        ecs_fixture fixture = {};
        populate(fixture, false);
        static_cast<void>(world_hash(fixture.world));
        testing::allocation_scope allocations = {};
        const u64 hash = world_hash(fixture.world);
        const u64 allocated = allocations.allocation_count();
        CHECK(allocated == 0);
        CHECK(hash != 0);
    }
}
