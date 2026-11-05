#include "conjure/testing/doctest.h"

#include "reflect_test_types.h"

#include "conjure/core/hash.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/string_id.h"

#include <format>
#include <set>
#include <string_view>
#include <unordered_map>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

TEST_SUITE("core/type_id") {

    TEST_CASE("type_id - the default id is null and from_name hashes with FNV-1a 64") {
        constexpr type_id null = {};
        static_assert(null.is_null());
        static_assert(null == type_id::null());
        static_assert(null.value == 0);

        constexpr type_id id = type_id::from_name("ns.my_struct");
        static_assert(!id.is_null());
        static_assert(id.value == hash::fnv1a64("ns.my_struct"));
        CHECK(id.value == string_id("ns.my_struct").hash());
        CHECK(type_id::from_name("").value == hash::FNV1A64_OFFSET_BASIS);
    }

    TEST_CASE("type_id - ids compare and order by value") {
        const type_id low = {1};
        const type_id high = {2};

        CHECK(low < high);
        CHECK(high > low);
        CHECK(low == type_id{1});
        CHECK(low != high);
        CHECK((low <=> high) == std::strong_ordering::less);

        const std::set<type_id> ordered = {high, low, type_id::null()};
        REQUIRE(ordered.size() == 3);
        CHECK(*ordered.begin() == type_id::null());
        CHECK(*ordered.rbegin() == high);
    }

    TEST_CASE("type_id - std::hash forwards the value so ids work as unordered keys") {
        const type_id id = type_id::from_name("test.key");
        CHECK(std::hash<type_id>{}(id) == id.value);

        std::unordered_map<type_id, int> map = {};
        map[id] = 7;
        map[type_id::from_name("test.other")] = 8;
        CHECK(map.size() == 2);
        CHECK(map.at(id) == 7);
    }

    TEST_CASE("type_id - the formatter prints '#' followed by 16 lower-case hex digits") {
        CHECK(std::format("{}", type_id{0xABCDull}) == "#000000000000abcd");
        CHECK(std::format("{}", type_id::null()) == "#0000000000000000");
        CHECK(std::format("{}", type_id{0xFFFFFFFFFFFFFFFFull}) == "#ffffffffffffffff");
        CHECK(std::format("[{:>19}]", type_id{1}) == "[  #0000000000000001]");
    }

    TEST_CASE("type_id - type_id_of derives the id from the registered name at compile time") {
        static_assert(type_id_of<f32>() == type_id::from_name("f32"));
        static_assert(type_id_of<bool>() == type_id::from_name("bool"));
        static_assert(type_id_of<std::string>() == type_id::from_name("string"));
        static_assert(type_id_of<string_id>() == type_id::from_name("string_id"));
        static_assert(type_id_of<uuid>() == type_id::from_name("uuid"));
        static_assert(type_id_of<math::vec3>() == type_id::from_name("vec3"));
        static_assert(type_id_of<math::quat>() == type_id::from_name("quat"));
        static_assert(type_id_of<math::mat4>() == type_id::from_name("mat4"));
        static_assert(type_id_of<math::transform>() == type_id::from_name("transform"));
        static_assert(type_id_of<reflect_test::inner>() == type_id::from_name("test.inner"));
        static_assert(type_id_of<reflect_test::everything>() == type_id::from_name("test.everything"));
        static_assert(type_id_of<reflect_test::weapon_kind>() == type_id::from_name("test.weapon_kind"));
        static_assert(type_id_of<reflect_test::body_flags>() == type_id::from_name("test.body_flags"));
        CHECK(type_id_of<reflect_test::inner>().value == hash::fnv1a64("test.inner"));
    }

    TEST_CASE("type_id - type_id_of ignores const, volatile and references") {
        static_assert(type_id_of<const f32>() == type_id_of<f32>());
        static_assert(type_id_of<const reflect_test::inner &>() == type_id_of<reflect_test::inner>());
        static_assert(type_id_of<reflect_test::inner &&>() == type_id_of<reflect_test::inner>());
    }

    TEST_CASE("type_id - kind overrides supply the name of foreign handle types") {
        static_assert(type_id_of<reflect_test::fake_entity>() == type_id::from_name("test.entity"));
        static_assert(type_id_of<reflect_test::fake_asset>() == type_id::from_name("test.asset_ref"));
        static_assert(reflect_name_of<reflect_test::fake_entity>() == "test.entity");
        static_assert(reflect_name_of<reflect_test::inner>() == "test.inner");
        static_assert(has_kind_override<reflect_test::fake_entity>);
        static_assert(!has_kind_override<reflect_test::inner>);
        static_assert(has_reflect_name<reflect_test::inner>);
        static_assert(!has_reflect_name<reflect_test::fake_entity>);
        static_assert(reflect_named<reflect_test::fake_entity>);
        static_assert(reflect_named<reflect_test::inner>);
        static_assert(!reflect_named<reflect_test::not_reflected>);
        static_assert(reflect_named<int>);   // int is i32
        static_assert(!reflect_named<char>);
        static_assert(!reflect_named<int *>);
        CHECK(true);
    }

    TEST_CASE("type_id - distinct names produce distinct ids") {
        CHECK(type_id_of<f32>() != type_id_of<f64>());
        CHECK(type_id_of<reflect_test::inner>() != type_id_of<reflect_test::everything>());
        CHECK(type_id::from_name("a") != type_id::from_name("A"));
    }
}
