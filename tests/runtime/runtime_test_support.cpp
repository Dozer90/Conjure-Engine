#include "runtime_test_support.h"

#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/string_utils.h"
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

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_class_registry.h"
    #include "conjure/scripting/script_component.h"
    #include "conjure/scripting/script_host.h"
    #include "conjure/scripting/script_runtime.h"
#endif

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#ifndef CONJURE_RUNTIME_TEST_HAS_DOTNET
    #define CONJURE_RUNTIME_TEST_HAS_DOTNET 0
#endif
#ifndef CONJURE_RUNTIME_TEST_SCRIPTS_DIR
    #define CONJURE_RUNTIME_TEST_SCRIPTS_DIR ""
#endif
#ifndef CONJURE_RUNTIME_TEST_HAS_MSVC_CRT
    #define CONJURE_RUNTIME_TEST_HAS_MSVC_CRT 0
#endif

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

        /// <summary>
        /// The DOTNET_RUNTIME precondition of this executable: the managed SDK sits where conjure_runtime
        /// looks for it ("managed" next to the executable), Conjure.TestScripts was built with this tree and
        /// nethost finds a runtime (the probe never starts the CLR).
        /// </summary>
        bool dotnet_runtime_available() noexcept {
#if CONJURE_RUNTIME_TEST_HAS_DOTNET && defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
            static const bool available = []() noexcept {
                std::error_code probe = {};
                const std::filesystem::path sdk = runtime_bin_dir() / "managed" / "Conjure.Managed.dll";
                const bool sdkPresent = std::filesystem::is_regular_file(sdk, probe);
                const bool scriptsPresent = std::filesystem::is_regular_file(test_scripts_assembly(), probe);
                return sdkPresent && scriptsPresent && scripting::is_runtime_available();
            }();
            return available;
#else
            return false;
#endif
        }

        struct probe_installer {
            probe_installer() {
                testing::set_precondition_probe(testing::precondition::DOTNET_RUNTIME, &dotnet_runtime_available);
            }
        };

        const probe_installer sProbeInstaller = {};
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
        return run_runtime(std::move(arguments), executable, {});
    }

    core::process_result run_runtime(
      std::vector<std::string> arguments,
      const std::filesystem::path &executable,
      std::vector<std::pair<std::string, std::string>> environment
    ) {
        core::process_desc desc = {};
        desc.executable = path_argument(executable);
        desc.arguments = std::move(arguments);
        desc.environment = std::move(environment);
        desc.timeoutSeconds = CHILD_TIMEOUT_SECONDS;
        core::result<core::process_result> ran = core::process::run(desc);
        CONJURE_REQUIRE_OK(ran);
        REQUIRE_FALSE(ran.value().timedOut);
        return std::move(ran).value();
    }

    std::vector<std::pair<std::string, std::string>> dotnet_root_environment(const std::filesystem::path &root) {
        // The host reads the path as the platform spells it, so the native separators stay.
        const std::u8string native = root.u8string();
        const std::string value(native.begin(), native.end());
        return {
            {"DOTNET_ROOT", value},
            {"DOTNET_ROOT_X64", value},
            {"DOTNET_ROOT_ARM64", value},
        };
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

    bool scripting_built() noexcept {
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        return true;
#else
        return false;
#endif
    }

    bool editor_built() noexcept {
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
        return true;
#else
        return false;
#endif
    }

    bool physics_built() noexcept {
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        return true;
#else
        return false;
#endif
    }

    std::filesystem::path test_scripts_assembly() {
        const std::filesystem::path root(CONJURE_RUNTIME_TEST_SCRIPTS_DIR);
        if (root.empty()) {
            return {};
        }
        return root / "v1" / "Conjure.TestScripts.dll";
    }

    void write_scripted_project(const std::filesystem::path &projectDir, const std::filesystem::path &gameAssembly) {
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        const std::string scripting = std::format(
          R"("scripting": {{ "enabled": true, "assembly": "{}", "hot_reload": false }})",
          path_argument(gameAssembly)
        );
        write_file(projectDir / "project.json", project_text(START_SCENE, scripting));

        // The script runtime on the null host is the authoring tool: it owns the script_component serializer.
        // The null host knows no class of that name, so the entry is saved by its hash, which is what the real
        // host resolves against the game assembly.
        application_desc desc = {};
        desc.config = inline_config();
        desc.config.scripting.enabled = true;
        desc.config.scripting.useNullHost = true;
        desc.config.scripting.hotReload = false;
        application authoring(std::move(desc));
        CONJURE_REQUIRE_OK(authoring.initialise());
        REQUIRE(authoring.context().scripting != nullptr);
        scene::scene &draft = authoring.context().scenes->active();
        draft.set_name("start");
        const core::result<ecs::entity> scripted = draft.create_entity(core::string_id("scripted"));
        CONJURE_REQUIRE_OK(scripted);
        const scripting::script_entry_id entry = authoring.context().scripting->add_script(
          scripted.value(),
          scripting::script_name_hash(SCRIPT_CLASS),
          core::json::value()
        );
        REQUIRE(entry.is_valid());
        core::vfs::native_file_system files(projectDir / "assets", false);
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene_file(draft, files, START_SCENE));
        authoring.shutdown();
#else
        static_cast<void>(projectDir);
        static_cast<void>(gameAssembly);
        FAIL("write_scripted_project needs the scripting module; check scripting_built() first");
#endif
    }

    bool msvc_runtime_libraries_copied() noexcept {
#if CONJURE_RUNTIME_TEST_HAS_MSVC_CRT
        return true;
#else
        return false;
#endif
    }

    std::vector<std::filesystem::path> msvc_runtime_libraries_in(const std::filesystem::path &directory) {
        std::vector<std::filesystem::path> libraries = {};
        std::error_code error = {};
        std::filesystem::directory_iterator walker(directory, error);
        const std::filesystem::directory_iterator end = {};
        while (!error && walker != end) {
            std::error_code status = {};
            // The names searched for are ASCII; one that is not is no runtime library, whatever it converts to.
            const std::string name = walker->path().filename().generic_string();
            const bool runtimeLibrary =
              (core::str::istarts_with(name, "vcruntime140") || core::str::istarts_with(name, "msvcp140"))
              && core::str::iends_with(name, ".dll");
            if (runtimeLibrary && walker->is_regular_file(status)) {
                libraries.push_back(walker->path().filename());
            }
            walker.increment(error);
        }
        const auto byName = [](const std::filesystem::path &left, const std::filesystem::path &right) {
            return left.native() < right.native();
        };
        std::sort(libraries.begin(), libraries.end(), byName);
        return libraries;
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
