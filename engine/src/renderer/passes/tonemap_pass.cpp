#include "renderer/passes/passes.h"

// Tonemap (docs/design/02_renderer.md R11, 3.8): HDR (plus bloom and the exposure state) into the
// LDR target, which is the view's target itself when that is RGBA8_SRGB and a transient otherwise.

using namespace conjure;
using namespace conjure::renderer;

tonemap_pass::tonemap_pass(pass_shared &shared) :
    mShared(shared) {}

tonemap_pass::~tonemap_pass() {}

const char *tonemap_pass::name() const noexcept {
    return "tonemap";
}

void tonemap_pass::setup(pass_builder &builder) {
    if (mShared.graph.hdr == INVALID_GRAPH_RESOURCE) {
        // Nothing rendered into HDR: a cleared target still needs tonemapping to a defined colour.
        graph_texture_desc hdr = {};
        hdr.width = mShared.view->targetSize.x;
        hdr.height = mShared.view->targetSize.y;
        hdr.format = rhi::texture_format::RGBA16_FLOAT;
        hdr.isRenderTarget = true;
        hdr.debugName = "hdr";
        mShared.graph.hdr = builder.create_texture(hdr);
        builder.write(mShared.graph.hdr);
    }
    builder.read(mShared.graph.hdr);
    if (mShared.view->bloom && mShared.graph.bloomMips[0] != INVALID_GRAPH_RESOURCE) {
        builder.read(mShared.graph.bloomMips[0]);
    }
    if (mShared.view->autoExposure) {
        builder.read(mShared.graph.exposureState);
    }
    if (mShared.graph.ldrIsTarget) {
        mShared.graph.ldr = mShared.graph.target;
    } else {
        graph_texture_desc ldr = {};
        ldr.width = mShared.view->targetSize.x;
        ldr.height = mShared.view->targetSize.y;
        ldr.format = rhi::texture_format::RGBA8_SRGB;
        ldr.isRenderTarget = true;
        ldr.debugName = "ldr";
        mShared.graph.ldr = builder.create_texture(ldr);
    }
    builder.colour_target(mShared.graph.ldr, 0);
}

void tonemap_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr) {
        return;
    }
    nvrhi::ITexture *hdr = context.texture(mShared.graph.hdr);
    if (!mShared.view->hdrCleared && hdr != nullptr) {
        // No pass wrote the HDR target this frame: clear it so the tonemap reads black.
        context.command_list().clearTextureFloat(hdr, nvrhi::AllSubresources, nvrhi::Color(0.0f));
        mShared.view->hdrCleared = true;
    }
    nvrhi::ITexture *bloom = mShared.view->bloom ? context.texture(mShared.graph.bloomMips[0]) : nullptr;
    nvrhi::IBuffer *exposure = mShared.frame->view(mShared.view->viewIndex).exposureState;
    nvrhi::IBindingSet *set = mShared.fullscreen_set(
      hdr,
      bloom,
      exposure,
      mShared.linearClampSampler,
      mShared.pointClampSampler
    );
    if (set == nullptr) {
        return;
    }
    nvrhi::GraphicsState state = {};
    state.pipeline = mShared.pipelines->graphics(graphics_variant::TONEMAP);
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
