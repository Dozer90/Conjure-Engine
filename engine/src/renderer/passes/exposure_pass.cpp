#include "renderer/passes/passes.h"

#include "conjure/renderer/limits.h"

// Auto-exposure (docs/design/02_renderer.md R11, 3.8): the histogram pass clears and fills the
// per-view histogram buffer from the HDR target, the adapt pass turns it into the persistent
// exposure state (one frame of latency accepted).

using namespace conjure;
using namespace conjure::renderer;

/*===========================================================================*/
/*===========================================================================*/
// exposure_histogram_pass

exposure_histogram_pass::exposure_histogram_pass(pass_shared &shared) :
    mShared(shared) {}

exposure_histogram_pass::~exposure_histogram_pass() {}

const char *exposure_histogram_pass::name() const noexcept {
    return "exposure_histogram";
}

void exposure_histogram_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.hdr);
    builder.write(mShared.graph.histogram);
}

void exposure_histogram_pass::execute(pass_context &context) {
    const view_resources &resourcesOfView = mShared.frame->view(mShared.view->viewIndex);
    context.command_list().clearBufferUInt(resourcesOfView.histogram, 0);
    nvrhi::BindingSetDesc desc = {};
    desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mShared.frame->view_constants()));
    desc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, context.texture(mShared.graph.hdr)));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_UAV(0, resourcesOfView.histogram));
    nvrhi::IBindingSet *set = mShared.compute_set(mShared.layouts->exposureHistogram, desc);
    if (set == nullptr) {
        return;
    }
    nvrhi::ComputeState state = {};
    state.pipeline = mShared.pipelines->compute(compute_variant::EXPOSURE_HISTOGRAM);
    state.addBindingSet(set);
    context.command_list().setComputeState(state);
    // Only the view's viewport is metered: pixels of the target outside it (cleared, or another
    // view's) must not bias the exposure. The rectangle travels in the push constants.
    const math::urect &viewport = mShared.view->viewport;
    gpu::draw_constants constants = {};
    constants.firstInstance = viewport.x;
    constants.flags = viewport.y;
    constants.param0 = gpu::bits_as_float(viewport.width);
    constants.param1 = gpu::bits_as_float(viewport.height);
    context.command_list().setPushConstants(&constants, sizeof(constants));
    context.command_list().dispatch((viewport.width + 15) / 16, (viewport.height + 15) / 16, 1);
    ++context.stats().dispatches;
}

/*===========================================================================*/
/*===========================================================================*/
// exposure_adapt_pass

exposure_adapt_pass::exposure_adapt_pass(pass_shared &shared) :
    mShared(shared) {}

exposure_adapt_pass::~exposure_adapt_pass() {}

const char *exposure_adapt_pass::name() const noexcept {
    return "exposure_adapt";
}

void exposure_adapt_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.histogram);
    builder.write(mShared.graph.exposureState);
}

void exposure_adapt_pass::execute(pass_context &context) {
    const view_resources &resourcesOfView = mShared.frame->view(mShared.view->viewIndex);
    nvrhi::BindingSetDesc desc = {};
    desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mShared.frame->view_constants()));
    desc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, resourcesOfView.histogram));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_UAV(0, resourcesOfView.exposureState));
    nvrhi::IBindingSet *set = mShared.compute_set(mShared.layouts->exposureAdapt, desc);
    if (set == nullptr) {
        return;
    }
    nvrhi::ComputeState state = {};
    state.pipeline = mShared.pipelines->compute(compute_variant::EXPOSURE_ADAPT);
    state.addBindingSet(set);
    context.command_list().setComputeState(state);
    gpu::draw_constants constants = {};
    context.command_list().setPushConstants(&constants, sizeof(constants));
    context.command_list().dispatch(1, 1, 1);
    ++context.stats().dispatches;
}
