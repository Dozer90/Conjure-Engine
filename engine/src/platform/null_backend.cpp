#include "conjure/platform/null_backend.h"
#include "conjure/platform/log_category.h"
#include "conjure/platform/platform_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

// The virtual window of docs/design/11_platform.md 3.5. Pending events are swapped into the
// delivered vector on every pump, so a steady-state pump allocates nothing (both vectors keep their
// capacity); drop paths are the one payload that allocates, and only when files are dropped.

using namespace conjure;
using namespace conjure::platform;

namespace {
    constexpr std::string_view VIRTUAL_MONITOR_NAME = "Null Monitor";

    [[nodiscard]] u32 round_scaled(u32 logical, f32 scale) noexcept {
        const f32 scaled = std::round(static_cast<f32>(logical) * scale);
        if (scaled <= 0.0f) {
            return 0;
        }
        return static_cast<u32>(scaled);
    }

    void assert_main_thread(const char *what) noexcept {
        CONJURE_ASSERT(core::jobs::is_main_thread(), "null_backend::{} must run on the main thread", what);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

null_backend::null_backend(const window_desc &desc) :
    window(),
    mTitle(desc.title),
    mLogicalSize(desc.width, desc.height),
    mWindowedSize(desc.width, desc.height),
    mContentScale(desc.nullContentScale),
    mVisible(has_flag(desc.flags, window_flags::VISIBLE)),
    mFullscreen(desc.fullscreen) {
    assert_main_thread("null_backend");
    CONJURE_ASSERT(validate_window_desc(desc).has_value(), "null_backend created from an invalid window_desc");

    for (u32 index = 0; index < MAX_GAMEPADS; ++index) {
        mGamepadNames[index] = std::format("Null Gamepad {}", index);
    }

    video_mode mode = {};
    mode.width = VIRTUAL_MONITOR_WIDTH;
    mode.height = VIRTUAL_MONITOR_HEIGHT;
    mode.refreshRateHz = VIRTUAL_MONITOR_REFRESH_HZ;
    mVideoModes.push_back(mode);

    monitor_info monitor = {};
    monitor.name = std::string(VIRTUAL_MONITOR_NAME);
    monitor.isPrimary = true;
    monitor.position = math::ivec2(0);
    monitor.currentMode = mode;
    monitor.contentScale = math::vec2(1.0f);
    monitor.workArea.width = VIRTUAL_MONITOR_WIDTH;
    monitor.workArea.height = VIRTUAL_MONITOR_HEIGHT;
    mMonitors.push_back(monitor);

    if (mFullscreen) {
        mLogicalSize = math::uvec2(VIRTUAL_MONITOR_WIDTH, VIRTUAL_MONITOR_HEIGHT);
        if (mWindowedSize.x == 0 || mWindowedSize.y == 0) {
            mWindowedSize = mLogicalSize;
        }
    }
    mPosition = desc.position.has_value() ? *desc.position : centred_position();

    // Capacity for a busy frame so the steady state never allocates (3.3 step 6 on the glfw side).
    mPending.reserve(256);
    mDelivered.reserve(256);
    mPendingDropPaths.reserve(8);
    mDeliveredDropPaths.reserve(8);
    mPendingDropRanges.reserve(4);
    mDeliveredDropRanges.reserve(4);
}

null_backend::~null_backend() {}

/*===========================================================================*/
/*===========================================================================*/
// Factory

std::unique_ptr<null_backend> null_backend::create(const window_desc &desc) {
    return std::make_unique<null_backend>(desc);
}

/*===========================================================================*/
/*===========================================================================*/
// window: frame

backend_kind null_backend::backend() const noexcept {
    return backend_kind::NULL_BACKEND;
}

std::span<const event> null_backend::pump_events() {
    assert_main_thread("pump_events");

    // The batch returned last frame dies here; its drop paths with it.
    mDelivered.clear();
    mDeliveredDropPaths.clear();
    mDeliveredDropRanges.clear();
    std::swap(mPending, mDelivered);
    std::swap(mPendingDropPaths, mDeliveredDropPaths);
    std::swap(mPendingDropRanges, mDeliveredDropRanges);

    // Drop spans were recorded as ranges because the path vector could still grow while pending.
    usize nextRange = 0;
    for (event &value : mDelivered) {
        drop_files_event *drop = std::get_if<drop_files_event>(&value);
        if (drop != nullptr) {
            const drop_range &range = mDeliveredDropRanges[nextRange];
            ++nextRange;
            drop->paths = std::span<const std::string>(mDeliveredDropPaths.data() + range.offset, range.count);
            continue;
        }
        const gamepad_connection_event *connection = std::get_if<gamepad_connection_event>(&value);
        if (connection != nullptr && connection->gamepadIndex < MAX_GAMEPADS) {
            mGamepadConnected[connection->gamepadIndex] = connection->connected;
        }
    }

    if (mPendingResize.has_value()) {
        framebuffer_resized_event resized = {};
        resized.width = mPendingResize->x;
        resized.height = mPendingResize->y;
        mDelivered.push_back(event(resized));
        mPendingResize.reset();
    }

    return std::span<const event>(mDelivered.data(), mDelivered.size());
}

bool null_backend::should_close() const noexcept {
    return mShouldClose;
}

void null_backend::request_close() noexcept {
    mShouldClose = true;
}

/*===========================================================================*/
/*===========================================================================*/
// window: geometry and state

math::uvec2 null_backend::framebuffer_size() const noexcept {
    if (mMinimised) {
        return math::uvec2(0);
    }
    return compute_framebuffer_size();
}

math::uvec2 null_backend::window_size() const noexcept {
    return mLogicalSize;
}

math::vec2 null_backend::content_scale() const noexcept {
    return mContentScale;
}

bool null_backend::is_minimised() const noexcept {
    return mMinimised;
}

bool null_backend::is_focused() const noexcept {
    return mFocused;
}

bool null_backend::is_visible() const noexcept {
    return mVisible;
}

bool null_backend::is_fullscreen() const noexcept {
    return mFullscreen;
}

void null_backend::set_title(std::string_view title) {
    assert_main_thread("set_title");
    mTitle.assign(title);
}

std::string_view null_backend::title() const noexcept {
    return mTitle;
}

core::result<void> null_backend::set_icon(std::span<const core::image> images) {
    assert_main_thread("set_icon");
    CONJURE_TRY(validate_window_icon(images));
    // Copy before replacing: the span may be icons() itself, and assign() must not read the vector it overwrites.
    std::vector<core::image> copies(images.begin(), images.end());
    mIcons = std::move(copies);
    return core::result<void>::success();
}

void null_backend::set_size(math::uvec2 logicalSize) {
    assert_main_thread("set_size");
    if (logicalSize == mLogicalSize) {
        return;
    }
    mLogicalSize = logicalSize;
    if (!mFullscreen) {
        mWindowedSize = logicalSize;
    }
    queue_resize();
}

core::result<void> null_backend::set_position(math::ivec2 position) {
    assert_main_thread("set_position");
    mPosition = position;
    return core::result<void>::success();
}

math::ivec2 null_backend::position() const noexcept {
    return mPosition;
}

void null_backend::show() {
    assert_main_thread("show");
    mVisible = true;
}

void null_backend::hide() {
    assert_main_thread("hide");
    mVisible = false;
}

core::result<void> null_backend::set_fullscreen(bool fullscreen, u32 monitorIndex) {
    assert_main_thread("set_fullscreen");
    if (fullscreen && monitorIndex >= mMonitors.size()) {
        return core::make_error(
          platform_error::MONITOR_NOT_FOUND,
          "monitor index {} out of range ({} monitor(s))",
          monitorIndex,
          mMonitors.size()
        );
    }
    if (fullscreen == mFullscreen) {
        return core::result<void>::success();
    }

    mFullscreen = fullscreen;
    if (fullscreen) {
        mWindowedSize = mLogicalSize;
        mLogicalSize = math::uvec2(VIRTUAL_MONITOR_WIDTH, VIRTUAL_MONITOR_HEIGHT);
    } else {
        mLogicalSize = mWindowedSize;
    }
    queue_resize();
    return core::result<void>::success();
}

std::span<const monitor_info> null_backend::monitors() const noexcept {
    return std::span<const monitor_info>(mMonitors.data(), mMonitors.size());
}

std::span<const video_mode> null_backend::video_modes(u32 monitorIndex) const noexcept {
    if (monitorIndex >= mMonitors.size()) {
        return {};
    }
    return std::span<const video_mode>(mVideoModes.data(), mVideoModes.size());
}

/*===========================================================================*/
/*===========================================================================*/
// window: cursor, gamepads, native access, clipboard

core::result<void> null_backend::set_cursor_mode(cursor_mode mode) {
    assert_main_thread("set_cursor_mode");
    mCursorMode = mode;
    return core::result<void>::success();
}

cursor_mode null_backend::current_cursor_mode() const noexcept {
    return mCursorMode;
}

core::result<void> null_backend::set_raw_mouse_motion(bool enabled) {
    assert_main_thread("set_raw_mouse_motion");
    mRawMouseMotion = enabled;
    return core::result<void>::success();
}

void null_backend::set_cursor_position(math::vec2 logicalPosition) {
    assert_main_thread("set_cursor_position");
    mCursorPosition = logicalPosition;
}

std::string_view null_backend::gamepad_name(u32 index) const noexcept {
    if (index >= MAX_GAMEPADS || !mGamepadConnected[index]) {
        return {};
    }
    return mGamepadNames[index];
}

native_window_handle null_backend::native_handle() const noexcept {
    return native_window_handle();
}

std::span<const char *const> null_backend::required_vulkan_instance_extensions() const noexcept {
    return {};
}

core::result<void> null_backend::create_vulkan_surface(void *instance, void **surface) const {
    // The same argument checks as the glfw backend, in the same order, so the rhi sees one contract.
    if (surface == nullptr) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "create_vulkan_surface: surface is null");
    }
    *surface = nullptr;
    if (instance == nullptr) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "create_vulkan_surface: instance is null");
    }
    return core::make_error(platform_error::VULKAN_UNSUPPORTED, "the null backend has no Vulkan surface");
}

core::result<std::string> null_backend::clipboard_text() const {
    assert_main_thread("clipboard_text");
    if (!mClipboardHasText) {
        return core::make_error(platform_error::CLIPBOARD_UNAVAILABLE, "the clipboard holds no text");
    }
    return mClipboard;
}

core::result<void> null_backend::set_clipboard_text(std::string_view text) {
    assert_main_thread("set_clipboard_text");
    mClipboard.assign(text);
    mClipboardHasText = true;   // an empty copy is still text, as it is on the OS clipboards
    return core::result<void>::success();
}

null_backend *null_backend::as_null() noexcept {
    return this;
}

/*===========================================================================*/
/*===========================================================================*/
// Simulation controls

void null_backend::push_event(const event &value) {
    assert_main_thread("push_event");

    const drop_files_event *drop = std::get_if<drop_files_event>(&value);
    if (drop != nullptr) {
        // Re-route through simulate_drop_files so the span outlives the caller's buffer.
        simulate_drop_files(drop->paths);
        return;
    }
    mPending.push_back(value);
}

void null_backend::simulate_resize(u32 width, u32 height) {
    assert_main_thread("simulate_resize");
    mLogicalSize = math::uvec2(width, height);
    if (!mFullscreen) {
        mWindowedSize = mLogicalSize;
    }
    queue_resize();
}

void null_backend::simulate_content_scale(math::vec2 scale) {
    assert_main_thread("simulate_content_scale");
    const math::uvec2 before = compute_framebuffer_size();
    mContentScale = scale;
    mMonitors.front().contentScale = scale;

    content_scale_changed_event changed = {};
    changed.scale = scale;
    mPending.push_back(event(changed));

    if (compute_framebuffer_size() != before) {
        queue_resize();
    }
}

void null_backend::simulate_focus(bool focused) {
    assert_main_thread("simulate_focus");
    mFocused = focused;
    focus_event value = {};
    value.focused = focused;
    mPending.push_back(event(value));
}

void null_backend::simulate_minimise(bool minimised) {
    assert_main_thread("simulate_minimise");
    if (minimised == mMinimised) {
        return;
    }
    mMinimised = minimised;
    minimise_event value = {};
    value.minimised = minimised;
    mPending.push_back(event(value));
    queue_resize();
}

void null_backend::simulate_close() {
    assert_main_thread("simulate_close");
    mShouldClose = true;
    mPending.push_back(event(close_event()));
}

void null_backend::simulate_drop_files(std::span<const std::string> paths) {
    assert_main_thread("simulate_drop_files");

    drop_range range = {};
    range.offset = mPendingDropPaths.size();
    range.count = paths.size();
    for (const std::string &path : paths) {
        mPendingDropPaths.push_back(path);
    }
    mPendingDropRanges.push_back(range);

    // The span is patched in pump_events once the paths have moved to the delivered buffer.
    mPending.push_back(event(drop_files_event()));
}

usize null_backend::pending_event_count() const noexcept {
    return mPending.size() + (mPendingResize.has_value() ? 1 : 0);
}

math::vec2 null_backend::cursor_position() const noexcept {
    return mCursorPosition;
}

bool null_backend::raw_mouse_motion() const noexcept {
    return mRawMouseMotion;
}

std::span<const core::image> null_backend::icons() const noexcept {
    return mIcons;
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

math::uvec2 null_backend::compute_framebuffer_size() const noexcept {
    return math::uvec2(round_scaled(mLogicalSize.x, mContentScale.x), round_scaled(mLogicalSize.y, mContentScale.y));
}

void null_backend::queue_resize() noexcept {
    // Only the final size of the frame is delivered (P10), as 0 x 0 while minimised.
    mPendingResize = framebuffer_size();
}

math::ivec2 null_backend::centred_position() const noexcept {
    const i32 width = static_cast<i32>(std::min(mLogicalSize.x, VIRTUAL_MONITOR_WIDTH));
    const i32 height = static_cast<i32>(std::min(mLogicalSize.y, VIRTUAL_MONITOR_HEIGHT));
    return math::ivec2(
      (static_cast<i32>(VIRTUAL_MONITOR_WIDTH) - width) / 2,
      (static_cast<i32>(VIRTUAL_MONITOR_HEIGHT) - height) / 2
    );
}
