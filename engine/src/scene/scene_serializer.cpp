#include "conjure/scene/scene_serializer.h"

#include "scene/archive_tools.h"
#include "scene/asset_ref_raw.h"
#include "scene/scene_json.h"

#include "conjure/scene/log_category.h"
#include "conjure/scene/prefab.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/ecs/serialization.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace conjure::scene {

namespace {

    [[nodiscard]] core::result<scene_document> document_of(const scene &source) {
        scene_document document = {};
        document.name = std::string(source.name());
        document.settings = source.settings();
        for (const root_entry &entry : source.roots()) {
            const std::optional<core::uuid> id = source.guid_of(entry.entity);
            if (id.has_value()) {
                document.roots.push_back(*id);
            } else {
                CONJURE_LOG_WARN(LOG_SCENE, "root {} has no guid and is not listed in the file header", entry.entity);
            }
        }
        std::vector<ecs::entity> entities = {};
        scene_serializer::collect_hierarchy_order(source, entities);
        CONJURE_TRY_ASSIGN(ecs::entity_archive archive, ecs::entity_archive::capture(source.world(), entities));
        // A file that loading would refuse is never written.
        CONJURE_TRY(detail::check_layers(archive, source.ids(), entities));
        document.archive = std::make_shared<const ecs::entity_archive>(std::move(archive));
        return document;
    }

    void write_roots(const std::vector<core::uuid> &roots, core::json::writer &out) {
        out.key("roots");
        out.begin_array();
        for (const core::uuid &id : roots) {
            out.value(id.to_string());
        }
        out.end_array();
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Writing

core::result<void> scene_serializer::write_document(const scene_document &document, core::json::writer &out) {
    if (document.archive == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "scene document without an archive");
    }
    out.begin_object();
    detail::write_header(out, SCENE_FORMAT_NAME, document.name);
    out.key("settings");
    out.write_raw_json(detail::canonical_settings(document.settings));
    write_roots(document.roots, out);
    out.key("archive");
    CONJURE_TRY(ecs::write_archive_json(*document.archive, out));
    out.end_object();
    return core::result<void>::success();
}

core::result<void> scene_serializer::write_prefab(const prefab_document &document, core::json::writer &out) {
    if (document.archive == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "prefab document without an archive");
    }
    out.begin_object();
    detail::write_header(out, PREFAB_FORMAT_NAME, document.name);
    out.key("archive");
    CONJURE_TRY(ecs::write_archive_json(*document.archive, out));
    out.end_object();
    return core::result<void>::success();
}

core::result<void> scene_serializer::save_scene(const scene &source, core::json::writer &out) {
    CONJURE_TRY_ASSIGN(const scene_document document, document_of(source));
    return write_document(document, out);
}

core::result<void> scene_serializer::save_scene_file(
  const scene &source,
  core::vfs::file_system &fs,
  std::string_view path
) {
    core::json::writer out = {};
    CONJURE_TRY(save_scene(source, out));
    const std::string text = out.finish();
    return fs.write_file_atomic(
      path,
      std::span<const std::byte>(reinterpret_cast<const std::byte *>(text.data()), text.size())
    );
}

core::result<void> scene_serializer::save_subtree(const scene &source, ecs::entity root, core::json::writer &out) {
    CONJURE_TRY_ASSIGN(
      const prefab_document document,
      prefab::build_document(source, root, source.name_of(root).view())
    );
    return write_prefab(document, out);
}


/*===========================================================================*/
/*===========================================================================*/
// Parsing

core::result<scene_document> scene_serializer::parse_document(
  const core::json::value &in,
  const ecs::component_registry &registry
) {
    CONJURE_TRY_ASSIGN(const detail::file_header header, detail::read_header(in, SCENE_FORMAT_NAME));
    scene_document document = {};
    document.name = header.name;

    const core::json::value settings = in["settings"];
    if (settings.is_valid() && !settings.is_null()) {
        if (!settings.is_object()) {
            return core::make_error(scene_error::BAD_FORMAT, "\"settings\" must be an object");
        }
        document.settings = core::json::document::from_value(settings);
    }

    const core::json::value roots = in["roots"];
    if (roots.is_valid() && !roots.is_null()) {
        if (!roots.is_array()) {
            return core::make_error(scene_error::BAD_FORMAT, "\"roots\" must be an array of guid strings");
        }
        for (const core::json::value item : roots) {
            const std::optional<core::uuid> id = core::uuid::parse(item.as_string());
            if (!item.is_string() || !id.has_value()) {
                return core::make_error(scene_error::BAD_FORMAT, "\"roots\" holds a value that is not a guid");
            }
            document.roots.push_back(*id);
        }
    }

    const core::json::value archive = in["archive"];
    if (!archive.is_object()) {
        return core::make_error(scene_error::BAD_FORMAT, "the file has no \"archive\" object");
    }
    CONJURE_TRY_ASSIGN(ecs::entity_archive parsed, ecs::read_archive_json(archive, registry));
    const u32 unknown = detail::count_unresolved(parsed);
    if (unknown > 0) {
        CONJURE_LOG_WARN(LOG_SCENE, "scene '{}' preserves {} unknown component(s)", document.name, unknown);
    }
    document.archive = std::make_shared<const ecs::entity_archive>(std::move(parsed));
    return document;
}

core::result<prefab_document> scene_serializer::parse_prefab(
  const core::json::value &in,
  const ecs::component_registry &registry
) {
    CONJURE_TRY_ASSIGN(const detail::file_header header, detail::read_header(in, PREFAB_FORMAT_NAME));
    prefab_document document = {};
    document.name = header.name;

    const core::json::value archive = in["archive"];
    if (!archive.is_object()) {
        return core::make_error(scene_error::BAD_FORMAT, "the file has no \"archive\" object");
    }
    CONJURE_TRY_ASSIGN(ecs::entity_archive parsed, ecs::read_archive_json(archive, registry));
    const scene_component_ids ids = scene_component_ids::resolve(registry);
    CONJURE_TRY(detail::check_prefab_roots(parsed, ids, "prefab '{}'", document.name));
    // Entry guids are the identities prefab_link points at: two entries sharing one would make diff and
    // apply match the wrong entity.
    CONJURE_TRY(detail::check_unique_guids(parsed, ids));
    const u32 unknown = detail::count_unresolved(parsed);
    if (unknown > 0) {
        CONJURE_LOG_WARN(LOG_SCENE, "prefab '{}' preserves {} unknown component(s)", document.name, unknown);
    }
    document.archive = std::make_shared<const ecs::entity_archive>(std::move(parsed));
    return document;
}


/*===========================================================================*/
/*===========================================================================*/
// Loading

core::result<instantiate_report> scene_serializer::load_scene(
  scene &target,
  const scene_document &document,
  scene_id owner,
  guid_policy guids
) {
    if (document.archive == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "scene document without an archive");
    }
    instantiate_options options = {};
    options.owner = owner;
    options.guids = guids;
    ecs::entity root = {};
    CONJURE_TRY_ASSIGN(
      instantiate_report report,
      target.instantiate_archive(document.archive, 1, options, std::span<ecs::entity>(&root, 1))
    );

    // Root order from the header: listed guids first, then unlisted roots in archive order.
    const scene_component_ids &ids = target.ids();
    const std::vector<u32> rootEntries = detail::archive_roots(*document.archive, ids);
    std::vector<ecs::entity> ordered = {};
    std::vector<bool> placed(rootEntries.size(), false);
    for (const core::uuid &id : document.roots) {
        bool found = false;
        for (usize slot = 0; slot < rootEntries.size(); ++slot) {
            if (placed[slot]) {
                continue;
            }
            const std::optional<core::uuid> entryId = detail::entry_guid(*document.archive, rootEntries[slot], ids);
            if (entryId.has_value() && *entryId == id) {
                ordered.push_back(report.entities[rootEntries[slot]]);
                placed[slot] = true;
                found = true;
                break;
            }
        }
        if (!found) {
            CONJURE_LOG_WARN(LOG_SCENE, "scene '{}': root guid {} is not in the archive and is skipped", document.name,
              id);
        }
    }
    for (usize slot = 0; slot < rootEntries.size(); ++slot) {
        if (placed[slot]) {
            continue;
        }
        const ecs::entity entity = report.entities[rootEntries[slot]];
        if (target.root_owner(entity).has_value()) {
            CONJURE_LOG_WARN(LOG_SCENE, "scene '{}': root entity {} is missing from the header and is appended",
              document.name, entity);
            ordered.push_back(entity);
        }
    }
    std::vector<ecs::entity> presentRoots = {};
    for (const ecs::entity entity : ordered) {
        if (target.root_owner(entity).has_value()) {
            presentRoots.push_back(entity);
        }
    }
    CONJURE_TRY(target.set_root_order(presentRoots));
    return report;
}

core::result<instantiate_report> scene_serializer::load_scene_file(
  scene &target,
  core::vfs::file_system &fs,
  std::string_view path,
  scene_id owner,
  guid_policy guids
) {
    CONJURE_TRY_ASSIGN(const std::string text, fs.read_text_file(path));
    CONJURE_TRY_ASSIGN(const scene_document document, detail::parse_scene_text(text, target.world().registry()));
    return load_scene(target, document, owner, guids);
}

core::result<ecs::entity> scene_serializer::instantiate_subtree(
  scene &target,
  const core::json::value &in,
  ecs::entity parent,
  bool keepGuids,
  u32 index
) {
    CONJURE_TRY_ASSIGN(const prefab_document document, parse_prefab(in, target.world().registry()));
    instantiate_options options = {};
    options.parent = parent;
    options.index = index;
    options.guids = keepGuids ? guid_policy::KEEP : guid_policy::REGENERATE_ALL;
    ecs::entity root = {};
    CONJURE_TRY(target.instantiate_archive(document.archive, 1, options, std::span<ecs::entity>(&root, 1)));
    return root;
}


/*===========================================================================*/
/*===========================================================================*/
// Archives and binding

core::result<ecs::entity_archive> scene_serializer::capture_subtree(
  const scene &source,
  ecs::entity root,
  ecs::archive_options options
) {
    if (!source.contains(root)) {
        return core::make_error(scene_error::INVALID_ENTITY, "capture_subtree: entity {} is not alive", root);
    }
    std::vector<ecs::entity> entities = {};
    source.tree().collect_subtree(root, entities);
    CONJURE_TRY_ASSIGN(ecs::entity_archive captured, ecs::entity_archive::capture(source.world(), entities, options));
    // Duplicates, prefab documents and the editor's undo payloads are instantiated later: refuse a layer that
    // instantiation would refuse while the subtree still exists.
    CONJURE_TRY(detail::check_layers(captured, source.ids(), entities));
    if (options.keepExternalReferences || !captured.entries().front().mask.test(source.ids().parent)) {
        return captured;
    }
    // Entry 0 is a true root of the archive: drop the link to the outside parent.
    const ecs::component_registry &registry = source.world().registry();
    const ecs::component_id parentId = source.ids().parent;
    return detail::clone_archive(captured, [&](u32 index, ecs::archive_entry &entry) {
        if (index == 0) {
            detail::remove_entry_component(registry, entry, parentId);
        }
    });
}

void scene_serializer::bind_asset_refs(
  ecs::world &target,
  std::span<const ecs::entity> entities,
  assets::asset_manager &assets,
  std::vector<assets::asset_id> *outIds
) {
    const ecs::component_registry &registry = target.registry();
    for (const ecs::entity entity : entities) {
        if (!target.is_alive(entity)) {
            continue;
        }
        const ecs::component_mask mask = target.mask_of(entity);
        mask.for_each_set([&](ecs::component_id id) {
            const ecs::component_info &info = registry.info(id);
            if (info.assetRefFieldOffsets.empty()) {
                return;
            }
            std::byte *bytes = static_cast<std::byte *>(target.get_raw(entity, id, true));
            if (bytes == nullptr) {
                return;
            }
            for (const u32 offset : info.assetRefFieldOffsets) {
                if (offset + sizeof(detail::asset_ref_raw) > info.size) {
                    continue;
                }
                detail::asset_ref_raw raw = {};
                std::memcpy(&raw, bytes + offset, sizeof(raw));
                if (raw.id.is_null()) {
                    continue;
                }
                raw.slot = assets.bind_slot(raw.id);
                std::memcpy(bytes + offset, &raw, sizeof(raw));
                if (outIds != nullptr) {
                    outIds->push_back(raw.id);
                }
            }
        });
    }
}

void scene_serializer::bind_entity(ecs::world &target, ecs::entity entity, assets::asset_manager &assets) {
    bind_asset_refs(target, std::span<const ecs::entity>(&entity, 1), assets, nullptr);
}

void scene_serializer::collect_hierarchy_order(const scene &source, std::vector<ecs::entity> &out) {
    for (const root_entry &entry : source.roots()) {
        source.tree().collect_subtree(entry.entity, out);
    }
}
}
