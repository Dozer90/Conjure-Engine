#include "runtime_test_support.h"

#include "conjure/testing/doctest.h"

#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace runtime_test {

    namespace {
        constexpr f64 CHILD_TIMEOUT_SECONDS = 180.0;

        /// <summary>
        /// The headless configuration the authoring and exporting applications run with: null backends, every
        /// job inline, no engine plugins, warnings and above only.
        /// </summary>
        [[nodiscard]] engine_config inline_config() {
            engine_config config = engine_config::headless_defaults();
            config.jobs.workerCount = 0;
            config.logLevel = core::log_level::WARN;
            config.modules.enginePlugins = false;
            return config;
        }

        /// <summary>
        /// A descriptor over a loaded project: the project's settings applied over the inline configuration.
        /// </summary>
        [[nodiscard]] application_desc project_desc(project loaded) {
            application_desc desc = {};
            desc.config = inline_config();
            desc.config.projectPath = loaded.root_directory();
            loaded.apply_to(desc.config);
            desc.project = std::move(loaded);
            return desc;
        }

        /// <summary>
        /// Writes "assets/scenes/start.scene.json" (two named entities under one root) with a stand-alone
        /// application as the authoring registry.
        /// </summary>
        void author_start_scene(const std::filesystem::path &assetsDirectory) {
            application_desc desc = {};
            desc.config = inline_config();
            application authoring(std::move(desc));
            CONJURE_REQUIRE_OK(authoring.initialise());
            scene::scene &draft = authoring.context().scenes->active();
            draft.set_name("start");
            const core::result<ecs::entity> root = draft.create_entity(core::string_id("root"));
            CONJURE_REQUIRE_OK(root);
            CONJURE_REQUIRE_OK(draft.create_entity(core::string_id("child"), root.value()));
            core::vfs::native_file_system files(assetsDirectory, false);
            CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene_file(draft, files, START_SCENE));
            authoring.shutdown();
        }
    }

    const std::filesystem::path &runtime_executable() {
        static const std::filesystem::path path = CONJURE_RUNTIME_EXECUTABLE;
        return path;
    }

    const std::filesystem::path &runtime_bin_dir() {
        static const std::filesystem::path path = CONJURE_RUNTIME_BIN_DIR;
        return path;
    }

    core::process_result run_runtime(std::vector<std::string> arguments) {
        return run_runtime(std::move(arguments), runtime_executable());
    }

    core::process_result run_runtime(std::vector<std::string> arguments, const std::filesystem::path &executable) {
        core::process_desc desc = {};
        desc.executable = path_argument(executable);
        desc.arguments = std::move(arguments);
        desc.timeoutSeconds = CHILD_TIMEOUT_SECONDS;
        core::result<core::process_result> ran = core::process::run(desc);
        CONJURE_REQUIRE_OK(ran);
        REQUIRE_FALSE(ran.value().timedOut);
        return std::move(ran).value();
    }

    std::string all_output(const core::process_result &result) {
        return result.standardOutput + "\n" + result.standardError;
    }

    bool contains(std::string_view text, std::string_view needle) {
        return text.find(needle) != std::string_view::npos;
    }

    std::string path_argument(const std::filesystem::path &path) {
        const std::u8string generic = path.generic_u8string();
        return std::string(generic.begin(), generic.end());
    }

    std::string read_text_file(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return {};
        }
        std::ostringstream text = {};
        text << stream.rdbuf();
        return text.str();
    }

    void write_file(const std::filesystem::path &path, std::string_view text) {
        std::error_code ignored = {};
        std::filesystem::create_directories(path.parent_path(), ignored);
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << text;
    }

    std::string project_text(std::string_view startupScene, std::string_view extraJson) {
        std::string text = "{\n  \"schema_version\": 1,\n  \"name\": \"runtime_tiny\",\n";
        text += "  \"assets_root\": \"assets\",\n  \"startup_scene\": \"";
        text += startupScene;
        text += "\"";
        if (!extraJson.empty()) {
            text += ",\n  ";
            text += extraJson;
        }
        text += "\n}\n";
        return text;
    }

    void write_tiny_project(const std::filesystem::path &projectDir) {
        write_file(projectDir / "project.json", project_text(START_SCENE));
        write_file(projectDir / "assets" / "data" / "notes.bin", "gold:10;potion:2");
        author_start_scene(projectDir / "assets");
    }

    void export_tiny_project(
      const std::filesystem::path &projectDir,
      const std::filesystem::path &outputDir,
      bool copyRuntime
    ) {
        core::result<project> loaded = project::load(projectDir);
        CONJURE_REQUIRE_OK(loaded);
        application app(project_desc(std::move(loaded).value()));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.publishManaged = false;
        settings.copyRuntime = copyRuntime;
        settings.copyShaders = false;
        settings.distributionDirectory = runtime_bin_dir();
        // The cook closure is the startup scene; the data blob is unreferenced and rides along explicitly.
        settings.alwaysInclude = {"assets://data/notes.bin"};
        const core::result<export_report> ran = pipeline.run(settings);
        CONJURE_REQUIRE_OK(ran);
        CHECK(ran.value().assetCount >= 2);
        app.shutdown();
        REQUIRE(std::filesystem::is_regular_file(outputDir / "project.json"));
        REQUIRE(std::filesystem::is_regular_file(outputDir / "cooked" / "content.cpak"));
    }
}
