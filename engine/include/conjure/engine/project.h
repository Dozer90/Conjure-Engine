#pragma once

#include "conjure/engine/engine_config.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// project.json (docs/design/06_engine_loop.md 2.10, D9, R20, R39, R52, R53): schema version 1, snake_case
// keys. Unknown top-level keys log one WARN each and are preserved in `extensions` on save, unless `extensions`
// already has a member of that name, which wins; relative paths resolve against the project root; "fixed_hz" is
// stored for readability and converted to fixedDeltaSeconds. The export pipeline writes the same file with
// "cooked": true.

namespace conjure::engine {

    inline constexpr u32 PROJECT_SCHEMA_VERSION = 1;
    inline constexpr std::string_view PROJECT_FILE_NAME = "project.json";

    /// <summary>
    /// A module manifest the project refers to: the root module.json (the single game module, required)
    /// or one found under modules.search_paths.
    /// </summary>
    struct project_module_entry {
        std::string name = {};                      // directory name of the module (the manifest names itself)
        std::filesystem::path manifestPath = {};    // absolute path of module.json
        bool required = false;                      // listed in modules.required, or the game module

        [[nodiscard]] bool operator==(const project_module_entry &) const = default;
    };

    struct project_description {
        u32 schemaVersion = PROJECT_SCHEMA_VERSION;
        std::string name = {};
        std::string version = "0.1.0";
        std::string engineVersion = {};
        std::filesystem::path assetsRoot = std::filesystem::path("assets");
        std::filesystem::path cookedRoot = std::filesystem::path("cooked");
        std::filesystem::path startupScene = {};    // relative to assetsRoot
        modules_settings modules = {};
        window_settings window = {};
        time_settings time = {};
        physics_config physics = {};
        rendering_settings rendering = {};
        audio_config audio = {};
        assets_settings assets = {};                // the "assets" section: hot_reload
        scripting_settings scripting = {};
        std::vector<std::string> layers = {};       // scene layer names, index order (at most 32)
        core::json::document physicsSection = core::json::document::object();   // the raw "physics" object (R53)
        core::json::document extensions = core::json::document::object();       // opaque per-module sections
        bool cooked = false;                        // true in an exported build: only cookedRoot is mounted
    };

    class CONJURE_API project {
        private:
            std::filesystem::path mRootDirectory = {};
            project_description mDescription = {};

        public:
            project();
            ~project();

            project(const project &) = default;
            project &operator=(const project &) = default;
            project(project &&) noexcept = default;
            project &operator=(project &&) noexcept = default;

        public:
            /// <summary>
            /// Reads &lt;directory&gt;/project.json. PROJECT_NOT_FOUND when the directory or file is missing,
            /// then as parse().
            /// </summary>
            [[nodiscard]] static core::result<project> load(const std::filesystem::path &directory);

            /// <summary>
            /// Parses project.json text for a project rooted at 'rootDirectory'. PROJECT_PARSE_FAILED (with
            /// yyjson's line and column), PROJECT_SCHEMA_TOO_NEW, PROJECT_INVALID (a non-object root, a
            /// missing or empty "name", a member of the wrong type).
            /// </summary>
            [[nodiscard]] static core::result<project> parse(
              std::string_view text,
              std::filesystem::path rootDirectory
            );

            /// <summary>
            /// Writes &lt;root&gt;/project.json (pretty printed) from to_json(). IO_FAILURE from the file system.
            /// </summary>
            [[nodiscard]] core::result<void> save() const;

            /// <summary>
            /// The document save() writes: every section in schema order, "fixed_hz" instead of the delta,
            /// unknown keys inside "extensions".
            /// </summary>
            [[nodiscard]] core::json::document to_json() const;

            [[nodiscard]] const project_description &description() const noexcept {
                return mDescription;
            }

            [[nodiscard]] project_description &description() noexcept {
                return mDescription;
            }

            [[nodiscard]] const std::filesystem::path &root_directory() const noexcept {
                return mRootDirectory;
            }

            /// <summary>
            /// &lt;root&gt;/project.json.
            /// </summary>
            [[nodiscard]] std::filesystem::path file_path() const;

            /// <summary>
            /// assetsRoot resolved against the root (an absolute assetsRoot is returned as is).
            /// </summary>
            [[nodiscard]] std::filesystem::path assets_directory() const;

            /// <summary>
            /// cookedRoot resolved against the root.
            /// </summary>
            [[nodiscard]] std::filesystem::path cooked_directory() const;

            /// <summary>
            /// Copies the settings sections, the module settings and startupScene (as startupSceneOverride
            /// when the config has none) into a config; headless, editor, maxFrames, jobs, memory, pacing,
            /// log settings and projectPath are untouched. rendering.enabled, the rendering and audio
            /// backends, assets.hotReload, scripting.enabled and the window size are copied only when the
            /// config is not headless, so headless_defaults() survive a project (the command line turns
            /// scripting back on with --scripting); audio.enabled is copied either way. rendering.gpuValidation
            /// is only raised, to the higher of the two levels: a saved project always names one, usually OFF,
            /// and that must not drop the debug layer of a Debug build (--gpu-validation sets any level).
            /// </summary>
            void apply_to(engine_config &config) const;

            /// <summary>
            /// The module manifests this project names: &lt;root&gt;/module.json when it exists (required, the game
            /// module) and every &lt;searchPath&gt;/*/module.json found on disk, in path order; `required` follows
            /// modules.required by directory name.
            /// </summary>
            [[nodiscard]] std::vector<project_module_entry> module_manifests() const;
    };
}
