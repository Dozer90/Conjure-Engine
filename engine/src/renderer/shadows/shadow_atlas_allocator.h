#pragma once

#include "conjure/renderer/light.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <vector>

// Quadtree tile allocator for the local-light shadow atlas (docs/design/02_renderer.md R9, 3.4):
// one 4096^2 (configurable) atlas with leaves of 1024, 512 and 256 texels. Every frame the
// shadow-casting lights are offered in importance order; a light keeps its tiles across frames while
// its request (size and tile count) is unchanged, a request that no longer fits is halved down to
// 256, and a light that still does not fit renders unshadowed. Point lights take six tiles.

namespace conjure::renderer {

    struct atlas_request {
        u64 lightKey = 0;               // stable identity of the light (atlas_light_key), never 0
        u16 size = 512;                 // requested tile edge, one of 256 / 512 / 1024
        u8 tileCount = 1;               // 1 for spot lights, 6 for point lights
    };

    /// <summary>
    /// The identity a light keeps across frames so its tile is reused: light::entityId when the
    /// extraction supplied one, otherwise a hash of the light's type, position (1 cm), direction and
    /// requested resolution. Never depends on the light's index in render_scene::lights.
    /// </summary>
    [[nodiscard]] u64 atlas_light_key(const light &value) noexcept;

    /// <summary>
    /// Distinguishes lights that share a key (identical lights without entity ids): the n-th
    /// occurrence of a key in one frame gets its own derived key, the first keeps the base key.
    /// </summary>
    [[nodiscard]] u64 atlas_light_key_occurrence(u64 baseKey, u32 occurrence) noexcept;

    struct atlas_tile {
        u32 x = 0;
        u32 y = 0;
        u32 size = 0;

        /// <summary>
        /// The tile's rectangle in [0, 1] atlas coordinates.
        /// </summary>
        [[nodiscard]] math::vec4 uv_rect(u32 atlasSize) const noexcept;

        [[nodiscard]] bool operator==(const atlas_tile &other) const noexcept = default;
    };

    struct atlas_allocation {
        u64 lightKey = 0;
        u32 firstTile = 0;              // index into shadow_atlas_allocator::tiles()
        u8 tileCount = 0;               // 0 when the light was dropped
        u16 grantedSize = 0;            // tile edge actually granted (halved when the atlas was full)
        u16 requestedSize = 0;          // the size asked for, as the match key of the next frame
    };

    class shadow_atlas_allocator {
        private:
            struct node {
                u32 x = 0;
                u32 y = 0;
                u32 size = 0;
                u32 firstChild = 0;     // 0 = no children (the root is never a child)
                bool occupied = false;  // this node is a leaf handed out
                bool split = false;
            };

            u32 mAtlasSize = 4096;
            u32 mMinTile = 256;
            u32 mMaxTile = 1024;
            std::vector<node> mNodes = {};
            std::vector<atlas_tile> mTiles = {};
            std::vector<atlas_allocation> mAllocations = {};
            std::vector<atlas_tile> mPreviousTiles = {};
            std::vector<atlas_allocation> mPreviousAllocations = {};
            u32 mDropped = 0;
            u32 mReused = 0;

        public:
            explicit shadow_atlas_allocator(u32 atlasSize = 4096);
            ~shadow_atlas_allocator();

            shadow_atlas_allocator(const shadow_atlas_allocator &) = delete;
            shadow_atlas_allocator &operator=(const shadow_atlas_allocator &) = delete;

        public:
            /// <summary>
            /// Allocates for one frame: requests are taken in order (importance). A light whose request
            /// equals last frame's keeps its tiles; the others get the largest size that still fits,
            /// halving down to the minimum; the rest are dropped (tileCount 0).
            /// </summary>
            void allocate(std::span<const atlas_request> requests);

            /// <summary>
            /// The result of the last allocate, one entry per request in request order.
            /// </summary>
            [[nodiscard]] std::span<const atlas_allocation> allocations() const noexcept {
                return mAllocations;
            }

            /// <summary>
            /// Every tile handed out by the last allocate; allocations index into it.
            /// </summary>
            [[nodiscard]] std::span<const atlas_tile> tiles() const noexcept {
                return mTiles;
            }

            [[nodiscard]] u32 dropped_count() const noexcept {
                return mDropped;
            }

            /// <summary>
            /// Tiles the last allocate kept at the previous frame's coordinates.
            /// </summary>
            [[nodiscard]] u32 reused_count() const noexcept {
                return mReused;
            }

            [[nodiscard]] u32 atlas_size() const noexcept {
                return mAtlasSize;
            }

            [[nodiscard]] u32 min_tile_size() const noexcept {
                return mMinTile;
            }

            [[nodiscard]] u32 max_tile_size() const noexcept {
                return mMaxTile;
            }

            /// <summary>
            /// Forgets every allocation (also the previous frame's) so the next allocate starts empty.
            /// </summary>
            void reset();

        private:
            void rebuild_tree();
            void split_node(u32 nodeIndex);
            [[nodiscard]] bool allocate_node(u32 nodeIndex, u32 size, atlas_tile &out);
            [[nodiscard]] bool claim_exact(u32 nodeIndex, const atlas_tile &tile);
            [[nodiscard]] bool allocate_tiles(u32 size, u8 tileCount);
    };
}
