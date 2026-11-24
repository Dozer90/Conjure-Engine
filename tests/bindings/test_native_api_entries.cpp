#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/native_api_entries.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/scripting/native_api.h"

#include <array>
#include <cstdint>
#include <format>
#include <set>
#include <string>
#include <string_view>

// native_api_entries.h (docs/design/15_bindings.md 3.1, B2, B3, 3.6): the table is complete after every
// fill, stubs answer NOT_SUPPORTED naming the entry, the eleven groups are exactly the documented ones, core
// slots are untouched, foreign slots are overridden or kept, the committed manifest matches the table, and the
// version 3 table holds the wave 3 entries (GAP-CSHARP) in their place with their signatures.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    int32_t foreign_set_gravity(const conjure_vec3 *) {
        return 1234;
    }

    int32_t foreign_check(const char *, int32_t, int32_t, const char *, int32_t) {
        return 4321;
    }

    void fake_query_end(uint32_t) {}

    [[nodiscard]] std::set<std::string_view> owned_groups() {
        std::set<std::string_view> groups = {};
        for (const native_entry_descriptor &descriptor : native_entry_descriptors()) {
            if (descriptor.ownedByBindings) {
                groups.insert(descriptor.group);
            }
        }
        return groups;
    }
}

TEST_SUITE("bindings/native_api_entries") {

    TEST_CASE("test_native_table_complete - a null context fills every owned slot with a stub") {
        log_capture logs = {};
        conjure_native_api api = scripting::make_native_api(nullptr);
        const binding_context empty = {};
        CONJURE_REQUIRE_OK(fill_native_table(api, empty));
        const std::span<void *const> slots = scripting::native_api_slots(api);
        for (u32 index = 0; index < NATIVE_FUNCTION_COUNT; ++index) {
            const native_function_id id = static_cast<native_function_id>(index);
            CAPTURE(function_name(id));
            if (!is_bindings_owned(id)) {
                CHECK(slots[index] == nullptr);           // core slots are scripting's
                continue;
            }
            CHECK(slots[index] != nullptr);
            if (native_entry_descriptor_of(id).group == "diagnostics") {
                // The diagnostics group needs no service: its entries are real in every context.
                CHECK(is_implementation_installed(api, id));
                continue;
            }
            CHECK(is_stub_installed(api, id));
            CHECK_FALSE(is_implementation_installed(api, id));
        }

        // Every stub returns -NOT_SUPPORTED and names its entry; uint64_t stubs return 0.
        clear_last_error();
        conjure_transform transform = {};
        CHECK(api.transform_get_local(1, &transform) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(last_error_message() == "transform_get_local: subsystem unavailable");
        CHECK(api.entity_get_parent(1) == 0);
        CHECK(last_error_message() == "entity_get_parent: subsystem unavailable");
        CHECK(api.scene_loaded_count() == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.modules_count() == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.asset_type_id("x", 1) == 0);
        CHECK(last_error_code() == bindings_error::NOT_SUPPORTED);
        conjure_frame_stats_block block = {};
        CHECK(api.diagnostics_frame_stats(&block) == to_status(bindings_error::NOT_SUPPORTED));   // real, no stats
        CHECK(active_context().world == nullptr);
        clear_active_context();
    }

    TEST_CASE("test_native_table_complete - a full headless context installs no stub for a compiled-in group") {
        bare_fixture_options options = {};
        options.physics = true;
        options.audio = true;
        bare_fixture fixture(options);
        const conjure_native_api &api = fixture.api();
        for (const native_entry_descriptor &descriptor : native_entry_descriptors()) {
            CAPTURE(function_name(descriptor.id));
            if (!descriptor.ownedByBindings) {
                continue;
            }
            if (!group_available(descriptor.group, fixture.context)) {
                // Compiled out, or a service the bare fixture does not create (the module registry).
                CHECK(is_stub_installed(api, descriptor.id));
                continue;
            }
            CHECK(is_implementation_installed(api, descriptor.id));
            CHECK_FALSE(is_stub_installed(api, descriptor.id));
            CHECK(is_counting_installed(api, descriptor.id));
        }
        CHECK(active_context().world == &fixture.world);
        CHECK(active_context().scenes == &fixture.scenes);
    }

    TEST_CASE("native_api_entries - a partial context stubs only the groups whose services are missing") {
        bare_fixture_options options = {};
        options.window = false;
        options.physics = false;
        options.audio = false;
        bare_fixture fixture(options);
        const conjure_native_api &api = fixture.api();
        CHECK(is_implementation_installed(api, native_function_id::transform_get_local));
        CHECK(is_implementation_installed(api, native_function_id::asset_find));
        CHECK(is_implementation_installed(api, native_function_id::diagnostics_check));
        CHECK(is_stub_installed(api, native_function_id::input_clipboard_get));
        CHECK(is_stub_installed(api, native_function_id::modules_count));
        CHECK(is_stub_installed(api, native_function_id::physics_raycast));
        CHECK(is_stub_installed(api, native_function_id::audio_play));
        CHECK(api.audio_play(1) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(call_count(native_function_id::audio_play) == 1);       // the stub is counted too
        CHECK(group_available("transform", fixture.context));
        CHECK_FALSE(group_available("physics", fixture.context));
        CHECK_FALSE(group_available("nonsense", fixture.context));
        CHECK(group_available("diagnostics", binding_context{}));
        CHECK(group_compiled("asset"));
        CHECK_FALSE(group_compiled("log"));
    }

    TEST_CASE("native_api_entries - owned groups are exactly the documented eleven") {
        const std::set<std::string_view> groups = owned_groups();
        const std::set<std::string_view> expected = {
          "transform", "entity", "scene", "prefab", "physics", "audio", "input", "asset", "diagnostics", "modules",
          "camera"
        };
        CHECK(groups == expected);
        CHECK(groups.size() == BINDINGS_GROUP_COUNT);
        for (const std::string_view group : expected) {
            CAPTURE(group);
            if (group == "physics") {
                CHECK(group_compiled(group) == (CONJURE_BINDINGS_GROUP_COMPILED(physics) == 1));
            } else if (group == "audio") {
                CHECK(group_compiled(group) == (CONJURE_BINDINGS_GROUP_COMPILED(audio) == 1));
            } else if (group == "camera") {
                CHECK(group_compiled(group) == (CONJURE_BINDINGS_GROUP_COMPILED(camera) == 1));
            } else {
                CHECK(group_compiled(group));
            }
        }
        CHECK(CONJURE_BINDINGS_OWNS_GROUP(transform) == 1);
        CHECK(CONJURE_BINDINGS_OWNS_GROUP(entity) == 1);
        CHECK(CONJURE_BINDINGS_OWNS_GROUP(modules) == 1);
        CHECK(CONJURE_BINDINGS_OWNS_GROUP(camera) == 1);
        CHECK(CONJURE_BINDINGS_OWNS_GROUP(log) == 0);
        CHECK(CONJURE_BINDINGS_OWNS_GROUP(query) == 0);
        // The five core entity entries stay scripting's.
        CHECK_FALSE(is_bindings_owned(native_function_id::entity_create));
        CHECK_FALSE(is_bindings_owned(native_function_id::entity_find_by_name));
        CHECK(is_bindings_owned(native_function_id::entity_set_name));
        CHECK(is_bindings_owned(native_function_id::entity_create_many));
    }

    TEST_CASE("native_api_entries - core slots are untouched by fill") {
        conjure_native_api api = scripting::make_native_api(nullptr);
        api.entity_create = [](const char *, int32_t) -> uint64_t {
            return 99;
        };
        api.log_write = [](uint8_t, const char *, int32_t) {};
        const auto createBefore = api.entity_create;
        const auto logBefore = api.log_write;
        CONJURE_REQUIRE_OK(fill_native_table(api, binding_context{}));
        CHECK(api.entity_create == createBefore);
        CHECK(api.log_write == logBefore);
        CHECK(api.query_begin == nullptr);
        CHECK(api.component_has == nullptr);
        CHECK(api.entity_create("a", 1) == 99);
        clear_active_context();
    }

    TEST_CASE("test_generated_manifest_matches - the committed manifest equals the native table") {
        const core::result<core::json::document> parsed = core::json::document::parse_file(manifest_path());
        CONJURE_REQUIRE_OK(parsed);
        const core::json::value root = parsed.value().root();
        REQUIRE(root.is_object());
        CHECK(root["entryCount"].as_u64() == NATIVE_FUNCTION_COUNT);
        CHECK(root["coreEntryCount"].as_u64() == CONJURE_NATIVE_API_CORE_ENTRY_COUNT);
        CHECK(root["version"].as_u64() == CONJURE_NATIVE_API_VERSION);
        const std::string hash = std::string(root["hash"].as_string());
        CHECK(hash == std::format("0x{:016X}", static_cast<u64>(CONJURE_NATIVE_API_HASH)));
        CHECK(scripting::compute_native_api_hash() == CONJURE_NATIVE_API_HASH);

        const core::json::value entries = root["entries"];
        REQUIRE(entries.is_array());
        REQUIRE(entries.size() == NATIVE_FUNCTION_COUNT);
        const std::span<const native_entry_descriptor> descriptors = native_entry_descriptors();
        for (usize index = 0; index < descriptors.size(); ++index) {
            CAPTURE(index);
            const core::json::value entry = entries[index];
            CHECK(entry["index"].as_u64() == index);
            CHECK(entry["group"].as_string() == descriptors[index].group);
            CHECK(entry["name"].as_string() == descriptors[index].name);
            CHECK(entry["signature"].as_string() == descriptors[index].signature);
            const std::string returns = std::string(entry["returns"].as_string());
            CHECK(descriptors[index].signature.starts_with(returns + " ("));
        }
    }

    TEST_CASE("native_api_entries - an override of a filled owned slot warns and installs ours") {
        log_capture logs = {};
        conjure_native_api api = scripting::make_native_api(nullptr);
        api.diagnostics_check = &foreign_check;     // a plugin filled a compiled-in slot by name (R36)
        CONJURE_REQUIRE_OK(fill_native_table(api, binding_context{}));
        CHECK(api.diagnostics_check != &foreign_check);
        CHECK(is_implementation_installed(api, native_function_id::diagnostics_check));
        CHECK(logs.contains(core::log_level::WARN, "diagnostics_check"));
        CHECK(logs.count(core::log_level::WARN) == 1);

        // Our own pointers are never reported as overrides: a second fill is quiet.
        logs.clear();
        CONJURE_REQUIRE_OK(fill_native_table(api, binding_context{}));
        wrap_with_counters(api);
        CONJURE_REQUIRE_OK(fill_native_table(api, binding_context{}));
        CHECK(logs.count(core::log_level::WARN) == 0);
        clear_active_context();
    }

    TEST_CASE("native_api_entries - a plugin fill is kept for a compiled-out group") {
        conjure_native_api api = scripting::make_native_api(nullptr);
        api.physics_set_gravity = &foreign_set_gravity;
        CONJURE_REQUIRE_OK(fill_native_table(api, binding_context{}));
        if (group_compiled("physics")) {
            // Compiled in: the bindings own the slot and replace the plugin's pointer (B3).
            CHECK(api.physics_set_gravity != &foreign_set_gravity);
            CHECK(is_stub_installed(api, native_function_id::physics_set_gravity));
        } else {
            CHECK(api.physics_set_gravity == &foreign_set_gravity);
            const conjure_vec3 gravity = {};
            CHECK(api.physics_set_gravity(&gravity) == 1234);
        }
        clear_active_context();
    }

    TEST_CASE("native_api_entries - fail_entry logs once per entry per frame") {
        log_capture logs = {};
        bindings::detail::clear_frame_failure_log();
        CHECK_FALSE(bindings::detail::failure_logged_this_frame(native_function_id::transform_get_local));
        CHECK(
          bindings::detail::fail_entry(native_function_id::transform_get_local, bindings_error::ENTITY_NOT_ALIVE) == -2
        );
        CHECK(
          bindings::detail::fail_entry(native_function_id::transform_get_local, bindings_error::ENTITY_NOT_ALIVE) == -2
        );
        CHECK(bindings::detail::failure_logged_this_frame(native_function_id::transform_get_local));
        CHECK(last_error_message() == "transform_get_local");
#if !CONJURE_DISTRIBUTION
        CHECK(logs.count(core::log_level::DEBUG) == 1);
#endif
        bindings::detail::clear_frame_failure_log();
        CHECK_FALSE(bindings::detail::failure_logged_this_frame(native_function_id::transform_get_local));
        CHECK_FALSE(bindings::detail::failure_logged_this_frame(native_function_id::COUNT));
        clear_last_error();
    }

    TEST_CASE("native_api_entries - version 3 holds 132 entries and appends the wave 3 entries last") {
        CHECK(CONJURE_NATIVE_API_VERSION == 3);
        CHECK(NATIVE_FUNCTION_COUNT == 132);
        CHECK(CONJURE_NATIVE_API_CORE_ENTRY_COUNT == 20);
        CHECK(scripting::compute_native_api_hash() == CONJURE_NATIVE_API_HASH);

        // Appended after entity_get_tag_text, in this order; the camera group is a bindings group of its own.
        constexpr std::array<std::string_view, 6> TAIL = {
          "entity_get_tag_text", "entity_spawn", "camera_main", "camera_screen_ray", "camera_screen_to_world",
          "camera_world_to_screen"
        };
        for (usize offset = 0; offset < TAIL.size(); ++offset) {
            CAPTURE(offset);
            const u32 index = NATIVE_FUNCTION_COUNT - static_cast<u32>(TAIL.size()) + static_cast<u32>(offset);
            const native_function_id id = static_cast<native_function_id>(index);
            CHECK(function_name(id) == TAIL[offset]);
            CHECK(is_bindings_owned(id));
            CHECK(find_function(TAIL[offset]) == id);
        }
        CHECK(native_entry_descriptor_of(native_function_id::entity_spawn).group == "entity");
        CHECK(native_entry_descriptor_of(native_function_id::camera_main).group == "camera");

        // The signatures the generator normalised: the three entries changed in place and the five new ones.
        CHECK(
          native_entry_descriptor_of(native_function_id::query_begin).signature
          == "int32_t (const uint32_t *ids, int32_t count, const uint32_t *withoutIds, int32_t withoutCount, "
             "conjure_query_scope *out)"
        );
        CHECK(
          native_entry_descriptor_of(native_function_id::physics_overlap_sphere).signature
          == "int32_t (const conjure_vec3 *center, float radius, uint32_t layerMask, uint32_t flags, "
             "uint64_t ignoreEntity, uint64_t *out, int32_t capacity)"
        );
        CHECK(
          native_entry_descriptor_of(native_function_id::physics_overlap_box).signature
          == "int32_t (const conjure_vec3 *center, const conjure_vec3 *halfExtents, const conjure_quat_xyzw "
             "*rotation, uint32_t layerMask, uint32_t flags, uint64_t ignoreEntity, uint64_t *out, int32_t capacity)"
        );
        CHECK(
          native_entry_descriptor_of(native_function_id::entity_spawn).signature
          == "int32_t (const conjure_spawn_desc *desc, uint64_t *outEntities)"
        );
        CHECK(native_entry_descriptor_of(native_function_id::camera_main).signature == "uint64_t (void)");
        CHECK(
          native_entry_descriptor_of(native_function_id::camera_screen_ray).signature
          == "int32_t (uint64_t camera, const conjure_vec2 *screenPixels, conjure_vec3 *outOrigin, "
             "conjure_vec3 *outDirection)"
        );
        CHECK(
          native_entry_descriptor_of(native_function_id::camera_screen_to_world).signature
          == "int32_t (uint64_t camera, const conjure_vec3 *screenPixelsAndDepth, conjure_vec3 *outWorld)"
        );
        CHECK(
          native_entry_descriptor_of(native_function_id::camera_world_to_screen).signature
          == "int32_t (uint64_t camera, const conjure_vec3 *world, conjure_vec3 *outScreenPixelsAndDepth)"
        );
    }

    TEST_CASE("native_api_entries - the camera group needs the world and the scenes and follows the renderer") {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        CHECK(CONJURE_BINDINGS_GROUP_COMPILED(camera) == 1);
#else
        CHECK(CONJURE_BINDINGS_GROUP_COMPILED(camera) == 0);
#endif
        CHECK(group_compiled("camera") == (CONJURE_BINDINGS_GROUP_COMPILED(camera) == 1));
        CHECK_FALSE(group_available("camera", binding_context{}));
        bare_fixture fixture = {};
        CHECK(group_available("camera", fixture.context) == group_compiled("camera"));
        binding_context noScenes = fixture.context;
        noScenes.scenes = nullptr;
        CHECK_FALSE(group_available("camera", noScenes));
        binding_context noWorld = fixture.context;
        noWorld.world = nullptr;
        CHECK_FALSE(group_available("camera", noWorld));
    }

    TEST_CASE("native_api_entries - the entries this build has not implemented answer NOT_SUPPORTED by name") {
        bare_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        clear_last_error();

        // entity_spawn: installed as the entity group's implementation, which refuses every request.
        CHECK(is_implementation_installed(api, native_function_id::entity_spawn));
        conjure_spawn_desc desc = {};
        desc.count = 1;
        std::array<uint64_t, 1> spawned = {};
        CHECK(api.entity_spawn(&desc, spawned.data()) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(last_error_code() == bindings_error::NOT_SUPPORTED);
        CHECK(last_error_message() == "entity_spawn: not implemented in this build");
        CHECK(spawned[0] == 0);

        // camera_*: the implementation with the renderer, the stubs without it; NOT_SUPPORTED either way.
        CHECK(is_implementation_installed(api, native_function_id::camera_main) == group_compiled("camera"));
        clear_last_error();
        CHECK(api.camera_main() == 0);
        CHECK(last_error_code() == bindings_error::NOT_SUPPORTED);
        const conjure_vec2 pixels = {1.0f, 2.0f};
        const conjure_vec3 point = {1.0f, 2.0f, 3.0f};
        conjure_vec3 origin = {};
        conjure_vec3 direction = {};
        CHECK(api.camera_screen_ray(1, &pixels, &origin, &direction) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.camera_screen_to_world(1, &point, &origin) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.camera_world_to_screen(1, &point, &origin) == to_status(bindings_error::NOT_SUPPORTED));
        if (group_compiled("camera")) {
            CHECK(last_error_message() == "camera_world_to_screen: not implemented in this build");
        } else {
            CHECK(last_error_message() == "camera_world_to_screen: subsystem unavailable");
        }
        clear_last_error();
    }

    TEST_CASE("native_api_entries - is_scope_hook_installed follows wrap_with_counters and a core refill") {
        conjure_native_api api = scripting::make_native_api(nullptr);
        CHECK_FALSE(is_scope_hook_installed(api));
        wrap_with_counters(api);
        CHECK_FALSE(is_scope_hook_installed(api));            // a null query_end gets no hook
        CHECK(api.query_end == nullptr);
        api.query_end = &fake_query_end;
        wrap_with_counters(api);
        CHECK(is_scope_hook_installed(api));
        // script_runtime::initialise writes the raw core entry back over the slot: the hook is gone until
        // the next wrap (the subsystem's begin_frame does that).
        api.query_end = &fake_query_end;
        CHECK_FALSE(is_scope_hook_installed(api));
        wrap_with_counters(api);
        CHECK(is_scope_hook_installed(api));
        reset_call_counters();
    }
}
