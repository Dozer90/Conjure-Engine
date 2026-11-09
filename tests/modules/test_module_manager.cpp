#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "fake_consumer.h"
#include "static_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/string_id.h"

#include <filesystem>
#include <string>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

TEST_SUITE("modules/module_manager") {

    TEST_CASE("module_manager_desc - defaults describe a runtime host of this build") {
        const module_manager_desc desc = {};
        CHECK(desc.host == module_host_kind::RUNTIME);
        CHECK(desc.engineVersion == host_engine_version());
        CHECK(desc.engineFeatures == host_feature_flags());
        CHECK(desc.binaryConfig == host_build_config());
        CHECK(desc.allowSharedLibraries);
        CHECK_FALSE(desc.shadowCopyBinaries);
        CHECK_FALSE(desc.keepBinariesResident);
        CHECK(desc.shadowCopyDirectory.empty());
        CHECK(desc.defaultUnloadPolicy == unload_policy::REFUSE_IF_IN_USE);
        CHECK(desc.isInsideFrame == nullptr);
    }

    TEST_CASE("module_manager - an empty manager has nothing registered, loaded or pending") {
        const static_host host = {};
        CHECK(host.manager.registry().size() == 0);
        CHECK(host.manager.registry().all().empty());
        CHECK_FALSE(host.manager.any_loaded());
        CHECK(host.manager.pending_count() == 0);
        CHECK(host.manager.services().size() == 0);
        CHECK(host.manager.desc().host == module_host_kind::TEST);
        CHECK_FALSE(host.manager.desc().allowSharedLibraries);
    }

    TEST_CASE("module_manager - add_static_module registers a manifest and rejects duplicates and bad manifests") {
        static_host host = {};
        static_module_source alpha("alpha", manifest_json("alpha"), nullptr);
        static_module_source alphaAgain("alpha_again", manifest_json("alpha", "2.0.0"), nullptr);
        static_module_source broken("broken", "{ nope", nullptr);
        static_module_source badName("bad_name", manifest_json("Bad Name"), nullptr);

        const result<module_id> added = host.manager.add_static_module(alpha.desc());
        CONJURE_REQUIRE_OK(added);
        CHECK(host.manager.registry().state(added.value()) == module_state::DISCOVERED);
        CHECK(host.manager.registry().manifest(added.value()).name == "alpha");
        CHECK(host.manager.registry().root_directory(added.value()).empty());

        CONJURE_CHECK_ERROR(host.manager.add_static_module(alphaAgain.desc()), module_error::DUPLICATE_NAME);
        const result<module_id> invalid = host.manager.add_static_module(broken.desc());
        CONJURE_CHECK_ERROR(invalid, module_error::MANIFEST_INVALID);
        CHECK(invalid.get_error().message().find("<static:broken>") != std::string::npos);
        CONJURE_CHECK_ERROR(host.manager.add_static_module(badName.desc()), module_error::INVALID_NAME);
        CHECK(host.manager.registry().size() == 1);
    }

    TEST_CASE("module_manager - add_manifest reads module.json and makes its directory the module root") {
        static_host host = {};
        const conjure::testing::temp_directory directory("modules_manager_manifest");
        CONJURE_REQUIRE_OK(directory.write_text("plugin/module.json", manifest_json("plugin.one")));

        const result<module_id> added = host.manager.add_manifest(directory.file("plugin/module.json"));
        CONJURE_REQUIRE_OK(added);
        CHECK(host.manager.registry().root_directory(added.value())
          == std::filesystem::absolute(directory.file("plugin")));
        CHECK(host.manager.registry().binary_path(added.value()).empty());

        const std::filesystem::path samePlugin = directory.file("plugin/module.json");
        const std::filesystem::path missingPlugin = directory.file("missing/module.json");
        CONJURE_CHECK_ERROR(host.manager.add_manifest(samePlugin), module_error::DUPLICATE_NAME);
        CONJURE_CHECK_ERROR(host.manager.add_manifest(missingPlugin), module_error::MANIFEST_NOT_FOUND);
    }

    TEST_CASE("module_manager - add_manifest derives the binary path for the configured triplet") {
        module_manager_desc desc = static_host::make_desc();
        desc.binaryConfig = build_config::DISTRIBUTION;
        static_host host(desc);
        const conjure::testing::temp_directory directory("modules_manager_binary");
        const std::string nativeManifest = manifest_json("native", "1.0.0", {}, R"(, "binary": "native")");
        CONJURE_REQUIRE_OK(directory.write_text("native/module.json", nativeManifest));

        const result<module_id> added = host.manager.add_manifest(directory.file("native/module.json"));
        CONJURE_REQUIRE_OK(added);
        const std::filesystem::path expected = module_binary_path(
          host.manager.registry().manifest(added.value()),
          std::filesystem::absolute(directory.file("native")),
          host_binary_triplet(build_config::DISTRIBUTION)
        );
        CHECK(host.manager.registry().binary_path(added.value()) == expected);
        CHECK(expected.generic_string().find("-distribution/") != std::string::npos);
    }

    TEST_CASE("module_manager - discover adds every valid subdirectory in name order and reports the rest") {
        static_host host = {};
        conjure::testing::log_capture logs = {};
        const conjure::testing::temp_directory directory("modules_manager_discover");
        CONJURE_REQUIRE_OK(directory.write_text("zeta/module.json", manifest_json("zeta")));
        CONJURE_REQUIRE_OK(directory.write_text("alpha/module.json", manifest_json("alpha")));
        CONJURE_REQUIRE_OK(directory.write_text("broken/module.json", "{"));
        CONJURE_REQUIRE_OK(directory.write_text("not_a_module/readme.txt", "nothing here"));
        CONJURE_REQUIRE_OK(directory.write_text("loose_file.json", manifest_json("loose")));

        const result<u32> discovered = host.manager.discover(directory.path());
        CONJURE_CHECK_ERROR(discovered, module_error::MANIFEST_INVALID);
        CHECK(discovered.get_error().message().find("broken") != std::string::npos);
        const module_registry &registry = host.manager.registry();
        REQUIRE(registry.size() == 2);
        CHECK(registry.manifest(registry.all()[0]).name == "alpha");
        CHECK(registry.manifest(registry.all()[1]).name == "zeta");
        CHECK(logs.contains(log_level::INFO, "discovered 2 module(s)"));

        const conjure::testing::temp_directory clean("modules_manager_discover_clean");
        CONJURE_REQUIRE_OK(clean.write_text("one/module.json", manifest_json("one")));
        CONJURE_REQUIRE_OK(clean.write_text("two/module.json", manifest_json("two")));
        const result<u32> count = host.manager.discover(clean.path());
        CONJURE_REQUIRE_OK(count);
        CHECK(count.value() == 2);
        CHECK(registry.size() == 4);

        CONJURE_CHECK_ERROR(host.manager.discover(directory.file("does_not_exist")), module_error::MANIFEST_NOT_FOUND);
        const result<u32> again = host.manager.discover(clean.path());
        CONJURE_CHECK_ERROR(again, module_error::DUPLICATE_NAME);

        // A path that cannot be listed as a directory is an error, never "zero modules".
        const result<u32> notADirectory = host.manager.discover(directory.file("loose_file.json"));
        CONJURE_CHECK_ERROR(notADirectory, module_error::MANIFEST_NOT_FOUND);
        CHECK(notADirectory.get_error().message().find("loose_file.json") != std::string::npos);
        CHECK(registry.size() == 4);
    }

    TEST_CASE("module_manager - operations on invalid, stale and unloaded ids are refused by code") {
        static_module_source alpha("alpha", manifest_json("alpha"), nullptr);
        static_host host = {};
        const module_id id = host.manager.add_static_module(alpha.desc()).value();

        CONJURE_CHECK_ERROR(host.manager.load(module_id{}), module_error::INVALID_ID);
        CONJURE_CHECK_ERROR(host.manager.load(module_id{42, 0}), module_error::INVALID_ID);
        CONJURE_CHECK_ERROR(host.manager.unload(id), module_error::NOT_LOADED);
        CONJURE_CHECK_ERROR(host.manager.reload(id, reload_scope::FULL), module_error::NOT_LOADED);
        CONJURE_CHECK_ERROR(host.manager.unload(module_id{id.index, 3}), module_error::STALE_ID);

        CONJURE_REQUIRE_OK(host.manager.load(id));
        CONJURE_CHECK_ERROR(host.manager.load(id), module_error::ALREADY_LOADED);
        CONJURE_REQUIRE_OK(host.manager.unload(id));
        const result<void> stale = host.manager.unload(id);
        CONJURE_CHECK_ERROR(stale, module_error::STALE_ID);
        CHECK(stale.get_error().message().find("alpha") != std::string::npos);
    }

    TEST_CASE("module_manager - a manifest-only module loads with empty registrations") {
        fake_consumer consumer("ecs");
        static_module_source engineLibrary(
          "conjure_ecs", manifest_json("conjure.ecs", "0.1.0", {}, R"(, "kind": "engine")"), nullptr);
        static_host host = {};
        host.manager.add_consumer(consumer);
        const module_id id = host.manager.add_static_module(engineLibrary.desc()).value();

        CONJURE_REQUIRE_OK(host.manager.load(id));
        CHECK(host.manager.registry().state(id) == module_state::LOADED);
        CHECK(host.manager.registry().registrations(id).extensions.empty());
        CHECK(host.manager.registry().registrations(id).managedAssemblies.empty());
        CHECK(consumer.is_attached(id));
        CHECK(counters().entryRuns == 0);
        CHECK(host.manager.any_loaded());
    }

    TEST_CASE("module_manager - the destructor unloads everything in reverse order") {
        std::vector<std::string> log = {};
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_module_source beta("beta", manifest_json("beta", "1.0.0", {{"alpha"}}), REGISTERING_ENTRY);
        {
            fake_consumer consumer("ecs", &log);
            static_host host = {};
            host.manager.add_consumer(consumer);
            consumer.bind_registry(host.manager.registry());
            CONJURE_REQUIRE_OK(host.manager.add_static_module(alpha.desc()));
            CONJURE_REQUIRE_OK(host.manager.add_static_module(beta.desc()));
            CONJURE_REQUIRE_OK(host.manager.load_all());
            CHECK(counters().activated == 2);
            log.clear();
        }
        REQUIRE(log.size() == 2);
        CHECK(log[0] == "detach:ecs:beta");
        CHECK(log[1] == "detach:ecs:alpha");
        CHECK(counters().deactivating == 2);
    }

    TEST_CASE("module_manager - a module whose manifest excludes the host is SKIPPED and cannot load") {
        static_module_source elsewhere(
          "elsewhere", manifest_json("elsewhere", "1.0.0", {}, R"(, "platforms": [])"), nullptr);
        static_host host = {};
        const module_id id = host.manager.add_static_module(elsewhere.desc()).value();
        CHECK(host.manager.registry().state(id) == module_state::SKIPPED);
        CONJURE_CHECK_ERROR(host.manager.load(id), module_error::PLATFORM_UNSUPPORTED);
        CONJURE_REQUIRE_OK(host.manager.load_all());
        CHECK(host.manager.registry().state(id) == module_state::SKIPPED);
    }
}
