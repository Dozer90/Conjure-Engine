#include "conjure/renderer/sprite_batch.h"
#include "conjure/renderer/debug_draw.h"
#include "conjure/renderer/log_category.h"

#include "conjure/core/radix_sort.h"

#include <algorithm>
#include <array>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    /// <summary>
    /// Biases a signed 16-bit value into [0, 65535] so it sorts ascending as an unsigned key.
    /// </summary>
    [[nodiscard]] constexpr u64 bias_i16(i16 value) noexcept {
        return static_cast<u64>(static_cast<i32>(value) + 32768);
    }

    /// <summary>
    /// The corners of a unit quad for one space: local offsets (in half sizes) and uv rect corners.
    /// WORLD quads have +y up; SCREEN quads live in pixel space with y down. Both are ordered
    /// top-left, top-right, bottom-right, bottom-left in texture space.
    /// </summary>
    struct quad_corner {
        f32 x = 0.0f;
        f32 y = 0.0f;
        bool maxU = false;
        bool maxV = false;
    };

    constexpr std::array<quad_corner, 4> WORLD_CORNERS = {{
      {-1.0f, 1.0f, false, false}, {1.0f, 1.0f, true, false}, {1.0f, -1.0f, true, true}, {-1.0f, -1.0f, false, true},
    }};

    constexpr std::array<quad_corner, 4> SCREEN_CORNERS = {{
      {-1.0f, -1.0f, false, false}, {1.0f, -1.0f, true, false}, {1.0f, 1.0f, true, true}, {-1.0f, 1.0f, false, true},
    }};
}

sprite_batcher::sprite_batcher() {}

sprite_batcher::~sprite_batcher() {}

u64 sprite_batcher::sort_key(const sprite &value) noexcept {
    // [space:1][layer:16][order:16][texture index:31], every field ascending.
    const u64 space = value.space == sprite_space::WORLD ? 0 : 1;
    return (space << 63) | (bias_i16(value.layer) << 47) | (bias_i16(value.order) << 31) |
      (static_cast<u64>(value.texture.index) & 0x7FFFFFFFull);
}

void sprite_batcher::build(std::span<const sprite> sprites) {
    clear();

    // SCREEN sprites (the HUD) take their slots first so UI never vanishes under a burst of world
    // sprites: the overflow drops the WORLD sprites last in sort order, the front-most ones. The counts
    // use sort_key's own split (WORLD, then every other value), so they partition the sorted order exactly.
    const usize total = sprites.size();
    const usize worldCount = static_cast<usize>(std::ranges::count(sprites, sprite_space::WORLD, &sprite::space));
    const usize screenCount = total - worldCount;
    const usize emittedScreen = std::min<usize>(screenCount, MAX_SPRITES_PER_FRAME);
    const usize emittedWorld = std::min<usize>(worldCount, MAX_SPRITES_PER_FRAME - emittedScreen);
    const usize emitted = emittedWorld + emittedScreen;
    mDropped = static_cast<u32>(total - emitted);
    if (mDropped != mLastWarnedDropped) {
        if (mDropped > 0) {
            CONJURE_LOG_WARN(
              LOG_RENDERER,
              "sprite_batcher: {} sprites exceed MAX_SPRITES_PER_FRAME ({}); {} WORLD and {} SCREEN sprites, the last "
              "of each space in sort order, are dropped",
              total,
              MAX_SPRITES_PER_FRAME,
              worldCount - emittedWorld,
              screenCount - emittedScreen
            );
        }
        mLastWarnedDropped = mDropped;
    }
    if (total == 0) {
        return;
    }

    mKeys.resize(total);
    mOrder.resize(total);
    mScratchKeys.resize(total);
    mScratchValues.resize(total);
    for (usize index = 0; index < total; ++index) {
        mKeys[index] = sort_key(sprites[index]);
        mOrder[index] = static_cast<u32>(index);
    }
    core::radix_sort_u64(mKeys, mOrder, mScratchKeys, mScratchValues);

    mVertices.reserve(emitted * 4);
    mIndices.reserve(emitted * 6);

    // The sorted order is every WORLD sprite, then every SCREEN sprite: emit the first emittedWorld
    // of the former and the first emittedScreen of the latter.
    for (usize slot = 0; slot < emitted; ++slot) {
        const usize sorted = slot < emittedWorld ? slot : worldCount + (slot - emittedWorld);
        const sprite &value = sprites[mOrder[sorted]];
        const u32 baseVertex = static_cast<u32>(mVertices.size());
        const u32 firstIndex = static_cast<u32>(mIndices.size());

        const bool newRun = mRuns.empty() || mRuns.back().space != value.space || mRuns.back().texture != value.texture;
        if (newRun) {
            sprite_run run = {};
            run.space = value.space;
            run.texture = value.texture;
            run.firstIndex = firstIndex;
            run.indexCount = 0;
            mRuns.push_back(run);
        }

        const std::array<quad_corner, 4> &corners = value.space == sprite_space::WORLD ? WORLD_CORNERS : SCREEN_CORNERS;
        const math::vec2 halfSize = value.size * 0.5f;
        const u32 packedColour = pack_debug_colour(value.colour);
        for (const quad_corner &corner : corners) {
            const math::vec4 local = math::vec4(corner.x * halfSize.x, corner.y * halfSize.y, 0.0f, 1.0f);
            const math::vec4 world = value.worldFromLocal * local;
            sprite_vertex vertex = {};
            vertex.position = math::vec3(world);
            vertex.uv = math::vec2(
              corner.maxU ? value.uvRect.z : value.uvRect.x,
              corner.maxV ? value.uvRect.w : value.uvRect.y
            );
            vertex.colour = packedColour;
            mVertices.push_back(vertex);
        }

        mIndices.push_back(baseVertex + 0);
        mIndices.push_back(baseVertex + 1);
        mIndices.push_back(baseVertex + 2);
        mIndices.push_back(baseVertex + 0);
        mIndices.push_back(baseVertex + 2);
        mIndices.push_back(baseVertex + 3);
        mRuns.back().indexCount += 6;
    }
}

void sprite_batcher::clear() noexcept {
    mVertices.clear();
    mIndices.clear();
    mRuns.clear();
    mDropped = 0;
}

std::string_view conjure::renderer::to_string(sprite_space space) noexcept {
    switch (space) {
        case sprite_space::WORLD: {
            return "WORLD";
        }
        case sprite_space::SCREEN: {
            return "SCREEN";
        }
    }
    return "WORLD";
}
