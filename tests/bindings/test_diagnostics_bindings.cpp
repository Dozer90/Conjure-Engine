#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/core/platform_detect.h"
#include "conjure/engine/diagnostics/check_registry.h"
#include "conjure/engine/frame_stats.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/debug_draw.h"
#endif

#include <array>
#include <string>
#include <vector>

// diagnostics_* (docs/design/15_bindings.md 10, B14): checks land in the registry with the frame index, the
// last-error protocol, debug drawing with and without a drawer, the frame stats block and the counters.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

TEST_SUITE("bindings/diagnostics_bindings") {

    TEST_CASE("diagnostics - check reports into the registry with the frame") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        fixture.time.frameIndex = 42;
        const utf8_argument name = utf8("gravity.applied");
        const utf8_argument detail = utf8("fell 4.9 m");
        CHECK(api.diagnostics_check(name.data, name.length, 1, detail.data, detail.length) == 0);
        CHECK(api.diagnostics_check(name.data, name.length, 0, nullptr, 0) == 0);
        const std::vector<engine::diagnostics::check_record> records = fixture.checks.records();
        REQUIRE(records.size() == 2);
        CHECK(records[0].name == "gravity.applied");
        CHECK(records[0].passed);
        CHECK(records[0].detail == "fell 4.9 m");
        CHECK(records[0].frame == 42);
        CHECK_FALSE(records[1].passed);
        CHECK(records[1].detail.empty());
        CHECK_FALSE(fixture.checks.all_passed());
        CHECK(api.diagnostics_check(nullptr, 0, 1, nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.diagnostics_check(nullptr, 4, 1, nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.diagnostics_check(name.data, name.length, 1, nullptr, 2) == to_status(bindings_error::INVALID_ARGUMENT)
        );
        fixture.context.checks = nullptr;
        fixture.fill();
        CHECK(api.diagnostics_check(name.data, name.length, 1, nullptr, 0) == to_status(bindings_error::NOT_SUPPORTED));
    }

    TEST_CASE("diagnostics - last_error reports the code and the message") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        conjure_transform local = {};
        REQUIRE(api.transform_get_local(999, &local) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        int32_t status = 0;
        std::array<char, 64> buffer = {};
        const int32_t needed = api.diagnostics_last_error(&status, buffer.data(), 64);
        CHECK(status == to_status(bindings_error::ENTITY_NOT_ALIVE));
        REQUIRE(needed > 0);
        CHECK(std::string_view(buffer.data(), static_cast<usize>(needed)) == "transform_get_local");
        // Reading clears nothing and tolerates a missing status pointer and a short buffer.
        CHECK(api.diagnostics_last_error(nullptr, buffer.data(), 4) == needed);
        CHECK(api.diagnostics_last_error(&status, nullptr, 0) == needed);
        CHECK(status == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(api.diagnostics_last_error(&status, buffer.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
        clear_last_error();
        CHECK(api.diagnostics_last_error(&status, buffer.data(), 64) == 0);
        CHECK(status == 0);
    }

    TEST_CASE("diagnostics - drawing without a drawer is a silent success") {
        log_capture logs = {};
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const conjure_vec3 a = {0.0f, 0.0f, 0.0f};
        const conjure_vec3 b = {1.0f, 0.0f, 0.0f};
        const conjure_quat_xyzw identity = {0.0f, 0.0f, 0.0f, 1.0f};
        const utf8_argument text = utf8("hello");
        CHECK(api.diagnostics_draw_line(&a, &b, 0xFF0000FFu, 0.0f) == 0);
        CHECK(api.diagnostics_draw_box(&a, &b, &identity, 0xFF0000FFu, 1.0f) == 0);
        CHECK(api.diagnostics_draw_box(&a, &b, nullptr, 0xFF0000FFu, 1.0f) == 0);
        CHECK(api.diagnostics_draw_sphere(&a, 0.5f, 0xFF0000FFu, 0.0f) == 0);
        CHECK(api.diagnostics_draw_text(&a, text.data, text.length, 0xFFFFFFFFu, 0.0f) == 0);
        CHECK(api.diagnostics_draw_line(nullptr, &b, 0, 0.0f) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.diagnostics_draw_line(&a, &b, 0, -1.0f) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.diagnostics_draw_box(&a, nullptr, &identity, 0, 0.0f) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.diagnostics_draw_sphere(nullptr, 0.5f, 0, 0.0f) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.diagnostics_draw_sphere(&a, -0.5f, 0, 0.0f) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.diagnostics_draw_text(nullptr, text.data, text.length, 0, 0.0f)
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(api.diagnostics_draw_text(&a, nullptr, 3, 0, 0.0f) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(call_count(native_function_id::diagnostics_draw_line) == 3);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    TEST_CASE("diagnostics - drawing with a drawer appends the expected vertex counts") {
        bare_fixture fixture = {};
        renderer::debug_draw drawer = {};
        fixture.context.debugDraw = &drawer;
        fixture.fill();
        conjure_native_api &api = fixture.api();
        const conjure_vec3 a = {0.0f, 0.0f, 0.0f};
        const conjure_vec3 b = {1.0f, 0.0f, 0.0f};
        const conjure_vec3 half = {0.5f, 0.5f, 0.5f};
        const conjure_quat_xyzw identity = {0.0f, 0.0f, 0.0f, 1.0f};
        CHECK(api.diagnostics_draw_line(&a, &b, 0xFF0000FFu, 0.0f) == 0);
        renderer::debug_draw_buffer frame = {};
        drawer.append_to(frame);
        CHECK(frame.depth_tested().size() == 2);
        CHECK(frame.depth_tested()[1].position == math::vec3(1.0f, 0.0f, 0.0f));
        CHECK(frame.depth_tested()[0].colour == renderer::pack_debug_colour(math::vec4(1.0f, 0.0f, 0.0f, 1.0f)));

        frame.clear();
        CHECK(api.diagnostics_draw_box(&a, &half, &identity, 0x00FF00FFu, 0.0f) == 0);
        drawer.append_to(frame);
        CHECK(frame.depth_tested().size() == 24);

        frame.clear();
        CHECK(api.diagnostics_draw_sphere(&a, 1.0f, 0x0000FFFFu, 2.0f) == 0);     // timed: alive for 2 s
        drawer.append_to(frame);
        CHECK(frame.depth_tested().size() == 144);                               // three circles of 24 lines
        CHECK(drawer.timed_entry_count() == 1);
        drawer.tick(3.0f);
        CHECK(drawer.timed_entry_count() == 0);
        fixture.context.debugDraw = nullptr;
        fixture.fill();
    }
#endif

    TEST_CASE("diagnostics - frame_stats block matches the latest sample") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        engine::frame_sample sample = {};
        sample.frameMilliseconds = 16.5f;
        sample.phaseMilliseconds[0] = 1.0f;
        sample.phaseMilliseconds[9] = 9.0f;
        sample.submitMilliseconds = 2.5f;
        sample.fixedSteps = 2;
        sample.jobsExecuted = 77;
        sample.frameMemoryHighWater = 4096;
        fixture.stats.commit(sample);
        conjure_frame_stats_block block = {};
        CHECK(api.diagnostics_frame_stats(&block) == 0);
        CHECK(block.frame_ms == 16.5f);
        CHECK(block.phase_ms[0] == 1.0f);
        CHECK(block.phase_ms[9] == 9.0f);
        CHECK(block.submit_ms == 2.5f);
        CHECK(block.fixed_steps == 2);
        CHECK(block.jobs_executed == 77);
        CHECK(block.frame_memory_high_water == 4096);
        CHECK(api.diagnostics_frame_stats(nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        fixture.context.stats = nullptr;
        fixture.fill();
        CHECK(api.diagnostics_frame_stats(&block) == to_status(bindings_error::NOT_SUPPORTED));
    }

    TEST_CASE("diagnostics - call_count matches the native span") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        reset_call_counters();
        conjure_vec3 gravity = {};
        static_cast<void>(api.physics_get_gravity(&gravity));
        static_cast<void>(api.physics_get_gravity(&gravity));
        const int32_t index = static_cast<int32_t>(native_function_id::physics_get_gravity);
#if CONJURE_DISTRIBUTION
        CHECK(api.diagnostics_call_count(index) == to_status(bindings_error::NOT_SUPPORTED));
#else
        CHECK(api.diagnostics_call_count(index) == 2);
        CHECK(static_cast<u32>(api.diagnostics_call_count(index)) == call_counters()[static_cast<usize>(index)]);
        // The thunk counts a call before it runs: this third diagnostics_call_count call sees itself.
        CHECK(api.diagnostics_call_count(static_cast<int32_t>(native_function_id::diagnostics_call_count)) == 3);
#endif
        CHECK(api.diagnostics_call_count(-1) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.diagnostics_call_count(static_cast<int32_t>(NATIVE_FUNCTION_COUNT))
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
    }
}
