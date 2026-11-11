#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/core/json/document.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"

#include <algorithm>
#include <filesystem>
#include <string>
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
