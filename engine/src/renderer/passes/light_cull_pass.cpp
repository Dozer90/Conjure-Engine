#include "renderer/passes/passes.h"

#include "conjure/renderer/limits.h"

// Tiled light culling (docs/design/02_renderer.md R1, 3.3): one 16x16 group per screen tile reads
// the depth buffer and the view's lights and writes the persistent tile list buffer.

using namespace conjure;
using namespace conjure::renderer;

light_cull_pass::light_cull_pass(pass_shared &shared) :
    mShared(shared) {}

light_cull_pass::~light_cull_pass() {}

const char *light_cull_pass::name() const noexcept {
    return "light_cull";
}

void light_cull_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.depth);
    builder.read(mShared.graph.lights);
    builder.write(mShared.graph.tileLists);
}

void light_cull_pass::execute(pass_context &context) {
    const view_resources &resourcesOfView = mShared.frame->view(mShared.view->viewIndex);
    nvrhi::BindingSetDesc desc = {};
    desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mShared.frame->view_constants()));
    desc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, context.texture(mShared.graph.depth)));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(1, resourcesOfView.lights));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_UAV(0, resourcesOfView.tileLists));
    nvrhi::IBindingSet *set = mShared.compute_set(mShared.layouts->lightCull, desc);
    if (set == nullptr) {
        return;
    }
    nvrhi::ComputeState state = {};
    state.pipeline = mShared.pipelines->compute(compute_variant::LIGHT_CULL);
    state.addBindingSet(set);
    context.command_list().setComputeState(state);
    gpu::draw_constants constants = {};
    context.command_list().setPushConstants(&constants, sizeof(constants));
    const math::uvec4 &counts = mShared.view->constants.counts;
    context.command_list().dispatch(counts.x, counts.y, 1);
    ++context.stats().dispatches;
}
