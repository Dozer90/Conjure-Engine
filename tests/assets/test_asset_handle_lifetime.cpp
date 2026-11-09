#include "conjure/testing/doctest.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"

#include <memory>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

TEST_SUITE("assets/asset_handle_lifetime") {

    TEST_CASE("asset_handle - default is unbound, terminal and reports no asset") {
        const asset_handle handle = {};
        CHECK_FALSE(handle.is_bound());
        CHECK(handle.id().is_null());
        CHECK(handle.type() == asset_type_id::INVALID);
        CHECK(handle.state() == asset_state::UNLOADED);
        CHECK(handle.is_terminal());
        CHECK(handle.version() == 0);
        CHECK(handle.get<test_asset>() == nullptr);
    }

    TEST_CASE("asset_handle - copy and move adjust the strong count exactly") {
        asset_slot slot = {};
        slot.id = test_id(1);
        {
            asset_handle first(&slot);
            CHECK(slot.strongRefs.load() == 1);

            asset_handle second = first;
            CHECK(slot.strongRefs.load() == 2);

            asset_handle third = std::move(first);
            CHECK(slot.strongRefs.load() == 2);
            CHECK_FALSE(first.is_bound());
            CHECK(third.slot() == &slot);

            // Self assignment changes nothing. Through an alias, so Clang's -Wself-assign-overloaded
            // (an error under -Wall -Werror) does not reject the very thing being tested.
            asset_handle &alias = second;
            second = alias;
            CHECK(slot.strongRefs.load() == 2);
            second = std::move(alias);
            CHECK(slot.strongRefs.load() == 2);
            CHECK(second.is_bound());

            asset_handle fourth = {};
            fourth = third;
            CHECK(slot.strongRefs.load() == 3);
            fourth = std::move(second);
            CHECK(slot.strongRefs.load() == 2);
            CHECK_FALSE(second.is_bound());

            third.reset();
            CHECK(slot.strongRefs.load() == 1);
            third.reset();
            CHECK(slot.strongRefs.load() == 1);
        }
        CHECK(slot.strongRefs.load() == 0);
    }

    TEST_CASE("asset_handle - assigning a handle of another slot moves the reference") {
        asset_slot first = {};
        asset_slot second = {};
        asset_handle handle(&first);
        handle = asset_handle(&second);
        CHECK(first.strongRefs.load() == 0);
        CHECK(second.strongRefs.load() == 1);
        CHECK(handle == asset_handle(&second));
        CHECK_FALSE(handle == asset_handle(&first));
    }

    TEST_CASE("asset_handle - ref of a mismatching type is unbound and logs once") {
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        asset_slot slot = {};
        slot.id = test_id(2);
        slot.type.store(asset_type_traits<test_asset>::id());
        const asset_handle handle(&slot);

        const asset_ref<other_asset> wrong = handle.ref<other_asset>();
        CHECK_FALSE(wrong.is_bound());
        CHECK(wrong.id() == slot.id);
        CHECK(handle.get<other_asset>() == nullptr);
        const u32 errorsAfterFirst = logs.count(core::log_level::ERR);
        CHECK(errorsAfterFirst == 1);

        static_cast<void>(handle.ref<other_asset>());
        CHECK(logs.count(core::log_level::ERR) == errorsAfterFirst);

        const asset_ref<test_asset> right = handle.ref<test_asset>();
        CHECK(right.is_bound());
    }

    TEST_CASE("asset_handle - a handle from the manager keeps the payload across collect") {
        pipeline_fixture fixture = {};
        auto source = std::make_unique<memory_asset_source>();
        const asset_id id = test_id(3);
        source->add(make_test_blob(id, "resident"));
        fixture.manager->swap_source(std::move(source));

        asset_handle handle = fixture.manager->load(id);
        settle(*fixture.manager);
        REQUIRE(handle.state() == asset_state::LOADED);
        CHECK(handle.is_terminal());
        CHECK(handle.type() == asset_type_traits<test_asset>::id());
        CHECK(handle.version() == 1);

        CHECK(fixture.manager->collect() == 0);
        CHECK(handle.get<test_asset>()->content == "resident");

        handle.reset();
        CHECK(fixture.manager->collect() == 1);
        CHECK(fixture.manager->state_of(id) == asset_state::UNLOADED);
    }

    TEST_CASE("asset_handle - a handle outliving the manager asserts in debug") {
        core::jobs::job_system jobs = {};
        CONJURE_REQUIRE_OK(jobs.start(core::jobs::job_system_desc{}));
        conjure::testing::log_capture logs = {};
        logs.allow_errors();

        asset_manager_desc desc = {};
        desc.jobs = &jobs;
        desc.source = std::make_unique<memory_asset_source>();
        auto manager = std::make_unique<asset_manager>(std::move(desc));
        auto escaped = std::make_unique<asset_handle>(manager->make_handle(test_id(4)));
        REQUIRE(escaped->is_bound());

        {
            conjure::testing::expect_assert guard = {};
            manager.reset();
            CHECK(guard.count() == 1);
            CHECK(guard.last_message().find("live asset_handle") != std::string::npos);
        }
        CHECK(logs.contains(core::log_level::ERR, "live asset_handle"));

        // The slot the handle points at died with the manager: running its destructor would touch
        // freed memory, so this programmer-error simulation deliberately leaks the one handle.
        static_cast<void>(escaped.release());
        jobs.stop();
    }
}
