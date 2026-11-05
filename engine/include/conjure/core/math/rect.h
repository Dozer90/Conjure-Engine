#pragma once

#include "conjure/core/math/glm_types.h"
#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <format>
#include <limits>
#include <string>
#include <string_view>

namespace conjure::math {

    /// <summary>
    /// A float rectangle given by its min and max corners (y may point either way; the struct
    /// only assumes min <= max). The default rectangle is empty at the origin. Containment and
    /// intersection are inclusive: touching counts.
    /// </summary>
    struct rect {
        vec2 min = vec2(0.0f);
        vec2 max = vec2(0.0f);

        /// <summary>
        /// True when min <= max on both axes (an empty rectangle is valid).
        /// </summary>
        [[nodiscard]] bool is_valid() const noexcept {
            return min.x <= max.x && min.y <= max.y;
        }

        /// <summary>
        /// True when the rectangle has no area (invalid or zero width or height).
        /// </summary>
        [[nodiscard]] bool is_empty() const noexcept {
            return !is_valid() || min.x == max.x || min.y == max.y;
        }

        /// <summary>
        /// max.x - min.x.
        /// </summary>
        [[nodiscard]] f32 width() const noexcept {
            return max.x - min.x;
        }

        /// <summary>
        /// max.y - min.y.
        /// </summary>
        [[nodiscard]] f32 height() const noexcept {
            return max.y - min.y;
        }

        /// <summary>
        /// max - min.
        /// </summary>
        [[nodiscard]] vec2 size() const noexcept {
            return max - min;
        }

        /// <summary>
        /// Midpoint of min and max.
        /// </summary>
        [[nodiscard]] vec2 center() const noexcept {
            return (min + max) * 0.5f;
        }

        /// <summary>
        /// width * height; 0 for an invalid rectangle.
        /// </summary>
        [[nodiscard]] f32 area() const noexcept {
            if (!is_valid()) {
                return 0.0f;
            }
            return width() * height();
        }

        /// <summary>
        /// True when the point lies inside or on the rectangle.
        /// </summary>
        [[nodiscard]] bool contains(vec2 point) const noexcept {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
        }

        /// <summary>
        /// True when other lies entirely inside or on this rectangle (false when either is invalid).
        /// </summary>
        [[nodiscard]] bool contains(const rect &other) const noexcept {
            return other.is_valid() && contains(other.min) && contains(other.max);
        }

        /// <summary>
        /// True when the rectangles overlap or touch (false when either is invalid).
        /// </summary>
        [[nodiscard]] bool intersects(const rect &other) const noexcept {
            if (!is_valid() || !other.is_valid()) {
                return false;
            }
            return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y;
        }

        /// <summary>
        /// The overlap of both rectangles; invalid when they do not intersect.
        /// </summary>
        [[nodiscard]] rect intersection(const rect &other) const noexcept {
            rect result = {};
            result.min = glm::max(min, other.min);
            result.max = glm::min(max, other.max);
            return result;
        }

        /// <summary>
        /// Grows the rectangle to include a point.
        /// </summary>
        void expand(vec2 point) noexcept {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        /// <summary>
        /// Grows the rectangle to include another (an invalid other is ignored).
        /// </summary>
        void expand(const rect &other) noexcept {
            if (!other.is_valid()) {
                return;
            }
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        /// <summary>
        /// A copy moved by offset.
        /// </summary>
        [[nodiscard]] rect translated(vec2 offset) const noexcept {
            rect result = *this;
            result.min += offset;
            result.max += offset;
            return result;
        }

        /// <summary>
        /// A copy grown by amount on every side (shrunk for negative amounts).
        /// </summary>
        [[nodiscard]] rect inflated(f32 amount) const noexcept {
            rect result = *this;
            result.min -= vec2(amount);
            result.max += vec2(amount);
            return result;
        }

        /// <summary>
        /// The point inside or on the rectangle nearest to point.
        /// </summary>
        [[nodiscard]] vec2 closest_point(vec2 point) const noexcept {
            return glm::clamp(point, min, max);
        }

        /// <summary>
        /// A rectangle from its min corner and size (negative sizes are made positive).
        /// </summary>
        [[nodiscard]] static rect from_position_size(vec2 position, vec2 extent) noexcept {
            return from_points(position, position + extent);
        }

        /// <summary>
        /// A rectangle from its centre and half-size on each axis (negative extents are made
        /// positive).
        /// </summary>
        [[nodiscard]] static rect from_center_extents(vec2 centerPoint, vec2 halfExtents) noexcept {
            const vec2 halfSize = glm::abs(halfExtents);
            rect result = {};
            result.min = centerPoint - halfSize;
            result.max = centerPoint + halfSize;
            return result;
        }

        /// <summary>
        /// A rectangle from two arbitrary corners.
        /// </summary>
        [[nodiscard]] static rect from_points(vec2 a, vec2 b) noexcept {
            rect result = {};
            result.min = glm::min(a, b);
            result.max = glm::max(a, b);
            return result;
        }

        [[nodiscard]] bool operator==(const rect &other) const = default;
    };

    /// <summary>
    /// True when both corners are within epsilon.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const rect &a, const rect &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.min, b.min, epsilon) && nearly_equal(a.max, b.max, epsilon);
    }

    /// <summary>
    /// An unsigned pixel rectangle (viewports, scissors, texture regions): top-left origin plus
    /// size. Pixel (x, y) is covered when x < right() and y < bottom(), so the right and bottom
    /// edges are exclusive. Invariant: x + width and y + height must not exceed 0xFFFFFFFF (the
    /// edges must be representable); right() and bottom() assert when they would wrap.
    /// </summary>
    struct urect {
        u32 x = 0;
        u32 y = 0;
        u32 width = 0;
        u32 height = 0;

        /// <summary>
        /// One past the last covered column. Asserts when x + width overflows u32.
        /// </summary>
        [[nodiscard]] u32 right() const noexcept {
            CONJURE_ASSERT(
              width <= std::numeric_limits<u32>::max() - x,
              "urect::right: x ({}) + width ({}) overflows u32",
              x,
              width
            );
            return x + width;
        }

        /// <summary>
        /// One past the last covered row. Asserts when y + height overflows u32.
        /// </summary>
        [[nodiscard]] u32 bottom() const noexcept {
            CONJURE_ASSERT(
              height <= std::numeric_limits<u32>::max() - y,
              "urect::bottom: y ({}) + height ({}) overflows u32",
              y,
              height
            );
            return y + height;
        }

        /// <summary>
        /// True when the rectangle covers no pixel.
        /// </summary>
        [[nodiscard]] bool is_empty() const noexcept {
            return width == 0 || height == 0;
        }

        /// <summary>
        /// Number of covered pixels.
        /// </summary>
        [[nodiscard]] u64 area() const noexcept {
            return static_cast<u64>(width) * static_cast<u64>(height);
        }

        /// <summary>
        /// True when the pixel at (pixelX, pixelY) is covered.
        /// </summary>
        [[nodiscard]] bool contains(u32 pixelX, u32 pixelY) const noexcept {
            return pixelX >= x && pixelX < right() && pixelY >= y && pixelY < bottom();
        }

        /// <summary>
        /// True when every pixel of other is covered by this rectangle (an empty other is contained
        /// when its origin is inside or on the edge).
        /// </summary>
        [[nodiscard]] bool contains(const urect &other) const noexcept {
            return other.x >= x && other.y >= y && other.right() <= right() && other.bottom() <= bottom();
        }

        /// <summary>
        /// True when both rectangles cover at least one common pixel.
        /// </summary>
        [[nodiscard]] bool intersects(const urect &other) const noexcept {
            if (is_empty() || other.is_empty()) {
                return false;
            }
            return x < other.right() && other.x < right() && y < other.bottom() && other.y < bottom();
        }

        /// <summary>
        /// The pixels covered by both rectangles; empty (zero size at the clamped origin) when they
        /// do not intersect.
        /// </summary>
        [[nodiscard]] urect intersection(const urect &other) const noexcept {
            urect result = {};
            result.x = std::max(x, other.x);
            result.y = std::max(y, other.y);
            const u32 rightEdge = std::min(right(), other.right());
            const u32 bottomEdge = std::min(bottom(), other.bottom());
            result.width = rightEdge > result.x ? rightEdge - result.x : 0;
            result.height = bottomEdge > result.y ? bottomEdge - result.y : 0;
            return result;
        }

        /// <summary>
        /// The same region as a float rectangle (min = origin, max = origin + size).
        /// </summary>
        [[nodiscard]] rect to_rect() const noexcept {
            rect result = {};
            result.min = vec2(static_cast<f32>(x), static_cast<f32>(y));
            result.max = vec2(static_cast<f32>(right()), static_cast<f32>(bottom()));
            return result;
        }

        [[nodiscard]] bool operator==(const urect &other) const = default;
    };
}

template <>
struct std::formatter<conjure::math::rect> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::rect &value, FormatContext &context) const {
        const float minimum[2] = {value.min.x, value.min.y};
        const float maximum[2] = {value.max.x, value.max.y};

        typename FormatContext::iterator out = std::format_to(context.out(), "{{min=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, minimum, context);
        out = std::format_to(out, ", max=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, maximum, context);
        *out++ = '}';
        return out;
    }
};

template <>
struct std::formatter<conjure::math::urect> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const conjure::math::urect &value, FormatContext &context) const {
        const std::string text =
          std::format("{{x={}, y={}, width={}, height={}}}", value.x, value.y, value.width, value.height);
        return std::formatter<std::string_view>::format(text, context);
    }
};
