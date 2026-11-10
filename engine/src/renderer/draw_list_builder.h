#pragma once

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/material_asset.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <vector>

// Draw lists (docs/design/02_renderer.md 2.8). One draw item per (instance, submesh); opaque and
// masked items sort ascending by [variant:8][material:22][mesh:22][submesh:4][depth:8] and runs with
// equal (variant, material, mesh, submesh) collapse into one instanced draw; blended items sort by
// [variant:8][layer:8][inverse depth:24][material:24] and are never instanced across different
// depths. The material and mesh fields are 22 bits (the design's 24) so the submesh fits the key
// (documented deviation, engine/src/renderer/README.md); add() asserts the ranges so an overflow can
// never silently merge different meshes into one batch. The builder is pure CPU: the renderer feeds
// it resolved ids and reads back batches.

namespace conjure::renderer {

    /// <summary>
    /// Largest material and mesh index the opaque key holds (22 bits each) and the submesh bound.
    /// </summary>
    inline constexpr u32 MAX_DRAW_KEY_INDEX = (1u << 22) - 1;
    inline constexpr u32 MAX_DRAW_KEY_SUBMESH = 16;

    /// <summary>
    /// Which pipeline a draw uses; the high bits of the opaque sort key.
    /// </summary>
    enum class draw_variant : u8 {
        OPAQUE_SURFACE = 0,
        MASKED,
        BLENDED
    };

    struct draw_item {
        u64 sortKey = 0;
        u32 instanceIndex = 0;          // index into the caller's instance array
        u32 submesh = 0;
        u32 materialSlot = 0;
        u32 meshIndex = 0;              // the resolved mesh key (dense index)
        u32 materialIndex = 0;          // the resolved material key (dense index)
        draw_variant variant = draw_variant::OPAQUE_SURFACE;
        bool doubleSided = false;
        u8 layer = 0;
    };

    /// <summary>
    /// One drawIndexed: instanceCount consecutive records starting at firstInstance of the GPU
    /// instance buffer, drawn with one mesh submesh and one material.
    /// </summary>
    struct draw_batch {
        draw_variant variant = draw_variant::OPAQUE_SURFACE;
        bool doubleSided = false;
        u32 meshIndex = 0;
        u32 materialIndex = 0;
        u32 submesh = 0;
        u32 firstItem = 0;              // index of the first item of the run in the sorted list
        u32 instanceCount = 0;
    };

    class draw_list_builder {
        private:
            std::vector<draw_item> mItems = {};
            std::vector<u64> mKeys = {};
            std::vector<u32> mOrder = {};
            std::vector<u64> mScratchKeys = {};
            std::vector<u32> mScratchValues = {};
            std::vector<draw_item> mSorted = {};
            std::vector<draw_batch> mBatches = {};

        public:
            draw_list_builder();
            ~draw_list_builder();

            draw_list_builder(const draw_list_builder &) = delete;
            draw_list_builder &operator=(const draw_list_builder &) = delete;

        public:
            /// <summary>
            /// Starts a new list; capacity is kept.
            /// </summary>
            void clear() noexcept;

            /// <summary>
            /// Adds one item; the sort key is computed here from the fields and the view depth. Asserts
            /// when materialIndex or meshIndex exceed MAX_DRAW_KEY_INDEX or submesh reaches
            /// MAX_DRAW_KEY_SUBMESH (the fields would alias in the key).
            /// </summary>
            void add(const draw_item &item, f32 viewDepth, f32 farPlane);

            /// <summary>
            /// Sorts and batches. Opaque / masked runs collapse; blended items get one batch each.
            /// </summary>
            void build();

            [[nodiscard]] std::span<const draw_item> items() const noexcept {
                return mSorted;
            }

            [[nodiscard]] std::span<const draw_batch> batches() const noexcept {
                return mBatches;
            }

            [[nodiscard]] usize item_count() const noexcept {
                return mItems.size();
            }

            /// <summary>
            /// The opaque / masked key: variant, material, mesh, submesh, depth bucket.
            /// </summary>
            [[nodiscard]] static u64 opaque_key(
              draw_variant variant,
              u32 materialIndex,
              u32 meshIndex,
              u32 submesh,
              f32 depthBucket
            ) noexcept;

            /// <summary>
            /// The blended key: layer, inverse depth (far first), material.
            /// </summary>
            [[nodiscard]] static u64 blend_key(u8 layer, f32 viewDepth, f32 farPlane, u32 materialIndex) noexcept;
    };
}
