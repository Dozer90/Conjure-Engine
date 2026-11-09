#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    std::unique_ptr<memory_asset_source> source_with(std::vector<std::pair<asset_id, std::string>> blobs) {
        auto source = std::make_unique<memory_asset_source>();
        for (auto &[id, content] : blobs) {
            source->add(make_test_blob(id, content), "assets://" + content + ".ta");
        }
        return source;
    }
}

TEST_SUITE("assets/asset_manager") {

    TEST_CASE("asset_manager - construction with an inline job system is cheap and empty") {
        pipeline_fixture fixture = {};
        const asset_manager_stats stats = fixture.manager->stats();
        CHECK(stats.slots == 0);
        CHECK(stats.loaded == 0);
        CHECK(stats.pending == 0);
        CHECK(stats.failed == 0);
        CHECK(stats.retired == 0);
        CHECK(stats.jobsInFlight == 0);
        CHECK(stats.bytesResident == 0);
        CHECK_FALSE(fixture.manager->is_shut_down());
        CHECK(fixture.manager->frame_index() == 0);
        CHECK(fixture.manager->types().size() == 3);
    }

    TEST_CASE("asset_manager - bind creates a slot, never loads and agrees with bind_slot and make_handle") {
        pipeline_fixture fixture = {};
        fixture.manager->swap_source(source_with({{test_id(1), "one"}}));
        const asset_ref<test_asset> ref = fixture.manager->bind<test_asset>(test_id(1));
        CHECK(ref.is_bound());
        CHECK(ref.get() == nullptr);
        CHECK(ref.state() == asset_state::UNLOADED);
        CHECK(fixture.manager->state_of(test_id(1)) == asset_state::UNLOADED);
        CHECK(fixture.manager->stats().slots == 1);
        CHECK(fixture.manager->stats().pending == 0);

        CHECK(fixture.manager->bind_slot(test_id(1)) == ref.slot());
        CHECK(fixture.manager->bind_slot(test_id(1))->type.load() == asset_type_traits<test_asset>::id());
        const asset_handle handle = fixture.manager->make_handle(test_id(1));
        CHECK(handle.slot() == ref.slot());
        CHECK(handle.state() == asset_state::UNLOADED);
        CHECK(fixture.manager->stats().slots == 1);

        // The null uuid never gets a slot.
        CHECK_FALSE(fixture.manager->bind<test_asset>(asset_id()).is_bound());
        CHECK(fixture.manager->bind_slot(asset_id()) == nullptr);
        CHECK_FALSE(fixture.manager->make_handle(asset_id()).is_bound());
        CHECK_FALSE(fixture.manager->load(asset_id()).is_bound());
    }

    TEST_CASE("asset_manager - bind takes the type from the source and reports a mismatch once") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.manager->swap_source(source_with({{test_id(1), "one"}}));

        const asset_ref<other_asset> wrong = fixture.manager->bind<other_asset>(test_id(1));
        CHECK_FALSE(wrong.is_bound());
        CHECK(wrong.id() == test_id(1));
        CHECK(logs.count(core::log_level::ERR) == 1);
        static_cast<void>(fixture.manager->bind<other_asset>(test_id(1)));
        CHECK(logs.count(core::log_level::ERR) == 1);
        CHECK(fixture.manager->bind<test_asset>(test_id(1)).is_bound());

        // An unknown uuid takes the type from the first typed bind; another type then mismatches.
        const asset_ref<other_asset> fresh = fixture.manager->bind<other_asset>(test_id(2));
        CHECK(fresh.is_bound());
        CHECK_FALSE(fixture.manager->bind<test_asset>(test_id(2)).is_bound());
        CHECK(logs.count(core::log_level::ERR) == 2);
        const asset_handle handle = fixture.manager->make_handle(test_id(2));
        CHECK(handle.type() == asset_type_traits<other_asset>::id());
    }

    TEST_CASE("asset_manager - find resolves paths through the sources") {
        pipeline_fixture fixture = {};
        fixture.manager->swap_source(source_with({{test_id(1), "one"}, {test_id(2), "two"}}));
        CHECK(fixture.manager->find("assets://one.ta") == test_id(1));
        CHECK(fixture.manager->find("assets://two.ta") == test_id(2));
        CHECK_FALSE(fixture.manager->find("assets://three.ta").has_value());
    }

    TEST_CASE("asset_manager - fallback is created lazily, once, pinned, and get_or_fallback uses it") {
        pipeline_fixture fixture = {};
        fixture.manager->swap_source(source_with({{test_id(1), "one"}}));
        test_asset *fallback = fixture.manager->fallback<test_asset>();
        REQUIRE(fallback != nullptr);
        CHECK(fallback->content == "fallback");
        CHECK(fixture.manager->fallback<test_asset>() == fallback);
        CHECK(fixture.manager->fallback<other_asset>() == nullptr);
        CHECK(fixture.manager->fallback<other_asset>() == nullptr);

        const asset_ref<test_asset> ref = fixture.manager->bind<test_asset>(test_id(1));
        CHECK(fixture.manager->get_or_fallback(ref) == fallback);
        asset_handle handle = fixture.manager->load(test_id(1));
        settle(*fixture.manager);
        CHECK(fixture.manager->get_or_fallback(ref) == ref.get());
        CHECK(fixture.manager->get_or_fallback(ref)->content == "one");

        const asset_ref<test_asset> unbound = {};
        CHECK(fixture.manager->get_or_fallback(unbound) == fallback);
        CHECK(fixture.manager->collect() == 0);   // the fallback is not a slot and the handle is live
    }

    TEST_CASE("asset_manager - a PINNED_FALLBACK type gets its fallback at registration") {
        pipeline_fixture fixture = {};
        asset_type_desc desc = {};
        desc.id = make_asset_type_id("conjure.test.pinned");
        desc.name = "conjure.test.pinned";
        desc.flags = asset_type_flags::PINNED_FALLBACK;
        desc.loader = nullptr;
        CONJURE_REQUIRE_OK(fixture.manager->types().register_type(std::move(desc)));
        // Without a loader there is nothing to create; the registration itself must not fail.
        CHECK(fixture.manager->types().find("conjure.test.pinned") != nullptr);

        // test_asset re-registered with the flag creates its fallback eagerly.
        CONJURE_REQUIRE_OK(fixture.manager->types().unregister_type(asset_type_traits<test_asset>::id()));
        CONJURE_REQUIRE_OK(fixture.manager->types().register_type(
          make_asset_type_desc<test_asset>(std::make_unique<test_loader>(), 1, asset_type_flags::PINNED_FALLBACK)
        ));
        CHECK(fixture.manager->fallback<test_asset>() != nullptr);
        CONJURE_REQUIRE_OK(fixture.manager->types().unregister_type(asset_type_traits<test_asset>::id()));
        CHECK(fixture.manager->fallback<test_asset>() == nullptr);
    }

    TEST_CASE("asset_manager - pin protects a slot from collect") {
        pipeline_fixture fixture = {};
        fixture.manager->swap_source(source_with({{test_id(1), "one"}}));
        CHECK_FALSE(fixture.manager->is_pinned(test_id(1)));
        fixture.manager->pin(test_id(1), true);
        CHECK(fixture.manager->is_pinned(test_id(1)));
        {
            asset_handle handle = fixture.manager->load(test_id(1));
            settle(*fixture.manager);
        }
        CHECK(fixture.manager->collect() == 0);
        CHECK(fixture.manager->state_of(test_id(1)) == asset_state::LOADED);
        fixture.manager->pin(test_id(1), false);
        CHECK(fixture.manager->collect() == 1);
        CHECK(fixture.manager->state_of(test_id(1)) == asset_state::UNLOADED);
        CHECK_FALSE(fixture.manager->is_pinned(test_id(99)));
        fixture.manager->pin(asset_id(), true);
    }

    TEST_CASE("asset_manager - state_of and last_error for unknown and failed assets") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.manager->swap_source(source_with({}));
        CHECK(fixture.manager->state_of(test_id(1)) == asset_state::UNLOADED);
        CHECK(fixture.manager->last_error(test_id(1)) == nullptr);
        asset_handle handle = fixture.manager->load(test_id(1));
        settle(*fixture.manager);
        CHECK(fixture.manager->state_of(test_id(1)) == asset_state::FAILED);
        REQUIRE(fixture.manager->last_error(test_id(1)) != nullptr);
        CHECK(fixture.manager->last_error(test_id(1))->is(asset_error_code::NOT_FOUND));
        CHECK(fixture.manager->failed_ids(asset_error_code::NOT_FOUND) == std::vector<asset_id>{test_id(1)});
        CHECK(fixture.manager->failed_ids(asset_error_code::CACHE_MISS).empty());
        CHECK(fixture.manager->stats().failed == 1);
    }

    TEST_CASE("asset_manager - subscribe and unsubscribe control event delivery") {
        pipeline_fixture fixture = {};
        fixture.manager->swap_source(source_with({{test_id(1), "one"}, {test_id(2), "two"}}));
        std::vector<asset_event_kind> kinds = {};
        const asset_subscription subscription = fixture.manager->subscribe([&kinds](const asset_event &event) {
            kinds.push_back(event.kind);
        });
        CHECK(subscription.is_valid());
        CHECK_FALSE(fixture.manager->subscribe({}).is_valid());

        asset_handle one = fixture.manager->load(test_id(1));
        settle(*fixture.manager);
        REQUIRE(kinds.size() == 1);
        CHECK(kinds[0] == asset_event_kind::LOADED);

        fixture.manager->unsubscribe(subscription);
        asset_handle two = fixture.manager->load(test_id(2));
        settle(*fixture.manager);
        CHECK(kinds.size() == 1);
        fixture.manager->unsubscribe(subscription);   // idempotent
    }

    TEST_CASE("asset_manager - begin_frame, end_frame and the frame index") {
        pipeline_fixture fixture = {};
        fixture.manager->begin_frame(41);
        CHECK(fixture.manager->frame_index() == 41);
        CHECK(fixture.manager->stats().frameIndex == 41);
        fixture.manager->update();
        fixture.manager->end_frame();
        fixture.manager->begin_frame(42);
        CHECK(fixture.manager->frame_index() == 42);
        fixture.manager->end_frame();
    }

    TEST_CASE("asset_manager - shutdown refuses new work and is idempotent") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.manager->swap_source(source_with({{test_id(1), "one"}}));
        asset_handle loaded = fixture.manager->load(test_id(1));
        settle(*fixture.manager);
        fixture.manager->shutdown();
        CHECK(fixture.manager->is_shut_down());
        fixture.manager->shutdown();

        asset_handle late = fixture.manager->load(test_id(2));
        CHECK(late.state() == asset_state::FAILED);
        REQUIRE(fixture.manager->last_error(test_id(2)) != nullptr);
        CHECK(fixture.manager->last_error(test_id(2))->is(asset_error_code::SHUTTING_DOWN));
        CONJURE_CHECK_ERROR(fixture.manager->load_blocking(test_id(2)), asset_error_code::SHUTTING_DOWN);
        fixture.manager->reload(test_id(1));
        CHECK(loaded.state() == asset_state::LOADED);
        CHECK(loaded.get<test_asset>()->content == "one");
        fixture.manager->update();
        fixture.manager->flush();
    }

    TEST_CASE("asset_load_batch - progress, completion and loaded count") {
        const asset_load_batch empty = {};
        CHECK(empty.is_complete());
        CHECK(empty.progress() == 1.0f);
        CHECK(empty.size() == 0);
        CHECK(empty.loaded_count() == 0);

        asset_slot loaded = {};
        loaded.state.store(asset_state::LOADED);
        asset_slot failed = {};
        failed.state.store(asset_state::FAILED);
        asset_slot pending = {};
        pending.state.store(asset_state::LOADING);
        std::vector<asset_handle> handles = {};
        handles.emplace_back(&loaded);
        handles.emplace_back(&failed);
        handles.emplace_back(&pending);
        handles.emplace_back();
        const asset_load_batch batch(std::move(handles));
        CHECK(batch.size() == 4);
        CHECK_FALSE(batch.is_complete());
        CHECK(batch.progress() == doctest::Approx(0.75f));
        CHECK(batch.loaded_count() == 1);
        CHECK(batch.handles().size() == 4);
        pending.state.store(asset_state::LOADED);
        CHECK(batch.is_complete());
        CHECK(batch.progress() == 1.0f);
        CHECK(batch.loaded_count() == 2);
    }
}
