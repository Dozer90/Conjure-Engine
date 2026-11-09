#pragma once

#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// module.json (docs/design/07_modules.md 2.1).
//
//   {
//     "schema": 1, "name": "com.studio.game", "version": "0.3.1", "kind": "game", "engine": "^0.1.0",
//     "platforms": ["windows", "linux", "macos"], "requires": ["renderer"], "binary": "game",
//     "dependencies": [{ "name": "conjure.physics", "version": "*", "optional": false }],
//     "managed": ["managed/Game.dll"], "content": ["assets"], "editorOnly": false
//   }
//
// Required: schema, name, version, engine. Everything else has a default. Unknown keys log a
// warning. The binary's location is derived, never declared: <root>/bin/<platform>-<arch>-<config>/
// plus the platform decoration of "binary" (game.dll, libgame.so, libgame.dylib).

namespace conjure::modules {

    inline constexpr std::string_view MODULE_MANIFEST_FILE_NAME = "module.json";
    inline constexpr u32 MODULE_MANIFEST_SCHEMA_VERSION = 1;
    inline constexpr usize MAX_MODULE_NAME_LENGTH = 64;

    /// <summary>
    /// One entry of "dependencies".
    /// </summary>
    struct module_dependency {
        std::string name = {};
        version_range version = {};
        bool optional = false;

        [[nodiscard]] bool operator==(const module_dependency &) const = default;
    };

    /// <summary>
    /// The parsed manifest. Paths are relative to the module root (the manifest's directory).
    /// </summary>
    struct module_manifest {
        u32 schemaVersion = MODULE_MANIFEST_SCHEMA_VERSION;
        std::string name = {};                              // [a-z0-9_.]{1,64}
        std::string displayName = {};
        std::string description = {};
        semantic_version version = {};
        module_kind kind = module_kind::PLUGIN;
        version_range engineVersion = {};                   // source compatibility
        module_platform_flags platforms = module_platform_flags::ALL;
        module_feature_flags requiredFeatures = module_feature_flags::NONE;
        std::string binary = {};                            // base name; empty = no native code
        std::vector<module_dependency> dependencies = {};
        std::vector<std::string> managedAssemblies = {};    // relative to the module root
        std::vector<std::string> contentDirectories = {};   // relative to the module root
        bool editorOnly = false;

        [[nodiscard]] bool operator==(const module_manifest &) const = default;
    };

    /// <summary>
    /// True for names matching [a-z0-9_.]{1,64}.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_valid_module_name(std::string_view name) noexcept;

    /// <summary>
    /// Parses and validates manifest text. 'sourceName' is the file the text came from, used in
    /// messages together with the JSON pointer of the offending member. MANIFEST_INVALID on any
    /// JSON, schema, type or range error; INVALID_NAME for a bad name.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<module_manifest> parse_module_manifest(
      std::string_view jsonText,
      std::string_view sourceName = "<memory>"
    );

    /// <summary>
    /// Reads and parses a manifest file. MANIFEST_NOT_FOUND when the file is missing, otherwise
    /// as parse_module_manifest().
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<module_manifest> load_module_manifest(const std::filesystem::path &path);

    /// <summary>
    /// Serialises a manifest. parse_module_manifest(write(m).to_string()) equals m. Export drops
    /// "editorOnly" by passing includeEditorOnly = false.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document write_module_manifest(
      const module_manifest &manifest,
      bool includeEditorOnly = true
    );

    /// <summary>
    /// The file name of a module binary on a platform: "game" becomes "game.dll", "libgame.so" or
    /// "libgame.dylib". Empty input yields an empty string.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string decorate_binary_name(std::string_view baseName, module_platform platform);

    /// <summary>
    /// &lt;root&gt;/bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;/&lt;decorated binary&gt;, or an empty path
    /// when the manifest declares no binary.
    /// </summary>
    [[nodiscard]] CONJURE_API std::filesystem::path module_binary_path(
      const module_manifest &manifest,
      const std::filesystem::path &rootDirectory,
      const binary_triplet &triplet
    );

    /// <summary>
    /// &lt;root&gt;/bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;: the directory module_binary_path() points into.
    /// </summary>
    [[nodiscard]] CONJURE_API std::filesystem::path module_binary_directory(
      const std::filesystem::path &rootDirectory,
      const binary_triplet &triplet
    );
}
