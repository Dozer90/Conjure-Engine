#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "fake_consumer.h"
#include "fixture_descriptors.h"
#include "shared_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_events.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/dynamic_library.h"
#include "conjure/core/event_bus.h"
#include "conjure/core/string_id.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    using count_fn = std::uint32_t (*)(void);

    /// <summary>
    /// How many times a refused fixture's entry ran, read through a second handle to its binary
    /// (the same module instance while the process keeps it mapped).
    /// </summary>
    std::uint32_t entry_count_of(std::string_view fixture) {
        result<dynamic_library> library = dynamic_library::load(fixture_binary(fixture));
        REQUIRE(library.has_value());
        const count_fn count = library->symbol_as<count_fn>("conjure_fixture_entry_count");
        REQUIRE(count != nullptr);
        return count();
    }

    struct discovered_host {
        conjure::testing::log_capture logs = {};
        std::vector<std::string> log = {};
        fake_consumer ecs{"ecs", &log};
        shared_host host = {};

        discovered_host() {
            host.manager.add_consumer(ecs);
            ecs.bind_registry(host.manager.registry());
            const result<u32> discovered = host.manager.discover(fixture_root());
            INFO("discover: ", conjure::testing::describe_error(discovered));
            REQUIRE(discovered.has_value());
        }

        [[nodiscard]] module_id id(std::string_view name) const {
            const module_id found = host.manager.registry().find(string_id(name));
            REQUIRE(found.is_valid());
            return found;
        }

        [[nodiscard]] module_state state(std::string_view name) const {
            return host.manager.registry().state(id(name));
        }
    };
}

TEST_SUITE("modules/module_manager_shared") {

    TEST_CASE("module_manager - discover registers every fixture and skips the one for other platforms") {
        discovered_host fixture = {};
        const module_registry &registry = fixture.host.manager.registry();
        CHECK(registry.size() == 11);
        CHECK(fixture.state("module_wrong_platform") == module_state::SKIPPED);
        CHECK(fixture.state("module_alpha") == module_state::DISCOVERED);
        CHECK(fixture.state("module_managed_only") == module_state::DISCOVERED);
        CHECK(registry.binary_path(fixture.id("module_alpha")) == fixture_binary("module_alpha"));
        CHECK(registry.binary_path(fixture.id("module_managed_only")).empty());
        CHECK(registry.root_directory(fixture.id("module_alpha")) == fixture_directory("module_alpha"));
        CHECK(registry.manifest(registry.all()[0]).name == "module_alpha");
    }

    TEST_CASE("module_manager - alpha loads, registers its records and logs through the host logger") {
        discovered_host fixture = {};
        const module_id alpha = fixture.id("module_alpha");
        CONJURE_REQUIRE_OK(fixture.host.manager.load(alpha));

        CHECK(fixture.state("module_alpha") == module_state::LOADED);
        CHECK(fixture.log == std::vector<std::string>{"validate:ecs:module_alpha", "attach:ecs:module_alpha"});
        CHECK(fixture.logs.contains(log_level::INFO, "module_alpha entry running on host test"));
        CHECK(fixture.logs.contains(log_level::INFO, "loaded module 'module_alpha'"));

        const module_registration_set &set = fixture.host.manager.registry().registrations(alpha);
        const small_vector<const fixtures::component_desc *, 8> components =
          extensions_of<fixtures::component_desc>(set);
        REQUIRE(components.size() == 1);
        CHECK(std::string_view(components[0]->name) == "alpha_spinner");
        CHECK(components[0]->size == 16);
        CHECK(extensions_of<fixtures::system_desc>(set).size() == 1);
        const small_vector<const fixtures::probe_desc *, 8> probes = extensions_of<fixtures::probe_desc>(set);
        REQUIRE(probes.size() == 1);
        CHECK(*probes[0]->activatedCount == 1);
        CHECK(*probes[0]->deactivatingCount == 0);
        CHECK(set.lifecycle.onActivated != nullptr);
        CHECK(set.lifecycle.onDeactivating != nullptr);

        CONJURE_REQUIRE_OK(fixture.host.manager.unload(alpha));
        CHECK(fixture.log.back() == "detach:ecs:module_alpha");
        CHECK(fixture.logs.contains(log_level::INFO, "unloaded module 'module_alpha'"));
    }

    TEST_CASE("module_manager - beta loads after alpha with its panel and both assembly paths") {
        discovered_host fixture = {};
        const module_id beta = fixture.id("module_beta");
        CONJURE_REQUIRE_OK(fixture.host.manager.load(beta));

        const module_registry &registry = fixture.host.manager.registry();
        REQUIRE(registry.load_order().size() == 2);
        CHECK(registry.load_order()[0] == fixture.id("module_alpha"));
        CHECK(registry.load_order()[1] == beta);
        const module_registration_set &set = registry.registrations(beta);
        const small_vector<const fixtures::panel_desc *, 8> panels = extensions_of<fixtures::panel_desc>(set);
        REQUIRE(panels.size() == 1);
        CHECK(std::string_view(panels[0]->title) == "Beta Panel");
        CHECK(set.managedAssemblies == std::vector<std::string>{"managed/Beta.dll", "managed/BetaExtra.dll"});
        REQUIRE(registry.dependents(fixture.id("module_alpha")).size() == 1);
        CHECK(registry.dependents(fixture.id("module_alpha"))[0] == beta);
    }

    TEST_CASE("module_manager - a subscription alpha retains fires while it is loaded and never after unload") {
        // A second handle keeps the fixture mapped across the unload so its counter stays readable;
        // the callback itself must still be gone, because the context released it before close.
        result<dynamic_library> holder = dynamic_library::load(fixture_binary("module_alpha"));
        REQUIRE(holder.has_value());
        const count_fn pingTotal = holder->symbol_as<count_fn>("conjure_alpha_ping_total");
        REQUIRE(pingTotal != nullptr);

        discovered_host fixture = {};
        fixtures::host_event_service events = {};
        events.bus = &fixture.host.events;
        fixture.host.manager.provide_service(events);
        const module_id alpha = fixture.id("module_alpha");
        const std::uint32_t before = pingTotal();

        CONJURE_REQUIRE_OK(fixture.host.manager.load(alpha));
        CHECK(fixture.host.events.subscriber_count<fixtures::host_ping>() == 1);
        fixture.host.events.publish(fixtures::host_ping{5});
        CHECK(pingTotal() == before + 5);

        CONJURE_REQUIRE_OK(fixture.host.manager.unload(alpha));
        CHECK(fixture.host.events.subscriber_count<fixtures::host_ping>() == 0);
        fixture.host.events.publish(fixtures::host_ping{7});
        CHECK(pingTotal() == before + 5);

        // Loaded again, the fresh context subscribes anew.
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.id("module_alpha")));
        fixture.host.events.publish(fixtures::host_ping{1});
        CHECK(pingTotal() == before + 6);
    }

    TEST_CASE("module_manager - refused binaries never run their entry") {
        discovered_host fixture = {};
        fixture.logs.allow_errors();

        const result<void> badAbi = fixture.host.manager.load(fixture.id("module_bad_abi"));
        CONJURE_CHECK_ERROR(badAbi, module_error::ABI_MISMATCH);
        CHECK(badAbi.get_error().message().find("'abiVersion'") != std::string::npos);
        CHECK(badAbi.get_error().message().find("999") != std::string::npos);
        CHECK(fixture.state("module_bad_abi") == module_state::FAILED);
        CHECK(entry_count_of("module_bad_abi") == 0);

        const result<void> badVersion = fixture.host.manager.load(fixture.id("module_bad_version"));
        CONJURE_CHECK_ERROR(badVersion, module_error::ABI_MISMATCH);
        CHECK(badVersion.get_error().message().find("'engineVersionMajor'") != std::string::npos);
        CHECK(badVersion.get_error().message().find("9000") != std::string::npos);
        CHECK(entry_count_of("module_bad_version") == 0);

        const result<void> wrongName = fixture.host.manager.load(fixture.id("module_wrong_name"));
        CONJURE_CHECK_ERROR(wrongName, module_error::NAME_MISMATCH);
        CHECK(wrongName.get_error().message().find("module_wrong_name_actual") != std::string::npos);
        CHECK(entry_count_of("module_wrong_name") == 0);

        const result<void> noSymbol = fixture.host.manager.load(fixture.id("module_no_symbol"));
        CONJURE_CHECK_ERROR(noSymbol, module_error::ENTRY_SYMBOL_MISSING);
        CHECK(noSymbol.get_error().message().find("conjure_module_describe") != std::string::npos);

        CHECK(fixture.log.empty());
        CHECK_FALSE(fixture.host.manager.any_loaded());
        CHECK(fixture.logs.count(log_level::ERR) == 4);
    }

    TEST_CASE("module_manager - a failing entry reports both reasons and applies nothing") {
        discovered_host fixture = {};
        fixture.logs.allow_errors();
        const result<void> failed = fixture.host.manager.load(fixture.id("module_bad_entry"));
        CONJURE_CHECK_ERROR(failed, module_error::ENTRY_FAILED);
        CHECK(failed.get_error().message().find("first reason") != std::string::npos);
        CHECK(failed.get_error().message().find("second reason") != std::string::npos);
        CHECK(fixture.state("module_bad_entry") == module_state::FAILED);
        CHECK(fixture.log.empty());
        CHECK(fixture.host.manager.registry().registrations(fixture.id("module_bad_entry")).extensions.empty());
    }

    TEST_CASE("module_manager - a managed-only module loads without a binary and lists its assembly") {
        discovered_host fixture = {};
        const module_id managed = fixture.id("module_managed_only");
        CONJURE_REQUIRE_OK(fixture.host.manager.load(managed));
        CHECK(fixture.state("module_managed_only") == module_state::LOADED);
        CHECK(fixture.host.manager.registry().registrations(managed).managedAssemblies
          == std::vector<std::string>{"managed/ManagedOnly.dll"});
        CHECK(fixture.host.manager.registry().registrations(managed).extensions.empty());
        CHECK(fixture.log
          == std::vector<std::string>{"validate:ecs:module_managed_only", "attach:ecs:module_managed_only"});
    }

    TEST_CASE("module_manager - load_all continues past the broken fixtures and reports the cycle") {
        discovered_host fixture = {};
        fixture.logs.allow_errors();
        const result<void> all = fixture.host.manager.load_all();
        REQUIRE_FALSE(all.has_value());

        CHECK(fixture.state("module_alpha") == module_state::LOADED);
        CHECK(fixture.state("module_beta") == module_state::LOADED);
        CHECK(fixture.state("module_managed_only") == module_state::LOADED);
        CHECK(fixture.state("module_wrong_platform") == module_state::SKIPPED);
        CHECK(fixture.state("module_bad_abi") == module_state::FAILED);
        CHECK(fixture.state("module_bad_version") == module_state::FAILED);
        CHECK(fixture.state("module_wrong_name") == module_state::FAILED);
        CHECK(fixture.state("module_no_symbol") == module_state::FAILED);
        CHECK(fixture.state("module_bad_entry") == module_state::FAILED);
        CHECK(fixture.state("module_cycle_a") == module_state::FAILED);
        CHECK(fixture.state("module_cycle_b") == module_state::FAILED);
        const module_registry &registry = fixture.host.manager.registry();
        const bool cycleReported =
          registry.last_error(fixture.id("module_cycle_a")).is(module_error::DEPENDENCY_CYCLE)
          || registry.last_error(fixture.id("module_cycle_b")).is(module_error::DEPENDENCY_CYCLE);
        CHECK(cycleReported);
        CHECK(registry.load_order().size() == 3);
        CHECK(registry.load_order()[0] == fixture.id("module_alpha"));
    }

    TEST_CASE("module_manager - alpha cannot unload while beta is loaded") {
        discovered_host fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.id("module_beta")));
        const result<void> refused = fixture.host.manager.unload(fixture.id("module_alpha"));
        CONJURE_CHECK_ERROR(refused, module_error::HAS_DEPENDENTS);
        CHECK(refused.get_error().message().find("module_beta") != std::string::npos);
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.id("module_beta")));
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.id("module_alpha")));
        CHECK_FALSE(fixture.host.manager.any_loaded());
    }

    TEST_CASE("module_manager - loading and unloading alpha 100 times grows nothing") {
        discovered_host fixture = {};
        const module_registry &registry = fixture.host.manager.registry();
        const usize recordCount = registry.size();
        for (u32 iteration = 0; iteration < 100; ++iteration) {
            CAPTURE(iteration);
            const module_id alpha = fixture.id("module_alpha");
            CHECK(alpha.generation == iteration);
            REQUIRE(fixture.host.manager.load(alpha).has_value());
            REQUIRE(fixture.host.manager.unload(alpha).has_value());
        }
        CHECK(registry.size() == recordCount);
        CHECK(registry.all().size() == recordCount);
        CHECK(registry.load_count(fixture.id("module_alpha")) == 100);
        CHECK(fixture.id("module_alpha").generation == 100);
        CHECK(fixture.log.size() == 400);
    }

    TEST_CASE("module_manager - a missing or unloadable binary is reported with its full path") {
        shared_host host = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        const conjure::testing::temp_directory directory("modules_shared_missing_binary");
        CONJURE_REQUIRE_OK(directory.write_text(
          "ghost/module.json",
          R"({"schema": 1, "name": "ghost", "version": "1.0.0", "engine": "*", "binary": "ghost"})"
        ));
        CONJURE_REQUIRE_OK(directory.write_text(
          "bogus/module.json",
          R"({"schema": 1, "name": "bogus", "version": "1.0.0", "engine": "*", "binary": "bogus"})"
        ));
        const module_id ghost = host.manager.add_manifest(directory.file("ghost/module.json")).value();
        const module_id bogus = host.manager.add_manifest(directory.file("bogus/module.json")).value();
        const std::filesystem::path bogusBinary = host.manager.registry().binary_path(bogus);
        CONJURE_REQUIRE_OK(directory.write_text(
          std::filesystem::relative(bogusBinary, directory.path()).generic_string(), "this is not a shared library"));

        const result<void> missing = host.manager.load(ghost);
        CONJURE_CHECK_ERROR(missing, module_error::BINARY_NOT_FOUND);
        const std::string ghostBinary = host.manager.registry().binary_path(ghost).generic_string();
        CHECK(missing.get_error().message().find(ghostBinary) != std::string::npos);

        const result<void> broken = host.manager.load(bogus);
        CONJURE_CHECK_ERROR(broken, module_error::BINARY_LOAD_FAILED);
        CHECK(broken.get_error().message().find("bogus") != std::string::npos);
        CHECK(broken.get_error().message().size() > bogusBinary.generic_string().size() + 20);
    }

    TEST_CASE("module_manager - shared libraries can be disabled per host") {
        module_manager_desc desc = shared_desc();
        desc.allowSharedLibraries = false;
        shared_host host(desc);
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        const module_id alpha = host.manager.add_manifest(fixture_manifest("module_alpha")).value();
        const module_id managed = host.manager.add_manifest(fixture_manifest("module_managed_only")).value();

        CONJURE_CHECK_ERROR(host.manager.load(alpha), module_error::SHARED_LIBRARIES_DISABLED);
        CONJURE_REQUIRE_OK(host.manager.load(managed));
    }

    TEST_CASE("module_manager - events accompany shared loads and unloads") {
        discovered_host fixture = {};
        std::vector<std::string> events = {};
        const event_subscription onLoaded =
          fixture.host.events.subscribe<module_loaded_event>([&](const module_loaded_event &event) {
              events.push_back("loaded:" + fixture.host.manager.registry().manifest(event.id).name);
          });
        const event_subscription onUnloaded =
          fixture.host.events.subscribe<module_unloaded_event>([&](const module_unloaded_event &event) {
              events.push_back("unloaded:" + std::string(event.name.view()));
          });
        const event_subscription onFailed =
          fixture.host.events.subscribe<module_load_failed_event>([&](const module_load_failed_event &event) {
              events.push_back("failed:" + std::string(event.error.code_name()));
          });

        fixture.logs.allow_errors();
        CONJURE_REQUIRE_OK(fixture.host.manager.load(fixture.id("module_alpha")));
        CHECK_FALSE(fixture.host.manager.load(fixture.id("module_bad_abi")).has_value());
        CONJURE_REQUIRE_OK(fixture.host.manager.unload(fixture.id("module_alpha")));
        CHECK(events
          == std::vector<std::string>{"loaded:module_alpha", "failed:ABI_MISMATCH", "unloaded:module_alpha"});
    }
}
