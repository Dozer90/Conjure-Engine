#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/assets/asset_extension_desc.h"
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/world.h"
#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_service.h"
#include "conjure/modules/module_types.h"
#include "engine/module_consumers/assets_module_consumer.h"
#include "engine/module_consumers/ecs_module_consumer.h"
#include "engine/module_consumers/module_content_file_system.h"
#include "engine/module_consumers/scripting_module_consumer.h"

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_host.h"
    #include "conjure/scripting/script_runtime.h"
#endif

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// The module consumers (docs/design/07_modules.md D3, D6, 9; R2, R19, R24): the ECS consumer turns
// extension records into components and systems, counts live uses and strips instances on unload; the
// assets consumer validates importer and type records and mounts module content; the scripting consumer
// records assemblies; module_content_file_system routes modules://<name>/ paths.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    struct spinner {
        f32 speed = 1.0f;
    };
}

CONJURE_COMPONENT(spinner, "test.module.spinner");

namespace {

    u32 sSpinnerRuns = 0;

    void spin(ecs::system_context &) {
        ++sSpinnerRuns;
    }

    constexpr core::string_id SPINNER_NAME{"test.module.spinner"};

    [[nodiscard]] modules::module_manifest manifest_named(std::string name) {
        modules::module_manifest manifest = {};
        manifest.name = std::move(name);
        manifest.kind = modules::module_kind::PLUGIN;
        manifest.managedAssemblies = {"managed/Game.dll"};
        manifest.contentDirectories = {"content"};
        return manifest;
    }

    struct consumer_fixture {
        ecs::component_registry registry = {};
        ecs::world world;                               // no default: built on the registry
        ecs::scheduler scheduler;                       // no default: built on the world
        modules::module_registry moduleRegistry = {};
        modules::service_table services = {};
        modules::module_manifest manifest = manifest_named("test.module");
        modules::module_id id = {};
        std::unique_ptr<modules::module_context> context = nullptr;
        modules::module_registration_set set = {};

        consumer_fixture() :
            world(registry),
            scheduler(world, nullptr) {
            id.index = 3;
            id.generation = 1;
            context = std::make_unique<modules::module_context>(
              id,
              modules::module_host_kind::TEST,
              modules::host_engine_version(),
              manifest,
              std::filesystem::path("C:/modules/test"),
              services
            );
        }

        void register_spinner(u32 size = sizeof(spinner), u32 alignment = alignof(spinner)) {
            modules::module_registrar registrar(set);
            ecs::component_type_desc component = {};
            component.name = "test.module.spinner";
            component.size = size;
            component.alignment = alignment;
            component.flags = ecs::component_flags::TRIVIAL | ecs::component_flags::TRANSIENT;
            registrar.register_extension(component);

            static const core::string_id WRITES[] = {SPINNER_NAME};
            ecs::system_extension_desc system = {};
            system.name = "test.module.spin";
            system.phase = ecs::system_phase::UPDATE;
            system.flags = ecs::system_flags::NONE;
            system.writes = WRITES;
            system.writeCount = 1;
            system.run = &spin;
            registrar.register_extension(system);
        }
    };
}

TEST_SUITE("engine/module_consumers") {

    TEST_CASE("ecs_module_consumer - attach registers the component and the system, detach removes them") {
        consumer_fixture fixture = {};
        fixture.register_spinner();
        ecs_module_consumer consumer(fixture.registry, fixture.world, fixture.scheduler);
        CHECK(consumer.name() == core::string_id("ecs"));
        CONJURE_REQUIRE_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        consumer.attach(fixture.id, *fixture.context, fixture.set);

        const std::vector<ecs::component_id> components = consumer.components_of(fixture.id);
        REQUIRE(components.size() == 1);
        CHECK(fixture.registry.find(SPINNER_NAME) == components[0]);
        CHECK(consumer.systems_of(fixture.id).size() == 1);
        CHECK(fixture.scheduler.system_count() == 1);

        // An entity with the module's component counts as a live use.
        const core::result<ecs::entity> entity = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(entity);
        std::array<std::byte, sizeof(spinner)> bytes = {};
        CONJURE_REQUIRE_OK(fixture.world.add_raw(entity.value(), components[0], bytes));
        CHECK(consumer.live_use_count(fixture.id) == 1);
        CONJURE_CHECK_ERROR(
          consumer.can_detach(fixture.id, modules::unload_policy::REFUSE_IF_IN_USE),
          modules::module_error::IN_USE
        );
        CONJURE_CHECK_OK(consumer.can_detach(fixture.id, modules::unload_policy::REMOVE_INSTANCES));

        sSpinnerRuns = 0;
        ecs::frame_info frame = {};
        fixture.scheduler.run_phase(ecs::system_phase::UPDATE, frame);
        CHECK(sSpinnerRuns == 1);

        consumer.detach(fixture.id, nullptr);
        CHECK(fixture.scheduler.system_count() == 0);
        CHECK(fixture.world.instance_count(components[0]) == 0);
        CHECK(consumer.live_use_count(fixture.id) == 0);
        CHECK(consumer.components_of(fixture.id).empty());
    }

    TEST_CASE("ecs_module_consumer - validate rejects a layout conflict and a system without access") {
        consumer_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.registry.register_component<spinner>(ecs::component_flags::TRANSIENT));
        fixture.register_spinner(64, 16);
        ecs_module_consumer consumer(fixture.registry, fixture.world, fixture.scheduler);
        CONJURE_CHECK_ERROR(
          consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry),
          modules::module_error::REGISTRATION_CONFLICT
        );

        consumer_fixture noAccess = {};
        {
            modules::module_registrar registrar(noAccess.set);
            ecs::system_extension_desc system = {};
            system.name = "test.module.lazy";
            system.run = &spin;
            registrar.register_extension(system);
        }
        ecs_module_consumer strict(noAccess.registry, noAccess.world, noAccess.scheduler);
        CONJURE_CHECK_ERROR(
          strict.validate(noAccess.id, noAccess.set, noAccess.moduleRegistry),
          modules::module_error::REGISTRATION_CONFLICT
        );
        {
            modules::module_registrar registrar(noAccess.set);
            ecs::system_extension_desc exclusive = {};
            exclusive.name = "test.module.exclusive";
            exclusive.flags = ecs::system_flags::EXCLUSIVE;
            exclusive.run = &spin;
            registrar.register_extension(exclusive);
        }
        noAccess.set.extensions.erase(noAccess.set.extensions.begin());
        CONJURE_CHECK_OK(strict.validate(noAccess.id, noAccess.set, noAccess.moduleRegistry));
    }

    TEST_CASE("ecs_module_consumer - a reload keeps trivial columns in place and stores the snapshot") {
        consumer_fixture fixture = {};
        fixture.register_spinner();
        ecs_module_consumer consumer(fixture.registry, fixture.world, fixture.scheduler);
        CONJURE_REQUIRE_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        consumer.attach(fixture.id, *fixture.context, fixture.set);
        const ecs::component_id component = consumer.components_of(fixture.id)[0];
        const core::result<ecs::entity> entity = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(entity);
        std::array<std::byte, sizeof(spinner)> bytes = {};
        CONJURE_REQUIRE_OK(fixture.world.add_raw(entity.value(), component, bytes));

        modules::reload_context snapshot = {};
        consumer.detach(fixture.id, &snapshot);
        CHECK(snapshot.size() == 1);
        CHECK(fixture.world.instance_count(component) == 1);
        consumer.attach(fixture.id, *fixture.context, fixture.set);
        consumer.restore(fixture.id, snapshot);
        CHECK(consumer.components_of(fixture.id)[0] == component);
        CHECK(consumer.live_use_count(fixture.id) == 1);
        consumer.detach(fixture.id, nullptr);
    }

    TEST_CASE("module_content_file_system - routes by module name and lists the roots") {
        module_content_file_system content = {};
        std::shared_ptr<core::vfs::memory_file_system> alpha = std::make_shared<core::vfs::memory_file_system>();
        CONJURE_REQUIRE_OK(alpha->write_text_file("textures/a.txt", "A"));
        CONJURE_REQUIRE_OK(content.add_root("alpha", alpha));
        CONJURE_CHECK_ERROR(content.add_root("alpha", alpha), core::error_code::ALREADY_EXISTS);
        CONJURE_CHECK_ERROR(content.add_root("bad/name", alpha), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(content.add_root("", alpha), core::error_code::INVALID_ARGUMENT);

        const core::result<std::string> text = content.read_text_file("alpha/textures/a.txt");
        CONJURE_REQUIRE_OK(text);
        CHECK(text.value() == "A");
        CHECK(content.exists("alpha/textures/a.txt"));
        CHECK_FALSE(content.exists("beta/textures/a.txt"));
        CONJURE_CHECK_ERROR(content.read_file("beta/x"), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(content.read_file(""), core::error_code::INVALID_ARGUMENT);
        CHECK(content.roots() == std::vector<std::string>{"alpha"});
        CHECK_FALSE(content.is_read_only());

        std::vector<std::string> listed = {};
        const core::vfs::enumerate_callback collect = [&listed](std::string_view path, const core::vfs::file_stat &) {
            listed.emplace_back(path);
        };
        CONJURE_REQUIRE_OK(content.enumerate("", core::vfs::enumerate_flags::RECURSIVE, collect));
        CHECK(std::find(listed.begin(), listed.end(), "alpha") != listed.end());
        CHECK(std::find(listed.begin(), listed.end(), "alpha/textures/a.txt") != listed.end());

        CONJURE_REQUIRE_OK(content.remove_root("alpha"));
        CONJURE_CHECK_ERROR(content.remove_root("alpha"), core::error_code::NOT_FOUND);
        CHECK(content.roots().empty());
    }

    TEST_CASE("assets_module_consumer - validate rejects records without factories or with known names") {
        assets::asset_type_registry types = {};
        assets::importer_registry importers = {};
        consumer_fixture fixture = {};
        assets_module_consumer consumer(types, importers, nullptr);
        CHECK(consumer.name() == core::string_id("assets"));
        CONJURE_CHECK_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        {
            modules::module_registrar registrar(fixture.set);
            assets::importer_extension_desc importer = {};
            importer.id = "test.importer";
            registrar.register_extension(importer);
        }
        CONJURE_CHECK_ERROR(
          consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry),
          modules::module_error::REGISTRATION_CONFLICT
        );
        fixture.set.extensions.clear();
        {
            modules::module_registrar registrar(fixture.set);
            assets::asset_type_extension_desc type = {};
            type.name = "";
            registrar.register_extension(type);
        }
        CONJURE_CHECK_ERROR(
          consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry),
          modules::module_error::REGISTRATION_CONFLICT
        );
        CONJURE_CHECK_OK(consumer.can_detach(fixture.id, modules::unload_policy::REFUSE_IF_IN_USE));
        CHECK(consumer.live_use_count(fixture.id) == 0);
    }

    TEST_CASE("assets_module_consumer - attach mounts the module content and detach unmounts it") {
        log_capture logs = {};
        assets::asset_type_registry types = {};
        assets::importer_registry importers = {};
        consumer_fixture fixture = {};
        assets_module_consumer consumer(types, importers, nullptr);
        consumer.attach(fixture.id, *fixture.context, fixture.set);
        CHECK(consumer.content().roots() == std::vector<std::string>{"test.module"});
        consumer.detach(fixture.id, nullptr);
        CHECK(consumer.content().roots().empty());
    }

    TEST_CASE("scripting_module_consumer - records manifest and registrar assemblies and joins managed reloads") {
        consumer_fixture fixture = {};
        {
            modules::module_registrar registrar(fixture.set);
            registrar.register_managed_assembly("managed/Extra.dll");
            registrar.register_managed_assembly("managed/Game.dll");
        }
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        scripting_module_consumer consumer(nullptr);
#else
        scripting_module_consumer consumer = {};
#endif
        CHECK(consumer.name() == core::string_id("scripting"));
        CHECK(consumer.participates_in_managed_reload());
        CONJURE_CHECK_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        consumer.attach(fixture.id, *fixture.context, fixture.set);
        const std::vector<std::filesystem::path> assemblies = consumer.assemblies_of(fixture.id);
        REQUIRE(assemblies.size() == 2);
        CHECK(assemblies[0] == std::filesystem::path("C:/modules/test") / "managed/Game.dll");
        CHECK(assemblies[1] == std::filesystem::path("C:/modules/test") / "managed/Extra.dll");
        CHECK(consumer.live_use_count(fixture.id) == 0);
        modules::reload_context reload = {};
        consumer.detach(fixture.id, &reload);
        CHECK(consumer.assemblies_of(fixture.id).empty());
        CHECK(consumer.managed_reload_count() == 0);
    }

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    TEST_CASE("scripting_module_consumer - a managed reload skips a runtime cleared with set_runtime(nullptr)") {
        log_capture logs = {};
        scripting::script_runtime runtime(scripting::create_script_host(scripting::script_backend::NULL_HOST));
        scripting::script_runtime_config config = {};
        config.host.backend = scripting::script_backend::NULL_HOST;
        CONJURE_REQUIRE_OK(runtime.initialize(config));
        CONJURE_REQUIRE_OK(runtime.load_game_assembly({}));
        REQUIRE(runtime.is_assembly_loaded());

        // Control: a consumer that keeps the runtime reloads the game assembly once.
        consumer_fixture kept = {};
        scripting_module_consumer keeping(&runtime);
        keeping.attach(kept.id, *kept.context, kept.set);
        modules::reload_context keptReload = {};
        keeping.detach(kept.id, &keptReload);
        CHECK(keeping.managed_reload_count() == 1);

        // The scripting subsystem clears the pointer before it destroys the runtime (disable or shutdown).
        consumer_fixture cleared = {};
        scripting_module_consumer clearing(&runtime);
        clearing.set_runtime(nullptr);
        clearing.attach(cleared.id, *cleared.context, cleared.set);
        modules::reload_context clearedReload = {};
        clearing.detach(cleared.id, &clearedReload);
        CHECK(clearing.managed_reload_count() == 0);
        runtime.shutdown();
    }
#endif
}
