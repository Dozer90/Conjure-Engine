#include "conjure/testing/doctest.h"

#include "conjure/editor/ui/editor_icon.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#include <memory>
#include <vector>

// editor_icon.h: the official logo as the editor's window icon, decoded from the PNGs generated into editor_ui.

using namespace conjure;
using namespace conjure::editor::ui;

namespace {
    constexpr core::rgba8 MIDNIGHT_ACCENT = {0x4C, 0x8D, 0xFF, 255};

    // Background pixels (alpha below half) that cannot be reached from the border through background: the eye.
    [[nodiscard]] usize enclosed_background_pixels(const core::image &icon) {
        const u32 width = icon.width();
        const u32 height = icon.height();
        std::vector<bool> reached(static_cast<usize>(width) * height, false);
        std::vector<usize> pending = {};
        const auto isBackground = [&](u32 x, u32 y) {
            return icon.pixel(x, y).a < 128;
        };
        const auto visit = [&](u32 x, u32 y) {
            const usize index = static_cast<usize>(y) * width + x;
            if (isBackground(x, y) && !reached[index]) {
                reached[index] = true;
                pending.push_back(index);
            }
        };
        for (u32 x = 0; x < width; ++x) {
            visit(x, 0);
            visit(x, height - 1);
        }
        for (u32 y = 0; y < height; ++y) {
            visit(0, y);
            visit(width - 1, y);
        }
        while (!pending.empty()) {
            const usize index = pending.back();
            pending.pop_back();
            const u32 x = static_cast<u32>(index % width);
            const u32 y = static_cast<u32>(index / width);
            if (x + 1 < width) {
                visit(x + 1, y);
            }
            if (x > 0) {
                visit(x - 1, y);
            }
            if (y + 1 < height) {
                visit(x, y + 1);
            }
            if (y > 0) {
                visit(x, y - 1);
            }
        }
        usize enclosed = 0;
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                if (isBackground(x, y) && !reached[static_cast<usize>(y) * width + x]) {
                    ++enclosed;
                }
            }
        }
        return enclosed;
    }
}

TEST_SUITE("editor/ui/editor_icon") {

    TEST_CASE("editor_icon_images - decodes square images from 16 to 256 px, smallest first") {
        const core::result<std::vector<core::image>> images = editor_icon_images();
        CONJURE_REQUIRE_OK(images);
        REQUIRE(images.value().size() >= 3);
        CHECK(images.value().front().width() == 16);
        CHECK(images.value().back().width() == 256);
        u32 previous = 0;
        for (const core::image &icon : images.value()) {
            CAPTURE(icon.width());
            CHECK(icon.width() == icon.height());
            CHECK(icon.width() > previous);
            previous = icon.width();
        }
        CONJURE_CHECK_OK(platform::validate_window_icon(images.value()));
    }

    TEST_CASE("editor_icon_images - every size is the logo in the Midnight accent with its eye open") {
        const core::result<std::vector<core::image>> images = editor_icon_images();
        CONJURE_REQUIRE_OK(images);
        for (const core::image &icon : images.value()) {
            CAPTURE(icon.width());
            usize inked = 0;
            usize clear = 0;
            bool oneInk = true;
            for (u32 y = 0; y < icon.height(); ++y) {
                for (u32 x = 0; x < icon.width(); ++x) {
                    const core::rgba8 pixel = icon.pixel(x, y);
                    if (pixel.a == 0) {
                        ++clear;
                        continue;
                    }
                    ++inked;
                    oneInk = oneInk && pixel.r == MIDNIGHT_ACCENT.r && pixel.g == MIDNIGHT_ACCENT.g
                      && pixel.b == MIDNIGHT_ACCENT.b;
                }
            }
            CHECK(oneInk);
            const usize total = static_cast<usize>(icon.width()) * icon.height();
            CHECK(clear * 3 > total);                       // a mark on a transparent canvas, not a plate
            CHECK(inked * 5 > total);
            // The eye is snapped to whole pixels in every small cut, so it stays an enclosed hole down to 16 px.
            CHECK(enclosed_background_pixels(icon) >= 4);
        }
    }

    TEST_CASE("apply_editor_icon - hands every decoded image to the window") {
        platform::window_desc desc = {};
        desc.backend = platform::backend_kind::NULL_BACKEND;
        desc.width = 640;
        desc.height = 360;
        const std::unique_ptr<platform::null_backend> window = platform::null_backend::create(desc);
        REQUIRE(window != nullptr);
        CONJURE_REQUIRE_OK(apply_editor_icon(*window));
        const core::result<std::vector<core::image>> images = editor_icon_images();
        CONJURE_REQUIRE_OK(images);
        REQUIRE(window->icons().size() == images.value().size());
        for (usize index = 0; index < images.value().size(); ++index) {
            CHECK(window->icons()[index] == images.value()[index]);
        }
    }
}
