#pragma once

#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// JSON conversion driven by type_info.
//
// Representation of every kind (the writer emits object members in field order; use
// document::to_canonical_string() for sorted keys):
//   BOOL -> true/false            integers -> numbers           F32/F64 -> numbers (non-finite -> null)
//   STRING -> string              STRING_ID -> the interned name, or "#" + 16 hex digits when unknown
//   UUID -> "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"              VEC2/3/4 -> [x, y(, z(, w))]
//   QUAT -> [x, y, z, w] (storage order)                        MAT4 -> 16 numbers, column-major
//   TRANSFORM -> {"position": [...], "rotation": [x, y, z, w], "scale": [...]}
//   ENUM -> the enumerator name (flags: "A|B"), or the number when no name spells the value
//   STRUCT -> object of its fields                              ARRAY -> array of arrayCount elements
//   ENTITY / ASSET_REF -> the raw handle: an unsigned number for 1, 2, 4 and 8 byte handles, a uuid
//   string for 16 byte handles and for an ASSET_REF wider than 16 bytes, whose leading 16 bytes are
//   the uuid (assets::asset_ref = uuid + slot pointer; the pointer reads back as null, R25). Other
//   sizes fail with UNSUPPORTED. Modules that need a richer representation remap these fields after
//   loading.
//
// Reading is lenient where it is safe: a missing member keeps the field's current value, unknown
// members are ignored (and reported when a list is supplied), a null for a float reads as NaN, an
// integral real (3.0) is accepted for an integer, a shorter array fills only the leading elements,
// and an enum accepts its name or its number. Everything else (wrong JSON type, out-of-range number,
// unknown enumerator, array longer than the field) fails with INVALID_ARGUMENT / OUT_OF_RANGE and a
// message naming the field path; fields read before the failure keep their new values.

namespace conjure::core::json {

    template <>
    struct CONJURE_API json_converter<string_id> {
        /// <summary>
        /// A string is the name ("#" + 16 hex digits is a raw hash); an unsigned number is the hash.
        /// </summary>
        [[nodiscard]] static result<string_id> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<math::vec2> {
        [[nodiscard]] static result<math::vec2> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<math::vec3> {
        [[nodiscard]] static result<math::vec3> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<math::vec4> {
        [[nodiscard]] static result<math::vec4> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<math::quat> {
        /// <summary>
        /// Reads [x, y, z, w].
        /// </summary>
        [[nodiscard]] static result<math::quat> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<math::mat4> {
        /// <summary>
        /// Reads 16 column-major numbers.
        /// </summary>
        [[nodiscard]] static result<math::mat4> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<math::transform> {
        /// <summary>
        /// Reads {"position", "rotation", "scale"}; absent members keep the identity.
        /// </summary>
        [[nodiscard]] static result<math::transform> from_json(const value &source);
    };
}

namespace conjure::core::reflect {

    /// <summary>
    /// Writes one math value in the representation documented above.
    /// </summary>
    CONJURE_API void write_json(const math::vec2 &value, json::writer &out);
    CONJURE_API void write_json(const math::vec3 &value, json::writer &out);
    CONJURE_API void write_json(const math::vec4 &value, json::writer &out);
    CONJURE_API void write_json(const math::quat &value, json::writer &out);
    CONJURE_API void write_json(const math::mat4 &value, json::writer &out);
    CONJURE_API void write_json(const math::transform &value, json::writer &out);
    CONJURE_API void write_json(const string_id &value, json::writer &out);

    /// <summary>
    /// Serialises 'object' (an instance of 'type') as a JSON object into 'out', fields in
    /// declaration order. Fails with INVALID_STATE for a STRUCT/ENUM field without its description
    /// and UNSUPPORTED for a custom kind of unsupported size; the writer may then be left mid-object.
    /// </summary>
    [[nodiscard]] CONJURE_API result<void> to_json(const type_info &type, const void *object, json::writer &out);

    /// <summary>
    /// Updates 'object' from a JSON object. Missing members keep their current value; unknown
    /// members are appended to 'unknownFields' (dotted paths) when it is not null. Stops at the
    /// first failing field.
    /// </summary>
    [[nodiscard]] CONJURE_API result<void> from_json(
      const type_info &type,
      void *object,
      const json::value &in,
      std::vector<std::string> *unknownFields = nullptr
    );

    /// <summary>
    /// to_json() into a new document.
    /// </summary>
    [[nodiscard]] CONJURE_API result<json::document> to_json_document(const type_info &type, const void *object);

    /// <summary>
    /// to_json() for a CONJURE_REFLECT_STRUCT type. INVALID_STATE when the type has no registered
    /// description.
    /// </summary>
    template <typename T>
    [[nodiscard]] result<void> to_json(const T &object, json::writer &out) {
        const type_info *type = type_info_of<T>();
        if (type == nullptr) {
            return error(error_code::INVALID_STATE, "type has no registered reflection description");
        }
        return to_json(*type, &object, out);
    }

    /// <summary>
    /// from_json() for a CONJURE_REFLECT_STRUCT type.
    /// </summary>
    template <typename T>
    [[nodiscard]] result<void> from_json(
      T &object,
      const json::value &in,
      std::vector<std::string> *unknownFields = nullptr
    ) {
        const type_info *type = type_info_of<T>();
        if (type == nullptr) {
            return error(error_code::INVALID_STATE, "type has no registered reflection description");
        }
        return from_json(*type, &object, in, unknownFields);
    }

    /// <summary>
    /// to_json_document() for a CONJURE_REFLECT_STRUCT type.
    /// </summary>
    template <typename T>
    [[nodiscard]] result<json::document> to_json_document(const T &object) {
        const type_info *type = type_info_of<T>();
        if (type == nullptr) {
            return error(error_code::INVALID_STATE, "type has no registered reflection description");
        }
        return to_json_document(*type, &object);
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Field access by path

    /// <summary>
    /// A resolved field inside an object: the data address plus its description. When the path
    /// ended in an array index the reference designates one element and kind() is the element kind.
    /// 'path' is the normalised path resolve_path() followed ("parts[1].weight"); error messages of
    /// write_json/set_from_json/get_as_json use it, falling back to the field name when it is empty.
    /// </summary>
    struct field_ref {
        void *data = nullptr;
        const field_info *field = nullptr;
        bool isElement = false;
        std::string path = {};

        /// <summary>
        /// The kind of the referenced value (the element kind for an array element).
        /// </summary>
        [[nodiscard]] field_kind kind() const noexcept {
            if (field == nullptr) {
                return field_kind::COUNT;
            }
            return isElement ? field->elementKind : field->kind;
        }

        /// <summary>
        /// Size in bytes of the referenced value.
        /// </summary>
        [[nodiscard]] u32 size() const noexcept {
            if (field == nullptr) {
                return 0;
            }
            return isElement ? field->element_size() : field->size;
        }

        /// <summary>
        /// True when both the data and the description are set.
        /// </summary>
        [[nodiscard]] bool is_valid() const noexcept {
            return data != nullptr && field != nullptr;
        }
    };

    /// <summary>
    /// Follows a path such as "a.b[2].c" through nested structs and arrays. Indices are plain decimal
    /// digits. NOT_FOUND for an unknown field name, OUT_OF_RANGE for an index past the array,
    /// INVALID_ARGUMENT for a malformed path (including a signed index such as "[+1]"), an index on a
    /// non-array or a '.' into a non-struct. The returned reference carries the normalised path.
    /// </summary>
    [[nodiscard]] CONJURE_API result<field_ref> resolve_path(
      const type_info &type,
      void *object,
      std::string_view path
    );

    /// <summary>
    /// Writes the referenced value (one field, or one array element) into 'out'. Errors name the
    /// reference's path ("field 'counts[1]'").
    /// </summary>
    [[nodiscard]] CONJURE_API result<void> write_json(field_ref ref, json::writer &out);

    /// <summary>
    /// Replaces the referenced value from JSON with the same rules as from_json(). Errors name the
    /// reference's path.
    /// </summary>
    [[nodiscard]] CONJURE_API result<void> set_from_json(field_ref ref, const json::value &in);

    /// <summary>
    /// The referenced value as a new document.
    /// </summary>
    [[nodiscard]] CONJURE_API result<json::document> get_as_json(field_ref ref);
}
