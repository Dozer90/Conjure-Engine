#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "scripting_test_support.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

// The managed table of version 3 (wave 3 GAP-CSHARP 2.7, 2.6): validate_managed_api requires sample_memory and
// set_active, the null host implements both (the memory figures a test sets, the activity a slot has), and
// script_runtime::sample_managed_memory turns the cumulative counters into per-frame figures.

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    /// <summary>
    /// Creates 'count' instances of the null host directly, as create_instances from native would.
    /// </summary>
    template <usize Count>
    [[nodiscard]] std::array<i32, Count> create_slots(null_script_host &host, const std::array<u8, Count> &flags) {
        std::array<conjure_create_request, Count> requests = {};
        for (usize index = 0; index < Count; ++index) {
            requests[index].entity = 100 + index;
            requests[index].class_hash = 7;
            requests[index].flags = flags[index];
        }
        const i32 created = host.managed_api().create_instances(requests.data(), static_cast<i32>(Count));
        REQUIRE(created == static_cast<i32>(Count));
        std::array<i32, Count> slots = {};
        for (usize index = 0; index < Count; ++index) {
            slots[index] = requests[index].out_slot;
        }
        return slots;
    }
}

TEST_SUITE("scripting/managed_table") {

    TEST_CASE("managed table - validate_managed_api requires sample_memory and set_active") {
        scripting_fixture fixture = {};
        const conjure_managed_api &complete = fixture.runtime.host().managed_api();
        CONJURE_CHECK_OK(validate_managed_api(complete));
        CHECK(complete.sample_memory != nullptr);
        CHECK(complete.set_active != nullptr);
        CHECK(sizeof(conjure_managed_api) == 16 + 12 * sizeof(void *));

        SUBCASE("without sample_memory") {
            conjure_managed_api partial = complete;
            partial.sample_memory = nullptr;
            CONJURE_CHECK_ERROR(validate_managed_api(partial), scripting_error::ABI_MISMATCH);
        }
        SUBCASE("without set_active") {
            conjure_managed_api partial = complete;
            partial.set_active = nullptr;
            CONJURE_CHECK_ERROR(validate_managed_api(partial), scripting_error::ABI_MISMATCH);
        }
        SUBCASE("a version 2 table") {
            conjure_managed_api older = complete;
            older.version = 2;
            CONJURE_CHECK_ERROR(validate_managed_api(older), scripting_error::ABI_MISMATCH);
        }
        SUBCASE("a table of the version 2 size") {
            conjure_managed_api smaller = complete;
            smaller.size = static_cast<uint32_t>(16 + 10 * sizeof(void *));
            CONJURE_CHECK_ERROR(validate_managed_api(smaller), scripting_error::ABI_MISMATCH);
        }
    }

    TEST_CASE("managed table - the null host reports the memory figures it was given") {
        scripting_fixture fixture = {};
        null_script_host &host = fixture.null_host();
        const conjure_managed_api &api = host.managed_api();
        conjure_managed_memory out = {9, 9, 9, 9};
        REQUIRE(api.sample_memory(&out) == 0);
        CHECK(out.thread_allocated_bytes == 0);                 // nothing set yet
        CHECK(out.heap_bytes == 0);

        host.set_managed_memory(conjure_managed_memory{4096, 65536, 12, 2});
        REQUIRE(api.sample_memory(&out) == 0);
        CHECK(out.thread_allocated_bytes == 4096);
        CHECK(out.heap_bytes == 65536);
        CHECK(out.gen0_collections == 12);
        CHECK(out.gen2_collections == 2);
        CHECK(api.sample_memory(nullptr) == status_of(scripting_error::NOT_SUPPORTED));

        // A failing sample leaves 'out' as it was.
        host.set_sample_memory_result(status_of(scripting_error::NOT_INITIALISED));
        out = conjure_managed_memory{1, 2, 3, 4};
        CHECK(api.sample_memory(&out) == status_of(scripting_error::NOT_INITIALISED));
        CHECK(out.thread_allocated_bytes == 1);
        CHECK(out.gen2_collections == 4);
        CHECK(host.recorder().sample_memory_count() == 4);
        CHECK(host.recorder().count("sample_memory") == 0);

        // Without an active host the trampoline answers NOT_INITIALISED.
        host.shutdown();
        CHECK(api.sample_memory(&out) == status_of(scripting_error::NOT_INITIALISED));
        CHECK(api.set_active(nullptr, 0, 1) == status_of(scripting_error::NOT_INITIALISED));
    }

    TEST_CASE("managed table - the null host's set_active changes and records the activity of live slots") {
        scripting_fixture fixture = {};
        null_script_host &host = fixture.null_host();
        const conjure_managed_api &api = host.managed_api();
        const std::array<i32, 3> slots = create_slots<3>(host, {0, 0, CREATE_FLAG_INACTIVE});
        CHECK(host.is_slot_active(slots[0]));
        CHECK(host.is_slot_active(slots[1]));
        CHECK_FALSE(host.is_slot_active(slots[2]));             // created inactive
        std::vector<std::pair<i32, bool>> hooked = {};
        host.set_activity_hook([&hooked](i32 slot, bool active) {
            hooked.emplace_back(slot, active);
        });
        host.reset_recorder();

        // Deactivate two slots: one live and active, one already inactive (unchanged), plus a free slot.
        const std::array<i32, 3> request = {slots[0], slots[2], 999};
        CHECK(api.set_active(request.data(), 3, 0) == 1);
        CHECK_FALSE(host.is_slot_active(slots[0]));
        CHECK(host.is_slot_active(slots[1]));
        CHECK_FALSE(host.is_slot_live(999));
        REQUIRE(host.recorder().activities().size() == 3);
        for (usize index = 0; index < request.size(); ++index) {
            CAPTURE(index);
            const null_host_recorder::activity_record &record = host.recorder().activities()[index];
            CHECK(record.slot == request[index]);
            CHECK_FALSE(record.active);
            CHECK(record.changed == (index == 0));
        }
        CHECK(host.recorder().count("set_active") == 1);
        REQUIRE(hooked.size() == 1);
        CHECK(hooked[0] == std::pair<i32, bool>(slots[0], false));

        // Reactivate every slot: two change, the third was active all along.
        CHECK(api.set_active(slots.data(), 3, 1) == 2);
        CHECK(host.is_slot_active(slots[0]));
        CHECK(host.is_slot_active(slots[2]));
        CHECK(hooked.size() == 3);

        // Nothing to do: an empty or null list changes nothing and records only the call.
        host.reset_recorder();
        CHECK(api.set_active(nullptr, 2, 1) == 0);
        CHECK(api.set_active(slots.data(), 0, 0) == 0);
        CHECK(host.recorder().activities().empty());
        CHECK(host.recorder().count("set_active") == 2);
        CHECK(host.is_slot_active(slots[1]));
        host.set_activity_hook({});
    }

    TEST_CASE("managed table - a freed slot reads inactive and a reused one starts active") {
        scripting_fixture fixture = {};
        null_script_host &host = fixture.null_host();
        const conjure_managed_api &api = host.managed_api();
        const std::array<i32, 1> first = create_slots<1>(host, {CREATE_FLAG_INACTIVE});
        CHECK_FALSE(host.is_slot_active(first[0]));
        REQUIRE(api.destroy_instances(first.data(), 1, 1) == 1);
        CHECK_FALSE(host.is_slot_active(first[0]));             // not live
        CHECK(api.set_active(first.data(), 1, 1) == 0);         // a free slot is skipped
        const std::array<i32, 1> reused = create_slots<1>(host, {0});
        CHECK(reused[0] == first[0]);                           // LIFO free list
        CHECK(host.is_slot_active(reused[0]));
    }

    TEST_CASE("managed table - sample_managed_memory reports per-frame figures") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        null_script_host &host = fixture.null_host();

        SUBCASE("the first sample is the baseline, later ones are the growth since the previous") {
            host.set_managed_memory(conjure_managed_memory{1000, 8000, 10, 1});
            const managed_memory_delta first = fixture.runtime.sample_managed_memory();
            CHECK(first.allocatedBytes == 0);
            CHECK(first.heapBytes == 8000);
            CHECK(first.collections == 0);
            CHECK(first.fullCollections == 0);

            host.set_managed_memory(conjure_managed_memory{3048, 7000, 13, 2});
            const managed_memory_delta second = fixture.runtime.sample_managed_memory();
            CHECK(second.allocatedBytes == 2048);
            CHECK(second.heapBytes == 7000);                    // the heap may shrink: it is no delta
            CHECK(second.collections == 3);
            CHECK(second.fullCollections == 1);

            const managed_memory_delta idle = fixture.runtime.sample_managed_memory();
            CHECK(idle == managed_memory_delta{0, 7000, 0, 0});
        }
        SUBCASE("a counter going down is a restart: no growth that frame") {
            host.set_managed_memory(conjure_managed_memory{5000, 8000, 10, 1});
            static_cast<void>(fixture.runtime.sample_managed_memory());
            const std::array<conjure_managed_memory, 3> restarts = {
              conjure_managed_memory{4000, 6000, 10, 1},        // fewer bytes allocated
              conjure_managed_memory{5000, 6000, 9, 1},         // fewer gen 0 collections
              conjure_managed_memory{5000, 6000, 10, 0},        // fewer gen 2 collections
            };
            for (usize index = 0; index < restarts.size(); ++index) {
                CAPTURE(index);
                host.set_managed_memory(conjure_managed_memory{5000, 8000, 10, 1});
                static_cast<void>(fixture.runtime.sample_managed_memory());
                host.set_managed_memory(restarts[index]);
                const managed_memory_delta restarted = fixture.runtime.sample_managed_memory();
                CHECK(restarted == managed_memory_delta{0, 6000, 0, 0});
            }
        }
        SUBCASE("a failed sample reports nothing and the next one starts over") {
            host.set_managed_memory(conjure_managed_memory{1000, 8000, 10, 1});
            static_cast<void>(fixture.runtime.sample_managed_memory());
            host.set_sample_memory_result(status_of(scripting_error::NOT_SUPPORTED));
            host.set_managed_memory(conjure_managed_memory{2000, 8100, 11, 1});
            CHECK(fixture.runtime.sample_managed_memory() == managed_memory_delta{});
            host.set_sample_memory_result(0);
            CHECK(fixture.runtime.sample_managed_memory() == managed_memory_delta{0, 8100, 0, 0});
        }
        SUBCASE("without a ready runtime every figure is 0, and the baseline is dropped") {
            host.set_managed_memory(conjure_managed_memory{1000, 8000, 10, 1});
            static_cast<void>(fixture.runtime.sample_managed_memory());
            fixture.runtime.shutdown();
            const u64 samplesBefore = host.recorder().sample_memory_count();
            CHECK(fixture.runtime.sample_managed_memory() == managed_memory_delta{});
            CHECK(host.recorder().sample_memory_count() == samplesBefore);  // the host is not asked
        }
    }
}
