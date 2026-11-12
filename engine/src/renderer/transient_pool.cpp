#include "renderer/transient_pool.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/log_category.h"

#include "rhi/texture_format_mapping.h"
#include "conjure/core/hash.h"

using namespace conjure;
using namespace conjure::renderer;

namespace {
    void hash_u64(u64 &hash, u64 value) noexcept {
        for (u32 byteIndex = 0; byteIndex < 8; ++byteIndex) {
            hash = core::hash::fnv1a64_step(hash, static_cast<u8>(value >> (byteIndex * 8)));
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Descriptions

u64 graph_texture_desc::hash() const noexcept {
    u64 value = core::hash::FNV1A64_OFFSET_BASIS;
    hash_u64(value, width);
    hash_u64(value, height);
    hash_u64(value, arraySize);
    hash_u64(value, mipLevels);
    hash_u64(value, static_cast<u64>(format));
    hash_u64(value, (isRenderTarget ? 1u : 0u) | (isUnorderedAccess ? 2u : 0u) | (isCubemap ? 4u : 0u));
    return value;
}

bool graph_texture_desc::same_storage(const graph_texture_desc &other) const noexcept {
    return width == other.width && height == other.height && arraySize == other.arraySize
      && mipLevels == other.mipLevels &&
      format == other.format && isRenderTarget == other.isRenderTarget
      && isUnorderedAccess == other.isUnorderedAccess &&
      isCubemap == other.isCubemap;
}

u64 graph_buffer_desc::hash() const noexcept {
    u64 value = core::hash::FNV1A64_OFFSET_BASIS;
    hash_u64(value, byteSize);
    hash_u64(value, structStride);
    hash_u64(value, isUnorderedAccess ? 1u : 0u);
    return value;
}

bool graph_buffer_desc::same_storage(const graph_buffer_desc &other) const noexcept {
    return byteSize == other.byteSize && structStride == other.structStride
      && isUnorderedAccess == other.isUnorderedAccess;
}

/*===========================================================================*/
/*===========================================================================*/
// transient_pool

transient_pool::transient_pool(nvrhi::IDevice &device, framebuffer_cache *framebuffers) :
    mDevice(device),
    mFramebuffers(framebuffers),
    mEvictionFrames(TRANSIENT_EVICTION_FRAMES) {}

transient_pool::~transient_pool() {}

nvrhi::TextureDesc transient_pool::to_nvrhi(const graph_texture_desc &desc) {
    nvrhi::TextureDesc textureDesc = {};
    textureDesc.width = desc.width;
    textureDesc.height = desc.height;
    textureDesc.arraySize = desc.arraySize;
    textureDesc.mipLevels = desc.mipLevels;
    textureDesc.format = rhi::to_nvrhi_format(desc.format);
    textureDesc.debugName = desc.debugName != nullptr ? desc.debugName : "transient";
    textureDesc.isShaderResource = true;
    textureDesc.isRenderTarget = desc.isRenderTarget;
    textureDesc.isUAV = desc.isUnorderedAccess;
    if (desc.isCubemap) {
        textureDesc.dimension = desc.arraySize > 6 ? nvrhi::TextureDimension::TextureCubeArray
          : nvrhi::TextureDimension::TextureCube;
    } else {
        textureDesc.dimension = desc.arraySize > 1 ? nvrhi::TextureDimension::Texture2DArray
          : nvrhi::TextureDimension::Texture2D;
    }
    // Tracking starts from a known state without beginTrackingTextureState (design 2.7).
    textureDesc.initialState = desc.isUnorderedAccess && !desc.isRenderTarget ? nvrhi::ResourceStates::UnorderedAccess
                                                                             : nvrhi::ResourceStates::ShaderResource;
    textureDesc.keepInitialState = true;
    if (desc.isRenderTarget) {
        textureDesc.useClearValue = true;
        textureDesc.clearValue = nvrhi::Color(0.0f);
    }
    return textureDesc;
}

nvrhi::BufferDesc transient_pool::to_nvrhi(const graph_buffer_desc &desc) {
    nvrhi::BufferDesc bufferDesc = {};
    bufferDesc.byteSize = desc.byteSize;
    bufferDesc.structStride = desc.structStride;
    bufferDesc.canHaveUAVs = desc.isUnorderedAccess;
    bufferDesc.debugName = desc.debugName != nullptr ? desc.debugName : "transient";
    bufferDesc.initialState = desc.isUnorderedAccess ? nvrhi::ResourceStates::UnorderedAccess
      : nvrhi::ResourceStates::ShaderResource;
    bufferDesc.keepInitialState = true;
    return bufferDesc;
}

nvrhi::ITexture *transient_pool::acquire(const graph_texture_desc &desc, u64 frameIndex) {
    std::vector<pooled_texture> &bucket = mTextures[desc.hash()];
    for (pooled_texture &entry : bucket) {
        // A 64-bit hash may collide: an entry of another description is never handed out.
        if (entry.inUse || !entry.desc.same_storage(desc)) {
            continue;
        }
        entry.inUse = true;
        entry.inUseFrame = frameIndex;
        entry.lastUsedFrame = frameIndex;
        return entry.texture.Get();
    }

    nvrhi::TextureHandle created = mDevice.createTexture(to_nvrhi(desc));
    if (created == nullptr) {
        CONJURE_LOG_ERROR(
          LOG_RENDERER,
          "transient_pool: the device refused texture '{}' ({}x{})",
          desc.debugName,
          desc.width,
          desc.height
        );
        return nullptr;
    }
    ++mTexturesCreated;
    pooled_texture entry = {};
    entry.texture = created;
    entry.desc = desc;
    entry.inUse = true;
    entry.inUseFrame = frameIndex;
    entry.lastUsedFrame = frameIndex;
    bucket.push_back(std::move(entry));
    return bucket.back().texture.Get();
}

nvrhi::IBuffer *transient_pool::acquire_buffer(const graph_buffer_desc &desc, u64 frameIndex) {
    std::vector<pooled_buffer> &bucket = mBuffers[desc.hash()];
    for (pooled_buffer &entry : bucket) {
        if (entry.inUse || !entry.desc.same_storage(desc)) {
            continue;
        }
        entry.inUse = true;
        entry.lastUsedFrame = frameIndex;
        return entry.buffer.Get();
    }
    nvrhi::BufferHandle created = mDevice.createBuffer(to_nvrhi(desc));
    if (created == nullptr) {
        CONJURE_LOG_ERROR(
          LOG_RENDERER,
          "transient_pool: the device refused buffer '{}' ({} bytes)",
          desc.debugName,
          desc.byteSize
        );
        return nullptr;
    }
    ++mBuffersCreated;
    pooled_buffer entry = {};
    entry.buffer = created;
    entry.desc = desc;
    entry.inUse = true;
    entry.lastUsedFrame = frameIndex;
    bucket.push_back(std::move(entry));
    return bucket.back().buffer.Get();
}

void transient_pool::end_frame(u64 frameIndex) {
    // A bucket left empty is erased, so descriptions no longer used are not walked every frame.
    for (auto bucket = mTextures.begin(); bucket != mTextures.end();) {
        std::vector<pooled_texture> &entries = bucket->second;
        for (auto iterator = entries.begin(); iterator != entries.end();) {
            iterator->inUse = false;
            const bool stale = frameIndex >= iterator->lastUsedFrame
              && frameIndex - iterator->lastUsedFrame > mEvictionFrames;
            if (stale) {
                if (mFramebuffers != nullptr) {
                    mFramebuffers->remove_texture(iterator->texture.Get());
                }
                iterator = entries.erase(iterator);
            } else {
                ++iterator;
            }
        }
        if (entries.empty()) {
            bucket = mTextures.erase(bucket);
        } else {
            ++bucket;
        }
    }
    for (auto bucket = mBuffers.begin(); bucket != mBuffers.end();) {
        std::vector<pooled_buffer> &entries = bucket->second;
        for (auto iterator = entries.begin(); iterator != entries.end();) {
            iterator->inUse = false;
            const bool stale = frameIndex >= iterator->lastUsedFrame
              && frameIndex - iterator->lastUsedFrame > mEvictionFrames;
            if (stale) {
                iterator = entries.erase(iterator);
            } else {
                ++iterator;
            }
        }
        if (entries.empty()) {
            bucket = mBuffers.erase(bucket);
        } else {
            ++bucket;
        }
    }
}

void transient_pool::clear() {
    if (mFramebuffers != nullptr) {
        for (auto &[key, bucket] : mTextures) {
            for (pooled_texture &entry : bucket) {
                mFramebuffers->remove_texture(entry.texture.Get());
            }
        }
    }
    mTextures.clear();
    mBuffers.clear();
}

u32 transient_pool::live_texture_count() const noexcept {
    u32 count = 0;
    for (const auto &[key, bucket] : mTextures) {
        count += static_cast<u32>(bucket.size());
    }
    return count;
}
