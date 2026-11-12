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
#include "conjure/core/string_id.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/engine/application.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_runtime.h"
#include "engine/subsystems/audio_components.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
#endif

#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
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
    constexpr std::string_view RECORD_SIZE_PROBE = "Conjure.TestScripts.BindingRecordSizeProbe";
    constexpr std::string_view LAYER_MASK_PROBE = "Conjure.TestScripts.BindingLayerMaskProbe";
    constexpr std::string_view AUDIO_FIELDS_PROBE = "Conjure.TestScripts.BindingAudioFieldsProbe";
    constexpr std::string_view COLOR_PROBE = "Conjure.TestScripts.BindingColorProbe";
    constexpr std::string_view ENGINE_PHASE_PROBE = "Conjure.TestScripts.BindingEnginePhaseProbe";
    constexpr std::string_view ASSET_MISMATCH_PROBE = "Conjure.TestScripts.BindingAssetMismatchProbe";
    constexpr std::string_view PUBLIC_TYPES_PROBE = "Conjure.TestScripts.PublicTypesProbe";
    constexpr std::string_view CLIP_PATH = "assets://audio/tone.wav";
    constexpr std::string_view OTHER_PREFAB_PATH = "assets://prefabs/other.prefab";

    /// <summary>
    /// A headless application over the real CLR with the test scripts loaded in PLAY mode and a clip asset
    /// published under CLIP_PATH.
    /// </summary>
    struct managed_fixture {
        std::unique_ptr<engine::application> app = nullptr;
        assets::asset_id clip = test_uuid(3);

        managed_fixture() {
            engine::engine_config config = headless_config();
            config.scripting.useNullHost = false;
            config.scripting.managedDirectory = managed_directory();
            config.scripting.gameAssembly = test_scripts_assembly();
            app = make_app(config);
            CONJURE_REQUIRE_OK(app->initialise());
            REQUIRE(app->context().scripting != nullptr);
            REQUIRE(app->context().scripting->is_assembly_loaded());
            REQUIRE(app->context().scripting->mode() == scripting::script_mode::PLAY);

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
            app->context().assets->add_source(std::move(owned));
        }

        ~managed_fixture() {
            app->shutdown();
        }

        managed_fixture(const managed_fixture &) = delete;
        managed_fixture &operator=(const managed_fixture &) = delete;

        [[nodiscard]] scripting::script_runtime &runtime() noexcept {
            return *app->context().scripting;
        }

        [[nodiscard]] ecs::entity create(std::string_view name) {
            const core::result<ecs::entity> created =
              app->context().scenes->active().create_entity(core::string_id(name));
            REQUIRE(created.has_value());
            return created.value();
        }

        /// <summary>
        /// Adds a script by class name and runs 'frames' engine frames so its callbacks execute.
        /// </summary>
        [[nodiscard]] scripting::script_entry_id attach(
          ecs::entity entity,
          std::string_view className,
          u32 frames = 2
        ) {
            const core::json::document fields = core::json::document::object();
            const scripting::script_entry_id id =
              runtime().add_script(entity, scripting::script_name_hash(className), fields.root());
            REQUIRE(id.is_valid());
            step(frames);
            return id;
        }

        void step(u32 frames) {
            for (u32 frame = 0; frame < frames; ++frame) {
                CONJURE_REQUIRE_OK(app->step_frame());
            }
        }

        [[nodiscard]] core::json::document fields_of(scripting::script_entry_id id) {
            core::result<core::json::document> collected = runtime().collect_entry_fields(id);
            REQUIRE(collected.has_value());
            return std::move(collected).value();
        }

        [[nodiscard]] i64 field_i64(scripting::script_entry_id id, std::string_view name) {
            return fields_of(id).root()[name].as_i64(-1);
        }

        [[nodiscard]] std::string field_string(scripting::script_entry_id id, std::string_view name) {
            return std::string(fields_of(id).root()[name].as_string());
        }
    };

    /// <summary>
    /// "Record.Field=offset;" for every field of the C records the managed side mirrors, in the order
    /// BindingRecordSizeProbe.RecordOffsets lists them. A managed PadN is byte N of the C 'pad' array, and the
    /// frame block's tail 'padding' has no managed field.
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
        add("ShapeQuery", "Ray", offsetof(conjure_shape_query, ray));
        add("ShapeQuery", "Shape", offsetof(conjure_shape_query, shape));
        add("ShapeQuery", "Pad0", offsetof(conjure_shape_query, pad) + 0);
        add("ShapeQuery", "Pad1", offsetof(conjure_shape_query, pad) + 1);
        add("ShapeQuery", "Pad2", offsetof(conjure_shape_query, pad) + 2);
        add("ShapeQuery", "HalfExtents", offsetof(conjure_shape_query, half_extents));
        add("ShapeQuery", "Radius", offsetof(conjure_shape_query, radius));
        add("ShapeQuery", "Rotation", offsetof(conjure_shape_query, rotation));
        add("RaycastHit", "Entity", offsetof(conjure_raycast_hit, entity));
        add("RaycastHit", "Point", offsetof(conjure_raycast_hit, point));
        add("RaycastHit", "Normal", offsetof(conjure_raycast_hit, normal));
        add("RaycastHit", "Distance", offsetof(conjure_raycast_hit, distance));
        add("RaycastHit", "Layer", offsetof(conjure_raycast_hit, layer));
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
        CHECK(fixture.app->context().checks->all_passed());
        CHECK(fixture.app->context().checks->record_count() >= 1);
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
        CHECK(fixture.app->context().assets->state_of(fixture.clip) == assets::asset_state::LOADED);
        scripting_bindings_subsystem *bindings = find_bindings(*fixture.app);
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
        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), MIRROR_PROBE);
        const i64 total = fixture.field_i64(probe, "Total");
        CHECK(total >= 20);
        CHECK(fixture.field_i64(probe, "Resolved") == total);
        CHECK(fixture.field_string(probe, "Unresolved").empty());
        CHECK(fixture.field_i64(probe, "LocalTransformRead") == 1);
        CHECK(fixture.field_i64(probe, "RigidBodyAdded") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
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
        const audio::audio_source *source = fixture.app->context().world->get<audio::audio_source>(speaker);
        REQUIRE(source != nullptr);
        CHECK(source->clip.id() == fixture.clip);
        CHECK(source->clip.is_bound());
        CHECK(source->clip.slot() == fixture.app->context().assets->bind_slot(fixture.clip));
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
        CONJURE_REQUIRE_OK(fixture.app->context().world->add<physics::rigid_body>(ground, body));
        physics::box_collider collider = {};
        collider.halfExtents = math::vec3(1.0f);
        CONJURE_REQUIRE_OK(fixture.app->context().world->add<physics::box_collider>(ground, collider));
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
        ecs::world &world = *fixture.app->context().world;

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
        fixture.app->context().assets->add_source(std::move(owned));

        const ecs::entity speaker = fixture.create("speaker");
        const scripting::script_entry_id probe = fixture.attach(speaker, ASSET_MISMATCH_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "BoundBefore") == 1);
        CHECK(fixture.field_i64(probe, "StillBound") == 0);
        const audio::audio_source *source = fixture.app->context().world->get<audio::audio_source>(speaker);
        REQUIRE(source != nullptr);
        CHECK(source->clip.id() == prefab);
        CHECK_FALSE(source->clip.is_bound());
        CHECK(logs.contains(core::log_level::WARN, "is not of the field's asset type"));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
