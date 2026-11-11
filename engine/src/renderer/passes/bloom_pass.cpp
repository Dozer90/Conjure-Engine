#include "renderer/passes/passes.h"

#include "conjure/renderer/limits.h"

#include <algorithm>
#include <cstdio>

// Bloom (docs/design/02_renderer.md R11, 3.8): six downsampled mips starting at half resolution
// (13-tap, Karis average on the first) then an additive 3x3 tent upsample chain back to mip 0.

using namespace conjure;
using namespace conjure::renderer;

namespace {
    [[nodiscard]] math::uvec2 mip_size(const math::uvec2 &target, u32 mip) noexcept {
        const u32 shift = mip + 1;
        return math::uvec2(std::max<u32>(target.x >> shift, 1), std::max<u32>(target.y >> shift, 1));
    }

    void dispatch_bloom(
      pass_context &context,
      const pass_shared &shared,
      compute_variant variant,
      nvrhi::ITexture *source,
      nvrhi::ITexture *destination,
      const math::uvec2 &size,
      u32 flags
    ) {
        nvrhi::BindingSetDesc &desc = shared.binding_desc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, shared.frame->view_constants()));
        desc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, source));
        desc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, destination));
        desc.addItem(nvrhi::BindingSetItem::Sampler(0, shared.linearClampSampler));
        nvrhi::IBindingSet *set = shared.compute_set(shared.layouts->bloom, desc);
        if (set == nullptr) {
            return;
        }
        nvrhi::ComputeState state = {};
        state.pipeline = shared.pipelines->compute(variant);
        state.addBindingSet(set);
        context.command_list().setComputeState(state);
        gpu::draw_constants constants = {};
        constants.flags = flags;
        context.command_list().setPushConstants(&constants, sizeof(constants));
        context.command_list().dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
        ++context.stats().dispatches;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// bloom_down_pass

bloom_down_pass::bloom_down_pass(pass_shared &shared, u32 mip) :
    mShared(shared),
    mMip(mip) {
    std::snprintf(mName.data(), mName.size(), "bloom_down_%u", mip);
}

bloom_down_pass::~bloom_down_pass() {}

const char *bloom_down_pass::name() const noexcept {
    return mName.data();
}

void bloom_down_pass::setup(pass_builder &builder) {
    const math::uvec2 size = mip_size(mShared.view->targetSize, mMip);
    graph_texture_desc desc = {};
    desc.width = size.x;
    desc.height = size.y;
    desc.format = rhi::texture_format::RGBA16_FLOAT;
    desc.isUnorderedAccess = true;
    desc.debugName = "bloom_mip";
    mShared.graph.bloomMips[mMip] = builder.create_texture(desc);
    builder.read(mMip == 0 ? mShared.graph.hdr : mShared.graph.bloomMips[mMip - 1]);
    builder.write(mShared.graph.bloomMips[mMip]);
}

void bloom_down_pass::execute(pass_context &context) {
    nvrhi::ITexture *source = context.texture(mMip == 0 ? mShared.graph.hdr : mShared.graph.bloomMips[mMip - 1]);
    nvrhi::ITexture *destination = context.texture(mShared.graph.bloomMips[mMip]);
    dispatch_bloom(
      context,
      mShared,
      compute_variant::BLOOM_DOWN,
      source,
      destination,
      mip_size(mShared.view->targetSize, mMip),
      mMip == 0 ? gpu::DRAW_FLAG_KARIS_AVERAGE : 0u
    );
}

/*===========================================================================*/
/*===========================================================================*/
// bloom_up_pass

bloom_up_pass::bloom_up_pass(pass_shared &shared, u32 mip) :
    mShared(shared),
    mMip(mip) {
    std::snprintf(mName.data(), mName.size(), "bloom_up_%u", mip);
}

bloom_up_pass::~bloom_up_pass() {}

const char *bloom_up_pass::name() const noexcept {
    return mName.data();
}

void bloom_up_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.bloomMips[mMip + 1]);
    builder.read(mShared.graph.bloomMips[mMip]);
    builder.write(mShared.graph.bloomMips[mMip]);
}

void bloom_up_pass::execute(pass_context &context) {
    dispatch_bloom(
      context,
      mShared,
      compute_variant::BLOOM_UP,
      context.texture(mShared.graph.bloomMips[mMip + 1]),
      context.texture(mShared.graph.bloomMips[mMip]),
      mip_size(mShared.view->targetSize, mMip),
      0
    );
}
