#pragma once

#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/world.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Scene and prefab files (docs/design/13_scene.md 2.3, 2.4, 3.9, 3.10, 4.5): a small header plus the
// verbatim ecs::write_archive_json output, entities in hierarchy order, unknown components preserved,
// save -> load -> save byte-identical. The post-load bind pass resolves every asset_ref field through
// asset_manager::bind_slot (R25).

namespace conjure::scene {

    inline constexpr std::string_view SCENE_FORMAT_NAME = "conjure.scene";
    inline constexpr std::string_view PREFAB_FORMAT_NAME = "conjure.prefab";
    inline constexpr std::string_view SCENE_ASSET_TYPE_NAME = "conjure.scene.scene";
    inline constexpr std::string_view PREFAB_ASSET_TYPE_NAME = "conjure.scene.prefab";
    inline constexpr std::string_view SCENE_FILE_EXTENSION = ".scene.json";
    inline constexpr std::string_view PREFAB_FILE_EXTENSION = ".prefab.json";

    /// <summary>
    /// A parsed scene file; also the published payload of the conjure.scene.scene asset type.
    /// </summary>
    struct scene_document {
        std::string name = {};
        core::json::document settings = core::json::document::object();
        std::vector<core::uuid> roots = {};
        std::shared_ptr<const ecs::entity_archive> archive = nullptr;
    };

    using scene_asset = scene_document;

    /// <summary>
    /// A parsed prefab file: entry 0 of the archive is the single root.
    /// </summary>
    struct prefab_document {
        std::string name = {};
        std::shared_ptr<const ecs::entity_archive> archive = nullptr;
    };
}

CONJURE_DECLARE_ASSET_TYPE(conjure::scene::scene_asset, "conjure.scene.scene");

namespace conjure::scene {

    /// <summary>
    /// Stateless file and archive operations over a scene.
    /// </summary>
    class CONJURE_API scene_serializer {
        public:
            /// <summary>
            /// Writes the scene (header keys format, version, name, settings, roots, archive) into the
            /// writer; every entity of the tree in hierarchy order.
            /// </summary>
            [[nodiscard]] static core::result<void> save_scene(const scene &source, core::json::writer &out);

            /// <summary>
            /// save_scene into a file written atomically.
            /// </summary>
            [[nodiscard]] static core::result<void> save_scene_file(
              const scene &source,
              core::vfs::file_system &fs,
              std::string_view path
            );

            /// <summary>
            /// Validates the header (BAD_FORMAT, UNSUPPORTED_VERSION) and reads the archive against the
            /// registry (unknown components preserved, SERIALIZATION_FAILED for malformed ones).
            /// </summary>
            [[nodiscard]] static core::result<scene_document> parse_document(
              const core::json::value &in,
              const ecs::component_registry &registry
            );

            /// <summary>
            /// Like parse_document for the prefab format; PREFAB_MULTIPLE_ROOTS unless entry 0 is the
            /// only entry without a parent.
            /// </summary>
            [[nodiscard]] static core::result<prefab_document> parse_prefab(
              const core::json::value &in,
              const ecs::component_registry &registry
            );

            /// <summary>
            /// Writes the prefab format (format, version, name, archive).
            /// </summary>
            [[nodiscard]] static core::result<void> write_prefab(
              const prefab_document &document,
              core::json::writer &out
            );

            /// <summary>
            /// Writes the scene format from a document (used by the importer to canonicalise a file).
            /// </summary>
            [[nodiscard]] static core::result<void> write_document(
              const scene_document &document,
              core::json::writer &out
            );

            /// <summary>
            /// Instantiates a parsed scene into the target (one copy, owner recorded on the new roots)
            /// and orders the roots as the header lists them (missing guids skipped with WARN, unlisted
            /// roots appended with WARN).
            /// </summary>
            [[nodiscard]] static core::result<instantiate_report> load_scene(
              scene &target,
              const scene_document &document,
              scene_id owner,
              guid_policy guids
            );

            /// <summary>
            /// Reads, parses and loads a file from the file system.
            /// </summary>
            [[nodiscard]] static core::result<instantiate_report> load_scene_file(
              scene &target,
              core::vfs::file_system &fs,
              std::string_view path,
              scene_id owner,
              guid_policy guids
            );

            /// <summary>
            /// Writes a subtree in the prefab format (the root keeps its name).
            /// </summary>
            [[nodiscard]] static core::result<void> save_subtree(
              const scene &source,
              ecs::entity root,
              core::json::writer &out
            );

            /// <summary>
            /// Parses a prefab-format value and instantiates it under 'parent' at 'index'; keepGuids keeps
            /// the file's guids (DUPLICATE_GUID on a collision), otherwise every guid is regenerated.
            /// </summary>
            [[nodiscard]] static core::result<ecs::entity> instantiate_subtree(
              scene &target,
              const core::json::value &in,
              ecs::entity parent,
              bool keepGuids,
              u32 index = APPEND_CHILD
            );

            /// <summary>
            /// Captures a subtree in hierarchy order (root first). Without keepExternalReferences the
            /// root's parent link is dropped so entry 0 is a true root; with it the link is kept for
            /// undo payloads. INVALID_ENTITY for a dead root.
            /// </summary>
            [[nodiscard]] static core::result<ecs::entity_archive> capture_subtree(
              const scene &source,
              ecs::entity root,
              ecs::archive_options options = {}
            );

            /// <summary>
            /// Binds every non-null asset_ref field of the entities through asset_manager::bind_slot
            /// (never loads) and appends the bound ids to outIds when given (R25).
            /// </summary>
            static void bind_asset_refs(
              ecs::world &target,
              std::span<const ecs::entity> entities,
              assets::asset_manager &assets,
              std::vector<assets::asset_id> *outIds
            );

            /// <summary>
            /// bind_asset_refs for one entity (the editor after component_from_json).
            /// </summary>
            static void bind_entity(ecs::world &target, ecs::entity entity, assets::asset_manager &assets);

            /// <summary>
            /// Every entity of the tree in hierarchy order: roots in root order, each subtree depth first
            /// in sibling order.
            /// </summary>
            static void collect_hierarchy_order(const scene &source, std::vector<ecs::entity> &out);
    };
}
