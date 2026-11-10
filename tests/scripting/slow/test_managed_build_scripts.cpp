#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/jobs/main_thread.h"
#include "conjure/scripting/managed_build.h"

#include <filesystem>
#include <string>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// The game project template beside the SDK projects (managed/Conjure.Templates/Game).
    /// </summary>
    [[nodiscard]] std::filesystem::path game_template_project() {
        return scripting_test::bootstrap_project().parent_path().parent_path() / "Conjure.Templates" / "Game" /
               "Game.csproj";
    }

    /// <summary>
    /// Runs one build to completion and returns its report. Every build writes its restore and compile
    /// intermediates below the temp_directory (ArtifactsPath), never beside the project in the source tree.
    /// </summary>
    [[nodiscard]] build_report run_build(const temp_directory &directory, build_request request) {
        request.outputDirectory = directory.file("out");
        request.properties.emplace_back("ArtifactsPath", directory.file("artifacts").string());
        request.timeoutSeconds = 540.0;
        REQUIRE_FALSE(request.projectPath.empty());

        managed_build_runner runner = {};
        build_report received = {};
        bool completed = false;
        runner.build(request, [&completed, &received](const build_report &report) {
            completed = true;
            received = report;
        });
        runner.wait();
        CHECK(core::jobs::drain_main_thread_queue() >= 1);
        REQUIRE(completed);
        CHECK_FALSE(received.timedOut);
        CHECK(received.elapsedSeconds > 0.0);
        return received;
    }

    /// <summary>
    /// True when no obj/ or bin/ sits beside the project file.
    /// </summary>
    [[nodiscard]] bool source_tree_clean(const std::filesystem::path &project) {
        std::error_code probe = {};
        const std::filesystem::path directory = project.parent_path();
        return !std::filesystem::exists(directory / "obj", probe) && !std::filesystem::exists(directory / "bin", probe);
    }
}

TEST_SUITE("scripting/managed_build_scripts") {

    TEST_CASE(
      "managed_build_runner - builds Conjure.TestScripts into a scratch directory [dotnet][slow]" *
      doctest::timeout(600)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        temp_directory directory = {};
        build_request request = {};
        request.projectPath = scripting_test::test_scripts_project();
        request.configuration = "Debug";
        request.properties = {{"ScriptVariant", "V1"}};

        const build_report received = run_build(directory, request);
        CHECK_MESSAGE(received.succeeded, received.output);
        CHECK(received.launch.has_value());
        CHECK(received.exitCode == 0);
        CHECK(received.error_count() == 0);
        std::error_code probe = {};
        CHECK(std::filesystem::is_regular_file(directory.file("out/Conjure.TestScripts.dll"), probe));
        CHECK(std::filesystem::is_directory(directory.file("artifacts/obj/Conjure.TestScripts"), probe));
        CHECK(source_tree_clean(request.projectPath));
    }

    TEST_CASE(
      "regression - the game template names ConjureManagedDir when it is missing [dotnet][slow]" *
      doctest::timeout(600)
    ) {
        // The template used to default ConjureManagedDir to a directory no build produces, so a build
        // without the property failed on the unresolved Conjure.Managed reference far from the cause.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        temp_directory directory = {};
        build_request request = {};
        request.projectPath = game_template_project();
        request.configuration = "Debug";

        const build_report received = run_build(directory, request);
        CHECK_FALSE(received.succeeded);
        CHECK_FALSE(received.launch.has_value());   // BUILD_FAILED, not a launch problem
        CHECK(received.exitCode != 0);
        CHECK_MESSAGE(received.output.find("ConjureManagedDir") != std::string::npos, received.output);
        CHECK(source_tree_clean(request.projectPath));
    }
}
