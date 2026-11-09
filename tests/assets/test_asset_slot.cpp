#include "conjure/testing/doctest.h"

#include "conjure/assets/asset_slot.h"
#include "conjure/core/containers/stable_vector.h"

#include <atomic>
#include <type_traits>

using namespace conjure;
using namespace conjure::assets;

TEST_SUITE("assets/asset_slot") {

    TEST_CASE("asset_slot - default state is unloaded with no payload") {
        asset_slot slot = {};
        CHECK(slot.object.load() == nullptr);
        CHECK(slot.version.load() == 0);
        CHECK(slot.state.load() == asset_state::UNLOADED);
        CHECK(slot.strongRefs.load() == 0);
        CHECK(slot.type.load() == asset_type_id::INVALID);
        CHECK(slot.id.is_null());
        CHECK(slot.index == 0);
    }

    TEST_CASE("asset_slot - every shared field is an atomic and the struct is not copyable") {
        static_assert(std::is_same_v<decltype(asset_slot::object), std::atomic<void *>>);
        static_assert(std::is_same_v<decltype(asset_slot::state), std::atomic<asset_state>>);
        static_assert(std::is_same_v<decltype(asset_slot::type), std::atomic<asset_type_id>>);
        static_assert(!std::is_copy_constructible_v<asset_slot>);
        CHECK(std::atomic<asset_state>::is_always_lock_free);
        CHECK(std::atomic<void *>::is_always_lock_free);
    }

    TEST_CASE("asset_slot - addresses are stable inside a stable_vector") {
        core::stable_vector<asset_slot> slots = {};
        asset_slot &first = slots.emplace_back();
        first.id = core::uuid(1, 2);
        const asset_slot *address = &first;
        for (int index = 0; index < 500; ++index) {
            slots.emplace_back();
        }
        CHECK(&slots[0] == address);
        CHECK(slots[0].id == core::uuid(1, 2));
    }
}
