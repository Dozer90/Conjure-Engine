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
#include "conjure/modules/module_abi.h"
#include "engine/export_commands.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

// export.h (docs/design/06_engine_loop.md 3.6, R21): the six steps over a temp project with null backends,
// then the exported folder booted headless as a cooked project. Step 5 runs over a stand-in distribution folder
// of fake files, the Visual C++ runtime DLLs of a Windows Distribution build (code sweep F790), the debug runtime
// that must stay behind, and the licence notices a build writes next to the runtime (06 D15, code sweep F200)
// among them.

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

    // What export_report::warnings says when the distribution directory of an MSVC runtime holds no Visual C++
    // runtime DLL.
    constexpr std::string_view REDISTRIBUTABLE_WARNING = "Microsoft Visual C++ Redistributable";
    // What it says when the distribution directory holds libraries of the debug runtime.
    constexpr std::string_view DEBUG_RUNTIME_WARNING = "debug Visual C++ runtime";
    // What it says when the distribution directory holds no licence notices.
    constexpr std::string_view NOTICES_WARNING = "without their licence notices";

    [[nodiscard]] std::string runtime_name() {
        return CONJURE_PLATFORM_WINDOWS ? "conjure_runtime.exe" : "conjure_runtime";
    }

    /// <summary>
    /// The stand-in distribution folder "dist" of a temp directory, as a Distribution build lays it out: the
    /// runtime executable and the shared library, both empty.
    /// </summary>
    void write_distribution(const temp_directory &directory) {
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + runtime_name(), ""));
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + core::dynamic_library::decorate_name("conjure_shared"), ""));
    }

    /// <summary>
    /// Exports the project into 'outputDir' with step 5 copying from the "dist" folder of 'directory' (no native
    /// module, no managed code) and returns what the pipeline answered.
    /// </summary>
    [[nodiscard]] core::result<export_report> export_with_runtime(
      const project &loaded,
      const temp_directory &directory,
      const std::filesystem::path &outputDir,
      bool copyShaders
    ) {
        application app(project_desc(loaded, 0));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.distributionDirectory = directory.path() / "dist";
        settings.buildNativeModule = false;
        settings.publishManaged = false;
        settings.copyRuntime = true;
        settings.copyShaders = copyShaders;
        core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();
        return ran;
    }

    /// <summary>
    /// How often an export that copies the runtime from a directory without Visual C++ runtime DLLs names the
    /// redistributable: once where the build imports those DLLs, which is a Windows build against Microsoft's
    /// standard library and its DLL runtime (/MD). On Linux and macOS, in a Windows build against another
    /// standard library and in one against the static runtime, such a directory is complete.
    /// </summary>
    [[nodiscard]] usize expected_redistributable_warnings() {
#if CONJURE_PLATFORM_WINDOWS && CONJURE_STDLIB_MSVC
        return (modules::ABI_SIGNATURE_RUNTIME_FLAGS & modules::ABI_RUNTIME_DYNAMIC_CRT) != 0 ? usize{1} : usize{0};
#else
        return usize{0};
#endif
    }

    [[nodiscard]] std::string file_text(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] usize count_warnings(const export_report &report, std::string_view needle) {
        const auto names = [needle](const std::string &warning) {
            return warning.find(needle) != std::string::npos;
        };
        return static_cast<usize>(std::count_if(report.warnings.begin(), report.warnings.end(), names));
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
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "gold:10;potion:2"));
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/more.bytes", "a second blob"));

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
            settings.alwaysInclude = {"assets://data/more.bytes", "assets://data/absent.bytes"};
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
            return warning.find("absent.bytes") != std::string::npos;
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
        CHECK(cooked.context().assets->find("assets://data/notes.bytes").has_value());
        CHECK(cooked.context().assets->find("assets://data/more.bytes").has_value());
        CHECK(cooked.context().filesystem->exists("cooked://content.cpak"));
        CHECK(cooked.run() == 0);
        CHECK(cooked.time().frameIndex == 60);
    }

    TEST_CASE("export_pipeline - refuses the project directory and a folder inside the assets root as output") {
        log_capture logs = {};
        temp_directory directory("engine_export_into_project");
        const std::filesystem::path projectDir = directory.path() / "project";
        project loaded = engine_test::write_project(projectDir, "inside");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
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
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));

        // A distribution folder as the Distribution build lays it out, with a module's shader set in a subfolder.
        const std::string runtimeName = runtime_name();
        write_distribution(directory);
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
            CAPTURE(file);
            CONJURE_REQUIRE_OK(directory.write_text("dist/shaders/" + std::string(file), "blob"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, true);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();

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

    TEST_CASE("regression - code sweep F790 copy_runtime ships every Visual C++ runtime DLL of the distribution "
      "directory") {
        log_capture logs = {};
        temp_directory directory("engine_export_crt");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "runtime_libraries");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);

        // Fake libraries, each with its own text, in the byte order of their names, which is the order the
        // report lists them in. The upper-case one is matched without regard to case and keeps its name.
        const std::array<std::string_view, 5> shipped = {
            "VCRUNTIME140_THREADS.DLL",
            "msvcp140.dll",
            "msvcp140_atomic_wait.dll",
            "vcruntime140.dll",
            "vcruntime140_1.dll",
        };
        for (const std::string_view name : shipped) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "library " + std::string(name)));
        }
        // Neighbours that are not Visual C++ runtime DLLs: other runtimes, other extensions, a name that only
        // contains the prefix, a library one folder down and a folder that carries a library's name.
        const std::array<std::string_view, 8> left = {
            "concrt140.dll",
            "ucrtbase.dll",
            "vcruntime14.dll",
            "my_msvcp140.dll",
            "msvcp140.lib",
            "vcruntime140.dll.bak",
            "nested/vcruntime140.dll",
            "msvcp140_folder.dll/inner.txt",
        };
        for (const std::string_view name : left) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "not shipped"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();

        for (const std::string_view name : shipped) {
            CAPTURE(name);
            CHECK(std::filesystem::is_regular_file(outputDir / name));
            CHECK(file_text(outputDir / name) == "library " + std::string(name));
        }
        for (const std::string_view name : left) {
            CAPTURE(name);
            CHECK_FALSE(std::filesystem::exists(outputDir / name));
        }
        CHECK_FALSE(std::filesystem::exists(outputDir / "nested"));
        CHECK_FALSE(std::filesystem::exists(outputDir / "msvcp140_folder.dll"));

        // The report names the runtime executable first and the five libraries as one run, in name order.
        const std::vector<std::string> &copied = report.copiedFiles;
        const auto runtime = std::find(copied.begin(), copied.end(), runtime_name());
        REQUIRE(runtime != copied.end());
        const auto firstLibrary = std::find(copied.begin(), copied.end(), shipped.front());
        REQUIRE(firstLibrary != copied.end());
        CHECK(runtime < firstLibrary);
        REQUIRE(static_cast<usize>(copied.end() - firstLibrary) >= shipped.size());
        CHECK(std::equal(shipped.begin(), shipped.end(), firstLibrary));
        CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
        CHECK(std::filesystem::is_regular_file(outputDir / core::dynamic_library::decorate_name("conjure_shared")));
        // With the libraries next to the runtime no platform is told to install the redistributable, and none
        // of them is taken for the debug runtime ("vcruntime140_threads.dll" ends in an 's', not in a 'd').
        CHECK(count_warnings(report, REDISTRIBUTABLE_WARNING) == 0);
        CHECK(count_warnings(report, DEBUG_RUNTIME_WARNING) == 0);
    }

    TEST_CASE("regression - code sweep F790 copy_runtime leaves the debug Visual C++ runtime behind and names it") {
        log_capture logs = {};
        temp_directory directory("engine_export_debug_crt");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "debug_runtime");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);

        // Every library of the debug runtime a toolset ships under the two prefixes, in the byte order of the
        // names: the 'd' follows the "140" or ends the name, in either case of the letter.
        const std::array<std::string_view, 8> debugLibraries = {
            "VCRUNTIME140D.DLL",
            "msvcp140_1d.dll",
            "msvcp140_2d.dll",
            "msvcp140d.dll",
            "msvcp140d_atomic_wait.dll",
            "msvcp140d_codecvt_ids.dll",
            "vcruntime140_1d.dll",
            "vcruntime140_threadsd.dll",
        };
        for (const std::string_view name : debugLibraries) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "debug library"));
        }
        // The release libraries whose names come closest: a 'd' inside the name is no mark.
        std::vector<std::string_view> releaseLibraries = {};
        usize expectedRedistributableWarnings = 0;

        SUBCASE("next to the release runtime, which ships") {
            releaseLibraries = {"msvcp140_codecvt_ids.dll", "vcruntime140.dll", "vcruntime140_threads.dll"};
        }
        SUBCASE("alone, which leaves the game without a runtime of its own") {
            expectedRedistributableWarnings = expected_redistributable_warnings();
        }
        for (const std::string_view name : releaseLibraries) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "release library"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();
        const std::vector<std::string> &copied = report.copiedFiles;

        for (const std::string_view name : debugLibraries) {
            CAPTURE(name);
            CHECK_FALSE(std::filesystem::exists(outputDir / name));
            CHECK(std::find(copied.begin(), copied.end(), name) == copied.end());
            // The distribution directory keeps its file: the export copies, it never moves.
            CHECK(std::filesystem::is_regular_file(directory.path() / "dist" / name));
        }
        for (const std::string_view name : releaseLibraries) {
            CAPTURE(name);
            CHECK(file_text(outputDir / name) == "release library");
            CHECK(std::find(copied.begin(), copied.end(), name) != copied.end());
        }

        // One warning names all eight, in name order, and none of the libraries that shipped.
        REQUIRE(count_warnings(report, DEBUG_RUNTIME_WARNING) == 1);
        const auto namesDebugRuntime = [](const std::string &text) {
            return text.find(DEBUG_RUNTIME_WARNING) != std::string::npos;
        };
        const std::string &warning =
          *std::find_if(report.warnings.begin(), report.warnings.end(), namesDebugRuntime);
        usize position = 0;
        for (const std::string_view name : debugLibraries) {
            CAPTURE(name);
            position = warning.find(name, position);
            REQUIRE(position != std::string::npos);
        }
        for (const std::string_view name : releaseLibraries) {
            CAPTURE(name);
            CHECK(warning.find(name) == std::string::npos);
        }
        CHECK(count_warnings(report, REDISTRIBUTABLE_WARNING) == expectedRedistributableWarnings);
    }

    TEST_CASE("regression - code sweep F200 copy_runtime ships the licence notices of the distribution directory") {
        log_capture logs = {};
        temp_directory directory("engine_export_notices");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "notices");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        const std::string noticesName(export_pipeline::NOTICES_FILE_NAME);
        CHECK(noticesName == "THIRD_PARTY_NOTICES.txt");
        // The file is copied byte for byte: either kind of line end and a name outside ASCII (UTF-8) survive.
        const std::string notices = "Third-party notices\r\nGLFW: Camilla L\xC3\xB6wy\nzlib licence text\n";
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + noticesName, notices));
        // A file whose name only resembles the notices stays behind.
        CONJURE_REQUIRE_OK(directory.write_text("dist/THIRD_PARTY_NOTICES.md", "not shipped"));

        SUBCASE("into an empty folder") {}
        SUBCASE("over the notices an earlier export left in the folder") {
            // Longer than the new file, so a copy that did not replace the old one would show.
            const std::string older = "the notices of an older build\n" + std::string(4096, '.');
            CONJURE_REQUIRE_OK(directory.write_text("out/" + noticesName, older));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();

        REQUIRE(std::filesystem::is_regular_file(outputDir / noticesName));
        CHECK(file_text(outputDir / noticesName) == notices);
        CHECK_FALSE(std::filesystem::exists(outputDir / "THIRD_PARTY_NOTICES.md"));
        // The report names the notices once, after the runtime executable they belong to.
        const std::vector<std::string> &copied = report.copiedFiles;
        CHECK(std::count(copied.begin(), copied.end(), noticesName) == 1);
        const auto runtime = std::find(copied.begin(), copied.end(), runtime_name());
        const auto shipped = std::find(copied.begin(), copied.end(), noticesName);
        REQUIRE(runtime != copied.end());
        REQUIRE(shipped != copied.end());
        CHECK(runtime < shipped);
        CHECK(count_warnings(report, NOTICES_WARNING) == 0);
    }

    TEST_CASE("export_pipeline - a distribution directory without the licence notices is named in a warning") {
        log_capture logs = {};
        temp_directory directory("engine_export_no_notices");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "no_notices");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        const std::string noticesName(export_pipeline::NOTICES_FILE_NAME);

        SUBCASE("the directory holds no such file") {}
        SUBCASE("the directory holds a folder of that name") {
            // A folder is not the notices, and nothing of it is copied.
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + noticesName + "/inner.txt", "not shipped"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        // The export itself succeeds: the runtime is there, and the report says what is missing, once.
        CONJURE_REQUIRE_OK(ran);
        CHECK(count_warnings(ran.value(), NOTICES_WARNING) == 1);
        CHECK(count_warnings(ran.value(), noticesName) == 1);
        CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
        CHECK_FALSE(std::filesystem::exists(outputDir / noticesName));
        const std::vector<std::string> &copied = ran.value().copiedFiles;
        CHECK(std::find(copied.begin(), copied.end(), noticesName) == copied.end());
    }

    TEST_CASE("export_pipeline - a distribution directory without Visual C++ runtime DLLs is named in a warning "
      "where the runtime imports them") {
        log_capture logs = {};
        temp_directory directory("engine_export_no_crt");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "no_runtime_libraries");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        // The Concurrency Runtime is no substitute: it is not one of the libraries step 5 ships.
        CONJURE_REQUIRE_OK(directory.write_text("dist/concrt140.dll", "not shipped"));
        const usize expectedWarnings = expected_redistributable_warnings();

        SUBCASE("the runtime is copied") {
            const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
            CONJURE_REQUIRE_OK(ran);
            CHECK(count_warnings(ran.value(), REDISTRIBUTABLE_WARNING) == expectedWarnings);
            CHECK(count_warnings(ran.value(), "vcruntime140*.dll") == expectedWarnings);
            CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
            CHECK_FALSE(std::filesystem::exists(outputDir / "concrt140.dll"));
        }

        SUBCASE("an export that does not copy the runtime says nothing about its libraries or its notices") {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            SUBCASE("copyRuntime is false") {
                settings.distributionDirectory = directory.path() / "dist";
                settings.copyRuntime = false;
            }
            SUBCASE("no distribution directory is given") {
                settings.copyRuntime = true;
            }
            const core::result<export_report> ran = pipeline.run(settings);
            CONJURE_REQUIRE_OK(ran);
            CHECK(count_warnings(ran.value(), REDISTRIBUTABLE_WARNING) == 0);
            // No engine binary went into the folder, so there is nothing whose notices would be missing.
            CHECK(count_warnings(ran.value(), NOTICES_WARNING) == 0);
            CHECK_FALSE(std::filesystem::exists(outputDir / runtime_name()));
            CHECK_FALSE(std::filesystem::exists(outputDir / export_pipeline::NOTICES_FILE_NAME));
            app.shutdown();
        }
    }

    TEST_CASE("export_pipeline - copy_runtime fails on a file it cannot copy and keeps what it copied before") {
        log_capture logs = {};
        temp_directory directory("engine_export_copy_failure");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "copy_failure");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        CONJURE_REQUIRE_OK(directory.write_text("dist/vcruntime140.dll", "library"));
        const std::string noticesName(export_pipeline::NOTICES_FILE_NAME);
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + noticesName, "notices"));
        const std::string sharedName = core::dynamic_library::decorate_name("conjure_shared");
        std::error_code error = {};
        std::string failing = {};

        // A folder where a file belongs is not replaced by the file, and a folder is not a file to copy, on
        // every platform.
        SUBCASE("a folder in the output carries the name of a Visual C++ runtime DLL") {
            failing = "vcruntime140.dll";
            std::filesystem::create_directories(outputDir / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("a folder in the output carries the name of the licence notices") {
            failing = noticesName;
            std::filesystem::create_directories(outputDir / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("a folder in the output carries the name of the shared library") {
            failing = sharedName;
            std::filesystem::create_directories(outputDir / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("the shared library is a folder in the distribution directory") {
            failing = sharedName;
            REQUIRE(std::filesystem::remove(directory.path() / "dist" / failing, error));
            std::filesystem::create_directories(directory.path() / "dist" / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("the shared library is missing from the distribution directory") {
            failing = sharedName;
            REQUIRE(std::filesystem::remove(directory.path() / "dist" / failing, error));
        }
        CAPTURE(failing);

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_CHECK_ERROR(ran, engine_error::EXPORT_FAILED);
        REQUIRE_FALSE(ran.has_value());
        const std::string message = ran.get_error().to_string();
        CHECK(message.find("copy_runtime") != std::string::npos);
        CHECK(message.find(failing) != std::string::npos);
        // Step 5 copies file by file: the runtime executable, copied first, stays. Nothing is put inside the
        // folder that was in the way, and step 6 never ran.
        CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
        CHECK_FALSE(std::filesystem::exists(outputDir / failing / failing));
        CHECK_FALSE(std::filesystem::exists(outputDir / "project.json"));
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
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
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
