#include "editor/core/commands/command_support.h"

#include "conjure/editor/log_category.h"

#include "conjure/core/string_utils.h"
#include "conjure/platform/button_action.h"
#include "conjure/platform/event.h"
#include "conjure/platform/gamepad.h"
#include "conjure/platform/input.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"

#include <format>
#include <memory>
#include <optional>
#include <variant>

// input.* (docs/design/04_editor.md 4.4, 7): synthetic input through platform::input::inject, which runs the
// same state machine on the null backend as on glfw. A "tap" presses now and releases holdFrames frames later
// through a frame task, so a game sees the press for at least one UPDATE.

namespace conjure::editor::support {

    namespace {
        /// <summary>
        /// Releases a tapped key or button once the hold frames elapsed.
        /// </summary>
        class tap_release_task final : public frame_task {
            private:
                platform::input_event mRelease = {};
                u32 mFramesLeft = 0;

            public:
                tap_release_task(platform::input_event release, u32 holdFrames) :
                    mRelease(release),
                    mFramesLeft(holdFrames) {}

                ~tap_release_task() override = default;

            public:
                bool tick(editor_context &context) override {
                    if (mFramesLeft > 0) {
                        --mFramesLeft;
                        return false;
                    }
                    platform::input *input = context.input();
                    if (input != nullptr) {
                        input->inject(mRelease);
                    }
                    return true;
                }
        };

        core::result<math::vec3> read_vec3_2d(const core::json::value &value) {
            if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
                return invalid_args("expected [x, y]");
            }
            return math::vec3(value[0].as_f32(), value[1].as_f32(), 0.0f);
        }

        enum class inject_action : u8 {
            PRESS,
            RELEASE,
            TAP
        };

        core::result<inject_action> read_action(const core::json::value &args) {
            const std::string_view text = string_or(args, "action", "tap");
            if (text == "press") {
                return inject_action::PRESS;
            }
            if (text == "release") {
                return inject_action::RELEASE;
            }
            if (text == "tap") {
                return inject_action::TAP;
            }
            return invalid_args("args.action: expected \"press\", \"release\" or \"tap\"");
        }

        core::result<platform::key_modifier_flags> read_modifiers(const core::json::value &args) {
            if (!args.contains("modifiers")) {
                return platform::key_modifier_flags::NONE;
            }
            const std::optional<platform::key_modifier_flags> parsed =
              platform::parse_key_modifier_flags(string_or(args, "modifiers", ""));
            if (!parsed.has_value()) {
                return invalid_args("args.modifiers: expected names joined with '|' (\"SHIFT|CONTROL\")");
            }
            return *parsed;
        }

        core::result<command_outcome> input_inject(command_invocation &invocation) {
            editor_context &context = invocation.context;
            platform::input *input = context.input();
            if (input == nullptr) {
                return not_available("the platform input is not available");
            }
            const core::json::value &args = invocation.args;
            CONJURE_TRY_ASSIGN(const inject_action action, read_action(args));
            CONJURE_TRY_ASSIGN(const u32 holdFrames, u32_or(args, "holdFrames", 1));
            CONJURE_TRY_ASSIGN(const platform::key_modifier_flags modifiers, read_modifiers(args));
            const bool pressed = action != inject_action::RELEASE;
            const platform::button_action buttonAction =
              pressed ? platform::button_action::PRESS : platform::button_action::RELEASE;
            u32 injected = 0;
            std::optional<platform::input_event> release = std::nullopt;
            core::json::document result = core::json::document::object();

            if (args.contains("position")) {
                CONJURE_TRY_ASSIGN(const math::vec3 position, read_vec3_2d(args["position"]));
                platform::mouse_move_event move = {};
                move.position = math::vec2(position.x, position.y);
                input->inject(move);
                ++injected;
                result.set("position", core::json::array({position.x, position.y}));
            }
            if (args.contains("key")) {
                const std::string_view keyName = string_or(args, "key", "");
                const std::optional<platform::key> key = platform::parse_key(keyName);
                if (!key.has_value()) {
                    return invalid_args(std::format("args.key: unknown key '{}'", keyName));
                }
                platform::key_event event = {};
                event.keyCode = *key;
                event.action = buttonAction;
                event.modifiers = modifiers;
                input->inject(event);
                ++injected;
                result.set("key", platform::to_string(*key));
                if (action == inject_action::TAP) {
                    event.action = platform::button_action::RELEASE;
                    release = platform::input_event(event);
                }
            }
            if (args.contains("button")) {
                const std::string_view buttonName = string_or(args, "button", "");
                const std::optional<platform::mouse_button> button = platform::parse_mouse_button(buttonName);
                if (!button.has_value()) {
                    return invalid_args(std::format("args.button: unknown mouse button '{}'", buttonName));
                }
                platform::mouse_button_event event = {};
                event.button = *button;
                event.action = buttonAction;
                event.modifiers = modifiers;
                input->inject(event);
                ++injected;
                result.set("button", platform::to_string(*button));
                if (action == inject_action::TAP) {
                    event.action = platform::button_action::RELEASE;
                    release = platform::input_event(event);
                }
            }
            if (args.contains("gamepadButton")) {
                const std::string_view name = string_or(args, "gamepadButton", "");
                const std::optional<platform::gamepad_button> button = platform::parse_gamepad_button(name);
                if (!button.has_value()) {
                    return invalid_args(std::format("args.gamepadButton: unknown button '{}'", name));
                }
                CONJURE_TRY_ASSIGN(const u32 gamepad, u32_or(args, "gamepad", 0));
                if (gamepad >= platform::MAX_GAMEPADS) {
                    return invalid_args(std::format("args.gamepad: expected 0..{}", platform::MAX_GAMEPADS - 1));
                }
                platform::gamepad_button_event event = {};
                event.gamepadIndex = static_cast<u8>(gamepad);
                event.button = *button;
                event.action = buttonAction;
                input->inject(event);
                ++injected;
                result.set("gamepadButton", platform::to_string(*button));
                if (action == inject_action::TAP) {
                    event.action = platform::button_action::RELEASE;
                    release = platform::input_event(event);
                }
            }
            if (args.contains("axis")) {
                const std::string_view axisName = string_or(args, "axis", "");
                const std::optional<platform::gamepad_axis> axis = platform::parse_gamepad_axis(axisName);
                if (!axis.has_value()) {
                    return invalid_args(std::format("args.axis: unknown gamepad axis '{}'", axisName));
                }
                CONJURE_TRY_ASSIGN(const u32 gamepad, u32_or(args, "gamepad", 0));
                if (gamepad >= platform::MAX_GAMEPADS) {
                    return invalid_args(std::format("args.gamepad: expected 0..{}", platform::MAX_GAMEPADS - 1));
                }
                CONJURE_TRY_ASSIGN(const f32 value, f32_or(args, "value", 0.0f));
                if (value < -1.0f || value > 1.0f) {
                    return invalid_args("args.value: expected -1..1");
                }
                platform::gamepad_axis_event event = {};
                event.gamepadIndex = static_cast<u8>(gamepad);
                event.axis = *axis;
                event.value = value;
                input->inject(event);
                ++injected;
                result.set("axis", platform::to_string(*axis));
                result.set("value", value);
            }
            if (args.contains("scroll")) {
                CONJURE_TRY_ASSIGN(const math::vec3 offset, read_vec3_2d(args["scroll"]));
                platform::mouse_scroll_event event = {};
                event.offset = math::vec2(offset.x, offset.y);
                input->inject(event);
                ++injected;
            }
            if (args.contains("text")) {
                const std::string_view text = string_or(args, "text", "");
                if (!core::str::is_valid_utf8(text)) {
                    return invalid_args("args.text: expected valid UTF-8");
                }
                // One text event per code point, not per byte: accents and CJK arrive whole.
                for (const char32_t codepoint : core::str::to_utf32(text)) {
                    platform::text_event event = {};
                    event.codepoint = static_cast<u32>(codepoint);
                    input->inject(event);
                    ++injected;
                }
            }
            if (injected == 0) {
                return invalid_args(
                  "args: expected at least one of key, button, gamepadButton, axis, position, scroll, text"
                );
            }
            if (release.has_value()) {
                context.add_frame_task(std::make_unique<tap_release_task>(*release, holdFrames));
            }
            result.set("injected", injected);
            result.set("action", string_or(args, "action", "tap"));
            result.set("releasePending", release.has_value());
            result.set("frame", context.frame_index());
            command_outcome outcome = {};
            outcome.result = std::move(result);
            return outcome;
        }

        core::result<command_outcome> input_clear(command_invocation &invocation) {
            platform::input *input = invocation.context.input();
            if (input == nullptr) {
                return not_available("the platform input is not available");
            }
            input->release_all();
            command_outcome outcome = {};
            outcome.result = core::json::object({{"cleared", true}, {"frame", invocation.context.frame_index()}});
            return outcome;
        }
    }

    void register_input_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "input.inject",
            "Injects synthetic input: a key, mouse button or gamepad button (press, release or tap held for "
            "holdFrames), "
            "a gamepad axis value, a mouse position, scroll or UTF-8 text (one text event per code point).",
            R"({"type":"object","properties":{"key":{"type":"string"},"button":{"type":"string"},)"
            R"("gamepadButton":{"type":"string"},"gamepad":{"type":"integer","minimum":0},"axis":{"type":"string"},)"
            R"("value":{"type":"number"},"action":{"type":"string","enum":["press","release","tap"]},)"
            R"("holdFrames":{"type":"integer","minimum":0},"modifiers":{"type":"string"},)"
            R"("position":{"type":"array","items":{"type":"number"},"minItems":2,"maxItems":2},)"
            R"("scroll":{"type":"array","items":{"type":"number"},"minItems":2,"maxItems":2},)"
            R"("text":{"type":"string"}}})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::NONE,
            &input_inject
          )
        );
        add_command(
          registry,
          make_command(
            "input.clear",
            "Releases every held key and button.",
            std::string(EMPTY_SCHEMA),
            R"({"type":"object","properties":{"cleared":{"type":"boolean"},"frame":{"type":"integer"}}})",
            command_flags::NONE,
            &input_clear
          )
        );
    }
}
