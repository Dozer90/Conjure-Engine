#include "conjure/editor/ui/property_drawer.h"

#include "editor/ui/ui_support.h"

#include "conjure/core/uuid.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// The default drawers: one ImGui widget per field kind over the component's JSON form (04_editor.md 4.2).
// Each drawer edits the JSON value in place and returns true when the widget reported a change, so the
// inspector writes exactly what the agent protocol would write.
//
// Floats follow the reflection JSON encoding (10_core.md C15): a finite value is a number, +infinity the string
// "inf", -infinity the string "-inf" and NaN null. The float drawers read all four and write back what they read
// for everything the user did not change, so an infinite or NaN value is shown as inf, -inf or nan and survives
// an edit of a neighbouring component. They never add one: a non-finite value the user typed is not written.
//
// A float field's unit attribute (core::reflect::field_unit) changes only what is shown: a RADIANS field is shown
// and typed in degrees, and its min, max and step, authored in radians, are converted with it; a DEGREES field is
// shown as stored. Both carry a "deg" suffix. What is written back is in the stored unit, and only when the user
// changed the value, so a field that is merely shown keeps its exact value.

namespace conjure::editor::ui {

    namespace {
        using core::reflect::field_kind;

        [[nodiscard]] const char *label_of(const property_draw_context &context) noexcept {
            return context.label.c_str();
        }

        [[nodiscard]] ImGuiInputTextFlags input_flags(const property_draw_context &context) noexcept {
            return context.readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
        }

        /// <summary>
        /// How a float field's unit is shown: the factor from the stored value to the shown one and the ImGui
        /// formats that carry the unit. Only F32 and F64 fields (and their array elements) take a unit, as
        /// validate_layout requires; any other field, a hand-built one included, is shown as a plain number.
        /// </summary>
        struct unit_display {
            f64 scale = 1.0;
            const char *singleFormat = "%.3f";
            const char *doubleFormat = "%.6f";
            bool isAngle = false;
        };

        // Degrees per pixel of a drag over an angle without a step: the rate of the rotation widget.
        constexpr f32 ANGLE_DRAG_SPEED = 0.5f;

        [[nodiscard]] unit_display unit_display_of(const property_draw_context &context) noexcept {
            if (context.field == nullptr || !core::reflect::is_float_kind(context.field->value_kind())) {
                return unit_display{};
            }
            switch (context.field->attributes.unit) {
                case core::reflect::field_unit::RADIANS:
                    return unit_display{180.0 / std::numbers::pi_v<f64>, "%.3f deg", "%.6f deg", true};
                case core::reflect::field_unit::DEGREES:
                    return unit_display{1.0, "%.3f deg", "%.6f deg", true};
                case core::reflect::field_unit::NONE:
                    break;
            }
            return unit_display{};
        }

        /// <summary>
        /// Value per pixel of a drag, in the shown unit: the field's step converted from the stored unit, or a
        /// default (half a degree for an angle).
        /// </summary>
        [[nodiscard]] f32 drag_speed(const property_draw_context &context) noexcept {
            const unit_display display = unit_display_of(context);
            if (context.field != nullptr && context.field->attributes.step > 0.0) {
                return static_cast<f32>(context.field->attributes.step * display.scale);
            }
            return display.isAngle ? ANGLE_DRAG_SPEED : 0.05f;
        }

        [[nodiscard]] std::optional<f64> minimum_of(const property_draw_context &context) noexcept {
            return context.field != nullptr ? context.field->attributes.min : std::nullopt;
        }

        [[nodiscard]] std::optional<f64> maximum_of(const property_draw_context &context) noexcept {
            return context.field != nullptr ? context.field->attributes.max : std::nullopt;
        }

        /// <summary>
        /// A double converted to an integer type without undefined behaviour: NaN becomes zero and values outside
        /// the type saturate. The type's limits convert to doubles exactly (powers of two), so the comparisons are
        /// exact at the edges.
        /// </summary>
        template <typename Integer>
        [[nodiscard]] Integer saturate_to(f64 value) noexcept {
            if (std::isnan(value)) {
                return Integer{0};
            }
            if (value <= static_cast<f64>(std::numeric_limits<Integer>::lowest())) {
                return std::numeric_limits<Integer>::lowest();
            }
            if (value >= static_cast<f64>(std::numeric_limits<Integer>::max())) {
                return std::numeric_limits<Integer>::max();
            }
            return static_cast<Integer>(value);
        }

        /// <summary>
        /// A float bound from a double attribute, saturated to the float range.
        /// </summary>
        [[nodiscard]] f32 float_bound(f64 value) noexcept {
            const f64 largest = static_cast<f64>(std::numeric_limits<f32>::max());
            return static_cast<f32>(std::clamp(value, -largest, largest));
        }

        constexpr std::string_view POSITIVE_INFINITY_TEXT = "inf";
        constexpr std::string_view NEGATIVE_INFINITY_TEXT = "-inf";
        // The per-component tolerance under which a rotation still matches the one its cached Euler angles were
        // made for (math::same_rotation): above float rounding of a value written and read back, below any change
        // made on purpose: rotations more than about 0.002 degrees (4e-5 radians) apart are a new rotation, whose
        // angles are decomposed afresh.
        constexpr f32 EULER_CACHE_TOLERANCE = 1.0e-5f;

        /// <summary>
        /// The value of a float in reflection JSON: a number, "inf", "-inf" or null (NaN). 'fallback' for anything
        /// else, a missing value included.
        /// </summary>
        [[nodiscard]] f64 read_float(const core::json::value &source, f64 fallback) noexcept {
            if (source.is_string()) {
                const std::string_view text = source.as_string();
                if (text == POSITIVE_INFINITY_TEXT) {
                    return std::numeric_limits<f64>::infinity();
                }
                if (text == NEGATIVE_INFINITY_TEXT) {
                    return -std::numeric_limits<f64>::infinity();
                }
                return fallback;
            }
            if (source.is_valid() && source.is_null()) {
                return std::numeric_limits<f64>::quiet_NaN();
            }
            return source.as_f64(fallback);
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

        /// <summary>
        /// An integer field edited in its own domain (Integer is i64 for signed kinds, u64 for unsigned ones;
        /// [typeMin, typeMax] is the field kind's range). A slider only when both bounds are set and within
        /// ImGui's slider limits (half the 64-bit range), otherwise a drag clamped to whatever bounds exist, so
        /// a field with one bound stays usable and typed values outside the range are clamped.
        /// </summary>
        template <typename Integer>
        bool draw_integer_as(
          const property_draw_context &context,
          core::json::document &value,
          ImGuiDataType dataType,
          Integer typeMin,
          Integer typeMax
        ) {
            const std::optional<f64> minimum = minimum_of(context);
            const std::optional<f64> maximum = maximum_of(context);
            Integer lower = typeMin;
            Integer upper = typeMax;
            if (minimum.has_value()) {
                lower = std::clamp(saturate_to<Integer>(*minimum), typeMin, typeMax);
            }
            if (maximum.has_value()) {
                upper = std::clamp(saturate_to<Integer>(*maximum), typeMin, typeMax);
            }
            if (lower > upper) {
                lower = typeMin;
                upper = typeMax;
            }
            Integer current = Integer{0};
            if constexpr (std::is_signed_v<Integer>) {
                current = value.root().as_i64(0);
            } else {
                current = value.root().as_u64(0);
            }
            current = std::clamp(current, lower, upper);
            bool sliderFits = upper <= std::numeric_limits<Integer>::max() / 2;
            if constexpr (std::is_signed_v<Integer>) {
                sliderFits = sliderFits && lower >= std::numeric_limits<Integer>::lowest() / 2;
            }
            ImGui::BeginDisabled(context.readOnly);
            bool changed = false;
            if (minimum.has_value() && maximum.has_value() && sliderFits) {
                changed = ImGui::SliderScalar(label_of(context), dataType, &current, &lower, &upper);
            } else {
                const f32 speed = context.field != nullptr && context.field->attributes.step > 0.0 ?
                  static_cast<f32>(context.field->attributes.step) :
                  1.0f;
                changed = ImGui::DragScalar(
                  label_of(context),
                  dataType,
                  &current,
                  speed,
                  &lower,
                  &upper,
                  nullptr,
                  ImGuiSliderFlags_AlwaysClamp
                );
            }
            ImGui::EndDisabled();
            show_tooltip(context);
            if (!changed) {
                return false;
            }
            current = std::clamp(current, lower, upper);
            value = core::json::document::from_value(core::json::object({{"v", current}}).root()["v"]);
            return true;
        }

        /// <summary>
        /// A signed field of storage type Field, edited as i64 within Field's range.
        /// </summary>
        template <typename Field>
        bool draw_signed(const property_draw_context &context, core::json::document &value) {
            return draw_integer_as<i64>(
              context,
              value,
              ImGuiDataType_S64,
              static_cast<i64>(std::numeric_limits<Field>::min()),
              static_cast<i64>(std::numeric_limits<Field>::max())
            );
        }

        /// <summary>
        /// An unsigned field of storage type Field, edited as u64 within Field's range.
        /// </summary>
        template <typename Field>
        bool draw_unsigned(const property_draw_context &context, core::json::document &value) {
            return draw_integer_as<u64>(
              context,
              value,
              ImGuiDataType_U64,
              u64{0},
              static_cast<u64>(std::numeric_limits<Field>::max())
            );
        }

        bool draw_number(const property_draw_context &context, core::json::document &value, bool singlePrecision) {
            // Each bound applies on its own; a missing or inverted one is open. The bounds and the value are in the
            // stored unit; the widget works on them in the shown unit (degrees for a RADIANS field).
            f64 lower = minimum_of(context).value_or(-std::numeric_limits<f64>::infinity());
            f64 upper = maximum_of(context).value_or(std::numeric_limits<f64>::infinity());
            if (lower > upper) {
                lower = -std::numeric_limits<f64>::infinity();
                upper = std::numeric_limits<f64>::infinity();
            }
            const unit_display display = unit_display_of(context);
            const f64 shownLower = lower * display.scale;
            const f64 shownUpper = upper * display.scale;
            f64 shown = read_float(value.root(), 0.0) * display.scale;
            // A slider needs both bounds within ImGui's float slider limits (half the float range) and a finite
            // value to place its grab at; otherwise the clamped drag takes the saturated bounds. The drag leaves
            // an infinity or a NaN alone until a number is typed over it, and shows it as inf, -inf or nan.
            const f64 sliderLimit = static_cast<f64>(std::numeric_limits<f32>::max()) / 2.0;
            const bool sliderFits = std::isfinite(shown) && std::isfinite(shownLower) && std::isfinite(shownUpper) &&
              shownLower < shownUpper && shownLower >= -sliderLimit && shownUpper <= sliderLimit;
            ImGui::BeginDisabled(context.readOnly);
            bool changed = false;
            if (singlePrecision) {
                f32 single = static_cast<f32>(shown);
                if (sliderFits) {
                    changed = ImGui::SliderFloat(
                      label_of(context),
                      &single,
                      float_bound(shownLower),
                      float_bound(shownUpper),
                      display.singleFormat
                    );
                } else {
                    changed = ImGui::DragFloat(
                      label_of(context),
                      &single,
                      drag_speed(context),
                      float_bound(shownLower),
                      float_bound(shownUpper),
                      display.singleFormat,
                      ImGuiSliderFlags_AlwaysClamp
                    );
                }
                shown = single;
            } else {
                changed = ImGui::InputDouble(
                  label_of(context),
                  &shown,
                  0.0,
                  0.0,
                  display.doubleFormat,
                  input_flags(context)
                );
            }
            ImGui::EndDisabled();
            show_tooltip(context);
            if (changed && std::isfinite(shown)) {
                // Back in the stored unit, clamped there so that rounding through the shown unit cannot leave the
                // authored range.
                const f64 stored = std::clamp(shown / display.scale, lower, upper);
                value = core::json::document::from_value(core::json::object({{"v", stored}}).root()["v"]);
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
                out[index] = static_cast<f32>(read_float(source[index], 0.0));
            }
            return true;
        }

        /// <summary>
        /// The inverse of read_floats: a document cannot hold a non-finite number, so an infinity goes back as its
        /// string and a NaN as null.
        /// </summary>
        core::json::document floats_to_json(std::span<const f32> values) {
            core::json::document array = core::json::document::array();
            for (const f32 value : values) {
                if (std::isnan(value)) {
                    array.push_back(nullptr);
                } else if (std::isinf(value)) {
                    array.push_back(value > 0.0f ? POSITIVE_INFINITY_TEXT : NEGATIVE_INFINITY_TEXT);
                } else {
                    array.push_back(value);
                }
            }
            return array;
        }

        [[nodiscard]] bool all_finite(std::span<const f32> values) noexcept {
            return std::all_of(values.begin(), values.end(), [](f32 value) { return std::isfinite(value); });
        }

        /// <summary>
        /// What an edit of a row of floats may write. A component the widget left non-finite goes back to what it
        /// was: either it was not touched (the infinity or NaN the value holds), or the user typed "inf" or "nan"
        /// or dragged a NaN, which adds nothing a field can use. Returns true when a component still differs from
        /// 'before'.
        /// </summary>
        [[nodiscard]] bool accept_float_edit(std::span<const f32> before, std::span<f32> after) noexcept {
            bool changed = false;
            for (usize index = 0; index < after.size(); ++index) {
                if (!std::isfinite(after[index])) {
                    after[index] = before[index];
                    continue;
                }
                // A NaN before compares unequal to the number typed over it, as it must.
                changed = changed || after[index] != before[index];
            }
            return changed;
        }

        template <usize Count>
        bool draw_vector(const property_draw_context &context, core::json::document &value, bool asColour) {
            std::array<f32, Count> components = {};
            (void) read_floats(value.root(), components);
            const std::array<f32, Count> before = components;
            // A colour picker converts its channels to bytes, which an infinity or a NaN has no value for: such a
            // colour is drawn as plain numbers until every channel is finite again.
            const bool asPicker = asColour && all_finite(components);
            ImGui::BeginDisabled(context.readOnly);
            bool changed = false;
            if (asPicker && Count == 4) {
                changed = ImGui::ColorEdit4(label_of(context), components.data(), ImGuiColorEditFlags_Float);
            } else if (asPicker && Count == 3) {
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
            if (!changed || !accept_float_edit(before, components)) {
                return false;
            }
            value = floats_to_json(components);
            return true;
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

        /// <summary>
        /// Where a rotation widget keeps the Euler angles it shows, in the window's ImGui state storage: the
        /// degrees and the quaternion they produced, so the angles are derived from the quaternion only when it
        /// changed elsewhere (an Euler decomposition folds yaw past 90 degrees into (180, 180 - yaw, 180)).
        /// </summary>
        struct euler_cache_keys {
            std::array<ImGuiID, 3> degrees = {};
            std::array<ImGuiID, 4> quaternion = {};
            ImGuiID valid = 0;
        };

        [[nodiscard]] euler_cache_keys euler_cache_keys_of(const property_draw_context &context) {
            euler_cache_keys keys = {};
            ImGui::PushID(label_of(context));
            keys.degrees = {ImGui::GetID("##ex"), ImGui::GetID("##ey"), ImGui::GetID("##ez")};
            keys.quaternion = {ImGui::GetID("##qx"), ImGui::GetID("##qy"), ImGui::GetID("##qz"), ImGui::GetID("##qw")};
            keys.valid = ImGui::GetID("##euler_valid");
            ImGui::PopID();
            return keys;
        }

        void store_euler(const euler_cache_keys &keys, const std::array<f32, 3> &degrees, const math::quat &rotation) {
            ImGuiStorage *storage = ImGui::GetStateStorage();
            for (usize axis = 0; axis < degrees.size(); ++axis) {
                storage->SetFloat(keys.degrees[axis], degrees[axis]);
            }
            storage->SetFloat(keys.quaternion[0], rotation.x);
            storage->SetFloat(keys.quaternion[1], rotation.y);
            storage->SetFloat(keys.quaternion[2], rotation.z);
            storage->SetFloat(keys.quaternion[3], rotation.w);
            storage->SetBool(keys.valid, true);
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
            const euler_cache_keys keys = euler_cache_keys_of(context);
            const ImGuiStorage *storage = ImGui::GetStateStorage();
            const math::quat cached = math::quat(
              storage->GetFloat(keys.quaternion[3], 1.0f),
              storage->GetFloat(keys.quaternion[0]),
              storage->GetFloat(keys.quaternion[1]),
              storage->GetFloat(keys.quaternion[2])
            );
            std::array<f32, 3> degrees = {};
            if (!all_finite(components)) {
                // A quaternion with an infinite or NaN component is no rotation, so its three angles are NaN,
                // shown as nan. They are set here, not decomposed: a NaN that arithmetic produced (an infinity
                // normalises through inf * 0) may carry a sign, which the C library prints ("-nan(ind)", "-nan").
                // Typing an angle repairs the rotation, with the angles that are still not numbers taken as zero.
                degrees.fill(std::numeric_limits<f32>::quiet_NaN());
            } else if (storage->GetBool(keys.valid) && math::same_rotation(cached, rotation, EULER_CACHE_TOLERANCE)) {
                for (usize axis = 0; axis < degrees.size(); ++axis) {
                    degrees[axis] = storage->GetFloat(keys.degrees[axis]);
                }
            } else {
                const math::vec3 euler = glm::degrees(glm::eulerAngles(rotation));
                degrees = {euler.x, euler.y, euler.z};
                store_euler(keys, degrees, rotation);
            }
            const std::array<f32, 3> before = degrees;
            ImGui::BeginDisabled(context.readOnly);
            const bool changed = ImGui::DragFloat3(label_of(context), degrees.data(), 0.5f, 0.0f, 0.0f, "%.1f deg");
            ImGui::EndDisabled();
            show_tooltip(context);
            if (!changed || !accept_float_edit(before, degrees)) {
                return false;
            }
            for (f32 &angle : degrees) {
                angle = std::isfinite(angle) ? angle : 0.0f;
            }
            const math::quat updated = glm::normalize(math::quat(glm::radians(math::vec3(
              degrees[0],
              degrees[1],
              degrees[2]
            ))));
            store_euler(keys, degrees, updated);
            const std::array<f32, 4> out = {updated.x, updated.y, updated.z, updated.w};
            value = floats_to_json(out);
            return true;
        }

        bool draw_mat4(const property_draw_context &context, core::json::document &value) {
            std::array<f32, 16> cells = {};
            (void) read_floats(value.root(), cells);
            const std::array<f32, 16> before = cells;
            bool changed = false;
            ImGui::TextUnformatted(label_of(context));
            ImGui::BeginDisabled(context.readOnly);
            // Scoped by the field label: two matrices of one component must not share the rows' IDs.
            ImGui::PushID(label_of(context));
            for (usize row = 0; row < 4; ++row) {
                ImGui::PushID(static_cast<int>(row));
                changed = ImGui::DragFloat4("##row", cells.data() + row * 4, drag_speed(context)) || changed;
                ImGui::PopID();
            }
            ImGui::PopID();
            ImGui::EndDisabled();
            if (!changed || !accept_float_edit(before, cells)) {
                return false;
            }
            value = floats_to_json(cells);
            return true;
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
            // The children's labels are fixed, so the field label scopes them for a second transform field.
            ImGui::PushID(label_of(context));
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
            ImGui::PopID();
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
                // The checkboxes are labelled by enumerator, so the field label scopes them for a second field.
                ImGui::PushID(label_of(context));
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
                ImGui::PopID();
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
            const core::json::value member = object.root()[field.name];
            core::json::document value = core::json::document::from_value(member);
            if (!member.is_valid() && (field.kind == field_kind::F32 || field.kind == field_kind::F64)) {
                // A member the object does not hold copies as null, which a float drawer reads as NaN. Null means
                // NaN only where the object holds it: a missing float starts at zero like every other number.
                value = core::json::document::from_value(core::json::object({{"v", 0.0}}).root()["v"]);
            }
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
            return draw_signed<i8>(context, value);
        });
        set(field_kind::U8, [](const property_draw_context &context, core::json::document &value) {
            return draw_unsigned<u8>(context, value);
        });
        set(field_kind::I16, [](const property_draw_context &context, core::json::document &value) {
            return draw_signed<i16>(context, value);
        });
        set(field_kind::U16, [](const property_draw_context &context, core::json::document &value) {
            return draw_unsigned<u16>(context, value);
        });
        set(field_kind::I32, [](const property_draw_context &context, core::json::document &value) {
            return draw_signed<i32>(context, value);
        });
        set(field_kind::U32, [](const property_draw_context &context, core::json::document &value) {
            return draw_unsigned<u32>(context, value);
        });
        set(field_kind::I64, [](const property_draw_context &context, core::json::document &value) {
            return draw_signed<i64>(context, value);
        });
        set(field_kind::U64, [](const property_draw_context &context, core::json::document &value) {
            return draw_unsigned<u64>(context, value);
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
