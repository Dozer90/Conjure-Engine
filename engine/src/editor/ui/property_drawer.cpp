#include "conjure/editor/ui/property_drawer.h"

#include "editor/ui/ui_support.h"

#include "conjure/core/uuid.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <utility>
#include <vector>

// The default drawers: one ImGui widget per field kind over the component's JSON form (04_editor.md 4.2).
// Each drawer edits the JSON value in place and returns true when the widget reported a change, so the
// inspector writes exactly what the agent protocol would write.

namespace conjure::editor::ui {

    namespace {
        using core::reflect::field_kind;

        [[nodiscard]] const char *label_of(const property_draw_context &context) noexcept {
            return context.label.c_str();
        }

        [[nodiscard]] ImGuiInputTextFlags input_flags(const property_draw_context &context) noexcept {
            return context.readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
        }

        [[nodiscard]] f32 drag_speed(const property_draw_context &context) noexcept {
            if (context.field != nullptr && context.field->attributes.step > 0.0) {
                return static_cast<f32>(context.field->attributes.step);
            }
            return 0.05f;
        }

        [[nodiscard]] std::pair<f64, f64> range_of(
          const property_draw_context &context,
          f64 fallbackMin,
          f64 fallbackMax
        ) noexcept {
            if (context.field == nullptr) {
                return {fallbackMin, fallbackMax};
            }
            return {
              context.field->attributes.min.value_or(fallbackMin),
              context.field->attributes.max.value_or(fallbackMax),
            };
        }

        void show_tooltip(const property_draw_context &context) {
            if (context.field != nullptr && !context.field->attributes.tooltip.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", context.field->attributes.tooltip.c_str());
            }
        }

        bool draw_bool(const property_draw_context &context, core::json::document &value) {
            bool current = value.root().as_bool(false);
            ImGui::BeginDisabled(context.readOnly);
            const bool changed = ImGui::Checkbox(label_of(context), &current);
            ImGui::EndDisabled();
            show_tooltip(context);
            if (changed) {
                value = core::json::document::from_value(core::json::object({{"v", current}}).root()["v"]);
            }
            return changed;
        }

        bool draw_integer(
          const property_draw_context &context,
          core::json::document &value,
          bool isSigned,
          f64 minimum,
          f64 maximum
        ) {
            const std::pair<f64, f64> range = range_of(context, minimum, maximum);
            i64 current = value.root().as_i64(0);
            if (!isSigned) {
                current = static_cast<i64>(value.root().as_u64(0));
            }
            ImGui::BeginDisabled(context.readOnly);
            bool changed = false;
            if (range.first > minimum || range.second < maximum) {
                i32 clamped = static_cast<i32>(std::clamp<i64>(
                  current,
                  static_cast<i64>(range.first),
                  static_cast<i64>(range.second)
                ));
                changed = ImGui::SliderInt(
                  label_of(context),
                  &clamped,
                  static_cast<i32>(range.first),
                  static_cast<i32>(range.second)
                );
                current = clamped;
            } else {
                const ImGuiDataType type = isSigned ? ImGuiDataType_S64 : ImGuiDataType_U64;
                changed = ImGui::InputScalar(
                  label_of(context),
                  type,
                  &current,
                  nullptr,
                  nullptr,
                  nullptr,
                  input_flags(context)
                );
            }
            ImGui::EndDisabled();
            show_tooltip(context);
            if (changed) {
                current = std::clamp<i64>(current, static_cast<i64>(minimum), static_cast<i64>(maximum));
                if (isSigned) {
                    value = core::json::document::from_value(core::json::object({{"v", current}}).root()["v"]);
                } else {
                    value = core::json::document::from_value(
                      core::json::object({{"v", static_cast<u64>(current)}}).root()["v"]
                    );
                }
            }
            return changed;
        }

        bool draw_number(const property_draw_context &context, core::json::document &value, bool singlePrecision) {
            const std::pair<f64, f64> range = range_of(context, 0.0, 0.0);
            ImGui::BeginDisabled(context.readOnly);
            bool changed = false;
            f64 current = value.root().as_f64(0.0);
            if (singlePrecision) {
                f32 single = static_cast<f32>(current);
                if (range.first < range.second) {
                    changed = ImGui::SliderFloat(
                      label_of(context),
                      &single,
                      static_cast<f32>(range.first),
                      static_cast<f32>(range.second)
                    );
                } else {
                    changed = ImGui::DragFloat(label_of(context), &single, drag_speed(context));
                }
                current = single;
            } else {
                changed = ImGui::InputDouble(label_of(context), &current, 0.0, 0.0, "%.6f", input_flags(context));
            }
            ImGui::EndDisabled();
            show_tooltip(context);
            if (changed && std::isfinite(current)) {
                value = core::json::document::from_value(core::json::object({{"v", current}}).root()["v"]);
                return true;
            }
            return false;
        }

        bool draw_string(const property_draw_context &context, core::json::document &value) {
            std::string current = std::string(value.root().as_string(""));
            const bool changed = ImGui::InputText(
              label_of(context),
              &current,
              input_flags(context) | ImGuiInputTextFlags_EnterReturnsTrue
            );
            show_tooltip(context);
            if (changed) {
                value = core::json::document::from_value(core::json::object({{"v", current}}).root()["v"]);
            }
            return changed;
        }

        bool draw_uuid(const property_draw_context &context, core::json::document &value) {
            std::string current = std::string(value.root().as_string(""));
            const bool changed = ImGui::InputText(
              label_of(context),
              &current,
              input_flags(context) | ImGuiInputTextFlags_EnterReturnsTrue
            );
            show_tooltip(context);
            if (!changed) {
                return false;
            }
            if (current.empty()) {
                value = core::json::document::from_value(core::json::object({{"v", ""}}).root()["v"]);
                return true;
            }
            const std::optional<core::uuid> parsed = core::uuid::parse(current);
            if (!parsed.has_value()) {
                return false;
            }
            value = core::json::document::from_value(core::json::object({{"v", parsed->to_string()}}).root()["v"]);
            return true;
        }

        bool read_floats(const core::json::value &source, std::span<f32> out) {
            if (!source.is_array() || source.size() < out.size()) {
                return false;
            }
            for (usize index = 0; index < out.size(); ++index) {
                out[index] = source[index].as_f32(0.0f);
            }
            return true;
        }

        core::json::document floats_to_json(std::span<const f32> values) {
            core::json::document array = core::json::document::array();
            for (const f32 value : values) {
                array.push_back(value);
            }
            return array;
        }

        template <usize Count>
        bool draw_vector(const property_draw_context &context, core::json::document &value, bool asColour) {
            std::array<f32, Count> components = {};
            (void) read_floats(value.root(), components);
            ImGui::BeginDisabled(context.readOnly);
            bool changed = false;
            if (asColour && Count == 4) {
                changed = ImGui::ColorEdit4(label_of(context), components.data(), ImGuiColorEditFlags_Float);
            } else if (asColour && Count == 3) {
                changed = ImGui::ColorEdit3(label_of(context), components.data(), ImGuiColorEditFlags_Float);
            } else {
                changed = ImGui::DragScalarN(
                  label_of(context),
                  ImGuiDataType_Float,
                  components.data(),
                  static_cast<int>(Count),
                  drag_speed(context)
                );
            }
            ImGui::EndDisabled();
            show_tooltip(context);
            if (changed) {
                value = floats_to_json(components);
            }
            return changed;
        }

        [[nodiscard]] bool looks_like_colour(const property_draw_context &context) noexcept {
            if (context.field == nullptr) {
                return false;
            }
            const std::string &name = context.field->name;
            return name.find("colour") != std::string::npos || name.find("color") != std::string::npos ||
              name.find("Colour") != std::string::npos || name.find("Color") != std::string::npos ||
              name.find("tint") != std::string::npos;
        }

        bool draw_quat(const property_draw_context &context, core::json::document &value) {
            std::array<f32, 4> components = {0.0f, 0.0f, 0.0f, 1.0f};
            (void) read_floats(value.root(), components);
            const math::quat rotation = glm::normalize(math::quat(
              components[3],
              components[0],
              components[1],
              components[2]
            ));
            const math::vec3 euler = glm::degrees(glm::eulerAngles(rotation));
            std::array<f32, 3> degrees = {euler.x, euler.y, euler.z};
            ImGui::BeginDisabled(context.readOnly);
            const bool changed = ImGui::DragFloat3(label_of(context), degrees.data(), 0.5f, 0.0f, 0.0f, "%.1f deg");
            ImGui::EndDisabled();
            show_tooltip(context);
            if (!changed) {
                return false;
            }
            const math::quat updated = glm::normalize(math::quat(glm::radians(math::vec3(
              degrees[0],
              degrees[1],
              degrees[2]
            ))));
            const std::array<f32, 4> out = {updated.x, updated.y, updated.z, updated.w};
            value = floats_to_json(out);
            return true;
        }

        bool draw_mat4(const property_draw_context &context, core::json::document &value) {
            std::array<f32, 16> cells = {};
            (void) read_floats(value.root(), cells);
            bool changed = false;
            ImGui::TextUnformatted(label_of(context));
            ImGui::BeginDisabled(context.readOnly);
            for (usize row = 0; row < 4; ++row) {
                ImGui::PushID(static_cast<int>(row));
                changed = ImGui::DragFloat4("##row", cells.data() + row * 4, drag_speed(context)) || changed;
                ImGui::PopID();
            }
            ImGui::EndDisabled();
            if (changed) {
                value = floats_to_json(cells);
            }
            return changed;
        }

        bool draw_transform(const property_draw_context &context, core::json::document &value) {
            if (!value.root().is_object()) {
                value = core::json::object({
                  {"position", core::json::array({0.0f, 0.0f, 0.0f})},
                  {"rotation", core::json::array({0.0f, 0.0f, 0.0f, 1.0f})},
                  {"scale", core::json::array({1.0f, 1.0f, 1.0f})},
                });
            }
            bool changed = false;
            ImGui::TextUnformatted(label_of(context));
            ImGui::Indent();
            core::json::document position = core::json::document::from_value(value.root()["position"]);
            core::json::document rotation = core::json::document::from_value(value.root()["rotation"]);
            core::json::document scale = core::json::document::from_value(value.root()["scale"]);
            property_draw_context child = context;
            child.field = nullptr;
            child.label = "Position";
            if (draw_vector<3>(child, position, false)) {
                value.set("position", position);
                changed = true;
            }
            child.label = "Rotation";
            if (draw_quat(child, rotation)) {
                value.set("rotation", rotation);
                changed = true;
            }
            child.label = "Scale";
            if (draw_vector<3>(child, scale, false)) {
                value.set("scale", scale);
                changed = true;
            }
            ImGui::Unindent();
            return changed;
        }

        bool draw_enum(const property_draw_context &context, core::json::document &value) {
            const core::reflect::enum_info *info = context.field != nullptr ? context.field->enumType : nullptr;
            if (info == nullptr || info->values.empty()) {
                return draw_string(context, value);
            }
            std::string current = std::string(value.root().as_string(""));
            if (value.root().is_integer()) {
                current = info->format_value(value.root().as_i64()).value_or(std::to_string(value.root().as_i64()));
            }
            bool changed = false;
            ImGui::BeginDisabled(context.readOnly);
            if (info->isFlags) {
                ImGui::TextUnformatted(label_of(context));
                ImGui::Indent();
                const i64 bits = info->parse_value(current).value_or(0);
                i64 updated = bits;
                for (const auto &[name, bit] : info->values) {
                    if (bit == 0) {
                        continue;
                    }
                    bool set = (bits & bit) == bit;
                    if (ImGui::Checkbox(name.c_str(), &set)) {
                        updated = set ? (updated | bit) : (updated & ~bit);
                        changed = true;
                    }
                }
                ImGui::Unindent();
                if (changed) {
                    value = core::json::document::from_value(
                      core::json::object(
                        {{"v", info->format_value(updated).value_or(std::to_string(updated))}}
                      ).root()["v"]
                    );
                }
            } else if (ImGui::BeginCombo(label_of(context), current.c_str())) {
                for (const auto &[name, enumValue] : info->values) {
                    (void) enumValue;
                    const bool selected = name == current;
                    if (ImGui::Selectable(name.c_str(), selected)) {
                        value = core::json::document::from_value(core::json::object({{"v", name}}).root()["v"]);
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            show_tooltip(context);
            return changed;
        }

        bool draw_entity(const property_draw_context &context, core::json::document &value) {
            std::string current = value.root().is_string() ? std::string(value.root().as_string()) : std::string("");
            const bool changed = ImGui::InputTextWithHint(
              label_of(context),
              "entity guid or empty",
              &current,
              input_flags(context) | ImGuiInputTextFlags_EnterReturnsTrue
            );
            show_tooltip(context);
            if (!changed) {
                return false;
            }
            if (current.empty()) {
                value = core::json::document::from_value(core::json::object({{"v", nullptr}}).root()["v"]);
                return true;
            }
            const std::optional<core::uuid> parsed = core::uuid::parse(current);
            if (!parsed.has_value()) {
                return false;
            }
            value = core::json::document::from_value(core::json::object({{"v", parsed->to_string()}}).root()["v"]);
            return true;
        }
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // property_drawer_registry

    property_drawer_registry::property_drawer_registry() {
        install_defaults();
    }

    property_drawer_registry::~property_drawer_registry() = default;

    void property_drawer_registry::set(field_kind kind, draw_fn drawer) {
        const usize index = static_cast<usize>(kind);
        if (index >= static_cast<usize>(field_kind::COUNT)) {
            return;
        }
        mDrawers[index] = std::move(drawer);
    }

    const draw_fn &property_drawer_registry::drawer(field_kind kind) const noexcept {
        const usize index = static_cast<usize>(kind);
        if (index >= static_cast<usize>(field_kind::COUNT) || !mDrawers[index]) {
            return mFallback;
        }
        return mDrawers[index];
    }

    bool property_drawer_registry::draw(
      field_kind kind,
      const property_draw_context &context,
      core::json::document &value
    ) const {
        const draw_fn &function = drawer(kind);
        if (!function) {
            return false;
        }
        return function(context, value);
    }

    bool property_drawer_registry::draw_object(
      const core::reflect::type_info &type,
      core::json::document &object,
      bool readOnly
    ) const {
        bool changed = false;
        for (const core::reflect::field_info &field : type.fields) {
            if (field.attributes.hidden) {
                continue;
            }
            property_draw_context context = {};
            context.field = &field;
            context.label = field.name;
            context.readOnly = readOnly || field.attributes.readOnly;
            core::json::document value = core::json::document::from_value(object.root()[field.name]);
            bool fieldChanged = false;
            if (field.kind == field_kind::STRUCT && field.structType != nullptr) {
                if (ImGui::TreeNodeEx(field.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (!value.root().is_object()) {
                        value = core::json::document::object();
                    }
                    fieldChanged = draw_object(*field.structType, value, context.readOnly);
                    ImGui::TreePop();
                }
            } else if (field.kind == field_kind::ARRAY) {
                if (ImGui::TreeNodeEx(field.name.c_str(), ImGuiTreeNodeFlags_None)) {
                    if (!value.root().is_array()) {
                        value = core::json::document::array();
                    }
                    const usize count = value.root().size();
                    for (usize index = 0; index < count; ++index) {
                        property_draw_context element = context;
                        element.label = std::format("[{}]", index);
                        core::json::document item = core::json::document::from_value(value.root()[index]);
                        bool itemChanged = false;
                        ImGui::PushID(static_cast<int>(index));
                        if (field.elementKind == field_kind::STRUCT && field.structType != nullptr) {
                            if (ImGui::TreeNodeEx(element.label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                                itemChanged = draw_object(*field.structType, item, context.readOnly);
                                ImGui::TreePop();
                            }
                        } else {
                            itemChanged = draw(field.elementKind, element, item);
                        }
                        ImGui::PopID();
                        if (itemChanged) {
                            (void) value.set_pointer(std::format("/{}", index), item);
                            fieldChanged = true;
                        }
                    }
                    ImGui::TreePop();
                }
            } else {
                fieldChanged = draw(field.kind, context, value);
            }
            if (fieldChanged) {
                object.set(field.name, value);
                changed = true;
            }
        }
        return changed;
    }

    bool property_drawer_registry::draw_json(std::string_view label, core::json::document &value, bool readOnly) const {
        const std::string labelText = std::string(label);
        property_draw_context context = {};
        context.label = labelText;
        context.readOnly = readOnly;
        const core::json::value root = value.root();
        if (root.is_bool()) {
            return draw(field_kind::BOOL, context, value);
        }
        if (root.is_integer()) {
            return draw(field_kind::I64, context, value);
        }
        if (root.is_number()) {
            return draw(field_kind::F64, context, value);
        }
        if (root.is_string()) {
            return draw(field_kind::STRING, context, value);
        }
        if (root.is_array()) {
            bool numeric = root.size() > 0 && root.size() <= 4;
            for (usize index = 0; numeric && index < root.size(); ++index) {
                numeric = root[index].is_number();
            }
            if (numeric) {
                switch (root.size()) {
                    case 2:
                        return draw(field_kind::VEC2, context, value);
                    case 3:
                        return draw(field_kind::VEC3, context, value);
                    case 4:
                        return draw(field_kind::VEC4, context, value);
                    default:
                        break;
                }
            }
            bool changed = false;
            if (ImGui::TreeNodeEx(labelText.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                const usize count = root.size();
                for (usize index = 0; index < count; ++index) {
                    core::json::document item = core::json::document::from_value(value.root()[index]);
                    ImGui::PushID(static_cast<int>(index));
                    if (draw_json(std::format("[{}]", index), item, readOnly)) {
                        (void) value.set_pointer(std::format("/{}", index), item);
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            return changed;
        }
        if (root.is_object()) {
            bool changed = false;
            if (ImGui::TreeNodeEx(labelText.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                std::vector<std::string> keys = {};
                for (const core::json::member entry : root.members()) {
                    keys.emplace_back(entry.key);
                }
                for (const std::string &key : keys) {
                    core::json::document item = core::json::document::from_value(value.root()[key]);
                    ImGui::PushID(key.c_str());
                    if (draw_json(key, item, readOnly)) {
                        value.set(key, item);
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            return changed;
        }
        ImGui::TextDisabled("%s: null", labelText.c_str());
        return false;
    }

    void property_drawer_registry::install_defaults() {
        set(field_kind::BOOL, &draw_bool);
        set(field_kind::I8, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, true, -128.0, 127.0);
        });
        set(field_kind::U8, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, false, 0.0, 255.0);
        });
        set(field_kind::I16, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, true, -32768.0, 32767.0);
        });
        set(field_kind::U16, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, false, 0.0, 65535.0);
        });
        set(field_kind::I32, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, true, -2147483648.0, 2147483647.0);
        });
        set(field_kind::U32, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, false, 0.0, 4294967295.0);
        });
        set(field_kind::I64, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, true, -9.2e18, 9.2e18);
        });
        set(field_kind::U64, [](const property_draw_context &context, core::json::document &value) {
            return draw_integer(context, value, false, 0.0, 1.8e19);
        });
        set(field_kind::F32, [](const property_draw_context &context, core::json::document &value) {
            return draw_number(context, value, true);
        });
        set(field_kind::F64, [](const property_draw_context &context, core::json::document &value) {
            return draw_number(context, value, false);
        });
        set(field_kind::STRING, &draw_string);
        set(field_kind::STRING_ID, &draw_string);
        set(field_kind::UUID, &draw_uuid);
        set(field_kind::VEC2, [](const property_draw_context &context, core::json::document &value) {
            return draw_vector<2>(context, value, false);
        });
        set(field_kind::VEC3, [](const property_draw_context &context, core::json::document &value) {
            return draw_vector<3>(context, value, looks_like_colour(context));
        });
        set(field_kind::VEC4, [](const property_draw_context &context, core::json::document &value) {
            return draw_vector<4>(context, value, looks_like_colour(context));
        });
        set(field_kind::QUAT, &draw_quat);
        set(field_kind::MAT4, &draw_mat4);
        set(field_kind::TRANSFORM, &draw_transform);
        set(field_kind::ENUM, &draw_enum);
        set(field_kind::ENTITY, &draw_entity);
        set(field_kind::ASSET_REF, &draw_uuid);
        mFallback = [](const property_draw_context &context, core::json::document &value) {
            const std::string text = value.to_string();
            ImGui::TextDisabled("%s: %s", label_of(context), text.c_str());
            return false;
        };
    }
}
