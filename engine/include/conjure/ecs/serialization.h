#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/entity.h"

#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string>
#include <vector>

// Serialisation (docs/design/01_ecs.md 3.7, 4.8, R40).
//
// Archives serialise as {"entities":[{"components":{"<name>":{...}}}]}. Entity fields are written as the
// integer local index of the referenced entry, null for NULL_ENTITY and {"external":"<index>:<generation>"}
// for a reference outside the archive; reading inverts the mapping. Component names that no type in the
// registry resolves are preserved verbatim in the built-in unresolved_components component (id 0) and
// re-emitted by write_archive_json, so data authored with an unloaded module survives a round trip.
//
// The default serializer walks core::reflect::type_info fields: scalars, strings, string_id, uuid, the
// math types, enums, nested reflected structs and std::array of any of those through core::reflect;
// entity fields with the archive encoding above; asset_ref fields (reflection kind ASSET_REF) as the uuid
// string stored in the first 16 bytes of the handle, leaving the rest of the handle zero on read (the
// scene module's bind pass resolves them, R25).

namespace conjure::ecs {

    class component_registry;
    class entity_archive;
    class world;

    /// <summary>
    /// One component of an unknown type kept verbatim through a load/save round trip.
    /// </summary>
    struct unresolved_component {
        std::string name = {};
        std::string json = {};

        [[nodiscard]] bool operator==(const unresolved_component &) const = default;
    };

    /// <summary>
    /// The built-in component (id 0, EDITOR_HIDDEN, non-trivial) every registry registers so unknown
    /// component names on load are preserved and written back unchanged.
    /// </summary>
    struct unresolved_components {
        std::vector<unresolved_component> entries = {};

        [[nodiscard]] bool operator==(const unresolved_components &) const = default;
    };

    /// <summary>
    /// Registered name of the unresolved_components component.
    /// </summary>
    inline constexpr std::string_view UNRESOLVED_COMPONENTS_NAME = "conjure.ecs.unresolved_components";

    namespace detail {
        /// <summary>
        /// Writes a reflected component as a JSON object using the archive encoding for entity and
        /// asset reference fields.
        /// </summary>
        [[nodiscard]] core::result<void> write_reflected_component(
          const core::reflect::type_info &type,
          const void *component,
          core::json::writer &out
        );

        /// <summary>
        /// Reads a reflected component from a JSON object; missing members keep their value. Errors
        /// name the field path.
        /// </summary>
        [[nodiscard]] core::result<void> read_reflected_component(
          const core::reflect::type_info &type,
          void *component,
          const core::json::value &in
        );

        /// <summary>
        /// Rewrites every entity field a reflected type contains through the remap table.
        /// </summary>
        void remap_reflected_entities(const core::reflect::type_info &type, void *component, const entity_remap &map);

        /// <summary>
        /// Serializer hooks of the unresolved_components component: an object mapping each preserved
        /// name to its verbatim JSON.
        /// </summary>
        [[nodiscard]] core::result<void> write_unresolved_components(const void *component, core::json::writer &out);
        [[nodiscard]] core::result<void> read_unresolved_components(void *component, const core::json::value &in);

        template <typename ComponentType>
        [[nodiscard]] core::result<void> write_typed_component(const void *component, core::json::writer &out) {
            const core::reflect::type_info *type = core::reflect::type_info_of<ComponentType>();
            if (type == nullptr) {
                return core::error(core::error_code::INVALID_STATE, "component type has no reflection description");
            }
            return write_reflected_component(*type, component, out);
        }

        template <typename ComponentType>
        [[nodiscard]] core::result<void> read_typed_component(void *component, const core::json::value &in) {
            const core::reflect::type_info *type = core::reflect::type_info_of<ComponentType>();
            if (type == nullptr) {
                return core::error(core::error_code::INVALID_STATE, "component type has no reflection description");
            }
            return read_reflected_component(*type, component, in);
        }

        template <typename ComponentType>
        void remap_typed_component(void *component, const entity_remap &map) {
            const core::reflect::type_info *type = core::reflect::type_info_of<ComponentType>();
            if (type == nullptr) {
                return;
            }
            remap_reflected_entities(*type, component, map);
        }
    }

    /// <summary>
    /// The reflection-driven serializer of a CONJURE_REFLECT_STRUCT component type. Types without a
    /// description get hooks that fail with INVALID_STATE.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] component_serializer make_default_serializer() noexcept {
        component_serializer serializer = {};
        serializer.writeJson = &detail::write_typed_component<ComponentType>;
        serializer.readJson = &detail::read_typed_component<ComponentType>;
        serializer.remapEntities = &detail::remap_typed_component<ComponentType>;
        return serializer;
    }

    /// <summary>
    /// Writes one component instance through the type's serializer (custom hooks, else the reflection
    /// walker, else the unresolved_components encoding). SERIALIZATION_FAILED when the type cannot be
    /// serialised; TRANSIENT types write an empty object.
    /// </summary>
    [[nodiscard]] core::result<void> write_component_json(
      const component_info &info,
      const void *component,
      core::json::writer &out
    );

    /// <summary>
    /// Reads one component instance through the type's serializer. SERIALIZATION_FAILED with the
    /// failing field path when the JSON does not match the type.
    /// </summary>
    [[nodiscard]] core::result<void> read_component_json(
      const component_info &info,
      void *component,
      const core::json::value &in
    );

    /// <summary>
    /// Emits {"entities":[{"components":{"<name>":{...}}}]} for the archive. Unresolved components are
    /// re-emitted verbatim under their original names.
    /// </summary>
    [[nodiscard]] core::result<void> write_archive_json(const entity_archive &archive, core::json::writer &out);

    /// <summary>
    /// Parses an archive written by write_archive_json. Unknown component names are preserved in an
    /// unresolved_components component (one INFO log each); malformed component JSON fails the whole
    /// load with SERIALIZATION_FAILED and an "<entity index>/<component>/<field>" path; an entity
    /// reference outside the archive fails with ARCHIVE_CORRUPT.
    /// </summary>
    [[nodiscard]] core::result<entity_archive> read_archive_json(
      const core::json::value &in,
      const component_registry &registry
    );

    /// <summary>
    /// Serialises the live component 'id' of 'target' with its serializer (editor inspector and AI
    /// protocol, R40). Entity references are written as {"external":"<index>:<generation>"}.
    /// ENTITY_NOT_ALIVE / COMPONENT_NOT_PRESENT / UNKNOWN_COMPONENT when the component cannot be found.
    /// </summary>
    [[nodiscard]] core::result<void> component_to_json(
      const world &source,
      entity target,
      component_id id,
      core::json::writer &out
    );

    /// <summary>
    /// Updates the live component 'id' of 'owner' from JSON (missing members keep their value) and
    /// stamps its column version. Same lookup errors as component_to_json; SERIALIZATION_FAILED with
    /// the field path for malformed input.
    /// </summary>
    [[nodiscard]] core::result<void> component_from_json(
      world &target,
      entity owner,
      component_id id,
      const core::json::value &in
    );
}
