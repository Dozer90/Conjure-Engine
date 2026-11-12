#include "conjure/platform/event.h"

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

// Conversions between event and input_event (index remaps: the input alternatives come first and in
// the same order in both variants) and the diagnostic formatter.

using namespace conjure;
using namespace conjure::platform;

namespace {
    template <typename EventType>
    constexpr bool IS_INPUT_ALTERNATIVE = requires(const EventType &value) { input_event(value); };

    static_assert(IS_INPUT_ALTERNATIVE<key_event>);
    static_assert(!IS_INPUT_ALTERNATIVE<close_event>);

    // Both variants must agree on the order of the shared alternatives for index() to be comparable.
    template <usize Index>
    constexpr bool SAME_ALTERNATIVE =
      std::is_same_v<std::variant_alternative_t<Index, input_event>, std::variant_alternative_t<Index, event>>;

    template <usize... Indices>
    constexpr bool same_prefix(std::index_sequence<Indices...>) noexcept {
        return (SAME_ALTERNATIVE<Indices> && ...);
    }

    static_assert(INPUT_EVENT_COUNT < EVENT_COUNT);
    static_assert(same_prefix(std::make_index_sequence<INPUT_EVENT_COUNT>()));

    template <typename EnumType>
    [[nodiscard]] std::string format_named(EnumType value) {
        const std::string_view name = to_string(value);
        if (name.empty()) {
            return std::format("{}", static_cast<u32>(value));
        }
        return std::string(name);
    }

    struct event_formatter {
        [[nodiscard]] std::string operator()(const key_event &value) const {
            return std::format(
              "key_event{{key={}, scancode={}, action={}, modifiers={}}}",
              format_named(value.keyCode),
              value.scancode,
              format_named(value.action),
              to_string(value.modifiers)
            );
        }

        [[nodiscard]] std::string operator()(const text_event &value) const {
            return std::format("text_event{{codepoint=U+{:04X}}}", value.codepoint);
        }

        [[nodiscard]] std::string operator()(const mouse_move_event &value) const {
            return std::format("mouse_move_event{{position={}}}", value.position);
        }

        [[nodiscard]] std::string operator()(const mouse_button_event &value) const {
            return std::format(
              "mouse_button_event{{button={}, action={}, modifiers={}}}",
              format_named(value.button),
              format_named(value.action),
              to_string(value.modifiers)
            );
        }

        [[nodiscard]] std::string operator()(const mouse_scroll_event &value) const {
            return std::format("mouse_scroll_event{{offset={}}}", value.offset);
        }

        [[nodiscard]] std::string operator()(const cursor_enter_event &value) const {
            return std::format("cursor_enter_event{{entered={}}}", value.entered);
        }

        [[nodiscard]] std::string operator()(const gamepad_connection_event &value) const {
            return std::format(
              "gamepad_connection_event{{gamepad={}, connected={}}}",
              value.gamepadIndex,
              value.connected
            );
        }

        [[nodiscard]] std::string operator()(const gamepad_button_event &value) const {
            return std::format(
              "gamepad_button_event{{gamepad={}, button={}, action={}}}",
              value.gamepadIndex,
              format_named(value.button),
              format_named(value.action)
            );
        }

        [[nodiscard]] std::string operator()(const gamepad_axis_event &value) const {
            return std::format(
              "gamepad_axis_event{{gamepad={}, axis={}, value={}}}",
              value.gamepadIndex,
              format_named(value.axis),
              value.value
            );
        }

        [[nodiscard]] std::string operator()(const framebuffer_resized_event &value) const {
            return std::format("framebuffer_resized_event{{width={}, height={}}}", value.width, value.height);
        }

        [[nodiscard]] std::string operator()(const content_scale_changed_event &value) const {
            return std::format("content_scale_changed_event{{scale={}}}", value.scale);
        }

        [[nodiscard]] std::string operator()(const focus_event &value) const {
            return std::format("focus_event{{focused={}}}", value.focused);
        }

        [[nodiscard]] std::string operator()(const minimise_event &value) const {
            return std::format("minimise_event{{minimised={}}}", value.minimised);
        }

        [[nodiscard]] std::string operator()(const close_event &) const {
            return "close_event{}";
        }

        [[nodiscard]] std::string operator()(const drop_files_event &value) const {
            std::string paths = {};
            for (const std::string &path : value.paths) {
                if (!paths.empty()) {
                    paths.append(", ");
                }
                paths.push_back('"');
                paths.append(path);
                paths.push_back('"');
            }
            return std::format("drop_files_event{{paths=[{}]}}", paths);
        }
    };

    struct event_namer {
        [[nodiscard]] std::string_view operator()(const key_event &) const noexcept {
            return "key_event";
        }
        [[nodiscard]] std::string_view operator()(const text_event &) const noexcept {
            return "text_event";
        }
        [[nodiscard]] std::string_view operator()(const mouse_move_event &) const noexcept {
            return "mouse_move_event";
        }
        [[nodiscard]] std::string_view operator()(const mouse_button_event &) const noexcept {
            return "mouse_button_event";
        }
        [[nodiscard]] std::string_view operator()(const mouse_scroll_event &) const noexcept {
            return "mouse_scroll_event";
        }
        [[nodiscard]] std::string_view operator()(const cursor_enter_event &) const noexcept {
            return "cursor_enter_event";
        }
        [[nodiscard]] std::string_view operator()(const gamepad_connection_event &) const noexcept {
            return "gamepad_connection_event";
        }
        [[nodiscard]] std::string_view operator()(const gamepad_button_event &) const noexcept {
            return "gamepad_button_event";
        }
        [[nodiscard]] std::string_view operator()(const gamepad_axis_event &) const noexcept {
            return "gamepad_axis_event";
        }
        [[nodiscard]] std::string_view operator()(const framebuffer_resized_event &) const noexcept {
            return "framebuffer_resized_event";
        }
        [[nodiscard]] std::string_view operator()(const content_scale_changed_event &) const noexcept {
            return "content_scale_changed_event";
        }
        [[nodiscard]] std::string_view operator()(const focus_event &) const noexcept {
            return "focus_event";
        }
        [[nodiscard]] std::string_view operator()(const minimise_event &) const noexcept {
            return "minimise_event";
        }
        [[nodiscard]] std::string_view operator()(const close_event &) const noexcept {
            return "close_event";
        }
        [[nodiscard]] std::string_view operator()(const drop_files_event &) const noexcept {
            return "drop_files_event";
        }
    };
}

bool conjure::platform::is_input_event(const event &value) noexcept {
    return value.index() < INPUT_EVENT_COUNT;
}

std::optional<input_event> conjure::platform::to_input_event(const event &value) noexcept {
    return std::visit(
      [](const auto &alternative) -> std::optional<input_event> {
          using alternative_type = std::remove_cvref_t<decltype(alternative)>;
          if constexpr (IS_INPUT_ALTERNATIVE<alternative_type>) {
              return input_event(alternative);
          } else {
              return std::nullopt;
          }
      },
      value
    );
}

event conjure::platform::to_event(const input_event &value) noexcept {
    return std::visit([](const auto &alternative) -> event { return event(alternative); }, value);
}

std::string_view conjure::platform::event_name(const event &value) noexcept {
    return std::visit(event_namer(), value);
}

std::string conjure::platform::to_string(const event &value) {
    return std::visit(event_formatter(), value);
}
