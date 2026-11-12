#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/core/dynamic_library.h"
#include "conjure/core/json/document.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"
#include "engine/export_commands.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// export.h (docs/design/06_engine_loop.md 3.6, R21): the six steps over a temp project with null backends,
// then the exported folder booted headless as a cooked project.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] application_desc project_desc(project loaded, u64 maxFrames) {
        engine_config config = engine_test::inline_config();
        config.projectPath = loaded.root_directory();
        config.maxFrames = maxFrames;
        loaded.apply_to(config);
        application_desc desc = engine_test::make_desc(config);
        desc.project = std::move(loaded);
        return desc;
    }
}

TEST_SUITE("engine/export") {

    TEST_CASE("export_pipeline - refuses without a project and without an output directory") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = "somewhere";
        CONJURE_CHECK_ERROR(pipeline.run(settings), core::error_code::INVALID_STATE);
        app.shutdown();
        CHECK(export_pipeline::STEP_NAMES.size() == 6);
        CHECK(export_pipeline::STEP_NAMES[1] == "cook_assets");
        CHECK(export_pipeline::STEP_NAMES[5] == "write_project");
    }

    TEST_CASE("export_pipeline - exports a development project and the result boots as a cooked project") {
        log_capture logs = {};
        temp_directory directory("engine_export");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "exported");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bin", "gold:10;potion:2"));
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/more.bin", "a second blob"));

        export_report report = {};
        {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            REQUIRE(app.context().assetDatabase != nullptr);
            CHECK(app.context().assetDatabase->record_count() >= 2);

            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            settings.copyRuntime = false;
            settings.alwaysInclude = {"assets://data/more.bin", "assets://data/absent.bin"};
            std::vector<std::string> announced = {};
            const progress_fn progress = [&announced](std::string_view step, f32 fraction) {
                announced.emplace_back(step);
                CHECK(fraction >= 0.0f);
                CHECK(fraction <= 1.0f);
            };
            const core::result<export_report> ran = pipeline.run(settings, progress);
            CONJURE_REQUIRE_OK(ran);
            report = ran.value();
            CHECK(announced.size() == 7);
            CHECK(announced.front() == "build_game_module");
            CHECK(announced.back() == "done");
            app.shutdown();
        }

        CHECK(report.steps.size() == 6);
        CHECK(report.assetCount >= 2);
        CHECK(report.packBytes > 0);
        CHECK_FALSE(report.nativeModuleBuilt);
        CHECK_FALSE(report.managedPublished);
        CHECK(report.modulesExported == 0);
        const std::vector<std::string> &copied = report.copiedFiles;
        CHECK(std::find(copied.begin(), copied.end(), "cooked/content.cpak") != copied.end());
        CHECK(std::find(copied.begin(), copied.end(), "project.json") != copied.end());
        CHECK(std::any_of(report.warnings.begin(), report.warnings.end(), [](const std::string &warning) {
            return warning.find("absent.bin") != std::string::npos;
        }));
        CHECK(std::filesystem::is_regular_file(outputDir / "cooked" / "content.cpak"));
        CHECK(std::filesystem::is_regular_file(outputDir / "cooked" / "manifest.json"));
        CHECK(std::filesystem::is_regular_file(outputDir / "project.json"));

        // The exported folder is a cooked project: it mounts the pack and finds the assets by path.
        core::result<project> exported = project::load(outputDir);
        CONJURE_REQUIRE_OK(exported);
        CHECK(exported.value().description().cooked);
        CHECK(exported.value().description().cookedRoot.generic_string() == "cooked");
        CHECK(exported.value().description().name == "exported");

        application cooked(project_desc(std::move(exported).value(), 60));
        CONJURE_REQUIRE_OK(cooked.initialise());
        CHECK(cooked.context().assetDatabase == nullptr);
        REQUIRE(cooked.context().assets != nullptr);
        CHECK(cooked.context().assets->find("assets://data/notes.bin").has_value());
        CHECK(cooked.context().assets->find("assets://data/more.bin").has_value());
        CHECK(cooked.context().filesystem->exists("cooked://content.cpak"));
        CHECK(cooked.run() == 0);
        CHECK(cooked.time().frameIndex == 60);
    }

    TEST_CASE("export_pipeline - refuses the project directory and a folder inside the assets root as output") {
        log_capture logs = {};
        temp_directory directory("engine_export_into_project");
        const std::filesystem::path projectDir = directory.path() / "project";
        project loaded = engine_test::write_project(projectDir, "inside");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bin", "x"));
        application app(project_desc(loaded, 0));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.buildNativeModule = false;
        settings.publishManaged = false;
        settings.copyRuntime = false;

        settings.outputDirectory = projectDir;
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        settings.outputDirectory = projectDir / "";
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        settings.outputDirectory = projectDir / "assets";
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        settings.outputDirectory = projectDir / "assets" / "out";
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        CHECK_FALSE(std::filesystem::exists(projectDir / "assets" / "out"));

        // The source project.json is still the development one.
        const core::result<project> reloaded = project::load(projectDir);
        CONJURE_REQUIRE_OK(reloaded);
        CHECK_FALSE(reloaded.value().description().cooked);

        // A folder of its own inside the project is fine.
        settings.outputDirectory = projectDir / "export";
        CONJURE_CHECK_OK(pipeline.run(settings));
        CHECK(std::filesystem::is_regular_file(projectDir / "export" / "project.json"));
        app.shutdown();
    }

    TEST_CASE("export_pipeline - copy_runtime ships the shader indexes and blobs but not the build depfiles") {
        log_capture logs = {};
        temp_directory directory("engine_export_shaders");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "shaded");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bin", "x"));

        // A distribution folder as the Distribution build lays it out, with a module's shader set in a subfolder.
        const std::string runtimeName = CONJURE_PLATFORM_WINDOWS ? "conjure_runtime.exe" : "conjure_runtime";
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + runtimeName, ""));
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + core::dynamic_library::decorate_name("conjure_shared"), ""));
        const std::array<std::string_view, 7> shaderFiles = {
            "index.json",
            "a.vs_main.dxil",
            "a.vs_main.dxil.d",
            "a.vs_main.spv.d",
            "particles/index.json",
            "particles/p.ps_main.spv",
            "particles/p.ps_main.spv.d",
        };
        for (const std::string_view file : shaderFiles) {
            CONJURE_REQUIRE_OK(directory.write_text("dist/shaders/" + std::string(file), "blob"));
        }

        export_report report = {};
        {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.distributionDirectory = directory.path() / "dist";
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            settings.copyRuntime = true;
            settings.copyShaders = true;
            const core::result<export_report> ran = pipeline.run(settings);
            CONJURE_REQUIRE_OK(ran);
            report = ran.value();
            app.shutdown();
        }

        const std::filesystem::path shaders = outputDir / "shaders";
        CHECK(std::filesystem::is_regular_file(outputDir / runtimeName));
        CHECK(std::filesystem::is_regular_file(shaders / "index.json"));
        CHECK(std::filesystem::is_regular_file(shaders / "a.vs_main.dxil"));
        CHECK(std::filesystem::is_regular_file(shaders / "particles" / "index.json"));
        CHECK(std::filesystem::is_regular_file(shaders / "particles" / "p.ps_main.spv"));
        CHECK_FALSE(std::filesystem::exists(shaders / "a.vs_main.dxil.d"));
        CHECK_FALSE(std::filesystem::exists(shaders / "a.vs_main.spv.d"));
        CHECK_FALSE(std::filesystem::exists(shaders / "particles" / "p.ps_main.spv.d"));
        const std::vector<std::string> &copied = report.copiedFiles;
        CHECK(std::find(copied.begin(), copied.end(), "shaders") != copied.end());
        CHECK(std::find(copied.begin(), copied.end(), runtimeName) != copied.end());
    }

    TEST_CASE("export_pipeline - managed commands strip symbols and the apphost unless asked to keep them") {
        const auto has = [](const std::vector<std::string> &arguments, std::string_view argument) {
            return std::find(arguments.begin(), arguments.end(), argument) != arguments.end();
        };
        const auto hasPrefix = [](const std::vector<std::string> &arguments, std::string_view prefix) {
            return std::any_of(arguments.begin(), arguments.end(), [prefix](const std::string &argument) {
                return argument.starts_with(prefix);
            });
        };
        const std::filesystem::path managed = std::filesystem::path("C:/out") / "managed";
        export_settings settings = {};
        settings.bootstrapProject = "Boot.csproj";
        settings.scriptsProject = "Scripts/Game.csproj";

        // The defaults: framework-dependent, no debug symbols.
        const scripting_settings defaults = {};
        const std::vector<std::vector<std::string>> stripped = managed_export_commands(settings, defaults, managed);
        REQUIRE(stripped.size() == 2);
        CHECK(stripped[0].front() == "publish");
        CHECK(has(stripped[0], "Boot.csproj"));
        CHECK(has(stripped[0], managed.string()));
        CHECK(has(stripped[0], "-p:DebugType=none"));
        CHECK(has(stripped[0], "-p:DebugSymbols=false"));
        CHECK(has(stripped[0], "-p:UseAppHost=false"));
        CHECK(stripped[1].front() == "build");
        CHECK(has(stripped[1], "-p:DebugType=none"));
        CHECK_FALSE(hasPrefix(stripped[1], "-p:UseAppHost"));

        scripting_settings symbols = {};
        symbols.exportDebugSymbols = true;
        const std::vector<std::vector<std::string>> kept = managed_export_commands(settings, symbols, managed);
        REQUIRE(kept.size() == 2);
        CHECK_FALSE(hasPrefix(kept[0], "-p:DebugType"));
        CHECK_FALSE(hasPrefix(kept[1], "-p:DebugType"));

        // A self-contained publish needs its apphost (NETSDK1067).
        scripting_settings selfContained = {};
        selfContained.exportMode = script_export_mode::SELF_CONTAINED;
        const std::vector<std::vector<std::string>> standalone =
          managed_export_commands(settings, selfContained, managed);
        REQUIRE_FALSE(standalone.empty());
        CHECK_FALSE(hasPrefix(standalone[0], "-p:UseAppHost"));
        const auto selfContainedFlag = std::find(standalone[0].begin(), standalone[0].end(), "--self-contained");
        REQUIRE(selfContainedFlag != standalone[0].end());
        REQUIRE(selfContainedFlag + 1 != standalone[0].end());
        CHECK(*(selfContainedFlag + 1) == "true");

        // Without game scripts only the bootstrap is published.
        settings.scriptsProject.clear();
        CHECK(managed_export_commands(settings, defaults, managed).size() == 1);
    }

    TEST_CASE("export_pipeline - a cooked project cannot be exported again") {
        log_capture logs = {};
        temp_directory directory("engine_export_twice");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "once");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bin", "x"));
        {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            settings.copyRuntime = false;
            CONJURE_REQUIRE_OK(pipeline.run(settings));
            app.shutdown();
        }
        core::result<project> exported = project::load(outputDir);
        CONJURE_REQUIRE_OK(exported);
        application cooked(project_desc(std::move(exported).value(), 0));
        CONJURE_REQUIRE_OK(cooked.initialise());
        export_pipeline again(cooked.context());
        export_settings settings = {};
        settings.outputDirectory = directory.path() / "out2";
        CONJURE_CHECK_ERROR(again.run(settings), core::error_code::INVALID_STATE);
        cooked.shutdown();
    }
}
