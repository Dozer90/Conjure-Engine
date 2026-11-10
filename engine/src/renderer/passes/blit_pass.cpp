#include "renderer/passes/passes.h"

// Blit (docs/design/02_renderer.md 3.3): the transient LDR into the view's target when the target's
// format differs from the LDR format; not added when the tonemap wrote the target directly.

using namespace conjure;
using namespace conjure::renderer;

blit_pass::blit_pass(pass_shared &shared) :
    mShared(shared) {}

blit_pass::~blit_pass() {}

const char *blit_pass::name() const noexcept {
    return "blit";
}

void blit_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.ldr);
    builder.colour_target(mShared.graph.target, 0);
}

void blit_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr || mPipeline == nullptr) {
        return;
    }
    nvrhi::IBindingSet *set = mShared.fullscreen_set(
      context.texture(mShared.graph.ldr),
      nullptr,
      nullptr,
      mShared.pointClampSampler,
      nullptr
    );
    if (set == nullptr) {
        return;
    }
    nvrhi::GraphicsState state = {};
    state.pipeline = mPipeline;
    state.framebuffer = framebuffer;
    state.viewport = mShared.viewport_state();
    state.addBindingSet(set);
    context.command_list().setGraphicsState(state);
    gpu::draw_constants constants = {};
    context.command_list().setPushConstants(&constants, sizeof(constants));
    nvrhi::DrawArguments arguments = {};
    arguments.vertexCount = 3;
    context.command_list().draw(arguments);
    ++context.stats().drawCalls;
    ++context.stats().trianglesDrawn;
}
