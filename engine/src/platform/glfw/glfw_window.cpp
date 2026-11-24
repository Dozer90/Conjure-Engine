#include "platform/glfw/glfw_window.h"
#include "platform/glfw/glfw_context.h"
#include "platform/glfw/glfw_native.h"
#include "conjure/platform/log_category.h"
#include "conjure/platform/platform_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <variant>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::glfw;

namespace {
    constexpr usize EVENT_RESERVE = 256;
    constexpr usize DROP_PATH_RESERVE = 8;

    void assert_main_thread(const char *what) noexcept {
        CONJURE_ASSERT(core::jobs::is_main_thread(), "glfw_window::{} must run on the main thread", what);
    }

    [[nodiscard]] key to_key(int keyCode) noexcept {
        if (keyCode <= 0 || keyCode > GLFW_KEY_LAST) {
            return key::UNKNOWN;
        }
        return static_cast<key>(keyCode);
    }

    [[nodiscard]] button_action to_action(int action) noexcept {
        switch (action) {
            case GLFW_PRESS:
                return button_action::PRESS;
            case GLFW_REPEAT:
                return button_action::REPEAT;
            default:
                return button_action::RELEASE;
        }
    }

    [[nodiscard]] key_modifier_flags to_modifiers(int modifiers) noexcept {
        constexpr int KNOWN_BITS = GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER |
          GLFW_MOD_CAPS_LOCK | GLFW_MOD_NUM_LOCK;
        return static_cast<key_modifier_flags>(static_cast<u8>(modifiers & KNOWN_BITS));
    }

    [[nodiscard]] u32 clamp_dimension(int value) noexcept {
        return value > 0 ? static_cast<u32>(value) : 0;
    }

    /// <summary>
    /// A u32 size as the int GLFW takes, saturating instead of wrapping.
    /// </summary>
    [[nodiscard]] int to_glfw_dimension(u32 value) noexcept {
        constexpr u32 LIMIT = static_cast<u32>(std::numeric_limits<int>::max());
        return static_cast<int>(std::min(value, LIMIT));
    }

    [[nodiscard]] int glfw_bool(bool value) noexcept {
        return value ? GLFW_TRUE : GLFW_FALSE;
    }

    [[nodiscard]] int cursor_mode_value(cursor_mode mode) noexcept {
        switch (mode) {
            case cursor_mode::HIDDEN:
                return GLFW_CURSOR_HIDDEN;
            case cursor_mode::DISABLED:
                return GLFW_CURSOR_DISABLED;
            case cursor_mode::CAPTURED:
                return GLFW_CURSOR_CAPTURED;
            case cursor_mode::NORMAL:
                break;
        }
        return GLFW_CURSOR_NORMAL;
    }

    /// <summary>
    /// GLFW reports triggers in [-1, 1]; the engine reports [0, 1] (P11). Sticks pass through.
    /// </summary>
    [[nodiscard]] f32 remap_axis(usize axisIndex, f32 raw) noexcept {
        if (axisIndex == static_cast<usize>(gamepad_axis::LEFT_TRIGGER) ||
            axisIndex == static_cast<usize>(gamepad_axis::RIGHT_TRIGGER)) {
            return (raw + 1.0f) * 0.5f;
        }
        return raw;
    }

    [[nodiscard]] GLFWgamepadstate rest_state() noexcept {
        // Triggers rest at -1 in GLFW's range, so a freshly connected pad produces no spurious axis
        // events while buttons already held at connection time still produce their PRESS edges.
        GLFWgamepadstate state = {};
        state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = -1.0f;
        state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = -1.0f;
        return state;
    }

    [[nodiscard]] video_mode to_video_mode(const GLFWvidmode *mode) noexcept {
        video_mode result = {};
        if (mode == nullptr) {
            return result;
        }
        result.width = clamp_dimension(mode->width);
        result.height = clamp_dimension(mode->height);
        result.refreshRateHz = clamp_dimension(mode->refreshRate);
        return result;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

glfw_window::glfw_window() :
    window() {}

glfw_window::~glfw_window() {
    assert_main_thread("~glfw_window");
    if (active_window() == this) {
        set_active_window(nullptr);
    }
    // The display-awake request and the hidden menu bar belong to the process and would outlive the window; another
    // window's claim keeps them on.
    if (mExtrasHeld) {
        release_fullscreen_extras();
        mExtrasHeld = false;
    }
    if (mWindow != nullptr) {
        glfwDestroyWindow(mWindow);
        mWindow = nullptr;
    }
    // create() acquired the context before constructing this object; the reference goes with it.
    release();
}

/*===========================================================================*/
/*===========================================================================*/
// Creation

core::result<std::unique_ptr<window>> glfw_window::create(const window_desc &desc) {
    assert_main_thread("create");
    CONJURE_TRY(acquire(desc.displayServer));

    std::unique_ptr<glfw_window> created = std::make_unique<glfw_window>();
    CONJURE_TRY(created->initialise(desc));
    return core::result<std::unique_ptr<window>>(std::unique_ptr<window>(std::move(created)));
}

core::result<void> glfw_window::initialise(const window_desc &desc) {
    mTitle = desc.title;
    // Capacity for a busy frame so the steady state never allocates (3.3 step 6); both halves of
    // each double buffer, since the pump swaps them.
    mPending.reserve(EVENT_RESERVE);
    mDelivered.reserve(EVENT_RESERVE);
    mPendingDropPaths.reserve(DROP_PATH_RESERVE);
    mDeliveredDropPaths.reserve(DROP_PATH_RESERVE);
    mPendingDropRanges.reserve(DROP_PATH_RESERVE);
    mDeliveredDropRanges.reserve(DROP_PATH_RESERVE);

    // Wayland clients never learn or choose their position and cannot warp the cursor; GLFW reports
    // both as GLFW_FEATURE_UNAVAILABLE through the error callback (an ERROR line each time), so the
    // calls are skipped there instead of attempted.
    const int glfwPlatform = glfwGetPlatform();
    mPositionSupported = glfwPlatform != GLFW_PLATFORM_WAYLAND;
    mCursorWarpSupported = glfwPlatform != GLFW_PLATFORM_WAYLAND;
    // Cocoa windows show the application bundle's icon and Wayland ones the desktop file's; glfwSetWindowIcon
    // reports GLFW_FEATURE_UNAVAILABLE there, so set_icon answers without calling it.
    mIconSupported = glfwPlatform != GLFW_PLATFORM_COCOA && glfwPlatform != GLFW_PLATFORM_WAYLAND;
    // Borderless fullscreen moves the window over its monitor, which a Wayland client cannot do.
    mBorderlessFullscreen = glfwPlatform != GLFW_PLATFORM_WAYLAND;
    const bool startBorderless = desc.fullscreen && mBorderlessFullscreen;

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // NVRHI owns the graphics API; never a GL context
    glfwWindowHint(GLFW_RESIZABLE, glfw_bool(has_flag(desc.flags, window_flags::RESIZABLE)));
    glfwWindowHint(GLFW_DECORATED, glfw_bool(has_flag(desc.flags, window_flags::DECORATED)));
    glfwWindowHint(GLFW_VISIBLE, glfw_bool(has_flag(desc.flags, window_flags::VISIBLE)));
    glfwWindowHint(GLFW_FLOATING, glfw_bool(has_flag(desc.flags, window_flags::FLOATING)));
    // A window created fullscreen is not maximised under its monitor: Win32 would create the borderless one with
    // WS_MAXIMIZE and keep that state through the placement, and an X11 window manager would hold it maximised. It
    // is maximised when it first leaves fullscreen instead (leave_fullscreen).
    glfwWindowHint(GLFW_MAXIMIZED, glfw_bool(has_flag(desc.flags, window_flags::MAXIMISED) && !desc.fullscreen));
    mMaximiseOnLeave = desc.fullscreen && has_flag(desc.flags, window_flags::MAXIMISED);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, glfw_bool(has_flag(desc.flags, window_flags::SCALE_TO_MONITOR)));
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, glfw_bool(has_flag(desc.flags, window_flags::SCALE_FRAMEBUFFER)));
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    // GLFW minimises a window that owns a monitor as soon as it loses focus (code sweep F281). Only the compositor
    // fullscreen of Wayland still owns one, and it stays put like the borderless window everywhere else.
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    if (startBorderless) {
        // Hidden until it covers its monitor, so the decorated window never flashes at its windowed place.
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }
    if (desc.position.has_value() && mPositionSupported) {
        glfwWindowHint(GLFW_POSITION_X, desc.position->x);
        glfwWindowHint(GLFW_POSITION_Y, desc.position->y);
    }

    GLFWmonitor *monitor = nullptr;
    GLFWvidmode mode = {};
    int width = to_glfw_dimension(desc.width);
    int height = to_glfw_dimension(desc.height);
    if (desc.fullscreen) {
        int monitorCount = 0;
        GLFWmonitor **monitorList = glfwGetMonitors(&monitorCount);
        if (monitorList == nullptr || desc.monitorIndex >= static_cast<u32>(std::max(monitorCount, 0))) {
            return core::make_error(
              platform_error::MONITOR_NOT_FOUND,
              "monitor index {} out of range ({} monitor(s))",
              desc.monitorIndex,
              std::max(monitorCount, 0)
            );
        }
        monitor = monitorList[desc.monitorIndex];
        const GLFWvidmode *currentMode = glfwGetVideoMode(monitor);
        if (currentMode == nullptr) {
            return core::make_error(
              platform_error::MONITOR_NOT_FOUND,
              "monitor {} has no current video mode",
              desc.monitorIndex
            );
        }
        mode = *currentMode;
        // validate_window_desc lets a fullscreen description leave its size at zero: like the null backend, the
        // window then leaves fullscreen at its monitor's size.
        if (width <= 0 || height <= 0) {
            width = mode.width;
            height = mode.height;
        }
        // The window never had a windowed size yet: the requested one is the best guess for leaving
        // fullscreen, and the size callback replaces it with the confirmed size as soon as it does.
        mWindowedSize = math::uvec2(clamp_dimension(width), clamp_dimension(height));
    }

    // A borderless window starts windowed at its windowed size and is moved over its monitor once it exists; the
    // compositor fullscreen of Wayland is created on its monitor at the monitor's size.
    GLFWmonitor *createOn = startBorderless ? nullptr : monitor;
    if (createOn != nullptr) {
        width = mode.width;
        height = mode.height;
    }

    clear_last_error();
    mWindow = glfwCreateWindow(width, height, mTitle.c_str(), createOn, nullptr);
    if (mWindow == nullptr) {
        return core::make_error(
          platform_error::WINDOW_CREATE_FAILED,
          "glfwCreateWindow({} x {}) failed: {}",
          width,
          height,
          last_error_summary()
        );
    }
    mFullscreen = desc.fullscreen;
    mWindowedDecorated = has_flag(desc.flags, window_flags::DECORATED);

    glfwSetWindowUserPointer(mWindow, this);
    install_callbacks();

    // A window created fullscreen reports its monitor's origin, which is no place to restore it to (the title
    // bar would be off screen): it returns to the requested position, or set_fullscreen(false) centres it.
    if (mPositionSupported && !mFullscreen) {
        int positionX = 0;
        int positionY = 0;
        glfwGetWindowPos(mWindow, &positionX, &positionY);
        mWindowedPosition = math::ivec2(positionX, positionY);
        mWindowedPositionKnown = true;
    } else if (mPositionSupported && desc.position.has_value()) {
        mWindowedPosition = *desc.position;
        mWindowedPositionKnown = true;
    }
    if (!mFullscreen) {
        mWindowedSize = window_size();
    }
    if (startBorderless) {
        enter_fullscreen(monitor, mode);
        if (has_flag(desc.flags, window_flags::VISIBLE)) {
            show_covering_monitor();
        }
        // The size the window starts at is no change: the resizes its placement raised are not events.
        mPendingResize.reset();
    }
    mMinimised = glfwGetWindowAttrib(mWindow, GLFW_ICONIFIED) == GLFW_TRUE;
    update_fullscreen_extras();
    // The scale at creation is the window's starting point, not a change: no event announces it.
    mContentScale.start(current_sizes());

    if (!desc.gamepadMappings.empty()) {
        const std::string mappings(desc.gamepadMappings);
        if (glfwUpdateGamepadMappings(mappings.c_str()) != GLFW_TRUE) {
            CONJURE_LOG_WARN(LOG_PLATFORM, "window_desc::gamepadMappings were rejected by GLFW");
        }
    }

    // Only ask for the extension list when a loader exists: the query itself emits
    // GLFW_API_UNAVAILABLE otherwise, which the error callback would log as an error.
    if (glfwVulkanSupported() == GLFW_TRUE) {
        u32 extensionCount = 0;
        const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        if (extensions != nullptr) {
            mVulkanExtensions = std::span<const char *const>(extensions, extensionCount);
        }
    }

    set_active_window(this);
    scan_joysticks();
    rebuild_monitors();

    CONJURE_LOG_INFO(
      LOG_PLATFORM,
      "Created GLFW window {} x {} (framebuffer {}), fullscreen={}, '{}'",
      window_size().x,
      window_size().y,
      framebuffer_size(),
      mFullscreen,
      mTitle
    );
    return core::result<void>::success();
}

void glfw_window::install_callbacks() noexcept {
    glfwSetKeyCallback(mWindow, &glfw_window::on_key);
    glfwSetCharCallback(mWindow, &glfw_window::on_char);
    glfwSetCursorPosCallback(mWindow, &glfw_window::on_cursor_position);
    glfwSetCursorEnterCallback(mWindow, &glfw_window::on_cursor_enter);
    glfwSetMouseButtonCallback(mWindow, &glfw_window::on_mouse_button);
    glfwSetScrollCallback(mWindow, &glfw_window::on_scroll);
    glfwSetDropCallback(mWindow, &glfw_window::on_drop);
    glfwSetWindowSizeCallback(mWindow, &glfw_window::on_window_size);
    glfwSetFramebufferSizeCallback(mWindow, &glfw_window::on_framebuffer_size);
    glfwSetWindowContentScaleCallback(mWindow, &glfw_window::on_content_scale);
    glfwSetWindowFocusCallback(mWindow, &glfw_window::on_focus);
    glfwSetWindowIconifyCallback(mWindow, &glfw_window::on_iconify);
    glfwSetWindowCloseCallback(mWindow, &glfw_window::on_close);
}

void glfw_window::scan_joysticks() {
    for (int joystickId = GLFW_JOYSTICK_1; joystickId <= GLFW_JOYSTICK_LAST; ++joystickId) {
        // glfwJoystickPresent polls presence and may report a disconnect (Win32). The slot it frees must not
        // be handed out in the same batch as its disconnect, so the next pump scans again instead.
        if (mRescanJoysticks) {
            return;
        }
        if (glfwJoystickPresent(joystickId) == GLFW_TRUE) {
            on_joystick(joystickId, true);
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// window: frame

backend_kind glfw_window::backend() const noexcept {
    return backend_kind::GLFW;
}

std::span<const event> glfw_window::pump_events() {
    assert_main_thread("pump_events");

    // The batch returned last frame dies here; its drop paths with it. Everything the callbacks
    // recorded since that pump (setters raise resize, focus and iconify callbacks synchronously on
    // Win32 and Cocoa) is still pending and goes out with this batch.
    mDelivered.clear();
    mDeliveredDropPaths.clear();
    mDeliveredDropRanges.clear();

    // A slot the last batch reported disconnected may go to a gamepad that found all slots taken. Scanning
    // before this pump polls keeps the disconnect and the connect in separate batches (input resets a slot on
    // connect, which would wipe the disconnect's released edges), and the slot, gamepad_name() with it, is
    // assigned only in the pump that delivers the connect.
    if (mRescanJoysticks) {
        mRescanJoysticks = false;
        scan_joysticks();
    }

    glfwPollEvents();
    poll_gamepads();

    if (mMonitorsDirty) {
        rebuild_monitors();
    }

    // Before the resize, which stays last in the batch (P10).
    announce_content_scale();

    if (mPendingResize.has_value()) {
        framebuffer_resized_event resized = {};
        resized.width = mPendingResize->x;
        resized.height = mPendingResize->y;
        mPending.push_back(event(resized));
        mMinimised = resized.width == 0 || resized.height == 0;
        mPendingResize.reset();
        update_fullscreen_extras();
    }

    std::swap(mPending, mDelivered);
    std::swap(mPendingDropPaths, mDeliveredDropPaths);
    std::swap(mPendingDropRanges, mDeliveredDropRanges);
    patch_drop_spans();
    return std::span<const event>(mDelivered.data(), mDelivered.size());
}

bool glfw_window::should_close() const noexcept {
    assert_main_thread("should_close");
    return glfwWindowShouldClose(mWindow) == GLFW_TRUE;
}

void glfw_window::request_close() noexcept {
    assert_main_thread("request_close");
    glfwSetWindowShouldClose(mWindow, GLFW_TRUE);
}

/*===========================================================================*/
/*===========================================================================*/
// window: geometry and state

math::uvec2 glfw_window::framebuffer_size() const noexcept {
    assert_main_thread("framebuffer_size");
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(mWindow, &width, &height);
    return math::uvec2(clamp_dimension(width), clamp_dimension(height));
}

math::uvec2 glfw_window::window_size() const noexcept {
    assert_main_thread("window_size");
    int width = 0;
    int height = 0;
    glfwGetWindowSize(mWindow, &width, &height);
    return math::uvec2(clamp_dimension(width), clamp_dimension(height));
}

math::vec2 glfw_window::content_scale() const noexcept {
    assert_main_thread("content_scale");
    // Live like the two sizes, so a setter's change is visible before the pump announces it.
    return detail::framebuffer_content_scale(current_sizes(), mContentScale.scale());
}

bool glfw_window::is_minimised() const noexcept {
    assert_main_thread("is_minimised");
    return mMinimised || glfwGetWindowAttrib(mWindow, GLFW_ICONIFIED) == GLFW_TRUE;
}

bool glfw_window::is_focused() const noexcept {
    assert_main_thread("is_focused");
    return glfwGetWindowAttrib(mWindow, GLFW_FOCUSED) == GLFW_TRUE;
}

bool glfw_window::is_visible() const noexcept {
    assert_main_thread("is_visible");
    return glfwGetWindowAttrib(mWindow, GLFW_VISIBLE) == GLFW_TRUE;
}

bool glfw_window::is_fullscreen() const noexcept {
    return mFullscreen;
}

void glfw_window::set_title(std::string_view title) {
    assert_main_thread("set_title");
    mTitle.assign(title);
    glfwSetWindowTitle(mWindow, mTitle.c_str());
}

std::string_view glfw_window::title() const noexcept {
    return mTitle;
}

core::result<void> glfw_window::set_icon(std::span<const core::image> images) {
    assert_main_thread("set_icon");
    CONJURE_TRY(validate_window_icon(images));
    if (!mIconSupported) {
        // Expected on these platforms rather than a fault, so no WARN line: the caller decides what to say.
        return core::make_error(
          platform_error::FEATURE_UNAVAILABLE,
          "window icons are not available on {}",
          platform_name()
        );
    }

    std::array<GLFWimage, MAX_WINDOW_ICON_IMAGES> glfwImages = {};
    for (usize index = 0; index < images.size(); ++index) {
        const core::image &image = images[index];
        glfwImages[index].width = static_cast<int>(image.width());
        glfwImages[index].height = static_cast<int>(image.height());
        // GLFWimage is not const-correct; glfwSetWindowIcon only reads the pixels and copies them before it
        // returns.
        std::byte *pixels = const_cast<std::byte *>(image.pixels().data());
        glfwImages[index].pixels = reinterpret_cast<unsigned char *>(pixels);
    }

    clear_last_error();
    glfwSetWindowIcon(mWindow, static_cast<int>(images.size()), images.empty() ? nullptr : glfwImages.data());
    if (last_error_code() != GLFW_NO_ERROR) {
        return core::make_error(
          core::error_code::BACKEND_FAILURE,
          "glfwSetWindowIcon failed: {}",
          last_error_summary()
        );
    }
    return core::result<void>::success();
}

void glfw_window::set_size(math::uvec2 logicalSize) {
    assert_main_thread("set_size");
    // A fullscreen window keeps covering its monitor: the borderless one would shrink, and GLFW would switch the
    // video mode of a monitor a window owns (P12 rules mode switches out).
    if (mFullscreen) {
        return;
    }
    // The OS may clamp the request (size limits, a compositor's refusal); on_window_size records
    // whatever it confirms, so nothing is assumed here.
    glfwSetWindowSize(mWindow, to_glfw_dimension(logicalSize.x), to_glfw_dimension(logicalSize.y));
}

core::result<void> glfw_window::set_position(math::ivec2 position) {
    assert_main_thread("set_position");
    if (!mPositionSupported) {
        return feature_unavailable(optional_feature::WINDOW_POSITION, "window positioning");
    }
    // A fullscreen window keeps covering its monitor (the borderless one would move off it), and the position is
    // not remembered for leaving fullscreen either: the window returns to where it was before.
    if (mFullscreen) {
        return core::result<void>::success();
    }
    glfwSetWindowPos(mWindow, position.x, position.y);
    mWindowedPosition = position;
    mWindowedPositionKnown = true;
    return core::result<void>::success();
}

math::ivec2 glfw_window::position() const noexcept {
    assert_main_thread("position");
    if (!mPositionSupported) {
        return mWindowedPosition;   // Wayland: the only position known is the one nobody could set
    }
    int x = 0;
    int y = 0;
    glfwGetWindowPos(mWindow, &x, &y);
    return math::ivec2(x, y);
}

void glfw_window::show() {
    assert_main_thread("show");
    show_covering_monitor();
}

void glfw_window::hide() {
    assert_main_thread("hide");
    glfwHideWindow(mWindow);
    update_fullscreen_extras();
}

core::result<void> glfw_window::set_fullscreen(bool fullscreen, u32 monitorIndex) {
    assert_main_thread("set_fullscreen");

    if (!fullscreen) {
        if (mFullscreen) {
            leave_fullscreen();
        }
        return core::result<void>::success();
    }

    int monitorCount = 0;
    GLFWmonitor **monitorList = glfwGetMonitors(&monitorCount);
    if (monitorList == nullptr || monitorIndex >= static_cast<u32>(std::max(monitorCount, 0))) {
        return core::make_error(
          platform_error::MONITOR_NOT_FOUND,
          "monitor index {} out of range ({} monitor(s))",
          monitorIndex,
          std::max(monitorCount, 0)
        );
    }
    GLFWmonitor *monitor = monitorList[monitorIndex];
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (mode == nullptr) {
        return core::make_error(
          platform_error::MONITOR_NOT_FOUND,
          "monitor {} has no current video mode",
          monitorIndex
        );
    }

    if (!mFullscreen) {
        mWindowedPosition = position();
        mWindowedPositionKnown = true;
        mWindowedDecorated = glfwGetWindowAttrib(mWindow, GLFW_DECORATED) == GLFW_TRUE;
    }
    // Fullscreen before the calls: the size callbacks they raise must not take the monitor's size for
    // the windowed one (mWindowedSize already holds the last confirmed windowed size). Already fullscreen, the
    // window moves to the named monitor.
    mFullscreen = true;
    enter_fullscreen(monitor, *mode);
    return core::result<void>::success();
}

std::span<const monitor_info> glfw_window::monitors() const noexcept {
    return std::span<const monitor_info>(mMonitors.data(), mMonitors.size());
}

std::span<const video_mode> glfw_window::video_modes(u32 monitorIndex) const noexcept {
    if (monitorIndex >= mVideoModes.size()) {
        return {};
    }
    const std::vector<video_mode> &modes = mVideoModes[monitorIndex];
    return std::span<const video_mode>(modes.data(), modes.size());
}

/*===========================================================================*/
/*===========================================================================*/
// window: cursor, gamepads, native access, clipboard

core::result<void> glfw_window::set_cursor_mode(cursor_mode mode) {
    assert_main_thread("set_cursor_mode");
    clear_last_error();
    glfwSetInputMode(mWindow, GLFW_CURSOR, cursor_mode_value(mode));

    const int errorCode = last_error_code();
    if (errorCode == GLFW_NO_ERROR) {
        mCursorMode = mode;
        return core::result<void>::success();
    }
    if (errorCode == GLFW_FEATURE_UNAVAILABLE || errorCode == GLFW_FEATURE_UNIMPLEMENTED) {
        // The display server lacks this mode (CAPTURED outside Win32/Cocoa, DISABLED on a Wayland
        // compositor without pointer locking): one WARN per mode, the mode stays as it was.
        optional_feature feature = optional_feature::CURSOR_NORMAL;
        switch (mode) {
            case cursor_mode::HIDDEN:
                feature = optional_feature::CURSOR_HIDDEN;
                break;
            case cursor_mode::DISABLED:
                feature = optional_feature::CURSOR_DISABLED;
                break;
            case cursor_mode::CAPTURED:
                feature = optional_feature::CURSOR_CAPTURED;
                break;
            case cursor_mode::NORMAL:
                break;
        }
        return feature_unavailable(feature, std::format("cursor_mode::{}", to_string(mode)));
    }
    return core::make_error(
      core::error_code::BACKEND_FAILURE,
      "glfwSetInputMode(GLFW_CURSOR, {}) failed: {}",
      to_string(mode),
      last_error_summary()
    );
}

cursor_mode glfw_window::current_cursor_mode() const noexcept {
    return mCursorMode;
}

core::result<void> glfw_window::set_raw_mouse_motion(bool enabled) {
    assert_main_thread("set_raw_mouse_motion");
    if (glfwRawMouseMotionSupported() != GLFW_TRUE) {
        return feature_unavailable(optional_feature::RAW_MOUSE_MOTION, "raw mouse motion");
    }
    glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, glfw_bool(enabled));
    return core::result<void>::success();
}

void glfw_window::set_cursor_position(math::vec2 logicalPosition) {
    assert_main_thread("set_cursor_position");
    if (!mCursorWarpSupported) {
        warn_feature_unavailable(optional_feature::CURSOR_POSITION, "cursor warping");
        return;
    }
    glfwSetCursorPos(mWindow, static_cast<double>(logicalPosition.x), static_cast<double>(logicalPosition.y));
}

std::string_view glfw_window::gamepad_name(u32 index) const noexcept {
    if (index >= MAX_GAMEPADS || mGamepads[index].joystickId < 0) {
        return {};
    }
    return mGamepads[index].name;
}

native_window_handle glfw_window::native_handle() const noexcept {
    assert_main_thread("native_handle");
    return query_native_handle(mWindow);
}

std::span<const char *const> glfw_window::required_vulkan_instance_extensions() const noexcept {
    return mVulkanExtensions;
}

core::result<void> glfw_window::create_vulkan_surface(void *instance, void **surface) const {
    assert_main_thread("create_vulkan_surface");
    return create_surface(mWindow, instance, surface);
}

core::result<std::string> glfw_window::clipboard_text() const {
    assert_main_thread("clipboard_text");
    clear_last_error();
    const char *text = glfwGetClipboardString(mWindow);
    if (text == nullptr) {
        return core::make_error(
          platform_error::CLIPBOARD_UNAVAILABLE,
          "the clipboard holds no text: {}",
          last_error_summary()
        );
    }
    return std::string(text);
}

core::result<void> glfw_window::set_clipboard_text(std::string_view text) {
    assert_main_thread("set_clipboard_text");
    const std::string terminated(text);
    clear_last_error();
    glfwSetClipboardString(mWindow, terminated.c_str());
    if (last_error_code() != GLFW_NO_ERROR) {
        return core::make_error(
          platform_error::CLIPBOARD_UNAVAILABLE,
          "the clipboard refused the text: {}",
          last_error_summary()
        );
    }
    return core::result<void>::success();
}

/*===========================================================================*/
/*===========================================================================*/
// Process-wide callbacks

void glfw_window::on_joystick(int joystickId, bool connected) {
    if (connected) {
        for (const gamepad_slot &slot : mGamepads) {
            if (slot.joystickId == joystickId) {
                return;     // already assigned (initial scan followed by a connect callback)
            }
        }
        if (glfwJoystickIsGamepad(joystickId) != GLFW_TRUE) {
            return;         // no SDL mapping: not a gamepad as far as the engine is concerned (P11)
        }
        for (u32 index = 0; index < MAX_GAMEPADS; ++index) {
            gamepad_slot &slot = mGamepads[index];
            if (slot.joystickId >= 0) {
                continue;
            }
            slot.joystickId = joystickId;
            slot.previous = rest_state();
            const char *name = glfwGetGamepadName(joystickId);
            slot.name = name != nullptr ? name : "";

            gamepad_connection_event value = {};
            value.gamepadIndex = static_cast<u8>(index);
            value.connected = true;
            mPending.push_back(event(value));
            CONJURE_LOG_INFO(LOG_PLATFORM, "Gamepad {} connected: {}", index, slot.name);
            return;
        }
        CONJURE_LOG_WARN(
          LOG_PLATFORM,
          "Gamepad (joystick {}) ignored: all {} slots are in use",
          joystickId,
          MAX_GAMEPADS
        );
        return;
    }

    for (u32 index = 0; index < MAX_GAMEPADS; ++index) {
        gamepad_slot &slot = mGamepads[index];
        if (slot.joystickId != joystickId) {
            continue;
        }
        slot.joystickId = -1;
        slot.previous = GLFWgamepadstate();
        slot.name.clear();

        gamepad_connection_event value = {};
        value.gamepadIndex = static_cast<u8>(index);
        value.connected = false;
        mPending.push_back(event(value));
        CONJURE_LOG_INFO(LOG_PLATFORM, "Gamepad {} disconnected", index);
        // Not scanned here: this callback runs inside glfwPollEvents or glfwGetGamepadState (pump_events).
        mRescanJoysticks = true;
        return;
    }
}

void glfw_window::on_monitors_changed() noexcept {
    mMonitorsDirty = true;
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

void glfw_window::poll_gamepads() {
    for (u32 index = 0; index < MAX_GAMEPADS; ++index) {
        gamepad_slot &slot = mGamepads[index];
        if (slot.joystickId < 0) {
            continue;
        }
        GLFWgamepadstate state = {};
        if (glfwGetGamepadState(slot.joystickId, &state) != GLFW_TRUE) {
            continue;       // the disconnect arrives through the joystick callback
        }

        for (usize button = 0; button < GAMEPAD_BUTTON_COUNT; ++button) {
            if (state.buttons[button] == slot.previous.buttons[button]) {
                continue;
            }
            gamepad_button_event value = {};
            value.gamepadIndex = static_cast<u8>(index);
            value.button = static_cast<gamepad_button>(button);
            value.action = state.buttons[button] == GLFW_PRESS ? button_action::PRESS : button_action::RELEASE;
            mPending.push_back(event(value));
        }

        for (usize axis = 0; axis < GAMEPAD_AXIS_COUNT; ++axis) {
            if (state.axes[axis] == slot.previous.axes[axis]) {
                continue;
            }
            gamepad_axis_event value = {};
            value.gamepadIndex = static_cast<u8>(index);
            value.axis = static_cast<gamepad_axis>(axis);
            value.value = remap_axis(axis, state.axes[axis]);
            mPending.push_back(event(value));
        }

        slot.previous = state;
    }
}

void glfw_window::rebuild_monitors() {
    mMonitorsDirty = false;
    mMonitors.clear();
    mVideoModes.clear();

    int monitorCount = 0;
    GLFWmonitor **monitorList = glfwGetMonitors(&monitorCount);
    if (monitorList == nullptr) {
        return;
    }
    GLFWmonitor *primary = glfwGetPrimaryMonitor();

    for (int index = 0; index < monitorCount; ++index) {
        GLFWmonitor *monitor = monitorList[index];
        monitor_info info = {};
        const char *name = glfwGetMonitorName(monitor);
        info.name = name != nullptr ? name : "";
        info.isPrimary = monitor == primary;

        int positionX = 0;
        int positionY = 0;
        glfwGetMonitorPos(monitor, &positionX, &positionY);
        info.position = math::ivec2(positionX, positionY);
        info.currentMode = to_video_mode(glfwGetVideoMode(monitor));

        float xScale = 1.0f;
        float yScale = 1.0f;
        glfwGetMonitorContentScale(monitor, &xScale, &yScale);
        info.contentScale = math::vec2(xScale, yScale);

        int workX = 0;
        int workY = 0;
        int workWidth = 0;
        int workHeight = 0;
        glfwGetMonitorWorkarea(monitor, &workX, &workY, &workWidth, &workHeight);
        info.workArea.x = clamp_dimension(workX);
        info.workArea.y = clamp_dimension(workY);
        info.workArea.width = clamp_dimension(workWidth);
        info.workArea.height = clamp_dimension(workHeight);

        std::vector<video_mode> modes = {};
        int modeCount = 0;
        const GLFWvidmode *modeList = glfwGetVideoModes(monitor, &modeCount);
        if (modeList != nullptr) {
            modes.reserve(static_cast<usize>(std::max(modeCount, 0)));
            for (int modeIndex = 0; modeIndex < modeCount; ++modeIndex) {
                modes.push_back(to_video_mode(&modeList[modeIndex]));
            }
        }

        mMonitors.push_back(std::move(info));
        mVideoModes.push_back(std::move(modes));
    }
}

detail::scaled_sizes glfw_window::current_sizes() const noexcept {
    detail::scaled_sizes sizes = {};
    sizes.framebuffer = framebuffer_size();
    sizes.window = window_size();
    return sizes;
}

void glfw_window::announce_content_scale() {
    // From the sizes the last size callback recorded: no size query here, so a pump without a resize costs
    // the X server no round trip.
    const std::optional<math::vec2> scale = mContentScale.announce();
    if (!scale.has_value()) {
        return;
    }
    content_scale_changed_event changed = {};
    changed.scale = *scale;
    mPending.push_back(event(changed));
}

void glfw_window::enter_fullscreen(GLFWmonitor *monitor, const GLFWvidmode &mode) {
    if (!mBorderlessFullscreen) {
        // Wayland: the compositor gives the window the whole output. GLFW_AUTO_ICONIFY is off (initialise), so it
        // stays fullscreen and restored when focus moves away. GLFW keeps the display awake only when the compositor
        // offers the idle-inhibit protocol (zwp_idle_inhibit_manager_v1); without it the display may sleep.
        glfwSetWindowMonitor(mWindow, monitor, 0, 0, mode.width, mode.height, mode.refreshRate);
        return;
    }

    // Decoration first: Cocoa keeps the frame when the style changes, so the placement after it sizes the content
    // area that is left. Win32 and X11 keep the content area, which the placement moves anyway.
    if (glfwGetWindowAttrib(mWindow, GLFW_DECORATED) == GLFW_TRUE) {
        glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GLFW_FALSE);
    }
    int monitorX = 0;
    int monitorY = 0;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);
    mFullscreenPosition = math::ivec2(monitorX, monitorY);
    mFullscreenSize = math::uvec2(clamp_dimension(mode.width), clamp_dimension(mode.height));
    place_content_area(mFullscreenPosition, mFullscreenSize);
    update_fullscreen_extras();
}

void glfw_window::leave_fullscreen() {
    // Asked while the window still covers its monitor: that is the monitor it returns to.
    if (!mWindowedPositionKnown && mPositionSupported) {
        mWindowedPosition = centred_on_work_area(mWindowedSize);
        mWindowedPositionKnown = true;
    }

    if (!mBorderlessFullscreen) {
        // Windowed before the call: the size callback it raises records the confirmed windowed size.
        mFullscreen = false;
        glfwSetWindowMonitor(
          mWindow,
          nullptr,
          mWindowedPosition.x,
          mWindowedPosition.y,
          to_glfw_dimension(mWindowedSize.x),
          to_glfw_dimension(mWindowedSize.y),
          0
        );
        maximise_if_created_maximised();
        return;
    }

    // Decoration while still fullscreen, so a size callback the restyle raises (Cocoa shrinks the content area under
    // the returning title bar) is not taken for the windowed size; the placement's callback then records it.
    if (mWindowedDecorated) {
        glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GLFW_TRUE);
    }
    mFullscreen = false;
    place_content_area(mWindowedPosition, mWindowedSize);
    update_fullscreen_extras();
    maximise_if_created_maximised();
}

void glfw_window::place_content_area(math::ivec2 position, math::uvec2 size) {
    CONJURE_ASSERT(mPositionSupported, "glfw_window places windows only where the display server lets it");
    const int width = std::max(1, to_glfw_dimension(size.x));
    const int height = std::max(1, to_glfw_dimension(size.y));
    const math::uvec2 requestedSize = math::uvec2(clamp_dimension(width), clamp_dimension(height));

    // One call moves and sizes the content area; GLFW adds the decoration around it for a window it has not
    // attached to a monitor.
    glfwSetWindowMonitor(mWindow, nullptr, position.x, position.y, width, height, 0);

    // Crossing onto a monitor with another DPI makes Win32 send WM_DPICHANGED inside the move, and GLFW applies the
    // rectangle Windows suggests for the new DPI (scaled by the DPI ratio for a SCALE_TO_MONITOR window), so the
    // window can land elsewhere than asked. It has the target monitor's DPI by then, and a second request is not
    // rescaled. On X11 the window manager may apply the first request later; the same request again is harmless.
    if (this->position() == position && window_size() == requestedSize) {
        return;
    }
    glfwSetWindowMonitor(mWindow, nullptr, position.x, position.y, width, height, 0);
}

void glfw_window::show_covering_monitor() {
    glfwShowWindow(mWindow);
    // X11: placing a hidden window goes through XMoveResizeWindow without the PPosition hint GLFW sets only in
    // glfwSetWindowPos, so a window manager may place the window anew when it maps it, and a window hidden while
    // fullscreen is mapped again by the same rules. Once shown it is put back over its monitor. Win32 and Cocoa keep
    // the rectangle of a hidden window, so nothing moves there.
    if (mFullscreen && mBorderlessFullscreen) {
        const bool covering = position() == mFullscreenPosition && window_size() == mFullscreenSize;
        if (!covering) {
            place_content_area(mFullscreenPosition, mFullscreenSize);
        }
    }
    update_fullscreen_extras();
}

void glfw_window::update_fullscreen_extras() {
    // GLFW's monitor fullscreen dropped its display-awake request while the window was minimised; a hidden window
    // shows nothing to stay awake for either, and on macOS the Dock must come back for the minimised window in it.
    const bool visible = glfwGetWindowAttrib(mWindow, GLFW_VISIBLE) == GLFW_TRUE;
    const bool wanted = mFullscreen && mBorderlessFullscreen && visible && !mMinimised;
    if (wanted == mExtrasHeld) {
        return;
    }
    mExtrasHeld = wanted;
    if (wanted) {
        claim_fullscreen_extras();
    } else {
        release_fullscreen_extras();
    }
}

void glfw_window::maximise_if_created_maximised() {
    // Once: the window is windowed now, and from here on the OS keeps its maximised state like any other window's.
    if (!mMaximiseOnLeave) {
        return;
    }
    mMaximiseOnLeave = false;
    glfwMaximizeWindow(mWindow);
}

GLFWmonitor *glfw_window::covered_monitor() const noexcept {
    // Wayland's compositor fullscreen attaches the window to its monitor; a borderless window is found by the
    // monitor its centre is on.
    GLFWmonitor *attached = glfwGetWindowMonitor(mWindow);
    if (attached != nullptr) {
        return attached;
    }

    int monitorCount = 0;
    GLFWmonitor **monitorList = glfwGetMonitors(&monitorCount);
    if (monitorList == nullptr) {
        return nullptr;
    }
    const math::ivec2 origin = position();
    const math::uvec2 size = window_size();
    const i64 centreX = static_cast<i64>(origin.x) + static_cast<i64>(size.x / 2);
    const i64 centreY = static_cast<i64>(origin.y) + static_cast<i64>(size.y / 2);
    for (int index = 0; index < monitorCount; ++index) {
        GLFWmonitor *candidate = monitorList[index];
        const GLFWvidmode *mode = glfwGetVideoMode(candidate);
        if (mode == nullptr) {
            continue;
        }
        int monitorX = 0;
        int monitorY = 0;
        glfwGetMonitorPos(candidate, &monitorX, &monitorY);
        const i64 left = static_cast<i64>(monitorX);
        const i64 top = static_cast<i64>(monitorY);
        const bool insideX = centreX >= left && centreX < left + static_cast<i64>(mode->width);
        const bool insideY = centreY >= top && centreY < top + static_cast<i64>(mode->height);
        if (insideX && insideY) {
            return candidate;
        }
    }
    // The OS moved the window off every monitor (its monitor was disconnected): the primary one takes it back.
    return glfwGetPrimaryMonitor();
}

math::ivec2 glfw_window::centred_on_work_area(math::uvec2 size) const noexcept {
    // Asked while the window still covers its monitor. GLFW places the content area there and adds the
    // decorations around it, so a window smaller than the work area keeps its title bar on screen.
    GLFWmonitor *monitor = covered_monitor();
    if (monitor == nullptr) {
        return mWindowedPosition;
    }

    const int width = to_glfw_dimension(size.x);
    const int height = to_glfw_dimension(size.y);
    int workX = 0;
    int workY = 0;
    int workWidth = 0;
    int workHeight = 0;
    glfwGetMonitorWorkarea(monitor, &workX, &workY, &workWidth, &workHeight);
    return math::ivec2(workX + std::max(0, workWidth - width) / 2, workY + std::max(0, workHeight - height) / 2);
}

void glfw_window::patch_drop_spans() noexcept {
    // Drop callbacks only record where their paths went: the path vector may grow again before the
    // pump, which would invalidate a span taken at callback time. Runs after the swap, against the
    // delivered buffers.
    usize nextRange = 0;
    for (event &value : mDelivered) {
        drop_files_event *drop = std::get_if<drop_files_event>(&value);
        if (drop == nullptr) {
            continue;
        }
        if (nextRange >= mDeliveredDropRanges.size()) {
            drop->paths = {};
            continue;
        }
        const drop_range &range = mDeliveredDropRanges[nextRange];
        ++nextRange;
        drop->paths = std::span<const std::string>(mDeliveredDropPaths.data() + range.offset, range.count);
    }
}

void glfw_window::warn_feature_unavailable(optional_feature feature, std::string_view what) {
    bool &warned = mFeatureWarned[static_cast<usize>(feature)];
    if (warned) {
        return;
    }
    warned = true;
    const std::string_view reason =
      last_error_code() != GLFW_NO_ERROR ? last_error_text() : "the display server does not provide it";
    CONJURE_LOG_WARN(LOG_PLATFORM, "{} is not available on {}: {}", what, platform_name(), reason);
}

core::result<void> glfw_window::feature_unavailable(optional_feature feature, std::string_view what) {
    warn_feature_unavailable(feature, what);
    return core::make_error(platform_error::FEATURE_UNAVAILABLE, "{} is not available on this platform", what);
}

glfw_window *glfw_window::self_of(GLFWwindow *window) noexcept {
    return static_cast<glfw_window *>(glfwGetWindowUserPointer(window));
}

/*===========================================================================*/
/*===========================================================================*/
// GLFW callbacks (main thread, inside glfwPollEvents or a setter)

void glfw_window::on_key(GLFWwindow *window, int keyCode, int scancode, int action, int modifiers) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    key_event value = {};
    value.keyCode = to_key(keyCode);
    value.scancode = scancode;
    value.action = to_action(action);
    value.modifiers = to_modifiers(modifiers);
    self->mPending.push_back(event(value));
}

void glfw_window::on_char(GLFWwindow *window, unsigned int codepoint) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    text_event value = {};
    value.codepoint = codepoint;
    self->mPending.push_back(event(value));
}

void glfw_window::on_cursor_position(GLFWwindow *window, double x, double y) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    mouse_move_event value = {};
    value.position = math::vec2(static_cast<f32>(x), static_cast<f32>(y));
    self->mPending.push_back(event(value));
}

void glfw_window::on_cursor_enter(GLFWwindow *window, int entered) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    cursor_enter_event value = {};
    value.entered = entered == GLFW_TRUE;
    self->mPending.push_back(event(value));
}

void glfw_window::on_mouse_button(GLFWwindow *window, int button, int action, int modifiers) {
    glfw_window *self = self_of(window);
    if (self == nullptr || button < 0 || button > GLFW_MOUSE_BUTTON_LAST) {
        return;
    }
    mouse_button_event value = {};
    value.button = static_cast<mouse_button>(button);
    value.action = to_action(action);
    value.modifiers = to_modifiers(modifiers);
    self->mPending.push_back(event(value));
}

void glfw_window::on_scroll(GLFWwindow *window, double xOffset, double yOffset) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    mouse_scroll_event value = {};
    value.offset = math::vec2(static_cast<f32>(xOffset), static_cast<f32>(yOffset));
    self->mPending.push_back(event(value));
}

void glfw_window::on_drop(GLFWwindow *window, int count, const char **paths) {
    glfw_window *self = self_of(window);
    if (self == nullptr || paths == nullptr || count <= 0) {
        return;
    }
    drop_range range = {};
    range.offset = self->mPendingDropPaths.size();
    for (int index = 0; index < count; ++index) {
        if (paths[index] == nullptr) {
            continue;
        }
        self->mPendingDropPaths.emplace_back(paths[index]);
    }
    range.count = self->mPendingDropPaths.size() - range.offset;
    self->mPendingDropRanges.push_back(range);
    self->mPending.push_back(event(drop_files_event()));    // the span is patched by the pump
}

void glfw_window::on_window_size(GLFWwindow *window, int width, int height) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    // Every size change records the sizes: a window minimised between two pumps reports 0 x 0 at the next
    // one, and content_scale() must still hold the ratio of the sizes it had just before.
    self->mContentScale.record(self->current_sizes());
    if (self->mFullscreen || width <= 0 || height <= 0) {
        return;     // only a confirmed windowed size is worth restoring; 0 x 0 is the iconified window
    }
    self->mWindowedSize = math::uvec2(clamp_dimension(width), clamp_dimension(height));
}

void glfw_window::on_framebuffer_size(GLFWwindow *window, int width, int height) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    self->mContentScale.record(self->current_sizes());
    // Only the last size of the pump is delivered (P10).
    self->mPendingResize = math::uvec2(clamp_dimension(width), clamp_dimension(height));
}

void glfw_window::on_content_scale(GLFWwindow *window, float, float) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    // GLFW's window content scale is the OS UI scale (DPI / 96 on Windows), not window::content_scale(): the
    // window moved to a monitor with another UI scale, or the user changed a monitor's scale. Only the second
    // makes monitors() stale, and GLFW gives no other notice of it, so the next pump rebuilds the list either way.
    // The OS reports this only for the window's own monitor: a scale change on another monitor raises no
    // callback, and that monitor's contentScale stays stale until something rebuilds the list (a monitor
    // connecting or disconnecting, or this callback again). A ratio this changes arrives through the size
    // callbacks and the pump.
    self->mMonitorsDirty = true;
}

void glfw_window::on_focus(GLFWwindow *window, int focused) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    focus_event value = {};
    value.focused = focused == GLFW_TRUE;
    self->mPending.push_back(event(value));
}

void glfw_window::on_iconify(GLFWwindow *window, int iconified) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    self->mMinimised = iconified == GLFW_TRUE;
    self->update_fullscreen_extras();
    minimise_event value = {};
    value.minimised = self->mMinimised;
    self->mPending.push_back(event(value));
}

void glfw_window::on_close(GLFWwindow *window) {
    glfw_window *self = self_of(window);
    if (self == nullptr) {
        return;
    }
    self->mPending.push_back(event(close_event()));
}
