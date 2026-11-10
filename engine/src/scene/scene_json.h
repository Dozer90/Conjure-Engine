#pragma once

#include "conjure/scene/scene_serializer.h"

#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"

#include <string>
#include <string_view>

// The file headers shared by scene_serializer, the importers and the loaders (docs/design/13_scene.md
// 2.3, 2.4): validation of format and version, the key order of the writers and the canonical form of
// the settings document.

namespace conjure::scene::detail {

    /// <summary>
    /// Header fields common to both formats.
    /// </summary>
    struct file_header {
        std::string name = {};
        u32 version = 0;
    };

    /// <summary>
    /// Checks the root object, "format" and "version" (BAD_FORMAT, UNSUPPORTED_VERSION) and reads the
    /// name; a missing name is empty.
    /// </summary>
    [[nodiscard]] core::result<file_header> read_header(const core::json::value &in, std::string_view expectedFormat);

    /// <summary>
    /// Writes the leading header members format, version and name.
    /// </summary>
    void write_header(core::json::writer &out, std::string_view format, std::string_view name);

    /// <summary>
    /// The canonical text of a settings document; "{}" for a document whose root is not an object.
    /// </summary>
    [[nodiscard]] std::string canonical_settings(const core::json::document &settings);

    /// <summary>
    /// Parses JSON text and the scene header + archive in one step (loaders, importers).
    /// </summary>
    [[nodiscard]] core::result<scene_document> parse_scene_text(
      std::string_view text,
      const ecs::component_registry &registry
    );

    /// <summary>
    /// Parses JSON text and the prefab header + archive in one step (loaders, importers).
    /// </summary>
    [[nodiscard]] core::result<prefab_document> parse_prefab_text(
      std::string_view text,
      const ecs::component_registry &registry
    );
}
