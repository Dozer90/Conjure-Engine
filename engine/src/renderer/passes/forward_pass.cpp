#include "renderer/passes/passes.h"

#include <nvrhi/utils.h>

// Forward shading (docs/design/02_renderer.md R1, 3.3): the opaque pass creates the HDR target and
// draws the opaque and masked batches with an Equal depth test against the prepass; the blended
// pass draws the blended batches back to front with depth writes off.

using namespace conjure;
using namespace conjure::renderer;

forward_pass::forward_pass(pass_shared &shared, bool blended) :
    mShared(shared),
    mBlended(blended) {}

forward_pass::~forward_pass() {}

const char *forward_pass::name() const noexcept {
    return mBlended ? "forward_blend" : "forward_opaque";
}

void forward_pass::setup(pass_builder &builder) {
    if (mShared.graph.hdr == INVALID_GRAPH_RESOURCE) {
        graph_texture_desc hdr = {};
        hdr.width = mShared.view->targetSize.x;
        hdr.height = mShared.view->targetSize.y;
        hdr.format = rhi::texture_format::RGBA16_FLOAT;
        hdr.isRenderTarget = true;
        hdr.debugName = "hdr";
        mShared.graph.hdr = builder.create_texture(hdr);
    }
    builder.read(mShared.graph.instances);
    builder.read(mShared.graph.lights);
    builder.read(mShared.graph.shadowTiles);
    builder.read(mShared.graph.depth);
    if (mShared.view->localLightCount > 0) {
        builder.read(mShared.graph.tileLists);
    }
    if (mShared.view->shadows) {
        builder.read(mShared.graph.shadowCascades);
        builder.read(mShared.graph.shadowAtlas);
    }
    if (!mBlended && mShared.view->ssao && mShared.graph.aoBlurred != INVALID_GRAPH_RESOURCE) {
        builder.read(mShared.graph.aoBlurred);
        builder.read(mShared.graph.aoDepthHalf);
    }
    builder.colour_target(mShared.graph.hdr, 0);
    builder.depth_target(mShared.graph.depth);
}

void forward_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr) {
        return;
    }
    mShared.clear_targets_once(context, framebuffer, true, true);
    const bool useAo = !mBlended && mShared.view->ssao;
    nvrhi::ITexture *ao = useAo ? context.texture(mShared.graph.aoBlurred) : nullptr;
    nvrhi::ITexture *aoDepth = useAo ? context.texture(mShared.graph.aoDepthHalf) : nullptr;
    nvrhi::IBindingSet *viewSet = mShared.mesh_view_set(ao, aoDepth);
    const mesh_pass_kind kind = mBlended ? mesh_pass_kind::FORWARD_BLEND : mesh_pass_kind::FORWARD_OPAQUE;
    const u32 flags = mBlended ? gpu::DRAW_FLAG_LIGHT_LIST_B : 0u;
    draw_mesh_region(context, mShared, framebuffer, mShared.viewport_state(), viewSet, mShared.view->main, kind, flags);
}
