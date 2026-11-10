#include "renderer/framebuffer_cache.h"

#include "conjure/core/hash.h"

#include <cstring>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    void hash_attachment(u64 &hash, const nvrhi::FramebufferAttachment &attachment) noexcept {
        const u64 pointer = reinterpret_cast<u64>(attachment.texture);
        const u64 fields[6] = {
          pointer,
          attachment.subresources.baseMipLevel,
          attachment.subresources.numMipLevels,
          attachment.subresources.baseArraySlice,
          attachment.subresources.numArraySlices,
          (static_cast<u64>(attachment.format) << 1) | (attachment.isReadOnly ? 1u : 0u),
        };
        for (const u64 field : fields) {
            for (u32 byteIndex = 0; byteIndex < 8; ++byteIndex) {
                hash = core::hash::fnv1a64_step(hash, static_cast<u8>(field >> (byteIndex * 8)));
            }
        }
    }

    [[nodiscard]] bool references(const nvrhi::FramebufferDesc &desc, const nvrhi::ITexture *texture) noexcept {
        for (const nvrhi::FramebufferAttachment &attachment : desc.colorAttachments) {
            if (attachment.texture == texture) {
                return true;
            }
        }
        return desc.depthAttachment.texture == texture || desc.shadingRateAttachment.texture == texture;
    }
}

framebuffer_cache::framebuffer_cache(nvrhi::IDevice &device) :
    mDevice(device) {}

framebuffer_cache::~framebuffer_cache() {}

u64 framebuffer_cache::hash(const nvrhi::FramebufferDesc &desc) noexcept {
    u64 value = core::hash::FNV1A64_OFFSET_BASIS;
    value = core::hash::fnv1a64_step(value, static_cast<u8>(desc.colorAttachments.size()));
    for (const nvrhi::FramebufferAttachment &attachment : desc.colorAttachments) {
        hash_attachment(value, attachment);
    }
    hash_attachment(value, desc.depthAttachment);
    hash_attachment(value, desc.shadingRateAttachment);
    return value;
}

nvrhi::IFramebuffer *framebuffer_cache::get(const nvrhi::FramebufferDesc &desc) {
    const u64 key = hash(desc);
    const auto found = mFramebuffers.find(key);
    if (found != mFramebuffers.end()) {
        ++mHits;
        return found->second.Get();
    }
    ++mMisses;
    nvrhi::FramebufferHandle framebuffer = mDevice.createFramebuffer(desc);
    if (framebuffer == nullptr) {
        return nullptr;
    }
    nvrhi::IFramebuffer *raw = framebuffer.Get();
    mFramebuffers.emplace(key, std::move(framebuffer));
    return raw;
}

void framebuffer_cache::remove_texture(nvrhi::ITexture *texture) {
    for (auto iterator = mFramebuffers.begin(); iterator != mFramebuffers.end();) {
        if (references(iterator->second->getDesc(), texture)) {
            iterator = mFramebuffers.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void framebuffer_cache::clear() {
    mFramebuffers.clear();
}
