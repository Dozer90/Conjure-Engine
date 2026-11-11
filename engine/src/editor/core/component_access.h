#pragma once

#include "conjure/editor/editor_context.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/scene.h"

#include <string>
#include <string_view>
#include <vector>

// Component access for the commands (docs/design/04_editor.md 3.5, R40): type names resolved through the
// component registry (exact, alias, unique suffix), the component as JSON through ecs::component_to_json /
// component_from_json, and the protocol form of entity references (guid strings instead of the ECS archive
// encoding) applied on the way in and out.

namespace conjure::editor::access {

    /// <summary>
    /// The registered component id for a protocol type name: the exact registered name, one of the aliases
    /// ("transform", "name", "script", "sprite_renderer", "camera", ...) or the unique registered name ending
    /// in ".<name>". TYPE_NOT_FOUND when nothing matches; INVALID_ARGS with details.candidates when several do.
    /// </summary>
    [[nodiscard]] core::result<ecs::component_id> resolve_component_type(
      const ecs::component_registry &registry,
      std::string_view name,
      core::json::document *details = nullptr
    );

    /// <summary>
    /// The registered name of a component ("conjure.scene.local_transform").
    /// </summary>
    [[nodiscard]] std::string component_name(const ecs::component_registry &registry, ecs::component_id id);

    /// <summary>
    /// The reflection description of a component, or nullptr (unreflected types such as the script component).
    /// </summary>
    [[nodiscard]] const core::reflect::type_info *reflection_of(
      const ecs::component_registry &registry,
      ecs::component_id id
    );

    /// <summary>
    /// False for TRANSIENT and EDITOR_HIDDEN types (not shown, not settable).
    /// </summary>
    [[nodiscard]] bool is_editable(const ecs::component_info &info) noexcept;

    /// <summary>
    /// The editable component types an entity carries, in id order.
    /// </summary>
    [[nodiscard]] std::vector<ecs::component_id> components_of(const ecs::world &world, ecs::entity entity);

    /// <summary>
    /// The component's JSON in protocol form (entity references as guid strings or null).
    /// </summary>
    [[nodiscard]] core::result<core::json::document> read_component(
      const scene::scene &scene,
      ecs::entity entity,
      ecs::component_id id
    );

    /// <summary>
    /// Replaces the component's data from protocol-form JSON (missing members keep their value) and binds its
    /// asset references. COMPONENT_NOT_FOUND, INVALID_ARGS (with the field path) on malformed data.
    /// </summary>
    [[nodiscard]] core::result<void> write_component(
      scene::scene &scene,
      ecs::entity entity,
      ecs::component_id id,
      const core::json::value &external
    );

    /// <summary>
    /// Adds a component with its default value, then writes 'initial' when it is an object. INVALID_ARGS when
    /// the entity already carries it.
    /// </summary>
    [[nodiscard]] core::result<void> add_component(
      scene::scene &scene,
      ecs::entity entity,
      ecs::component_id id,
      const core::json::value &initial
    );

    /// <summary>
    /// Removes a component. COMPONENT_NOT_FOUND when absent.
    /// </summary>
    [[nodiscard]] core::result<void> remove_component(scene::scene &scene, ecs::entity entity, ecs::component_id id);

    /// <summary>
    /// Converts the archive encoding of entity fields ({"external":"i:g"}) to guid strings in place.
    /// </summary>
    void externalise_entity_refs(
      core::json::document &document,
      const core::reflect::type_info *type,
      const scene::scene &scene
    );

    /// <summary>
    /// Converts guid strings in entity fields back to the archive encoding in place.
    /// </summary>
    [[nodiscard]] core::result<void> internalise_entity_refs(
      core::json::document &document,
      const core::reflect::type_info *type,
      const scene::scene &scene
    );

    /// <summary>
    /// The aliases the resolver accepts, for editor.schema and the SKILL file: {alias, registeredName}.
    /// </summary>
    [[nodiscard]] core::json::document alias_table();
}
