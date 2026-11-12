#include "conjure/engine/imgui_layer.h"

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR

#include "conjure/engine/engine_time.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"
#include "conjure/platform/window.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/renderer.h"
#endif

#include <imgui.h>

#include <algorithm>
#include <cfloat>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] ImGuiKey to_imgui_key(platform::key code) noexcept {
        using platform::key;
        const u32 value = static_cast<u32>(code);
        if (value >= static_cast<u32>(key::A) && value <= static_cast<u32>(key::Z)) {
            return static_cast<ImGuiKey>(ImGuiKey_A + (value - static_cast<u32>(key::A)));
        }
        if (value >= static_cast<u32>(key::NUM_0) && value <= static_cast<u32>(key::NUM_9)) {
            return static_cast<ImGuiKey>(ImGuiKey_0 + (value - static_cast<u32>(key::NUM_0)));
        }
        if (value >= static_cast<u32>(key::F1) && value <= static_cast<u32>(key::F24)) {
            return static_cast<ImGuiKey>(ImGuiKey_F1 + (value - static_cast<u32>(key::F1)));
        }
        if (value >= static_cast<u32>(key::KP_0) && value <= static_cast<u32>(key::KP_9)) {
            return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (value - static_cast<u32>(key::KP_0)));
        }
        switch (code) {
            case key::SPACE:
                return ImGuiKey_Space;
            case key::APOSTROPHE:
                return ImGuiKey_Apostrophe;
            case key::COMMA:
                return ImGuiKey_Comma;
            case key::MINUS:
                return ImGuiKey_Minus;
            case key::PERIOD:
                return ImGuiKey_Period;
            case key::SLASH:
                return ImGuiKey_Slash;
            case key::SEMICOLON:
                return ImGuiKey_Semicolon;
            case key::EQUAL:
                return ImGuiKey_Equal;
            case key::LEFT_BRACKET:
                return ImGuiKey_LeftBracket;
            case key::BACKSLASH:
                return ImGuiKey_Backslash;
            case key::RIGHT_BRACKET:
                return ImGuiKey_RightBracket;
            case key::GRAVE_ACCENT:
                return ImGuiKey_GraveAccent;
            case key::ESCAPE:
                return ImGuiKey_Escape;
            case key::ENTER:
                return ImGuiKey_Enter;
            case key::TAB:
                return ImGuiKey_Tab;
            case key::BACKSPACE:
                return ImGuiKey_Backspace;
            case key::INSERT:
                return ImGuiKey_Insert;
            case key::DEL:
                return ImGuiKey_Delete;
            case key::RIGHT:
                return ImGuiKey_RightArrow;
            case key::LEFT:
                return ImGuiKey_LeftArrow;
            case key::DOWN:
                return ImGuiKey_DownArrow;
            case key::UP:
                return ImGuiKey_UpArrow;
            case key::PAGE_UP:
                return ImGuiKey_PageUp;
            case key::PAGE_DOWN:
                return ImGuiKey_PageDown;
            case key::HOME:
                return ImGuiKey_Home;
            case key::END:
                return ImGuiKey_End;
            case key::CAPS_LOCK:
                return ImGuiKey_CapsLock;
            case key::SCROLL_LOCK:
                return ImGuiKey_ScrollLock;
            case key::NUM_LOCK:
                return ImGuiKey_NumLock;
            case key::PRINT_SCREEN:
                return ImGuiKey_PrintScreen;
            case key::PAUSE:
                return ImGuiKey_Pause;
            case key::KP_DECIMAL:
                return ImGuiKey_KeypadDecimal;
            case key::KP_DIVIDE:
                return ImGuiKey_KeypadDivide;
            case key::KP_MULTIPLY:
                return ImGuiKey_KeypadMultiply;
            case key::KP_SUBTRACT:
                return ImGuiKey_KeypadSubtract;
            case key::KP_ADD:
                return ImGuiKey_KeypadAdd;
            case key::KP_ENTER:
                return ImGuiKey_KeypadEnter;
            case key::KP_EQUAL:
                return ImGuiKey_KeypadEqual;
            case key::LEFT_SHIFT:
                return ImGuiKey_LeftShift;
            case key::LEFT_CONTROL:
                return ImGuiKey_LeftCtrl;
            case key::LEFT_ALT:
                return ImGuiKey_LeftAlt;
            case key::LEFT_SUPER:
                return ImGuiKey_LeftSuper;
            case key::RIGHT_SHIFT:
                return ImGuiKey_RightShift;
            case key::RIGHT_CONTROL:
                return ImGuiKey_RightCtrl;
            case key::RIGHT_ALT:
                return ImGuiKey_RightAlt;
            case key::RIGHT_SUPER:
                return ImGuiKey_RightSuper;
            case key::MENU:
                return ImGuiKey_Menu;
            default:
                break;
        }
        return ImGuiKey_None;
    }

    [[nodiscard]] int to_imgui_mouse_button(platform::mouse_button button) noexcept {
        const int index = static_cast<int>(button);
        return index < ImGuiMouseButton_COUNT ? index : -1;
    }

    // ImGui 1.91: the clipboard hooks receive the context; the layer travels in Platform_ClipboardUserData.
    [[nodiscard]] const char *clipboard_get(ImGuiContext *context) {
        CONJURE_UNUSED(context);
        imgui_layer *layer = static_cast<imgui_layer *>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
        return layer != nullptr ? layer->clipboard_text() : "";
    }

    void clipboard_set(ImGuiContext *context, const char *text) {
        CONJURE_UNUSED(context);
        imgui_layer *layer = static_cast<imgui_layer *>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
        if (layer != nullptr && text != nullptr) {
            layer->set_clipboard_text(text);
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

imgui_layer::imgui_layer() :
    layer("imgui") {}

imgui_layer::~imgui_layer() {
    if (mContext != nullptr) {
        ImGui::DestroyContext(mContext);
        mContext = nullptr;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> imgui_layer::on_attach(engine_context &context) {
    IMGUI_CHECKVERSION();
    mWindow = context.window;
    mContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(mContext);

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendPlatformName = "conjure_engine";
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2(1.0f, 1.0f);
    io.DeltaTime = 1.0f / 60.0f;
    if (mWindow != nullptr) {
        const math::uvec2 size = mWindow->window_size();
        io.DisplaySize = ImVec2(static_cast<f32>(std::max(size.x, 1u)), static_cast<f32>(std::max(size.y, 1u)));
    }
    io.Fonts->AddFontDefault();
    if (!io.Fonts->Build()) {
        CONJURE_LOG_WARN(LOG_ENGINE, "imgui: the default font atlas did not build");
    }

    ImGuiPlatformIO &platformIo = ImGui::GetPlatformIO();
    platformIo.Platform_ClipboardUserData = this;
    platformIo.Platform_GetClipboardTextFn = &clipboard_get;
    platformIo.Platform_SetClipboardTextFn = &clipboard_set;

    mStats = {};
    mFrameOpen = false;
    return core::result<void>::success();
}

void imgui_layer::on_detach(engine_context &context) {
    CONJURE_UNUSED(context);
    if (mContext != nullptr) {
        if (mFrameOpen) {
            ImGui::SetCurrentContext(mContext);
            ImGui::EndFrame();
            mFrameOpen = false;
        }
        ImGui::DestroyContext(mContext);
        mContext = nullptr;
    }
    mWindow = nullptr;
}

void imgui_layer::on_frame_begin(engine_context &context) {
    if (mContext == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(mContext);
    ImGuiIO &io = ImGui::GetIO();
    if (mWindow != nullptr) {
        const math::uvec2 logical = mWindow->window_size();
        const math::uvec2 framebuffer = mWindow->framebuffer_size();
        io.DisplaySize = ImVec2(static_cast<f32>(std::max(logical.x, 1u)), static_cast<f32>(std::max(logical.y, 1u)));
        io.DisplayFramebufferScale = ImVec2(
          static_cast<f32>(std::max(framebuffer.x, 1u)) / io.DisplaySize.x,
          static_cast<f32>(std::max(framebuffer.y, 1u)) / io.DisplaySize.y
        );
    }
    const f32 delta = context.time != nullptr ? context.time->unscaledDeltaTime : 1.0f / 60.0f;
    io.DeltaTime = std::max(delta, 1.0e-6f);
    ImGui::NewFrame();
    mFrameOpen = true;
    ++mStats.framesBegun;
}

void imgui_layer::on_imgui_end(engine_context &context) {
    if (mContext == nullptr || !mFrameOpen) {
        return;
    }
    ImGui::SetCurrentContext(mContext);
    ImGui::Render();
    mFrameOpen = false;
    ++mStats.framesRendered;
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    if (context.renderer != nullptr) {
        context.renderer->set_imgui_draw_data(ImGui::GetDrawData());
    }
#else
    CONJURE_UNUSED(context);
#endif
}

bool imgui_layer::on_event(engine_context &context, const platform::event &event) {
    CONJURE_UNUSED(context);
    if (mContext == nullptr) {
        return false;
    }
    ImGui::SetCurrentContext(mContext);
    ImGuiIO &io = ImGui::GetIO();
    bool consumed = false;
    bool forwarded = true;

    if (const platform::key_event *key = platform::event_as<platform::key_event>(event); key != nullptr) {
        const bool down = key->action != platform::button_action::RELEASE;
        io.AddKeyEvent(ImGuiMod_Ctrl, has_flag(key->modifiers, platform::key_modifier_flags::CONTROL));
        io.AddKeyEvent(ImGuiMod_Shift, has_flag(key->modifiers, platform::key_modifier_flags::SHIFT));
        io.AddKeyEvent(ImGuiMod_Alt, has_flag(key->modifiers, platform::key_modifier_flags::ALT));
        io.AddKeyEvent(ImGuiMod_Super, has_flag(key->modifiers, platform::key_modifier_flags::SUPER));
        const ImGuiKey mapped = to_imgui_key(key->keyCode);
        if (mapped != ImGuiKey_None) {
            io.AddKeyEvent(mapped, down);
        }
        // A release always reaches the input state so a key pressed before ImGui took capture cannot stay
        // down; input ignores a release of a key that is already up.
        consumed = down && io.WantCaptureKeyboard;
    } else if (const platform::text_event *text = platform::event_as<platform::text_event>(event); text != nullptr) {
        io.AddInputCharacter(text->codepoint);
        consumed = io.WantCaptureKeyboard;
    } else if (const platform::mouse_move_event *move = platform::event_as<platform::mouse_move_event>(event);
               move != nullptr) {
        io.AddMousePosEvent(move->position.x, move->position.y);
        consumed = io.WantCaptureMouse;
    } else if (const platform::mouse_button_event *button = platform::event_as<platform::mouse_button_event>(event);
               button != nullptr) {
        const bool pressed = button->action != platform::button_action::RELEASE;
        const int index = to_imgui_mouse_button(button->button);
        if (index >= 0) {
            io.AddMouseButtonEvent(index, pressed);
        }
        // As for keys: a release is never consumed, so a button cannot stay down in the input state.
        consumed = pressed && io.WantCaptureMouse;
    } else if (const platform::mouse_scroll_event *scroll = platform::event_as<platform::mouse_scroll_event>(event);
               scroll != nullptr) {
        io.AddMouseWheelEvent(scroll->offset.x, scroll->offset.y);
        consumed = io.WantCaptureMouse;
    } else if (const platform::cursor_enter_event *enter = platform::event_as<platform::cursor_enter_event>(event);
               enter != nullptr) {
        if (!enter->entered) {
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        }
    } else if (const platform::focus_event *focus = platform::event_as<platform::focus_event>(event);
               focus != nullptr) {
        io.AddFocusEvent(focus->focused);
    } else {
        forwarded = false;
    }

    if (forwarded) {
        ++mStats.eventsForwarded;
    }
    if (consumed) {
        ++mStats.eventsConsumed;
    }
    return consumed;
}

const char *imgui_layer::clipboard_text() {
    mClipboard.clear();
    if (mWindow != nullptr) {
        core::result<std::string> text = mWindow->clipboard_text();
        if (text.has_value()) {
            mClipboard = std::move(text).value();
        }
    }
    return mClipboard.c_str();
}

void imgui_layer::set_clipboard_text(std::string_view text) {
    mClipboard = std::string(text);
    if (mWindow != nullptr) {
        CONJURE_UNUSED(mWindow->set_clipboard_text(text));
    }
}

#endif
