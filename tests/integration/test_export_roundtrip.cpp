#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "integration_engine_support.h"

#include "conjure/core/process.h"
#include "conjure/core/types.h"
#include "conjure/ecs/archetype.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"
#include "conjure/scene/scene.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// docs/design/08_testing.md 6.1 test_export_roundtrip (R21, ARCHITECTURE.md 8): the in-repo project
// tests/assets/projects/minimal is exported into a temp directory through engine::export_pipeline (the build's
// licence notices travel with the runtime), the built conjure_runtime runs the result as a process with
// --headless --frames 60 and exits 0, and the exported folder loads headless into the same world the development
// project does: the same number of entities carrying each registered component type.

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr f64 CHILD_TIMEOUT_SECONDS = 180.0;
    constexpr u32 RUNTIME_FRAMES = 60;

    /// <summary>
    /// Entities carrying one component type, keyed by the type's name hash (stable across two applications,
    /// unlike the component ids a registry hands out); types no entity carries are left out. A vector reserved
    /// before the counting fixture exists, so filling it inside the fixture's scope allocates nothing that would
    /// outlive the fixture's memory baseline.
    /// </summary>
    using component_count = std::pair<u64, u32>;
    constexpr usize COMPONENT_COUNT_CAPACITY = 256;

    /// <summary>
    /// The headless, inline, warnings-only configuration the exporting application runs with.
    /// </summary>
    [[nodiscard]] engine::engine_config exporter_config(const engine::project &loaded) {
        engine::engine_config config = engine::engine_config::headless_defaults();
        config.jobs.workerCount = 0;
        config.logLevel = core::log_level::WARN;
        config.modules.enginePlugins = false;
        config.projectPath = loaded.root_directory();
        loaded.apply_to(config);
        return config;
    }

    [[nodiscard]] engine::export_report export_minimal(
      const std::filesystem::path &projectDirectory,
      const std::filesystem::path &outputDirectory
    ) {
        core::result<engine::project> loaded = engine::project::load(projectDirectory);
        CONJURE_REQUIRE_OK(loaded);
        engine::application_desc desc = {};
        desc.config = exporter_config(loaded.value());
        desc.project = std::move(loaded).value();
        engine::application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());

        engine::export_pipeline pipeline(app.context());
        engine::export_settings settings = {};
        settings.outputDirectory = outputDirectory;
        settings.distributionDirectory = integration_test::runtime_bin_directory();
        settings.buildNativeModule = false;      // the fixture has no native game module
        settings.publishManaged = false;         // and no scripts
        settings.copyRuntime = true;
        settings.copyShaders = false;            // the runtime below runs headless on the null device
        settings.alwaysInclude = {"assets://data/notes.bytes"};
        core::result<engine::export_report> ran = pipeline.run(settings);
        CONJURE_REQUIRE_OK(ran);
        app.shutdown();
        return std::move(ran).value();
    }

    void count_components(
      const ecs::world &world,
      const ecs::component_registry &registry,
      std::vector<component_count> &out
    ) {
        for (const ecs::component_info &info : registry.types()) {
            u32 carriers = 0;
            for (const ecs::archetype *archetype : world.archetypes()) {
                if (archetype->mask().test(info.id)) {
                    carriers += archetype->entity_count();
                }
            }
            if (carriers > 0) {
                REQUIRE(out.size() < out.capacity());
                out.emplace_back(info.name.hash(), carriers);
            }
        }
        std::sort(out.begin(), out.end());
    }

    /// <summary>
    /// Loads a project headless for one frame, fills 'counts' (reserved by the caller) with its per-type component
    /// counts and returns its entity count.
    /// </summary>
    [[nodiscard]] u32 load_project(
      const std::filesystem::path &projectDirectory,
      std::vector<component_count> &counts
    ) {
        headless_engine_options options = integration_test::quiet_options();
        options.projectDirectory = &projectDirectory;
        headless_engine engine(options);
        engine.step_frames(1);
        count_components(engine.world(), *engine.context().components, counts);
        return engine.world().entity_count();
    }
}

TEST_SUITE("integration/export_roundtrip") {

    TEST_CASE("export_roundtrip - the minimal project exports and the runtime boots the result for 60 frames") {
        REQUIRE_MESSAGE(
          std::filesystem::is_regular_file(integration_test::runtime_executable()),
          "conjure_runtime was not built next to these tests"
        );
        log_capture logs = {};
        temp_directory directory("integration_export_roundtrip");
        const std::filesystem::path projectDirectory = integration_test::copy_minimal_project(directory.path());
        const std::filesystem::path outputDirectory = directory.path() / "export";

        const engine::export_report report = export_minimal(projectDirectory, outputDirectory);
        CHECK(report.assetCount >= 2);
        CHECK(report.packBytes > 0);
        CHECK_FALSE(report.nativeModuleBuilt);
        CHECK_FALSE(report.managedPublished);
        CHECK(report.steps.size() == engine::export_pipeline::STEP_NAMES.size());
        REQUIRE(std::filesystem::is_regular_file(outputDirectory / "project.json"));
        REQUIRE(std::filesystem::is_regular_file(outputDirectory / "cooked" / "content.cpak"));
        REQUIRE(std::filesystem::is_regular_file(outputDirectory / "cooked" / "manifest.json"));
        const std::filesystem::path exportedRuntime =
          outputDirectory / integration_test::runtime_executable().filename();
        REQUIRE(std::filesystem::is_regular_file(exportedRuntime));
        // The licence notices the build wrote next to the runtime travel with it (06_engine_loop.md D15): the
        // same bytes, named in the report, and no warning that they are missing.
        const std::string noticesName(engine::export_pipeline::NOTICES_FILE_NAME);
        REQUIRE(std::filesystem::is_regular_file(outputDirectory / noticesName));
        const std::string builtNotices =
          integration_test::read_file(integration_test::runtime_bin_directory() / noticesName);
        CHECK(builtNotices.find("the Conjure contributors") != std::string::npos);
        CHECK(integration_test::read_file(outputDirectory / noticesName) == builtNotices);
        const std::vector<std::string> &copied = report.copiedFiles;
        CHECK(std::find(copied.begin(), copied.end(), noticesName) != copied.end());
        const auto namesNotices = [&noticesName](const std::string &warning) {
            return warning.find(noticesName) != std::string::npos;
        };
        CHECK(std::none_of(report.warnings.begin(), report.warnings.end(), namesNotices));
        const std::string exportedProject = integration_test::read_file(outputDirectory / "project.json");
        CHECK(exportedProject.find("\"cooked\": true") != std::string::npos);

        core::process_desc desc = {};
        desc.executable = exportedRuntime.generic_string();
        desc.arguments = {
            "--project", outputDirectory.generic_string(), "--headless", "--frames", std::to_string(RUNTIME_FRAMES)
        };
        desc.timeoutSeconds = CHILD_TIMEOUT_SECONDS;
        const core::result<core::process_result> ran = core::process::run(desc);
        CONJURE_REQUIRE_OK(ran);
        const std::string output = ran.value().standardOutput + "\n" + ran.value().standardError;
        INFO(output);
        CHECK_FALSE(ran.value().timedOut);
        // The exit code is the R21 contract; the shutdown line is the runtime's default INFO output and only
        // confirms that the frame limit, not a quit request, ended the run.
        CHECK(ran.value().exitCode == static_cast<i32>(engine::exit_code::OK));
        CHECK_MESSAGE(
          output.find("shutting down after 60 frame(s)") != std::string::npos,
          "the runtime did not report the 60-frame shutdown on its standard streams"
        );

        // The cooked folder describes the same world the development project does: as many entities, each
        // component type carried by as many of them.
        std::vector<component_count> developmentCounts = {};
        std::vector<component_count> exportedCounts = {};
        developmentCounts.reserve(COMPONENT_COUNT_CAPACITY);
        exportedCounts.reserve(COMPONENT_COUNT_CAPACITY);
        const u32 developmentEntities = load_project(projectDirectory, developmentCounts);
        const u32 exportedEntities = load_project(outputDirectory, exportedCounts);
        CHECK(developmentEntities > 0);
        CHECK(developmentEntities == exportedEntities);
        CHECK(developmentCounts.size() >= 3);    // at least local_transform, name and guid are carried
        CHECK(developmentCounts == exportedCounts);
        for (const component_count &expected : developmentCounts) {
            CAPTURE(expected.first);
            const auto sameType = [&expected](const component_count &candidate) {
                return candidate.first == expected.first;
            };
            const auto match = std::find_if(exportedCounts.begin(), exportedCounts.end(), sameType);
            REQUIRE(match != exportedCounts.end());
            CHECK(match->second == expected.second);
        }
    }
}
