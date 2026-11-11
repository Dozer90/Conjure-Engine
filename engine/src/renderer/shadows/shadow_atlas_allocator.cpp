#include "renderer/shadows/shadow_atlas_allocator.h"

#include "conjure/renderer/light.h"

#include "conjure/core/hash.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr u64 ENTITY_KEY_FLAG = u64{1} << 63;

    void hash_value(u64 &hash, const void *data, usize size) noexcept {
        hash = core::hash::fnv1a64(std::span<const std::byte>(static_cast<const std::byte *>(data), size), hash);
    }

    [[nodiscard]] i32 quantise(f32 value, f32 unitsPerMetre) noexcept {
        return static_cast<i32>(std::lround(value * unitsPerMetre));
    }
}

u64 conjure::renderer::atlas_light_key(const light &value) noexcept {
    if (value.entityId != 0) {
        return ENTITY_KEY_FLAG | value.entityId;
    }
    // Attributes only: 1 cm positions and 1e-3 directions so float noise does not re-key a light.
    u64 hash = core::hash::FNV1A64_OFFSET_BASIS;
    const u8 type = static_cast<u8>(value.type);
    hash_value(hash, &type, sizeof(type));
    const i32 position[3] = {quantise(value.position.x, 100.0f), quantise(value.position.y, 100.0f),
                             quantise(value.position.z, 100.0f)};
    hash_value(hash, position, sizeof(position));
    const i32 direction[3] = {quantise(value.direction.x, 1000.0f), quantise(value.direction.y, 1000.0f),
                              quantise(value.direction.z, 1000.0f)};
    hash_value(hash, direction, sizeof(direction));
    const u16 resolution = clamp_shadow_resolution(value.shadowResolution);
    hash_value(hash, &resolution, sizeof(resolution));
    return (hash & ~ENTITY_KEY_FLAG) | 1u;
}

u64 conjure::renderer::atlas_light_key_occurrence(u64 baseKey, u32 occurrence) noexcept {
    if (occurrence == 0) {
        return baseKey;
    }
    u64 hash = baseKey;
    hash_value(hash, &occurrence, sizeof(occurrence));
    return (hash & ~ENTITY_KEY_FLAG) | 1u;
}

math::vec4 atlas_tile::uv_rect(u32 atlasSize) const noexcept {
    const f32 inverse = 1.0f / static_cast<f32>(std::max<u32>(atlasSize, 1));
    return math::vec4(
      static_cast<f32>(x) * inverse,
      static_cast<f32>(y) * inverse,
      static_cast<f32>(x + size) * inverse,
      static_cast<f32>(y + size) * inverse
    );
}

shadow_atlas_allocator::shadow_atlas_allocator(u32 atlasSize) :
    mAtlasSize(std::max<u32>(atlasSize, MIN_SHADOW_TILE_SIZE)),
    mMinTile(MIN_SHADOW_TILE_SIZE),
    mMaxTile(std::min<u32>(MAX_SHADOW_TILE_SIZE, mAtlasSize)) {
    rebuild_tree();
}

shadow_atlas_allocator::~shadow_atlas_allocator() {}

void shadow_atlas_allocator::rebuild_tree() {
    mNodes.clear();
    node root = {};
    root.size = mAtlasSize;
    mNodes.push_back(root);
}

void shadow_atlas_allocator::reset() {
    rebuild_tree();
    mTiles.clear();
    mAllocations.clear();
    mPreviousTiles.clear();
    mPreviousAllocations.clear();
    mDropped = 0;
    mReused = 0;
}

void shadow_atlas_allocator::split_node(u32 nodeIndex) {
    const u32 childSize = mNodes[nodeIndex].size / 2;
    const u32 x = mNodes[nodeIndex].x;
    const u32 y = mNodes[nodeIndex].y;
    const u32 firstChild = static_cast<u32>(mNodes.size());
    mNodes[nodeIndex].split = true;
    mNodes[nodeIndex].firstChild = firstChild;
    for (u32 child = 0; child < 4; ++child) {
        node childNode = {};
        childNode.x = x + (child & 1u) * childSize;
        childNode.y = y + (child >> 1u) * childSize;
        childNode.size = childSize;
        mNodes.push_back(childNode);
    }
}

bool shadow_atlas_allocator::allocate_node(u32 nodeIndex, u32 size, atlas_tile &out) {
    if (mNodes[nodeIndex].occupied || mNodes[nodeIndex].size < size) {
        return false;
    }
    if (mNodes[nodeIndex].size == size) {
        if (mNodes[nodeIndex].split) {
            return false;
        }
        mNodes[nodeIndex].occupied = true;
        out.x = mNodes[nodeIndex].x;
        out.y = mNodes[nodeIndex].y;
        out.size = mNodes[nodeIndex].size;
        return true;
    }
    if (!mNodes[nodeIndex].split) {
        if (mNodes[nodeIndex].size / 2 < mMinTile) {
            return false;
        }
        split_node(nodeIndex);
    }
    const u32 firstChild = mNodes[nodeIndex].firstChild;
    for (u32 child = 0; child < 4; ++child) {
        if (allocate_node(firstChild + child, size, out)) {
            return true;
        }
    }
    return false;
}

bool shadow_atlas_allocator::claim_exact(u32 nodeIndex, const atlas_tile &tile) {
    const node &current = mNodes[nodeIndex];
    if (current.occupied) {
        return false;
    }
    const bool inside = tile.x >= current.x && tile.y >= current.y && tile.x + tile.size <= current.x + current.size &&
      tile.y + tile.size <= current.y + current.size;
    if (!inside) {
        return false;
    }
    if (current.size == tile.size) {
        if (current.split) {
            return false;
        }
        mNodes[nodeIndex].occupied = true;
        return true;
    }
    if (!current.split) {
        if (current.size / 2 < mMinTile) {
            return false;
        }
        split_node(nodeIndex);
    }
    const u32 firstChild = mNodes[nodeIndex].firstChild;
    for (u32 child = 0; child < 4; ++child) {
        if (claim_exact(firstChild + child, tile)) {
            return true;
        }
    }
    return false;
}

bool shadow_atlas_allocator::allocate_tiles(u32 size, u8 tileCount) {
    // All or nothing: the tree is restored when one of the tiles does not fit. The backup is a
    // member so the copy reuses its capacity frame after frame (vector assignment never shrinks).
    mNodesBackup = mNodes;
    const usize firstTile = mTiles.size();
    for (u8 index = 0; index < tileCount; ++index) {
        atlas_tile tile = {};
        if (!allocate_node(0, size, tile)) {
            mNodes = mNodesBackup;
            mTiles.resize(firstTile);
            return false;
        }
        mTiles.push_back(tile);
    }
    return true;
}

void shadow_atlas_allocator::allocate(std::span<const atlas_request> requests) {
    mPreviousTiles = mTiles;
    mPreviousAllocations = mAllocations;
    rebuild_tree();
    mTiles.clear();
    mAllocations.assign(requests.size(), atlas_allocation());
    mDropped = 0;
    mReused = 0;

    // Pass 1: lights whose request is unchanged keep their tiles (stable map coordinates).
    std::vector<bool> &settled = mSettled;
    settled.assign(requests.size(), false);
    for (usize index = 0; index < requests.size(); ++index) {
        const atlas_request &request = requests[index];
        const auto unchanged = [&request](const atlas_allocation &entry) {
            return entry.lightKey == request.lightKey && entry.tileCount == request.tileCount &&
              entry.requestedSize == request.size && entry.tileCount > 0;
        };
        const auto previous = std::find_if(mPreviousAllocations.begin(), mPreviousAllocations.end(), unchanged);
        if (previous == mPreviousAllocations.end()) {
            continue;
        }
        mNodesBackup = mNodes;
        const usize firstTile = mTiles.size();
        bool kept = true;
        for (u8 tile = 0; tile < previous->tileCount; ++tile) {
            const atlas_tile &old = mPreviousTiles[previous->firstTile + tile];
            if (!claim_exact(0, old)) {
                kept = false;
                break;
            }
            mTiles.push_back(old);
        }
        if (!kept) {
            mNodes = mNodesBackup;
            mTiles.resize(firstTile);
            continue;
        }
        atlas_allocation &allocation = mAllocations[index];
        allocation.lightKey = request.lightKey;
        allocation.firstTile = static_cast<u32>(firstTile);
        allocation.tileCount = request.tileCount;
        allocation.grantedSize = previous->grantedSize;
        allocation.requestedSize = request.size;
        settled[index] = true;
        mReused += request.tileCount;
    }

    // Pass 2: everyone else, in importance order, largest size that fits.
    for (usize index = 0; index < requests.size(); ++index) {
        if (settled[index]) {
            continue;
        }
        const atlas_request &request = requests[index];
        atlas_allocation &allocation = mAllocations[index];
        allocation.lightKey = request.lightKey;
        allocation.tileCount = 0;
        allocation.grantedSize = 0;
        allocation.requestedSize = request.size;
        if (request.tileCount == 0) {
            continue;
        }
        u32 size = std::clamp<u32>(clamp_shadow_resolution(request.size), mMinTile, mMaxTile);
        bool granted = false;
        while (size >= mMinTile) {
            const usize firstTile = mTiles.size();
            if (allocate_tiles(size, request.tileCount)) {
                allocation.firstTile = static_cast<u32>(firstTile);
                allocation.tileCount = request.tileCount;
                allocation.grantedSize = static_cast<u16>(size);
                granted = true;
                break;
            }
            size /= 2;
        }
        if (!granted) {
            ++mDropped;
        }
    }
}
