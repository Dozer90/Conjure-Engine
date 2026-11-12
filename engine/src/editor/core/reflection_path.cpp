#include "conjure/editor/reflection_path.h"

#include "conjure/editor/editor_error.h"

#include "conjure/core/string_utils.h"

#include <format>

namespace conjure::editor {

    namespace {
        using core::reflect::field_info;
        using core::reflect::field_kind;
        using core::reflect::type_info;

        [[nodiscard]] core::error path_error(std::string_view text, std::string_view problem) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, std::format("path '{}': {}", text, problem));
        }

        [[nodiscard]] core::error not_found(std::string_view path, std::string_view segment, std::string_view why) {
            return make_editor_error(
              protocol_error_code::PATH_NOT_FOUND,
              std::format("path '{}': segment '{}' {}", path, segment, why)
            );
        }

        [[nodiscard]] bool is_identifier_char(char character) noexcept {
            return core::str::is_alnum(character) || character == '_' || character == '-' || character == ':';
        }

        /// <summary>
        /// Component index of a vector or quaternion member name, or nullopt.
        /// </summary>
        [[nodiscard]] std::optional<u32> component_index(std::string_view name, u32 count) noexcept {
            static constexpr std::string_view LETTERS = "xyzw";
            static constexpr std::string_view COLOURS = "rgba";
            if (name.size() == 1) {
                const usize letter = LETTERS.find(name[0]);
                if (letter != std::string_view::npos && letter < count) {
                    return static_cast<u32>(letter);
                }
                const usize colour = COLOURS.find(name[0]);
                if (colour != std::string_view::npos && colour < count) {
                    return static_cast<u32>(colour);
                }
            }
            const std::optional<u64> number = core::str::parse_uint(name);
            if (number.has_value() && *number < count) {
                return static_cast<u32>(*number);
            }
            return std::nullopt;
        }

        [[nodiscard]] u32 component_count(field_kind kind) noexcept {
            switch (kind) {
                case field_kind::VEC2:
                    return 2;
                case field_kind::VEC3:
                    return 3;
                case field_kind::VEC4:
                case field_kind::QUAT:
                    return 4;
                case field_kind::MAT4:
                    return 16;
                default:
                    return 0;
            }
        }

        struct cursor {
            const type_info *structType = nullptr;      // current struct (nullptr when inside a leaf value)
            const field_info *field = nullptr;          // the innermost named field
            field_kind kind = field_kind::COUNT;        // kind of the value at the cursor
            std::string pointer = "";
            bool arrayPending = false;                  // the cursor is on an ARRAY field that still needs its index
        };

        core::result<void> descend_named(cursor &at, std::string_view name, std::string_view path) {
            if (at.arrayPending) {
                return not_found(path, name, "needs an array index before a member name");
            }
            if (at.structType != nullptr) {
                const field_info *field = at.structType->find_field(name);
                if (field == nullptr) {
                    return not_found(path, name, std::format("is not a field of '{}'", at.structType->name));
                }
                at.field = field;
                at.pointer += '/';
                at.pointer += name;
                if (field->kind == field_kind::ARRAY) {
                    at.arrayPending = true;
                    at.kind = field_kind::ARRAY;
                    at.structType = nullptr;
                    return core::result<void>::success();
                }
                at.kind = field->kind;
                at.structType = field->kind == field_kind::STRUCT ? field->structType : nullptr;
                return core::result<void>::success();
            }
            // Inside a leaf value: vectors, quaternions, transforms and matrices expose named parts.
            if (at.kind == field_kind::TRANSFORM) {
                if (name == "position" || name == "scale") {
                    at.kind = field_kind::VEC3;
                } else if (name == "rotation") {
                    at.kind = field_kind::QUAT;
                } else {
                    return not_found(path, name, "is not a member of a transform (position, rotation, scale)");
                }
                at.pointer += '/';
                at.pointer += name;
                return core::result<void>::success();
            }
            const u32 count = component_count(at.kind);
            if (count != 0) {
                const std::optional<u32> index = component_index(name, count);
                if (!index.has_value()) {
                    return not_found(path, name, std::format("is not a component of a {}-component value", count));
                }
                at.pointer += std::format("/{}", *index);
                at.kind = field_kind::F32;
                return core::result<void>::success();
            }
            return not_found(path, name, "cannot be descended: the value is a scalar");
        }

        core::result<void> descend_index(cursor &at, u32 index, std::string_view segmentText, std::string_view path) {
            if (at.arrayPending) {
                const field_info *field = at.field;
                if (field != nullptr && index >= field->arrayCount) {
                    return make_editor_error(
                      protocol_error_code::PATH_NOT_FOUND,
                      std::format(
                        "path '{}': index {} is past the end of '{}' ({} elements)",
                        path,
                        index,
                        field->name,
                        field->arrayCount
                      )
                    );
                }
                at.arrayPending = false;
                at.pointer += std::format("/{}", index);
                at.kind = field == nullptr ? field_kind::COUNT : field->elementKind;
                at.structType = at.kind == field_kind::STRUCT && field != nullptr ? field->structType : nullptr;
                return core::result<void>::success();
            }
            const u32 count = component_count(at.kind);
            if (count != 0) {
                if (index >= count) {
                    return not_found(path, segmentText, std::format("indexes past the {} components", count));
                }
                at.pointer += std::format("/{}", index);
                at.kind = field_kind::F32;
                return core::result<void>::success();
            }
            return not_found(path, segmentText, "indexes a value that is not an array");
        }
    }

    reflection_path::reflection_path() = default;
    reflection_path::~reflection_path() = default;

    core::result<reflection_path> reflection_path::parse(std::string_view text) {
        if (text.empty()) {
            return path_error(text, "a path must not be empty");
        }
        reflection_path result = {};
        usize offset = 0;
        while (offset <= text.size()) {
            usize end = text.find('.', offset);
            if (end == std::string_view::npos) {
                end = text.size();
            }
            const std::string_view segment = text.substr(offset, end - offset);
            if (segment.empty()) {
                return path_error(text, "empty segment");
            }
            path_segment parsed = {};
            const usize bracket = segment.find('[');
            std::string_view name = segment;
            if (bracket != std::string_view::npos) {
                if (segment.back() != ']' || bracket + 1 >= segment.size() - 1) {
                    return path_error(text, std::format("malformed index in '{}'", segment));
                }
                const std::string_view indexText = segment.substr(bracket + 1, segment.size() - bracket - 2);
                for (const char character : indexText) {
                    if (!core::str::is_digit(character)) {
                        return path_error(text, std::format("malformed index in '{}'", segment));
                    }
                }
                const std::optional<u64> index = core::str::parse_uint(indexText);
                if (!index.has_value() || *index > 0xFFFFFFFFull) {
                    return path_error(text, std::format("malformed index in '{}'", segment));
                }
                parsed.index = static_cast<u32>(*index);
                name = segment.substr(0, bracket);
            }
            if (name.empty()) {
                return path_error(text, std::format("segment '{}' has no name", segment));
            }
            for (const char character : name) {
                if (!is_identifier_char(character)) {
                    return path_error(text, std::format("invalid character '{}' in '{}'", character, segment));
                }
            }
            parsed.name = std::string(name);
            result.mSegments.push_back(std::move(parsed));
            if (end == text.size()) {
                break;
            }
            offset = end + 1;
            if (offset == text.size()) {
                return path_error(text, "trailing dot");
            }
        }
        return result;
    }

    bool reflection_path::merge_component(usize segmentCount) {
        if (segmentCount < 2 || segmentCount > mSegments.size()) {
            return false;
        }
        std::string joined = "";
        for (usize index = 0; index < segmentCount; ++index) {
            if (mSegments[index].index.has_value()) {
                return false;
            }
            if (index > 0) {
                joined += '.';
            }
            joined += mSegments[index].name;
        }
        mSegments.front().name = std::move(joined);
        mSegments.erase(mSegments.begin() + 1, mSegments.begin() + static_cast<isize>(segmentCount));
        return true;
    }

    std::string_view reflection_path::component() const noexcept {
        if (mSegments.empty()) {
            return "";
        }
        return mSegments.front().name;
    }

    std::span<const path_segment> reflection_path::fields() const noexcept {
        if (mSegments.empty()) {
            return {};
        }
        return std::span<const path_segment>(mSegments).subspan(1);
    }

    std::string reflection_path::to_string() const {
        std::string text = "";
        for (const path_segment &segment : mSegments) {
            if (!text.empty()) {
                text += '.';
            }
            text += segment.name;
            if (segment.index.has_value()) {
                text += std::format("[{}]", *segment.index);
            }
        }
        return text;
    }

    core::result<field_ref> reflection_path::resolve(const type_info *componentType) const {
        const std::string path = to_string();
        field_ref reference = {};
        reference.type = componentType;
        reference.path = path;
        const std::span<const path_segment> parts = fields();
        if (parts.empty()) {
            // The whole component; the first segment's index would be meaningless.
            if (!mSegments.empty() && mSegments.front().index.has_value()) {
                return path_error(path, "the component segment takes no index");
            }
            reference.kind = componentType == nullptr ? field_kind::COUNT : field_kind::STRUCT;
            return reference;
        }

        if (componentType == nullptr) {
            // Unreflected component: names and indices map straight onto the JSON form.
            std::string pointer = "";
            for (const path_segment &segment : parts) {
                pointer += '/';
                pointer += core::str::replace_all(core::str::replace_all(segment.name, "~", "~0"), "/", "~1");
                if (segment.index.has_value()) {
                    pointer += std::format("/{}", *segment.index);
                }
            }
            reference.pointer = std::move(pointer);
            reference.kind = field_kind::COUNT;
            return reference;
        }

        cursor at = {};
        at.structType = componentType;
        at.kind = field_kind::STRUCT;
        for (const path_segment &segment : parts) {
            CONJURE_TRY(descend_named(at, segment.name, path));
            if (segment.index.has_value()) {
                CONJURE_TRY(descend_index(
                  at,
                  *segment.index,
                  std::format("{}[{}]", segment.name, *segment.index),
                  path
                ));
            }
        }
        if (at.arrayPending) {
            // A whole array: fine, the value is the array itself.
            at.arrayPending = false;
        }
        reference.field = at.field;
        reference.kind = at.kind;
        reference.pointer = std::move(at.pointer);
        return reference;
    }

    core::result<core::json::document> read_at(const core::json::value &componentJson, const field_ref &reference) {
        if (reference.is_whole_component()) {
            return core::json::document::from_value(componentJson);
        }
        const core::json::document owner = core::json::document::from_value(componentJson);
        const core::json::value found = owner.at_pointer(reference.pointer);
        if (!found.is_valid()) {
            return make_editor_error(
              protocol_error_code::PATH_NOT_FOUND,
              std::format("path '{}' does not exist in the component's data", reference.path)
            );
        }
        return core::json::document::from_value(found);
    }

    core::result<void> write_at(
      core::json::document &componentJson,
      const field_ref &reference,
      const core::json::value &value
    ) {
        if (reference.is_whole_component()) {
            if (!value.is_object()) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, "a whole component is set from an object");
            }
            componentJson = core::json::document::from_value(value);
            return core::result<void>::success();
        }
        if (!componentJson.set_pointer(reference.pointer, value)) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              std::format("path '{}' cannot be written in the component's data", reference.path)
            );
        }
        return core::result<void>::success();
    }
}
