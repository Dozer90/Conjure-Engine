#include "renderer/passes/passes.h"

#include <nvrhi/utils.h>

#include <cstdio>

// Shadow map passes (docs/design/02_renderer.md 3.3, 3.4): one pass per cascade slice (depth only
// into the array texture) and one atlas pass drawing every allocated tile with its own viewport and
// scissor. Both write imported persistent textures, so the graph never culls them.

using namespace conjure;
using namespace conjure::renderer;

/*===========================================================================*/
/*===========================================================================*/
// shadow_cascade_pass

shadow_cascade_pass::shadow_cascade_pass(pass_shared &shared, u32 cascade) :
    mShared(shared),
    mCascade(cascade) {
    std::snprintf(mName.data(), mName.size(), "shadow_cascade_%u", cascade);
}

shadow_cascade_pass::~shadow_cascade_pass() {}

const char *shadow_cascade_pass::name() const noexcept {
    return mName.data();
}

void shadow_cascade_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.instances);
    builder.read(mShared.graph.shadowTiles);
    builder.depth_target(mShared.graph.shadowCascades, mCascade);
}

void shadow_cascade_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr) {
        return;
    }
    nvrhi::utils::ClearDepthStencilAttachment(&context.command_list(), framebuffer, 0.0f, 0);
    const shadow_draw_list &list = mShared.view->cascades[mCascade];
    const f32 resolution = static_cast<f32>(mShared.frame->cascade_resolution());
    nvrhi::ViewportState viewport = {};
    viewport.addViewportAndScissorRect(nvrhi::Viewport(resolution, resolution));
    nvrhi::IBindingSet *viewSet = mShared.mesh_view_set(nullptr);
    draw_mesh_region(
      context,
      mShared,
      framebuffer,
      viewport,
      viewSet,
      list.region,
      mesh_pass_kind::SHADOW,
      list.tileIndex
    );
    ++context.stats().shadowCascadesRendered;
}

/*===========================================================================*/
/*===========================================================================*/
// shadow_atlas_pass

shadow_atlas_pass::shadow_atlas_pass(pass_shared &shared) :
    mShared(shared) {}

shadow_atlas_pass::~shadow_atlas_pass() {}

const char *shadow_atlas_pass::name() const noexcept {
    return "shadow_atlas";
}

void shadow_atlas_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.instances);
    builder.read(mShared.graph.shadowTiles);
    builder.depth_target(mShared.graph.shadowAtlas);
}

void shadow_atlas_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr) {
        return;
    }
    nvrhi::utils::ClearDepthStencilAttachment(&context.command_list(), framebuffer, 0.0f, 0);
    nvrhi::IBindingSet *viewSet = mShared.mesh_view_set(nullptr);
    for (const shadow_draw_list &list : mShared.view->atlasTiles) {
        nvrhi::ViewportState viewport = {};
        viewport.addViewportAndScissorRect(nvrhi::Viewport(
          static_cast<f32>(list.atlasX),
          static_cast<f32>(list.atlasX + list.atlasSize),
          static_cast<f32>(list.atlasY),
          static_cast<f32>(list.atlasY + list.atlasSize),
          0.0f,
          1.0f
        ));
        draw_mesh_region(
          context,
          mShared,
          framebuffer,
          viewport,
          viewSet,
          list.region,
          mesh_pass_kind::SHADOW,
          list.tileIndex
        );
    }
}
