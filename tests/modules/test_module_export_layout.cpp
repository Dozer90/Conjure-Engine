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

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
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

    // Windows creates symbolic links only for administrators or in developer mode. A case that needs
    // one says why it stops instead of passing silently.
    bool cannot_link(const std::error_code &linkError) {
        if (!linkError) {
            return false;
        }
        MESSAGE("this account cannot create a symbolic link (" << linkError.message() << "): the case did not run");
        return true;
    }

    // Planted in module_alpha's binary folder, relative to it; export_layout ships none of them. The
    // stems differ, so the names stay distinct on a volume that ignores case.
    constexpr std::array<std::string_view, 10> BY_PRODUCTS = {
        "module_alpha.lib",
        "module_alpha.exp",
        "module_alpha.ilk",
        "module_alpha.pdb",
        "libalpha_static.a",
        "SHOUTED.PDB",
        "Mixed.Lib",
        "alpha.dSYM/Contents/DWARF/alpha",
        "plugins/extra.pdb",
        "plugins/deeper/extra_import.lib",
    };

    // Planted beside them; export_layout ships every one: dependent libraries, data files, names that
    // only contain a by-product extension, and subfolders at any depth.
    constexpr std::array<std::string_view, 10> COMPANIONS = {
        "companion.dll",
        "libcompanion.so.1",
        "libcompanion.dylib",
        "settings.json",
        "notes.pdb.txt",
        "library.archive",
        "README",
        "plugins/extra.bin",
        "plugins/deeper/table.dat",
        "fonts/ui.ttf",
    };

    /// <summary>
    /// module_alpha copied into a private project (at &lt;project&gt;/module_alpha unless the constructor names
    /// another place) with BY_PRODUCTS and COMPANIONS planted in its binary folder, registered and loaded. The
    /// host is declared after the folders, so it unloads the copied binary before they are removed.
    /// </summary>
    struct planted_export {
        // Short names: a Windows checkout in a deep folder stays below the 260-character path limit.
        conjure::testing::temp_directory project{"mx_plant"};
        conjure::testing::temp_directory output{"mx_out"};
        shared_host host = {};
        std::filesystem::path sourceBin = {};
        std::filesystem::path exportedRoot = {};
        std::filesystem::path exportedBin = {};
        module_id alpha = {};

        planted_export() :
            planted_export("module_alpha") {}

        // The copy is <project>/<relativeRoot>, which is also where the by-products and companions are planted.
        explicit planted_export(std::string_view relativeRoot) {
            const std::filesystem::path root = copy_fixture(project, "module_alpha", relativeRoot);
            sourceBin = module_binary_directory(root, host_binary_triplet());
            exportedRoot = output.path() / "modules" / "module_alpha";
            exportedBin = module_binary_directory(exportedRoot, host_binary_triplet());
            const std::filesystem::path binRelative =
              module_binary_directory(std::filesystem::path(relativeRoot), host_binary_triplet());
            for (const std::string_view name : BY_PRODUCTS) {
                const std::string relative = (binRelative / std::filesystem::path(name)).generic_string();
                CONJURE_REQUIRE_OK(project.write_text(relative, "a by-product"));
            }
            for (const std::string_view name : COMPANIONS) {
                const std::string relative = (binRelative / std::filesystem::path(name)).generic_string();
                CONJURE_REQUIRE_OK(project.write_text(relative, std::string("ships: ").append(name)));
            }
            alpha = host.manager.add_manifest(root / MODULE_MANIFEST_FILE_NAME).value();
            CONJURE_REQUIRE_OK(host.manager.load(alpha));
        }
    };

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
        const std::filesystem::path betaRoot = copy_fixture(project, "module_beta");
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

    TEST_CASE("regression - code sweep F339 export_layout leaves link and debug by-products out of the binary folder") {
        planted_export fixture = {};

        CONJURE_REQUIRE_OK(fixture.host.manager.export_layout(fixture.output.path(), host_build_config()));

        const module_manifest &manifest = fixture.host.manager.registry().manifest(fixture.alpha);
        CHECK(path_exists(module_binary_path(manifest, fixture.exportedRoot, host_binary_triplet())));
        for (const std::string_view name : BY_PRODUCTS) {
            const std::string planted(name);
            CAPTURE(planted);
            REQUIRE(path_exists(fixture.sourceBin / std::filesystem::path(name)));
            CHECK_FALSE(path_exists(fixture.exportedBin / std::filesystem::path(name)));
        }
        // The whole bundle stays behind, not only the files inside it.
        CHECK_FALSE(path_exists(fixture.exportedBin / "alpha.dSYM"));
    }

    TEST_CASE("module_manager - export_layout ships the dependent libraries, data files and subfolders of bin") {
        planted_export fixture = {};

        CONJURE_REQUIRE_OK(fixture.host.manager.export_layout(fixture.output.path(), host_build_config()));

        for (const std::string_view name : COMPANIONS) {
            const std::string planted(name);
            CAPTURE(planted);
            const std::filesystem::path exported = fixture.exportedBin / std::filesystem::path(name);
            REQUIRE(path_exists(exported));
            std::error_code sourceFailure = {};
            std::error_code exportedFailure = {};
            const std::uintmax_t sourceSize =
              std::filesystem::file_size(fixture.sourceBin / std::filesystem::path(name), sourceFailure);
            const std::uintmax_t exportedSize = std::filesystem::file_size(exported, exportedFailure);
            REQUIRE_FALSE(static_cast<bool>(sourceFailure));
            REQUIRE_FALSE(static_cast<bool>(exportedFailure));
            CHECK(exportedSize == sourceSize);
        }
    }

    TEST_CASE("module_manager - export_layout over an earlier export overwrites files and removes by-products") {
        planted_export fixture = {};
        const std::filesystem::path binRelative =
          module_binary_directory(std::filesystem::path("modules") / "module_alpha", host_binary_triplet());
        // What an export made before by-products were left out, or another tool, put in the output folder.
        constexpr std::array<std::string_view, 4> STALE_BY_PRODUCTS = {
            "stale.pdb",
            "old/stale.LIB",
            "old/deeper/stale.ilk",
            "stale.dSYM/Contents/DWARF/stale",
        };
        constexpr std::array<std::string_view, 3> STALE_KEPT = {
            "stale.dat",
            "old/keep.dat",
            "old/deeper/keep.pdb.txt",
        };
        for (const std::string_view name : STALE_BY_PRODUCTS) {
            CONJURE_REQUIRE_OK(fixture.output.write_text((binRelative / name).generic_string(), "an earlier export"));
        }
        for (const std::string_view name : STALE_KEPT) {
            CONJURE_REQUIRE_OK(fixture.output.write_text((binRelative / name).generic_string(), "an earlier export"));
        }
        const std::string readme = (binRelative / "README").generic_string();
        CONJURE_REQUIRE_OK(fixture.output.write_text(readme, "an older README that is longer than the new one"));

        CONJURE_REQUIRE_OK(fixture.host.manager.export_layout(fixture.output.path(), host_build_config()));

        std::error_code failure = {};
        const std::uintmax_t size = std::filesystem::file_size(fixture.exportedBin / "README", failure);
        REQUIRE_FALSE(static_cast<bool>(failure));
        CHECK(size == std::string_view("ships: README").size());
        for (const std::string_view name : STALE_BY_PRODUCTS) {
            const std::string stale(name);
            CAPTURE(stale);
            CHECK_FALSE(path_exists(fixture.exportedBin / std::filesystem::path(name)));
        }
        CHECK_FALSE(path_exists(fixture.exportedBin / "stale.dSYM"));
        // Everything else of the earlier export stays: an output folder that must hold only this export starts
        // empty.
        for (const std::string_view name : STALE_KEPT) {
            const std::string kept(name);
            CAPTURE(kept);
            CHECK(path_exists(fixture.exportedBin / std::filesystem::path(name)));
        }
    }

    TEST_CASE("module_manager - export_layout into a folder that holds the module keeps the module's own by-products") {
        // The module sits at <project>/modules/module_alpha, so exporting into <project> makes the output bin
        // folder the module's own bin folder.
        planted_export fixture("modules/module_alpha");

        // copy_file onto the same file is an error on every standard library, so the export fails; only what is
        // left of the module matters here.
        const result<void> exported = fixture.host.manager.export_layout(fixture.project.path(), host_build_config());
        CONJURE_CHECK_ERROR(exported, module_error::EXPORT_FAILED);

        for (const std::string_view name : BY_PRODUCTS) {
            const std::string planted(name);
            CAPTURE(planted);
            CHECK(path_exists(fixture.sourceBin / std::filesystem::path(name)));
        }
    }

    TEST_CASE("module_manager - export_layout is EXPORT_FAILED naming a file it cannot copy") {
        planted_export fixture = {};
        const std::filesystem::path binRelative =
          module_binary_directory(std::filesystem::path("modules") / "module_alpha", host_binary_triplet());
        // A folder where settings.json goes: copy_file refuses a folder as its destination on every OS.
        const std::string occupant = (binRelative / "settings.json" / "occupant").generic_string();
        CONJURE_REQUIRE_OK(fixture.output.write_text(occupant, "in the way"));

        const result<void> exported = fixture.host.manager.export_layout(fixture.output.path(), host_build_config());
        CONJURE_CHECK_ERROR(exported, module_error::EXPORT_FAILED);
        CHECK(exported.get_error().message().find("module 'module_alpha'") != std::string::npos);
        CHECK(exported.get_error().message().find("cannot copy") != std::string::npos);
        CHECK(exported.get_error().message().find("settings.json") != std::string::npos);
    }

    TEST_CASE("module_manager - export_layout is EXPORT_FAILED when the binary folder cannot be created") {
        export_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.host.manager.load_all());
        // A file where the export puts the bin folder.
        CONJURE_REQUIRE_OK(fixture.output.write_text("modules/module_alpha/bin", "in the way"));

        const result<void> exported = fixture.host.manager.export_layout(fixture.output.path(), host_build_config());
        CONJURE_CHECK_ERROR(exported, module_error::EXPORT_FAILED);
        CHECK(exported.get_error().message().find("module 'module_alpha'") != std::string::npos);
        CHECK(exported.get_error().message().find("cannot create") != std::string::npos);
    }

    TEST_CASE("module_manager - export_layout is EXPORT_FAILED when a managed assembly's folder cannot be created") {
        const conjure::testing::temp_directory project("modules_export_managed_folder");
        const conjure::testing::temp_directory output("modules_export_managed_folder_out");
        shared_host host = {};
        const std::filesystem::path betaRoot = copy_fixture(project, "module_beta");
        CONJURE_REQUIRE_OK(project.write_text("module_beta/managed/Beta.dll", "not a real assembly"));
        CONJURE_REQUIRE_OK(project.write_text("module_beta/managed/BetaExtra.dll", "not a real assembly"));
        REQUIRE(host.manager.add_manifest(fixture_manifest("module_alpha")).has_value());
        const module_id beta = host.manager.add_manifest(betaRoot / MODULE_MANIFEST_FILE_NAME).value();
        CONJURE_REQUIRE_OK(host.manager.load(beta));
        // A file where the export puts the managed folder.
        CONJURE_REQUIRE_OK(output.write_text("modules/module_beta/managed", "in the way"));

        const result<void> exported = host.manager.export_layout(output.path(), host_build_config());
        CONJURE_CHECK_ERROR(exported, module_error::EXPORT_FAILED);
        CHECK(exported.get_error().message().find("module 'module_beta'") != std::string::npos);
        CHECK(exported.get_error().message().find("cannot create") != std::string::npos);
        CHECK(exported.get_error().message().find("module_beta/managed'") != std::string::npos);
    }

    TEST_CASE("module_manager - export_layout ships a linked file or folder of the binary folder as a copy") {
        planted_export fixture = {};
        CONJURE_REQUIRE_OK(fixture.project.write_text("outside/linked.dll", "a linked library"));
        CONJURE_REQUIRE_OK(fixture.project.write_text("outside/folder/inside.bin", "a file in a linked folder"));
        CONJURE_REQUIRE_OK(fixture.project.write_text("outside/folder/inside.pdb", "symbols in a linked folder"));
        std::error_code fileLinkError = {};
        std::error_code folderLinkError = {};
        std::filesystem::create_symlink(
          fixture.project.file("outside/linked.dll"), fixture.sourceBin / "linked.dll", fileLinkError);
        std::filesystem::create_directory_symlink(
          fixture.project.file("outside/folder"), fixture.sourceBin / "linked_folder", folderLinkError);
        if (cannot_link(fileLinkError) || cannot_link(folderLinkError)) {
            return;
        }

        CONJURE_REQUIRE_OK(fixture.host.manager.export_layout(fixture.output.path(), host_build_config()));

        std::error_code ignored = {};
        const std::filesystem::path linkedFile = fixture.exportedBin / "linked.dll";
        const std::filesystem::path linkedFolder = fixture.exportedBin / "linked_folder";
        CHECK(std::filesystem::is_regular_file(std::filesystem::symlink_status(linkedFile, ignored)));
        CHECK(std::filesystem::is_directory(std::filesystem::symlink_status(linkedFolder, ignored)));
        CHECK(path_exists(linkedFolder / "inside.bin"));
        CHECK_FALSE(path_exists(linkedFolder / "inside.pdb"));
    }

    TEST_CASE("module_manager - export_layout is EXPORT_FAILED for a broken link in the binary folder") {
        planted_export fixture = {};
        std::error_code linkError = {};
        std::filesystem::create_symlink(
          fixture.project.file("missing.dll"), fixture.sourceBin / "broken.dll", linkError);
        if (cannot_link(linkError)) {
            return;
        }

        const result<void> exported = fixture.host.manager.export_layout(fixture.output.path(), host_build_config());
        CONJURE_CHECK_ERROR(exported, module_error::EXPORT_FAILED);
        CHECK(exported.get_error().message().find("broken.dll") != std::string::npos);
    }

    TEST_CASE("module_manager - export_layout is EXPORT_FAILED for a link back to a folder it is copying") {
        planted_export fixture = {};
        std::error_code linkError = {};
        std::filesystem::create_directory_symlink(fixture.sourceBin, fixture.sourceBin / "plugins" / "loop", linkError);
        if (cannot_link(linkError)) {
            return;
        }

        const result<void> exported = fixture.host.manager.export_layout(fixture.output.path(), host_build_config());
        CONJURE_CHECK_ERROR(exported, module_error::EXPORT_FAILED);
        CHECK(exported.get_error().message().find("loop") != std::string::npos);
        CHECK(exported.get_error().message().find("links back") != std::string::npos);
    }
}
