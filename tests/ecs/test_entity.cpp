#include "conjure/testing/doctest.h"

#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/ecs/entity.h"

#include <format>
#include <functional>
#include <type_traits>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;

TEST_SUITE("ecs/entity") {

    TEST_CASE("entity - null entity is all zero") {
        const entity value = {};
        CHECK(value.index == 0);
        CHECK(value.generation == 0);
        CHECK(value.is_null());
        CHECK(value == NULL_ENTITY);
        CHECK(NULL_ENTITY.to_u64() == 0);
        CHECK_FALSE(entity{0, 1}.is_null());
        CHECK_FALSE(entity{1, 0}.is_null());
        static_assert(sizeof(entity) == 8);
        static_assert(std::is_trivially_copyable_v<entity>);
    }

    TEST_CASE("entity - u64 round trip") {
        const entity value{0x12345678u, 0x9ABCDEF0u};
        const u64 packed = value.to_u64();
        CHECK(packed == 0x9ABCDEF012345678ull);
        CHECK(entity::from_u64(packed) == value);
        CHECK(entity::from_u64(0) == NULL_ENTITY);
        CHECK(entity::from_u64(0xFFFFFFFFFFFFFFFFull) == entity{0xFFFFFFFFu, 0xFFFFFFFFu});
    }

    TEST_CASE("entity - hash and equality") {
        const entity a{5, 2};
        const entity b{5, 2};
        const entity c{5, 3};
        const entity d{6, 2};
        CHECK(a == b);
        CHECK_FALSE(a == c);
        CHECK_FALSE(a == d);
        const std::hash<entity> hasher = {};
        CHECK(hasher(a) == hasher(b));
        CHECK(hasher(a) != hasher(c));
        CHECK(hasher(a) != hasher(d));
        CHECK(std::format("{}", a) == "5:2");
    }

    TEST_CASE("entity - archive local markers") {
        const entity local = entity::archive_local(7);
        CHECK(local.is_archive_local());
        CHECK(local.index == 7);
        CHECK(local.generation == ARCHIVE_LOCAL_GENERATION);
        CHECK_FALSE(NULL_ENTITY.is_archive_local());
        CHECK_FALSE(entity{7, 1}.is_archive_local());
    }

    TEST_CASE("entity_remap - remap resolves local and leaves real") {
        entity_remap map = {};
        CHECK(map.size() == 0);
        map.set(0, entity{10, 1});
        map.set(2, entity{12, 4});
        CHECK(map.size() == 3);
        CHECK(map.target(1) == NULL_ENTITY);
        CHECK(map.target(99) == NULL_ENTITY);

        CHECK(map.resolve(entity::archive_local(0)) == entity{10, 1});
        CHECK(map.resolve(entity::archive_local(2)) == entity{12, 4});
        CHECK(map.resolve(entity::archive_local(5)) == NULL_ENTITY);
        CHECK(map.resolve(entity{3, 9}) == entity{3, 9});
        CHECK(map.resolve(NULL_ENTITY) == NULL_ENTITY);

        const entity_remap fromVector(std::vector<entity>{entity{1, 1}, entity{2, 1}});
        CHECK(fromVector.size() == 2);
        CHECK(fromVector.targets()[1] == entity{2, 1});

        map.clear();
        CHECK(map.size() == 0);
        map.resize(4);
        CHECK(map.size() == 4);
        CHECK(map.target(3) == NULL_ENTITY);
    }

    TEST_CASE("entity_remap - remap serves capture and instantiate with one table") {
        const entity_remap map(std::vector<entity>{entity{10, 1}, entity{12, 4}});
        // Instantiate direction: local -> real, out of range -> null.
        CHECK(map.remap(entity::archive_local(0)) == entity{10, 1});
        CHECK(map.remap(entity::archive_local(1)) == entity{12, 4});
        CHECK(map.remap(entity::archive_local(7)) == NULL_ENTITY);
        // Capture direction: a target -> its local index; anything else unchanged.
        CHECK(map.remap(entity{12, 4}) == entity::archive_local(1));
        CHECK(map.remap(entity{10, 1}) == entity::archive_local(0));
        CHECK(map.remap(entity{10, 2}) == entity{10, 2});
        CHECK(map.remap(NULL_ENTITY) == NULL_ENTITY);
        // Applying it twice is the identity for members of the set.
        CHECK(map.remap(map.remap(entity{12, 4})) == entity{12, 4});
        CHECK(entity_remap().remap(entity{5, 5}) == entity{5, 5});
    }

    TEST_CASE("entity - reflects as the ENTITY field kind") {
        static_assert(core::reflect::kind_of<entity>() == core::reflect::field_kind::ENTITY);
        CHECK(core::reflect::type_id_of<entity>() == core::reflect::type_id::from_name("conjure.ecs.entity"));
    }
}
