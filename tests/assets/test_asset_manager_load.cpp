#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/core/jobs/dedicated_thread.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    struct load_fixture {
        pipeline_fixture pipeline;
        memory_asset_source *source = nullptr;

        load_fixture(u32 workers = 0) :
            pipeline(workers) {
            auto owned = std::make_unique<memory_asset_source>();
            source = owned.get();
            pipeline.manager->swap_source(std::move(owned));
        }

        asset_manager &manager() {
            return *pipeline.manager;
        }

        void add(const asset_id &id, std::string_view content, std::vector<asset_id> dependencies = {}) {
            source->add(make_test_blob(id, content, std::move(dependencies)));
        }
    };

    struct event_log {
        std::vector<asset_event> events = {};
        std::vector<core::error> errors = {};
        asset_subscription subscription = {};

        explicit event_log(asset_manager &manager) {
            subscription = manager.subscribe([this](const asset_event &event) {
                events.push_back(event);
                errors.emplace_back(event.error != nullptr ? *event.error : core::error());
                events.back().error = nullptr;
            });
        }

        usize count(asset_event_kind kind) const {
            usize total = 0;
            for (const asset_event &event : events) {
                if (event.kind == kind) {
                    ++total;
                }
            }
            return total;
        }
    };

    /// <summary>
    /// A manager with desc's timing settings on the fixture's job system, with the test types and a
    /// source of its own holding the test blobs 1 to blobCount.
    /// </summary>
    std::unique_ptr<asset_manager> make_timed_manager(load_fixture &fixture, asset_manager_desc desc, u64 blobCount) {
        desc.jobs = &fixture.pipeline.jobs;
        auto source = std::make_unique<memory_asset_source>();
        for (u64 index = 1; index <= blobCount; ++index) {
            source->add(make_test_blob(test_id(index), "n" + std::to_string(index)));
        }
        desc.source = std::move(source);
        auto manager = std::make_unique<asset_manager>(std::move(desc));
        register_test_types(manager->types());
        return manager;
    }
}

TEST_SUITE("assets/asset_manager_load") {

    TEST_CASE("load - an async load reaches LOADED through update and fires LOADED once") {
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            load_fixture fixture(workers);
            fixture.add(test_id(1), "one");
            event_log events(fixture.manager());

            const asset_ref<test_asset> ref = fixture.manager().bind<test_asset>(test_id(1));
            asset_handle handle = fixture.manager().load(test_id(1));
            CHECK(handle.state() == asset_state::QUEUED);
            CHECK(fixture.manager().stats().pending == 1);
            CHECK(ref.get() == nullptr);

            fixture.manager().flush();
            CHECK(handle.state() == asset_state::LOADED);
            REQUIRE(ref.get() != nullptr);
            CHECK(ref.get()->content == "one");
            CHECK(ref.get()->finaliseCount == 1);
            CHECK(ref.version() == 1);
            CHECK(fixture.manager().stats().loaded == 1);
            CHECK(fixture.manager().stats().pending == 0);
            CHECK(fixture.manager().stats().bytesResident == 3);
            REQUIRE(events.events.size() == 1);
            CHECK(events.events[0].kind == asset_event_kind::LOADED);
            CHECK(events.events[0].id == test_id(1));
            CHECK(events.events[0].type == asset_type_traits<test_asset>::id());
            CHECK(events.events[0].version == 1);
            CHECK(controls().loadCpuCalls.load() == 1);
            CHECK(controls().finaliseCalls.load() == 1);
        }
    }

    TEST_CASE("load - re-requesting a queued or loaded asset is idempotent") {
        load_fixture fixture = {};
        fixture.add(test_id(1), "one");
        asset_handle first = fixture.manager().load(test_id(1));
        asset_handle second = fixture.manager().load(test_id(1));
        CHECK(first == second);
        CHECK(first.slot()->strongRefs.load() == 2);
        fixture.manager().flush();
        asset_handle third = fixture.manager().load(test_id(1));
        CHECK(third.state() == asset_state::LOADED);
        CHECK(controls().loadCpuCalls.load() == 1);
        CHECK(fixture.manager().stats().loaded == 1);
    }

    TEST_CASE("load - the finalise budget decides how many ready slots one update finalises") {
        f64 budgetSeconds = 0.0;
        u64 loadedPerUpdate = 1;
        SUBCASE("a budget of 0 finalises one") {
            budgetSeconds = 0.0;
        }
        SUBCASE("a negative budget counts as 0") {
            budgetSeconds = -1.0;
        }
        SUBCASE("negative infinity counts as 0") {
            budgetSeconds = -std::numeric_limits<f64>::infinity();
        }
        SUBCASE("NaN counts as 0") {
            budgetSeconds = std::numeric_limits<f64>::quiet_NaN();
        }
        SUBCASE("infinity is held at the longest duration and finalises all") {
            budgetSeconds = std::numeric_limits<f64>::infinity();
            loadedPerUpdate = 3;
        }
        SUBCASE("a budget beyond the longest duration is held there") {
            budgetSeconds = 1.0e15;
            loadedPerUpdate = 3;
        }
        CAPTURE(budgetSeconds);
        CAPTURE(loadedPerUpdate);

        load_fixture fixture = {};
        asset_manager_desc desc = {};
        desc.finaliseBudgetSeconds = budgetSeconds;
        const std::unique_ptr<asset_manager> manager = make_timed_manager(fixture, std::move(desc), 3);
        std::vector<asset_handle> handles = {};
        for (u64 index = 1; index <= 3; ++index) {
            handles.push_back(manager->load(test_id(index)));
        }

        // Each update dispatches (inline jobs complete), drains and finalises up to the budget.
        for (u64 update = 1; update <= 3; ++update) {
            CAPTURE(update);
            manager->update();
            CHECK(manager->stats().loaded == std::min<u64>(3, update * loadedPerUpdate));
        }
        handles.clear();
    }

    TEST_CASE("load - priorities decide the finalise order, FIFO within a level") {
        load_fixture fixture = {};
        asset_manager_desc desc = {};
        desc.jobs = &fixture.pipeline.jobs;
        desc.finaliseBudgetSeconds = 0.0;
        auto source = std::make_unique<memory_asset_source>();
        for (u64 index = 1; index <= 4; ++index) {
            source->add(make_test_blob(test_id(index), "p"));
        }
        desc.source = std::move(source);
        asset_manager manager(std::move(desc));
        register_test_types(manager.types());
        std::vector<asset_id> order = {};
        const asset_subscription subscription = manager.subscribe([&order](const asset_event &event) {
            order.push_back(event.id);
        });

        asset_handle low = manager.load(test_id(1), asset_load_priority::LOW);
        asset_handle normalA = manager.load(test_id(2), asset_load_priority::NORMAL);
        asset_handle high = manager.load(test_id(3), asset_load_priority::HIGH);
        asset_handle normalB = manager.load(test_id(4), asset_load_priority::NORMAL);
        manager.flush();
        REQUIRE(order.size() == 4);
        CHECK(order[0] == test_id(3));
        CHECK(order[1] == test_id(2));
        CHECK(order[2] == test_id(4));
        CHECK(order[3] == test_id(1));
        manager.unsubscribe(subscription);
    }

    TEST_CASE("load - raising the priority of a queued load takes effect") {
        load_fixture fixture = {};
        asset_manager_desc desc = {};
        desc.jobs = &fixture.pipeline.jobs;
        desc.finaliseBudgetSeconds = 0.0;
        auto source = std::make_unique<memory_asset_source>();
        source->add(make_test_blob(test_id(1), "a"));
        source->add(make_test_blob(test_id(2), "b"));
        desc.source = std::move(source);
        asset_manager manager(std::move(desc));
        register_test_types(manager.types());
        std::vector<asset_id> order = {};
        const asset_subscription subscription = manager.subscribe([&order](const asset_event &event) {
            order.push_back(event.id);
        });
        asset_handle first = manager.load(test_id(1), asset_load_priority::LOW);
        asset_handle second = manager.load(test_id(2), asset_load_priority::LOW);
        asset_handle again = manager.load(test_id(2), asset_load_priority::HIGH);
        manager.flush();
        REQUIRE(order.size() == 2);
        CHECK(order[0] == test_id(2));
        CHECK(order[1] == test_id(1));
        manager.unsubscribe(subscription);
    }

    TEST_CASE("load_blocking - succeeds inline, dependencies first, and returns the error of a failure") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.add(test_id(1), "leaf");
        fixture.add(test_id(2), "root", {test_id(1)});

        core::result<asset_handle> root = fixture.manager().load_blocking(test_id(2));
        CONJURE_REQUIRE_OK(root);
        CHECK(root.value().state() == asset_state::LOADED);
        CHECK(fixture.manager().state_of(test_id(1)) == asset_state::LOADED);
        CHECK(root.value().get<test_asset>()->content == "root");
        CHECK(fixture.manager().stats().pending == 0);

        // Already LOADED: returns immediately.
        CONJURE_REQUIRE_OK(fixture.manager().load_blocking(test_id(2)));
        CHECK(controls().loadCpuCalls.load() == 2);

        CONJURE_CHECK_ERROR(fixture.manager().load_blocking(test_id(9)), asset_error_code::NOT_FOUND);
        CHECK(fixture.manager().state_of(test_id(9)) == asset_state::FAILED);
        CONJURE_CHECK_ERROR(fixture.manager().load_blocking(test_id(9)), asset_error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(fixture.manager().load_blocking(asset_id()), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("load_blocking - inside a frame it is permitted and warns once per frame") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.add(test_id(1), "a");
        fixture.add(test_id(2), "b");
        fixture.manager().begin_frame(1);
        CONJURE_REQUIRE_OK(fixture.manager().load_blocking(test_id(1)));
        CONJURE_REQUIRE_OK(fixture.manager().load_blocking(test_id(2)));
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "hitch"));
        fixture.manager().end_frame();
        fixture.manager().begin_frame(2);
        fixture.manager().reload(test_id(1));
        CONJURE_REQUIRE_OK(fixture.manager().load_blocking(test_id(1)));
        CHECK(logs.count(core::log_level::WARN) == 2);
        fixture.manager().end_frame();
    }

    TEST_CASE("load_blocking - takes over a queued load and waits for an in-flight job [parallel]") {
        load_fixture fixture(4);
        fixture.add(test_id(1), "queued");
        fixture.add(test_id(2), "in flight");
        asset_handle queued = fixture.manager().load(test_id(1));
        CHECK(queued.state() == asset_state::QUEUED);
        CONJURE_REQUIRE_OK(fixture.manager().load_blocking(test_id(1)));
        CHECK(queued.state() == asset_state::LOADED);
        fixture.manager().flush();
        CHECK(controls().loadCpuCalls.load() == 1);

        controls().blockLoadCpu = true;
        asset_handle inFlight = fixture.manager().load(test_id(2));
        fixture.manager().update();
        CHECK(inFlight.state() == asset_state::LOADING);
        CHECK(fixture.manager().stats().jobsInFlight == 1);
        controls().blockLoadCpu = false;
        CONJURE_REQUIRE_OK(fixture.manager().load_blocking(test_id(2)));
        CHECK(inFlight.state() == asset_state::LOADED);
        CHECK(fixture.manager().stats().jobsInFlight == 0);
    }

    TEST_CASE("load_blocking - times out when the job owning the slot does not finish [parallel]") {
        f64 timeoutSeconds = 0.0;
        SUBCASE("a timeout of 0") {
            timeoutSeconds = 0.0;
        }
        SUBCASE("a negative timeout counts as 0") {
            timeoutSeconds = -1.0;
        }
        SUBCASE("negative infinity counts as 0") {
            timeoutSeconds = -std::numeric_limits<f64>::infinity();
        }
        SUBCASE("NaN counts as 0") {
            timeoutSeconds = std::numeric_limits<f64>::quiet_NaN();
        }
        CAPTURE(timeoutSeconds);

        load_fixture fixture(4);
        asset_manager_desc desc = {};
        desc.blockingTimeoutSeconds = timeoutSeconds;
        const std::unique_ptr<asset_manager> manager = make_timed_manager(fixture, std::move(desc), 1);

        controls().blockLoadCpu = true;
        asset_handle handle = manager->load(test_id(1));
        manager->update();
        REQUIRE(handle.state() == asset_state::LOADING);
        CONJURE_CHECK_ERROR(manager->load_blocking(test_id(1)), asset_error_code::TIMEOUT);
        CHECK(handle.state() == asset_state::LOADING);
        controls().blockLoadCpu = false;
        manager->flush();
        CHECK(handle.state() == asset_state::LOADED);
        handle.reset();
    }

    TEST_CASE("load_blocking - an infinite timeout waits for the job owning the slot [parallel]") {
        // The conversion that holds infinity at 1e9 s is pinned exactly by test_durations.cpp; this case
        // shows that load_blocking waits on the converted timeout instead of timing out at once.
        load_fixture fixture(4);
        asset_manager_desc desc = {};
        desc.blockingTimeoutSeconds = std::numeric_limits<f64>::infinity();
        const std::unique_ptr<asset_manager> manager = make_timed_manager(fixture, std::move(desc), 1);

        controls().blockLoadCpu = true;
        asset_handle handle = manager->load(test_id(1));
        manager->update();
        REQUIRE(handle.state() == asset_state::LOADING);

        // The job is released once load_blocking has logged its in-frame WARN, which it does right
        // before it sets its deadline and waits. This depends on that order in
        // asset_manager.cpp (state::load_blocking): were the WARN moved after the wait or removed,
        // the job would never be released and the case would hang until the CTest timeout.
        conjure::testing::log_capture logs = {};
        manager->begin_frame(1);
        core::jobs::dedicated_thread release("release-load", [&logs](std::stop_token stop) {
            while (!stop.stop_requested() && !logs.contains(core::log_level::WARN, "is a hitch")) {
                std::this_thread::yield();
            }
            controls().blockLoadCpu = false;
        });
        const core::result<asset_handle> loaded = manager->load_blocking(test_id(1));
        release.request_stop();
        release.join();
        manager->end_frame();
        CONJURE_REQUIRE_OK(loaded);
        CHECK(handle.state() == asset_state::LOADED);
        handle.reset();
    }

    TEST_CASE("load_blocking - a dependency timeout leaves the dependent to the async pipeline [parallel]") {
        load_fixture fixture(4);
        asset_manager_desc desc = {};
        desc.jobs = &fixture.pipeline.jobs;
        desc.blockingTimeoutSeconds = 0.0;
        auto source = std::make_unique<memory_asset_source>();
        source->add(make_test_blob(test_id(1), "dependent", {test_id(2)}));
        source->add(make_test_blob(test_id(2), "dependency"));
        desc.source = std::move(source);
        asset_manager manager(std::move(desc));
        register_test_types(manager.types());

        // A finishes its stage and waits in FINALISING; the load of B it queued is then held in
        // load_cpu, so load_blocking(A) times out on B.
        // Each wait step blocks in wait_idle() until the worker jobs have run, so a loaded machine
        // that leaves a worker unscheduled for a while slows the case down instead of failing it.
        asset_handle dependent = manager.load(test_id(1));
        for (u32 step = 0; step < 100 && dependent.state() != asset_state::FINALISING; ++step) {
            fixture.pipeline.jobs.wait_idle();
            manager.update();
        }
        REQUIRE(dependent.state() == asset_state::FINALISING);
        REQUIRE(manager.state_of(test_id(2)) == asset_state::QUEUED);
        controls().blockLoadCpu = true;
        manager.update();
        CHECK(manager.state_of(test_id(2)) == asset_state::LOADING);   // no REQUIRE: B's job must be released
        CONJURE_CHECK_ERROR(manager.load_blocking(test_id(1)), asset_error_code::TIMEOUT);
        CHECK(dependent.state() == asset_state::FINALISING);

        // Once B lands, the normal pipeline finalises A.
        controls().blockLoadCpu = false;
        for (u32 step = 0; step < 100 && !dependent.is_terminal(); ++step) {
            fixture.pipeline.jobs.wait_idle();
            manager.update();
        }
        CHECK(dependent.state() == asset_state::LOADED);
        CHECK(manager.state_of(test_id(2)) == asset_state::LOADED);
        dependent.reset();
    }

    TEST_CASE("load - an unknown uuid fails with NOT_FOUND and logs an ERROR once") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        event_log events(fixture.manager());
        asset_handle handle = fixture.manager().load(test_id(1));
        fixture.manager().flush();
        CHECK(handle.state() == asset_state::FAILED);
        CHECK(handle.is_terminal());
        CHECK(handle.get<test_asset>() == nullptr);
        REQUIRE(fixture.manager().last_error(test_id(1)) != nullptr);
        CHECK(fixture.manager().last_error(test_id(1))->is(asset_error_code::NOT_FOUND));
        REQUIRE(events.events.size() == 1);
        CHECK(events.events[0].kind == asset_event_kind::FAILED);
        CHECK(events.errors[0].is(asset_error_code::NOT_FOUND));
        CHECK(logs.count(core::log_level::ERR) == 1);

        fixture.manager().retry_failed(test_id(1));
        fixture.manager().flush();
        CHECK(handle.state() == asset_state::FAILED);
        CHECK(logs.count(core::log_level::ERR) == 1);
        CHECK(events.count(asset_event_kind::FAILED) == 2);

        // load() does not retry a FAILED slot; retry_failed after the blob appears does.
        fixture.add(test_id(1), "late");
        asset_handle again = fixture.manager().load(test_id(1));
        fixture.manager().flush();
        CHECK(again.state() == asset_state::FAILED);
        fixture.manager().retry_failed(test_id(1));
        fixture.manager().flush();
        CHECK(again.state() == asset_state::LOADED);
        CHECK(fixture.manager().last_error(test_id(1)) == nullptr);
    }

    TEST_CASE("load - a corrupt blob, a type mismatch and loader failures end FAILED with the right code") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();

        cooked_blob corrupt = make_test_blob(test_id(1), "corrupt");
        corrupt.header.type = make_asset_type_id("conjure.test.unregistered");
        fixture.source->add(std::move(corrupt));
        asset_handle noLoader = fixture.manager().load(test_id(1));

        // Bound as other_asset before the source knows the uuid; the blob then says test_asset.
        static_cast<void>(fixture.manager().bind<other_asset>(test_id(2)));
        fixture.add(test_id(2), "wrong type");
        asset_handle mismatch = fixture.manager().load(test_id(2));
        fixture.manager().flush();
        CHECK(fixture.manager().last_error(test_id(1))->is(asset_error_code::NO_LOADER));
        CHECK(fixture.manager().last_error(test_id(2))->is(asset_error_code::TYPE_MISMATCH));

        fixture.add(test_id(3), "cpu fails");
        controls().failLoadCpu = true;
        asset_handle cpu = fixture.manager().load(test_id(3));
        fixture.manager().flush();
        controls().failLoadCpu = false;
        CHECK(cpu.state() == asset_state::FAILED);
        CHECK(fixture.manager().last_error(test_id(3))->is(asset_error_code::LOAD_FAILED));

        fixture.add(test_id(4), "finalise fails");
        controls().failFinalise = true;
        asset_handle finalise = fixture.manager().load(test_id(4));
        fixture.manager().flush();
        controls().failFinalise = false;
        CHECK(finalise.state() == asset_state::FAILED);
        CHECK(fixture.manager().last_error(test_id(4))->is(asset_error_code::FINALISE_FAILED));
        CHECK(fixture.manager().stats().failed == 4);
        CHECK(logs.count(core::log_level::ERR) == 0);   // only NOT_FOUND is an ERROR; these WARN
    }

    TEST_CASE("load - dependencies finalise before dependents and residency follows the root") {
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            load_fixture fixture(workers);
            fixture.add(test_id(1), "texture");
            fixture.add(test_id(2), "material", {test_id(1)});
            fixture.add(test_id(3), "model", {test_id(2), test_id(1)});
            std::vector<asset_id> order = {};
            const asset_subscription subscription = fixture.manager().subscribe([&order](const asset_event &event) {
                if (event.kind == asset_event_kind::LOADED) {
                    order.push_back(event.id);
                }
            });

            asset_handle model = fixture.manager().load(test_id(3));
            fixture.manager().flush();
            REQUIRE(order.size() == 3);
            CHECK(order[0] == test_id(1));
            CHECK(order[1] == test_id(2));
            CHECK(order[2] == test_id(3));
            CHECK(model.get<test_asset>()->dependencies.size() == 2);

            // Dependencies are held by the root: collect releases the whole tree once the root goes.
            CHECK(fixture.manager().collect() == 0);
            CHECK(fixture.manager().state_of(test_id(1)) == asset_state::LOADED);
            model.reset();
            CHECK(fixture.manager().collect() == 3);
            CHECK(fixture.manager().state_of(test_id(1)) == asset_state::UNLOADED);
            CHECK(fixture.manager().state_of(test_id(2)) == asset_state::UNLOADED);
            fixture.manager().unsubscribe(subscription);
        }
    }

    TEST_CASE("load - a failed dependency does not fail the dependent and warns once") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.add(test_id(2), "dependent", {test_id(1)});
        asset_handle dependent = fixture.manager().load(test_id(2));
        fixture.manager().flush();
        CHECK(dependent.state() == asset_state::LOADED);
        CHECK(fixture.manager().state_of(test_id(1)) == asset_state::FAILED);
        CHECK(logs.contains(core::log_level::WARN, "failed to load; it will read the fallback"));
        const asset_ref<test_asset> missing = fixture.manager().bind<test_asset>(test_id(1));
        CHECK(missing.get() == nullptr);
        CHECK(fixture.manager().get_or_fallback(missing)->content == "fallback");

        fixture.manager().reload(test_id(2));
        fixture.manager().flush();
        CHECK(logs.count(core::log_level::WARN) >= 1);
    }

    TEST_CASE("load - a dependency cycle fails every member with CYCLIC_DEPENDENCY") {
        for (const bool blocking : {false, true}) {
            CAPTURE(blocking);
            load_fixture fixture = {};
            conjure::testing::log_capture logs = {};
            fixture.add(test_id(1), "a", {test_id(2)});
            fixture.add(test_id(2), "b", {test_id(3)});
            fixture.add(test_id(3), "c", {test_id(1)});
            fixture.add(test_id(4), "outside", {test_id(1)});
            if (blocking) {
                CONJURE_CHECK_ERROR(fixture.manager().load_blocking(test_id(1)), asset_error_code::CYCLIC_DEPENDENCY);
            } else {
                asset_handle a = fixture.manager().load(test_id(1));
                fixture.manager().flush();
                CHECK(a.state() == asset_state::FAILED);
            }
            for (u64 index = 1; index <= 3; ++index) {
                CAPTURE(index);
                CHECK(fixture.manager().state_of(test_id(index)) == asset_state::FAILED);
                REQUIRE(fixture.manager().last_error(test_id(index)) != nullptr);
                CHECK(fixture.manager().last_error(test_id(index))->is(asset_error_code::CYCLIC_DEPENDENCY));
            }
            asset_handle outside = fixture.manager().load(test_id(4));
            fixture.manager().flush();
            CHECK(outside.state() == asset_state::LOADED);
        }
    }

    TEST_CASE("regression - a reversed dependency reloads without a false cycle or a leaked job [parallel]") {
        // With inline jobs both reload completions drain in dispatch order: A's fresh list says
        // "B" while B's in-flight reload still carries its stale list "A", which the old check
        // walked and reported as a cycle; B's skipped completion then kept jobsInFlight at 1.
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            load_fixture fixture(workers);
            conjure::testing::log_capture logs = {};
            fixture.add(test_id(1), "a");
            fixture.add(test_id(2), "b", {test_id(1)});
            event_log events(fixture.manager());
            asset_handle a = fixture.manager().load(test_id(1));
            asset_handle b = fixture.manager().load(test_id(2));
            fixture.manager().flush();
            REQUIRE(a.state() == asset_state::LOADED);
            REQUIRE(b.state() == asset_state::LOADED);

            fixture.add(test_id(1), "a2", {test_id(2)});
            fixture.add(test_id(2), "b2");
            fixture.manager().reload(test_id(1));
            fixture.manager().reload(test_id(2));
            fixture.manager().flush();

            CHECK(events.count(asset_event_kind::RELOAD_FAILED) == 0);
            CHECK(events.count(asset_event_kind::RELOADED) == 2);
            CHECK(a.state() == asset_state::LOADED);
            CHECK(b.state() == asset_state::LOADED);
            CHECK(a.get<test_asset>()->content == "a2");
            CHECK(b.get<test_asset>()->content == "b2");
            CHECK(a.get<test_asset>()->dependencies == std::vector<asset_id>{test_id(2)});
            CHECK(fixture.manager().stats().jobsInFlight == 0);
            CHECK(fixture.manager().stats().pending == 0);
            CHECK(logs.count(core::log_level::WARN) == 0);

            // Dispatch concurrency is intact: a burst of loads all reach LOADED.
            std::vector<asset_id> ids = {};
            for (u64 index = 10; index < 10 + DEFAULT_MAX_JOBS_IN_FLIGHT * 2; ++index) {
                ids.push_back(test_id(index));
                fixture.add(test_id(index), "n");
            }
            asset_load_batch batch = fixture.manager().load_batch(ids);
            fixture.manager().flush();
            CHECK(batch.loaded_count() == ids.size());
            CHECK(fixture.manager().stats().jobsInFlight == 0);
        }
    }

    TEST_CASE("regression - a real cycle between reloading members is still reported") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.add(test_id(1), "a");
        fixture.add(test_id(2), "b", {test_id(1)});
        asset_handle a = fixture.manager().load(test_id(1));
        asset_handle b = fixture.manager().load(test_id(2));
        fixture.manager().flush();
        event_log events(fixture.manager());

        fixture.add(test_id(1), "a2", {test_id(2)});
        fixture.add(test_id(2), "b2", {test_id(1)});
        fixture.manager().reload(test_id(1));
        fixture.manager().reload(test_id(2));
        fixture.manager().flush();
        CHECK(events.count(asset_event_kind::RELOAD_FAILED) == 2);
        CHECK(a.state() == asset_state::LOADED);   // failed reloads keep the old payloads
        CHECK(a.get<test_asset>()->content == "a");
        CHECK(b.get<test_asset>()->content == "b");
        REQUIRE(fixture.manager().last_error(test_id(1)) != nullptr);
        CHECK(fixture.manager().last_error(test_id(1))->is(asset_error_code::CYCLIC_DEPENDENCY));
        CHECK(fixture.manager().stats().jobsInFlight == 0);
        CHECK(fixture.manager().stats().pending == 0);
    }

    TEST_CASE("load_batch - progress and completion") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        asset_manager_desc desc = {};
        desc.jobs = &fixture.pipeline.jobs;
        desc.finaliseBudgetSeconds = 0.0;
        auto source = std::make_unique<memory_asset_source>();
        source->add(make_test_blob(test_id(1), "a"));
        source->add(make_test_blob(test_id(2), "b"));
        desc.source = std::move(source);
        asset_manager manager(std::move(desc));
        register_test_types(manager.types());

        const std::vector<asset_id> ids = {test_id(1), test_id(2), test_id(3)};
        asset_load_batch batch = manager.load_batch(ids, asset_load_priority::HIGH);
        CHECK(batch.size() == 3);
        CHECK_FALSE(batch.is_complete());
        CHECK(batch.progress() == 0.0f);
        manager.update();
        CHECK(batch.progress() > 0.0f);
        CHECK_FALSE(batch.is_complete());
        manager.flush();
        CHECK(batch.is_complete());
        CHECK(batch.progress() == 1.0f);
        CHECK(batch.loaded_count() == 2);
        CHECK(batch.handles()[2].state() == asset_state::FAILED);
        batch = asset_load_batch();
        CHECK(manager.collect() == 2);
    }

    TEST_CASE("collect - unloads only zero-ref unpinned slots, releases dependency handles and fires UNLOADED") {
        load_fixture fixture = {};
        fixture.add(test_id(1), "leaf");
        fixture.add(test_id(2), "root", {test_id(1)});
        fixture.add(test_id(3), "held");
        fixture.add(test_id(4), "pinned");
        event_log events(fixture.manager());
        const asset_ref<test_asset> leafRef = fixture.manager().bind<test_asset>(test_id(1));
        asset_handle root = fixture.manager().load(test_id(2));
        asset_handle held = fixture.manager().load(test_id(3));
        asset_handle pinned = fixture.manager().load(test_id(4));
        fixture.manager().pin(test_id(4), true);
        fixture.manager().flush();
        CHECK(fixture.manager().stats().loaded == 4);
        CHECK(leafRef.version() == 1);

        root.reset();
        pinned.reset();
        CHECK(fixture.manager().collect() == 2);
        CHECK(fixture.manager().state_of(test_id(2)) == asset_state::UNLOADED);
        CHECK(fixture.manager().state_of(test_id(1)) == asset_state::UNLOADED);
        CHECK(fixture.manager().state_of(test_id(3)) == asset_state::LOADED);
        CHECK(fixture.manager().state_of(test_id(4)) == asset_state::LOADED);
        CHECK(leafRef.get() == nullptr);
        CHECK(leafRef.version() == 2);
        CHECK(fixture.manager().stats().retired == 2);
        fixture.manager().update();
        CHECK(events.count(asset_event_kind::UNLOADED) == 2);
        CHECK(fixture.manager().collect() == 0);
        CHECK(fixture.manager().stats().loaded == 2);
    }

    TEST_CASE("publish - a retired payload is destroyed after exactly RETIRE_FRAMES frames") {
        load_fixture fixture = {};
        fixture.source->add(make_test_blob(test_id(1), "v1", {}, asset_type_traits<counted_asset>::id()));
        asset_handle handle = fixture.manager().load(test_id(1));
        fixture.manager().begin_frame(10);
        fixture.manager().flush();
        fixture.manager().end_frame();
        const counted_asset *first = handle.get<counted_asset>();
        REQUIRE(first != nullptr);
        CHECK(first->content == "v1");
        CHECK(controls().liveCountedAssets.load() == 1);

        fixture.source->add(make_test_blob(test_id(1), "v2", {}, asset_type_traits<counted_asset>::id()));
        fixture.manager().begin_frame(11);
        fixture.manager().reload(test_id(1));
        fixture.manager().flush();
        fixture.manager().end_frame();
        CHECK(handle.get<counted_asset>() != first);
        CHECK(handle.get<counted_asset>()->content == "v2");
        CHECK(handle.version() == 2);
        CHECK(fixture.manager().stats().retired == 1);
        CHECK(first->content == "v1");   // still alive: valid until the end of the next frame
        CHECK(controls().liveCountedAssets.load() == 2);

        fixture.manager().begin_frame(12);
        fixture.manager().update();
        fixture.manager().end_frame();
        CHECK(fixture.manager().stats().retired == 1);
        CHECK(controls().liveCountedAssets.load() == 2);

        fixture.manager().begin_frame(13);
        fixture.manager().update();
        fixture.manager().end_frame();
        CHECK(fixture.manager().stats().retired == 0);
        CHECK(controls().liveCountedAssets.load() == 1);
    }

    TEST_CASE("reload - keeps the old payload published until the new one finalises, and on failure") {
        load_fixture fixture = {};
        fixture.add(test_id(1), "v1");
        event_log events(fixture.manager());
        const asset_ref<test_asset> ref = fixture.manager().bind<test_asset>(test_id(1));
        asset_handle handle = fixture.manager().load(test_id(1));
        fixture.manager().flush();
        const test_asset *old = ref.get();

        fixture.add(test_id(1), "v2");
        fixture.manager().reload(test_id(1));
        CHECK(ref.state() == asset_state::RELOADING);
        CHECK(ref.get() == old);
        CHECK(fixture.manager().stats().loaded == 1);
        fixture.manager().flush();
        CHECK(ref.state() == asset_state::LOADED);
        CHECK(ref.get() != old);
        CHECK(ref.get()->content == "v2");
        CHECK(ref.version() == 2);
        CHECK(events.count(asset_event_kind::RELOADED) == 1);

        controls().failLoadCpu = true;
        fixture.manager().reload(test_id(1));
        fixture.manager().flush();
        controls().failLoadCpu = false;
        CHECK(ref.state() == asset_state::LOADED);
        CHECK(ref.get()->content == "v2");
        CHECK(ref.version() == 2);
        CHECK(events.count(asset_event_kind::RELOAD_FAILED) == 1);
        REQUIRE(fixture.manager().last_error(test_id(1)) != nullptr);
        CHECK(fixture.manager().last_error(test_id(1))->is(asset_error_code::LOAD_FAILED));

        fixture.manager().reload(test_id(9));   // unknown: ignored
        fixture.manager().retry_failed(test_id(1));   // not FAILED: ignored
        CHECK(ref.state() == asset_state::LOADED);
    }

    TEST_CASE("regression - a reload that fails in finalise keeps the published payload's statistics") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.add(test_id(1), "v1");
        asset_handle handle = fixture.manager().load(test_id(1));
        fixture.manager().flush();
        REQUIRE(handle.state() == asset_state::LOADED);
        const u64 bytesBefore = fixture.manager().stats().bytesResident;
        CHECK(bytesBefore == 2);

        fixture.add(test_id(1), std::string(64, 'x'));
        controls().failFinalise = true;
        fixture.manager().reload(test_id(1));
        fixture.manager().flush();
        controls().failFinalise = false;
        CHECK(handle.state() == asset_state::LOADED);
        CHECK(handle.get<test_asset>()->content == "v1");
        CHECK(fixture.manager().stats().bytesResident == bytesBefore);
        REQUIRE(fixture.manager().last_error(test_id(1)) != nullptr);
        CHECK(fixture.manager().last_error(test_id(1))->is(asset_error_code::FINALISE_FAILED));

        // A reload that succeeds commits the new size.
        fixture.manager().reload(test_id(1));
        fixture.manager().flush();
        CHECK(handle.get<test_asset>()->content == std::string(64, 'x'));
        CHECK(fixture.manager().stats().bytesResident == 64);
    }

    TEST_CASE("regression - collect clears the error of an earlier failed reload") {
        load_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.add(test_id(1), "v1");
        asset_handle handle = fixture.manager().load(test_id(1));
        fixture.manager().flush();
        controls().failFinalise = true;
        fixture.manager().reload(test_id(1));
        fixture.manager().flush();
        controls().failFinalise = false;
        REQUIRE(handle.state() == asset_state::LOADED);
        CHECK(fixture.manager().last_error(test_id(1)) != nullptr);

        handle.reset();
        CHECK(fixture.manager().collect() == 1);
        CHECK(fixture.manager().state_of(test_id(1)) == asset_state::UNLOADED);
        CHECK(fixture.manager().last_error(test_id(1)) == nullptr);
        CHECK(fixture.manager().stats().bytesResident == 0);
    }

    TEST_CASE("regression - a load batch stays complete while a member hot-reloads") {
        load_fixture fixture = {};
        fixture.add(test_id(1), "v1");
        asset_load_batch batch = fixture.manager().load_batch(std::vector<asset_id>{test_id(1)});
        fixture.manager().flush();
        REQUIRE(batch.is_complete());

        fixture.add(test_id(1), "v2");
        fixture.manager().reload(test_id(1));
        REQUIRE(fixture.manager().state_of(test_id(1)) == asset_state::RELOADING);
        CHECK(batch.is_complete());
        CHECK(batch.progress() == 1.0f);
        CHECK(batch.loaded_count() == 1);

        fixture.manager().flush();
        CHECK(batch.is_complete());
        CHECK(batch.loaded_count() == 1);
        CHECK(batch.handles()[0].get<test_asset>()->content == "v2");
    }

    TEST_CASE("events - fire once each on the main thread in order, after finalisation") {
        load_fixture fixture(4);
        fixture.add(test_id(1), "a");
        fixture.add(test_id(2), "b", {test_id(1)});
        event_log events(fixture.manager());
        asset_handle handle = fixture.manager().load(test_id(2));
        fixture.manager().flush();
        REQUIRE(events.events.size() == 2);
        CHECK(events.events[0].id == test_id(1));
        CHECK(events.events[1].id == test_id(2));
        fixture.manager().update();
        CHECK(events.events.size() == 2);
    }

    TEST_CASE("flush - returns once every queued load is terminal [parallel]") {
        load_fixture fixture(4);
        for (u64 index = 1; index <= 64; ++index) {
            fixture.add(
              test_id(index),
              "n" + std::to_string(index),
              index > 1 ? std::vector<asset_id>{test_id(index - 1)} : std::vector<asset_id>{}
            );
        }
        asset_handle top = fixture.manager().load(test_id(64));
        fixture.manager().flush();
        CHECK(fixture.manager().stats().pending == 0);
        CHECK(fixture.manager().stats().loaded == 64);
        CHECK(fixture.manager().stats().jobsInFlight == 0);
        CHECK(top.get<test_asset>()->content == "n64");
    }

    TEST_CASE("shutdown - with in-flight jobs waits, discards completions and resets queued slots [parallel]") {
        load_fixture fixture(4);
        for (u64 index = 1; index <= 12; ++index) {
            fixture.add(test_id(index), "n" + std::to_string(index));
        }
        controls().blockLoadCpu = true;
        std::vector<asset_handle> handles = {};
        for (u64 index = 1; index <= 12; ++index) {
            handles.push_back(fixture.manager().load(test_id(index)));
        }
        fixture.manager().update();
        CHECK(fixture.manager().stats().jobsInFlight == DEFAULT_MAX_JOBS_IN_FLIGHT);
        CHECK(fixture.manager().stats().pending == 12);

        controls().blockLoadCpu = false;
        fixture.manager().shutdown();
        CHECK(fixture.manager().stats().jobsInFlight == 0);
        CHECK(fixture.manager().stats().pending == 0);
        CHECK(fixture.manager().stats().loaded == 0);
        for (const asset_handle &handle : handles) {
            CHECK(handle.state() == asset_state::UNLOADED);
        }
    }

    TEST_CASE("load - 1000 blobs load through 4 workers [parallel]") {
        load_fixture fixture(4);
        std::vector<asset_id> ids = {};
        for (u64 index = 1; index <= 1000; ++index) {
            ids.push_back(test_id(index));
            fixture.add(test_id(index), "blob " + std::to_string(index));
        }
        asset_load_batch batch = fixture.manager().load_batch(ids);
        fixture.manager().flush();
        CHECK(batch.is_complete());
        CHECK(batch.loaded_count() == 1000);
        CHECK(fixture.manager().stats().loaded == 1000);
        CHECK(controls().loadCpuCalls.load() == 1000);
    }
}
