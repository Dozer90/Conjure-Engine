#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "shared_fixtures.h"
#include "static_fixtures.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/json/document.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    bool path_exists(const std::filesystem::path &path) {
        std::error_code ignored = {};
        return std::filesystem::exists(path, ignored);
    }

    struct export_fixture {
        conjure::testing::temp_directory project{"modules_export_project"};
        conjure::testing::temp_directory output{"modules_export_output"};
        static_module_source engine{
            "conjure_ecs", manifest_json("conjure.ecs", "0.1.0", {}, R"(, "kind": "engine")"), nullptr};
        shared_host host = {};

        export_fixture() {
            REQUIRE(project.write_text(
              "editor_tools/module.json",
              manifest_json("editor_tools", "1.0.0", {}, R"(, "editorOnly": true)")
            ).has_value());
            REQUIRE(host.manager.add_static_module(engine.desc()).has_value());
            REQUIRE(host.manager.add_manifest(fixture_manifest("module_alpha")).has_value());
            REQUIRE(host.manager.add_manifest(fixture_manifest("module_managed_only")).has_value());
            REQUIRE(host.manager.add_manifest(project.file("editor_tools/module.json")).has_value());
        }
    };
}

TEST_SUITE("modules/module_export_layout") {

    TEST_CASE("module_manager - export_layout writes the non-engine, non-editor modules with binaries and assemblies") {
        export_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load_all());
        REQUIRE(fixture.host.manager.registry().load_order().size() == 4);

        CONJURE_REQUIRE_OK(fixture.host.manager.export_layout(fixture.output.path(), host_build_config()));

        const std::filesystem::path modules = fixture.output.path() / "modules";
        CHECK(path_exists(modules / "module_alpha" / "module.json"));
        CHECK(path_exists(modules / "module_managed_only" / "module.json"));
        CHECK(path_exists(modules / "module_managed_only" / "managed" / "ManagedOnly.dll"));
        CHECK_FALSE(path_exists(modules / "conjure.ecs"));
        CHECK_FALSE(path_exists(modules / "editor_tools"));

        const std::filesystem::path alphaBinary = module_binary_path(
          fixture.host.manager.registry().manifest(fixture.host.manager.registry().find(string_id("module_alpha"))),
          modules / "module_alpha",
          host_binary_triplet()
        );
        CHECK(path_exists(alphaBinary));

        const result<json::document> written = json::document::parse_file(modules / "module_alpha" / "module.json");
        CONJURE_REQUIRE_OK(written);
        CHECK(written->root()["name"].as_string() == "module_alpha");
        CHECK_FALSE(written->root().contains("editorOnly"));
        const result<module_manifest> reparsed = load_module_manifest(modules / "module_alpha" / "module.json");
        CONJURE_REQUIRE_OK(reparsed);
        CHECK(reparsed->binary == "module_alpha");
        CHECK(reparsed->contentDirectories == std::vector<std::string>{"assets"});
    }

    TEST_CASE("module_manager - export_layout copies the assemblies the entry registered as well as the manifest's") {
        const conjure::testing::temp_directory project("modules_export_entry_assemblies");
        const conjure::testing::temp_directory output("modules_export_entry_assemblies_out");
        shared_host host = {};

        // A private copy of the beta fixture tree with both managed files present: the manifest
        // names Beta.dll, the entry adds BetaExtra.dll through module_registrar.
        const std::filesystem::path betaRoot = project.path() / "module_beta";
        std::error_code failure = {};
        std::filesystem::copy(
          fixture_directory("module_beta"), betaRoot, std::filesystem::copy_options::recursive, failure);
        REQUIRE_FALSE(static_cast<bool>(failure));
        CONJURE_REQUIRE_OK(project.write_text("module_beta/managed/Beta.dll", "not a real assembly"));
        CONJURE_REQUIRE_OK(project.write_text("module_beta/managed/BetaExtra.dll", "not a real assembly"));
        REQUIRE(host.manager.add_manifest(fixture_manifest("module_alpha")).has_value());
        const module_id beta = host.manager.add_manifest(betaRoot / MODULE_MANIFEST_FILE_NAME).value();
        CONJURE_REQUIRE_OK(host.manager.load(beta));
        REQUIRE(host.manager.registry().registrations(beta).managedAssemblies.size() == 2);

        CONJURE_REQUIRE_OK(host.manager.export_layout(output.path(), host_build_config()));

        const std::filesystem::path exported = output.path() / "modules" / "module_beta";
        CHECK(path_exists(exported / "managed" / "Beta.dll"));
        CHECK(path_exists(exported / "managed" / "BetaExtra.dll"));
        const result<module_manifest> reparsed = load_module_manifest(exported / MODULE_MANIFEST_FILE_NAME);
        CONJURE_REQUIRE_OK(reparsed);
        CHECK(reparsed->managedAssemblies == std::vector<std::string>{"managed/Beta.dll", "managed/BetaExtra.dll"});
        CHECK(reparsed->dependencies.size() == 1);

        // The registry's own manifest is untouched by the export.
        CHECK(host.manager.registry().manifest(beta).managedAssemblies == std::vector<std::string>{"managed/Beta.dll"});
    }

    TEST_CASE("module_manager - export_layout for a configuration without binaries is BINARY_NOT_FOUND") {
        export_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load_all());
        const bool hostIsDistribution = host_build_config() == build_config::DISTRIBUTION;
        const build_config other = hostIsDistribution ? build_config::DEBUG : build_config::DISTRIBUTION;

        const result<void> exported = fixture.host.manager.export_layout(fixture.output.path(), other);
        CONJURE_CHECK_ERROR(exported, module_error::BINARY_NOT_FOUND);
        CHECK(exported.get_error().message().find(std::string(to_string(other))) != std::string::npos);
        CHECK(exported.get_error().message().find("build the project") != std::string::npos);
    }

    TEST_CASE("module_manager - export_layout with nothing loaded writes nothing") {
        export_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.export_layout(fixture.output.path(), host_build_config()));
        CHECK_FALSE(path_exists(fixture.output.path() / "modules"));
    }

    TEST_CASE("module_manager - a missing managed assembly is EXPORT_FAILED") {
        shared_host host = {};
        const conjure::testing::temp_directory project("modules_export_missing_assembly");
        const conjure::testing::temp_directory output("modules_export_missing_assembly_out");
        CONJURE_REQUIRE_OK(project.write_text(
          "scripts/module.json", manifest_json("scripts", "1.0.0", {}, R"(, "managed": ["managed/Gone.dll"])")));
        const module_id id = host.manager.add_manifest(project.file("scripts/module.json")).value();
        CONJURE_REQUIRE_OK(host.manager.load(id));

        const result<void> exported = host.manager.export_layout(output.path(), host_build_config());
        CONJURE_CHECK_ERROR(exported, module_error::EXPORT_FAILED);
        CHECK(exported.get_error().message().find("Gone.dll") != std::string::npos);
        CHECK(path_exists(output.path() / "modules" / "scripts" / "module.json"));
    }
}
