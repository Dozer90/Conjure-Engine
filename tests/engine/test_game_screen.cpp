#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/engine/game_screen.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/platform/backend.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/monitor.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/engine/engine_time.h"
    #include "conjure/scripting/native_api.h"
    #include "conjure/scripting/script_runtime.h"
    #include "engine/subsystems/scripting_subsystem.h"
#endif

#include <array>
#include <limits>
#include <memory>
#include <span>
#include <string>

// game_screen.h (wave 3 GAP-CSHARP 2.2): the game screen of a window (framebuffer pixels, the content scale, the
// UI scale of the monitor under the window centre and the window states), the monitor choice on hand-built
// monitor lists, and the copy the scripting subsystem writes into the per-frame block, which follows a resize, a
// content scale change and a minimise in the next frame.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] std::unique_ptr<platform::null_backend> make_window(math::vec2 contentScale) {
        platform::window_desc desc = {};
        desc.backend = platform::backend_kind::NULL_BACKEND;
        desc.width = 800;
        desc.height = 600;
        desc.position = math::ivec2(100, 100);
        desc.nullContentScale = contentScale;
        return platform::null_backend::create(desc);
    }

    // A monitor at 'position' whose current mode is width x height. Its work area is what a backend reports:
    // clamped like glfw_window.cpp clamp_dimension (a negative origin becomes 0) and 40 short for a task bar, so a
    // choice made through the work area fails these cases.
    [[nodiscard]] platform::monitor_info make_monitor(
      math::ivec2 position,
      u32 width,
      u32 height,
      f32 scale,
      bool primary
    ) {
        platform::monitor_info monitor = {};
        monitor.isPrimary = primary;
        monitor.position = position;
        monitor.currentMode.width = width;
        monitor.currentMode.height = height;
        monitor.workArea.x = static_cast<u32>(position.x < 0 ? 0 : position.x);
        monitor.workArea.y = static_cast<u32>(position.y < 0 ? 0 : position.y);
        monitor.workArea.width = width;
        monitor.workArea.height = height > 40u ? height - 40u : 0u;
        monitor.contentScale = math::vec2(scale);
        return monitor;
    }
}

TEST_SUITE("engine/game_screen") {

    TEST_CASE("game_screen - no window gives the default screen") {
        const game_screen screen = game_screen_of(nullptr);
        CHECK(screen == game_screen{});
        CHECK(screen.pixels == math::uvec2(0));
        CHECK(screen.contentScale == math::vec2(1.0f));
        CHECK(screen.uiScale == 1.0f);
        CHECK_FALSE(screen.focused);
        CHECK_FALSE(screen.minimised);
        CHECK_FALSE(screen.fullscreen);
    }

    TEST_CASE("game_screen - game_screen_of reports the null window") {
        const std::unique_ptr<platform::null_backend> window = make_window(math::vec2(2.0f));
        REQUIRE(window != nullptr);

        SUBCASE("windowed and focused") {
            const game_screen screen = game_screen_of(window.get());
            CHECK(screen.pixels == window->framebuffer_size());
            CHECK(screen.pixels == math::uvec2(1600, 1200));
            CHECK(screen.contentScale == math::vec2(2.0f));
            CHECK(screen.uiScale == 2.0f);                          // the virtual monitor's scale
            CHECK(screen.focused);
            CHECK_FALSE(screen.minimised);
            CHECK_FALSE(screen.fullscreen);
        }
        SUBCASE("minimised: no pixels") {
            window->simulate_minimise(true);
            static_cast<void>(window->pump_events());
            const game_screen screen = game_screen_of(window.get());
            CHECK(screen.minimised);
            CHECK(screen.pixels == math::uvec2(0));
            CHECK(screen.contentScale == math::vec2(2.0f));         // the last ratio is kept while minimised
        }
        SUBCASE("unfocused") {
            window->simulate_focus(false);
            static_cast<void>(window->pump_events());
            CHECK_FALSE(game_screen_of(window.get()).focused);
        }
        SUBCASE("fullscreen covers the monitor") {
            CONJURE_REQUIRE_OK(window->set_fullscreen(true));
            static_cast<void>(window->pump_events());
            const game_screen screen = game_screen_of(window.get());
            CHECK(screen.fullscreen);
            CHECK(screen.pixels == window->framebuffer_size());
            CHECK(screen.uiScale == 2.0f);
        }
        SUBCASE("a window centre outside every monitor takes the primary monitor") {
            CONJURE_REQUIRE_OK(window->set_position(math::ivec2(50000, 50000)));
            REQUIRE(window->monitors().size() == 1);
            REQUIRE(window->monitors()[0].isPrimary);
            CHECK(game_screen_of(window.get()).uiScale == window->monitors()[0].contentScale.y);
        }
    }

    TEST_CASE("game_screen - monitor_ui_scale picks the monitor whose bounds hold the point") {
        // The second monitor is listed first and is not the primary one, so neither "first" nor "primary" can
        // pass for "contains".
        const std::array<platform::monitor_info, 2> monitors = {
          make_monitor(math::ivec2(1920, 0), 2560, 1440, 1.5f, false),
          make_monitor(math::ivec2(0, 0), 1920, 1080, 1.25f, true),
        };

        CHECK(monitor_ui_scale(monitors, math::ivec2(100, 100)) == 1.25f);
        CHECK(monitor_ui_scale(monitors, math::ivec2(2000, 100)) == 1.5f);
        // The right and bottom edges are exclusive: x 1920 is the second monitor's first column.
        CHECK(monitor_ui_scale(monitors, math::ivec2(1919, 0)) == 1.25f);
        CHECK(monitor_ui_scale(monitors, math::ivec2(1920, 0)) == 1.5f);
        CHECK(monitor_ui_scale(monitors, math::ivec2(2000, 1440)) == 1.25f);
        // Over the task bar (below the work area, inside the bounds): still the monitor under it.
        CHECK(monitor_ui_scale(monitors, math::ivec2(2000, 1430)) == 1.5f);
        // Outside every monitor, a negative point included: the primary monitor.
        CHECK(monitor_ui_scale(monitors, math::ivec2(9000, 9000)) == 1.25f);
        CHECK(monitor_ui_scale(monitors, math::ivec2(-10, -10)) == 1.25f);
    }

    TEST_CASE("game_screen - monitor_ui_scale finds a monitor left of or above the primary") {
        // A backend clamps a negative work area origin to 0, so the left and the upper monitor's work areas both
        // claim the primary's corner; they are listed before the monitors they would take points from.
        const std::array<platform::monitor_info, 4> monitors = {
          make_monitor(math::ivec2(-2560, 0), 2560, 1440, 2.0f, false),
          make_monitor(math::ivec2(1920, -1080), 1920, 1080, 1.75f, false),
          make_monitor(math::ivec2(0, 0), 1920, 1080, 1.0f, true),
          make_monitor(math::ivec2(1920, 0), 1920, 1080, 1.5f, false),
        };

        CHECK(monitor_ui_scale(monitors, math::ivec2(-100, 500)) == 2.0f);        // the left monitor
        CHECK(monitor_ui_scale(monitors, math::ivec2(2100, -500)) == 1.75f);      // the upper monitor
        CHECK(monitor_ui_scale(monitors, math::ivec2(100, 500)) == 1.0f);         // the primary
        CHECK(monitor_ui_scale(monitors, math::ivec2(2100, 500)) == 1.5f);        // the right monitor
    }

    TEST_CASE("game_screen - monitor_ui_scale falls back to the first monitor, then to 1") {
        SUBCASE("no primary monitor: the first one") {
            const std::array<platform::monitor_info, 2> monitors = {
              make_monitor(math::ivec2(0, 0), 100, 100, 1.75f, false),
              make_monitor(math::ivec2(100, 0), 100, 100, 3.0f, false),
            };
            CHECK(monitor_ui_scale(monitors, math::ivec2(500, 500)) == 1.75f);
        }
        SUBCASE("a monitor whose current mode is 0 x 0 contains nothing") {
            const std::array<platform::monitor_info, 2> monitors = {
              make_monitor(math::ivec2(0, 0), 0, 0, 3.0f, false),
              make_monitor(math::ivec2(100, 0), 100, 100, 1.5f, true),
            };
            CHECK(monitor_ui_scale(monitors, math::ivec2(0, 0)) == 1.5f);
        }
        SUBCASE("no monitor at all") {
            CHECK(monitor_ui_scale(std::span<const platform::monitor_info>(), math::ivec2(0, 0)) == 1.0f);
        }
        SUBCASE("a scale that is not a positive finite number") {
            const std::array<f32, 4> invalidScales = {
              0.0f,
              -2.0f,
              std::numeric_limits<f32>::quiet_NaN(),
              std::numeric_limits<f32>::infinity(),
            };
            for (usize index = 0; index < invalidScales.size(); ++index) {
                CAPTURE(index);
                const std::array<platform::monitor_info, 1> monitors = {
                  make_monitor(math::ivec2(0, 0), 100, 100, invalidScales[index], true),
                };
                CHECK(monitor_ui_scale(monitors, math::ivec2(10, 10)) == 1.0f);
            }
        }
    }

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    TEST_CASE("game_screen - write_frame_block copies the screen into the frame block") {
        const std::unique_ptr<platform::null_backend> window = make_window(math::vec2(1.5f));
        REQUIRE(window != nullptr);
        CONJURE_REQUIRE_OK(window->set_fullscreen(true));
        static_cast<void>(window->pump_events());
        const engine_time time = {};
        const platform::input_state input = {};

        conjure_frame_block block = {};
        scripting_subsystem::write_frame_block(block, time, input, window.get());
        CHECK(block.screen_width == window->framebuffer_size().x);
        CHECK(block.screen_height == window->framebuffer_size().y);
        CHECK(block.content_scale_x == 1.5f);
        CHECK(block.content_scale_y == 1.5f);
        CHECK(block.ui_scale == 1.5f);
        CHECK(block.window_state == (scripting::FRAME_WINDOW_FOCUSED | scripting::FRAME_WINDOW_FULLSCREEN));

        // No window: the screen of none, every state bit cleared (a previous frame's values do not survive).
        scripting_subsystem::write_frame_block(block, time, input, nullptr);
        CHECK(block.screen_width == 0);
        CHECK(block.screen_height == 0);
        CHECK(block.content_scale_x == 1.0f);
        CHECK(block.content_scale_y == 1.0f);
        CHECK(block.ui_scale == 1.0f);
        CHECK(block.window_state == 0);
    }

    TEST_CASE("game_screen - the frame block follows the window in the next frame") {
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        config.scripting.enabled = true;
        config.scripting.required = true;
        config.scripting.useNullHost = true;
        config.scripting.hotReload = false;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().scripting != nullptr);
        REQUIRE(app.context().window != nullptr);
        platform::null_backend *window = app.context().window->as_null();
        REQUIRE(window != nullptr);
        const conjure_frame_block &block = app.context().scripting->frame_block();

        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(block.screen_width == window->framebuffer_size().x);
        CHECK(block.screen_height == window->framebuffer_size().y);
        CHECK((block.window_state & scripting::FRAME_WINDOW_MINIMISED) == 0);

        SUBCASE("a resize") {
            window->simulate_resize(640, 480);
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(block.screen_width == window->framebuffer_size().x);
            CHECK(block.screen_height == window->framebuffer_size().y);
            CHECK(block.screen_width == static_cast<u32>(640.0f * block.content_scale_x + 0.5f));
        }
        SUBCASE("a content scale change") {
            window->simulate_content_scale(math::vec2(1.5f, 1.5f));
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(block.content_scale_x == 1.5f);
            CHECK(block.content_scale_y == 1.5f);
            CHECK(block.ui_scale == 1.5f);                          // the null monitor follows the window
            CHECK(block.screen_width == window->framebuffer_size().x);
        }
        SUBCASE("a minimise") {
            window->simulate_minimise(true);
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(block.screen_width == 0);
            CHECK(block.screen_height == 0);
            CHECK((block.window_state & scripting::FRAME_WINDOW_MINIMISED) != 0);
        }
        app.shutdown();
    }
#endif
}
