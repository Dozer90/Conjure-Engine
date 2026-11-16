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
#include "conjure/ecs/ecs_error.h"
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
#include <span>
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

    /// <summary>
    /// The record of a trivially copyable, transient component with spinner's layout.
    /// </summary>
    [[nodiscard]] ecs::component_type_desc trivial_component(const char *name) {
        ecs::component_type_desc component = {};
        component.name = name;
        component.size = sizeof(spinner);
        component.alignment = alignof(spinner);
        component.flags = ecs::component_flags::TRIVIAL | ecs::component_flags::TRANSIENT;
        return component;
    }

    /// <summary>
    /// The record of an UPDATE system that runs spin() and names the given components. The lists must
    /// outlive the registration set, as a module's static tables do.
    /// </summary>
    [[nodiscard]] ecs::system_extension_desc system_naming(
      const char *name,
      std::span<const core::string_id> reads,
      std::span<const core::string_id> writes
    ) {
        ecs::system_extension_desc system = {};
        system.name = name;
        system.phase = ecs::system_phase::UPDATE;
        system.reads = reads.data();
        system.readCount = static_cast<u32>(reads.size());
        system.writes = writes.data();
        system.writeCount = static_cast<u32>(writes.size());
        system.run = &spin;
        return system;
    }

    [[nodiscard]] ecs::component_mask mask_of(ecs::component_id id) {
        ecs::component_mask mask = {};
        mask.set(id);
        return mask;
    }

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

    TEST_CASE("regression - code sweep F259 ecs_module_consumer validate refuses a system name its phase holds") {
        // scheduler::add_system answers NAME_CONFLICT for a name its phase already holds. validate() used
        // to accept such a record, and attach() then logged an ERROR and dropped the system; the clash
        // fails the module load now.
        consumer_fixture fixture = {};
        fixture.register_spinner();
        ecs_module_consumer consumer(fixture.registry, fixture.world, fixture.scheduler);

        // The name in another phase is no clash.
        ecs::system_desc elsewhere = {};
        elsewhere.name = core::string_id("test.module.spin");
        elsewhere.phase = ecs::system_phase::LATE_UPDATE;
        elsewhere.run = [](ecs::system_context &) {};
        CONJURE_REQUIRE_OK(fixture.scheduler.add_system(std::move(elsewhere)));
        CONJURE_CHECK_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));

        // The name in the record's own phase is: nothing of the module is applied.
        ecs::system_desc taken = {};
        taken.name = core::string_id("test.module.spin");
        taken.phase = ecs::system_phase::UPDATE;
        taken.run = [](ecs::system_context &) {};
        const core::result<ecs::system_id> holder = fixture.scheduler.add_system(std::move(taken));
        CONJURE_REQUIRE_OK(holder);
        const core::result<void> refused = consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry);
        REQUIRE_FALSE(refused.has_value());
        CONJURE_CHECK_ERROR(refused, modules::module_error::REGISTRATION_CONFLICT);
        CHECK(refused.get_error().message().find("'test.module.spin'") != std::string::npos);
        CHECK(refused.get_error().message().find("UPDATE") != std::string::npos);
        CHECK(fixture.scheduler.system_count() == 2);
        CHECK_FALSE(fixture.registry.find(SPINNER_NAME).is_valid());

        // Once the holder is gone the same set validates and attaches without an ERROR.
        CONJURE_REQUIRE_OK(fixture.scheduler.remove_system(holder.value()));
        CONJURE_REQUIRE_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        {
            const log_capture logs = {};
            consumer.attach(fixture.id, *fixture.context, fixture.set);
            CHECK(logs.count(core::log_level::ERR) == 0);
        }
        CHECK(consumer.systems_of(fixture.id).size() == 1);
        CHECK(fixture.scheduler.system_count() == 2);

        // A second module bringing the same system clashes with the first one's.
        consumer_fixture second = {};
        second.register_spinner();
        modules::module_id secondId = {};
        secondId.index = 4;
        secondId.generation = 1;
        CONJURE_CHECK_ERROR(
          consumer.validate(secondId, second.set, fixture.moduleRegistry),
          modules::module_error::REGISTRATION_CONFLICT
        );
        CHECK(consumer.systems_of(secondId).empty());

        // After the first module is detached (an unload, or the first half of a reload) the name is free.
        consumer.detach(fixture.id, nullptr);
        CONJURE_CHECK_OK(consumer.validate(secondId, second.set, fixture.moduleRegistry));
    }

    TEST_CASE("ecs_module_consumer - validate refuses a system a module registers twice in one phase") {
        static const core::string_id WRITES[] = {SPINNER_NAME};
        const auto system_record = [](const char *name, ecs::system_phase phase) {
            ecs::system_extension_desc system = {};
            system.name = name;
            system.phase = phase;
            system.writes = WRITES;
            system.writeCount = 1;
            system.run = &spin;
            return system;
        };

        // The same name in two phases is two systems.
        consumer_fixture twoPhases = {};
        {
            modules::module_registrar registrar(twoPhases.set);
            registrar.register_extension(system_record("test.module.tick", ecs::system_phase::UPDATE));
            registrar.register_extension(system_record("test.module.tick", ecs::system_phase::FIXED_UPDATE));
        }
        ecs_module_consumer lenient(twoPhases.registry, twoPhases.world, twoPhases.scheduler);
        CONJURE_REQUIRE_OK(lenient.validate(twoPhases.id, twoPhases.set, twoPhases.moduleRegistry));
        {
            const log_capture logs = {};
            lenient.attach(twoPhases.id, *twoPhases.context, twoPhases.set);
            CHECK(logs.count(core::log_level::ERR) == 0);
        }
        CHECK(lenient.systems_of(twoPhases.id).size() == 2);
        lenient.detach(twoPhases.id, nullptr);

        // Twice in one phase is refused before anything reaches the scheduler.
        consumer_fixture twice = {};
        {
            modules::module_registrar registrar(twice.set);
            registrar.register_extension(system_record("test.module.tick", ecs::system_phase::UPDATE));
            registrar.register_extension(system_record("test.module.other", ecs::system_phase::UPDATE));
            registrar.register_extension(system_record("test.module.tick", ecs::system_phase::UPDATE));
        }
        ecs_module_consumer strict(twice.registry, twice.world, twice.scheduler);
        const core::result<void> refused = strict.validate(twice.id, twice.set, twice.moduleRegistry);
        REQUIRE_FALSE(refused.has_value());
        CONJURE_CHECK_ERROR(refused, modules::module_error::REGISTRATION_CONFLICT);
        CHECK(refused.get_error().message().find("'test.module.tick' is registered twice") != std::string::npos);
        CHECK(twice.scheduler.system_count() == 0);
    }

    TEST_CASE("ecs_module_consumer - a system added after a module attached cannot take a name the module holds") {
        // validate() compares a module's systems with the ones the scheduler holds when the module loads.
        // The engine adds its own systems at startup step 18, after the modules loaded at startup, so a
        // startup module that uses the name of an engine system is not refused: it keeps the name, and
        // the later add_system is the one that answers NAME_CONFLICT (01_ecs.md D13).
        consumer_fixture fixture = {};
        fixture.register_spinner();
        ecs_module_consumer consumer(fixture.registry, fixture.world, fixture.scheduler);
        CONJURE_REQUIRE_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        consumer.attach(fixture.id, *fixture.context, fixture.set);
        const std::vector<ecs::system_id> moduleSystems = consumer.systems_of(fixture.id);
        REQUIRE(moduleSystems.size() == 1);

        const core::string_id name("test.module.spin");
        u32 lateRuns = 0;
        const auto late_system = [&lateRuns, name](ecs::system_phase phase) {
            ecs::system_desc late = {};
            late.name = name;
            late.phase = phase;
            late.flags = ecs::system_flags::EXCLUSIVE;
            late.run = [&lateRuns](ecs::system_context &) {
                ++lateRuns;
            };
            return late;
        };

        const core::result<ecs::system_id> refused =
          fixture.scheduler.add_system(late_system(ecs::system_phase::UPDATE));
        REQUIRE_FALSE(refused.has_value());
        CONJURE_CHECK_ERROR(refused, ecs::ecs_error::NAME_CONFLICT);
        CHECK(fixture.scheduler.system_count() == 1);
        CHECK(fixture.scheduler.find_system(ecs::system_phase::UPDATE, name) == moduleSystems[0]);

        // The module's system is the one that runs under the name.
        sSpinnerRuns = 0;
        const ecs::frame_info frame = {};
        fixture.scheduler.run_phase(ecs::system_phase::UPDATE, frame);
        CHECK(sSpinnerRuns == 1);
        CHECK(lateRuns == 0);

        // Another phase is free, and so is the module's phase once the module is detached.
        CONJURE_CHECK_OK(fixture.scheduler.add_system(late_system(ecs::system_phase::LATE_UPDATE)));
        consumer.detach(fixture.id, nullptr);
        CONJURE_CHECK_OK(fixture.scheduler.add_system(late_system(ecs::system_phase::UPDATE)));
        fixture.scheduler.run_phase(ecs::system_phase::UPDATE, frame);
        CHECK(sSpinnerRuns == 1);
        CHECK(lateRuns == 1);
    }

    TEST_CASE("regression - code sweep F077 a module system gains a component registered after its attach") {
        // The consumer used to drop a read or write name the registry did not know, silently and for good:
        // the system kept a smaller access set and shared a stage with a writer of the component once the
        // module that owns it had loaded. The name is warned about at attach and the scheduler resolves it
        // when the component is registered.
        static const core::string_id GEAR_NAME = core::string_id::intern("test.optional.gear");
        static const core::string_id WATCH_READS[] = {GEAR_NAME};
        static const core::string_id WATCH_WRITES[] = {SPINNER_NAME};
        static const core::string_id TURN_WRITES[] = {GEAR_NAME};

        consumer_fixture fixture = {};
        {
            modules::module_registrar registrar(fixture.set);
            registrar.register_extension(trivial_component("test.module.spinner"));
            registrar.register_extension(system_naming("test.module.watch", WATCH_READS, WATCH_WRITES));
        }
        ecs_module_consumer consumer(fixture.registry, fixture.world, fixture.scheduler);
        CONJURE_REQUIRE_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        {
            const log_capture logs = {};
            consumer.attach(fixture.id, *fixture.context, fixture.set);
            CHECK(logs.count(core::log_level::WARN) == 1);
            CHECK(logs.contains(core::log_level::WARN, "module 'test.module'"));
            CHECK(logs.contains(core::log_level::WARN, "system 'test.module.watch'"));
            CHECK(logs.contains(core::log_level::WARN, "component 'test.optional.gear' (reads[0])"));
        }
        const std::vector<ecs::system_id> watchSystems = consumer.systems_of(fixture.id);
        REQUIRE(watchSystems.size() == 1);
        const ecs::system_desc *watch = fixture.scheduler.system(watchSystems[0]);
        REQUIRE(watch != nullptr);
        const ecs::component_mask spinnerMask = mask_of(fixture.registry.find(SPINNER_NAME));
        CHECK(watch->reads.none());
        CHECK(watch->writes == spinnerMask);
        CHECK(fixture.scheduler.stages(ecs::system_phase::UPDATE).size() == 1);

        // The optional module loads afterwards: it owns the component and a system that writes it.
        consumer_fixture optional = {};
        modules::module_id optionalId = {};
        optionalId.index = 4;
        optionalId.generation = 1;
        {
            modules::module_registrar registrar(optional.set);
            registrar.register_extension(trivial_component("test.optional.gear"));
            registrar.register_extension(system_naming("test.optional.turn", {}, TURN_WRITES));
        }
        CONJURE_REQUIRE_OK(consumer.validate(optionalId, optional.set, fixture.moduleRegistry));
        {
            const log_capture logs = {};
            consumer.attach(optionalId, *optional.context, optional.set);
            CHECK(logs.count(core::log_level::WARN) == 0);
        }
        const ecs::component_id gear = fixture.registry.find(GEAR_NAME);
        REQUIRE(gear.is_valid());
        const ecs::component_mask gearMask = mask_of(gear);

        // Its attach finalised the scheduler, which resolved the first module's system again: the reader
        // and the writer of the component are in two stages.
        CHECK(watch->reads == gearMask);
        CHECK(watch->writes == spinnerMask);
        const std::vector<ecs::system_id> turnSystems = consumer.systems_of(optionalId);
        REQUIRE(turnSystems.size() == 1);
        const std::span<const ecs::stage_info> stages = fixture.scheduler.stages(ecs::system_phase::UPDATE);
        REQUIRE(stages.size() == 2);
        CHECK(stages[0].systems == std::vector<ecs::system_id>{watchSystems[0]});
        CHECK(stages[1].systems == std::vector<ecs::system_id>{turnSystems[0]});
        sSpinnerRuns = 0;
        const ecs::frame_info frame = {};
        fixture.scheduler.run_phase(ecs::system_phase::UPDATE, frame);
        CHECK(sSpinnerRuns == 2);

        // The component stays registered when its module unloads (ids are never reused), so the access
        // stays as well.
        consumer.detach(optionalId, nullptr);
        CHECK(watch->reads == gearMask);
        CHECK(fixture.scheduler.stages(ecs::system_phase::UPDATE).size() == 1);
        consumer.detach(fixture.id, nullptr);
        CHECK(fixture.scheduler.system_count() == 0);
    }

    TEST_CASE("ecs_module_consumer - attach warns once for each component name that is not registered") {
        static const core::string_id MISSING_READ = core::string_id::intern("test.missing.read");
        // Constant-initialised and never interned, as a module's table usually is: the name prints as its hash.
        static constexpr core::string_id MISSING_WRITE{"test.missing.write.never_interned"};
        static const core::string_id TYPO_READS[] = {SPINNER_NAME, MISSING_READ};
        static const core::string_id TYPO_WRITES[] = {MISSING_WRITE};
        static const core::string_id BLIND_READS[] = {MISSING_READ};

        consumer_fixture fixture = {};
        {
            modules::module_registrar registrar(fixture.set);
            registrar.register_extension(trivial_component("test.module.spinner"));
            registrar.register_extension(system_naming("test.module.typo", TYPO_READS, TYPO_WRITES));
            registrar.register_extension(system_naming("test.module.blind", BLIND_READS, {}));
        }
        ecs_module_consumer consumer(fixture.registry, fixture.world, fixture.scheduler);

        // An unknown name is no reason to refuse the module, even when a system knows none of its names.
        CONJURE_REQUIRE_OK(consumer.validate(fixture.id, fixture.set, fixture.moduleRegistry));
        {
            const log_capture logs = {};
            consumer.attach(fixture.id, *fixture.context, fixture.set);
            CHECK(logs.count(core::log_level::WARN) == 3);
            CHECK(logs.contains(
              core::log_level::WARN,
              "system 'test.module.typo' names component 'test.missing.read' (reads[1])"
            ));
            CHECK(logs.contains(core::log_level::WARN, "system 'test.module.typo' names component '#"));
            CHECK(logs.contains(core::log_level::WARN, "(writes[0]), which is not registered"));
            CHECK(logs.contains(
              core::log_level::WARN,
              "system 'test.module.blind' names component 'test.missing.read' (reads[0])"
            ));
            CHECK_FALSE(logs.contains(core::log_level::WARN, "'test.module.spinner'"));
        }

        // Both systems are registered and run; each holds the access of the names that resolved.
        const std::vector<ecs::system_id> systems = consumer.systems_of(fixture.id);
        REQUIRE(systems.size() == 2);
        const ecs::system_desc *typo = fixture.scheduler.system(systems[0]);
        const ecs::system_desc *blind = fixture.scheduler.system(systems[1]);
        REQUIRE(typo != nullptr);
        REQUIRE(blind != nullptr);
        const ecs::component_mask spinnerMask = mask_of(fixture.registry.find(SPINNER_NAME));
        CHECK(typo->reads == spinnerMask);
        CHECK(typo->writes.none());
        CHECK(blind->reads.none());
        CHECK(blind->writes.none());
        sSpinnerRuns = 0;
        const ecs::frame_info frame = {};
        fixture.scheduler.run_phase(ecs::system_phase::UPDATE, frame);
        CHECK(sSpinnerRuns == 2);

        // A name nothing ever registers stays out, and the warning is not repeated when the registry grows.
        {
            const log_capture logs = {};
            CONJURE_REQUIRE_OK(fixture.registry.register_described(trivial_component("test.module.unrelated")));
            CONJURE_REQUIRE_OK(fixture.scheduler.finalise());
            fixture.scheduler.run_phase(ecs::system_phase::UPDATE, frame);
            CHECK(logs.count(core::log_level::WARN) == 0);
        }
        CHECK(typo->reads == spinnerMask);
        CHECK(typo->writes.none());
        CHECK(blind->reads.none());

        // A reload adds the systems again and says it again; a set whose names all resolve is quiet.
        consumer.detach(fixture.id, nullptr);
        {
            const log_capture logs = {};
            consumer.attach(fixture.id, *fixture.context, fixture.set);
            CHECK(logs.count(core::log_level::WARN) == 3);
        }
        consumer.detach(fixture.id, nullptr);
        consumer_fixture clean = {};
        clean.register_spinner();
        ecs_module_consumer quiet(clean.registry, clean.world, clean.scheduler);
        CONJURE_REQUIRE_OK(quiet.validate(clean.id, clean.set, clean.moduleRegistry));
        {
            const log_capture logs = {};
            quiet.attach(clean.id, *clean.context, clean.set);
            CHECK(logs.count(core::log_level::WARN) == 0);
        }
        quiet.detach(clean.id, nullptr);
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
