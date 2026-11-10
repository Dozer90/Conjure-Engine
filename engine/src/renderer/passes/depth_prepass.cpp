#include "renderer/passes/passes.h"

#include <nvrhi/utils.h>

// Depth + normal prepass (docs/design/02_renderer.md R1, 3.3): creates the view's depth buffer and
// the octahedral view-space normals, both at the target size.

using namespace conjure;
using namespace conjure::renderer;

depth_prepass::depth_prepass(pass_shared &shared) :
    mShared(shared) {}

depth_prepass::~depth_prepass() {}

const char *depth_prepass::name() const noexcept {
    return "depth_prepass";
}

void depth_prepass::setup(pass_builder &builder) {
    const math::uvec2 size = mShared.view->targetSize;
    graph_texture_desc depth = {};
    depth.width = size.x;
    depth.height = size.y;
    depth.format = rhi::texture_format::D32_FLOAT;
    depth.isRenderTarget = true;
    depth.debugName = "depth";
    mShared.graph.depth = builder.create_texture(depth);

    graph_texture_desc normals = {};
    normals.width = size.x;
    normals.height = size.y;
    normals.format = rhi::texture_format::RG16_SNORM;
    normals.isRenderTarget = true;
    normals.debugName = "view_normals";
    mShared.graph.viewNormals = builder.create_texture(normals);

    builder.read(mShared.graph.instances);
    builder.colour_target(mShared.graph.viewNormals, 0);
    builder.depth_target(mShared.graph.depth);
}

void depth_prepass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr) {
        return;
    }
    nvrhi::utils::ClearColorAttachment(&context.command_list(), framebuffer, 0, nvrhi::Color(0.0f));
    mShared.clear_targets_once(context, framebuffer, false, true);
    nvrhi::IBindingSet *viewSet = mShared.mesh_view_set(nullptr);
    draw_mesh_region(
      context,
      mShared,
      framebuffer,
      mShared.viewport_state(),
      viewSet,
      mShared.view->main,
      mesh_pass_kind::PREPASS,
      0
    );
}
