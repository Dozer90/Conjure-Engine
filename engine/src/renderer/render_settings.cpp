#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/string_utils.h"

#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {

    constexpr std::string_view DEFAULTS_PATH = "rendering/defaults";
    constexpr usize GROUP_COUNT = static_cast<usize>(render_settings_group::COUNT);

    /*---------------------------------------------------------------------------*/
    // sanitise helpers

    /// <summary>
    /// Records the first field a sanitise pass changed.
    /// </summary>
    void note_change(std::string_view &first, std::string_view name) noexcept {
        if (first.empty()) {
            first = name;
        }
    }

    /// <summary>
    /// Clamps a float into [minimum, maximum]; a NaN takes 'fallback'. True when the value changed.
    /// </summary>
    [[nodiscard]] bool clamp_field(f32 &value, f32 minimum, f32 maximum, f32 fallback) noexcept {
        if (std::isnan(value)) {
            value = fallback;
            return true;
        }
        if (value < minimum) {
            value = minimum;
            return true;
        }
        if (value > maximum) {
            value = maximum;
            return true;
        }
        return false;
    }

    void sanitise_float(
      std::string_view &first,
      std::string_view name,
      f32 &value,
      f32 minimum,
      f32 maximum,
      f32 fallback
    ) noexcept {
        if (clamp_field(value, minimum, maximum, fallback)) {
            note_change(first, name);
        }
    }

    /// <summary>
    /// Puts an enumerator above 'last' (the enum's last enumerator) back to 'fallback'.
    /// </summary>
    template <typename EnumType>
    void sanitise_enum(
      std::string_view &first,
      std::string_view name,
      EnumType &value,
      EnumType last,
      EnumType fallback
    ) noexcept {
        if (static_cast<u8>(value) <= static_cast<u8>(last)) {
            return;
        }
        value = fallback;
        note_change(first, name);
    }

    /*---------------------------------------------------------------------------*/
    // Group access through reflection

    [[nodiscard]] const core::reflect::type_info *group_type(render_settings_group group) noexcept {
        switch (group) {
            case render_settings_group::POST_PROCESS: {
                return core::reflect::type_info_of<post_process_settings>();
            }
            case render_settings_group::SHADOWS: {
                return core::reflect::type_info_of<shadow_settings>();
            }
            case render_settings_group::SSAO: {
                return core::reflect::type_info_of<ssao_settings>();
            }
            case render_settings_group::COUNT: {
                return nullptr;
            }
        }
        return nullptr;
    }

    [[nodiscard]] u8 *group_data(render_settings &settings, render_settings_group group) noexcept {
        switch (group) {
            case render_settings_group::POST_PROCESS: {
                return reinterpret_cast<u8 *>(&settings.postProcess);
            }
            case render_settings_group::SHADOWS: {
                return reinterpret_cast<u8 *>(&settings.shadows);
            }
            case render_settings_group::SSAO: {
                return reinterpret_cast<u8 *>(&settings.ssao);
            }
            case render_settings_group::COUNT: {
                return nullptr;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const u8 *group_data(const render_settings &settings, render_settings_group group) noexcept {
        return group_data(const_cast<render_settings &>(settings), group);
    }

    /// <summary>
    /// The snake_case spelling of a camelCase member name: each upper-case letter becomes '_' and its lower-case
    /// form ("manualEv100" -> "manual_ev100").
    /// </summary>
    [[nodiscard]] std::string snake_case(std::string_view camel) {
        std::string snake = {};
        snake.reserve(camel.size() + 4);
        for (const char character : camel) {
            if (character >= 'A' && character <= 'Z') {
                snake.push_back('_');
                snake.push_back(core::str::to_lower(character));
                continue;
            }
            snake.push_back(character);
        }
        return snake;
    }

    /// <summary>
    /// True when 'snake' is the snake_case spelling of 'camel', without building it.
    /// </summary>
    [[nodiscard]] bool is_snake_spelling(std::string_view camel, std::string_view snake) noexcept {
        usize position = 0;
        for (const char character : camel) {
            const bool upper = character >= 'A' && character <= 'Z';
            if (upper) {
                if (position >= snake.size() || snake[position] != '_') {
                    return false;
                }
                ++position;
            }
            if (position >= snake.size() || snake[position] != (upper ? core::str::to_lower(character) : character)) {
                return false;
            }
            ++position;
        }
        return position == snake.size();
    }

    [[nodiscard]] const core::reflect::field_info *find_by_key(
      const core::reflect::type_info &type,
      std::string_view key
    ) noexcept {
        for (const core::reflect::field_info &field : type.fields) {
            if (is_snake_spelling(field.name, key)) {
                return &field;
            }
        }
        return nullptr;
    }

    /// <summary>
    /// The value of an enum field stored in 'underlyingSize' bytes at 'address'.
    /// </summary>
    [[nodiscard]] i64 load_enumerator(const u8 *address, u32 underlyingSize) noexcept {
        switch (underlyingSize) {
            case 1: {
                u8 value = 0;
                std::memcpy(&value, address, sizeof(value));
                return static_cast<i64>(value);
            }
            case 2: {
                u16 value = 0;
                std::memcpy(&value, address, sizeof(value));
                return static_cast<i64>(value);
            }
            case 4: {
                u32 value = 0;
                std::memcpy(&value, address, sizeof(value));
                return static_cast<i64>(value);
            }
            default: {
                i64 value = 0;
                std::memcpy(&value, address, sizeof(value));
                return value;
            }
        }
    }

    void store_enumerator(u8 *address, u32 underlyingSize, i64 enumerator) noexcept {
        switch (underlyingSize) {
            case 1: {
                const u8 value = static_cast<u8>(enumerator);
                std::memcpy(address, &value, sizeof(value));
                return;
            }
            case 2: {
                const u16 value = static_cast<u16>(enumerator);
                std::memcpy(address, &value, sizeof(value));
                return;
            }
            case 4: {
                const u32 value = static_cast<u32>(enumerator);
                std::memcpy(address, &value, sizeof(value));
                return;
            }
            default: {
                std::memcpy(address, &enumerator, sizeof(enumerator));
                return;
            }
        }
    }

    /// <summary>
    /// The one enumerator the settings refuse although its enum has it: a shadow filter of DEFAULT means
    /// "use shadow_settings::defaultFilter" on a light, so the setting itself cannot be DEFAULT (02 R9).
    /// </summary>
    [[nodiscard]] bool is_refused_enumerator(
      render_settings_group group,
      const core::reflect::field_info &field,
      i64 enumerator
    ) noexcept {
        return group == render_settings_group::SHADOWS && field.name == "defaultFilter" &&
          enumerator == static_cast<i64>(shadow_filter::DEFAULT);
    }

    [[nodiscard]] std::string allowed_enumerators(render_settings_group group, const core::reflect::field_info &field) {
        std::string names = {};
        for (const std::pair<std::string, i64> &entry : field.enumType->values) {
            if (is_refused_enumerator(group, field, entry.second)) {
                continue;
            }
            if (!names.empty()) {
                names.append(", ");
            }
            names.append(core::str::to_lower(entry.first));
        }
        return names;
    }

    /// <summary>
    /// The range of a numeric field as text ("[0, 1]"); f32 bounds are printed as f32 so 0.05f reads "0.05".
    /// </summary>
    [[nodiscard]] std::string range_text(const core::reflect::field_info &field) {
        const core::reflect::field_attributes &attributes = field.attributes;
        const bool isFloat = field.kind == core::reflect::field_kind::F32;
        const auto bound = [isFloat](f64 value) {
            return isFloat ? std::format("{}", static_cast<f32>(value)) : std::format("{}", value);
        };
        if (attributes.min.has_value() && attributes.max.has_value()) {
            return std::format("[{}, {}]", bound(*attributes.min), bound(*attributes.max));
        }
        if (attributes.min.has_value()) {
            return std::format("at least {}", bound(*attributes.min));
        }
        if (attributes.max.has_value()) {
            return std::format("at most {}", bound(*attributes.max));
        }
        return "any value";
    }

    [[nodiscard]] std::string wrong_type(std::string_view expected, const core::json::value &item) {
        return std::format("expected {}, got {}", expected, core::json::to_string(item.type()));
    }

    [[nodiscard]] std::string read_float(
      const core::reflect::field_info &field,
      const core::json::value &item,
      u8 *address
    ) {
        if (!item.is_number()) {
            return wrong_type("a number", item);
        }
        const f64 number = item.as_f64();
        // No core::json document holds a non-finite number (the parser and the mutation API refuse one); a
        // backstop so the reader's contract does not depend on that.
        if (!std::isfinite(number)) {
            return "expected a finite number";
        }
        const core::reflect::field_attributes &attributes = field.attributes;
        const bool fitsFloat = std::fabs(number) <= static_cast<f64>(std::numeric_limits<f32>::max());
        const f32 value = fitsFloat ? static_cast<f32>(number) : 0.0f;
        // Compared as f32, so the f32 bound 0.05f accepts the JSON spelling 0.05 that rounds to it.
        const bool belowMinimum = attributes.min.has_value() && value < static_cast<f32>(*attributes.min);
        const bool aboveMaximum = attributes.max.has_value() && value > static_cast<f32>(*attributes.max);
        if (!fitsFloat || belowMinimum || aboveMaximum) {
            return std::format("{} is outside {}", number, range_text(field));
        }
        std::memcpy(address, &value, sizeof(value));
        return {};
    }

    [[nodiscard]] std::string read_unsigned(
      const core::reflect::field_info &field,
      const core::json::value &item,
      u8 *address
    ) {
        if (!item.is_number()) {
            return wrong_type("a whole number", item);
        }
        const f64 number = item.as_f64();
        // isfinite is the same backstop as in read_float.
        if (!std::isfinite(number) || std::floor(number) != number) {
            return std::format("expected a whole number, got {}", number);
        }
        const core::reflect::field_attributes &attributes = field.attributes;
        const bool belowMinimum = number < 0.0 || (attributes.min.has_value() && number < *attributes.min);
        const bool aboveMaximum = number > static_cast<f64>(std::numeric_limits<u32>::max()) ||
          (attributes.max.has_value() && number > *attributes.max);
        if (belowMinimum || aboveMaximum) {
            return std::format("{} is outside {}", number, range_text(field));
        }
        const u32 value = static_cast<u32>(number);
        std::memcpy(address, &value, sizeof(value));
        return {};
    }

    [[nodiscard]] std::string read_bool(const core::json::value &item, u8 *address) {
        if (!item.is_bool()) {
            return wrong_type("true or false", item);
        }
        const bool value = item.as_bool();
        std::memcpy(address, &value, sizeof(value));
        return {};
    }

    [[nodiscard]] std::string read_enumerator(
      render_settings_group group,
      const core::reflect::field_info &field,
      const core::json::value &item,
      u8 *address
    ) {
        if (!item.is_string()) {
            return wrong_type(std::format("one of {}", allowed_enumerators(group, field)), item);
        }
        const std::string_view text = item.as_string();
        for (const std::pair<std::string, i64> &entry : field.enumType->values) {
            if (!core::str::iequals(entry.first, text) || is_refused_enumerator(group, field, entry.second)) {
                continue;
            }
            store_enumerator(address, field.enumType->underlyingSize, entry.second);
            return {};
        }
        return std::format("'{}' is not one of {}", text, allowed_enumerators(group, field));
    }

    /// <summary>
    /// Reads one leaf into the field's storage; the reason it was refused, or "" when it was stored. A refused
    /// leaf leaves the storage as it was.
    /// </summary>
    [[nodiscard]] std::string read_leaf(
      render_settings_group group,
      const core::reflect::field_info &field,
      const core::json::value &item,
      u8 *address
    ) {
        switch (field.kind) {
            case core::reflect::field_kind::F32: {
                return read_float(field, item, address);
            }
            case core::reflect::field_kind::U32: {
                return read_unsigned(field, item, address);
            }
            case core::reflect::field_kind::BOOL: {
                return read_bool(item, address);
            }
            case core::reflect::field_kind::ENUM: {
                if (field.enumType == nullptr) {
                    return "the field's enumeration is not registered";
                }
                return read_enumerator(group, field, item, address);
            }
            default: {
                CONJURE_ASSERT(false, "render settings: field '{}' has a kind the reader does not know", field.name);
                return std::format("field kind {} is not readable", field.kind);
            }
        }
    }

    void add_problem(render_settings_reading &reading, std::string path, std::string reason) {
        render_settings_problem problem = {};
        problem.path = std::move(path);
        problem.reason = std::move(reason);
        reading.errors.push_back(std::move(problem));
    }

    /// <summary>
    /// The automatic exposure range must not be inverted: when it is, both bounds go back to their defaults and
    /// one error names max_ev100, or min_ev100 when only that key was given.
    /// </summary>
    void check_exposure_range(
      render_settings_reading &reading,
      const std::string &groupPath,
      const core::json::value &groupValue
    ) {
        post_process_settings &value = reading.settings.postProcess;
        if (value.maxEv100 >= value.minEv100) {
            return;
        }
        const bool maxGiven = groupValue.contains("max_ev100");
        const std::string_view key = maxGiven ? "max_ev100" : "min_ev100";
        add_problem(
          reading,
          std::format("{}/{}", groupPath, key),
          std::format("max_ev100 {} is below min_ev100 {}", value.maxEv100, value.minEv100)
        );
        const post_process_settings defaults = {};
        value.minEv100 = defaults.minEv100;
        value.maxEv100 = defaults.maxEv100;
    }

    void read_group(
      render_settings_reading &reading,
      render_settings_group group,
      const core::json::value &groupValue
    ) {
        const std::string groupPath = std::format("{}/{}", DEFAULTS_PATH, to_string(group));
        if (!groupValue.is_object()) {
            add_problem(reading, groupPath, wrong_type("an object", groupValue));
            return;
        }
        const core::reflect::type_info *type = group_type(group);
        if (type == nullptr) {
            add_problem(reading, groupPath, "the group's reflection description is not registered");
            return;
        }

        u8 *data = group_data(reading.settings, group);
        const render_settings defaults = {};
        const u8 *defaultData = group_data(defaults, group);
        for (const core::json::member &entry : groupValue.members()) {
            const core::reflect::field_info *field = find_by_key(*type, entry.key);
            if (field == nullptr) {
                reading.unknownKeys.push_back(std::format("{}/{}", groupPath, entry.key));
                continue;
            }
            std::string reason = read_leaf(group, *field, entry.item, data + field->offset);
            if (reason.empty()) {
                continue;
            }
            // A repeated key that was valid earlier also returns to the engine default.
            std::memcpy(data + field->offset, defaultData + field->offset, field->size);
            add_problem(reading, std::format("{}/{}", groupPath, entry.key), std::move(reason));
        }

        if (group == render_settings_group::POST_PROCESS) {
            check_exposure_range(reading, groupPath, groupValue);
        }
    }

    /// <summary>
    /// Writes one field in project.json form into a group object.
    /// </summary>
    void write_leaf(core::json::document &out, const core::reflect::field_info &field, const u8 *address) {
        const std::string key = snake_case(field.name);
        switch (field.kind) {
            case core::reflect::field_kind::F32: {
                f32 value = 0.0f;
                std::memcpy(&value, address, sizeof(value));
                out.set(key, value);
                return;
            }
            case core::reflect::field_kind::U32: {
                u32 value = 0;
                std::memcpy(&value, address, sizeof(value));
                out.set(key, value);
                return;
            }
            case core::reflect::field_kind::BOOL: {
                bool value = false;
                std::memcpy(&value, address, sizeof(value));
                out.set(key, value);
                return;
            }
            case core::reflect::field_kind::ENUM: {
                if (field.enumType == nullptr) {
                    out.set_null(key);
                    return;
                }
                const i64 enumerator = load_enumerator(address, field.enumType->underlyingSize);
                const std::string *name = field.enumType->find_name(enumerator);
                if (name == nullptr) {
                    out.set(key, enumerator);
                    return;
                }
                out.set(key, std::string_view(core::str::to_lower(*name)));
                return;
            }
            default: {
                CONJURE_ASSERT(false, "render settings: field '{}' has a kind the writer does not know", field.name);
                out.set_null(key);
                return;
            }
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// sanitise

std::string_view conjure::renderer::sanitise(post_process_settings &value) noexcept {
    const post_process_settings defaults = {};
    std::string_view first = {};
    sanitise_float(first, "manual_ev100", value.manualEv100, MIN_EV100, MAX_EV100, defaults.manualEv100);
    sanitise_float(
      first,
      "exposure_compensation",
      value.exposureCompensation,
      -MAX_EXPOSURE_COMPENSATION,
      MAX_EXPOSURE_COMPENSATION,
      defaults.exposureCompensation
    );
    sanitise_float(
      first,
      "adaptation_speed_up",
      value.adaptationSpeedUp,
      0.0f,
      MAX_ADAPTATION_SPEED,
      defaults.adaptationSpeedUp
    );
    sanitise_float(
      first,
      "adaptation_speed_down",
      value.adaptationSpeedDown,
      0.0f,
      MAX_ADAPTATION_SPEED,
      defaults.adaptationSpeedDown
    );
    sanitise_float(first, "min_ev100", value.minEv100, MIN_EV100, MAX_EV100, defaults.minEv100);
    sanitise_float(first, "max_ev100", value.maxEv100, MIN_EV100, MAX_EV100, defaults.maxEv100);
    if (value.maxEv100 < value.minEv100) {
        value.maxEv100 = value.minEv100;
        note_change(first, "max_ev100");
    }
    sanitise_float(first, "bloom_intensity", value.bloomIntensity, 0.0f, MAX_BLOOM_INTENSITY, defaults.bloomIntensity);
    sanitise_float(
      first,
      "reinhard_white_point",
      value.reinhardWhitePoint,
      MIN_REINHARD_WHITE_POINT,
      MAX_REINHARD_WHITE_POINT,
      defaults.reinhardWhitePoint
    );
    sanitise_enum(first, "exposure", value.exposure, exposure_mode::MANUAL, defaults.exposure);
    sanitise_enum(first, "tonemap", value.tonemap, tonemap_operator::NONE, defaults.tonemap);
    sanitise_enum(first, "agx_look", value.agxLook, agx_look::PUNCHY, defaults.agxLook);
    return first;
}

std::string_view conjure::renderer::sanitise(shadow_settings &value) noexcept {
    const shadow_settings defaults = {};
    std::string_view first = {};
    sanitise_float(first, "distance", value.distance, MIN_SHADOW_DISTANCE, MAX_SHADOW_DISTANCE, defaults.distance);
    sanitise_float(first, "split_lambda", value.splitLambda, 0.0f, 1.0f, defaults.splitLambda);
    sanitise_float(first, "caster_padding", value.casterPadding, 0.0f, MAX_CASTER_PADDING, defaults.casterPadding);
    sanitise_float(
      first,
      "pcf_radius_texels",
      value.pcfRadiusTexels,
      0.0f,
      MAX_SHADOW_FILTER_TEXELS,
      defaults.pcfRadiusTexels
    );
    sanitise_float(
      first,
      "normal_offset_texels",
      value.normalOffsetTexels,
      0.0f,
      MAX_SHADOW_FILTER_TEXELS,
      defaults.normalOffsetTexels
    );
    if (value.cascadeCount < 1) {
        value.cascadeCount = 1;
        note_change(first, "cascade_count");
    } else if (value.cascadeCount > MAX_SHADOW_CASCADES) {
        value.cascadeCount = MAX_SHADOW_CASCADES;
        note_change(first, "cascade_count");
    }
    if (value.defaultFilter == shadow_filter::DEFAULT) {
        value.defaultFilter = defaults.defaultFilter;
        note_change(first, "default_filter");
    } else {
        sanitise_enum(first, "default_filter", value.defaultFilter, shadow_filter::PCSS, defaults.defaultFilter);
    }
    return first;
}

std::string_view conjure::renderer::sanitise(ssao_settings &value) noexcept {
    const ssao_settings defaults = {};
    std::string_view first = {};
    sanitise_float(first, "radius", value.radius, MIN_SSAO_RADIUS, MAX_SSAO_RADIUS, defaults.radius);
    sanitise_float(first, "intensity", value.intensity, 0.0f, MAX_SSAO_INTENSITY, defaults.intensity);
    sanitise_float(first, "thickness", value.thickness, 0.0f, 1.0f, defaults.thickness);
    return first;
}

/*===========================================================================*/
/*===========================================================================*/
// project.json form

render_settings_reading conjure::renderer::read_render_settings(const core::json::value &defaults) {
    render_settings_reading reading = {};
    if (defaults.is_null()) {
        return reading;
    }
    if (!defaults.is_object()) {
        add_problem(reading, std::string(DEFAULTS_PATH), wrong_type("an object", defaults));
        return reading;
    }

    for (const core::json::member &entry : defaults.members()) {
        bool known = false;
        for (usize index = 0; index < GROUP_COUNT; ++index) {
            const render_settings_group group = static_cast<render_settings_group>(index);
            if (entry.key != to_string(group)) {
                continue;
            }
            known = true;
            read_group(reading, group, entry.item);
        }
        if (!known) {
            reading.unknownKeys.push_back(std::format("{}/{}", DEFAULTS_PATH, entry.key));
        }
    }
    return reading;
}

core::result<render_settings> conjure::renderer::render_settings_from_json(const core::json::value &defaults) {
    render_settings_reading reading = read_render_settings(defaults);
    if (!reading.errors.empty()) {
        const render_settings_problem &problem = reading.errors.front();
        return core::make_error(renderer_error::INVALID_SETTINGS, "{}: {}", problem.path, problem.reason);
    }
    return reading.settings;
}

core::json::document conjure::renderer::render_setting_leaf_to_json(
  render_settings_group group,
  std::string_view camelField,
  const core::json::value &reflectedValue
) {
    core::json::document leaf = core::json::document::object();
    const core::reflect::type_info *type = group_type(group);
    const core::reflect::field_info *field = type == nullptr ? nullptr : type->find_field(camelField);
    if (field == nullptr) {
        CONJURE_ASSERT(false, "render_setting_leaf_to_json: {} has no field '{}'", to_string(group), camelField);
        return leaf;
    }

    core::json::document member = core::json::document::object();
    const std::string key = snake_case(field->name);
    if (!reflectedValue.is_valid()) {
        member.set_null(key);
    } else if (field->kind == core::reflect::field_kind::ENUM && reflectedValue.is_string()) {
        member.set(key, std::string_view(core::str::to_lower(reflectedValue.as_string())));
    } else {
        member.set(key, reflectedValue);
    }
    leaf.set(to_string(group), member);
    return leaf;
}

core::json::document conjure::renderer::render_settings_to_json(const render_settings &settings) {
    core::json::document out = core::json::document::object();
    for (usize index = 0; index < GROUP_COUNT; ++index) {
        const render_settings_group group = static_cast<render_settings_group>(index);
        const core::reflect::type_info *type = group_type(group);
        if (type == nullptr) {
            CONJURE_ASSERT(false, "render_settings_to_json: group {} has no reflection description", index);
            continue;
        }
        const u8 *data = group_data(settings, group);
        core::json::document groupObject = core::json::document::object();
        for (const core::reflect::field_info &field : type->fields) {
            write_leaf(groupObject, field, data + field.offset);
        }
        out.set(to_string(group), groupObject);
    }
    return out;
}

/*===========================================================================*/
/*===========================================================================*/
// Names

std::string_view conjure::renderer::to_string(tonemap_operator value) noexcept {
    switch (value) {
        case tonemap_operator::ACES_FITTED: {
            return "ACES_FITTED";
        }
        case tonemap_operator::REINHARD_EXTENDED: {
            return "REINHARD_EXTENDED";
        }
        case tonemap_operator::AGX: {
            return "AGX";
        }
        case tonemap_operator::NONE: {
            return "NONE";
        }
    }
    return "ACES_FITTED";
}

std::string_view conjure::renderer::to_string(exposure_mode value) noexcept {
    switch (value) {
        case exposure_mode::AUTOMATIC: {
            return "AUTOMATIC";
        }
        case exposure_mode::MANUAL: {
            return "MANUAL";
        }
    }
    return "AUTOMATIC";
}

std::string_view conjure::renderer::to_string(agx_look value) noexcept {
    switch (value) {
        case agx_look::DEFAULT: {
            return "DEFAULT";
        }
        case agx_look::GOLDEN: {
            return "GOLDEN";
        }
        case agx_look::PUNCHY: {
            return "PUNCHY";
        }
    }
    return "DEFAULT";
}

std::string_view conjure::renderer::to_string(render_settings_group value) noexcept {
    switch (value) {
        case render_settings_group::POST_PROCESS: {
            return "post_process";
        }
        case render_settings_group::SHADOWS: {
            return "shadows";
        }
        case render_settings_group::SSAO: {
            return "ssao";
        }
        case render_settings_group::COUNT: {
            return "";
        }
    }
    return "";
}

/*===========================================================================*/
/*===========================================================================*/
// render_defaults

void render_defaults::set(const render_settings &value) noexcept {
    render_settings sanitised = value;
    static_cast<void>(sanitise(sanitised.postProcess));
    static_cast<void>(sanitise(sanitised.shadows));
    static_cast<void>(sanitise(sanitised.ssao));
    mValue = sanitised;
    ++mRevision;
}
