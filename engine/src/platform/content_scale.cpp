#include "platform/content_scale.h"

#include "conjure/core/types.h"

// The framebuffer / window ratio and the rule that decides when its change is announced
// (docs/design/11_platform.md P9).

using namespace conjure;
using namespace conjure::platform;

namespace {
    bool has_area(const detail::scaled_sizes &sizes) noexcept {
        return sizes.framebuffer.x > 0 && sizes.framebuffer.y > 0 && sizes.window.x > 0 && sizes.window.y > 0;
    }

    // The range of scales [framebuffer / window, (framebuffer + 1) / window) of one pair and of the other do
    // not overlap. Cross-multiplied in u64, where every product of two u32 values plus one is exact, so no
    // rounding of a ratio can make two equal scales look apart.
    bool scale_ranges_apart(u32 announcedFramebuffer, u32 announcedWindow, u32 framebuffer, u32 window) noexcept {
        const u64 announcedEndTimesWindow = (static_cast<u64>(announcedFramebuffer) + 1) * window;
        const u64 framebufferTimesAnnouncedWindow = static_cast<u64>(framebuffer) * announcedWindow;
        const u64 endTimesAnnouncedWindow = (static_cast<u64>(framebuffer) + 1) * announcedWindow;
        const u64 announcedFramebufferTimesWindow = static_cast<u64>(announcedFramebuffer) * window;
        return announcedEndTimesWindow <= framebufferTimesAnnouncedWindow
            || endTimesAnnouncedWindow <= announcedFramebufferTimesWindow;
    }
}

math::vec2 conjure::platform::detail::framebuffer_content_scale(
  const scaled_sizes &sizes,
  math::vec2 previous
) noexcept {
    if (!has_area(sizes)) {
        return previous;
    }
    // Divided in double: a u32 size above 2^24 has no exact f32, and the ratio is what the caller keeps.
    const f64 scaleX = static_cast<f64>(sizes.framebuffer.x) / static_cast<f64>(sizes.window.x);
    const f64 scaleY = static_cast<f64>(sizes.framebuffer.y) / static_cast<f64>(sizes.window.y);
    return math::vec2(static_cast<f32>(scaleX), static_cast<f32>(scaleY));
}

bool conjure::platform::detail::content_scale_outdated(
  const scaled_sizes &announced,
  const scaled_sizes &current
) noexcept {
    if (!has_area(current)) {
        return false;
    }
    if (!has_area(announced)) {
        return current.framebuffer != current.window;
    }
    return scale_ranges_apart(announced.framebuffer.x, announced.window.x, current.framebuffer.x, current.window.x)
        || scale_ranges_apart(announced.framebuffer.y, announced.window.y, current.framebuffer.y, current.window.y);
}

void conjure::platform::detail::content_scale_tracker::start(const scaled_sizes &sizes) noexcept {
    mScale = framebuffer_content_scale(sizes, math::vec2(1.0f));
    mRecorded = sizes;
    mAnnounced = sizes;
}

void conjure::platform::detail::content_scale_tracker::record(const scaled_sizes &sizes) noexcept {
    mScale = framebuffer_content_scale(sizes, mScale);
    mRecorded = sizes;
}

std::optional<math::vec2> conjure::platform::detail::content_scale_tracker::announce() noexcept {
    // Compared with the sizes of the last announcement, not with its ratio: a ratio taken from truncated
    // sizes is off by up to a pixel over its window, and a window several times larger multiplies that.
    if (!content_scale_outdated(mAnnounced, mRecorded)) {
        return std::nullopt;
    }
    mAnnounced = mRecorded;
    return mScale;
}

math::vec2 conjure::platform::detail::content_scale_tracker::scale() const noexcept {
    return mScale;
}
