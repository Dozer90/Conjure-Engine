#include "editor/core/commands/command_support.h"
#include "editor/core/component_access.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/vfs/path_utils.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_registry.h"

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <utility>

// reflection.* (docs/design/04_editor.md D15, 4.4): the component catalogue with fields, defaults and
// attributes, one type's field tree and the asset types; everything an agent needs to write component.set
// without reading documentation.

namespace conjure::editor::support {

    namespace {
        core::json::document attributes_json(const core::reflect::field_attributes &attributes) {
            core::json::document result = core::json::document::object();
            if (attributes.min.has_value()) {
                result.set("min", *attributes.min);
            }
            if (attributes.max.has_value()) {
                result.set("max", *attributes.max);
            }
            if (attributes.step != 0.0) {
                result.set("step", attributes.step);
            }
            if (!attributes.tooltip.empty()) {
                result.set("tooltip", std::string_view(attributes.tooltip));
            }
            if (attributes.hidden) {
                result.set("hidden", true);
            }
            if (attributes.readOnly) {
                result.set("readOnly", true);
            }
            if (!attributes.category.empty()) {
                result.set("category", std::string_view(attributes.category));
            }
            // The unit the value, min, max and step are stored and written in ("radians", "degrees"); a plain
            // number has none.
            if (attributes.unit != core::reflect::field_unit::NONE) {
                result.set("unit", core::reflect::to_string(attributes.unit));
            }
            return result;
        }

        core::json::document enum_json(const core::reflect::enum_info &info) {
            core::json::document values = core::json::document::array();
            for (const auto &[name, value] : info.values) {
                values.push_back(core::json::object({{"name", std::string_view(name)}, {"value", value}}));
            }
            return core::json::object({
              {"name", std::string_view(info.name)},
              {"flags", info.isFlags},
              {"underlyingSize", info.underlyingSize},
              {"values", values},
            });
        }

        core::json::document fields_json(const core::reflect::type_info &type, u32 depth);

        core::json::document field_json(const core::reflect::field_info &field, u32 depth) {
            core::json::document result = core::json::object({
              {"name", std::string_view(field.name)},
              {"type", core::reflect::to_string(field.kind)},
              {"offset", field.offset},
              {"size", field.size},
            });
            if (field.is_array()) {
                result.set("elementType", core::reflect::to_string(field.elementKind));
                result.set("count", field.arrayCount);
            }
            if (!field.defaultValueJson.empty()) {
                core::result<core::json::document> parsed = core::json::document::parse(field.defaultValueJson);
                if (parsed.has_value()) {
                    result.set("default", parsed.value());
                } else {
                    result.set("default", std::string_view(field.defaultValueJson));
                }
            } else {
                result.set_null("default");
            }
            result.set("attributes", attributes_json(field.attributes));
            if (field.enumType != nullptr) {
                result.set("enum", enum_json(*field.enumType));
            }
            if (field.structType != nullptr) {
                result.set("structType", std::string_view(field.structType->name));
                if (depth < 8) {
                    result.set("fields", fields_json(*field.structType, depth + 1));
                }
            }
            return result;
        }

        core::json::document fields_json(const core::reflect::type_info &type, u32 depth) {
            core::json::document fields = core::json::document::array();
            for (const core::reflect::field_info &field : type.fields) {
                fields.push_back(field_json(field, depth));
            }
            return fields;
        }

        core::json::document type_json(const core::reflect::type_info &type) {
            return core::json::object({
              {"name", std::string_view(type.name)},
              {"size", type.size},
              {"alignment", type.alignment},
              {"triviallyCopyable", type.isTriviallyCopyable},
              {"layoutHash", std::format("{:016x}", type.layout_hash())},
              {"fields", fields_json(type, 0)},
            });
        }

        std::string module_of_component(editor_context &context, const ecs::component_info &info) {
            modules::module_manager *manager = context.modules();
            if (manager == nullptr) {
                return "engine";
            }
            const modules::module_id owner = manager->registry().owner_of(core::string_id("ecs.component"), info.name);
            if (!owner.is_valid()) {
                return "engine";
            }
            return manager->registry().manifest(owner).name;
        }

        core::json::document flags_json(ecs::component_flags flags) {
            core::json::document list = core::json::document::array();
            if (has_flag(flags, ecs::component_flags::TAG)) {
                list.push_back("tag");
            }
            if (has_flag(flags, ecs::component_flags::TRIVIAL)) {
                list.push_back("trivial");
            }
            if (has_flag(flags, ecs::component_flags::DYNAMIC)) {
                list.push_back("dynamic");
            }
            if (has_flag(flags, ecs::component_flags::TRANSIENT)) {
                list.push_back("transient");
            }
            if (has_flag(flags, ecs::component_flags::EDITOR_HIDDEN)) {
                list.push_back("editor_hidden");
            }
            return list;
        }

        core::json::document component_json(editor_context &context, const ecs::component_info &info, bool withFields) {
            core::json::document result = core::json::object({
              {"name", info.name.to_string()},
              {"id", info.id.value},
              {"module", module_of_component(context, info)},
              {"size", info.size},
              {"flags", flags_json(info.flags)},
              {"editable", access::is_editable(info)},
              {"reflected", info.reflection != nullptr},
            });
            core::json::document aliases = core::json::document::array();
            const core::json::document table = access::alias_table();
            for (const core::json::member entry : table.root().members()) {
                if (entry.item.as_string() == info.name.view()) {
                    aliases.push_back(entry.key);
                }
            }
            result.set("aliases", aliases);
            if (withFields) {
                if (info.reflection != nullptr) {
                    result.set("fields", fields_json(*info.reflection, 0));
                } else {
                    result.set("fields", core::json::document::array());
                }
            }
            return result;
        }

        core::result<command_outcome> reflection_components(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const bool includeHidden = bool_or(invocation.args, "includeHidden", false);
            const bool withFields = bool_or(invocation.args, "fields", true);
            const std::string_view moduleFilter = string_or(invocation.args, "module", "");
            core::json::document components = core::json::document::array();
            for (const ecs::component_info &info : context.components().types()) {
                if (!includeHidden && !access::is_editable(info)) {
                    continue;
                }
                if (!moduleFilter.empty() && module_of_component(context, info) != moduleFilter) {
                    continue;
                }
                components.push_back(component_json(context, info, withFields));
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"components", components}, {"count", components.root().size()}});
            return outcome;
        }

        core::result<command_outcome> reflection_type(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view name, require_string(invocation.args, "name"));
            // A component name (exact, alias or suffix) first, then any reflected struct or enumeration.
            core::json::document details = core::json::document::object();
            const core::result<ecs::component_id> component = access::resolve_component_type(
              context.components(),
              name,
              &details
            );
            if (component.has_value()) {
                const ecs::component_info &info = context.components().info(component.value());
                core::json::document result = component_json(context, info, true);
                if (info.reflection != nullptr) {
                    result.set("type", type_json(*info.reflection));
                }
                command_outcome outcome = {};
                outcome.result = std::move(result);
                return outcome;
            }
            if (component.get_error().is(protocol_error_code::INVALID_ARGS)) {
                invocation.errorDetails = std::move(details);
                return core::error(component.get_error());
            }
            const core::reflect::type_info *type = core::reflect::registry().find(name);
            if (type != nullptr) {
                command_outcome outcome = {};
                outcome.result = core::json::object({
                  {"name", std::string_view(type->name)},
                  {"type", type_json(*type)},
                });
                return outcome;
            }
            const core::reflect::enum_info *enumeration = core::reflect::registry().find_enum(name);
            if (enumeration != nullptr) {
                command_outcome outcome = {};
                outcome.result = core::json::object({
                  {"name", std::string_view(enumeration->name)},
                  {"enum", enum_json(*enumeration)},
                });
                return outcome;
            }
            return core::make_error(
              protocol_error_code::TYPE_NOT_FOUND,
              std::format("no component, struct or enumeration named '{}'", name)
            );
        }

        core::json::document schema_of_settings(const core::json::value &settings) {
            core::json::document properties = core::json::document::object();
            if (settings.is_object()) {
                for (const auto &[key, member] : settings.members()) {
                    std::string_view type = "string";
                    if (member.is_bool()) {
                        type = "boolean";
                    } else if (member.is_integer()) {
                        type = "integer";
                    } else if (member.is_number()) {
                        type = "number";
                    } else if (member.is_array()) {
                        type = "array";
                    } else if (member.is_object()) {
                        type = "object";
                    }
                    properties.set(key, core::json::object({{"type", type}, {"default", member}}));
                }
            }
            return core::json::object({{"type", "object"}, {"properties", properties}, {"additionalProperties", true}});
        }

        core::result<command_outcome> reflection_asset_types(command_invocation &invocation) {
            editor_context &context = invocation.context;
            assets::asset_database *database = context.asset_database();
            if (database == nullptr) {
                return not_available("the asset database is not available (cooked project or no project)");
            }
            // The importer registry is engine-internal: importers and their extensions are learnt from the
            // records (one row per importer id seen) and the settings schema from an existing .meta.
            struct importer_row {
                std::set<std::string> extensions = {};
                std::set<std::string> types = {};
                core::json::document settings = core::json::document::object();
                bool hasSettings = false;
            };
            std::map<std::string, importer_row> importers = {};
            for (const assets::asset_record &record : database->records()) {
                if (record.is_sub_asset() || record.importerId.empty()) {
                    continue;
                }
                importer_row &row = importers[record.importerId];
                const std::string_view fileName = core::vfs::path_utils::filename(record.relativePath);
                const usize dot = fileName.find('.');
                if (dot != std::string_view::npos) {
                    row.extensions.insert(std::string(fileName.substr(dot)));
                }
                const assets::asset_type_desc *type = database->types().find(record.type);
                if (type != nullptr) {
                    row.types.insert(type->name);
                }
                if (!row.hasSettings) {
                    const core::result<assets::asset_meta> meta = database->read_meta(record.id);
                    if (meta.has_value()) {
                        row.settings = meta.value().settings;
                        row.hasSettings = true;
                    }
                }
            }
            core::json::document types = core::json::document::array();
            for (const assets::asset_type_desc *desc : database->types().types()) {
                if (desc == nullptr) {
                    continue;
                }
                core::json::document importerIds = core::json::document::array();
                core::json::document extensions = core::json::document::array();
                core::json::document optionsSchema = core::json::object({
                  {"type", "object"},
                  {"additionalProperties", true},
                });
                for (const auto &[importerId, row] : importers) {
                    if (!row.types.contains(desc->name)) {
                        continue;
                    }
                    importerIds.push_back(std::string_view(importerId));
                    for (const std::string &extension : row.extensions) {
                        extensions.push_back(std::string_view(extension));
                    }
                    if (row.hasSettings) {
                        optionsSchema = schema_of_settings(row.settings.root());
                    }
                }
                types.push_back(core::json::object({
                  {"name", std::string_view(desc->name)},
                  {"version", desc->version},
                  {"flags", static_cast<u32>(desc->flags)},
                  {"importers", importerIds},
                  {"extensions", extensions},
                  {"importOptionsSchema", optionsSchema},
                }));
            }
            core::json::document importerList = core::json::document::array();
            for (const auto &[importerId, row] : importers) {
                core::json::document extensions = core::json::document::array();
                for (const std::string &extension : row.extensions) {
                    extensions.push_back(std::string_view(extension));
                }
                importerList.push_back(core::json::object({
                  {"id", std::string_view(importerId)},
                  {"extensions", extensions},
                  {"importOptionsSchema", row.hasSettings ? schema_of_settings(row.settings.root())
                                                          : core::json::object({
                                                            {"type", "object"},
                                                            {"additionalProperties", true},
                                                          })},
                }));
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"types", types}, {"importers", importerList}});
            return outcome;
        }
    }

    void register_reflection_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "reflection.components",
            "Every component type with its module, flags, aliases and reflected fields (type, default, attributes).",
            R"({"type":"object","properties":{"includeHidden":{"type":"boolean"},"fields":{"type":"boolean"},)"
            R"("module":{"type":"string"}}})",
            R"({"type":"object","properties":{"components":{"type":"array"},"count":{"type":"integer"}}})",
            command_flags::READ_ONLY,
            &reflection_components
          )
        );
        add_command(
          registry,
          make_command(
            "reflection.type",
            "The field tree of a component (name, alias or suffix), a reflected struct or an enumeration.",
            R"({"type":"object","properties":{"name":{"type":"string"}},"required":["name"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::READ_ONLY,
            &reflection_type
          )
        );
        add_command(
          registry,
          make_command(
            "reflection.asset_types",
            "The registered asset types with the importers and extensions seen in the project and their settings "
            "schema.",
            std::string(EMPTY_SCHEMA),
            R"({"type":"object","properties":{"types":{"type":"array"},"importers":{"type":"array"}}})",
            command_flags::REQUIRES_PROJECT | command_flags::READ_ONLY,
            &reflection_asset_types
          )
        );
    }
}
