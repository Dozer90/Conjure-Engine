#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/ecs/component.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/component") {

    TEST_CASE("component_id - default is invalid and comparable") {
        const component_id none = {};
        CHECK_FALSE(none.is_valid());
        CHECK(none.value == component_id::INVALID);
        CHECK(component_id{0}.is_valid());
        CHECK(component_id{static_cast<u16>(MAX_COMPONENTS - 1)}.is_valid());
        CHECK_FALSE(component_id{static_cast<u16>(MAX_COMPONENTS)}.is_valid());
        CHECK(component_id{3} == component_id{3});
        CHECK_FALSE(component_id{3} == component_id{4});
    }

    TEST_CASE("component_flags - are a flag set") {
        component_flags flags = component_flags::TAG | component_flags::TRIVIAL;
        CHECK(has_flag(flags, component_flags::TAG));
        CHECK(has_flag(flags, component_flags::TRIVIAL));
        CHECK_FALSE(has_flag(flags, component_flags::DYNAMIC));
        flags &= ~component_flags::TAG;
        CHECK_FALSE(has_flag(flags, component_flags::TAG));
        CHECK(static_cast<u32>(component_flags::NONE) == 0);
    }

    TEST_CASE("component_info - flag helpers follow the flags") {
        component_info info = {};
        CHECK_FALSE(info.is_tag());
        CHECK_FALSE(info.is_trivial());
        CHECK_FALSE(info.is_transient());
        info.flags = component_flags::TAG | component_flags::TRIVIAL | component_flags::TRANSIENT;
        CHECK(info.is_tag());
        CHECK(info.is_trivial());
        CHECK(info.is_transient());
    }

    TEST_CASE("component_name - CONJURE_COMPONENT names a type") {
        static_assert(named_component<position>);
        static_assert(!named_component<int>);
        CHECK(component_name<position>::VALUE == "test.position");
        CHECK(component_name<player_tag>::VALUE == "test.player_tag");
    }

    TEST_CASE("derive_component_flags - tag trivial and non trivial") {
        constexpr component_flags tag = derive_component_flags<player_tag>(component_flags::NONE);
        constexpr component_flags trivial = derive_component_flags<position>(component_flags::TRANSIENT);
        constexpr component_flags nonTrivial = derive_component_flags<name_component>(component_flags::NONE);
        static_assert(has_flag(tag, component_flags::TAG) && has_flag(tag, component_flags::TRIVIAL));
        static_assert(has_flag(trivial, component_flags::TRIVIAL) && has_flag(trivial, component_flags::TRANSIENT));
        static_assert(!has_flag(trivial, component_flags::TAG));
        static_assert(!has_flag(nonTrivial, component_flags::TRIVIAL));
        CHECK(has_flag(tag, component_flags::TAG));
        CHECK(has_flag(tag, component_flags::TRIVIAL));
        CHECK(has_flag(trivial, component_flags::TRIVIAL));
        CHECK(has_flag(trivial, component_flags::TRANSIENT));
        CHECK_FALSE(has_flag(trivial, component_flags::TAG));
        CHECK_FALSE(has_flag(nonTrivial, component_flags::TRIVIAL));
        CHECK_FALSE(has_flag(nonTrivial, component_flags::TAG));
    }

    TEST_CASE("make_component_ops - trivial types use the memcpy path") {
        const component_ops ops = make_component_ops<position>();
        CHECK(ops.constructDefault == nullptr);
        CHECK(ops.destruct == nullptr);
        CHECK(ops.relocate == nullptr);
        CHECK(ops.copy == nullptr);
    }

    TEST_CASE("make_component_ops - non trivial types run their special members") {
        tracked::reset_counters();
        const component_ops ops = make_component_ops<tracked>();
        REQUIRE(ops.constructDefault != nullptr);
        REQUIRE(ops.destruct != nullptr);
        REQUIRE(ops.relocate != nullptr);
        REQUIRE(ops.copy != nullptr);

        alignas(tracked) std::byte storageA[sizeof(tracked) * 2] = {};
        alignas(tracked) std::byte storageB[sizeof(tracked) * 2] = {};
        ops.constructDefault(storageA, 2);
        CHECK(tracked::sConstructed == 2);
        reinterpret_cast<tracked *>(storageA)[1].value = 42;

        ops.copy(storageB, storageA, 2);
        CHECK(tracked::sCopied == 2);
        CHECK(reinterpret_cast<tracked *>(storageB)[1].value == 42);
        ops.destruct(storageB, 2);

        ops.relocate(storageB, storageA, 2);
        CHECK(tracked::sMoved == 2);
        CHECK(reinterpret_cast<tracked *>(storageB)[1].value == 42);
        ops.destruct(storageB, 2);
        CHECK(tracked::live() == 0);
    }

    TEST_CASE("default_component_bytes - captures member initialisers of trivial types") {
        const std::vector<std::byte> none = default_component_bytes<position>();
        CHECK(none.empty());

        const std::vector<std::byte> bytes = default_component_bytes<health>();
        REQUIRE(bytes.size() == sizeof(health));
        health decoded = {};
        decoded.value = 0;
        decoded.maximum = 0;
        std::memcpy(&decoded, bytes.data(), sizeof(decoded));
        CHECK(decoded.value == 100);
        CHECK(decoded.maximum == 100);

        CHECK(default_component_bytes<name_component>().empty());
        CHECK(default_component_bytes<player_tag>().empty());
    }

    TEST_CASE("regression - review 4 default bytes of an over-aligned type") {
        // Trivially copyable, alignas(64) and exactly 64 bytes (no padding) with non-zero initialisers:
        // the default object is built in storage aligned for the type, never in the byte vector.
        struct alignas(64) aligned_defaults {
            std::array<f32, 16> values = {1.0f, 2.0f, 3.0f, 4.0f};
        };
        static_assert(std::is_trivially_copyable_v<aligned_defaults>);
        static_assert(!std::is_trivially_default_constructible_v<aligned_defaults>);
        static_assert(sizeof(aligned_defaults) == 64 && alignof(aligned_defaults) == 64);

        const std::vector<std::byte> bytes = default_component_bytes<aligned_defaults>();
        REQUIRE(bytes.size() == sizeof(aligned_defaults));
        std::array<f32, 16> decoded = {};
        std::memcpy(decoded.data(), bytes.data(), bytes.size());
        CHECK(decoded[0] == 1.0f);
        CHECK(decoded[3] == 4.0f);
        CHECK(decoded[4] == 0.0f);
        CHECK(decoded[15] == 0.0f);
        CHECK(default_component_bytes<wide_component>().empty());
    }

    TEST_CASE("component_type_cache - one atomic slot per type") {
        std::atomic<u64> &first = component_type_cache<position>::slot();
        std::atomic<u64> &again = component_type_cache<position>::slot();
        std::atomic<u64> &other = component_type_cache<velocity>::slot();
        CHECK(&first == &again);
        CHECK(&first != &other);
    }
}
