#include "conjure/core/reflect/type_info.h"

#include "conjure/core/reflect/field_info.h"

#include "conjure/core/error.h"
#include "conjure/core/hash.h"
#include "conjure/core/result.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {
    /// <summary>
    /// Continues an FNV-1a hash with a u32 in little-endian byte order (the C# mirror hashes the
    /// same four bytes).
    /// </summary>
    constexpr u64 hash_u32(u64 hash, u32 value) noexcept {
        hash = hash::fnv1a64_step(hash, static_cast<u8>(value & 0xFFu));
        hash = hash::fnv1a64_step(hash, static_cast<u8>((value >> 8) & 0xFFu));
        hash = hash::fnv1a64_step(hash, static_cast<u8>((value >> 16) & 0xFFu));
        hash = hash::fnv1a64_step(hash, static_cast<u8>((value >> 24) & 0xFFu));
        return hash;
    }

    error layout_error(const reflect::type_info &type, const field_info &field, std::string_view problem) {
        return error(
          error_code::INVALID_ARGUMENT,
          std::format("type '{}': field '{}' {}", type.name, field.name, problem)
        );
    }

    /// <summary>
    /// Size one value of the field's value kind must have, or 0 when any size is acceptable.
    /// </summary>
    u32 expected_value_size(const field_info &field) noexcept {
        const field_kind kind = field.value_kind();
        if (is_fixed_size_kind(kind)) {
            return kind_size(kind);
        }
        if (kind == field_kind::ENUM && field.enumType != nullptr) {
            return field.enumType->underlyingSize;
        }
        if (kind == field_kind::STRUCT && field.structType != nullptr) {
            return field.structType->size;
        }
        return 0;
    }

    /// <summary>
    /// Alignment one value of the field's value kind requires.
    /// </summary>
    u32 required_alignment(const reflect::type_info &type, const field_info &field) noexcept {
        const field_kind kind = field.value_kind();
        if (is_fixed_size_kind(kind)) {
            return kind_alignment(kind);
        }
        if (kind == field_kind::ENUM && field.enumType != nullptr) {
            return field.enumType->underlyingSize;
        }
        if (kind == field_kind::STRUCT && field.structType != nullptr) {
            return field.structType->alignment;
        }

        // Custom handles (ENTITY / ASSET_REF): core does not know the handle type, so the natural
        // guess is a power-of-two size up to 8 aligning to itself and larger handles to 8. The guess
        // is capped at the owning type's alignment: a handle is a member of that type, so its real
        // alignment can never exceed it (ecs::entity is {u32, u32}, 8 bytes aligned to 4, and lives
        // in 4-byte aligned components), and instances are allocated with the type's alignment anyway.
        const u32 valueSize = field.element_size();
        u32 natural = 1;
        if (valueSize == 1 || valueSize == 2 || valueSize == 4 || valueSize == 8) {
            natural = valueSize;
        } else if (valueSize >= 16) {
            natural = 8;
        }
        return std::min(natural, type.alignment);
    }

    bool is_valid_field_name(std::string_view name) noexcept {
        if (name.empty()) {
            return false;
        }
        for (const char character : name) {
            if (character == '.' || character == '[' || character == ']' || str::is_space(character)) {
                return false;
            }
        }
        return true;
    }

    result<void> validate_field(const reflect::type_info &type, const field_info &field) {
        if (!is_valid_field_name(field.name)) {
            return error(
              error_code::INVALID_ARGUMENT,
              std::format(
                "type '{}': field name '{}' is empty or contains '.', '[', ']' or whitespace",
                type.name,
                field.name
              )
            );
        }
        if (field.kind >= field_kind::COUNT) {
            return layout_error(type, field, "has no kind");
        }
        if (field.size == 0) {
            return layout_error(type, field, "has size 0");
        }
        if (static_cast<u64>(field.offset) + field.size > type.size) {
            return layout_error(
              type,
              field,
              std::format("at offset {} with size {} does not fit in {} bytes", field.offset, field.size, type.size)
            );
        }

        const field_kind valueKind = field.value_kind();
        if (field.kind == field_kind::ARRAY) {
            if (field.arrayCount == 0) {
                return layout_error(type, field, "is an array with no elements");
            }
            if (valueKind >= field_kind::COUNT || valueKind == field_kind::ARRAY) {
                return layout_error(type, field, "has an invalid element kind");
            }
            if (field.size % field.arrayCount != 0) {
                return layout_error(type, field, "has a size that is not a multiple of its element count");
            }
        } else if (field.arrayCount != 0) {
            return layout_error(type, field, "has an element count but is not an array");
        }

        if (valueKind == field_kind::STRUCT) {
            if (field.structType == nullptr) {
                return layout_error(type, field, "is a struct without a type description");
            }
            if (field.type != field.structType->type) {
                return layout_error(type, field, "has a type id that does not match its struct description");
            }
        }
        if (valueKind == field_kind::ENUM) {
            if (field.enumType == nullptr) {
                return layout_error(type, field, "is an enum without an enum description");
            }
            if (field.type != field.enumType->type) {
                return layout_error(type, field, "has a type id that does not match its enum description");
            }
            // A hand-built enum_info (dynamic C# component) never went through register_enum, so the
            // width must be checked here before the JSON code trusts it for memcpy sizes and shifts.
            const u32 width = field.enumType->underlyingSize;
            if (!is_valid_enum_size(width)) {
                return layout_error(
                  type,
                  field,
                  std::format(
                    "uses enum '{}' with underlying size {} (must be 1, 2, 4 or 8)",
                    field.enumType->name,
                    width
                  )
                );
            }
        }

        const u32 expectedSize = expected_value_size(field);
        if (expectedSize != 0 && field.element_size() != expectedSize) {
            return layout_error(
              type,
              field,
              std::format(
                "has value size {} but kind {} needs {}",
                field.element_size(),
                to_string(valueKind),
                expectedSize
              )
            );
        }

        const u32 alignment = required_alignment(type, field);
        if (alignment > type.alignment) {
            // Instances are allocated with the type's alignment, so a stricter field could land
            // misaligned (and the serialiser forms real references over STRING fields).
            return layout_error(
              type,
              field,
              std::format("requires alignment {} but the type is aligned to {}", alignment, type.alignment)
            );
        }
        if (alignment != 0 && field.offset % alignment != 0) {
            return layout_error(type, field, std::format("at offset {} is not aligned to {}", field.offset, alignment));
        }
        return result<void>::success();
    }
}

/*===========================================================================*/
/*===========================================================================*/
// reflect::type_info

const field_info *reflect::type_info::find_field(std::string_view fieldName) const noexcept {
    for (const field_info &field : fields) {
        if (field.name == fieldName) {
            return &field;
        }
    }
    return nullptr;
}

u64 reflect::type_info::layout_hash() const noexcept {
    u64 hash = hash::FNV1A64_OFFSET_BASIS;
    for (const field_info &field : fields) {
        hash = hash::fnv1a64(field.name, hash);
        hash = hash::fnv1a64_step(hash, static_cast<u8>(field.kind));
        hash = hash_u32(hash, field.offset);
        hash = hash_u32(hash, field.size);
    }
    return hash;
}

bool reflect::type_info::has_same_layout(const reflect::type_info &other) const noexcept {
    return size == other.size && alignment == other.alignment && layout_hash() == other.layout_hash();
}

/*===========================================================================*/
/*===========================================================================*/
// Validation and runtime construction

result<void> conjure::core::reflect::validate_layout(const reflect::type_info &info) {
    if (info.name.empty()) {
        return error(error_code::INVALID_ARGUMENT, "type has no name");
    }
    if (info.alignment == 0 || (info.alignment & (info.alignment - 1)) != 0) {
        return error(
          error_code::INVALID_ARGUMENT,
          std::format("type '{}': alignment {} is not a power of two", info.name, info.alignment)
        );
    }
    if (info.size % info.alignment != 0) {
        return error(
          error_code::INVALID_ARGUMENT,
          std::format("type '{}': size {} is not a multiple of alignment {}", info.name, info.size, info.alignment)
        );
    }

    for (const field_info &field : info.fields) {
        CONJURE_TRY(validate_field(info, field));
    }

    // Names must be unique and extents must not overlap: sort by offset and compare neighbours.
    std::vector<const field_info *> ordered = {};
    ordered.reserve(info.fields.size());
    for (const field_info &field : info.fields) {
        ordered.push_back(&field);
    }
    std::sort(ordered.begin(), ordered.end(), [](const field_info *left, const field_info *right) {
        return left->offset < right->offset;
    });
    for (usize index = 1; index < ordered.size(); ++index) {
        const field_info &previous = *ordered[index - 1];
        const field_info &current = *ordered[index];
        if (previous.offset + previous.size > current.offset) {
            return error(
              error_code::INVALID_ARGUMENT,
              std::format("type '{}': fields '{}' and '{}' overlap", info.name, previous.name, current.name)
            );
        }
    }
    for (usize first = 0; first < info.fields.size(); ++first) {
        for (usize second = first + 1; second < info.fields.size(); ++second) {
            if (info.fields[first].name == info.fields[second].name) {
                return error(
                  error_code::INVALID_ARGUMENT,
                  std::format("type '{}': field '{}' is declared twice", info.name, info.fields[first].name)
                );
            }
        }
    }
    return result<void>::success();
}

result<reflect::type_info> conjure::core::reflect::build_type_info(
  std::string name,
  u32 size,
  u32 alignment,
  std::span<const field_info> fields
) {
    reflect::type_info info = {};
    info.type = type_id::from_name(name);
    info.name = std::move(name);
    info.size = size;
    info.alignment = alignment;
    info.fields.assign(fields.begin(), fields.end());

    info.isTriviallyCopyable = true;
    for (const field_info &field : info.fields) {
        const field_kind valueKind = field.value_kind();
        if (valueKind == field_kind::STRING) {
            info.isTriviallyCopyable = false;
        }
        if (valueKind == field_kind::STRUCT && field.structType != nullptr && !field.structType->isTriviallyCopyable) {
            info.isTriviallyCopyable = false;
        }
    }

    CONJURE_TRY(validate_layout(info));
    return info;
}
