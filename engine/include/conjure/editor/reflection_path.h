#pragma once

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Property paths (docs/design/04_editor.md 3.5, R40): "transform.position.x", "materials[1]",
// "script.fields.speed". The first segment names the component type (resolved by the command through the
// component registry), the rest walks the component's reflection description down to one value. Values move
// through JSON only: the component is serialised with ecs::component_to_json, patched at the JSON pointer
// the path resolves to, and written back with ecs::component_from_json.

namespace conjure::editor {

    struct path_segment {
        std::string name = "";
        std::optional<u32> index = std::nullopt;

        [[nodiscard]] bool operator==(const path_segment &) const = default;
    };

    /// <summary>
    /// A resolved property inside a component: the JSON pointer of the value in the component's JSON form
    /// plus the reflection description of the field when the component is reflected.
    /// </summary>
    struct field_ref {
        const core::reflect::type_info *type = nullptr;       // the component (nullptr when unreflected)
        // the innermost named field; nullptr for the whole component
        const core::reflect::field_info *field = nullptr;
        core::reflect::field_kind kind = core::reflect::field_kind::COUNT;   // kind of the value; COUNT = unknown
        std::string pointer = "";                             // RFC 6901 pointer; "" = the whole component
        std::string path = "";                                // normalised path ("transform.position.x")

        [[nodiscard]] bool is_whole_component() const noexcept {
            return pointer.empty();
        }
    };

    class CONJURE_API reflection_path {
        private:
            std::vector<path_segment> mSegments = {};

        public:
            reflection_path();
            ~reflection_path();

            reflection_path(const reflection_path &) = default;
            reflection_path &operator=(const reflection_path &) = default;
            reflection_path(reflection_path &&) noexcept = default;
            reflection_path &operator=(reflection_path &&) noexcept = default;

        public:
            /// <summary>
            /// Parses "a.b[2].c". INVALID_ARGS for an empty path, an empty segment ("a..b", a trailing dot),
            /// a malformed index ("a[x]", "a[", "a[-1]") or characters outside identifiers, dots and brackets.
            /// </summary>
            [[nodiscard]] static core::result<reflection_path> parse(std::string_view text);

            /// <summary>
            /// The first segment: the component type name as written.
            /// </summary>
            [[nodiscard]] std::string_view component() const noexcept;

            /// <summary>
            /// The segments after the component (empty = the whole component).
            /// </summary>
            [[nodiscard]] std::span<const path_segment> fields() const noexcept;

            [[nodiscard]] std::span<const path_segment> segments() const noexcept {
                return mSegments;
            }

            /// <summary>
            /// Folds the first segmentCount segments into one component segment joined with dots, for type
            /// names written in full ("conjure.renderer.sprite.size" -> component "conjure.renderer.sprite",
            /// field "size"). False, and no change, for fewer than two segments, more than the path has, or an
            /// indexed segment among them.
            /// </summary>
            bool merge_component(usize segmentCount);

            /// <summary>
            /// The normalised text ("a.b[2].c").
            /// </summary>
            [[nodiscard]] std::string to_string() const;

            /// <summary>
            /// Resolves the field segments against a component description. VEC2/3/4 and QUAT accept
            /// x, y, z, w (or an index), TRANSFORM accepts position, rotation and scale, MAT4 an index 0..15,
            /// ARRAY an index below the count, STRUCT its member names. Without a description (unreflected
            /// components such as the script component) names and indices map straight onto JSON members and
            /// elements. PATH_NOT_FOUND names the first segment that does not resolve.
            /// </summary>
            [[nodiscard]] core::result<field_ref> resolve(const core::reflect::type_info *componentType) const;
    };

    /// <summary>
    /// The value at the reference inside a component's JSON form. PATH_NOT_FOUND when the pointer does not
    /// resolve in the document (an unreflected component lacking the member).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<core::json::document> read_at(
      const core::json::value &componentJson,
      const field_ref &reference
    );

    /// <summary>
    /// Replaces the value at the reference inside a component's JSON form (the whole document for a
    /// whole-component reference). INVALID_ARGS when the pointer cannot be set.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> write_at(
      core::json::document &componentJson,
      const field_ref &reference,
      const core::json::value &value
    );
}
