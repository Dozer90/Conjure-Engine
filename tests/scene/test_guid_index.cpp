#include "conjure/testing/doctest.h"
#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "scene_test_fixture.h"

#include "conjure/scene/guid_index.h"

#include <vector>

using namespace conjure;
using namespace scene_test;

TEST_SUITE("scene/guid_index") {

    TEST_CASE("guid_index - insert find erase") {
        scene::guid_index index = {};
        CHECK(index.size() == 0);
        CHECK(index.find(test_uuid(1)).is_null());
        CHECK_FALSE(index.contains(test_uuid(1)));

        const ecs::entity first{1, 1};
        const ecs::entity second{2, 1};
        CHECK(index.insert(test_uuid(1), first));
        CHECK(index.insert(test_uuid(2), second));
        CHECK(index.size() == 2);
        CHECK(index.find(test_uuid(1)) == first);
        CHECK(index.find(test_uuid(2)) == second);
        CHECK(index.contains(test_uuid(2)));

        // Erase only when the mapping points at the expected entity.
        index.erase(test_uuid(1), second);
        CHECK(index.find(test_uuid(1)) == first);
        index.erase(test_uuid(1), first);
        CHECK(index.find(test_uuid(1)).is_null());
        CHECK(index.size() == 1);
        index.erase(test_uuid(9), first);
        CHECK(index.size() == 1);

        // A null guid is never inserted.
        CHECK_FALSE(index.insert(core::uuid(), first));
        CHECK(index.size() == 1);
        index.clear();
        CHECK(index.size() == 0);
        CHECK_FALSE(index.contains(test_uuid(2)));
    }

    TEST_CASE("guid_index - collision keeps first") {
        scene::guid_index index = {};
        const ecs::entity first{1, 1};
        const ecs::entity second{2, 1};
        CHECK(index.insert(test_uuid(1), first));
        CHECK_FALSE(index.insert(test_uuid(1), second));
        CHECK(index.find(test_uuid(1)) == first);
        CHECK(index.size() == 1);
    }

    TEST_CASE("guid_index - rebind") {
        scene::guid_index index = {};
        const ecs::entity first{1, 1};
        const ecs::entity second{2, 1};
        CHECK(index.insert(test_uuid(1), first));
        CHECK(index.insert(test_uuid(2), second));
        index.rebind(first, test_uuid(1), test_uuid(3));
        CHECK(index.find(test_uuid(1)).is_null());
        CHECK(index.find(test_uuid(3)) == first);

        // Rebinding onto a taken guid keeps the existing mapping; the old one is still released.
        index.rebind(first, test_uuid(3), test_uuid(2));
        CHECK(index.find(test_uuid(2)) == second);
        CHECK(index.find(test_uuid(3)).is_null());
        CHECK(index.size() == 1);

        // Rebinding from a guid mapped to another entity leaves that mapping alone.
        index.rebind(first, test_uuid(2), test_uuid(4));
        CHECK(index.find(test_uuid(2)) == second);
        CHECK(index.find(test_uuid(4)) == first);
    }

    TEST_CASE("guid_index - observer indexes on add and removes on destroy") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity created = fixture.create("indexed");
        const std::optional<core::uuid> id = target.guid_of(created);
        REQUIRE(id.has_value());
        CHECK(target.find_guid(*id) == created);
        CHECK(target.guids().size() == 1);

        // A raw guid add on an entity made outside the scene API is indexed too.
        const core::result<ecs::entity> raw = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(raw);
        CONJURE_REQUIRE_OK(fixture.world.add<scene::guid>(raw.value(), scene::guid{test_uuid(5)}));
        CHECK(target.find_guid(test_uuid(5)) == raw.value());
        CHECK(target.guids().size() == 2);

        // A raw duplicate keeps the first mapping and warns.
        {
            testing::log_capture logs = {};
            const core::result<ecs::entity> duplicate = fixture.world.create_entity();
            CONJURE_REQUIRE_OK(duplicate);
            CONJURE_REQUIRE_OK(fixture.world.add<scene::guid>(duplicate.value(), scene::guid{test_uuid(5)}));
            CHECK(target.find_guid(test_uuid(5)) == raw.value());
            CHECK(logs.count(core::log_level::WARN) == 1);
            CONJURE_REQUIRE_OK(fixture.world.destroy_entity(duplicate.value()));
        }
        CHECK(target.find_guid(test_uuid(5)) == raw.value());

        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(raw.value()));
        CHECK(target.find_guid(test_uuid(5)).is_null());
        CONJURE_REQUIRE_OK(target.destroy_now(created));
        CHECK(target.find_guid(*id).is_null());
        CHECK(target.guids().size() == 0);
    }

    TEST_CASE("guid_index - reserve prevents allocation during batch") {
        scene::guid_index index = {};
        CHECK(index.capacity() >= scene::guid_index::INITIAL_CAPACITY);
        index.reserve(4096);
        const usize capacity = index.capacity();
        std::vector<core::uuid> ids = {};
        for (u64 value = 1; value <= 4096; ++value) {
            ids.push_back(test_uuid(value));
        }
        CONJURE_CHECK_NO_ALLOCATIONS({
            for (u64 value = 0; value < ids.size(); ++value) {
                const bool inserted = index.insert(ids[value], ecs::entity{static_cast<u32>(value + 1), 1});
                CONJURE_UNUSED(inserted);
            }
        });
        CHECK(index.size() == 4096);
        CHECK(index.capacity() == capacity);
        CONJURE_CHECK_NO_ALLOCATIONS(CONJURE_UNUSED(index.find(ids[100])));
        CONJURE_CHECK_NO_ALLOCATIONS(index.erase(ids[100], ecs::entity{101, 1}));
        CHECK(index.size() == 4095);
    }
}
