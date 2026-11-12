#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/input.h"
#include "conjure/platform/native_handle.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

// The conformance suite (docs/design/11_platform.md 10, 08_testing.md 8.4): every case runs against
// the null backend always and against the glfw backend only when a developer allows a real window
// (CONJURE_TEST_WINDOW=1; the glfw instantiations carry [window] in their name and skip otherwise).

using namespace conjure;
using namespace conjure::core;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

namespace {
    struct null_factory {
        static constexpr bool NEEDS_REAL_WINDOW = false;
        static constexpr backend_kind KIND = backend_kind::NULL_BACKEND;

        [[nodiscard]] static window_desc desc() {
            window_desc value = null_desc(640, 360);
            value.title = "Conformance";
            return value;
        }

        static void warm_up() {}
    };

    struct glfw_factory {
        static constexpr bool NEEDS_REAL_WINDOW = true;
        static constexpr backend_kind KIND = backend_kind::GLFW;

        [[nodiscard]] static window_desc desc() {
            window_desc value = {};
            value.backend = backend_kind::GLFW;
            value.title = "Conformance";
            value.width = 640;
            value.height = 360;
            // A visible window would steal focus from the developer's terminal while the suite runs.
            value.flags = window_flags::DEFAULT & ~window_flags::VISIBLE;
            return value;
        }

        /// <summary>
        /// The first glfwInit / glfwTerminate pair and the first glfwVulkanSupported() of a process
        /// keep a handful of allocations alive until exit (DLL and loader initialisation inside GLFW
        /// and the OS; measured 6 + 2 on Windows), while every later window is balanced. Creating
        /// one window before a case takes its memory baseline keeps the guard about the window code.
        /// </summary>
        static void warm_up() {
            if (live_window_count() != 0) {
                return;
            }
            result<std::unique_ptr<window>> created = create_window(desc());
            REQUIRE_MESSAGE(created.has_value(), testing::describe_error(created));
            static_cast<void>(created.value()->pump_events());
        }
    };

    template <typename Factory>
    [[nodiscard]] std::unique_ptr<window> create_or_fail() {
        result<std::unique_ptr<window>> created = create_window(Factory::desc());
        REQUIRE_MESSAGE(created.has_value(), testing::describe_error(created));
        REQUIRE(created.value() != nullptr);
        return std::move(created).value();
    }

    /// <summary>
    /// What one pump delivered about the window itself: how many resizes (at most one, P10), the
    /// size of the last one, and the last focus state announced, if any.
    /// </summary>
    struct window_batch {
        u32 resizeCount = 0;
        math::uvec2 lastResize = math::uvec2(0);
        std::optional<bool> lastFocus = std::nullopt;
        usize eventCount = 0;
    };

    [[nodiscard]] window_batch pump_window_events(window &target) {
        window_batch batch = {};
        const std::span<const event> events = target.pump_events();
        batch.eventCount = events.size();
        for (const event &value : events) {
            if (const framebuffer_resized_event *resized = event_as<framebuffer_resized_event>(value)) {
                ++batch.resizeCount;
                batch.lastResize = math::uvec2(resized->width, resized->height);
            }
            if (const focus_event *focus = event_as<focus_event>(value)) {
                batch.lastFocus = focus->focused;
            }
        }
        return batch;
    }

    /// <summary>
    /// Pumps after a visibility change and checks the focus story is coherent: whenever the OS
    /// moved focus (it does on Win32 for show() with GLFW_FOCUS_ON_SHOW and for hide(); a Wayland
    /// compositor may refuse) the batch carries a focus_event ending on the new state. The null
    /// backend never moves focus on its own, so it passes by changing nothing.
    /// </summary>
    void check_focus_follows(window &target, bool focusedBefore) {
        const window_batch batch = pump_window_events(target);
        const bool focusedAfter = target.is_focused();
        if (batch.lastFocus.has_value()) {
            CHECK(*batch.lastFocus == focusedAfter);
        }
        if (focusedAfter != focusedBefore) {
            REQUIRE(batch.lastFocus.has_value());
        }
    }

    [[nodiscard]] std::vector<input_event> conformance_sequence() {
        std::vector<input_event> sequence = {};
        sequence.emplace_back(make_key(key::W, button_action::PRESS, key_modifier_flags::NONE));
        sequence.emplace_back(make_key(key::SPACE, button_action::PRESS));
        sequence.emplace_back(make_key(key::SPACE, button_action::RELEASE));
        sequence.emplace_back(make_mouse_move(20.0f, 30.0f));
        sequence.emplace_back(make_mouse_move(25.0f, 30.0f));
        sequence.emplace_back(
          make_mouse_button(mouse_button::RIGHT, button_action::PRESS, key_modifier_flags::CONTROL)
        );
        sequence.emplace_back(make_scroll(0.0f, -1.0f));
        sequence.emplace_back(make_text('k'));
        sequence.emplace_back(make_gamepad_connection(0, true));
        sequence.emplace_back(make_gamepad_axis(0, gamepad_axis::RIGHT_TRIGGER, 0.5f));
        sequence.emplace_back(make_gamepad_button(0, gamepad_button::Y, button_action::PRESS));
        return sequence;
    }

}

// The precondition guard must be the first statement of a case; for the glfw instantiation it skips
// the case (exit 77 under CTest) unless CONJURE_TEST_WINDOW=1, and a build without the glfw backend
// has nothing to conform. The backend is then warmed up once (see glfw_factory::warm_up) before the
// case creates its memory_guard.
#define CONJURE_CONFORMANCE_BEGIN()                                                                         \
    if constexpr (Factory::NEEDS_REAL_WINDOW) {                                                             \
        CONJURE_TEST_REQUIRE(::conjure::testing::precondition::REAL_WINDOW);                                \
        if (!is_backend_compiled(backend_kind::GLFW)) {                                                     \
            MESSAGE("glfw backend not compiled (CONJURE_PLATFORM_GLFW=OFF); nothing to conform");           \
            return;                                                                                         \
        }                                                                                                   \
    }                                                                                                       \
    Factory::warm_up();                                                                                     \
    static_assert(true, "")

TYPE_TO_STRING_AS("null", null_factory);
TYPE_TO_STRING_AS("glfw [window]", glfw_factory);

TEST_SUITE("platform/backend_conformance") {

    TEST_CASE_TEMPLATE(
      "backend_conformance - create and destroy twice in one process",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        for (u32 round = 0; round < 2; ++round) {
            CAPTURE(round);
            const std::unique_ptr<window> created = create_or_fail<Factory>();
            CHECK(created->backend() == Factory::KIND);
            CHECK(live_window_count() == 1);
            CHECK_FALSE(created->should_close());
            static_cast<void>(created->pump_events());
        }
        CHECK(live_window_count() == 0);
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - framebuffer size is window size times content scale",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        static_cast<void>(created->pump_events());

        const math::uvec2 logical = created->window_size();
        const math::uvec2 pixels = created->framebuffer_size();
        const math::vec2 scale = created->content_scale();
        CHECK(logical.x > 0);
        CHECK(logical.y > 0);
        CHECK(scale.x > 0.0f);
        CHECK(scale.y > 0.0f);
        // Rounding of the scaled size may differ by a pixel between the OS and a float product.
        CHECK(static_cast<f32>(pixels.x) == doctest::Approx(static_cast<f32>(logical.x) * scale.x).epsilon(0.01));
        CHECK(static_cast<f32>(pixels.y) == doctest::Approx(static_cast<f32>(logical.y) * scale.y).epsilon(0.01));
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - title round trips",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        CHECK(created->title() == "Conformance");
        created->set_title("Conformance - renamed");
        CHECK(created->title() == "Conformance - renamed");
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - cursor mode round trips for the universal modes",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        logs.allow_errors();    // a backend may report CAPTURED as unavailable through its error callback
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        CHECK(created->current_cursor_mode() == cursor_mode::NORMAL);
        CONJURE_CHECK_OK(created->set_cursor_mode(cursor_mode::HIDDEN));
        CHECK(created->current_cursor_mode() == cursor_mode::HIDDEN);
        CONJURE_CHECK_OK(created->set_cursor_mode(cursor_mode::DISABLED));
        CHECK(created->current_cursor_mode() == cursor_mode::DISABLED);
        CONJURE_CHECK_OK(created->set_cursor_mode(cursor_mode::NORMAL));
        CHECK(created->current_cursor_mode() == cursor_mode::NORMAL);

        // CAPTURED is optional (3.4, not on every display server): either it sticks or it reports
        // FEATURE_UNAVAILABLE and leaves the mode alone.
        const result<void> captured = created->set_cursor_mode(cursor_mode::CAPTURED);
        if (captured.has_value()) {
            CHECK(created->current_cursor_mode() == cursor_mode::CAPTURED);
        } else {
            CHECK(captured.get_error().is(platform_error::FEATURE_UNAVAILABLE));
            CHECK(created->current_cursor_mode() == cursor_mode::NORMAL);
        }
        CONJURE_CHECK_OK(created->set_cursor_mode(cursor_mode::NORMAL));
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - clipboard text round trips",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        CONJURE_CHECK_OK(created->set_clipboard_text("conjure clipboard \xC3\xBC"));
        const result<std::string> text = created->clipboard_text();
        CONJURE_REQUIRE_OK(text);
        CHECK(text.value() == "conjure clipboard \xC3\xBC");
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - a clipboard without text is CLIPBOARD_UNAVAILABLE and an empty copy is text",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        logs.allow_errors();    // glfw reports an empty OS clipboard through its error callback (ERROR)
        const std::unique_ptr<window> created = create_or_fail<Factory>();

        // The null clipboard starts empty; the OS clipboard holds whatever the developer last copied,
        // so on glfw only the error contract is pinned: success or CLIPBOARD_UNAVAILABLE, nothing else.
        const result<std::string> initial = created->clipboard_text();
        if (Factory::KIND == backend_kind::NULL_BACKEND) {
            CONJURE_CHECK_ERROR(initial, platform_error::CLIPBOARD_UNAVAILABLE);
        } else if (!initial.has_value()) {
            CHECK(initial.get_error().is(platform_error::CLIPBOARD_UNAVAILABLE));
        }

        CONJURE_CHECK_OK(created->set_clipboard_text(""));
        const result<std::string> emptyCopy = created->clipboard_text();
        CONJURE_REQUIRE_OK(emptyCopy);
        CHECK(emptyCopy.value().empty());
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - monitors are non-empty with exactly one primary",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        static_cast<void>(created->pump_events());

        const std::span<const monitor_info> monitors = created->monitors();
        REQUIRE_FALSE(monitors.empty());
        u32 primaries = 0;
        for (usize index = 0; index < monitors.size(); ++index) {
            CAPTURE(index);
            if (monitors[index].isPrimary) {
                ++primaries;
            }
            CHECK(monitors[index].currentMode.width > 0);
            CHECK(monitors[index].currentMode.height > 0);
            CHECK_FALSE(created->video_modes(static_cast<u32>(index)).empty());
        }
        CHECK(primaries == 1);
        CHECK(created->video_modes(static_cast<u32>(monitors.size())).empty());
        const u32 missingIndex = static_cast<u32>(monitors.size());
        CONJURE_CHECK_ERROR(created->set_fullscreen(true, missingIndex), platform_error::MONITOR_NOT_FOUND);
        CHECK_FALSE(created->is_fullscreen());
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - the native handle kind matches the backend and OS",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        const native_window_handle handle = created->native_handle();
        if (Factory::KIND == backend_kind::NULL_BACKEND) {
            CHECK(handle.kind == native_handle_kind::NONE);
            CHECK(handle.window == nullptr);
            return;
        }
        // A real window names its display server; which one is the OS's business, not the test's.
        CHECK(handle.kind != native_handle_kind::NONE);
        if (handle.kind == native_handle_kind::X11) {
            CHECK(handle.x11Window != 0);
            CHECK(handle.display != nullptr);
        } else {
            CHECK(handle.window != nullptr);
        }
        if (handle.kind == native_handle_kind::WAYLAND) {
            CHECK(handle.display != nullptr);
        }
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - request_close makes should_close true",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        CHECK_FALSE(created->should_close());
        created->request_close();
        CHECK(created->should_close());
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - set_size delivers one coalesced framebuffer_resized_event on the next pump",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        static_cast<void>(created->pump_events());      // creation-time events (gamepads, focus) go first

        created->set_size(math::uvec2(800, 600));
        CHECK(created->window_size() == math::uvec2(800, 600));

        // window.h: "the framebuffer change arrives as an event". A hidden Win32 window receives its
        // WM_SIZE inside glfwSetWindowSize, before this pump, and must not lose it.
        const window_batch batch = pump_window_events(*created);
        CHECK(batch.resizeCount == 1);
        CHECK(batch.lastResize == created->framebuffer_size());
        CHECK(batch.lastResize.x > 0);
        CHECK(batch.lastResize.y > 0);

        // Nothing changed since: no stale resize is replayed.
        CHECK(pump_window_events(*created).resizeCount == 0);
    }

    TEST_CASE_TEMPLATE(
      "regression - platform-review-1 events raised between two pumps are delivered by the next pump",
      Factory,
      null_factory,
      glfw_factory
    ) {
        // The glfw pump cleared its event vector at the start of pump_events(), so everything the
        // callbacks recorded synchronously inside setters (Win32 and Cocoa raise WM_SIZE / WM_SETFOCUS /
        // windowDidResize inside SetWindowPos and friends) was thrown away before glfwPollEvents ran.
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        static_cast<void>(created->pump_events());

        created->set_size(math::uvec2(800, 600));
        created->set_size(math::uvec2(1000, 700));
        CHECK(created->window_size() == math::uvec2(1000, 700));

        // Two setters, one pump: the resize arrives, coalesced to the final size (P10).
        const window_batch batch = pump_window_events(*created);
        CHECK(batch.resizeCount == 1);
        CHECK(batch.lastResize == created->framebuffer_size());
        CHECK(pump_window_events(*created).resizeCount == 0);
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - set_fullscreen round trip delivers the monitor mode then the windowed size",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        static_cast<void>(created->pump_events());
        const math::uvec2 windowedSize = created->window_size();
        const math::uvec2 windowedFramebuffer = created->framebuffer_size();
        REQUIRE_FALSE(created->monitors().empty());
        const video_mode mode = created->monitors()[0].currentMode;

        CONJURE_CHECK_OK(created->set_fullscreen(true, 0));
        CHECK(created->is_fullscreen());
        const window_batch entered = pump_window_events(*created);
        CHECK(entered.resizeCount == 1);
        CHECK(entered.lastResize == math::uvec2(mode.width, mode.height));
        CHECK(entered.lastResize == created->framebuffer_size());

        // Leaving fullscreen restores the size the OS last confirmed while windowed, as an event.
        CONJURE_CHECK_OK(created->set_fullscreen(false));
        CHECK_FALSE(created->is_fullscreen());
        const window_batch left = pump_window_events(*created);
        CHECK(left.resizeCount == 1);
        CHECK(left.lastResize == windowedFramebuffer);
        CHECK(created->window_size() == windowedSize);
        CHECK(created->framebuffer_size() == windowedFramebuffer);
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - leaving fullscreen on a window created fullscreen centres it on the monitor work area",
      Factory,
      null_factory,
      glfw_factory
    ) {
        // The glfw window restored itself at the monitor origin, the position GLFW reported while it covered the
        // monitor, so Win32 put the title bar above the top of the screen. A glfw fullscreen window is shown and
        // focused even though the factory clears VISIBLE.
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        window_desc desc = Factory::desc();
        desc.fullscreen = true;
        result<std::unique_ptr<window>> created = create_window(desc);
        REQUIRE_MESSAGE(created.has_value(), testing::describe_error(created));
        window &target = *created.value();
        static_cast<void>(target.pump_events());
        REQUIRE(target.is_fullscreen());
        REQUIRE_FALSE(target.monitors().empty());
        const math::urect workArea = target.monitors()[0].workArea;

        CONJURE_CHECK_OK(target.set_fullscreen(false));
        static_cast<void>(target.pump_events());
        CHECK_FALSE(target.is_fullscreen());
        CHECK(target.window_size() == math::uvec2(640, 360));

        // Wayland never tells a client where its window is; there is nothing to compare.
        const math::ivec2 restored = target.position();
        const result<void> positioned = target.set_position(restored);
        if (!positioned.has_value()) {
            CHECK(positioned.get_error().is(platform_error::FEATURE_UNAVAILABLE));
            return;
        }
        const i32 expectedX = static_cast<i32>(workArea.x) + (static_cast<i32>(workArea.width) - 640) / 2;
        const i32 expectedY = static_cast<i32>(workArea.y) + (static_cast<i32>(workArea.height) - 360) / 2;
        CHECK(restored.x >= expectedX - 1);
        CHECK(restored.x <= expectedX + 1);
        CHECK(restored.y >= expectedY - 1);
        CHECK(restored.y <= expectedY + 1);
        if (Factory::KIND == backend_kind::NULL_BACKEND) {
            CHECK(restored == math::ivec2(640, 360));
        }
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - show and hide change visibility and deliver the focus change when the OS moves it",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        static_cast<void>(created->pump_events());

        // The glfw factory starts hidden; showing it briefly takes focus from the terminal (this is a
        // [window] case) and GLFW_FOCUS_ON_SHOW makes that focus gain an event the pump must deliver.
        created->hide();
        CHECK_FALSE(created->is_visible());
        check_focus_follows(*created, created->is_focused());

        const bool focusedBeforeShow = created->is_focused();
        created->show();
        CHECK(created->is_visible());
        check_focus_follows(*created, focusedBeforeShow);

        const bool focusedBeforeHide = created->is_focused();
        created->hide();
        CHECK_FALSE(created->is_visible());
        check_focus_follows(*created, focusedBeforeHide);
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - the same injected sequence yields the same snapshot on every backend",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::vector<input_event> sequence = conformance_sequence();

        // The reference is the backend-free accumulator; a window must add nothing to injection.
        input reference = {};
        for (const input_event &value : sequence) {
            reference.inject(value);
        }
        reference.end_frame();

        const std::unique_ptr<window> created = create_or_fail<Factory>();
        input accumulator = {};
        for (const event &value : created->pump_events()) {
            // A real window may report a focus gain or an initial resize; neither touches input state.
            CHECK_FALSE(accumulator.apply(value));
        }
        for (const input_event &value : sequence) {
            accumulator.inject(value);
        }
        accumulator.end_frame();

        CHECK(accumulator.snapshot() == reference.snapshot());
        CHECK(accumulator.snapshot().is_key_pressed(key::W));
        CHECK(accumulator.snapshot().is_key_released(key::SPACE));
        CHECK(accumulator.snapshot().gamepad(0).is_pressed(gamepad_button::Y));
        CHECK(accumulator.snapshot().mouseDelta == math::vec2(5.0f, 0.0f));
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - set_icon validates first, then applies or reports the platform has no window icons",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();

        // Validation comes before any platform question, so a bad set is INVALID_ARGUMENT on every backend,
        // even where windows have no icon of their own.
        const std::array<image, 1> empty = {image()};
        CONJURE_CHECK_ERROR(created->set_icon(empty), error_code::INVALID_ARGUMENT);

        std::array<image, 3> icons = {image(16, 16), image(32, 32), image(48, 48)};
        for (image &icon : icons) {
            icon.fill(rgba8{0x4C, 0x8D, 0xFF, 255});
        }
        const result<void> applied = created->set_icon(icons);
        if (applied.has_value()) {
            CONJURE_CHECK_OK(created->set_icon({}));
        } else {
            // Cocoa and Wayland: the window shows the bundle's or the desktop file's icon, an expected state
            // that is reported without a log line.
            CHECK(Factory::KIND == backend_kind::GLFW);
            CONJURE_CHECK_ERROR(applied, platform_error::FEATURE_UNAVAILABLE);
            CONJURE_CHECK_ERROR(created->set_icon({}), platform_error::FEATURE_UNAVAILABLE);
            CHECK(logs.count(log_level::WARN) == 0);
        }
    }

    TEST_CASE_TEMPLATE(
      "backend_conformance - Vulkan helpers answer consistently",
      Factory,
      null_factory,
      glfw_factory
    ) {
        CONJURE_CONFORMANCE_BEGIN();
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_or_fail<Factory>();
        const std::span<const char *const> extensions = created->required_vulkan_instance_extensions();
        int fakeInstance = 0;
        void *surface = &fakeInstance;

        // Argument checks come before any Vulkan question on both backends: a null surface pointer
        // or a null instance is INVALID_ARGUMENT, and *surface is cleared before the instance is seen.
        CONJURE_CHECK_ERROR(created->create_vulkan_surface(&fakeInstance, nullptr), error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(created->create_vulkan_surface(nullptr, &surface), error_code::INVALID_ARGUMENT);
        CHECK(surface == nullptr);

        if (Factory::KIND == backend_kind::NULL_BACKEND) {
            CHECK(extensions.empty());
            const result<void> surfaceResult = created->create_vulkan_surface(&fakeInstance, &surface);
            CONJURE_CHECK_ERROR(surfaceResult, platform_error::VULKAN_UNSUPPORTED);
            CHECK(surface == nullptr);
            return;
        }
        // A machine with a Vulkan loader lists at least VK_KHR_surface and one platform extension;
        // without one the list is empty and surface creation says so.
        if (extensions.empty()) {
            const result<void> surfaceResult = created->create_vulkan_surface(&fakeInstance, &surface);
            CONJURE_CHECK_ERROR(surfaceResult, platform_error::VULKAN_UNSUPPORTED);
            return;
        }
        CHECK(extensions.size() >= 2);
        bool hasSurface = false;
        for (const char *extension : extensions) {
            if (std::string_view(extension) == "VK_KHR_surface") {
                hasSurface = true;
            }
        }
        CHECK(hasSurface);
    }
}
