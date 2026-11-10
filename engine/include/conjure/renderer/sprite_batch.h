#pragma once

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/limits.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <type_traits>
#include <vector>

// The 2D sprite batcher (docs/design/02_renderer.md 3.9): pure CPU, public so tests build batches
// by hand. WORLD sprites become world-space quads drawn into the HDR target after blended meshes;
// SCREEN sprites become pixel-space quads (origin top-left, y down) drawn after tonemapping.

namespace conjure::renderer {

    enum class sprite_space : u8 {
        WORLD = 0,
        SCREEN
    };

    /// <summary>
    /// One sprite as the extraction contract carries it (render_scene::sprites).
    /// </summary>
    struct sprite {
        gpu_texture_id texture = {};
        math::mat4 worldFromLocal = math::mat4(1.0f);   // SCREEN: pixel-space transform, y down
        math::vec4 uvRect = math::vec4(0.0f, 0.0f, 1.0f, 1.0f);   // min u, min v, max u, max v
        math::vec4 colour = math::vec4(1.0f);           // linear, premultiplied on the GPU
        math::vec2 size = math::vec2(1.0f);             // world units or pixels
        sprite_space space = sprite_space::WORLD;
        i16 layer = 0;
        i16 order = 0;

        [[nodiscard]] bool operator==(const sprite &other) const noexcept = default;
    };

    /// <summary>
    /// One quad vertex as the GPU consumes it (gpu_sprite_vertex, 32 bytes).
    /// </summary>
    struct sprite_vertex {
        math::vec3 position = math::vec3(0.0f);
        math::vec2 uv = math::vec2(0.0f);
        u32 colour = 0xFFFFFFFFu;                       // RGBA8, red in the low byte
        u32 pad0 = 0;
        u32 pad1 = 0;

        [[nodiscard]] bool operator==(const sprite_vertex &other) const noexcept = default;
    };

    static_assert(sizeof(sprite_vertex) == 32, "sprite_vertex mirrors gpu_sprite_vertex");
    static_assert(std::is_trivially_copyable_v<sprite_vertex>, "sprite_vertex is uploaded verbatim");

    /// <summary>
    /// A run of consecutive sprites sharing a space and a texture: one drawIndexed each.
    /// </summary>
    struct sprite_run {
        sprite_space space = sprite_space::WORLD;
        gpu_texture_id texture = {};
        u32 firstIndex = 0;
        u32 indexCount = 0;

        [[nodiscard]] bool operator==(const sprite_run &other) const noexcept = default;
    };

    /// <summary>
    /// Sorts sprites by (space, layer, order, texture), emits 4 vertices and 6 indices per sprite
    /// and splits the result into runs. The capacity is MAX_SPRITES_PER_FRAME; sprites beyond it are
    /// dropped (in sorted order, the last ones) and counted, with a WARN logged once per change of
    /// the dropped count.
    /// </summary>
    class sprite_batcher {
        private:
            std::vector<sprite_vertex> mVertices = {};
            std::vector<u32> mIndices = {};
            std::vector<sprite_run> mRuns = {};
            std::vector<u32> mOrder = {};               // scratch: sorted sprite indices
            std::vector<u64> mKeys = {};                // scratch: sort keys
            std::vector<u64> mScratchKeys = {};
            std::vector<u32> mScratchValues = {};
            u32 mDropped = 0;
            u32 mLastWarnedDropped = 0;

        public:
            sprite_batcher();
            ~sprite_batcher();

            sprite_batcher(const sprite_batcher &) = delete;
            sprite_batcher &operator=(const sprite_batcher &) = delete;

        public:
            /// <summary>
            /// Rebuilds vertices, indices and runs from the sprites. Keeps capacity between calls.
            /// </summary>
            void build(std::span<const sprite> sprites);

            [[nodiscard]] std::span<const sprite_vertex> vertices() const noexcept {
                return mVertices;
            }

            [[nodiscard]] std::span<const u32> indices() const noexcept {
                return mIndices;
            }

            [[nodiscard]] std::span<const sprite_run> runs() const noexcept {
                return mRuns;
            }

            /// <summary>
            /// Sprites dropped by the last build because the cap was reached.
            /// </summary>
            [[nodiscard]] u32 dropped_count() const noexcept {
                return mDropped;
            }

            /// <summary>
            /// Number of sprites the last build emitted.
            /// </summary>
            [[nodiscard]] u32 sprite_count() const noexcept {
                return static_cast<u32>(mVertices.size() / 4);
            }

            /// <summary>
            /// Forgets the output; capacity is kept.
            /// </summary>
            void clear() noexcept;

            /// <summary>
            /// The sort key of a sprite: space, then layer, order and texture, all ascending.
            /// </summary>
            [[nodiscard]] static u64 sort_key(const sprite &value) noexcept;
    };

    /// <summary>
    /// The enumerator name of a sprite space ("WORLD").
    /// </summary>
    [[nodiscard]] std::string_view to_string(sprite_space space) noexcept;
}
