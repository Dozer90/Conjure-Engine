#include "conjure/testing/doctest.h"

#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/scripting_error.h"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <type_traits>

using namespace conjure;
using namespace conjure::scripting;

TEST_SUITE("scripting/native_api") {

    TEST_CASE("conjure_native_api - struct is standard layout with the header before the entries") {
        CHECK(std::is_standard_layout_v<conjure_native_api>);
        CHECK(std::is_standard_layout_v<conjure_managed_api>);
        CHECK(std::is_standard_layout_v<conjure_frame_block>);
        CHECK(offsetof(conjure_native_api, size) == 0);
        CHECK(offsetof(conjure_native_api, version) == 4);
        CHECK(offsetof(conjure_native_api, layout_hash) == 8);
        CHECK(offsetof(conjure_native_api, frame) == 16);
        CHECK(offsetof(conjure_native_api, log_write) == 24);
        CHECK(sizeof(conjure_native_api) == 24 + NATIVE_API_ENTRY_COUNT * sizeof(void *));
        CHECK(sizeof(conjure_managed_api) == 16 + 12 * sizeof(void *));
    }

    TEST_CASE("native_entry_descriptors - entry count matches the table and the manifest") {
        const std::span<const native_entry_descriptor> entries = native_entry_descriptors();
        CHECK(entries.size() == NATIVE_API_ENTRY_COUNT);
        CHECK(entries.size() == (sizeof(conjure_native_api) - 24) / sizeof(void *));
        REQUIRE(entries.size() > 20);
        CHECK(entries[0].group == "log");
        CHECK(entries[0].name == "write");
        CHECK(entries[0].index == 0);
        CHECK(entries[0].signature == "void (uint8_t level, const char *utf8, int32_t length)");
        CHECK(entries[NATIVE_API_CORE_ENTRY_COUNT - 1].name == "end");
        CHECK(entries[NATIVE_API_CORE_ENTRY_COUNT].group == "transform");
        for (u32 index = 0; index < entries.size(); ++index) {
            CAPTURE(index);
            CHECK(entries[index].index == index);
        }

        const std::filesystem::path manifestPath = scripting_test::manifest_path();
        REQUIRE_FALSE(manifestPath.empty());
        core::result<core::json::document> manifest = core::json::document::parse_file(manifestPath);
        REQUIRE_MESSAGE(manifest.has_value(), testing::describe_error(manifest));
        const core::json::value root = manifest.value().root();
        CHECK(root["entryCount"].as_u64() == entries.size());
        CHECK(root["coreEntryCount"].as_u64() == NATIVE_API_CORE_ENTRY_COUNT);
        CHECK(root["version"].as_u64() == CONJURE_NATIVE_API_VERSION);
        const core::json::value manifestEntries = root["entries"];
        REQUIRE(manifestEntries.size() == entries.size());
        for (u32 index = 0; index < entries.size(); ++index) {
            CAPTURE(index);
            const core::json::value item = manifestEntries[index];
            CHECK(item["group"].as_string() == entries[index].group);
            CHECK(item["name"].as_string() == entries[index].name);
            CHECK(item["signature"].as_string() == entries[index].signature);
        }
    }

    TEST_CASE("native_entry_descriptors - the version 3 entries follow entity_get_tag_text") {
        // Entries are appended, never reordered: entity_get_tag_text (version 2) keeps its index after
        // modules_name, and entity_spawn and the camera group (version 3) follow it, camera_world_to_screen last.
        const std::span<const native_entry_descriptor> entries = native_entry_descriptors();
        REQUIRE(entries.size() == NATIVE_API_ENTRY_COUNT);
        const usize tagText = NATIVE_API_ENTRY_COUNT - 6;
        CHECK(tagText == 126);
        CHECK(entries[tagText].group == "entity");
        CHECK(entries[tagText].name == "get_tag_text");
        CHECK(entries[tagText].signature == "int32_t (uint64_t entity, char *utf8, int32_t capacity)");
        const usize getName = static_cast<usize>(find_native_entry("entity_get_name"));
        CHECK(entries[tagText].signature == entries[getName].signature);
        CHECK(offsetof(conjure_native_api, entity_get_tag_text) == 24 + tagText * sizeof(void *));
        CHECK(find_native_entry("modules_name") == static_cast<i32>(NATIVE_API_ENTRY_COUNT) - 7);
        CHECK(entries[tagText + 1].group == "entity");
        CHECK(entries[tagText + 1].name == "spawn");
        const native_entry_descriptor &last = entries[NATIVE_API_ENTRY_COUNT - 1];
        CHECK(last.group == "camera");
        CHECK(last.name == "world_to_screen");
        CHECK(
          offsetof(conjure_native_api, camera_world_to_screen) == 24 + (NATIVE_API_ENTRY_COUNT - 1) * sizeof(void *)
        );
    }

    TEST_CASE("make_native_api - all entries are null before fill and the header is set") {
        conjure_frame_block frame = {};
        const conjure_native_api api = make_native_api(&frame);
        CHECK(api.size == sizeof(conjure_native_api));
        CHECK(api.version == CONJURE_NATIVE_API_VERSION);
        CHECK(api.layout_hash == CONJURE_NATIVE_API_HASH);
        CHECK(api.frame == &frame);
        for (void *const slot : native_api_slots(api)) {
            CHECK(slot == nullptr);
        }
        CHECK(api.log_write == nullptr);
        CHECK(api.modules_name == nullptr);
    }

    TEST_CASE("compute_native_api_hash - the constant matches the generator output") {
        CHECK(compute_native_api_hash() == CONJURE_NATIVE_API_HASH);
        core::result<core::json::document> manifest = core::json::document::parse_file(scripting_test::manifest_path());
        REQUIRE_MESSAGE(manifest.has_value(), testing::describe_error(manifest));
        const std::string expected = std::format("0x{:016X}", static_cast<u64>(CONJURE_NATIVE_API_HASH));
        CHECK(manifest.value().root()["hash"].as_string() == expected);
    }

    TEST_CASE("validate_native_api - fails on missing core entries and header drift") {
        conjure_native_api api = make_native_api(nullptr);
        CONJURE_CHECK_ERROR(validate_native_api(api), scripting_error::NOT_INITIALISED);

        fill_core_native_entries(api);
        CONJURE_CHECK_OK(validate_native_api(api));

        SUBCASE("one core entry cleared") {
            api.component_get = nullptr;
            const core::result<void> validated = validate_native_api(api);
            CONJURE_CHECK_ERROR(validated, scripting_error::NOT_INITIALISED);
            CHECK(validated.get_error().message().find("component_get") != std::string::npos);
        }
        SUBCASE("bindings entries may stay null") {
            api.transform_get_local = nullptr;
            CONJURE_CHECK_OK(validate_native_api(api));
        }
        SUBCASE("size mismatch") {
            api.size = 4;
            CONJURE_CHECK_ERROR(validate_native_api(api), scripting_error::ABI_MISMATCH);
        }
        SUBCASE("version mismatch") {
            api.version = CONJURE_NATIVE_API_VERSION + 1;
            CONJURE_CHECK_ERROR(validate_native_api(api), scripting_error::ABI_MISMATCH);
        }
        SUBCASE("hash mismatch names both hashes") {
            api.layout_hash = 0x1234;
            const core::result<void> validated = validate_native_api(api);
            CONJURE_CHECK_ERROR(validated, scripting_error::ABI_MISMATCH);
            CHECK(validated.get_error().message().find("0x0000000000001234") != std::string::npos);
        }
    }

    TEST_CASE("validate_managed_api - checks header and every function") {
        conjure_managed_api api = {};
        CONJURE_CHECK_ERROR(validate_managed_api(api), scripting_error::ABI_MISMATCH);
        api.size = sizeof(conjure_managed_api);
        api.version = CONJURE_NATIVE_API_VERSION;
        api.layout_hash = CONJURE_NATIVE_API_HASH;
        CONJURE_CHECK_ERROR(validate_managed_api(api), scripting_error::ABI_MISMATCH);

        scripting_test::scripting_fixture fixture = {};
        CONJURE_CHECK_OK(validate_managed_api(fixture.runtime.host().managed_api()));
        conjure_managed_api partial = fixture.runtime.host().managed_api();
        partial.collect_garbage = nullptr;
        CONJURE_CHECK_ERROR(validate_managed_api(partial), scripting_error::ABI_MISMATCH);
        partial = fixture.runtime.host().managed_api();
        partial.layout_hash ^= 1;
        CONJURE_CHECK_ERROR(validate_managed_api(partial), scripting_error::ABI_MISMATCH);
    }

    TEST_CASE("find_native_entry - resolves names in both groups and rejects unknown ones") {
        CHECK(find_native_entry("log_write") == 0);
        CHECK(find_native_entry("query_end") == static_cast<i32>(NATIVE_API_CORE_ENTRY_COUNT) - 1);
        CHECK(find_native_entry("transform_get_local") == static_cast<i32>(NATIVE_API_CORE_ENTRY_COUNT));
        CHECK(find_native_entry("entity_get_tag_text") == static_cast<i32>(NATIVE_API_ENTRY_COUNT) - 6);
        CHECK(find_native_entry("camera_world_to_screen") == static_cast<i32>(NATIVE_API_ENTRY_COUNT) - 1);
        CHECK(find_native_entry("physics_raycast") > 0);
        CHECK(find_native_entry("no_such_entry") == -1);
        CHECK(find_native_entry("") == -1);
    }

    TEST_CASE("native_api_slots - the mutable view writes the entry it addresses") {
        conjure_native_api api = make_native_api(nullptr);
        fill_core_native_entries(api);
        const std::span<void *> slots = native_api_slots(api);
        REQUIRE(slots.size() == NATIVE_API_ENTRY_COUNT);
        void *expected = nullptr;
        std::memcpy(&expected, &api.query_end, sizeof(expected));
        CHECK(slots[static_cast<usize>(find_native_entry("query_end"))] == expected);
        slots[static_cast<usize>(find_native_entry("query_end"))] = nullptr;
        CHECK(api.query_end == nullptr);
    }

    TEST_CASE("script_field_kind - names round trip and unknown names parse to COUNT") {
        for (u8 index = 0; index < static_cast<u8>(script_field_kind::COUNT); ++index) {
            CAPTURE(index);
            const script_field_kind kind = static_cast<script_field_kind>(index);
            CHECK(parse_script_field_kind(to_string(kind)) == kind);
        }
        CHECK(to_string(script_field_kind::COUNT) == "COUNT");
        CHECK(parse_script_field_kind("nope") == script_field_kind::COUNT);
        CHECK(parse_script_field_kind("COUNT") == script_field_kind::COUNT);
    }

    TEST_CASE("script_callback - names round trip and bits are distinct") {
        u16 seen = 0;
        for (u8 index = 0; index < static_cast<u8>(script_callback::COUNT); ++index) {
            CAPTURE(index);
            const script_callback callback = static_cast<script_callback>(index);
            CHECK(parse_script_callback(to_string(callback)) == callback);
            CHECK((seen & callback_bit(callback)) == 0);
            seen = static_cast<u16>(seen | callback_bit(callback));
        }
        CHECK(parse_script_callback("UPDATE") == script_callback::UPDATE);
        CHECK(parse_script_callback("nope") == script_callback::COUNT);
        // RELOAD was appended: the older callbacks keep the bits ScriptCallback in C# mirrors.
        CHECK(to_string(script_callback::RELOAD) == "RELOAD");
        CHECK(static_cast<u8>(script_callback::TRIGGER_EXIT) == 9);
        CHECK(static_cast<u8>(script_callback::RELOAD) == 10);
        CHECK(to_string(script_phase::LATE_UPDATE) == "LATE_UPDATE");
        CHECK(to_string(script_phase::COUNT) == "COUNT");
    }

    TEST_CASE("conjure_create_request - the create flags are distinct single bits") {
        CHECK(CREATE_FLAG_ALREADY_STARTED == 1);
        CHECK(CREATE_FLAG_ALREADY_CREATED == 2);
        CHECK(CREATE_FLAG_INACTIVE == 4);
        CHECK((CREATE_FLAG_ALREADY_STARTED & CREATE_FLAG_ALREADY_CREATED) == 0);
        CHECK((CREATE_FLAG_INACTIVE & (CREATE_FLAG_ALREADY_STARTED | CREATE_FLAG_ALREADY_CREATED)) == 0);
    }

    TEST_CASE("regression - code sweep W3-CS-L1 the query and spawn flags are flag sets with pinned bits") {
        CHECK(static_cast<u32>(conjure_query_flags::NONE) == 0);
        CHECK(static_cast<u32>(conjure_query_flags::INCLUDE_TRIGGERS) == 1);
        CHECK(static_cast<u32>(conjure_query_flags::EXCLUDE_STATIC) == 2);
        const conjure_query_flags both = conjure_query_flags::INCLUDE_TRIGGERS | conjure_query_flags::EXCLUDE_STATIC;
        CHECK(static_cast<u32>(both) == 3);
        CHECK(has_flag(both, conjure_query_flags::EXCLUDE_STATIC));
        CHECK_FALSE(has_any_flag(both, ~both));
        CHECK(static_cast<u32>(conjure_spawn_flags::NONE) == 0);
        CHECK(static_cast<u32>(conjure_spawn_flags::SCENE_ENTITIES) == 1);
        CHECK((conjure_spawn_flags::SCENE_ENTITIES & conjure_spawn_flags::NONE) == conjure_spawn_flags::NONE);
    }

    TEST_CASE("conjure_frame_block - is 64-byte aligned with the documented offsets") {
        CHECK(alignof(conjure_frame_block) == 64);
        CHECK(sizeof(conjure_frame_block) == 512);
        CHECK(offsetof(conjure_frame_block, delta_time) == 16);
        CHECK(offsetof(conjure_frame_block, frame_index) == 32);
        CHECK(offsetof(conjure_frame_block, current_phase) == 44);
        CHECK(offsetof(conjure_frame_block, input) == 48);
        CHECK(sizeof(conjure_input_block) == 392);
        CHECK(offsetof(conjure_input_block, mouse_x) == 192);
        CHECK(offsetof(conjure_input_block, gamepads) == 228);
        CHECK(offsetof(conjure_frame_block, screen_width) == 440);
        CHECK(offsetof(conjure_frame_block, ui_scale) == 456);
        CHECK(offsetof(conjure_frame_block, window_state) == 460);
    }

    TEST_CASE("ABI records - sizes match 15_bindings.md 2.4") {
        CHECK(sizeof(conjure_uuid) == 16);
        CHECK(sizeof(conjure_ray_query) == 48);
        CHECK(sizeof(conjure_shape_query) == 88);
        CHECK(sizeof(conjure_raycast_hit) == 48);
        CHECK(sizeof(conjure_asset_info) == 16);
        CHECK(sizeof(conjure_module_record) == 20);
        CHECK(sizeof(conjure_frame_stats_block) == 88);
        CHECK(sizeof(conjure_spawn_component) == 16);
        CHECK(sizeof(conjure_spawn_desc) == 32);
        CHECK(sizeof(conjure_managed_memory) == 24);
        CHECK(sizeof(conjure_create_request) == 40);
        CHECK(sizeof(conjure_collision_record) == 48);
        CHECK(sizeof(conjure_chunk_view) == 80);
        CHECK(sizeof(conjure_query_scope) == 16);
    }
}
