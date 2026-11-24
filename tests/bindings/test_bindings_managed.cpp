#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "bindings_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/audio/clip.h"
#include "conjure/audio/clip_asset.h"
#include "conjure/audio/components.h"
#include "conjure/bindings/bindings_subsystem.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/engine/application.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"
#include "engine/subsystems/audio_components.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
#endif
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/components.h"
    #include "engine/subsystems/renderer_components.h"
#endif

#include <charconv>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// The C# facades (docs/design/15_bindings.md 2.5, 2.6, 10) driven through Conjure.TestScripts over the real CLR
// inside a headless application: every probe records what it saw in public fields the test reads back. The
// cases are skipped without the .NET runtime, which CI treats as a failure where the SDK is installed.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr std::string_view FACADE_PROBE = "Conjure.TestScripts.BindingFacadeProbe";
    constexpr std::string_view EXCEPTION_PROBE = "Conjure.TestScripts.BindingExceptionProbe";
    constexpr std::string_view PREDICATE_PROBE = "Conjure.TestScripts.BindingPredicateProbe";
    constexpr std::string_view ASSET_PROBE = "Conjure.TestScripts.BindingAssetRefProbe";
    constexpr std::string_view MIRROR_PROBE = "Conjure.TestScripts.BindingMirrorProbe";
    constexpr std::string_view BIND_PROBE = "Conjure.TestScripts.BindingAssetBindProbe";
    constexpr std::string_view MODULES_PROBE = "Conjure.TestScripts.BindingModulesProbe";
    constexpr std::string_view INPUT_PROBE = "Conjure.TestScripts.BindingInputProbe";
    constexpr std::string_view INPUT_HELD_PROBE = "Conjure.TestScripts.BindingInputHeldProbe";
    constexpr std::string_view RECORD_SIZE_PROBE = "Conjure.TestScripts.BindingRecordSizeProbe";
    constexpr std::string_view LAYER_MASK_PROBE = "Conjure.TestScripts.BindingLayerMaskProbe";
    constexpr std::string_view AUDIO_FIELDS_PROBE = "Conjure.TestScripts.BindingAudioFieldsProbe";
    constexpr std::string_view COLOR_PROBE = "Conjure.TestScripts.BindingColorProbe";
    constexpr std::string_view ENGINE_PHASE_PROBE = "Conjure.TestScripts.BindingEnginePhaseProbe";
    constexpr std::string_view ASSET_MISMATCH_PROBE = "Conjure.TestScripts.BindingAssetMismatchProbe";
    constexpr std::string_view PUBLIC_TYPES_PROBE = "Conjure.TestScripts.PublicTypesProbe";
    constexpr std::string_view FACADE_EXISTS_PROBE = "Conjure.TestScripts.FacadeExistsProbe";
    constexpr std::string_view ENTITY_FAILURE_PROBE = "Conjure.TestScripts.EntityFailureProbe";
    constexpr std::string_view ENTITY_NAME_PROBE = "Conjure.TestScripts.EntityNameProbe";
    constexpr std::string_view ENTITY_TAG_PROBE = "Conjure.TestScripts.EntityTagProbe";
    constexpr std::string_view WORLD_AXIS_PROBE = "Conjure.TestScripts.WorldAxisProbe";
    constexpr std::string_view RESERVED_FACADE_PROBE = "Conjure.TestScripts.ReservedFacadeProbe";
    constexpr std::string_view RESERVED_PREFAB_DESTROY_PROBE = "Conjure.TestScripts.ReservedPrefabDestroyProbe";
    constexpr std::string_view LAMP_PREFAB_PATH = "assets://prefabs/lamp.prefab";
    constexpr std::string_view CLIP_PATH = "assets://audio/tone.wav";
    constexpr std::string_view OTHER_PREFAB_PATH = "assets://prefabs/other.prefab";

    /// <summary>
    /// managed_app in PLAY mode with a clip asset published under CLIP_PATH.
    /// </summary>
    struct managed_fixture : managed_app {
        assets::asset_id clip = test_uuid(3);

        managed_fixture() {
            REQUIRE(runtime().mode() == scripting::script_mode::PLAY);
            std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
            const std::vector<std::byte> wav = make_test_wav();
            const core::result<std::vector<std::byte>> cooked = audio::cook_clip(wav, audio::clip_import_settings{});
            CONJURE_REQUIRE_OK(cooked);
            assets::cooked_blob_header header = {};
            header.type = assets::asset_type_traits<audio::audio_clip>::id();
            header.typeVersion = 1;
            header.importerVersion = 1;
            header.id = clip;
            owned->add(assets::make_cooked_blob(header, {}, cooked.value()), CLIP_PATH);
            app().context().assets->add_source(std::move(owned));
        }
    };

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    constexpr std::string_view NATIVE_STRUCT_HASH_PROBE = "Conjure.TestScripts.NativeStructHashProbe";
    constexpr std::string_view MIRROR_DEFAULTS_PROBE = "Conjure.TestScripts.MirrorDefaultsProbe";

    /// <summary>
    /// One "name=hash;" pair of NativeStructHashProbe: a registered struct name and the C# layout hash.
    /// </summary>
    struct struct_hash {
        std::string name = {};
        u64 hash = 0;
    };

    /// <summary>
    /// The pairs of a "name=hash;..." field (the hash in hex); a malformed pair fails the case.
    /// </summary>
    [[nodiscard]] std::vector<struct_hash> parse_struct_hashes(std::string_view text) {
        std::vector<struct_hash> pairs = {};
        while (!text.empty()) {
            const usize end = text.find(';');
            REQUIRE(end != std::string_view::npos);
            const std::string_view pair = text.substr(0, end);
            text.remove_prefix(end + 1);
            const usize equals = pair.find('=');
            REQUIRE(equals != std::string_view::npos);
            const std::string_view digits = pair.substr(equals + 1);
            struct_hash parsed = {};
            parsed.name = std::string(pair.substr(0, equals));
            const char *digitsEnd = digits.data() + digits.size();
            const std::from_chars_result read = std::from_chars(digits.data(), digitsEnd, parsed.hash, 16);
            REQUIRE(read.ec == std::errc{});
            REQUIRE(read.ptr == digitsEnd);
            pairs.push_back(std::move(parsed));
        }
        return pairs;
    }
#endif

    /// <summary>
    /// "Record.Field=offset;" for every field of the C records the managed side mirrors, in the order
    /// BindingRecordSizeProbe.RecordOffsets lists them, version 3's fields and records included. A managed PadN
    /// is byte N of the C 'pad' array; the raycast hit's tail 'pad' and the frame block's tail 'padding' have no
    /// managed field. PublicRaycastHit is Conjure.RaycastHit, whose Layer and private trigger byte sit where the
    /// native hit holds them.
    /// </summary>
    [[nodiscard]] std::string native_record_offsets() {
        std::string text = {};
        const auto add = [&text](std::string_view record, std::string_view field, usize offset) {
            text += std::format("{}.{}={};", record, field, offset);
        };
        add("RayQuery", "Origin", offsetof(conjure_ray_query, origin));
        add("RayQuery", "Direction", offsetof(conjure_ray_query, direction));
        add("RayQuery", "MaxDistance", offsetof(conjure_ray_query, max_distance));
        add("RayQuery", "LayerMask", offsetof(conjure_ray_query, layer_mask));
        add("RayQuery", "IgnoreEntity", offsetof(conjure_ray_query, ignore_entity));
        add("RayQuery", "Flags", offsetof(conjure_ray_query, flags));
        add("RayQuery", "Pad", offsetof(conjure_ray_query, pad));
        add("ShapeQuery", "Ray", offsetof(conjure_shape_query, ray));
        add("ShapeQuery", "Shape", offsetof(conjure_shape_query, shape));
        add("ShapeQuery", "Pad0", offsetof(conjure_shape_query, pad) + 0);
        add("ShapeQuery", "Pad1", offsetof(conjure_shape_query, pad) + 1);
        add("ShapeQuery", "Pad2", offsetof(conjure_shape_query, pad) + 2);
        add("ShapeQuery", "HalfExtents", offsetof(conjure_shape_query, half_extents));
        add("ShapeQuery", "Radius", offsetof(conjure_shape_query, radius));
        add("ShapeQuery", "Rotation", offsetof(conjure_shape_query, rotation));
        add("ShapeQuery", "PadTail", offsetof(conjure_shape_query, pad_tail));
        add("RaycastHit", "Entity", offsetof(conjure_raycast_hit, entity));
        add("RaycastHit", "Point", offsetof(conjure_raycast_hit, point));
        add("RaycastHit", "Normal", offsetof(conjure_raycast_hit, normal));
        add("RaycastHit", "Distance", offsetof(conjure_raycast_hit, distance));
        add("RaycastHit", "Layer", offsetof(conjure_raycast_hit, layer));
        add("RaycastHit", "IsTrigger", offsetof(conjure_raycast_hit, is_trigger));
        add("PublicRaycastHit", "Layer", offsetof(conjure_raycast_hit, layer));
        add("PublicRaycastHit", "_isTrigger", offsetof(conjure_raycast_hit, is_trigger));
        add("AssetInfo", "State", offsetof(conjure_asset_info, state));
        add("AssetInfo", "Pad0", offsetof(conjure_asset_info, pad) + 0);
        add("AssetInfo", "Pad1", offsetof(conjure_asset_info, pad) + 1);
        add("AssetInfo", "Pad2", offsetof(conjure_asset_info, pad) + 2);
        add("AssetInfo", "Version", offsetof(conjure_asset_info, version));
        add("AssetInfo", "TypeId", offsetof(conjure_asset_info, type_id));
        add("ModuleRecord", "VersionMajor", offsetof(conjure_module_record, version_major));
        add("ModuleRecord", "VersionMinor", offsetof(conjure_module_record, version_minor));
        add("ModuleRecord", "VersionPatch", offsetof(conjure_module_record, version_patch));
        add("ModuleRecord", "State", offsetof(conjure_module_record, state));
        add("ModuleRecord", "Kind", offsetof(conjure_module_record, kind));
        add("ModuleRecord", "Pad0", offsetof(conjure_module_record, pad) + 0);
        add("ModuleRecord", "Pad1", offsetof(conjure_module_record, pad) + 1);
        add("ModuleRecord", "FeatureFlags", offsetof(conjure_module_record, feature_flags));
        add("FrameStatsBlock", "FrameMs", offsetof(conjure_frame_stats_block, frame_ms));
        add("FrameStatsBlock", "PhaseMs", offsetof(conjure_frame_stats_block, phase_ms));
        add("FrameStatsBlock", "SubmitMs", offsetof(conjure_frame_stats_block, submit_ms));
        add("FrameStatsBlock", "FixedSteps", offsetof(conjure_frame_stats_block, fixed_steps));
        add("FrameStatsBlock", "JobsExecuted", offsetof(conjure_frame_stats_block, jobs_executed));
        add("FrameStatsBlock", "FrameMemoryHighWater", offsetof(conjure_frame_stats_block, frame_memory_high_water));
        add("FrameStatsBlock", "ManagedAllocatedBytes", offsetof(conjure_frame_stats_block, managed_allocated_bytes));
        add("FrameStatsBlock", "ManagedHeapBytes", offsetof(conjure_frame_stats_block, managed_heap_bytes));
        add("FrameStatsBlock", "ManagedCollections", offsetof(conjure_frame_stats_block, managed_collections));
        add(
          "FrameStatsBlock",
          "ManagedFullCollections",
          offsetof(conjure_frame_stats_block, managed_full_collections)
        );
        add("TransformData", "Position", offsetof(conjure_transform, position));
        add("TransformData", "Rotation", offsetof(conjure_transform, rotation));
        add("TransformData", "Scale", offsetof(conjure_transform, scale));
        add("UuidData", "High", offsetof(conjure_uuid, high));
        add("UuidData", "Low", offsetof(conjure_uuid, low));
        add("CollisionRecord", "Slot", offsetof(conjure_collision_record, slot));
        add("CollisionRecord", "Other", offsetof(conjure_collision_record, other));
        add("CollisionRecord", "Point", offsetof(conjure_collision_record, point));
        add("CollisionRecord", "Normal", offsetof(conjure_collision_record, normal));
        add("CollisionRecord", "Impulse", offsetof(conjure_collision_record, impulse));
        add("CollisionRecord", "Kind", offsetof(conjure_collision_record, kind));
        add("CreateRequest", "Entity", offsetof(conjure_create_request, entity));
        add("CreateRequest", "ClassHash", offsetof(conjure_create_request, class_hash));
        add("CreateRequest", "Fields", offsetof(conjure_create_request, fields));
        add("CreateRequest", "FieldsSize", offsetof(conjure_create_request, fields_size));
        add("CreateRequest", "Flags", offsetof(conjure_create_request, flags));
        add("CreateRequest", "OutSlot", offsetof(conjure_create_request, out_slot));
        add("ScriptClassDesc", "Hash", offsetof(conjure_script_class_desc, hash));
        add("ScriptClassDesc", "FullName", offsetof(conjure_script_class_desc, full_name));
        add("ScriptClassDesc", "NameLength", offsetof(conjure_script_class_desc, name_length));
        add("ScriptClassDesc", "CallbackMask", offsetof(conjure_script_class_desc, callback_mask));
        add("ScriptClassDesc", "FieldCount", offsetof(conjure_script_class_desc, field_count));
        add("ScriptFieldDesc", "Hash", offsetof(conjure_script_field_desc, hash));
        add("ScriptFieldDesc", "Name", offsetof(conjure_script_field_desc, name));
        add("ScriptFieldDesc", "NameLength", offsetof(conjure_script_field_desc, name_length));
        add("ScriptFieldDesc", "Kind", offsetof(conjure_script_field_desc, kind));
        add("ScriptFieldDesc", "ElementKind", offsetof(conjure_script_field_desc, element_kind));
        add("ScriptFieldDesc", "DefaultJson", offsetof(conjure_script_field_desc, default_json));
        add("ScriptFieldDesc", "DefaultLength", offsetof(conjure_script_field_desc, default_length));
        add("ScriptFieldDesc", "MetaJson", offsetof(conjure_script_field_desc, meta_json));
        add("ScriptFieldDesc", "MetaLength", offsetof(conjure_script_field_desc, meta_length));
        add("ManagedComponentDesc", "NameHash", offsetof(conjure_managed_component_desc, name_hash));
        add("ManagedComponentDesc", "FullName", offsetof(conjure_managed_component_desc, full_name));
        add("ManagedComponentDesc", "NameLength", offsetof(conjure_managed_component_desc, name_length));
        add("ManagedComponentDesc", "Size", offsetof(conjure_managed_component_desc, size));
        add("ManagedComponentDesc", "Alignment", offsetof(conjure_managed_component_desc, alignment));
        add("ManagedComponentDesc", "LayoutHash", offsetof(conjure_managed_component_desc, layout_hash));
        add("ManagedComponentDesc", "SchemaJson", offsetof(conjure_managed_component_desc, schema_json));
        add("ManagedComponentDesc", "SchemaLength", offsetof(conjure_managed_component_desc, schema_length));
        add("QueryScope", "ScopeId", offsetof(conjure_query_scope, scope_id));
        add("QueryScope", "ChunkCount", offsetof(conjure_query_scope, chunk_count));
        add("QueryScope", "Chunks", offsetof(conjure_query_scope, chunks));
        add("ChunkViewData", "Entities", offsetof(conjure_chunk_view, entities));
        add("ChunkViewData", "Columns", offsetof(conjure_chunk_view, columns));
        add("ChunkViewData", "Count", offsetof(conjure_chunk_view, count));
        add("GamepadState", "ButtonsDown", offsetof(conjure_gamepad_state, buttons_down));
        add("GamepadState", "ButtonsPressed", offsetof(conjure_gamepad_state, buttons_pressed));
        add("GamepadState", "ButtonsReleased", offsetof(conjure_gamepad_state, buttons_released));
        add("GamepadState", "Axes", offsetof(conjure_gamepad_state, axes));
        add("GamepadState", "Connected", offsetof(conjure_gamepad_state, connected));
        add("InputBlock", "KeysDown", offsetof(conjure_input_block, keys_down));
        add("InputBlock", "KeysPressed", offsetof(conjure_input_block, keys_pressed));
        add("InputBlock", "KeysReleased", offsetof(conjure_input_block, keys_released));
        add("InputBlock", "MouseX", offsetof(conjure_input_block, mouse_x));
        add("InputBlock", "MouseY", offsetof(conjure_input_block, mouse_y));
        add("InputBlock", "MouseDx", offsetof(conjure_input_block, mouse_dx));
        add("InputBlock", "MouseDy", offsetof(conjure_input_block, mouse_dy));
        add("InputBlock", "ScrollX", offsetof(conjure_input_block, scroll_x));
        add("InputBlock", "ScrollY", offsetof(conjure_input_block, scroll_y));
        add("InputBlock", "MouseButtonsDown", offsetof(conjure_input_block, mouse_buttons_down));
        add("InputBlock", "MouseButtonsPressed", offsetof(conjure_input_block, mouse_buttons_pressed));
        add("InputBlock", "MouseButtonsReleased", offsetof(conjure_input_block, mouse_buttons_released));
        add("InputBlock", "Gamepads", offsetof(conjure_input_block, gamepads));
        add("FrameBlock", "TimeSeconds", offsetof(conjure_frame_block, time_seconds));
        add("FrameBlock", "UnscaledTimeSeconds", offsetof(conjure_frame_block, unscaled_time_seconds));
        add("FrameBlock", "DeltaTime", offsetof(conjure_frame_block, delta_time));
        add("FrameBlock", "FixedDeltaTime", offsetof(conjure_frame_block, fixed_delta_time));
        add("FrameBlock", "TimeScale", offsetof(conjure_frame_block, time_scale));
        add("FrameBlock", "FrameIndex", offsetof(conjure_frame_block, frame_index));
        add("FrameBlock", "FixedStepIndex", offsetof(conjure_frame_block, fixed_step_index));
        add("FrameBlock", "CurrentPhase", offsetof(conjure_frame_block, current_phase));
        add("FrameBlock", "InPlayMode", offsetof(conjure_frame_block, in_play_mode));
        add("FrameBlock", "Input", offsetof(conjure_frame_block, input));
        add("FrameBlock", "ScreenWidth", offsetof(conjure_frame_block, screen_width));
        add("FrameBlock", "ScreenHeight", offsetof(conjure_frame_block, screen_height));
        add("FrameBlock", "ContentScaleX", offsetof(conjure_frame_block, content_scale_x));
        add("FrameBlock", "ContentScaleY", offsetof(conjure_frame_block, content_scale_y));
        add("FrameBlock", "UiScale", offsetof(conjure_frame_block, ui_scale));
        add("FrameBlock", "WindowState", offsetof(conjure_frame_block, window_state));
        add("SpawnComponent", "TypeId", offsetof(conjure_spawn_component, type_id));
        add("SpawnComponent", "Stride", offsetof(conjure_spawn_component, stride));
        add("SpawnComponent", "Data", offsetof(conjure_spawn_component, data));
        add("SpawnDesc", "Components", offsetof(conjure_spawn_desc, components));
        add("SpawnDesc", "ComponentCount", offsetof(conjure_spawn_desc, component_count));
        add("SpawnDesc", "Count", offsetof(conjure_spawn_desc, count));
        add("SpawnDesc", "Parent", offsetof(conjure_spawn_desc, parent));
        add("SpawnDesc", "Flags", offsetof(conjure_spawn_desc, flags));
        add("SpawnDesc", "Pad", offsetof(conjure_spawn_desc, pad));
        add("ManagedMemory", "ThreadAllocatedBytes", offsetof(conjure_managed_memory, thread_allocated_bytes));
        add("ManagedMemory", "HeapBytes", offsetof(conjure_managed_memory, heap_bytes));
        add("ManagedMemory", "Gen0Collections", offsetof(conjure_managed_memory, gen0_collections));
        add("ManagedMemory", "Gen2Collections", offsetof(conjure_managed_memory, gen2_collections));
        return text;
    }
}

TEST_SUITE("bindings/bindings_managed") {

    TEST_CASE("bindings_managed - facade calls increment the counters [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        reset_call_counters();
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), FACADE_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Ran") == 1);
        CHECK(fixture.field_i64(probe, "PositionY") == 3);
        CHECK(fixture.field_i64(probe, "ChildCount") == 2);
        CHECK(fixture.field_i64(probe, "TagMatches") == 1);
        CHECK(fixture.field_i64(probe, "Layer") == 5);
        CHECK(fixture.field_i64(probe, "WasDisabled") == 1);
        CHECK(fixture.field_i64(probe, "LoadedScenes") == 0);
        CHECK(fixture.field_i64(probe, "GravityNegative") == 1);
        CHECK(fixture.field_i64(probe, "MasterGainPercent") == 100);
        CHECK(fixture.field_i64(probe, "CursorMode") == static_cast<i64>(conjure_cursor_mode::HIDDEN));
        CHECK(fixture.field_i64(probe, "ModuleCount") > 0);
        CHECK(fixture.field_i64(probe, "SetLocalCallsSeen") >= 2);
        CHECK(call_count(native_function_id::transform_set_local) >= 2);
        CHECK(call_count(native_function_id::entity_create_many) >= 1);
        CHECK(call_count(native_function_id::entity_set_tag) >= 1);
        CHECK(call_count(native_function_id::physics_get_gravity) >= 1);
        CHECK(call_count(native_function_id::audio_get_master_gain) >= 1);
        CHECK(call_count(native_function_id::input_set_cursor_mode) >= 1);
        CHECK(call_count(native_function_id::diagnostics_check) >= 1);
        CHECK(call_count(native_function_id::diagnostics_call_count) >= 1);
        CHECK(call_count(native_function_id::modules_count) >= 1);
        CHECK(fixture.app().context().checks->all_passed());
        CHECK(fixture.app().context().checks->record_count() >= 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("bindings_managed - every status maps to its exception type [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), EXCEPTION_PROBE);
        CHECK(fixture.field_string(probe, "DeadEntity") == "EntityNotAliveException");
        CHECK(fixture.field_string(probe, "UnknownLayer") == "KeyNotFoundException");
        CHECK(fixture.field_string(probe, "UnknownScene") == "KeyNotFoundException");
        CHECK(fixture.field_string(probe, "NullAsset") == "ArgumentException");
        CHECK(fixture.field_string(probe, "UnloadedPrefab") == "AssetNotLoadedException");
        CHECK(fixture.field_string(probe, "MissingComponent") == "ConjureNativeException");
        CHECK(fixture.field_string(probe, "LastErrorMessage").find("transform_get_local") != std::string::npos);
        CHECK(fixture.field_i64(probe, "LastErrorStatus") == -static_cast<i64>(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "bindings_managed - predicates are false on dead entities and setters throw [dotnet]" * doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), PREDICATE_PROBE);
        CHECK(fixture.field_i64(probe, "IsAlive") == 0);
        CHECK(fixture.field_i64(probe, "Enabled") == 0);
        CHECK(fixture.field_i64(probe, "IsSleeping") == 0);
        CHECK(fixture.field_i64(probe, "IsPlaying") == 0);
        CHECK(fixture.field_i64(probe, "IsInstanceRoot") == 0);
        CHECK(fixture.field_i64(probe, "ChildCount") == 0);
        CHECK(fixture.field_string(probe, "SetterThrew") == "EntityNotAliveException");
        CHECK(fixture.field_string(probe, "EnabledSetterThrew") == "EntityNotAliveException");
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("bindings_managed - AssetRef find, load and IsLoaded [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), ASSET_PROBE, 4);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Found") == 1);
        CHECK(fixture.field_i64(probe, "LoadedBeforeLoad") == 0);
        CHECK(fixture.field_i64(probe, "LoadedAfterFrames") == 1);
        CHECK(fixture.field_i64(probe, "TypeIdNonZero") == 1);
        CHECK(fixture.field_i64(probe, "MissingIsInvalid") == 1);
        CHECK(fixture.field_string(probe, "IdText") == fixture.clip.to_string());
        CHECK(fixture.field_i64(probe, "DurationMs") == 100);
        CHECK(fixture.app().context().assets->state_of(fixture.clip) == assets::asset_state::LOADED);
        scripting_bindings_subsystem *bindings = find_bindings(fixture.app());
        REQUIRE(bindings != nullptr);
        CHECK(bindings->residency().holds(fixture.clip));
        fixture.step(1);
        CHECK(fixture.field_i64(probe, "Released") == 1);
        CHECK_FALSE(bindings->residency().holds(fixture.clip));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("bindings_managed - every mirror resolves to a non-zero id [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const ecs::entity entity = fixture.create("probe");
        const scripting::script_entry_id probe = fixture.attach(entity, MIRROR_PROBE);
        const i64 total = fixture.field_i64(probe, "Total");
        // 21 scene, physics and audio mirrors, then Inactive, Sprite, CameraSettings and Light.
        CHECK(total >= 25);
        CHECK(fixture.field_i64(probe, "LocalTransformRead") == 1);
        CHECK(fixture.field_i64(probe, "RigidBodyAdded") == 1);
        // The probe entity carries no camera in either build.
        CHECK(fixture.field_i64(probe, "CameraExists") == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
#if !defined(CONJURE_WITH_RENDERER) || !CONJURE_WITH_RENDERER
        // Without the renderer its components are never registered: the three mirrors resolve to 0, the Sprite
        // round trip throws, Camera.Exists stays false instead of throwing and the camera entries are stubs.
        MESSAGE("this build has no renderer: its mirrors must stay unresolved; the Sprite round trip is skipped");
        CHECK(fixture.field_i64(probe, "Resolved") == total - 3);
        CHECK(fixture.field_string(probe, "Unresolved") == "Sprite CameraSettings Light ");
        CHECK(fixture.field_string(probe, "Errors").starts_with("InvalidOperationException"));
        CHECK(fixture.field_string(probe, "MainRay") == "NotSupportedException");
#else
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Resolved") == total);
        CHECK(fixture.field_string(probe, "Unresolved").empty());
        // No camera at all: Main is the null entity.
        CHECK(fixture.field_string(probe, "MainRay") == "EntityNotAliveException");

        // The Sprite round trip: the bytes C# wrote are the native sprite_component, and C# reads them back.
        const std::string_view expectedSprite = "125,250,500,750 1000,500,250,750 2000,3000 Screen -2 7 untextured";
        CHECK(fixture.field_string(probe, "SpriteRead") == expectedSprite);
        const ecs::world &world = *fixture.app().context().world;
        const renderer::sprite_component *sprite = world.get<renderer::sprite_component>(entity);
        REQUIRE(sprite != nullptr);
        CHECK(sprite->texture.id().is_null());
        CHECK(sprite->uvRect == math::vec4(0.125f, 0.25f, 0.5f, 0.75f));
        CHECK(sprite->colour == math::vec4(1.0f, 0.5f, 0.25f, 0.75f));
        CHECK(sprite->size == math::vec2(2.0f, 3.0f));
        CHECK(sprite->space == renderer::sprite_space::SCREEN);
        CHECK(sprite->layer == -2);
        CHECK(sprite->order == 7);
#endif
    }

    TEST_CASE(
      "bindings_managed - each renderer mirror's Default is the native default component [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
#if !defined(CONJURE_WITH_RENDERER) || !CONJURE_WITH_RENDERER
        MESSAGE("this build has no renderer: Sprite, CameraSettings and Light have no native component to compare");
#else
        log_capture logs = {};
        managed_fixture fixture = {};
        const ecs::entity sprite = fixture.create("sprite");
        const ecs::entity camera = fixture.create("camera");
        const ecs::entity light = fixture.create("light");
        const std::string entities = std::format(
          R"({{"SpriteEntity":{},"CameraEntity":{},"LightEntity":{}}})",
          sprite.to_u64(),
          camera.to_u64(),
          light.to_u64()
        );
        const scripting::script_entry_id probe =
          fixture.attach(fixture.create("probe"), MIRROR_DEFAULTS_PROBE, 2, entities);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Ran") == 1);
        const ecs::world &world = *fixture.app().context().world;
        const renderer::sprite_component *spriteComponent = world.get<renderer::sprite_component>(sprite);
        REQUIRE(spriteComponent != nullptr);
        CHECK(*spriteComponent == renderer::sprite_component{});
        const renderer::camera_component *cameraComponent = world.get<renderer::camera_component>(camera);
        REQUIRE(cameraComponent != nullptr);
        CHECK(*cameraComponent == renderer::camera_component{});
        const renderer::light_component *lightComponent = world.get<renderer::light_component>(light);
        REQUIRE(lightComponent != nullptr);
        CHECK(*lightComponent == renderer::light_component{});
        CHECK(logs.count(core::log_level::ERR) == 0);
#endif
    }

    TEST_CASE(
      "bindings_managed - every [NativeStruct] mirror hashes like its registered native struct [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
#if !defined(CONJURE_WITH_RENDERER) || !CONJURE_WITH_RENDERER
        MESSAGE("this build has no renderer: the shipped [NativeStruct] mirrors have no registered native struct");
#else
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), NATIVE_STRUCT_HASH_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Ran") == 1);
        const std::vector<struct_hash> shipped = parse_struct_hashes(fixture.field_string(probe, "Shipped"));
        // CameraData and LightData at least; a later nested mirror joins the list without a change here.
        CHECK(shipped.size() >= 2);
        for (const struct_hash &mirror : shipped) {
            CAPTURE(mirror.name);
            const core::reflect::type_info *native = core::reflect::registry().find(mirror.name);
            // A name the registry does not know is a mirror of nothing: it fails too.
            REQUIRE(native != nullptr);
            CHECK(native->layout_hash() == mirror.hash);
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
#endif
    }

    TEST_CASE(
      "bindings_managed - a drifted [NativeStruct] mirror hashes unlike its native struct [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
#if !defined(CONJURE_WITH_RENDERER) || !CONJURE_WITH_RENDERER
        MESSAGE("this build has no renderer: conjure.renderer.camera_data is not registered to compare with");
#else
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), NATIVE_STRUCT_HASH_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        // DriftedCameraData swaps the near and far planes: same size, same kinds, a different layout.
        const std::vector<struct_hash> drifted = parse_struct_hashes(fixture.field_string(probe, "Drifted"));
        REQUIRE(drifted.size() == 1);
        CHECK(drifted.front().name == "conjure.renderer.camera_data");
        const core::reflect::type_info *native = core::reflect::registry().find(drifted.front().name);
        REQUIRE(native != nullptr);
        CHECK(native->layout_hash() != drifted.front().hash);
        const std::vector<struct_hash> shipped = parse_struct_hashes(fixture.field_string(probe, "Shipped"));
        bool shippedSeen = false;
        for (const struct_hash &mirror : shipped) {
            if (mirror.name != drifted.front().name) {
                continue;
            }
            shippedSeen = true;
            CHECK(mirror.hash != drifted.front().hash);
        }
        CHECK(shippedSeen);
#endif
    }

    TEST_CASE(
      "bindings_managed - SetComponent with an AssetRef field binds the slot [dotnet]" * doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const ecs::entity speaker = fixture.create("speaker");
        const scripting::script_entry_id probe = fixture.attach(speaker, BIND_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Added") == 1);
        const audio::audio_source *source = fixture.app().context().world->get<audio::audio_source>(speaker);
        REQUIRE(source != nullptr);
        CHECK(source->clip.id() == fixture.clip);
        CHECK(source->clip.is_bound());
        CHECK(source->clip.slot() == fixture.app().context().assets->bind_slot(fixture.clip));
        CHECK(source->volume == 0.5f);
        CHECK(source->loop);
        CHECK(fixture.field_i64(probe, "VolumeReadPercent") == 50);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("bindings_managed - Modules.Registry lists the engine libraries [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), MODULES_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Count") >= 6);
        CHECK(fixture.field_i64(probe, "EcsLoaded") == 1);
        CHECK(fixture.field_i64(probe, "UnknownLoaded") == 0);
        CHECK(fixture.field_string(probe, "FirstName").starts_with("conjure."));
        CHECK(fixture.field_i64(probe, "EcsIsEngine") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "bindings_managed - the C# records and NativeFunction match the native layouts [dotnet]" * doctest::timeout(120)
    ) {
        // Design 10's BindingRecordSizesMatchDocumented and NativeFunctionEnumMatchesManifest, run from here
        // because the repository has no Conjure.Managed.Tests project: a Records.cs edit whose sizes drift from
        // the native static_asserts fails this case (the field offsets are compared by the F670 case below)
        // instead of producing garbage query results.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), RECORD_SIZE_PROBE);
        CHECK(fixture.field_i64(probe, "Ran") == 1);
        CHECK(fixture.field_i64(probe, "RayQuery") == static_cast<i64>(sizeof(conjure_ray_query)));
        CHECK(fixture.field_i64(probe, "ShapeQuery") == static_cast<i64>(sizeof(conjure_shape_query)));
        CHECK(fixture.field_i64(probe, "RaycastHit") == static_cast<i64>(sizeof(conjure_raycast_hit)));
        CHECK(fixture.field_i64(probe, "AssetInfo") == static_cast<i64>(sizeof(conjure_asset_info)));
        CHECK(fixture.field_i64(probe, "ModuleRecord") == static_cast<i64>(sizeof(conjure_module_record)));
        CHECK(fixture.field_i64(probe, "FrameStatsBlock") == static_cast<i64>(sizeof(conjure_frame_stats_block)));
        CHECK(fixture.field_i64(probe, "TransformData") == static_cast<i64>(sizeof(conjure_transform)));
        CHECK(fixture.field_i64(probe, "UuidData") == static_cast<i64>(sizeof(conjure_uuid)));
        CHECK(fixture.field_i64(probe, "CollisionRecord") == static_cast<i64>(sizeof(conjure_collision_record)));
        const i64 assetRefSize = static_cast<i64>(sizeof(assets::asset_ref<scene::prefab_asset>));
        CHECK(fixture.field_i64(probe, "AssetRefPrefab") == assetRefSize);
        CHECK(fixture.field_i64(probe, "PublicRaycastHit") == static_cast<i64>(sizeof(conjure_raycast_hit)));
        CHECK(fixture.field_i64(probe, "SpawnComponent") == static_cast<i64>(sizeof(conjure_spawn_component)));
        CHECK(fixture.field_i64(probe, "SpawnDesc") == static_cast<i64>(sizeof(conjure_spawn_desc)));
        CHECK(fixture.field_i64(probe, "ManagedMemory") == static_cast<i64>(sizeof(conjure_managed_memory)));
        CHECK(fixture.field_i64(probe, "FrameBlock") == static_cast<i64>(sizeof(conjure_frame_block)));
        CHECK(fixture.field_i64(probe, "NativeFunctionCount") == static_cast<i64>(NATIVE_FUNCTION_COUNT));
        CHECK(fixture.field_i64(probe, "EntryCount") == static_cast<i64>(NATIVE_FUNCTION_COUNT));
        CHECK(fixture.field_i64(probe, "LastFunction") == static_cast<i64>(NATIVE_FUNCTION_COUNT) - 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("bindings_managed - an injected key is seen in the next frame [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), INPUT_PROBE, 3);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "PressedSameFrame") == 0);
        CHECK(fixture.field_i64(probe, "PressedNextFrame") == 1);
        CHECK(fixture.field_i64(probe, "DownNextFrame") == 1);
        CHECK(fixture.field_i64(probe, "ReleasedLater") == 1);
        CHECK(fixture.field_string(probe, "Clipboard") == "managed clipboard");
        CHECK(fixture.field_i64(probe, "GamepadNameEmpty") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - code sweep F637 C# keys, mouse and gamepad buttons are pressed for one frame then held "
      "[dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), INPUT_HELD_PROBE, 5);
        CHECK(fixture.field_string(probe, "Errors").empty());
        // Pressed, Down, Held and Released per frame: the press frame, the frame after it, the release frame.
        CHECK(fixture.field_string(probe, "KeyFrames") == "1100 0110 0001");
        CHECK(fixture.field_string(probe, "MouseFrames") == "1100 0110 0001");
        CHECK(fixture.field_string(probe, "GamepadFrames") == "1100 0110 0001");
        CHECK(fixture.field_i64(probe, "Aliased") == 0);
        // The range check ends at the last real button: Button8 and DpadLeft are down and held on the frame after.
        CHECK(fixture.field_i64(probe, "LastValid") == 4);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F670: every C# record field sits at its native offset [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), RECORD_SIZE_PROBE);
        const std::string managed = fixture.field_string(probe, "Offsets");
        const std::string native = native_record_offsets();
        CHECK_MESSAGE(managed == native, "managed: ", managed, "\nnative:  ", native);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE(
      "regression - sweep-2026-11-12 F437: Physics queries with LayerMask.None hit nothing [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const ecs::entity ground = fixture.create("ground");
        physics::rigid_body body = {};
        body.motionType = physics::motion_type::STATIC;
        CONJURE_REQUIRE_OK(fixture.app().context().world->add<physics::rigid_body>(ground, body));
        physics::box_collider collider = {};
        collider.halfExtents = math::vec3(1.0f);
        CONJURE_REQUIRE_OK(fixture.app().context().world->add<physics::box_collider>(ground, collider));
        fixture.step(1);

        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), LAYER_MASK_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "HitDefault") == 1);            // no mask: every layer
        CHECK(fixture.field_i64(probe, "HitNone") == 0);               // LayerMask.None: no layer
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
#endif

    TEST_CASE(
      "regression - sweep-2026-11-12 F630: the gameplay enums and TransformData live in namespace Conjure [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), PUBLIC_TYPES_PROBE);
        CHECK(fixture.field_string(probe, "Namespaces") == "Conjure,Conjure,Conjure,Conjure,Conjure");
        CHECK(fixture.field_i64(probe, "Sum") == 5);                    // Impulse 1, Normal 0, Press 1, High 2, scale 1
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F675: AudioSource volume, pitch and loop setters skip the asset bind [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        ecs::world &world = *fixture.app().context().world;

        // A source whose clip native already bound: the three setters cost no asset_ref_bind.
        const ecs::entity speaker = fixture.create("speaker");
        static_cast<void>(fixture.attach(speaker, BIND_PROBE));
        const scripting::script_entry_id probe = fixture.attach(speaker, AUDIO_FIELDS_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "BindCalls") == 0);
        const audio::audio_source *source = world.get<audio::audio_source>(speaker);
        REQUIRE(source != nullptr);
        CHECK(source->volume == 0.25f);
        CHECK(source->pitch == 1.5f);
        CHECK(source->loop);
        CHECK(source->clip.id() == fixture.clip);
        CHECK(source->clip.is_bound());

        // A source holding an unbound clip id: the setter still binds it, as every write did before.
        const ecs::entity unbound = fixture.create("unbound");
        audio::audio_source authored = {};
        authored.clip = assets::asset_ref<audio::audio_clip>(fixture.clip);
        authored.playOnStart = false;
        CONJURE_REQUIRE_OK(world.add<audio::audio_source>(unbound, authored));
        CHECK_FALSE(world.get<audio::audio_source>(unbound)->clip.is_bound());
        const scripting::script_entry_id fallback = fixture.attach(unbound, AUDIO_FIELDS_PROBE);
        CHECK(fixture.field_string(fallback, "Errors").empty());
        // Volume binds through SetComponent; Pitch and Loop then read a bound slot and skip the bind.
        CHECK(fixture.field_i64(fallback, "BindCalls") == 1);
        const audio::audio_source *rebound = world.get<audio::audio_source>(unbound);
        REQUIRE(rebound != nullptr);
        CHECK(rebound->volume == 0.25f);
        CHECK(rebound->clip.id() == fixture.clip);
        CHECK(rebound->clip.is_bound());
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - sweep-2026-11-12 F677: Color compares with == and != [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), COLOR_PROBE);
        CHECK(fixture.field_i64(probe, "EqualWhite") == 1);
        CHECK(fixture.field_i64(probe, "DifferentBlack") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F676: with the component registered, Exists answers whether the entity has it "
      "[dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        // The application registers the audio (and, when compiled, physics) components, so Exists falls through to
        // the entity's own components and HasComponent<AudioSourceData> answers instead of throwing.
        const ecs::entity speaker = fixture.create("speaker");
        audio::audio_source source = {};
        source.playOnStart = false;
        CONJURE_REQUIRE_OK(fixture.app().context().world->add<audio::audio_source>(speaker, source));
        const scripting::script_entry_id probe = fixture.attach(speaker, FACADE_EXISTS_PROBE);
        CHECK(fixture.field_i64(probe, "AudioSourceExists") == 1);
        CHECK(fixture.field_i64(probe, "RigidbodyExists") == 0);
        CHECK(fixture.field_i64(probe, "CharacterControllerExists") == 0);
        CHECK(fixture.field_string(probe, "HasComponentThrows") == "none");
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F679: EnginePhase mirrors ecs::system_phase [dotnet]" * doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), ENGINE_PHASE_PROBE);
        std::string expected = {};
        for (u32 index = 0; index < ecs::SYSTEM_PHASE_COUNT; ++index) {
            if (index > 0) {
                expected += ',';
            }
            for (const char character : ecs::to_string(static_cast<ecs::system_phase>(index))) {
                if (character != '_') {
                    expected += character;
                }
            }
        }
        CHECK(fixture.field_string(probe, "Names") == expected);
        CHECK(fixture.field_i64(probe, "MatchesPhaseCount") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F681: a mismatched AssetRef on SetComponent lands unbound [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        // A prefab the sources know by its header only: never loaded, its type is what refuses the bind.
        const assets::asset_id prefab = test_uuid(4);
        std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<scene::prefab_asset>::id();
        header.typeVersion = 1;
        header.importerVersion = 1;
        header.id = prefab;
        owned->add(assets::make_cooked_blob(header, {}, std::vector<std::byte>(16)), OTHER_PREFAB_PATH);
        fixture.app().context().assets->add_source(std::move(owned));

        const ecs::entity speaker = fixture.create("speaker");
        const scripting::script_entry_id probe = fixture.attach(speaker, ASSET_MISMATCH_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "BoundBefore") == 1);
        CHECK(fixture.field_i64(probe, "StillBound") == 0);
        const audio::audio_source *source = fixture.app().context().world->get<audio::audio_source>(speaker);
        REQUIRE(source != nullptr);
        CHECK(source->clip.id() == prefab);
        CHECK_FALSE(source->clip.is_bound());
        CHECK(logs.contains(core::log_level::WARN, "is not of the field's asset type"));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F631: Entity component and script calls report failure by exception [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), ENTITY_FAILURE_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        SUBCASE("dead entity: the mutators and GetComponent throw EntityNotAliveException") {
            CHECK(fixture.field_string(probe, "AddDead") == "EntityNotAliveException");
            CHECK(fixture.field_string(probe, "SetDead") == "EntityNotAliveException");
            CHECK(fixture.field_string(probe, "GetDead") == "EntityNotAliveException");
            CHECK(fixture.field_string(probe, "AddScriptDead") == "EntityNotAliveException");
        }
        SUBCASE("dead entity: the facade Data getter and setter throw like the facade's other members") {
            CHECK(fixture.field_string(probe, "DataGetDead") == "EntityNotAliveException");
            CHECK(fixture.field_string(probe, "DataSetDead") == "EntityNotAliveException");
        }
        SUBCASE("dead entity: the message names the entry and the entity") {
            const std::string message = fixture.field_string(probe, "DeadMessage");
            CHECK(message.starts_with("component_set: entity "));
            CHECK(message.ends_with(" is not alive"));
        }
        SUBCASE("dead entity: RemoveComponent and Destroy answer false instead") {
            CHECK(fixture.field_i64(probe, "RemoveDead") == 0);
            CHECK(fixture.field_i64(probe, "DestroyDead") == 0);
        }
        SUBCASE("a present component: AddComponent throws InvalidOperationException") {
            CHECK(fixture.field_string(probe, "AddFirst") == "none");
            CHECK(fixture.field_string(probe, "AddPresent") == "InvalidOperationException");
            const std::string message = fixture.field_string(probe, "PresentMessage");
            CHECK(message.starts_with("component_add: entity "));
            CHECK(message.ends_with(" already has Layer"));
        }
        SUBCASE("a missing component: SetComponent and GetComponent throw InvalidOperationException") {
            CHECK(fixture.field_string(probe, "SetPresent") == "none");
            CHECK(fixture.field_string(probe, "SetMissing") == "InvalidOperationException");
            CHECK(fixture.field_string(probe, "GetMissing") == "InvalidOperationException");
            // The message pins that the throw is Entity's own, not ComponentType's for an unregistered type.
            const std::string message = fixture.field_string(probe, "MissingMessage");
            CHECK(message.starts_with("component_set: entity "));
            CHECK(message.ends_with(" has no AudioListenerData"));
        }
        SUBCASE("the ScriptBehaviour wrappers and the facade Data setter report the failure") {
            CHECK(fixture.field_string(probe, "WrapperAddPresent") == "InvalidOperationException");
            CHECK(fixture.field_string(probe, "WrapperSetMissing") == "InvalidOperationException");
            CHECK(fixture.field_string(probe, "DataSetMissing") == "InvalidOperationException");
        }
        SUBCASE("RemoveComponent and Destroy answer whether there was something to do") {
            CHECK(fixture.field_i64(probe, "RemovePresent") == 1);
            CHECK(fixture.field_i64(probe, "RemoveMissing") == 0);
            CHECK(fixture.field_i64(probe, "DestroyAlive") == 1);
        }
        SUBCASE("AddScript past MAX_SCRIPTS_PER_ENTITY") {
            static_assert(scripting::MAX_SCRIPTS_PER_ENTITY == 8, "EntityFailureProbe adds exactly this many");
            CHECK(fixture.field_string(probe, "ScriptsUpToMax") == "none");
            CHECK(fixture.field_string(probe, "ScriptPastMax") == "InvalidOperationException");
            CHECK(logs.contains(core::log_level::WARN, "already carries 8 scripts"));
        }
        SUBCASE("inside a query scope an add to a live entity is deferred and the flush applies it like a set") {
            // The call cannot see that the entity already has a Layer; command_buffer::add then overwrites it.
            CHECK(fixture.field_string(probe, "ScopeAddPresent") == "none");
            CHECK(fixture.field_i64(probe, "ScopeLayerAfter") == 6);
            CHECK(fixture.runtime().open_query_scope_count() == 0);
        }
        SUBCASE("inside a query scope an entity Create reserved takes the add and has the component once it closes") {
            CHECK(fixture.field_string(probe, "ScopeAddReserved") == "none");
            CHECK(fixture.field_i64(probe, "ScopeReservedAlive") == 1);
            CHECK(fixture.field_i64(probe, "ScopeReservedLayer") == 7);
        }
        SUBCASE("inside a query scope an entity Create reserved takes scripts up to the maximum, then refuses one") {
            // The refusal is InvalidOperationException, not EntityNotAliveException: the entity exists at the flush
            // and refused (script_add's status says which), although IsAlive answers false for it in the scope.
            CHECK(fixture.field_string(probe, "ScopeReservedScripts") == "none");
            CHECK(fixture.field_string(probe, "ScopeReservedPastMax") == "InvalidOperationException");
            const i64 packed = fixture.field_i64(probe, "ScopeReservedId");
            const ecs::entity reserved = ecs::entity::from_u64(static_cast<u64>(packed));
            CHECK(fixture.runtime().scripts_of(reserved).size() == scripting::MAX_SCRIPTS_PER_ENTITY);
        }
        SUBCASE("inside a query scope the Name, Tag, Layer and Enabled setters record on an entity Create reserved") {
            CHECK(fixture.field_string(probe, "ScopeReservedSetters") == "none");
            CHECK(fixture.field_string(probe, "ScopeSpawnedTagInside").empty());
            CHECK(fixture.field_i64(probe, "ScopeSpawnedLayerInside") == -1);
            CHECK(fixture.field_string(probe, "ScopeSpawnedName") == "scope_spawned");
            CHECK(fixture.field_string(probe, "ScopeSpawnedTag") == "scope_tag");
            CHECK(fixture.field_i64(probe, "ScopeSpawnedLayer") == 9);
            CHECK(fixture.field_i64(probe, "ScopeSpawnedEnabled") == 0);
        }
        SUBCASE("inside a query scope a first AddScript is refused, a further one joins the set") {
            // The first script needs the script component, which the locked world cannot add: add_script keeps
            // its WARN, the only reason its C++ callers get, and the managed caller gets the exception as well.
            CHECK(fixture.field_string(probe, "ScopeAddScriptFresh") == "InvalidOperationException");
            CHECK(logs.contains(core::log_level::WARN, "add_script: no script_component"));
            CHECK(fixture.field_string(probe, "ScopeAddScriptHolder") == "none");
        }
        // The three WARNs are the refused AddScripts' (past the maximum on a live and on a reserved entity, and a
        // first script in a locked scope).
        CHECK(logs.count(core::log_level::WARN) == 3);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "bindings_managed - Entity.Name sets, clears and keeps SetName as a forward [dotnet]" * doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        reset_call_counters();
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), ENTITY_NAME_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_string(probe, "AfterSet") == "renamed");
        CHECK(fixture.field_i64(probe, "FoundByNewName") == 1);
        CHECK(fixture.field_string(probe, "AfterForward") == "forwarded");
        CHECK(fixture.field_i64(probe, "ClearedByEmpty") == 1);
        CHECK(fixture.field_i64(probe, "ClearedByNull") == 1);
        CHECK(fixture.field_string(probe, "DeadSetter") == "EntityNotAliveException");
        CHECK(fixture.field_string(probe, "DeadGetter").empty());           // a getter never throws for it
        CHECK(call_count(native_function_id::entity_set_name) >= 5);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - code sweep OWN-TAG: Entity.Tag reads and writes the text, TagId keeps the hash [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        reset_call_counters();
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), ENTITY_TAG_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        SUBCASE("an untagged live entity reads \"\" and StringId.Null") {
            CHECK(fixture.field_string(probe, "Untagged").empty());
            CHECK(fixture.field_i64(probe, "UntaggedIdNull") == 1);
        }
        SUBCASE("the setter tags the entity and the getter returns the text, also past the stack scratch") {
            CHECK(fixture.field_string(probe, "AfterSet") == "Enemy");
            CHECK(fixture.field_i64(probe, "IdMatchesText") == 1);
            CHECK(fixture.field_i64(probe, "HasTagAfterSet") == 1);
            CHECK(fixture.field_i64(probe, "FoundByTag") == 1);
            CHECK(fixture.field_i64(probe, "LongRoundTrip") == 1);
        }
        SUBCASE("SetTag stays as a forward to the setter") {
            CHECK(fixture.field_string(probe, "AfterForward") == "forwarded");
        }
        SUBCASE("\"\" and null remove the tag") {
            CHECK(fixture.field_i64(probe, "ClearedByEmpty") == 1);
            CHECK(fixture.field_i64(probe, "ClearedByNull") == 1);
        }
        SUBCASE("a StringId.Of value written into the Tag component reads \"\" while TagId keeps it") {
            CHECK(fixture.field_string(probe, "Unspelled").empty());
            CHECK(fixture.field_i64(probe, "UnspelledIdKept") == 1);
        }
        SUBCASE("a dead entity: the setter and the forward throw, the getters answer \"\" and StringId.Null") {
            CHECK(fixture.field_string(probe, "DeadSetter") == "EntityNotAliveException");
            CHECK(fixture.field_string(probe, "DeadForward") == "EntityNotAliveException");
            CHECK(fixture.field_string(probe, "DeadGetter").empty());
            CHECK(fixture.field_i64(probe, "DeadIdNull") == 1);
        }
        CHECK(call_count(native_function_id::entity_get_tag_text) >= 8);
        CHECK(call_count(native_function_id::entity_set_tag) >= 7);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - code sweep OWN-P2 the component facades take writes on an entity a query scope reserved "
      "[dotnet]" *
      doctest::timeout(120)
    ) {
        // Owner ruling 2026-11-24: an entity accepts work as soon as it has an id. The Transform and AudioSource
        // writes on an entity Entity.Create reserved never throw; they are made in call order once the scope closed,
        // after the flush that created the entity, while the reads still throw as for a dead entity.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        reset_call_counters();
        ecs::world &world = *fixture.app().context().world;
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), RESERVED_FACADE_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        SUBCASE("inside the scope the writes are kept and the reads throw") {
            CHECK(fixture.field_string(probe, "InsideWrites") == "none");
            CHECK(fixture.field_string(probe, "InsidePositionRead") == "EntityNotAliveException");
            CHECK(fixture.field_string(probe, "InsideVolumeRead") == "EntityNotAliveException");
            CHECK(fixture.field_i64(probe, "InsidePlaying") == 0);
        }
        SUBCASE("once the scope closed the writes were made in call order on the created entity") {
            CHECK(fixture.field_f64(probe, "PositionX") == doctest::Approx(11.0));     // Position, then Translate
            CHECK(fixture.field_f64(probe, "PositionY") == doctest::Approx(2.0));
            CHECK(fixture.field_f64(probe, "ScaleX") == doctest::Approx(2.0));
            CHECK(fixture.field_f64(probe, "Volume") == doctest::Approx(0.5));
            const ecs::entity spawned = ecs::entity::from_u64(static_cast<u64>(fixture.field_i64(probe, "SpawnedId")));
            REQUIRE(world.is_alive(spawned));
            const scene::local_transform *local = std::as_const(world).get<scene::local_transform>(spawned);
            REQUIRE(local != nullptr);
            CHECK(local->position.x == doctest::Approx(11.0f));
            CHECK(local->scale.y == doctest::Approx(2.0f));
            const audio::audio_source *source = std::as_const(world).get<audio::audio_source>(spawned);
            REQUIRE(source != nullptr);
            CHECK(source->volume == doctest::Approx(0.5f));
            CHECK(call_count(native_function_id::audio_play) == 1);                    // the doomed Play was dropped
        }
        SUBCASE("a Destroy recorded in the scope drops the writes kept for the entity") {
            CHECK(fixture.field_string(probe, "DoomedWrites") == "none");
            const ecs::entity doomed = ecs::entity::from_u64(static_cast<u64>(fixture.field_i64(probe, "DoomedId")));
            CHECK_FALSE(world.is_alive(doomed));
        }
        SUBCASE("a scope left open: the next managed call makes the write once native force-closed it") {
            const ecs::entity leaked = ecs::entity::from_u64(static_cast<u64>(fixture.field_i64(probe, "LeakedId")));
            REQUIRE(world.is_alive(leaked));
            const scene::local_transform *local = std::as_const(world).get<scene::local_transform>(leaked);
            REQUIRE(local != nullptr);
            CHECK(local->position.x == doctest::Approx(7.0f));
            CHECK(logs.contains(core::log_level::WARN, "force-closed"));
        }
        CHECK(logs.count(core::log_level::WARN) == 1);           // the leaked scope's force-close
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - code sweep OWN-P2 Destroy on a prefab root instantiated in the same query scope cancels it "
      "[dotnet]" *
      doctest::timeout(120)
    ) {
        // Owner ruling 2026-11-24: the destroy is recorded in call order and the flush cancels the instantiation, so
        // neither the root nor the prefab's child is ever created.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        scene::scene &active = fixture.app().context().scenes->active();
        const ecs::entity source = fixture.create("lamp_root");
        const core::result<ecs::entity> child = active.create_entity(core::string_id("lamp_child"), source);
        CONJURE_REQUIRE_OK(child);
        const core::result<scene::prefab_document> document = scene::prefab::build_document(active, source, "lamp");
        CONJURE_REQUIRE_OK(document);
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::write_prefab(document.value(), out));
        const std::string text = out.finish();
        const assets::asset_id lamp = test_uuid(5);
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<scene::prefab_asset>::id();
        header.typeVersion = 1;
        header.importerVersion = 1;
        header.id = lamp;
        std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
        const std::span<const std::byte> bytes = std::as_bytes(std::span<const char>(text.data(), text.size()));
        owned->add(
          assets::make_cooked_blob(header, {}, std::vector<std::byte>(bytes.begin(), bytes.end())),
          LAMP_PREFAB_PATH
        );
        fixture.app().context().assets->add_source(std::move(owned));
        const core::result<assets::asset_handle> loaded = fixture.app().context().assets->load_blocking(lamp);
        CONJURE_REQUIRE_OK(loaded);
        fixture.app().context().assets->pin(lamp, true);
        CONJURE_REQUIRE_OK(active.destroy_now(source));

        ecs::world &world = *fixture.app().context().world;
        const ecs::entity holder = fixture.create("probe");
        const u32 entitiesBefore = world.entity_count();
        const scripting::script_entry_id probe = fixture.attach(holder, RESERVED_PREFAB_DESTROY_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "AliveInside") == 0);
        CHECK(fixture.field_i64(probe, "DestroyResult") == 1);
        CHECK(fixture.field_i64(probe, "AliveAfter") == 0);
        const ecs::entity root = ecs::entity::from_u64(static_cast<u64>(fixture.field_i64(probe, "RootId")));
        CHECK_FALSE(root.is_null());
        CHECK_FALSE(world.is_alive(root));
        CHECK(world.entity_count() == entitiesBefore);
        CONJURE_CHECK_OK(active.validate());
        CHECK(logs.count(core::log_level::ERR) == 0);
        fixture.app().context().assets->pin(lamp, false);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F631: Transform world axes follow the parent chain while Forward stays local "
      "[dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_fixture fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), WORLD_AXIS_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        SUBCASE("a child at identity under a parent turned 90 degrees about +Y and scaled by 2") {
            CHECK(fixture.field_string(probe, "Forward") == "0,0,-1000");        // parent space
            CHECK(fixture.field_string(probe, "WorldForward") == "-1000,0,0");   // unit length despite the scale
            CHECK(fixture.field_string(probe, "WorldRight") == "0,0,-1000");
            CHECK(fixture.field_string(probe, "WorldUp") == "0,1000,0");
        }
        SUBCASE("a negative scale mirrors the axis, a zero scale collapses it to zero") {
            CHECK(fixture.field_string(probe, "MirroredForward") == "1000,0,0");
            CHECK(fixture.field_string(probe, "CollapsedForward") == "0,0,0");
            CHECK(fixture.field_string(probe, "CollapsedRight") == "0,0,-1000");
        }
        SUBCASE("a tiny but nonzero scale still gives a unit axis") {
            CHECK(fixture.field_string(probe, "TinyForward") == "-1000,0,0");
            CHECK(fixture.field_string(probe, "FaintForward") == "-1000,0,0");
        }
        SUBCASE("after LookAt the world axis points at the target, the local one does not") {
            CHECK(fixture.field_string(probe, "LookAtWorldForward") == "600,0,800");
            CHECK(fixture.field_string(probe, "LookAtForward") == "-800,0,600");
        }
        SUBCASE("a dead entity throws") {
            CHECK(fixture.field_string(probe, "DeadAxis") == "EntityNotAliveException");
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
