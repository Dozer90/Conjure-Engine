#include "renderer/passes/passes.h"

// Skybox (docs/design/02_renderer.md 3.3): a fullscreen triangle at the far depth into the HDR
// target; added only when the view has an environment.

using namespace conjure;
using namespace conjure::renderer;

skybox_pass::skybox_pass(pass_shared &shared) :
    mShared(shared) {}

skybox_pass::~skybox_pass() {}

const char *skybox_pass::name() const noexcept {
    return "skybox";
}

void skybox_pass::setup(pass_builder &builder) {
    if (mShared.graph.hdr == INVALID_GRAPH_RESOURCE) {
        graph_texture_desc hdr = {};
        hdr.width = mShared.view->targetSize.x;
        hdr.height = mShared.view->targetSize.y;
        hdr.format = rhi::texture_format::RGBA16_FLOAT;
        hdr.isRenderTarget = true;
        hdr.debugName = "hdr";
        mShared.graph.hdr = builder.create_texture(hdr);
    }
    if (mShared.graph.depth == INVALID_GRAPH_RESOURCE) {
        graph_texture_desc depth = {};
        depth.width = mShared.view->targetSize.x;
        depth.height = mShared.view->targetSize.y;
        depth.format = rhi::texture_format::D32_FLOAT;
        depth.isRenderTarget = true;
        depth.debugName = "depth";
        mShared.graph.depth = builder.create_texture(depth);
    }
    builder.colour_target(mShared.graph.hdr, 0);
    builder.depth_target(mShared.graph.depth);
}

void skybox_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr || mShared.view->environment == nullptr) {
        return;
    }
    mShared.clear_targets_once(context, framebuffer, true, true);
    nvrhi::IBindingSet *set = mShared.fullscreen_set(
      mShared.view->environment->sky,
      nullptr,
      nullptr,
      mShared.linearClampSampler,
      nullptr
    );
    if (set == nullptr) {
        return;
    }
    nvrhi::GraphicsState state = {};
    state.pipeline = mShared.pipelines->graphics(graphics_variant::SKYBOX);
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
