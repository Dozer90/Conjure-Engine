#include "conjure/testing/doctest.h"

#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/native_api_entries.h"
#include "conjure/scripting/native_api.h"

#include <array>
#include <cstdint>
#include <string_view>

// call_counters.h (docs/design/15_bindings.md 2.7, 3.5, B6): ids follow the .inl order, descriptors carry
// the generator's normalised signatures, the counting thunks increment and forward, wrapping is idempotent.

using namespace conjure;
using namespace conjure::bindings;

namespace {

    int32_t sLastEntity = 0;
    uint32_t sLastRgba = 0;
    float sLastSeconds = 0.0f;
    conjure_transform sLastTransform = {};

    int32_t fake_set_local(uint64_t entity, const conjure_transform *value) {
        sLastEntity = static_cast<int32_t>(entity);
        sLastTransform = *value;
        return 77;
    }

    int32_t fake_draw_line(const conjure_vec3 *a, const conjure_vec3 *b, uint32_t rgba, float seconds) {
        sLastRgba = rgba;
        sLastSeconds = seconds + a->x + b->y;
        return 1;
    }

    uint64_t fake_get_parent(uint64_t entity) {
        return entity + 1;
    }

    u32 sQueryEndCalls = 0;
    uint32_t sLastScopeId = 0;

    void fake_query_end(uint32_t scopeId) {
        ++sQueryEndCalls;
        sLastScopeId = scopeId;
    }
}

TEST_SUITE("bindings/call_counters") {

    TEST_CASE("call_counters - count matches the .inl line count and the generated constants") {
        CHECK(NATIVE_FUNCTION_COUNT == CONJURE_NATIVE_API_ENTRY_COUNT);
        CHECK(NATIVE_FUNCTION_COUNT == scripting::NATIVE_API_ENTRY_COUNT);
        CHECK(static_cast<u32>(native_function_id::log_write) == 0);
        CHECK(static_cast<u32>(native_function_id::transform_get_local) == CONJURE_NATIVE_API_CORE_ENTRY_COUNT);
        CHECK(static_cast<u32>(native_function_id::entity_get_tag_text) == NATIVE_FUNCTION_COUNT - 6);
        CHECK(static_cast<u32>(native_function_id::camera_world_to_screen) == NATIVE_FUNCTION_COUNT - 1);
        CHECK(native_entry_descriptors().size() == NATIVE_FUNCTION_COUNT);
        CHECK(BINDINGS_ENTRY_COUNT == 112);
    }

    TEST_CASE("call_counters - descriptor order is the .inl order and matches scripting's table") {
        const std::span<const native_entry_descriptor> ours = native_entry_descriptors();
        const std::span<const scripting::native_entry_descriptor> theirs = scripting::native_entry_descriptors();
        REQUIRE(ours.size() == theirs.size());
        for (usize index = 0; index < ours.size(); ++index) {
            CAPTURE(index);
            CHECK(static_cast<usize>(ours[index].id) == index);
            CHECK(ours[index].group == theirs[index].group);
            CHECK(ours[index].name == theirs[index].name);
            CHECK(ours[index].signature == theirs[index].signature);
            CHECK(ours[index].ownedByBindings == (index >= CONJURE_NATIVE_API_CORE_ENTRY_COUNT));
            CHECK(is_bindings_owned(ours[index].id) == ours[index].ownedByBindings);
        }
        CHECK(ours[0].group == "log");
        CHECK(ours[0].name == "write");
        CHECK(ours[0].signature == "void (uint8_t level, const char *utf8, int32_t length)");
        const native_entry_descriptor &setLocal = native_entry_descriptor_of(native_function_id::transform_set_local);
        CHECK(setLocal.signature == "int32_t (uint64_t entity, const conjure_transform *value)");
        CHECK(setLocal.ownedByBindings);
        CHECK(native_entry_descriptor_of(native_function_id::COUNT).name.empty());
        CHECK_FALSE(is_bindings_owned(native_function_id::COUNT));
    }

    TEST_CASE("call_counters - thunk increments and forwards the return value") {
        reset_call_counters();
        using thunk =
          counting_thunk<native_function_id::transform_set_local, int32_t(uint64_t, const conjure_transform *)>;
        thunk::sTarget = &fake_set_local;
        conjure_transform value = {};
        value.position.y = 3.0f;
        CHECK(thunk::invoke(42, &value) == 77);
        CHECK(sLastEntity == 42);
        CHECK(sLastTransform.position.y == 3.0f);
        CHECK(call_count(native_function_id::transform_set_local) == 1);
        CHECK(thunk::invoke(43, &value) == 77);
        CHECK(call_count(native_function_id::transform_set_local) == 2);
        CHECK(call_count(native_function_id::transform_get_local) == 0);
        thunk::sTarget = nullptr;
        reset_call_counters();
    }

    TEST_CASE("call_counters - thunk forwards every argument kind") {
        reset_call_counters();
        using draw = counting_thunk<native_function_id::diagnostics_draw_line, int32_t(
          const conjure_vec3 *,
          const conjure_vec3 *,
          uint32_t,
          float
        )>;
        draw::sTarget = &fake_draw_line;
        const conjure_vec3 a = {1.0f, 0.0f, 0.0f};
        const conjure_vec3 b = {0.0f, 2.0f, 0.0f};
        CHECK(draw::invoke(&a, &b, 0xAABBCCDDu, 0.5f) == 1);
        CHECK(sLastRgba == 0xAABBCCDDu);
        CHECK(sLastSeconds == 3.5f);
        CHECK(call_count(native_function_id::diagnostics_draw_line) == 1);
        draw::sTarget = nullptr;

        using parent = counting_thunk<native_function_id::entity_get_parent, uint64_t(uint64_t)>;
        parent::sTarget = &fake_get_parent;
        CHECK(parent::invoke(9) == 10);
        CHECK(call_count(native_function_id::entity_get_parent) == 1);
        parent::sTarget = nullptr;
        reset_call_counters();
    }

    TEST_CASE("call_counters - wrap_with_counters is idempotent and skips null slots") {
        reset_call_counters();
        conjure_native_api api = scripting::make_native_api(nullptr);
        api.transform_set_local = &fake_set_local;
        wrap_with_counters(api);
        CHECK(is_counting_installed(api, native_function_id::transform_set_local));
        CHECK_FALSE(is_counting_installed(api, native_function_id::transform_get_local));   // null stays null
        CHECK(api.transform_get_local == nullptr);
        const auto wrapped = api.transform_set_local;
        wrap_with_counters(api);
        CHECK(api.transform_set_local == wrapped);
        conjure_transform value = {};
        CHECK(api.transform_set_local(5, &value) == 77);
        CHECK(api.transform_set_local(6, &value) == 77);
        CHECK(call_count(native_function_id::transform_set_local) == 2);   // no double count after rewrap
        CHECK(sLastEntity == 6);
        reset_call_counters();
    }

    TEST_CASE("regression - wrapping a filled query_end twice keeps one hook and one count") {
        // The query_end slot carries the scope-end hook over its thunk. A second wrap used to wrap the hook
        // itself, chaining hook -> thunk -> hook until the stack overflowed on the first query_end call.
        reset_call_counters();
        sQueryEndCalls = 0;
        conjure_native_api api = scripting::make_native_api(nullptr);
        api.query_end = &fake_query_end;
        wrap_with_counters(api);
        CHECK(is_scope_hook_installed(api));
        const auto wrapped = api.query_end;
        wrap_with_counters(api);
        wrap_with_counters(api);
        CHECK(api.query_end == wrapped);
        CHECK(is_scope_hook_installed(api));
#if !CONJURE_DISTRIBUTION
        CHECK(is_counting_installed(api, native_function_id::query_end));
#endif
        api.query_end(7);
        CHECK(sQueryEndCalls == 1);
        CHECK(sLastScopeId == 7);
#if CONJURE_DISTRIBUTION
        CHECK(call_count(native_function_id::query_end) == 0);
#else
        CHECK(call_count(native_function_id::query_end) == 1);
#endif
        api.query_end(8);
        CHECK(sQueryEndCalls == 2);
        reset_call_counters();
    }

    TEST_CASE("call_counters - reset zeroes every counter") {
        detail::counter_storage()[static_cast<usize>(native_function_id::audio_play)] = 9;
        detail::counter_storage()[static_cast<usize>(native_function_id::log_write)] = 3;
        CHECK(call_count(native_function_id::audio_play) == 9);
        reset_call_counters();
        for (const u32 count : call_counters()) {
            CHECK(count == 0);
        }
        CHECK(call_count(native_function_id::audio_play) == 0);
        CHECK(call_count(native_function_id::COUNT) == 0);
    }

    TEST_CASE("call_counters - function_name matches the descriptor and find_function inverts it") {
        for (const native_entry_descriptor &descriptor : native_entry_descriptors()) {
            CAPTURE(descriptor.name);
            const std::string_view fullName = function_name(descriptor.id);
            CHECK(fullName.starts_with(descriptor.group));
            CHECK(fullName.ends_with(descriptor.name));
            CHECK(fullName.size() == descriptor.group.size() + 1 + descriptor.name.size());
            CHECK(fullName.size() < 64);                       // the name buffers are sized from the .inl
            CHECK(find_function(fullName) == descriptor.id);
        }
        CHECK(function_name(native_function_id::transform_set_local) == "transform_set_local");
        CHECK(function_name(native_function_id::COUNT).empty());
        CHECK(find_function("no_such_entry") == native_function_id::COUNT);
    }

    TEST_CASE("call_counters - the span is empty in Distribution and full otherwise") {
#if CONJURE_DISTRIBUTION
        CHECK(call_counters().empty());
#else
        CHECK(call_counters().size() == NATIVE_FUNCTION_COUNT);
#endif
    }
}
