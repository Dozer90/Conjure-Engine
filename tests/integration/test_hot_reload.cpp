#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"
#include "conjure/testing/world_builder.h"
#include "integration_engine_support.h"

#include "conjure/core/hash.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_context.h"
#include "conjure/scene/scene.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"

#include <filesystem>
#include <system_error>
#include <vector>

// docs/design/08_testing.md 6.1 test_hot_reload [scripting]: the game assembly (the variant A build of
// Conjure.TestScripts, staged into a temp directory) is rebuilt in place as variant B, the way the managed build
// delivers a rebuilt assembly over the one the game loaded, and reloaded inside a running application: a serialised
// field keeps its value, a class removed in B leaves its component disabled with one WARN, and the frame loop keeps
// running OnUpdate on the surviving scripts. The reload is requested through script_runtime::reload_game_assembly,
// the call scripting::hot_reload_controller::tick makes once a request (an editor build, script.reload) has passed
// its debounce window; the engine does not expose the controller to its host (09_cross_area_resolutions.md 4 item
// 15e) and never enables its file watch, so the request itself cannot be driven from here. The controller's own
// debounce and tick are covered by tests/scripting/test_hot_reload.cpp.

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr std::string_view RELOAD_PROBE = "Conjure.TestScripts.ReloadProbe";
    constexpr std::string_view VANISHING = "Conjure.TestScripts.Vanishing";
    constexpr std::string_view VARIANT_PROBE = "Conjure.TestScripts.VariantProbe";
    constexpr u32 FRAMES_BEFORE_RELOAD = 3;
    constexpr u32 FRAMES_AFTER_RELOAD = 2;
    constexpr u32 FRAMES_SURVIVING = 3;

    [[nodiscard]] scripting::script_entry_id only_script(scripting::script_runtime &runtime, ecs::entity entity) {
        const std::vector<scripting::script_entry_id> scripts = runtime.scripts_of(entity);
        REQUIRE(scripts.size() == 1);
        return scripts.front();
    }

    [[nodiscard]] i64 field_i64(
      scripting::script_runtime &runtime,
      scripting::script_entry_id id,
      std::string_view name
    ) {
        core::result<core::json::document> fields = runtime.collect_entry_fields(id);
        CONJURE_REQUIRE_OK(fields);
        return fields.value().root()[name].as_i64(-1);
    }

    /// <summary>
    /// "Builds" a variant of the game assembly at 'target': the prebuilt variant copied over whatever is there, with
    /// a fresh write time, the way a managed rebuild replaces the output the game loaded (the load context reads
    /// the bytes, so the file is free to change underneath a running game).
    /// </summary>
    void build_variant(std::string_view variant, const std::filesystem::path &target) {
        const std::filesystem::path source = integration_test::test_scripts_assembly(variant);
        REQUIRE(std::filesystem::is_regular_file(source));
        std::error_code status = {};
        std::filesystem::create_directories(target.parent_path(), status);
        REQUIRE_FALSE(status);
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, status);
        REQUIRE_FALSE(status);
        std::filesystem::last_write_time(target, std::filesystem::file_time_type::clock::now(), status);
        REQUIRE_FALSE(status);
    }
}

TEST_SUITE("integration/hot_reload") {

    TEST_CASE("hot_reload - reloading the variant B assembly keeps serialised fields and disables the removed class "
      "[scripting]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        temp_directory staging("integration_hot_reload");
        const std::filesystem::path assembly = staging.path() / "game" / "Conjure.TestScripts.dll";
        build_variant("v1", assembly);

        headless_engine_options options = integration_test::quiet_options();
        options.enableScripting = true;
        options.useNullScriptHost = false;
        options.gameAssembly = &assembly;
        headless_engine engine(options);
        scripting::script_runtime *runtime = engine.context().scripting;
        REQUIRE(runtime != nullptr);
        REQUIRE(runtime->is_assembly_loaded());
        scene::scene &draft = engine.active_scene();

        const core::result<ecs::entity> probeEntity = spawn_script_entity(draft, "Probe", RELOAD_PROBE);
        CONJURE_REQUIRE_OK(probeEntity);
        const scripting::script_entry_id probe = only_script(*runtime, probeEntity.value());
        core::result<core::json::document> health = core::json::document::parse(R"({"Health": 3})");
        CONJURE_REQUIRE_OK(health);
        CONJURE_REQUIRE_OK(runtime->set_field(probe, core::hash::fnv1a64("Health"), health.value().root()["Health"]));
        const core::result<ecs::entity> vanishingEntity = spawn_script_entity(draft, "Vanishing", VANISHING);
        CONJURE_REQUIRE_OK(vanishingEntity);
        const scripting::script_entry_id vanishing = only_script(*runtime, vanishingEntity.value());
        const core::result<ecs::entity> variantEntity = spawn_script_entity(draft, "Variant", VARIANT_PROBE);
        CONJURE_REQUIRE_OK(variantEntity);
        const scripting::script_entry_id variant = only_script(*runtime, variantEntity.value());

        engine.step_frames(FRAMES_BEFORE_RELOAD);
        CHECK(field_i64(*runtime, probe, "Health") == 3);
        CHECK(field_i64(*runtime, probe, "Updates") == FRAMES_BEFORE_RELOAD);
        CHECK(field_i64(*runtime, vanishing, "Updates") == FRAMES_BEFORE_RELOAD);
        CHECK(field_i64(*runtime, variant, "Variant") == 1);
        CHECK(engine.logs().count(core::log_level::WARN) == 0);

        // The rebuild lands on the loaded assembly's path; the reload picks the new bytes up from there.
        build_variant("v2", assembly);
        CONJURE_REQUIRE_OK(runtime->reload_game_assembly(assembly));
        CHECK(runtime->stats().reloadCount == 1);
        engine.step_frames(FRAMES_AFTER_RELOAD);

        // Serialised state survived, the private state and OnCreate-only counters restarted on the new assembly,
        // and the surviving scripts kept updating (Updates is public, so its count carried over the reload).
        REQUIRE(runtime->entry(probe) != nullptr);
        CHECK(runtime->entry(probe)->is_live());
        CHECK(field_i64(*runtime, probe, "Health") == 3);
        CHECK(field_i64(*runtime, probe, "OnlyInV2") == 7);
        CHECK(field_i64(*runtime, probe, "Updates") == FRAMES_BEFORE_RELOAD + FRAMES_AFTER_RELOAD);
        CHECK(field_i64(*runtime, variant, "Variant") == 2);

        // The class that vanished in B: its entry is disabled, its JSON kept, exactly one WARN named it.
        const scripting::script_entry *vanished = runtime->entry(vanishing);
        REQUIRE(vanished != nullptr);
        CHECK_FALSE(vanished->is_live());
        CHECK(vanished->has_flag(scripting::script_entry_flags::CLASS_MISSING));
        CHECK(vanished->fields.root()["Updates"].as_i64() == FRAMES_BEFORE_RELOAD);
        CHECK(engine.logs().count(core::log_level::WARN) == 1);
        CHECK(engine.logs().contains(core::log_level::WARN, "is not in the loaded assembly"));

        // The frame loop keeps running the survivors: three more frames, three more updates, the vanished one still.
        engine.step_frames(FRAMES_SURVIVING);
        CHECK(field_i64(*runtime, probe, "Updates") == FRAMES_BEFORE_RELOAD + FRAMES_AFTER_RELOAD + FRAMES_SURVIVING);
        CHECK(vanished->fields.root()["Updates"].as_i64() == FRAMES_BEFORE_RELOAD);
        CHECK(engine.logs().count(core::log_level::WARN) == 1);
        CHECK(engine.logs().count(core::log_level::ERR) == 0);
    }
}
