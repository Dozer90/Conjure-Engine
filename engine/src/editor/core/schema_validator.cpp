#include "conjure/editor/schema_validator.h"

#include "conjure/editor/editor_error.h"

#include <array>
#include <cmath>
#include <format>
#include <string>

namespace conjure::editor {

    namespace {
        using core::json::document;
        using core::json::json_type;
        using core::json::value;

        constexpr std::array<std::string_view, 7> TYPE_NAMES = {
            "object", "array", "string", "number", "integer", "boolean", "null"
        };

        constexpr std::array<std::string_view, 16> KEYWORDS = {
            "type", "properties", "required", "additionalProperties", "items", "enum", "minimum", "maximum",
            "minItems", "maxItems", "oneOf", "description", "title", "default", "minLength", "maxLength"
        };

        [[nodiscard]] core::error schema_error(std::string_view pointer, std::string_view problem) {
            return core::make_error(protocol_error_code::INVALID_ARGS, std::format("schema {}: {}", pointer, problem));
        }

        [[nodiscard]] core::error instance_error(std::string_view path, std::string_view problem) {
            return core::make_error(protocol_error_code::INVALID_ARGS, std::format("{}: {}", path, problem));
        }

        [[nodiscard]] bool is_type_name(std::string_view name) noexcept {
            for (const std::string_view known : TYPE_NAMES) {
                if (known == name) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool is_keyword(std::string_view name) noexcept {
            for (const std::string_view known : KEYWORDS) {
                if (known == name) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::string_view type_name_of(const value &instance) noexcept {
            if (!instance.is_valid()) {
                return "nothing";
            }
            if (instance.is_integer()) {
                return "integer";
            }
            return core::json::to_string(instance.type());
        }

        [[nodiscard]] bool matches_type(const value &instance, std::string_view typeName) noexcept {
            if (typeName == "object") {
                return instance.is_object();
            }
            if (typeName == "array") {
                return instance.is_array();
            }
            if (typeName == "string") {
                return instance.is_string();
            }
            if (typeName == "number") {
                return instance.is_number();
            }
            if (typeName == "integer") {
                if (!instance.is_number()) {
                    return false;
                }
                if (instance.is_integer()) {
                    return true;
                }
                const f64 real = instance.as_f64();
                return std::isfinite(real) && std::floor(real) == real;
            }
            if (typeName == "boolean") {
                return instance.is_bool();
            }
            if (typeName == "null") {
                return instance.is_null();
            }
            return false;
        }

        core::result<void> check_schema(const value &schema, const std::string &pointer) {
            if (!schema.is_object()) {
                return schema_error(pointer.empty() ? "/" : pointer, "a schema must be an object");
            }
            for (const core::json::member entry : schema.members()) {
                const std::string here = pointer + "/" + std::string(entry.key);
                if (!is_keyword(entry.key)) {
                    return schema_error(here, std::format("unsupported keyword '{}'", entry.key));
                }
                if (entry.key == "type") {
                    if (entry.item.is_string()) {
                        if (!is_type_name(entry.item.as_string())) {
                            return schema_error(here, std::format("unknown type '{}'", entry.item.as_string()));
                        }
                    } else if (entry.item.is_array()) {
                        if (entry.item.size() == 0) {
                            return schema_error(here, "the type list is empty");
                        }
                        for (const value typeName : entry.item) {
                            if (!typeName.is_string() || !is_type_name(typeName.as_string())) {
                                return schema_error(here, "the type list holds something other than a type name");
                            }
                        }
                    } else {
                        return schema_error(here, "type must be a name or a list of names");
                    }
                } else if (entry.key == "properties") {
                    if (!entry.item.is_object()) {
                        return schema_error(here, "properties must be an object");
                    }
                    for (const core::json::member property : entry.item.members()) {
                        CONJURE_TRY(check_schema(property.item, here + "/" + std::string(property.key)));
                    }
                } else if (entry.key == "required") {
                    if (!entry.item.is_array()) {
                        return schema_error(here, "required must be an array of names");
                    }
                    for (const value name : entry.item) {
                        if (!name.is_string()) {
                            return schema_error(here, "required must be an array of names");
                        }
                    }
                } else if (entry.key == "additionalProperties") {
                    if (!entry.item.is_bool()) {
                        return schema_error(here, "additionalProperties must be a boolean");
                    }
                } else if (entry.key == "items") {
                    CONJURE_TRY(check_schema(entry.item, here));
                } else if (entry.key == "enum") {
                    if (!entry.item.is_array() || entry.item.size() == 0) {
                        return schema_error(here, "enum must be a non-empty array");
                    }
                } else if (entry.key == "minimum" || entry.key == "maximum") {
                    if (!entry.item.is_number()) {
                        return schema_error(here, std::format("{} must be a number", entry.key));
                    }
                } else if (entry.key == "minItems" || entry.key == "maxItems" || entry.key == "minLength" ||
                  entry.key == "maxLength") {
                    if (!entry.item.is_integer() || entry.item.as_i64(-1) < 0) {
                        return schema_error(here, std::format("{} must be a non-negative integer", entry.key));
                    }
                } else if (entry.key == "oneOf") {
                    if (!entry.item.is_array() || entry.item.size() == 0) {
                        return schema_error(here, "oneOf must be a non-empty array of schemas");
                    }
                    usize index = 0;
                    for (const value alternative : entry.item) {
                        CONJURE_TRY(check_schema(alternative, here + "/" + std::to_string(index)));
                        ++index;
                    }
                } else if (entry.key == "description" || entry.key == "title") {
                    if (!entry.item.is_string()) {
                        return schema_error(here, std::format("{} must be a string", entry.key));
                    }
                }
            }
            return core::result<void>::success();
        }

        core::result<void> check_instance(
          const value &schema,
          const value &instance,
          const std::string &path,
          bool additionalPropertiesDefault
        ) {
            // type
            const value type = schema["type"];
            if (type.is_valid()) {
                bool matched = false;
                std::string expected = "";
                if (type.is_string()) {
                    matched = matches_type(instance, type.as_string());
                    expected = std::string(type.as_string());
                } else {
                    for (const value typeName : type) {
                        if (!expected.empty()) {
                            expected += " or ";
                        }
                        expected += std::string(typeName.as_string());
                        if (matches_type(instance, typeName.as_string())) {
                            matched = true;
                        }
                    }
                }
                if (!matched) {
                    return instance_error(path, std::format("expected {}, got {}", expected, type_name_of(instance)));
                }
            }

            // enum
            const value allowed = schema["enum"];
            if (allowed.is_valid()) {
                bool found = false;
                for (const value candidate : allowed) {
                    if (candidate == instance) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return instance_error(
                      path,
                      std::format(
                        "value {} is not one of {}",
                        document::from_value(instance).to_string(),
                        document::from_value(allowed).to_string()
                      )
                    );
                }
            }

            // numbers
            if (instance.is_number()) {
                const value minimum = schema["minimum"];
                if (minimum.is_valid() && instance.as_f64() < minimum.as_f64()) {
                    return instance_error(
                      path,
                      std::format("value {} is below the minimum {}", instance.as_f64(), minimum.as_f64())
                    );
                }
                const value maximum = schema["maximum"];
                if (maximum.is_valid() && instance.as_f64() > maximum.as_f64()) {
                    return instance_error(
                      path,
                      std::format("value {} is above the maximum {}", instance.as_f64(), maximum.as_f64())
                    );
                }
            }

            // strings
            if (instance.is_string()) {
                const value minLength = schema["minLength"];
                if (minLength.is_valid() && instance.as_string().size() < minLength.as_u64()) {
                    return instance_error(path, std::format("string shorter than {} characters", minLength.as_u64()));
                }
                const value maxLength = schema["maxLength"];
                if (maxLength.is_valid() && instance.as_string().size() > maxLength.as_u64()) {
                    return instance_error(path, std::format("string longer than {} characters", maxLength.as_u64()));
                }
            }

            // arrays
            if (instance.is_array()) {
                const value minItems = schema["minItems"];
                if (minItems.is_valid() && instance.size() < minItems.as_u64()) {
                    return instance_error(path, std::format("fewer than {} items", minItems.as_u64()));
                }
                const value maxItems = schema["maxItems"];
                if (maxItems.is_valid() && instance.size() > maxItems.as_u64()) {
                    return instance_error(path, std::format("more than {} items", maxItems.as_u64()));
                }
                const value items = schema["items"];
                if (items.is_valid()) {
                    usize index = 0;
                    for (const value element : instance) {
                        CONJURE_TRY(check_instance(items, element, std::format("{}[{}]", path, index), true));
                        ++index;
                    }
                }
            }

            // objects
            if (instance.is_object()) {
                const value properties = schema["properties"];
                const value required = schema["required"];
                if (required.is_valid()) {
                    for (const value name : required) {
                        if (!instance.contains(name.as_string())) {
                            return instance_error(
                              std::format("{}.{}", path, name.as_string()),
                              "required member is missing"
                            );
                        }
                    }
                }
                const value additional = schema["additionalProperties"];
                const bool allowAdditional = additional.is_valid() ? additional.as_bool() : additionalPropertiesDefault;
                for (const core::json::member entry : instance.members()) {
                    const std::string memberPath = std::format("{}.{}", path, entry.key);
                    const value propertySchema = properties.is_valid() ? properties[entry.key] : value();
                    if (propertySchema.is_valid()) {
                        CONJURE_TRY(check_instance(propertySchema, entry.item, memberPath, true));
                    } else if (!allowAdditional) {
                        return instance_error(memberPath, "unknown member");
                    }
                }
            }

            // oneOf (shallow: exactly one alternative must accept the value)
            const value alternatives = schema["oneOf"];
            if (alternatives.is_valid()) {
                u32 matches = 0;
                std::string firstError = "";
                for (const value alternative : alternatives) {
                    const core::result<void> checked = check_instance(alternative, instance, path, true);
                    if (checked.has_value()) {
                        ++matches;
                    } else if (firstError.empty()) {
                        firstError = checked.get_error().message();
                    }
                }
                if (matches != 1) {
                    if (matches == 0) {
                        return instance_error(path, std::format("matches none of the alternatives ({})", firstError));
                    }
                    return instance_error(path, "matches more than one alternative");
                }
            }
            return core::result<void>::success();
        }
    }

    compiled_schema::compiled_schema() = default;
    compiled_schema::~compiled_schema() = default;

    core::result<compiled_schema> compiled_schema::compile(std::string_view schemaJson) {
        core::result<core::json::document> parsed = core::json::document::parse(schemaJson);
        if (!parsed.has_value()) {
            return core::make_error(
              protocol_error_code::INVALID_ARGS,
              std::format("schema does not parse: {}", parsed.get_error().message())
            );
        }
        CONJURE_TRY(check_schema(parsed.value().root(), ""));
        compiled_schema result = {};
        result.mSchema = std::move(parsed).value();
        return result;
    }

    core::result<void> compiled_schema::validate(const value &instance, std::string_view rootName) const {
        if (!instance.is_valid()) {
            return instance_error(rootName, "missing");
        }
        return check_instance(mSchema.root(), instance, std::string(rootName), false);
    }

    core::result<void> validate_args(std::string_view schemaJson, const value &args) {
        CONJURE_TRY_ASSIGN(const compiled_schema schema, compiled_schema::compile(schemaJson));
        return schema.validate(args, "args");
    }

    core::result<compiled_schema> compile_schema(std::string_view schemaJson) {
        return compiled_schema::compile(schemaJson);
    }
}
