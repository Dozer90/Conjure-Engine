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
#include "conjure/engine/application.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_runtime.h"
#include "engine/subsystems/audio_components.h"

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
    constexpr std::string_view CLIP_PATH = "assets://audio/tone.wav";

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
        // because the repository has no Conjure.Managed.Tests project: a Records.cs edit that drifts from the
        // native static_asserts fails this case instead of producing garbage query results.
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
}
