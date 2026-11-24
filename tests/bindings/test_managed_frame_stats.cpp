#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/frame_stats.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <memory>
#include <string_view>

// The managed figures of the frame statistics end to end (wave 3 GAP-CSHARP 2.7): the null host reports the
// counters a CLR would, the engine samples them once per frame at step 15 into engine::frame_sample as per-frame
// deltas, and diagnostics_frame_stats copies them into conjure_frame_stats_block for Diagnostics.Stats. The
// [dotnet] case runs the real CLR: an update that allocates shows in the next frame, an idle one shows 0.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr std::string_view FRAME_STATS_PROBE = "Conjure.TestScripts.FrameStatsProbe";

    /// <summary>
    /// The four managed fields of the latest sample and of what diagnostics_frame_stats reports must agree.
    /// </summary>
    void check_block_matches(engine::application &app, const engine::frame_sample &sample) {
        conjure_frame_stats_block block = {};
        REQUIRE(app.context().scripting->native_api().diagnostics_frame_stats(&block) == 0);
        CHECK(block.managed_allocated_bytes == sample.managedAllocatedBytes);
        CHECK(block.managed_heap_bytes == sample.managedHeapBytes);
        CHECK(block.managed_collections == sample.managedCollections);
        CHECK(block.managed_full_collections == sample.managedFullCollections);
    }
}

TEST_SUITE("bindings/managed_frame_stats") {

    TEST_CASE("managed frame stats - each frame reports what the managed heap did during it") {
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        REQUIRE(app->context().scripting != nullptr);
        scripting::null_script_host *host = scripting::null_script_host::active();
        REQUIRE(host != nullptr);

        // The first sample is the baseline: the heap size, and no growth to report yet.
        host->set_managed_memory(conjure_managed_memory{1000, 5000, 3, 1});
        CONJURE_REQUIRE_OK(app->step_frame());
        const engine::frame_sample &first = app->stats().latest();
        CHECK(first.managedAllocatedBytes == 0);
        CHECK(first.managedHeapBytes == 5000);
        CHECK(first.managedCollections == 0);
        CHECK(first.managedFullCollections == 0);
        check_block_matches(*app, first);

        host->set_managed_memory(conjure_managed_memory{2024, 6000, 5, 2});
        CONJURE_REQUIRE_OK(app->step_frame());
        const engine::frame_sample &grown = app->stats().latest();
        CHECK(grown.managedAllocatedBytes == 1024);
        CHECK(grown.managedHeapBytes == 6000);
        CHECK(grown.managedCollections == 2);
        CHECK(grown.managedFullCollections == 1);
        check_block_matches(*app, grown);

        // A frame that allocated nothing reports 0.
        CONJURE_REQUIRE_OK(app->step_frame());
        CHECK(app->stats().latest().managedAllocatedBytes == 0);
        CHECK(app->stats().latest().managedCollections == 0);
        CHECK(app->stats().latest().managedHeapBytes == 6000);
        CHECK(host->recorder().sample_memory_count() == 3);
        CHECK(host->recorder().count("sample_memory") == 0);     // never in calls(): it runs every frame
        app->shutdown();
    }

    TEST_CASE("managed frame stats - a restart or a failed sample reports no growth") {
        log_capture logs = {};
        std::unique_ptr<engine::application> app = make_app(headless_config());
        CONJURE_REQUIRE_OK(app->initialise());
        scripting::null_script_host *host = scripting::null_script_host::active();
        REQUIRE(host != nullptr);
        host->set_managed_memory(conjure_managed_memory{50000, 9000, 40, 4});
        CONJURE_REQUIRE_OK(app->step_frame());

        SUBCASE("the counters went down: a restarted managed runtime") {
            host->set_managed_memory(conjure_managed_memory{100, 3000, 0, 0});
            CONJURE_REQUIRE_OK(app->step_frame());
            CHECK(app->stats().latest().managedAllocatedBytes == 0);
            CHECK(app->stats().latest().managedHeapBytes == 3000);
            CHECK(app->stats().latest().managedCollections == 0);
            // The restart is the new baseline.
            host->set_managed_memory(conjure_managed_memory{400, 3100, 1, 0});
            CONJURE_REQUIRE_OK(app->step_frame());
            CHECK(app->stats().latest().managedAllocatedBytes == 300);
            CHECK(app->stats().latest().managedCollections == 1);
        }
        SUBCASE("the sample failed") {
            host->set_sample_memory_result(scripting::status_of(scripting::scripting_error::NOT_SUPPORTED));
            host->set_managed_memory(conjure_managed_memory{60000, 9500, 41, 4});
            CONJURE_REQUIRE_OK(app->step_frame());
            const engine::frame_sample &failed = app->stats().latest();
            CHECK(failed.managedAllocatedBytes == 0);
            CHECK(failed.managedHeapBytes == 0);
            CHECK(failed.managedCollections == 0);
            check_block_matches(*app, failed);
            // The next good sample starts over from a new baseline.
            host->set_sample_memory_result(0);
            CONJURE_REQUIRE_OK(app->step_frame());
            CHECK(app->stats().latest().managedAllocatedBytes == 0);
            CHECK(app->stats().latest().managedHeapBytes == 9500);
            host->set_managed_memory(conjure_managed_memory{61000, 9600, 41, 4});
            CONJURE_REQUIRE_OK(app->step_frame());
            CHECK(app->stats().latest().managedAllocatedBytes == 1000);
        }
        app->shutdown();
    }

    TEST_CASE("managed frame stats - the CLR's figures reach Diagnostics.Stats [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_app fixture = {};
        engine::application &app = fixture.app();

        SUBCASE("an update that allocates 1 KiB shows at least 1 KiB in the next frame") {
            const scripting::script_entry_id probe =
              fixture.attach(fixture.create("allocating"), FRAME_STATS_PROBE, 0, "{\"AllocateBytes\":1024}");
            for (u32 frame = 0; frame < 10; ++frame) {
                CONJURE_REQUIRE_OK(app.step_frame());
                CAPTURE(frame);
                if (frame >= 2) {
                    CHECK(app.stats().latest().managedAllocatedBytes >= 1024);
                }
            }
            CHECK(fixture.field_i64(probe, "Updates") >= 9);
            CHECK(fixture.field_i64(probe, "KeptLength") == 1024);
            CHECK(fixture.field_i64(probe, "LastAllocatedBytes") >= 1024);
            CHECK(fixture.field_i64(probe, "LastHeapBytes") > 0);
            CHECK(app.stats().latest().managedHeapBytes > 0);
        }
        SUBCASE("updates that allocate nothing show 0 once warm") {
            const scripting::script_entry_id probe =
              fixture.attach(fixture.create("idle"), FRAME_STATS_PROBE, 0, "{\"AllocateBytes\":0}");
            constexpr u32 WARM_UP = 20;
            constexpr u32 MEASURED = 60;
            for (u32 frame = 0; frame < WARM_UP; ++frame) {
                CONJURE_REQUIRE_OK(app.step_frame());
            }
            for (u32 frame = 0; frame < MEASURED; ++frame) {
                CONJURE_REQUIRE_OK(app.step_frame());
                CAPTURE(frame);
                CHECK(app.stats().latest().managedAllocatedBytes == 0);
            }
            CHECK(fixture.field_i64(probe, "LastAllocatedBytes") == 0);
            CHECK(fixture.field_i64(probe, "LastHeapBytes") > 0);
        }
        fixture.shutdown();
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("managed frame stats - without scripting every managed figure is 0") {
        log_capture logs = {};
        engine::engine_config config = headless_config();
        config.scripting.enabled = false;
        config.scripting.required = false;
        std::unique_ptr<engine::application> app = make_app(config);
        CONJURE_REQUIRE_OK(app->initialise());
        CHECK(app->context().scripting == nullptr);
        CONJURE_REQUIRE_OK(app->step_frame());
        const engine::frame_sample &sample = app->stats().latest();
        CHECK(sample.managedAllocatedBytes == 0);
        CHECK(sample.managedHeapBytes == 0);
        CHECK(sample.managedCollections == 0);
        CHECK(sample.managedFullCollections == 0);
        app->shutdown();
    }
}
