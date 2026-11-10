#include "renderer/passes/passes.h"

#include <algorithm>

// GTAO at half resolution and its separable depth-aware blur (docs/design/02_renderer.md R10, 3.7).

using namespace conjure;
using namespace conjure::renderer;

namespace {
    [[nodiscard]] math::uvec2 half_size(const math::uvec2 &size) noexcept {
        return math::uvec2(std::max<u32>(size.x / 2, 1), std::max<u32>(size.y / 2, 1));
    }

    [[nodiscard]] graph_texture_desc ao_desc(const math::uvec2 &size, const char *name) noexcept {
        graph_texture_desc desc = {};
        desc.width = size.x;
        desc.height = size.y;
        desc.format = rhi::texture_format::R8_UNORM;
        desc.isUnorderedAccess = true;
        desc.debugName = name;
        return desc;
    }

    [[nodiscard]] graph_texture_desc ao_depth_desc(const math::uvec2 &size) noexcept {
        graph_texture_desc desc = {};
        desc.width = size.x;
        desc.height = size.y;
        desc.format = rhi::texture_format::R16_FLOAT;
        desc.isUnorderedAccess = true;
        desc.debugName = "ao_depth_half";
        return desc;
    }

    /// <summary>
    /// One SSAO dispatch; 'depthOutput' is the GTAO pass's second UAV (nullptr for the blur).
    /// </summary>
    void dispatch_ssao(
      pass_context &context,
      const pass_shared &shared,
      compute_variant variant,
      nvrhi::ITexture *texture0,
      nvrhi::ITexture *texture1,
      nvrhi::ITexture *output,
      nvrhi::ITexture *depthOutput,
      u32 flags
    ) {
        nvrhi::BindingSetDesc desc = {};
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, shared.frame->view_constants()));
        desc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture0));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, texture1));
        desc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, output));
        desc.addItem(nvrhi::BindingSetItem::Sampler(0, shared.pointClampSampler));
        if (depthOutput != nullptr) {
            desc.addItem(nvrhi::BindingSetItem::Texture_UAV(1, depthOutput));
        }
        nvrhi::IBindingLayout *layout = depthOutput != nullptr ? shared.layouts->ssaoGtao : shared.layouts->ssao;
        nvrhi::IBindingSet *set = shared.compute_set(layout, desc);
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
        const math::uvec2 half = half_size(shared.view->targetSize);
        context.command_list().dispatch((half.x + 7) / 8, (half.y + 7) / 8, 1);
        ++context.stats().dispatches;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// ssao_gtao_pass

ssao_gtao_pass::ssao_gtao_pass(pass_shared &shared) :
    mShared(shared) {}

ssao_gtao_pass::~ssao_gtao_pass() {}

const char *ssao_gtao_pass::name() const noexcept {
    return "ssao_gtao";
}

void ssao_gtao_pass::setup(pass_builder &builder) {
    const math::uvec2 half = half_size(mShared.view->targetSize);
    mShared.graph.aoHalf = builder.create_texture(ao_desc(half, "ao_half"));
    mShared.graph.aoDepthHalf = builder.create_texture(ao_depth_desc(half));
    builder.read(mShared.graph.depth);
    builder.read(mShared.graph.viewNormals);
    builder.write(mShared.graph.aoHalf);
    builder.write(mShared.graph.aoDepthHalf);
}

void ssao_gtao_pass::execute(pass_context &context) {
    dispatch_ssao(
      context,
      mShared,
      compute_variant::SSAO_GTAO,
      context.texture(mShared.graph.depth),
      context.texture(mShared.graph.viewNormals),
      context.texture(mShared.graph.aoHalf),
      context.texture(mShared.graph.aoDepthHalf),
      0
    );
}

/*===========================================================================*/
/*===========================================================================*/
// ssao_blur_pass

ssao_blur_pass::ssao_blur_pass(pass_shared &shared, bool vertical) :
    mShared(shared),
    mVertical(vertical) {}

ssao_blur_pass::~ssao_blur_pass() {}

const char *ssao_blur_pass::name() const noexcept {
    return mVertical ? "ssao_blur_v" : "ssao_blur_h";
}

void ssao_blur_pass::setup(pass_builder &builder) {
    const math::uvec2 half = half_size(mShared.view->targetSize);
    if (mVertical) {
        mShared.graph.aoBlurred = builder.create_texture(ao_desc(half, "ao_blurred"));
        builder.read(mShared.graph.aoBlurredH);
        builder.read(mShared.graph.depth);
        builder.write(mShared.graph.aoBlurred);
    } else {
        mShared.graph.aoBlurredH = builder.create_texture(ao_desc(half, "ao_blurred_h"));
        builder.read(mShared.graph.aoHalf);
        builder.read(mShared.graph.depth);
        builder.write(mShared.graph.aoBlurredH);
    }
}

void ssao_blur_pass::execute(pass_context &context) {
    nvrhi::ITexture *input = context.texture(mVertical ? mShared.graph.aoBlurredH : mShared.graph.aoHalf);
    nvrhi::ITexture *output = context.texture(mVertical ? mShared.graph.aoBlurred : mShared.graph.aoBlurredH);
    dispatch_ssao(
      context,
      mShared,
      compute_variant::SSAO_BLUR,
      input,
      context.texture(mShared.graph.depth),
      output,
      nullptr,
      mVertical ? 1u : 0u
    );
}
