#include "renderer/passes/passes.h"

// Debug lines (docs/design/02_renderer.md 3.10): the depth-tested list then the overlay list, two
// draws from one vertex buffer uploaded per frame.

using namespace conjure;
using namespace conjure::renderer;

debug_draw_pass::debug_draw_pass(pass_shared &shared) :
    mShared(shared) {}

debug_draw_pass::~debug_draw_pass() {}

const char *debug_draw_pass::name() const noexcept {
    return "debug_draw";
}

void debug_draw_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.debugVertices);
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

void debug_draw_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr) {
        return;
    }
    mShared.clear_targets_once(context, framebuffer, true, true);
    nvrhi::IBindingSet *set = mShared.fullscreen_set(nullptr, nullptr, nullptr, nullptr, nullptr);
    if (set == nullptr) {
        return;
    }
    const u32 depthTested = mShared.view->debugDepthTestedVertices;
    const u32 overlay = mShared.view->debugOverlayVertices;
    const nvrhi::ViewportState viewport = mShared.viewport_state();
    const auto drawLines = [&](graphics_variant variant, u32 first, u32 count) {
        if (count < 2) {
            return;
        }
        nvrhi::GraphicsState state = {};
        state.pipeline = mShared.pipelines->graphics(variant);
        state.framebuffer = framebuffer;
        state.viewport = viewport;
        state.addBindingSet(set);
        state.addVertexBuffer(
          nvrhi::VertexBufferBinding().setBuffer(mShared.frame->debug_vertices()).setSlot(0).setOffset(0)
        );
        context.command_list().setGraphicsState(state);
        gpu::draw_constants constants = {};
        context.command_list().setPushConstants(&constants, sizeof(constants));
        nvrhi::DrawArguments arguments = {};
        arguments.vertexCount = count;
        arguments.startVertexLocation = first;
        context.command_list().draw(arguments);
        ++context.stats().drawCalls;
        context.stats().debugVerticesDrawn += count;
    };
    drawLines(graphics_variant::DEBUG_DEPTH_TESTED, 0, depthTested);
    drawLines(graphics_variant::DEBUG_OVERLAY, depthTested, overlay);
}
