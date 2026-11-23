#include "conjure/testing/doctest.h"

#include "platform/content_scale.h"

#include <array>
#include <optional>

// Private-header test of the glfw backend's content scale rules, which need no window.

using namespace conjure;
using namespace conjure::platform::detail;

namespace {
    scaled_sizes sizes_of(math::uvec2 framebuffer, math::uvec2 window) {
        scaled_sizes sizes = {};
        sizes.framebuffer = framebuffer;
        sizes.window = window;
        return sizes;
    }

    // What GLFW reports for a window at numerator / 120 (the Wayland fractional scale unit; Cocoa's backing
    // rectangle truncates the same way): the framebuffer is window * scale cut to whole pixels.
    scaled_sizes truncated_sizes(math::uvec2 window, u32 numerator) {
        const math::uvec2 framebuffer = math::uvec2(window.x * numerator / 120, window.y * numerator / 120);
        return sizes_of(framebuffer, window);
    }

    constexpr u32 LARGEST = 0xFFFFFFFFu;
}

TEST_SUITE("platform/content_scale") {

    TEST_CASE("framebuffer_content_scale - is the framebuffer to window ratio per axis") {
        const math::vec2 previous = math::vec2(3.0f, 4.0f);     // ignored while both sizes have an area

        SUBCASE("a framebuffer as large as the window (Windows, X11) is 1") {
            const scaled_sizes sizes = sizes_of(math::uvec2(640, 360), math::uvec2(640, 360));
            CHECK(framebuffer_content_scale(sizes, previous) == math::vec2(1.0f));
        }
        SUBCASE("a Retina framebuffer is 2") {
            const scaled_sizes sizes = sizes_of(math::uvec2(1280, 720), math::uvec2(640, 360));
            CHECK(framebuffer_content_scale(sizes, previous) == math::vec2(2.0f));
        }
        SUBCASE("a fractional output scale is the exact ratio") {
            const scaled_sizes sizes = sizes_of(math::uvec2(800, 450), math::uvec2(640, 360));
            CHECK(framebuffer_content_scale(sizes, previous) == math::vec2(1.25f));
        }
        SUBCASE("each axis has its own ratio") {
            const scaled_sizes sizes = sizes_of(math::uvec2(200, 150), math::uvec2(100, 100));
            CHECK(framebuffer_content_scale(sizes, previous) == math::vec2(2.0f, 1.5f));
        }
        SUBCASE("the largest sizes divide without overflow") {
            CHECK(framebuffer_content_scale(sizes_of(math::uvec2(LARGEST), math::uvec2(LARGEST)), previous)
                  == math::vec2(1.0f));
            const math::vec2 scale =
              framebuffer_content_scale(sizes_of(math::uvec2(LARGEST, 1), math::uvec2(1, LARGEST)), previous);
            CHECK(scale.x == doctest::Approx(4294967295.0));
            CHECK(scale.y > 0.0f);
            CHECK(scale.y < 1.0e-9f);
        }
    }

    TEST_CASE("framebuffer_content_scale - keeps the previous scale while either size has a zero axis") {
        const math::vec2 previous = math::vec2(2.0f, 1.5f);
        const scaled_sizes announced = sizes_of(math::uvec2(1280, 720), math::uvec2(640, 360));
        // A minimised window is 0 x 0 on both; the others are the half-updated states a backend may report.
        const std::array<scaled_sizes, 5> degenerate = {
            sizes_of(math::uvec2(0, 0), math::uvec2(0, 0)),
            sizes_of(math::uvec2(0, 0), math::uvec2(640, 360)),
            sizes_of(math::uvec2(1280, 720), math::uvec2(0, 0)),
            sizes_of(math::uvec2(1280, 0), math::uvec2(640, 360)),
            sizes_of(math::uvec2(1280, 720), math::uvec2(0, 360)),
        };
        for (usize index = 0; index < degenerate.size(); ++index) {
            CAPTURE(index);
            CHECK(framebuffer_content_scale(degenerate[index], previous) == previous);
            CHECK_FALSE(content_scale_outdated(announced, degenerate[index]));
        }
    }

    TEST_CASE("content_scale_outdated - is false while one scale can have produced both pairs of sizes") {
        SUBCASE("the same sizes, or a resize at the same whole scale") {
            const scaled_sizes standard = sizes_of(math::uvec2(640, 360), math::uvec2(640, 360));
            const scaled_sizes retina = sizes_of(math::uvec2(1280, 720), math::uvec2(640, 360));
            CHECK_FALSE(content_scale_outdated(standard, standard));
            CHECK_FALSE(content_scale_outdated(standard, sizes_of(math::uvec2(1920, 1080), math::uvec2(1920, 1080))));
            CHECK_FALSE(content_scale_outdated(retina, sizes_of(math::uvec2(3840, 2160), math::uvec2(1920, 1080))));
        }
        SUBCASE("a fractional scale announced at a small window and truncated at a far larger one") {
            // 125 % announced at 641 x 361 (801 x 451, a ratio of 1.24961), then maximised to 3841 x 2161
            // (4801 x 2701). The announced ratio predicts 4799.75 pixels, 1.25 away from the framebuffer.
            const scaled_sizes announced = truncated_sizes(math::uvec2(641, 361), 150);
            const scaled_sizes maximised = truncated_sizes(math::uvec2(3841, 2161), 150);
            CHECK(announced.framebuffer == math::uvec2(801, 451));
            CHECK(maximised.framebuffer == math::uvec2(4801, 2701));
            CHECK_FALSE(content_scale_outdated(announced, maximised));
            CHECK_FALSE(content_scale_outdated(maximised, announced));
        }
        SUBCASE("every fractional scale from 100 % to 300 % across window sizes") {
            const std::array<math::uvec2, 6> windows = {
                math::uvec2(1, 1),
                math::uvec2(7, 3),
                math::uvec2(641, 361),
                math::uvec2(1280, 720),
                math::uvec2(3072, 1728),
                math::uvec2(7681, 4321),
            };
            for (u32 numerator = 120; numerator <= 360; ++numerator) {
                for (const math::uvec2 announcedWindow : windows) {
                    for (const math::uvec2 currentWindow : windows) {
                        CAPTURE(numerator);
                        CAPTURE(announcedWindow.x);
                        CAPTURE(currentWindow.x);
                        const scaled_sizes announced = truncated_sizes(announcedWindow, numerator);
                        const scaled_sizes current = truncated_sizes(currentWindow, numerator);
                        CHECK_FALSE(content_scale_outdated(announced, current));
                    }
                }
            }
        }
        SUBCASE("nothing announced yet and a framebuffer as large as the window") {
            const scaled_sizes nothing = {};
            CHECK_FALSE(content_scale_outdated(nothing, sizes_of(math::uvec2(640, 360), math::uvec2(640, 360))));
        }
        SUBCASE("the largest sizes compare without overflow") {
            const scaled_sizes largest = sizes_of(math::uvec2(LARGEST), math::uvec2(LARGEST));
            CHECK_FALSE(content_scale_outdated(largest, largest));
            CHECK_FALSE(content_scale_outdated(largest, sizes_of(math::uvec2(1), math::uvec2(1))));
        }
    }

    TEST_CASE("content_scale_outdated - is true once no single scale can have produced both pairs of sizes") {
        const scaled_sizes standard = sizes_of(math::uvec2(640, 360), math::uvec2(640, 360));
        const scaled_sizes retina = sizes_of(math::uvec2(1280, 720), math::uvec2(640, 360));

        SUBCASE("a window moved from a standard to a Retina display and back") {
            CHECK(content_scale_outdated(standard, retina));
            CHECK(content_scale_outdated(retina, standard));
        }
        SUBCASE("a Wayland output changed from 125 % to 150 % while the window was resized") {
            const scaled_sizes announced = truncated_sizes(math::uvec2(641, 361), 150);
            CHECK(content_scale_outdated(announced, truncated_sizes(math::uvec2(641, 361), 180)));
            CHECK(content_scale_outdated(announced, truncated_sizes(math::uvec2(3841, 2161), 180)));
        }
        SUBCASE("one axis is enough") {
            CHECK(content_scale_outdated(standard, sizes_of(math::uvec2(640, 720), math::uvec2(640, 360))));
            CHECK(content_scale_outdated(standard, sizes_of(math::uvec2(1280, 360), math::uvec2(640, 360))));
        }
        SUBCASE("one pixel more at the same window, even on a one pixel window") {
            const scaled_sizes hundred = sizes_of(math::uvec2(100, 100), math::uvec2(100, 100));
            CHECK(content_scale_outdated(hundred, sizes_of(math::uvec2(101, 100), math::uvec2(100, 100))));
            const scaled_sizes single = sizes_of(math::uvec2(1, 1), math::uvec2(1, 1));
            CHECK(content_scale_outdated(single, sizes_of(math::uvec2(2, 2), math::uvec2(1, 1))));
        }
        SUBCASE("nothing announced yet and a framebuffer larger than the window") {
            const scaled_sizes nothing = {};
            CHECK(content_scale_outdated(nothing, retina));
            const scaled_sizes halfAnnounced = sizes_of(math::uvec2(0, 360), math::uvec2(640, 360));
            CHECK(content_scale_outdated(halfAnnounced, retina));
        }
        SUBCASE("the largest sizes compare without overflow") {
            const scaled_sizes wide = sizes_of(math::uvec2(LARGEST, 1), math::uvec2(1, 1));
            const scaled_sizes narrow = sizes_of(math::uvec2(1, 1), math::uvec2(LARGEST, 1));
            CHECK(content_scale_outdated(wide, narrow));
            CHECK(content_scale_outdated(narrow, wide));
        }
    }

    TEST_CASE("regression - code sweep F015 content_scale_tracker announces from the sizes the callbacks recorded") {
        // The glfw pump queried both sizes on every pump, two round trips to the X server per frame on X11. It now
        // announces from the sizes the last size callback recorded, so the tracker alone decides, with no query.
        const scaled_sizes standard = sizes_of(math::uvec2(640, 360), math::uvec2(640, 360));
        const scaled_sizes retina = sizes_of(math::uvec2(1280, 720), math::uvec2(640, 360));

        SUBCASE("the scale at creation is the starting point, not a change") {
            content_scale_tracker tracker = {};
            tracker.start(retina);
            CHECK(tracker.scale() == math::vec2(2.0f));
            CHECK_FALSE(tracker.announce().has_value());
        }
        SUBCASE("a recorded change is announced once, with the recorded ratio") {
            content_scale_tracker tracker = {};
            tracker.start(standard);
            tracker.record(retina);
            CHECK(tracker.scale() == math::vec2(2.0f));
            const std::optional<math::vec2> announced = tracker.announce();
            REQUIRE(announced.has_value());
            CHECK(*announced == math::vec2(2.0f));
            CHECK_FALSE(tracker.announce().has_value());
            tracker.record(sizes_of(math::uvec2(1920, 1080), math::uvec2(960, 540)));
            CHECK_FALSE(tracker.announce().has_value());
        }
        SUBCASE("a minimised window keeps its scale and announces nothing") {
            content_scale_tracker tracker = {};
            tracker.start(retina);
            tracker.record(sizes_of(math::uvec2(0), math::uvec2(0)));
            CHECK(tracker.scale() == math::vec2(2.0f));
            CHECK_FALSE(tracker.announce().has_value());
            tracker.record(standard);
            const std::optional<math::vec2> announced = tracker.announce();
            REQUIRE(announced.has_value());
            CHECK(*announced == math::vec2(1.0f));
        }
        SUBCASE("a window with no size at creation starts at 1") {
            content_scale_tracker tracker = {};
            tracker.start(scaled_sizes{});
            CHECK(tracker.scale() == math::vec2(1.0f));
            tracker.record(standard);
            CHECK_FALSE(tracker.announce().has_value());
            tracker.record(retina);
            const std::optional<math::vec2> announced = tracker.announce();
            REQUIRE(announced.has_value());
            CHECK(*announced == math::vec2(2.0f));
        }
    }
}
