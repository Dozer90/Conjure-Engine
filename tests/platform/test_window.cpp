#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/null_backend.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

TEST_SUITE("platform/window") {

    TEST_CASE("window_desc - defaults are the engine's default window") {
        const window_desc desc = {};
        CHECK(desc.backend == backend_kind::GLFW);
        CHECK(desc.title == "Conjure");
        CHECK(desc.width == 1600);
        CHECK(desc.height == 900);
        CHECK(desc.flags == window_flags::DEFAULT);
        CHECK_FALSE(desc.fullscreen);
        CHECK(desc.monitorIndex == 0);
        CHECK_FALSE(desc.position.has_value());
        CHECK(desc.displayServer == display_server_hint::ANY);
        CHECK(desc.gamepadMappings.empty());
        CHECK(desc.nullContentScale == math::vec2(1.0f));
        CHECK(MAX_WINDOW_TITLE_BYTES == 255);
    }

    TEST_CASE("window_flags - DEFAULT is resizable, decorated, visible and scaled") {
        CHECK(has_flag(window_flags::DEFAULT, window_flags::RESIZABLE));
        CHECK(has_flag(window_flags::DEFAULT, window_flags::DECORATED));
        CHECK(has_flag(window_flags::DEFAULT, window_flags::VISIBLE));
        CHECK(has_flag(window_flags::DEFAULT, window_flags::SCALE_TO_MONITOR));
        CHECK(has_flag(window_flags::DEFAULT, window_flags::SCALE_FRAMEBUFFER));
        CHECK_FALSE(has_flag(window_flags::DEFAULT, window_flags::FLOATING));
        CHECK_FALSE(has_flag(window_flags::DEFAULT, window_flags::MAXIMISED));
        CHECK(static_cast<u8>(window_flags::NONE) == 0);
        CHECK(static_cast<u8>(window_flags::RESIZABLE) == 1);
        CHECK(static_cast<u8>(window_flags::SCALE_FRAMEBUFFER) == 64);

        window_flags flags = window_flags::DEFAULT;
        flags &= ~window_flags::VISIBLE;
        CHECK_FALSE(has_flag(flags, window_flags::VISIBLE));
        flags |= window_flags::FLOATING;
        CHECK(has_flag(flags, window_flags::FLOATING));
        CHECK((flags ^ window_flags::FLOATING) == without_flag(flags, window_flags::FLOATING));
    }

    TEST_CASE("cursor_mode - values mirror GLFW and names round trip") {
        CHECK(static_cast<u8>(cursor_mode::NORMAL) == 0);
        CHECK(static_cast<u8>(cursor_mode::HIDDEN) == 1);
        CHECK(static_cast<u8>(cursor_mode::DISABLED) == 2);
        CHECK(static_cast<u8>(cursor_mode::CAPTURED) == 3);
        for (u8 code = 0; code <= 3; ++code) {
            CAPTURE(code);
            const cursor_mode mode = static_cast<cursor_mode>(code);
            CHECK_FALSE(to_string(mode).empty());
            CHECK(parse_cursor_mode(to_string(mode)) == mode);
        }
        CHECK(to_string(cursor_mode::DISABLED) == "DISABLED");
        CHECK(to_string(static_cast<cursor_mode>(4)).empty());
        CHECK(parse_cursor_mode("hidden") == cursor_mode::HIDDEN);
        CHECK_FALSE(parse_cursor_mode("LOCKED").has_value());
        CHECK_FALSE(parse_cursor_mode("").has_value());
    }

    TEST_CASE("display_server_hint - enumerators are stable") {
        CHECK(static_cast<u8>(display_server_hint::ANY) == 0);
        CHECK(static_cast<u8>(display_server_hint::X11) == 1);
        CHECK(static_cast<u8>(display_server_hint::WAYLAND) == 2);
    }

    TEST_CASE("validate_window_desc - accepts the defaults and a fullscreen window without a size") {
        CONJURE_CHECK_OK(validate_window_desc(window_desc()));
        CONJURE_CHECK_OK(validate_window_desc(null_desc()));

        window_desc fullscreen = null_desc(0, 0);
        fullscreen.fullscreen = true;
        CONJURE_CHECK_OK(validate_window_desc(fullscreen));

        window_desc longest = null_desc();
        longest.title = std::string(MAX_WINDOW_TITLE_BYTES, 'x');
        CONJURE_CHECK_OK(validate_window_desc(longest));
    }

    TEST_CASE("validate_window_desc - rejects a zero size, an over-long title and a bad backend") {
        const result<void> zeroWidth = validate_window_desc(null_desc(0, 720));
        CONJURE_CHECK_ERROR(zeroWidth, platform_error::INVALID_DESC);
        CHECK(zeroWidth.get_error().message().find("width") != std::string::npos);
        CONJURE_CHECK_ERROR(validate_window_desc(null_desc(1280, 0)), platform_error::INVALID_DESC);

        window_desc longTitle = null_desc();
        longTitle.title = std::string(MAX_WINDOW_TITLE_BYTES + 1, 't');
        const result<void> titleResult = validate_window_desc(longTitle);
        CONJURE_CHECK_ERROR(titleResult, platform_error::INVALID_DESC);
        CHECK(titleResult.get_error().message().find("title") != std::string::npos);

        window_desc badBackend = null_desc();
        badBackend.backend = backend_kind::COUNT;
        const result<void> backendResult = validate_window_desc(badBackend);
        CONJURE_CHECK_ERROR(backendResult, platform_error::INVALID_DESC);
        CHECK(backendResult.get_error().message().find("backend") != std::string::npos);
    }

    TEST_CASE("validate_window_icon - accepts no images, any non-empty size and the largest allowed set") {
        CONJURE_CHECK_OK(validate_window_icon({}));

        const std::array<image, 2> mixed = {image(16, 16), image(48, 32)};
        CONJURE_CHECK_OK(validate_window_icon(mixed));

        const std::vector<image> most(
          MAX_WINDOW_ICON_IMAGES,
          image(MAX_WINDOW_ICON_DIMENSION, MAX_WINDOW_ICON_DIMENSION)
        );
        CONJURE_CHECK_OK(validate_window_icon(most));
    }

    TEST_CASE("validate_window_icon - rejects too many, empty and oversized images naming the first offender") {
        const std::vector<image> tooMany(MAX_WINDOW_ICON_IMAGES + 1, image(16, 16));
        const result<void> countResult = validate_window_icon(tooMany);
        CONJURE_CHECK_ERROR(countResult, error_code::INVALID_ARGUMENT);
        CHECK(countResult.get_error().message().find("17 images") != std::string::npos);

        const std::array<image, 3> withEmpty = {image(16, 16), image(), image()};
        const result<void> emptyResult = validate_window_icon(withEmpty);
        CONJURE_CHECK_ERROR(emptyResult, error_code::INVALID_ARGUMENT);
        CHECK(emptyResult.get_error().message().find("image 1 is empty") != std::string::npos);

        const std::array<image, 1> wide = {image(MAX_WINDOW_ICON_DIMENSION + 1, 16)};
        CONJURE_CHECK_ERROR(validate_window_icon(wide), error_code::INVALID_ARGUMENT);
        const std::array<image, 1> tall = {image(16, MAX_WINDOW_ICON_DIMENSION + 1)};
        const result<void> tallResult = validate_window_icon(tall);
        CONJURE_CHECK_ERROR(tallResult, error_code::INVALID_ARGUMENT);
        CHECK(tallResult.get_error().message().find("16 x 257") != std::string::npos);
    }

    TEST_CASE("create_window - rejects an invalid description with INVALID_DESC and creates nothing") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        CONJURE_CHECK_ERROR(create_window(null_desc(0, 0)), platform_error::INVALID_DESC);
        window_desc longTitle = null_desc();
        longTitle.title = std::string(300, 't');
        CONJURE_CHECK_ERROR(create_window(longTitle), platform_error::INVALID_DESC);
        CHECK(live_window_count() == 0);
    }

    TEST_CASE("create_window - the null backend happy path") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        CHECK(live_window_count() == 0);
        {
            const std::unique_ptr<window> created = create_null_window();
            CHECK(live_window_count() == 1);
            CHECK(created->backend() == backend_kind::NULL_BACKEND);
            CHECK(created->as_null() != nullptr);
            CHECK(created->as_null() == static_cast<null_backend *>(created.get()));
            CHECK(created->window_size() == math::uvec2(1280, 720));
            CHECK(created->framebuffer_size() == math::uvec2(1280, 720));
            CHECK(created->content_scale() == math::vec2(1.0f));
            CHECK(created->title() == "Conjure Test");
            CHECK_FALSE(created->should_close());
            CHECK_FALSE(created->is_minimised());
            CHECK(created->is_focused());
            CHECK(created->is_visible());
            CHECK_FALSE(created->is_fullscreen());
            CHECK(created->current_cursor_mode() == cursor_mode::NORMAL);
            CHECK(created->position() == math::ivec2(320, 180));
            CHECK(logs.contains(log_level::INFO, "null window"));
        }
        CHECK(live_window_count() == 0);
    }

    TEST_CASE("create_window - honours the position, visibility and content scale of the description") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        window_desc desc = null_desc(800, 600);
        desc.position = math::ivec2(-10, 25);
        desc.flags = window_flags::DEFAULT & ~window_flags::VISIBLE;
        desc.nullContentScale = math::vec2(2.0f, 1.5f);
        const std::unique_ptr<window> created = create_null_window(desc);

        CHECK(created->position() == math::ivec2(-10, 25));
        CHECK_FALSE(created->is_visible());
        CHECK(created->content_scale() == math::vec2(2.0f, 1.5f));
        CHECK(created->window_size() == math::uvec2(800, 600));
        CHECK(created->framebuffer_size() == math::uvec2(1600, 900));
        created->show();
        CHECK(created->is_visible());
        created->hide();
        CHECK_FALSE(created->is_visible());
    }

    TEST_CASE("create_window - a fullscreen null window takes the virtual monitor's mode") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        window_desc desc = null_desc(0, 0);
        desc.fullscreen = true;
        const std::unique_ptr<window> created = create_null_window(desc);
        CHECK(created->is_fullscreen());
        CHECK(created->window_size() == math::uvec2(1920, 1080));
        CHECK(created->framebuffer_size() == math::uvec2(1920, 1080));

        CONJURE_CHECK_OK(created->set_fullscreen(false));
        CHECK_FALSE(created->is_fullscreen());
        CHECK(created->window_size() == math::uvec2(1920, 1080));
    }

    TEST_CASE("create_window - a fullscreen description naming a missing monitor fails") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        window_desc desc = null_desc();
        desc.fullscreen = true;
        desc.monitorIndex = 1;
        CONJURE_CHECK_ERROR(create_window(desc), platform_error::MONITOR_NOT_FOUND);
        CHECK(live_window_count() == 0);
    }

    TEST_CASE("window - title, size, cursor mode, raw motion, position and clipboard round trip") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();

        created->set_title("Renamed");
        CHECK(created->title() == "Renamed");
        created->set_title("");
        CHECK(created->title().empty());

        created->set_size(math::uvec2(640, 480));
        CHECK(created->window_size() == math::uvec2(640, 480));
        CHECK(created->framebuffer_size() == math::uvec2(640, 480));

        CONJURE_CHECK_OK(created->set_cursor_mode(cursor_mode::DISABLED));
        CHECK(created->current_cursor_mode() == cursor_mode::DISABLED);
        CONJURE_CHECK_OK(created->set_cursor_mode(cursor_mode::CAPTURED));
        CHECK(created->current_cursor_mode() == cursor_mode::CAPTURED);
        CONJURE_CHECK_OK(created->set_cursor_mode(cursor_mode::NORMAL));
        CHECK(created->current_cursor_mode() == cursor_mode::NORMAL);

        CONJURE_CHECK_OK(created->set_raw_mouse_motion(true));
        CHECK(created->as_null()->raw_mouse_motion());
        CONJURE_CHECK_OK(created->set_raw_mouse_motion(false));
        CHECK_FALSE(created->as_null()->raw_mouse_motion());

        CONJURE_CHECK_OK(created->set_position(math::ivec2(100, 200)));
        CHECK(created->position() == math::ivec2(100, 200));

        created->set_cursor_position(math::vec2(12.5f, 7.0f));
        CHECK(created->as_null()->cursor_position() == math::vec2(12.5f, 7.0f));

        CONJURE_CHECK_ERROR(created->clipboard_text(), platform_error::CLIPBOARD_UNAVAILABLE);
        CONJURE_CHECK_OK(created->set_clipboard_text("copied text \xC3\xA9"));
        result<std::string> text = created->clipboard_text();
        CONJURE_REQUIRE_OK(text);
        CHECK(text.value() == "copied text \xC3\xA9");
        CONJURE_CHECK_OK(created->set_clipboard_text(""));
        result<std::string> cleared = created->clipboard_text();
        CONJURE_REQUIRE_OK(cleared);
        CHECK(cleared.value().empty());
    }

    TEST_CASE("window - set_icon keeps its own copy, rejects invalid images and restores the default when empty") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        REQUIRE(created->as_null() != nullptr);
        CHECK(created->as_null()->icons().empty());

        constexpr rgba8 INK = {0x4C, 0x8D, 0xFF, 255};
        std::array<image, 2> icons = {image(16, 16), image(32, 32)};
        icons[0].fill(INK);
        CONJURE_REQUIRE_OK(created->set_icon(icons));
        icons[0].fill(rgba8{});
        REQUIRE(created->as_null()->icons().size() == 2);
        CHECK(created->as_null()->icons()[0].pixel(15, 15) == INK);
        CHECK(created->as_null()->icons()[1].width() == 32);

        const std::array<image, 1> oversized = {image(MAX_WINDOW_ICON_DIMENSION + 1, 1)};
        CONJURE_CHECK_ERROR(created->set_icon(oversized), error_code::INVALID_ARGUMENT);
        CHECK(created->as_null()->icons().size() == 2);

        CONJURE_CHECK_OK(created->set_icon({}));
        CHECK(created->as_null()->icons().empty());
    }

    TEST_CASE("window - set_fullscreen resizes to the virtual mode and restores the windowed size") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window(null_desc(1000, 500));

        CONJURE_CHECK_OK(created->set_fullscreen(true));
        CHECK(created->is_fullscreen());
        CHECK(created->window_size() == math::uvec2(1920, 1080));
        CHECK(created->framebuffer_size() == math::uvec2(1920, 1080));

        CONJURE_CHECK_OK(created->set_fullscreen(true));    // already fullscreen: no-op
        CHECK(created->window_size() == math::uvec2(1920, 1080));

        CONJURE_CHECK_OK(created->set_fullscreen(false));
        CHECK_FALSE(created->is_fullscreen());
        CHECK(created->window_size() == math::uvec2(1000, 500));
        CONJURE_CHECK_OK(created->set_fullscreen(false));
        CHECK(created->window_size() == math::uvec2(1000, 500));
    }

    TEST_CASE("window - set_fullscreen with a missing monitor fails and leaves the window unchanged") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();

        const result<void> missing = created->set_fullscreen(true, 1);
        CONJURE_CHECK_ERROR(missing, platform_error::MONITOR_NOT_FOUND);
        CHECK(missing.get_error().message().find("1") != std::string::npos);
        CHECK_FALSE(created->is_fullscreen());
        CHECK(created->window_size() == math::uvec2(1280, 720));
        CHECK(created->as_null()->pending_event_count() == 0);
        CONJURE_CHECK_ERROR(created->set_fullscreen(true, 0xFFFFFFFFu), platform_error::MONITOR_NOT_FOUND);
    }

    TEST_CASE("window - the null window has no Vulkan support") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();

        CHECK(created->required_vulkan_instance_extensions().empty());
        int fakeInstance = 0;
        void *surface = &fakeInstance;
        const result<void> surfaceResult = created->create_vulkan_surface(&fakeInstance, &surface);
        CONJURE_CHECK_ERROR(surfaceResult, platform_error::VULKAN_UNSUPPORTED);
        CHECK(surface == nullptr);
        CONJURE_CHECK_ERROR(created->create_vulkan_surface(nullptr, nullptr), error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("regression - platform-review-6 create_vulkan_surface checks its pointers like the glfw backend") {
        // The null backend answered VULKAN_UNSUPPORTED to every input while the glfw backend rejected
        // null pointers with INVALID_ARGUMENT first; the rhi needs one contract.
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();

        int fakeInstance = 0;
        void *surface = &fakeInstance;
        CONJURE_CHECK_ERROR(created->create_vulkan_surface(nullptr, &surface), error_code::INVALID_ARGUMENT);
        CHECK(surface == nullptr);      // cleared before the instance is looked at, as on glfw

        CONJURE_CHECK_ERROR(created->create_vulkan_surface(&fakeInstance, nullptr), error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(created->create_vulkan_surface(nullptr, nullptr), error_code::INVALID_ARGUMENT);

        surface = &fakeInstance;
        const result<void> unsupported = created->create_vulkan_surface(&fakeInstance, &surface);
        CONJURE_CHECK_ERROR(unsupported, platform_error::VULKAN_UNSUPPORTED);
        CHECK(surface == nullptr);
    }

    TEST_CASE("regression - platform-review-5 the null clipboard is CLIPBOARD_UNAVAILABLE until text is copied") {
        // The null backend returned an empty string where the glfw backend (and window.h) report
        // CLIPBOARD_UNAVAILABLE; an empty copy counts as text on both, as it does on the OS clipboards.
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();

        const result<std::string> untouched = created->clipboard_text();
        CONJURE_CHECK_ERROR(untouched, platform_error::CLIPBOARD_UNAVAILABLE);
        CHECK(untouched.get_error().message().find("no text") != std::string::npos);

        CONJURE_CHECK_OK(created->set_clipboard_text(""));
        const result<std::string> emptyCopy = created->clipboard_text();
        CONJURE_REQUIRE_OK(emptyCopy);
        CHECK(emptyCopy.value().empty());

        CONJURE_CHECK_OK(created->set_clipboard_text("text"));
        const result<std::string> text = created->clipboard_text();
        CONJURE_REQUIRE_OK(text);
        CHECK(text.value() == "text");
    }

    TEST_CASE("window - request_close makes should_close true") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        CHECK_FALSE(created->should_close());
        created->request_close();
        CHECK(created->should_close());
        created->request_close();
        CHECK(created->should_close());
    }

    TEST_CASE("window - gamepad_name is empty for every slot until a gamepad connects") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        for (u32 index = 0; index < MAX_GAMEPADS; ++index) {
            CAPTURE(index);
            CHECK(created->gamepad_name(index).empty());
        }
        CHECK(created->gamepad_name(MAX_GAMEPADS).empty());
        CHECK(created->gamepad_name(0xFFFFFFFFu).empty());
    }

    TEST_CASE("create_window - a second live window asserts and fails with INVALID_DESC") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> first = create_null_window();

        testing::expect_assert assertGuard = {};
        const result<std::unique_ptr<window>> second = create_window(null_desc());
        CHECK(assertGuard.count() == 1);
        CONJURE_CHECK_ERROR(second, platform_error::INVALID_DESC);
        CHECK(live_window_count() == 1);
    }

    TEST_CASE("create_window - a window can be created again after the previous one is destroyed") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        for (u32 round = 0; round < 3; ++round) {
            CAPTURE(round);
            const std::unique_ptr<window> created = create_null_window();
            CHECK(live_window_count() == 1);
            CHECK(created->window_size() == math::uvec2(1280, 720));
        }
        CHECK(live_window_count() == 0);
    }
}
