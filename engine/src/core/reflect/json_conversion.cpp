#include "conjure/core/reflect/json_conversion.h"

#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/json.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {

    /*-----------------------------------------------------------------------*/
    // Errors

    error mismatch(std::string_view expected, const json::value &in) {
        if (!in.is_valid()) {
            return error(error_code::NOT_FOUND, "value is missing");
        }
        const std::string_view found = json::to_string(in.type());
        return error(error_code::INVALID_ARGUMENT, std::format("expected {}, found {}", expected, found));
    }

    error with_path(const std::string &path, const error &failure) {
        return error(failure.generic_code(), std::format("field '{}': {}", path, failure.message()));
    }

    std::string join_path(const std::string &prefix, std::string_view name) {
        if (prefix.empty()) {
            return std::string(name);
        }
        return std::format("{}.{}", prefix, name);
    }

    /// <summary>
    /// True for a non-empty run of decimal digits, the only spelling of an array index in a path
    /// (parse_uint on its own would also accept a leading '+').
    /// </summary>
    bool is_decimal_index(std::string_view text) noexcept {
        if (text.empty()) {
            return false;
        }
        for (const char character : text) {
            if (!str::is_digit(character)) {
                return false;
            }
        }
        return true;
    }

    /*-----------------------------------------------------------------------*/
    // Floats

    void write_f32(f32 number, json::writer &out) {
        if (!std::isfinite(number)) {
            out.value(nullptr);
            return;
        }
        out.value(number);
    }

    void write_f64(f64 number, json::writer &out) {
        if (!std::isfinite(number)) {
            out.value(nullptr);
            return;
        }
        out.value(number);
    }

    void write_floats(std::span<const f32> components, json::writer &out) {
        out.begin_array();
        for (const f32 component : components) {
            write_f32(component, out);
        }
        out.end_array();
    }

    result<f32> read_f32(const json::value &in) {
        if (in.is_valid() && in.is_null()) {
            return std::numeric_limits<f32>::quiet_NaN();
        }
        if (!in.is_number()) {
            return mismatch("number", in);
        }
        return in.to<f32>();
    }

    result<f64> read_f64(const json::value &in) {
        if (in.is_valid() && in.is_null()) {
            return std::numeric_limits<f64>::quiet_NaN();
        }
        if (!in.is_number()) {
            return mismatch("number", in);
        }
        return in.to<f64>();
    }

    result<void> read_floats(const json::value &in, std::span<f32> components) {
        const std::string expected = std::format("array of {} numbers", components.size());
        if (!in.is_array()) {
            return mismatch(expected, in);
        }
        if (in.size() != components.size()) {
            return error(
              error_code::INVALID_ARGUMENT,
              std::format("expected {}, found array of {} elements", expected, in.size())
            );
        }
        for (usize index = 0; index < components.size(); ++index) {
            result<f32> component = read_f32(in[index]);
            if (!component.has_value()) {
                const error &failure = component.get_error();
                return error(failure.generic_code(), std::format("element {}: {}", index, failure.message()));
            }
            components[index] = component.value();
        }
        return result<void>::success();
    }

    /*-----------------------------------------------------------------------*/
    // Math types

    result<void> read_transform(
      const json::value &in,
      math::transform &inOut,
      const std::string &path,
      std::vector<std::string> *unknown
    ) {
        if (!in.is_object()) {
            return mismatch("object", in);
        }
        for (const json::member member : in.members()) {
            if (member.key == "position") {
                CONJURE_TRY_ASSIGN(inOut.position, member.item.to<math::vec3>());
            } else if (member.key == "rotation") {
                CONJURE_TRY_ASSIGN(inOut.rotation, member.item.to<math::quat>());
            } else if (member.key == "scale") {
                CONJURE_TRY_ASSIGN(inOut.scale, member.item.to<math::vec3>());
            } else if (unknown != nullptr) {
                unknown->push_back(join_path(path, member.key));
            }
        }
        return result<void>::success();
    }

    /*-----------------------------------------------------------------------*/
    // Enumerations: values travel as i64 and are stored in the enum's underlying width.

    /// <summary>
    /// The byte count an enum value occupies, clamped to the u64 scratch width. validate_layout
    /// rejects other widths before a description reaches the JSON code; the clamp keeps a hand-built
    /// description that bypassed it from overrunning the stack.
    /// </summary>
    u32 enum_width(const enum_info &info) noexcept {
        CONJURE_ASSERT(
          is_valid_enum_size(info.underlyingSize),
          "enum '{}' has underlying size {}",
          info.name,
          info.underlyingSize
        );
        return std::min(info.underlyingSize, static_cast<u32>(sizeof(u64)));
    }

    i64 load_enum(const enum_info &info, const void *data) noexcept {
        const u32 width = enum_width(info);
        u64 bits = 0;
        std::memcpy(&bits, data, width);
        if (info.isSigned && width > 0 && width < 8) {
            const u32 shift = 64 - width * 8;
            return static_cast<i64>(bits << shift) >> shift;   // sign-extend
        }
        return static_cast<i64>(bits);
    }

    void store_enum(const enum_info &info, void *data, i64 value) noexcept {
        const u64 bits = static_cast<u64>(value);
        std::memcpy(data, &bits, enum_width(info));   // little-endian: the low bytes come first
    }

    void write_enum(const enum_info &info, const void *data, json::writer &out) {
        const i64 value = load_enum(info, data);
        const std::optional<std::string> spelled = info.format_value(value);
        if (spelled.has_value()) {
            out.value(std::string_view(*spelled));
            return;
        }
        if (info.isSigned) {
            out.value(value);
        } else {
            out.value(static_cast<u64>(value));
        }
    }

    result<void> read_enum(const enum_info &info, void *data, const json::value &in) {
        if (in.is_string()) {
            const std::optional<i64> value = info.parse_value(in.as_string());
            if (!value.has_value()) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format("'{}' is not an enumerator of {}", in.as_string(), info.name)
                );
            }
            store_enum(info, data, *value);
            return result<void>::success();
        }
        if (!in.is_integer()) {
            return mismatch(std::format("enumerator name or integer of {}", info.name), in);
        }

        // Non-negative integers may exceed the i64 range; they are kept as their bit pattern, which
        // only an 8 byte unsigned enum accepts. is_representable() cannot tell a wrapped u64 from a
        // negative i64 at width 8, so the signed case is checked on the unsigned value first.
        const bool negative = in.as_f64() < 0.0;
        const bool beyondSigned = !negative && in.as_u64() > static_cast<u64>(std::numeric_limits<i64>::max());
        if (info.isSigned && beyondSigned) {
            return error(
              error_code::OUT_OF_RANGE,
              std::format("{} does not fit the signed {} byte enum {}", in.as_u64(), info.underlyingSize, info.name)
            );
        }
        const i64 value = negative ? in.as_i64() : static_cast<i64>(in.as_u64());
        if (!info.is_representable(value)) {
            return error(
              error_code::OUT_OF_RANGE,
              std::format("{} does not fit the {} byte enum {}", in.as_f64(), info.underlyingSize, info.name)
            );
        }
        store_enum(info, data, value);
        return result<void>::success();
    }

    /*-----------------------------------------------------------------------*/
    // Custom handle kinds (ENTITY / ASSET_REF): raw bits, since core does not know the type.

    bool is_integer_handle_size(u32 size) noexcept {
        return size == 1 || size == 2 || size == 4 || size == 8;
    }

    result<void> write_custom(field_kind kind, u32 size, const void *data, json::writer &out) {
        if (is_integer_handle_size(size)) {
            u64 bits = 0;
            std::memcpy(&bits, data, size);
            out.value(bits);
            return result<void>::success();
        }
        if (size == sizeof(uuid)) {
            uuid id = {};
            std::memcpy(&id, data, sizeof(uuid));
            out.value(std::string_view(id.to_string()));
            return result<void>::success();
        }
        return error(
          error_code::UNSUPPORTED,
          std::format("kind {} of size {} has no JSON representation", to_string(kind), size)
        );
    }

    result<void> read_custom(field_kind kind, u32 size, void *data, const json::value &in) {
        if (is_integer_handle_size(size)) {
            if (!in.is_integer() || in.as_f64() < 0.0) {
                return mismatch("unsigned integer", in);
            }
            const u64 bits = in.as_u64();
            if (size < 8 && bits >= (u64{1} << (size * 8))) {
                return error(error_code::OUT_OF_RANGE, std::format("{} does not fit a {} byte handle", bits, size));
            }
            std::memcpy(data, &bits, size);
            return result<void>::success();
        }
        if (size == sizeof(uuid)) {
            CONJURE_TRY_ASSIGN(const uuid id, in.to<uuid>());
            std::memcpy(data, &id, sizeof(uuid));
            return result<void>::success();
        }
        return error(
          error_code::UNSUPPORTED,
          std::format("kind {} of size {} has no JSON representation", to_string(kind), size)
        );
    }

    /*-----------------------------------------------------------------------*/
    // Generic values

    template <typename T>
    void write_scalar(const void *data, json::writer &out) {
        T value = {};
        std::memcpy(&value, data, sizeof(T));
        out.value(value);
    }

    template <typename T>
    result<void> read_scalar(void *data, const json::value &in) {
        CONJURE_TRY_ASSIGN(const T value, in.to<T>());
        std::memcpy(data, &value, sizeof(T));
        return result<void>::success();
    }

    result<void> write_struct(
      const reflect::type_info &type,
      const void *object,
      json::writer &out,
      const std::string &path
    );

    result<void> read_struct(
      const reflect::type_info &type,
      void *object,
      const json::value &in,
      const std::string &path,
      std::vector<std::string> *unknown
    );

    /// <summary>
    /// Writes one value of 'kind' (a whole non-array field or one array element) at 'data'.
    /// </summary>
    result<void> write_value(
      field_kind kind,
      const field_info &field,
      u32 size,
      const void *data,
      json::writer &out,
      const std::string &path
    ) {
        switch (kind) {
            case field_kind::BOOL: {
                bool value = false;
                std::memcpy(&value, data, sizeof(bool));
                out.value(value);
                return result<void>::success();
            }
            case field_kind::I8:
                write_scalar<i8>(data, out);
                return result<void>::success();
            case field_kind::I16:
                write_scalar<i16>(data, out);
                return result<void>::success();
            case field_kind::I32:
                write_scalar<i32>(data, out);
                return result<void>::success();
            case field_kind::I64:
                write_scalar<i64>(data, out);
                return result<void>::success();
            case field_kind::U8:
                write_scalar<u8>(data, out);
                return result<void>::success();
            case field_kind::U16:
                write_scalar<u16>(data, out);
                return result<void>::success();
            case field_kind::U32:
                write_scalar<u32>(data, out);
                return result<void>::success();
            case field_kind::U64:
                write_scalar<u64>(data, out);
                return result<void>::success();
            case field_kind::F32: {
                f32 value = 0.0f;
                std::memcpy(&value, data, sizeof(f32));
                write_f32(value, out);
                return result<void>::success();
            }
            case field_kind::F64: {
                f64 value = 0.0;
                std::memcpy(&value, data, sizeof(f64));
                write_f64(value, out);
                return result<void>::success();
            }
            case field_kind::STRING:
                out.value(std::string_view(*static_cast<const std::string *>(data)));
                return result<void>::success();
            case field_kind::STRING_ID: {
                string_id value = {};
                std::memcpy(&value, data, sizeof(string_id));
                write_json(value, out);
                return result<void>::success();
            }
            case field_kind::UUID: {
                uuid value = {};
                std::memcpy(&value, data, sizeof(uuid));
                out.value(std::string_view(value.to_string()));
                return result<void>::success();
            }
            case field_kind::VEC2: {
                math::vec2 value = {};
                std::memcpy(&value, data, sizeof(math::vec2));
                write_json(value, out);
                return result<void>::success();
            }
            case field_kind::VEC3: {
                math::vec3 value = {};
                std::memcpy(&value, data, sizeof(math::vec3));
                write_json(value, out);
                return result<void>::success();
            }
            case field_kind::VEC4: {
                math::vec4 value = {};
                std::memcpy(&value, data, sizeof(math::vec4));
                write_json(value, out);
                return result<void>::success();
            }
            case field_kind::QUAT: {
                math::quat value = {};
                std::memcpy(&value, data, sizeof(math::quat));
                write_json(value, out);
                return result<void>::success();
            }
            case field_kind::MAT4: {
                math::mat4 value = {};
                std::memcpy(&value, data, sizeof(math::mat4));
                write_json(value, out);
                return result<void>::success();
            }
            case field_kind::TRANSFORM: {
                math::transform value = {};
                std::memcpy(&value, data, sizeof(math::transform));
                write_json(value, out);
                return result<void>::success();
            }
            case field_kind::ENUM:
                if (field.enumType == nullptr) {
                    return with_path(path, error(error_code::INVALID_STATE, "enum field has no enum description"));
                }
                write_enum(*field.enumType, data, out);
                return result<void>::success();
            case field_kind::STRUCT:
                if (field.structType == nullptr) {
                    return with_path(path, error(error_code::INVALID_STATE, "struct field has no type description"));
                }
                return write_struct(*field.structType, data, out, path);
            case field_kind::ARRAY: {
                const u32 elementSize = field.element_size();
                const std::byte *elements = static_cast<const std::byte *>(data);
                out.begin_array();
                for (u32 index = 0; index < field.arrayCount; ++index) {
                    const std::string elementPath = std::format("{}[{}]", path, index);
                    CONJURE_TRY(write_value(field.elementKind, field, elementSize, elements, out, elementPath));
                    elements += elementSize;
                }
                out.end_array();
                return result<void>::success();
            }
            case field_kind::ENTITY:
            case field_kind::ASSET_REF: {
                result<void> written = write_custom(kind, size, data, out);
                if (!written.has_value()) {
                    return with_path(path, written.get_error());
                }
                return result<void>::success();
            }
            case field_kind::COUNT:
                break;
        }
        return with_path(path, error(error_code::INVALID_STATE, "field has no kind"));
    }

    /// <summary>
    /// Reads one value of 'kind' into 'data'. Errors carry the field path.
    /// </summary>
    result<void> read_value(
      field_kind kind,
      const field_info &field,
      u32 size,
      void *data,
      const json::value &in,
      const std::string &path,
      std::vector<std::string> *unknown
    ) {
        result<void> outcome = result<void>::success();
        switch (kind) {
            case field_kind::BOOL:
                outcome = read_scalar<bool>(data, in);
                break;
            case field_kind::I8:
                outcome = read_scalar<i8>(data, in);
                break;
            case field_kind::I16:
                outcome = read_scalar<i16>(data, in);
                break;
            case field_kind::I32:
                outcome = read_scalar<i32>(data, in);
                break;
            case field_kind::I64:
                outcome = read_scalar<i64>(data, in);
                break;
            case field_kind::U8:
                outcome = read_scalar<u8>(data, in);
                break;
            case field_kind::U16:
                outcome = read_scalar<u16>(data, in);
                break;
            case field_kind::U32:
                outcome = read_scalar<u32>(data, in);
                break;
            case field_kind::U64:
                outcome = read_scalar<u64>(data, in);
                break;
            case field_kind::F32: {
                result<f32> value = read_f32(in);
                if (value.has_value()) {
                    const f32 number = value.value();
                    std::memcpy(data, &number, sizeof(f32));
                } else {
                    outcome = value.get_error();
                }
                break;
            }
            case field_kind::F64: {
                result<f64> value = read_f64(in);
                if (value.has_value()) {
                    const f64 number = value.value();
                    std::memcpy(data, &number, sizeof(f64));
                } else {
                    outcome = value.get_error();
                }
                break;
            }
            case field_kind::STRING:
                if (!in.is_string()) {
                    outcome = mismatch("string", in);
                } else {
                    *static_cast<std::string *>(data) = std::string(in.as_string());
                }
                break;
            case field_kind::STRING_ID:
                outcome = read_scalar<string_id>(data, in);
                break;
            case field_kind::UUID:
                outcome = read_scalar<uuid>(data, in);
                break;
            case field_kind::VEC2:
                outcome = read_scalar<math::vec2>(data, in);
                break;
            case field_kind::VEC3:
                outcome = read_scalar<math::vec3>(data, in);
                break;
            case field_kind::VEC4:
                outcome = read_scalar<math::vec4>(data, in);
                break;
            case field_kind::QUAT:
                outcome = read_scalar<math::quat>(data, in);
                break;
            case field_kind::MAT4:
                outcome = read_scalar<math::mat4>(data, in);
                break;
            case field_kind::TRANSFORM: {
                // Read into a copy so absent members keep their current value and a failure leaves
                // the field untouched.
                math::transform current = {};
                std::memcpy(&current, data, sizeof(math::transform));
                outcome = read_transform(in, current, path, unknown);
                if (outcome.has_value()) {
                    std::memcpy(data, &current, sizeof(math::transform));
                }
                break;
            }
            case field_kind::ENUM:
                if (field.enumType == nullptr) {
                    outcome = error(error_code::INVALID_STATE, "enum field has no enum description");
                } else {
                    outcome = read_enum(*field.enumType, data, in);
                }
                break;
            case field_kind::STRUCT:
                if (field.structType == nullptr) {
                    outcome = error(error_code::INVALID_STATE, "struct field has no type description");
                } else {
                    return read_struct(*field.structType, data, in, path, unknown);
                }
                break;
            case field_kind::ARRAY: {
                if (!in.is_array()) {
                    outcome = mismatch("array", in);
                    break;
                }
                if (in.size() > field.arrayCount) {
                    outcome = error(
                      error_code::OUT_OF_RANGE,
                      std::format("array has {} elements but the field holds {}", in.size(), field.arrayCount)
                    );
                    break;
                }
                const u32 elementSize = field.element_size();
                std::byte *elements = static_cast<std::byte *>(data);
                usize index = 0;
                for (const json::value item : in) {
                    const std::string elementPath = std::format("{}[{}]", path, index);
                    CONJURE_TRY(
                      read_value(field.elementKind, field, elementSize, elements, item, elementPath, unknown)
                    );
                    elements += elementSize;
                    ++index;
                }
                return result<void>::success();
            }
            case field_kind::ENTITY:
            case field_kind::ASSET_REF:
                outcome = read_custom(kind, size, data, in);
                break;
            case field_kind::COUNT:
                outcome = error(error_code::INVALID_STATE, "field has no kind");
                break;
        }

        if (!outcome.has_value()) {
            return with_path(path, outcome.get_error());
        }
        return result<void>::success();
    }

    result<void> write_struct(
      const reflect::type_info &type,
      const void *object,
      json::writer &out,
      const std::string &path
    ) {
        const std::byte *base = static_cast<const std::byte *>(object);
        out.begin_object();
        for (const field_info &field : type.fields) {
            out.key(field.name);
            const std::string fieldPath = join_path(path, field.name);
            CONJURE_TRY(write_value(field.kind, field, field.size, base + field.offset, out, fieldPath));
        }
        out.end_object();
        return result<void>::success();
    }

    result<void> read_struct(
      const reflect::type_info &type,
      void *object,
      const json::value &in,
      const std::string &path,
      std::vector<std::string> *unknown
    ) {
        if (!in.is_object()) {
            const error failure = mismatch("object", in);
            if (path.empty()) {
                return error(failure.generic_code(), std::format("type '{}': {}", type.name, failure.message()));
            }
            return with_path(path, failure);
        }

        std::byte *base = static_cast<std::byte *>(object);
        for (const json::member member : in.members()) {
            const field_info *field = type.find_field(member.key);
            if (field == nullptr) {
                if (unknown != nullptr) {
                    unknown->push_back(join_path(path, member.key));
                }
                continue;
            }
            const std::string fieldPath = join_path(path, member.key);
            std::byte *data = base + field->offset;
            CONJURE_TRY(read_value(field->kind, *field, field->size, data, member.item, fieldPath, unknown));
        }
        return result<void>::success();
    }
}

/*===========================================================================*/
/*===========================================================================*/
// json_converter specialisations

result<string_id> json::json_converter<string_id>::from_json(const value &source) {
    if (source.is_string()) {
        const std::string_view text = source.as_string();
        if (text.size() == 17 && text.front() == '#') {
            const std::optional<u64> hash = str::parse_uint(text.substr(1), 16);
            if (hash.has_value()) {
                return string_id::from_hash(*hash);
            }
        }
        return string_id(text);
    }
    if (source.is_integer() && source.as_f64() >= 0.0) {
        return string_id::from_hash(source.as_u64());
    }
    return mismatch("string or unsigned integer", source);
}

result<math::vec2> json::json_converter<math::vec2>::from_json(const value &source) {
    std::array<f32, 2> components = {};
    CONJURE_TRY(read_floats(source, components));
    return math::vec2(components[0], components[1]);
}

result<math::vec3> json::json_converter<math::vec3>::from_json(const value &source) {
    std::array<f32, 3> components = {};
    CONJURE_TRY(read_floats(source, components));
    return math::vec3(components[0], components[1], components[2]);
}

result<math::vec4> json::json_converter<math::vec4>::from_json(const value &source) {
    std::array<f32, 4> components = {};
    CONJURE_TRY(read_floats(source, components));
    return math::vec4(components[0], components[1], components[2], components[3]);
}

result<math::quat> json::json_converter<math::quat>::from_json(const value &source) {
    std::array<f32, 4> components = {};
    CONJURE_TRY(read_floats(source, components));
    // JSON order is the storage order x, y, z, w; glm's constructor takes w first.
    return math::quat(components[3], components[0], components[1], components[2]);
}

result<math::mat4> json::json_converter<math::mat4>::from_json(const value &source) {
    std::array<f32, 16> components = {};
    CONJURE_TRY(read_floats(source, components));
    math::mat4 matrix(1.0f);
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            matrix[column][row] = components[static_cast<usize>(column * 4 + row)];
        }
    }
    return matrix;
}

result<math::transform> json::json_converter<math::transform>::from_json(const value &source) {
    math::transform value = {};
    CONJURE_TRY(read_transform(source, value, std::string(), nullptr));
    return value;
}

/*===========================================================================*/
/*===========================================================================*/
// Writers for the converter types

void conjure::core::reflect::write_json(const math::vec2 &value, json::writer &out) {
    const f32 components[2] = {value.x, value.y};
    write_floats(components, out);
}

void conjure::core::reflect::write_json(const math::vec3 &value, json::writer &out) {
    const f32 components[3] = {value.x, value.y, value.z};
    write_floats(components, out);
}

void conjure::core::reflect::write_json(const math::vec4 &value, json::writer &out) {
    const f32 components[4] = {value.x, value.y, value.z, value.w};
    write_floats(components, out);
}

void conjure::core::reflect::write_json(const math::quat &value, json::writer &out) {
    const f32 components[4] = {value.x, value.y, value.z, value.w};
    write_floats(components, out);
}

void conjure::core::reflect::write_json(const math::mat4 &value, json::writer &out) {
    f32 components[16] = {};
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            components[column * 4 + row] = value[column][row];
        }
    }
    write_floats(components, out);
}

void conjure::core::reflect::write_json(const math::transform &value, json::writer &out) {
    out.begin_object();
    out.key("position");
    write_json(value.position, out);
    out.key("rotation");
    write_json(value.rotation, out);
    out.key("scale");
    write_json(value.scale, out);
    out.end_object();
}

void conjure::core::reflect::write_json(const string_id &value, json::writer &out) {
    out.value(std::string_view(value.to_string()));
}

/*===========================================================================*/
/*===========================================================================*/
// Whole objects

result<void> conjure::core::reflect::to_json(const reflect::type_info &type, const void *object, json::writer &out) {
    if (object == nullptr) {
        return error(error_code::INVALID_ARGUMENT, "object is null");
    }
    return write_struct(type, object, out, std::string());
}

result<void> conjure::core::reflect::from_json(
  const reflect::type_info &type,
  void *object,
  const json::value &in,
  std::vector<std::string> *unknownFields
) {
    if (object == nullptr) {
        return error(error_code::INVALID_ARGUMENT, "object is null");
    }
    return read_struct(type, object, in, std::string(), unknownFields);
}

result<json::document> conjure::core::reflect::to_json_document(const reflect::type_info &type, const void *object) {
    json::writer out = {};
    CONJURE_TRY(to_json(type, object, out));
    return json::document::parse(out.finish());
}

/*===========================================================================*/
/*===========================================================================*/
// Field access by path

result<field_ref> conjure::core::reflect::resolve_path(
  const reflect::type_info &type,
  void *object,
  std::string_view path
) {
    if (object == nullptr) {
        return error(error_code::INVALID_ARGUMENT, "object is null");
    }
    if (path.empty()) {
        return error(error_code::INVALID_ARGUMENT, "path is empty");
    }

    const reflect::type_info *currentType = &type;
    std::byte *data = static_cast<std::byte *>(object);
    std::string resolved = {};   // the normalised path so far, reported by the field_ref
    usize position = 0;
    while (true) {
        usize nameEnd = position;
        while (nameEnd < path.size() && path[nameEnd] != '.' && path[nameEnd] != '[') {
            ++nameEnd;
        }
        const std::string_view name = path.substr(position, nameEnd - position);
        if (name.empty()) {
            return error(
              error_code::INVALID_ARGUMENT,
              std::format("path '{}': expected a field name at position {}", path, position)
            );
        }

        const field_info *field = currentType->find_field(name);
        if (field == nullptr) {
            return error(error_code::NOT_FOUND, std::format("type '{}' has no field '{}'", currentType->name, name));
        }
        data += field->offset;
        resolved = join_path(resolved, name);
        field_ref ref = {data, field, false, resolved};
        field_kind kind = field->kind;
        position = nameEnd;

        if (position < path.size() && path[position] == '[') {
            if (!field->is_array()) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format("path '{}': field '{}' is not an array", path, name)
                );
            }
            const usize close = path.find(']', position);
            if (close == std::string_view::npos) {
                return error(error_code::INVALID_ARGUMENT, std::format("path '{}': missing ']'", path));
            }
            const std::string_view indexText = path.substr(position + 1, close - position - 1);
            const std::optional<u64> index =
              is_decimal_index(indexText) ? str::parse_uint(indexText) : std::optional<u64>(std::nullopt);
            if (!index.has_value()) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format("path '{}': '{}' is not an array index", path, indexText)
                );
            }
            if (*index >= field->arrayCount) {
                return error(
                  error_code::OUT_OF_RANGE,
                  std::format(
                    "path '{}': index {} is past the {} elements of '{}'",
                    path,
                    *index,
                    field->arrayCount,
                    name
                  )
                );
            }
            data += static_cast<usize>(*index) * field->element_size();
            resolved += std::format("[{}]", *index);
            ref = {data, field, true, resolved};
            kind = field->elementKind;
            position = close + 1;
        }

        if (position == path.size()) {
            return ref;
        }
        if (path[position] != '.') {
            return error(
              error_code::INVALID_ARGUMENT,
              std::format("path '{}': unexpected '{}' at position {}", path, path[position], position)
            );
        }
        ++position;

        if (kind != field_kind::STRUCT || field->structType == nullptr) {
            return error(error_code::INVALID_ARGUMENT, std::format("path '{}': '{}' is not a struct", path, name));
        }
        currentType = field->structType;
    }
}

result<void> conjure::core::reflect::write_json(field_ref ref, json::writer &out) {
    if (!ref.is_valid()) {
        return error(error_code::INVALID_ARGUMENT, "field reference is invalid");
    }
    const std::string &path = ref.path.empty() ? ref.field->name : ref.path;
    return write_value(ref.kind(), *ref.field, ref.size(), ref.data, out, path);
}

result<void> conjure::core::reflect::set_from_json(field_ref ref, const json::value &in) {
    if (!ref.is_valid()) {
        return error(error_code::INVALID_ARGUMENT, "field reference is invalid");
    }
    const std::string &path = ref.path.empty() ? ref.field->name : ref.path;
    return read_value(ref.kind(), *ref.field, ref.size(), ref.data, in, path, nullptr);
}

result<json::document> conjure::core::reflect::get_as_json(field_ref ref) {
    json::writer out = {};
    CONJURE_TRY(write_json(ref, out));
    return json::document::parse(out.finish());
}
