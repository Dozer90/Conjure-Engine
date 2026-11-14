#include "editor/core/component_access.h"

#include "conjure/editor/editor_error.h"

#include "conjure/core/json/writer.h"
#include "conjure/core/string_utils.h"
#include "conjure/ecs/serialization.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_serializer.h"

#include <array>
#include <format>
#include <utility>

namespace conjure::editor::access {

    namespace {
        using core::reflect::field_info;
        using core::reflect::field_kind;
        using core::reflect::type_info;

        struct alias_entry {
            std::string_view alias = "";
            std::string_view name = "";
        };

        constexpr std::array<alias_entry, 22> ALIASES = {
            alias_entry{"transform", "conjure.scene.local_transform"},
            alias_entry{"local_transform", "conjure.scene.local_transform"},
            alias_entry{"name", "conjure.scene.name"},
            alias_entry{"tag", "conjure.scene.tag"},
            alias_entry{"layer", "conjure.scene.layer"},
            alias_entry{"guid", "conjure.scene.guid"},
            alias_entry{"disabled", "conjure.scene.disabled"},
            alias_entry{"prefab_instance", "conjure.scene.prefab_instance"},
            alias_entry{"prefab_link", "conjure.scene.prefab_link"},
            alias_entry{"mesh_renderer", "conjure.renderer.mesh_renderer"},
            alias_entry{"sprite", "conjure.renderer.sprite"},
            alias_entry{"sprite_renderer", "conjure.renderer.sprite"},
            alias_entry{"camera", "conjure.renderer.camera"},
            alias_entry{"light", "conjure.renderer.light"},
            alias_entry{"environment", "conjure.renderer.environment"},
            alias_entry{"rigid_body", "conjure.physics.rigid_body"},
            alias_entry{"box_collider", "conjure.physics.box_collider"},
            alias_entry{"sphere_collider", "conjure.physics.sphere_collider"},
            alias_entry{"capsule_collider", "conjure.physics.capsule_collider"},
            alias_entry{"character_controller", "conjure.physics.character_controller"},
            alias_entry{"script", "conjure.scripting.script_component"},
            alias_entry{"script_component", "conjure.scripting.script_component"},
        };

        /// <summary>
        /// Walks a reflected type alongside its JSON form and calls 'visit(pointer)' for every ENTITY value.
        /// </summary>
        template <typename Visitor>
        void visit_entity_fields(const type_info &type, const std::string &prefix, Visitor &&visit) {
            for (const field_info &field : type.fields) {
                const std::string pointer = prefix + "/" + field.name;
                if (field.kind == field_kind::ENTITY) {
                    visit(pointer);
                } else if (field.kind == field_kind::STRUCT && field.structType != nullptr) {
                    visit_entity_fields(*field.structType, pointer, visit);
                } else if (field.kind == field_kind::ARRAY) {
                    for (u32 index = 0; index < field.arrayCount; ++index) {
                        const std::string element = std::format("{}/{}", pointer, index);
                        if (field.elementKind == field_kind::ENTITY) {
                            visit(element);
                        } else if (field.elementKind == field_kind::STRUCT && field.structType != nullptr) {
                            visit_entity_fields(*field.structType, element, visit);
                        }
                    }
                }
            }
        }

        [[nodiscard]] ecs::entity parse_external(std::string_view text) noexcept {
            const usize colon = text.find(':');
            if (colon == std::string_view::npos) {
                return ecs::NULL_ENTITY;
            }
            const std::optional<u64> index = core::str::parse_uint(text.substr(0, colon));
            const std::optional<u64> generation = core::str::parse_uint(text.substr(colon + 1));
            if (!index.has_value() || !generation.has_value()) {
                return ecs::NULL_ENTITY;
            }
            ecs::entity entity = {};
            entity.index = static_cast<u32>(*index);
            entity.generation = static_cast<u32>(*generation);
            return entity;
        }

        /// <summary>
        /// Writes the layer component through scene::set_layer, which owns the 0..31 range that every capture
        /// (snapshot, save, duplicate, prefab) relies on; the reflected range is only an inspector hint. The value
        /// is read into a copy first, so a refused write leaves the component as it was.
        /// </summary>
        [[nodiscard]] core::result<void> write_layer(
          scene::scene &scene,
          ecs::entity entity,
          const ecs::component_info &info,
          const core::json::value &in
        ) {
            const ecs::world &world = scene.world();
            const scene::layer *current = world.get<scene::layer>(entity);
            scene::layer value = current != nullptr ? *current : scene::layer{};
            const core::result<void> read = ecs::read_component_json(info, &value, in);
            if (!read.has_value()) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, read.get_error().message());
            }
            const core::result<void> layered = scene.set_layer(entity, value.index);
            if (!layered.has_value()) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, layered.get_error().message());
            }
            return core::result<void>::success();
        }
    }

    core::result<ecs::component_id> resolve_component_type(
      const ecs::component_registry &registry,
      std::string_view name,
      core::json::document *details
    ) {
        if (name.empty()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "a component type name must not be empty");
        }
        const ecs::component_id exact = registry.find(core::string_id(name));
        if (exact.is_valid()) {
            return exact;
        }
        for (const alias_entry &entry : ALIASES) {
            if (entry.alias == name) {
                const ecs::component_id aliased = registry.find(core::string_id(entry.name));
                if (aliased.is_valid()) {
                    return aliased;
                }
                return make_editor_error(
                  protocol_error_code::TYPE_NOT_FOUND,
                  std::format("component type '{}' ({}) is not registered in this build", name, entry.name)
                );
            }
        }
        // Unique suffix match: "rigid_body" -> "conjure.physics.rigid_body".
        const std::string suffix = "." + std::string(name);
        std::vector<const ecs::component_info *> matches = {};
        for (const ecs::component_info &info : registry.types()) {
            const std::string_view registered = info.name.view();
            if (core::str::ends_with(registered, suffix)) {
                matches.push_back(&info);
            }
        }
        if (matches.size() == 1) {
            return matches.front()->id;
        }
        if (matches.size() > 1) {
            if (details != nullptr) {
                core::json::document candidates = core::json::document::array();
                for (const ecs::component_info *info : matches) {
                    candidates.push_back(info->name.view());
                }
                details->set("candidates", candidates);
            }
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              std::format("component type '{}' is ambiguous ({} registered types end with it)", name, matches.size())
            );
        }
        return make_editor_error(protocol_error_code::TYPE_NOT_FOUND, std::format("unknown component type '{}'", name));
    }

    std::string component_name(const ecs::component_registry &registry, ecs::component_id id) {
        if (!registry.is_registered(id)) {
            return "";
        }
        return registry.info(id).name.to_string();
    }

    const type_info *reflection_of(const ecs::component_registry &registry, ecs::component_id id) {
        if (!registry.is_registered(id)) {
            return nullptr;
        }
        return registry.info(id).reflection;
    }

    bool is_editable(const ecs::component_info &info) noexcept {
        return !info.is_transient() && !has_flag(info.flags, ecs::component_flags::EDITOR_HIDDEN);
    }

    std::vector<ecs::component_id> components_of(const ecs::world &world, ecs::entity entity) {
        std::vector<ecs::component_id> ids = {};
        if (!world.is_alive(entity)) {
            return ids;
        }
        const ecs::component_registry &registry = world.registry();
        world.mask_of(entity).for_each_set([&](ecs::component_id id) {
            if (registry.is_registered(id) && is_editable(registry.info(id))) {
                ids.push_back(id);
            }
        });
        return ids;
    }

    core::result<core::json::document> read_component(
      const scene::scene &scene,
      ecs::entity entity,
      ecs::component_id id
    ) {
        const ecs::world &world = scene.world();
        if (!world.is_alive(entity)) {
            return make_editor_error(
              protocol_error_code::ENTITY_NOT_FOUND,
              std::format("entity {} is not alive", entity)
            );
        }
        if (!world.has_raw(entity, id)) {
            return make_editor_error(
              protocol_error_code::COMPONENT_NOT_FOUND,
              std::format("entity {} has no '{}' component", entity, component_name(world.registry(), id))
            );
        }
        const ecs::component_info &info = world.registry().info(id);
        if (info.is_tag()) {
            return core::json::document::object();
        }
        core::json::writer out = {};
        const core::result<void> written = ecs::component_to_json(world, entity, id, out);
        if (!written.has_value()) {
            return make_editor_error(protocol_error_code::INTERNAL, written.get_error().message());
        }
        core::result<core::json::document> parsed = core::json::document::parse(out.finish());
        if (!parsed.has_value()) {
            return make_editor_error(protocol_error_code::INTERNAL, parsed.get_error().message());
        }
        core::json::document document = std::move(parsed).value();
        if (!document.root().is_object()) {
            document = core::json::document::object();
        }
        externalise_entity_refs(document, info.reflection, scene);
        return document;
    }

    core::result<void> write_component(
      scene::scene &scene,
      ecs::entity entity,
      ecs::component_id id,
      const core::json::value &external
    ) {
        ecs::world &world = scene.world();
        if (!world.is_alive(entity)) {
            return make_editor_error(
              protocol_error_code::ENTITY_NOT_FOUND,
              std::format("entity {} is not alive", entity)
            );
        }
        if (!world.has_raw(entity, id)) {
            return make_editor_error(
              protocol_error_code::COMPONENT_NOT_FOUND,
              std::format("entity {} has no '{}' component", entity, component_name(world.registry(), id))
            );
        }
        if (!external.is_object()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "a component value is a JSON object");
        }
        const ecs::component_info &info = world.registry().info(id);
        if (info.is_tag()) {
            return core::result<void>::success();
        }
        core::json::document internal = core::json::document::from_value(external);
        CONJURE_TRY(internalise_entity_refs(internal, info.reflection, scene));
        if (id == scene.ids().layer) {
            return write_layer(scene, entity, info, internal.root());
        }
        const core::result<void> applied = ecs::component_from_json(world, entity, id, internal.root());
        if (!applied.has_value()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, applied.get_error().message());
        }
        if (scene.assets() != nullptr) {
            scene::scene_serializer::bind_entity(world, entity, *scene.assets());
        }
        return core::result<void>::success();
    }

    core::result<void> add_component(
      scene::scene &scene,
      ecs::entity entity,
      ecs::component_id id,
      const core::json::value &initial
    ) {
        ecs::world &world = scene.world();
        if (!world.is_alive(entity)) {
            return make_editor_error(
              protocol_error_code::ENTITY_NOT_FOUND,
              std::format("entity {} is not alive", entity)
            );
        }
        if (world.has_raw(entity, id)) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              std::format("entity {} already has a '{}' component", entity, component_name(world.registry(), id))
            );
        }
        const ecs::component_info &info = world.registry().info(id);
        if (id == scene.ids().localTransform) {
            CONJURE_TRY(scene.add_transform(entity));
        } else {
            const core::result<void *> added = world.add_raw(entity, id);
            if (!added.has_value()) {
                return make_editor_error(classify_error(added.get_error()), added.get_error().message());
            }
        }
        if (initial.is_object() && !info.is_tag() && initial.size() > 0) {
            const core::result<void> written = write_component(scene, entity, id, initial);
            if (!written.has_value()) {
                // All or nothing: a malformed initial value leaves the entity as it was.
                const core::result<void> removed = world.remove_raw(entity, id);
                (void) removed;
                return written;
            }
        }
        return core::result<void>::success();
    }

    core::result<void> remove_component(scene::scene &scene, ecs::entity entity, ecs::component_id id) {
        ecs::world &world = scene.world();
        if (!world.is_alive(entity)) {
            return make_editor_error(
              protocol_error_code::ENTITY_NOT_FOUND,
              std::format("entity {} is not alive", entity)
            );
        }
        if (!world.has_raw(entity, id)) {
            return make_editor_error(
              protocol_error_code::COMPONENT_NOT_FOUND,
              std::format("entity {} has no '{}' component", entity, component_name(world.registry(), id))
            );
        }
        const core::result<void> removed = world.remove_raw(entity, id);
        if (!removed.has_value()) {
            return make_editor_error(classify_error(removed.get_error()), removed.get_error().message());
        }
        return core::result<void>::success();
    }

    void externalise_entity_refs(core::json::document &document, const type_info *type, const scene::scene &scene) {
        if (type == nullptr) {
            return;
        }
        visit_entity_fields(*type, "", [&](const std::string &pointer) {
            const core::json::value current = document.at_pointer(pointer);
            if (!current.is_valid()) {
                return;
            }
            ecs::entity entity = ecs::NULL_ENTITY;
            if (current.is_object()) {
                entity = parse_external(current["external"].as_string());
            } else if (current.is_integer()) {
                entity = ecs::entity::from_u64(current.as_u64());
            }
            if (entity.is_null() || !scene.contains(entity)) {
                document.set_pointer(pointer, nullptr);
                return;
            }
            const std::optional<core::uuid> guid = scene.guid_of(entity);
            if (guid.has_value()) {
                document.set_pointer(pointer, guid->to_string());
            } else {
                document.set_pointer(pointer, nullptr);
            }
        });
    }

    core::result<void> internalise_entity_refs(
      core::json::document &document,
      const type_info *type,
      const scene::scene &scene
    ) {
        if (type == nullptr) {
            return core::result<void>::success();
        }
        std::optional<core::error> failure = std::nullopt;
        visit_entity_fields(*type, "", [&](const std::string &pointer) {
            if (failure.has_value()) {
                return;
            }
            const core::json::value current = document.at_pointer(pointer);
            if (!current.is_valid()) {
                return;     // absent: keeps its value
            }
            if (current.is_null()) {
                return;
            }
            // Only guid strings cross the protocol (D5): the archive's {"external": "index:generation"} form
            // would let a client plant stale or foreign ecs::entity ids in a component.
            if (!current.is_string()) {
                failure = make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("{}: an entity reference is a guid string or null", pointer)
                );
                return;
            }
            const std::optional<core::uuid> guid = core::uuid::parse(current.as_string());
            if (!guid.has_value()) {
                failure = make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("{}: '{}' is not a guid", pointer, current.as_string())
                );
                return;
            }
            const ecs::entity entity = scene.find_guid(*guid);
            if (entity.is_null()) {
                failure = make_editor_error(
                  protocol_error_code::ENTITY_NOT_FOUND,
                  std::format("{}: no entity carries guid {}", pointer, guid->to_string())
                );
                return;
            }
            document.set_pointer(
              pointer,
              core::json::object({{"external", std::format("{}:{}", entity.index, entity.generation)}})
            );
        });
        if (failure.has_value()) {
            return *failure;
        }
        return core::result<void>::success();
    }

    core::json::document alias_table() {
        core::json::document table = core::json::document::object();
        for (const alias_entry &entry : ALIASES) {
            table.set(entry.alias, entry.name);
        }
        return table;
    }
}
