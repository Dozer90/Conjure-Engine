#include "conjure/editor/entity_ref.h"

#include "conjure/editor/editor_error.h"

#include "conjure/core/string_utils.h"

#include <format>

namespace conjure::editor {

    namespace {
        [[nodiscard]] core::result<ecs::entity> resolve_path(
          std::string_view path,
          const scene::scene &target,
          core::json::document *details
        ) {
            const std::vector<std::string_view> segments = core::str::split(path, '/', false);
            if (segments.empty()) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, "an entity path must not be empty");
            }
            ecs::entity current = ecs::NULL_ENTITY;
            std::string walked = "";
            for (const std::string_view segment : segments) {
                const core::string_id wanted(segment);
                std::vector<ecs::entity> matches = {};
                if (current.is_null()) {
                    for (const scene::root_entry &root : target.roots()) {
                        if (target.name_of(root.entity) == wanted) {
                            matches.push_back(root.entity);
                        }
                    }
                } else {
                    for (const ecs::entity child : target.tree().children_of(current)) {
                        if (target.name_of(child) == wanted) {
                            matches.push_back(child);
                        }
                    }
                }
                if (!walked.empty()) {
                    walked += '/';
                }
                walked += std::string(segment);
                if (matches.empty()) {
                    return make_editor_error(
                      protocol_error_code::ENTITY_NOT_FOUND,
                      std::format("no entity '{}' (segment '{}' did not match)", path, segment)
                    );
                }
                if (matches.size() > 1) {
                    if (details != nullptr) {
                        core::json::document candidates = core::json::document::array();
                        for (const ecs::entity candidate : matches) {
                            const std::optional<core::uuid> guid = target.guid_of(candidate);
                            candidates.push_back(guid.has_value() ? guid->to_string() : std::string(""));
                        }
                        details->set("candidates", candidates);
                    }
                    return make_editor_error(
                      protocol_error_code::INVALID_ARGS,
                      std::format("path '{}' is ambiguous: {} entities are named '{}'", walked, matches.size(), segment)
                    );
                }
                current = matches.front();
            }
            return current;
        }
    }

    core::result<ecs::entity> entity_ref::resolve(
      const core::json::value &reference,
      const scene::scene &target,
      core::json::document *details
    ) {
        if (!reference.is_string()) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              "an entity reference is a guid string or a hierarchy path"
            );
        }
        const std::string_view text = reference.as_string();
        if (text.empty()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "an entity reference must not be empty");
        }
        const std::optional<core::uuid> guid = core::uuid::parse(text);
        if (guid.has_value()) {
            const ecs::entity found = target.find_guid(*guid);
            if (found.is_null() || !target.contains(found)) {
                return make_editor_error(
                  protocol_error_code::ENTITY_NOT_FOUND,
                  std::format("no live entity carries guid {}", guid->to_string())
                );
            }
            return found;
        }
        return resolve_path(text, target, details);
    }

    core::result<std::vector<ecs::entity>> entity_ref::resolve_many(
      const core::json::value &references,
      const scene::scene &target,
      core::json::document *details
    ) {
        if (!references.is_array()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "expected an array of entity references");
        }
        std::vector<ecs::entity> entities = {};
        entities.reserve(references.size());
        for (const core::json::value reference : references) {
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve(reference, target, details));
            entities.push_back(entity);
        }
        return entities;
    }

    core::result<core::uuid> entity_ref::guid_of(scene::scene &target, ecs::entity entity) {
        if (!target.contains(entity)) {
            return make_editor_error(
              protocol_error_code::ENTITY_NOT_FOUND,
              std::format("entity {} is not alive", entity)
            );
        }
        const core::result<core::uuid> guid = target.ensure_guid(entity);
        if (!guid.has_value()) {
            return make_editor_error(protocol_error_code::ENTITY_NOT_FOUND, guid.get_error().message());
        }
        return guid.value();
    }

    std::string entity_ref::to_json(const core::uuid &guid) {
        return guid.to_string();
    }

    std::string entity_ref::path_of(const scene::scene &target, ecs::entity entity) {
        std::vector<std::string> parts = {};
        ecs::entity current = entity;
        while (!current.is_null() && target.contains(current)) {
            const core::string_id name = target.name_of(current);
            if (name.is_null()) {
                parts.push_back(std::format("#{}", current.index));
            } else {
                parts.push_back(name.to_string());
            }
            current = target.tree().parent_of(current);
        }
        std::string path = "";
        for (usize index = parts.size(); index > 0; --index) {
            if (!path.empty()) {
                path += '/';
            }
            path += parts[index - 1];
        }
        return path;
    }

    bool entity_ref::looks_like_guid(std::string_view text) noexcept {
        return core::uuid::parse(text).has_value();
    }
}
