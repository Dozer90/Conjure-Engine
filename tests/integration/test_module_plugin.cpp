#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/temp_directory.h"
#include "integration_engine_support.h"

#include "conjure/core/json/writer.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/engine_context.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"
#include "conjure/scene/scene.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>

// docs/design/08_testing.md 6.1 test_module_plugin: a project whose modules.search_paths names the built
// sample_plugin fixture loads it at startup; entities carrying its plugin_counter component are counted by its
// system for 60 frames; unloading the module removes the component type and leaves no ERROR behind. This
// executable links conjure_shared (LINK_ENGINE_SHARED) because the plugin does: one engine copy in the process.
// In such an executable the allocation counters see nothing (cmake/ConjureTesting.cmake), so this scenario has no
// leak gate; the leak coverage of a module load and unload lives in tests/modules over the static engine.

using namespace conjure;
using namespace conjure::testing;

#ifndef CONJURE_INTEGRATION_MODULE_DIR
    #define CONJURE_INTEGRATION_MODULE_DIR ""
#endif

namespace {
    constexpr std::string_view PLUGIN_NAME = "sample_plugin";
    constexpr core::string_id COUNTER_NAME("integration.plugin_counter");
    constexpr u32 FRAMES = 60;
    constexpr u32 ENTITY_COUNT = 3;

    /// <summary>
    /// A project whose module search path is the directory the build placed the fixture plugin in. The path is
    /// written through the JSON writer, so whatever characters the build directory holds are escaped.
    /// </summary>
    void write_host_project(const std::filesystem::path &directory) {
        const std::filesystem::path moduleRoot(CONJURE_INTEGRATION_MODULE_DIR);
        REQUIRE_FALSE(moduleRoot.empty());
        core::json::writer modules = {};
        modules.begin_object();
        modules.key("search_paths");
        modules.begin_array();
        modules.value(std::string_view(moduleRoot.generic_string()));
        modules.end_array();
        modules.key("engine_plugins");
        modules.value(false);
        modules.key("required");
        modules.begin_array();
        modules.value(PLUGIN_NAME);
        modules.end_array();
        modules.end_object();
        const std::string project =
          integration_test::project_json("plugin_host", {}, "\"modules\": " + modules.finish());
        integration_test::write_file(directory / "project.json", project);
    }

    [[nodiscard]] u32 counter_of(const ecs::world &world, ecs::entity entity, ecs::component_id id) {
        const void *raw = world.get_raw(entity, id);
        REQUIRE(raw != nullptr);
        u32 value = 0;
        std::memcpy(&value, raw, sizeof(value));
        return value;
    }
}

TEST_SUITE("integration/module_plugin") {

    TEST_CASE("module_plugin - sample_plugin loads, counts 60 frames on its component and unloads cleanly") {
        temp_directory directory("integration_module_plugin");
        write_host_project(directory.path());
        headless_engine_options options = integration_test::quiet_options();
        options.projectDirectory = &directory.path();
        options.checkMemory = false;   // LINK_ENGINE_SHARED: the counters see nothing here (ConjureTesting.cmake)
        headless_engine engine(options);

        modules::module_manager *manager = engine.context().modules;
        REQUIRE(manager != nullptr);
        const modules::module_id plugin = manager->registry().find(core::string_id(PLUGIN_NAME));
        REQUIRE(manager->registry().is_valid(plugin));
        REQUIRE(manager->registry().state(plugin) == modules::module_state::LOADED);

        ecs::component_registry &registry = *engine.context().components;
        const ecs::component_id counterId = registry.find(COUNTER_NAME);
        REQUIRE(counterId.is_valid());
        CHECK(registry.info(counterId).size == sizeof(u32));

        std::array<ecs::entity, ENTITY_COUNT> counted = {};
        const std::array<std::byte, sizeof(u32)> zero = {};
        for (ecs::entity &entity : counted) {
            entity = integration_test::create_at(engine.active_scene(), "Counted", math::vec3(0.0f));
            CONJURE_REQUIRE_OK(engine.world().add_raw(entity, counterId, zero));
        }

        engine.step_frames(FRAMES);
        for (const ecs::entity entity : counted) {
            CHECK(counter_of(engine.world(), entity, counterId) == FRAMES);
        }

        // Unloading strips every instance of the component; the entities survive, and the type keeps its id so a
        // reload finds the same component_id (engine/src/engine/README.md: ids are stable per name).
        CONJURE_REQUIRE_OK(manager->unload(plugin, modules::unload_policy::REMOVE_INSTANCES));
        // An unload retires the id's generation; the record is found again by name.
        const modules::module_id unloaded = manager->registry().find(core::string_id(PLUGIN_NAME));
        REQUIRE(manager->registry().is_valid(unloaded));
        CHECK(manager->registry().state(unloaded) == modules::module_state::UNLOADED);
        for (const ecs::entity entity : counted) {
            CHECK(engine.world().is_alive(entity));
            CHECK_FALSE(engine.world().has_raw(entity, counterId));
        }
        engine.step_frames(5);
        CHECK(engine.world().entity_count() == ENTITY_COUNT);
        for (const ecs::entity entity : counted) {
            CHECK_FALSE(engine.world().has_raw(entity, counterId));
        }
        CHECK(engine.logs().count(core::log_level::ERR) == 0);
    }
}
