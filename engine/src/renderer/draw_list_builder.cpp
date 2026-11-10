#include "renderer/draw_list_builder.h"

#include "conjure/core/assert.h"
#include "conjure/core/radix_sort.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr u64 MASK_22 = (u64{1} << 22) - 1;
    constexpr u64 MASK_24 = (u64{1} << 24) - 1;

    /// <summary>
    /// Blended draws come after every opaque / masked draw in the merged list.
    /// </summary>
    [[nodiscard]] constexpr u64 variant_prefix(draw_variant variant) noexcept {
        return static_cast<u64>(variant) << 56;
    }
}

draw_list_builder::draw_list_builder() {}

draw_list_builder::~draw_list_builder() {}

u64 draw_list_builder::opaque_key(
  draw_variant variant,
  u32 materialIndex,
  u32 meshIndex,
  u32 submesh,
  f32 depthBucket
) noexcept {
    const u64 depth = static_cast<u64>(math::clamp(depthBucket, 0.0f, 255.0f));
    return variant_prefix(variant) | ((static_cast<u64>(materialIndex) & MASK_22) << 34) |
      ((static_cast<u64>(meshIndex) & MASK_22) << 12) | ((static_cast<u64>(submesh) & 0xF) << 8) | depth;
}

u64 draw_list_builder::blend_key(u8 layer, f32 viewDepth, f32 farPlane, u32 materialIndex) noexcept {
    // Far first: invert the normalised depth so the largest distance sorts lowest.
    const f32 normalised = farPlane > 0.0f ? math::saturate(viewDepth / farPlane) : 0.0f;
    const u64 inverseDepth = static_cast<u64>((1.0f - normalised) * static_cast<f32>(MASK_24 - 1));
    return variant_prefix(draw_variant::BLENDED) | (static_cast<u64>(layer) << 48) | (inverseDepth << 24) |
      (static_cast<u64>(materialIndex) & MASK_24);
}

void draw_list_builder::clear() noexcept {
    mItems.clear();
    mSorted.clear();
    mBatches.clear();
}

void draw_list_builder::add(const draw_item &item, f32 viewDepth, f32 farPlane) {
    CONJURE_ASSERT(
      item.materialIndex <= MAX_DRAW_KEY_INDEX && item.meshIndex <= MAX_DRAW_KEY_INDEX,
      "draw_list_builder: material / mesh index beyond the 22-bit key fields"
    );
    CONJURE_ASSERT(item.submesh < MAX_DRAW_KEY_SUBMESH, "draw_list_builder: submesh index beyond the 4-bit key field");
    draw_item record = item;
    if (item.variant == draw_variant::BLENDED) {
        record.sortKey = blend_key(item.layer, viewDepth, farPlane, item.materialIndex);
    } else {
        const f32 bucket = farPlane > 0.0f ? math::saturate(viewDepth / farPlane) * 255.0f : 0.0f;
        record.sortKey = opaque_key(item.variant, item.materialIndex, item.meshIndex, item.submesh, bucket);
    }
    mItems.push_back(record);
}

void draw_list_builder::build() {
    mSorted.clear();
    mBatches.clear();
    const usize count = mItems.size();
    if (count == 0) {
        return;
    }
    mKeys.resize(count);
    mOrder.resize(count);
    mScratchKeys.resize(count);
    mScratchValues.resize(count);
    for (usize index = 0; index < count; ++index) {
        mKeys[index] = mItems[index].sortKey;
        mOrder[index] = static_cast<u32>(index);
    }
    core::radix_sort_u64(mKeys, mOrder, mScratchKeys, mScratchValues);

    mSorted.reserve(count);
    for (usize index = 0; index < count; ++index) {
        mSorted.push_back(mItems[mOrder[index]]);
    }

    for (usize index = 0; index < count; ++index) {
        const draw_item &item = mSorted[index];
        const bool mergeable = item.variant != draw_variant::BLENDED && !mBatches.empty() &&
          mBatches.back().variant == item.variant && mBatches.back().doubleSided == item.doubleSided &&
          mBatches.back().meshIndex == item.meshIndex && mBatches.back().materialIndex == item.materialIndex &&
          mBatches.back().submesh == item.submesh;
        if (mergeable) {
            ++mBatches.back().instanceCount;
            continue;
        }
        draw_batch batch = {};
        batch.variant = item.variant;
        batch.doubleSided = item.doubleSided;
        batch.meshIndex = item.meshIndex;
        batch.materialIndex = item.materialIndex;
        batch.submesh = item.submesh;
        batch.firstItem = static_cast<u32>(index);
        batch.instanceCount = 1;
        mBatches.push_back(batch);
    }
}
