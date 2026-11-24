#include "renderer/passes/passes.h"

#include <nvrhi/utils.h>

#include <algorithm>

// Helpers every pass uses: viewport state, the cached binding sets of the mesh and fullscreen
// layouts, lazy clears and the mesh region draw loop.

using namespace conjure;
using namespace conjure::renderer;

nvrhi::ViewportState pass_shared::viewport_state() const noexcept {
    const math::urect &rect = view->viewport;
    nvrhi::ViewportState state = {};
    state.addViewport(nvrhi::Viewport(
      static_cast<f32>(rect.x),
      static_cast<f32>(rect.x + rect.width),
      static_cast<f32>(rect.y),
      static_cast<f32>(rect.y + rect.height),
      0.0f,
      1.0f
    ));
    state.addScissorRect(nvrhi::Rect(
      static_cast<int>(rect.x),
      static_cast<int>(rect.x + rect.width),
      static_cast<int>(rect.y),
      static_cast<int>(rect.y + rect.height)
    ));
    return state;
}

nvrhi::IBindingSet *pass_shared::mesh_view_set(
  nvrhi::ITexture *ambientOcclusion,
  nvrhi::ITexture *ambientOcclusionDepth
) const {
    const view_resources &resourcesOfView = frame->view(view->viewIndex);
    const gpu_environment &environment = view->environment != nullptr ? *view->environment
      : resources->fallback_environment();
    nvrhi::ITexture *white = resources->fallback_texture(material_texture::BASE_COLOUR).texture.Get();
    nvrhi::ITexture *ao = ambientOcclusion != nullptr ? ambientOcclusion : white;
    nvrhi::ITexture *aoDepth = ambientOcclusionDepth != nullptr ? ambientOcclusionDepth : white;
    nvrhi::BindingSetDesc &desc = binding_desc();
    desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, frame->view_constants()));
    desc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, frame->instances()));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(1, resourcesOfView.lights));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, resourcesOfView.shadowTiles));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, frame->shadow_cascades()));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, frame->shadow_atlas()));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(5, resourcesOfView.tileLists));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, ao));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(7, environment.prefiltered));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(8, frame->brdf_lut()));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(9, aoDepth));
    desc.addItem(nvrhi::BindingSetItem::Sampler(1, shadowSampler));
    desc.addItem(nvrhi::BindingSetItem::Sampler(2, linearClampSampler));
    desc.addItem(nvrhi::BindingSetItem::Sampler(3, pointClampSampler));
    return bindingSets->get(layouts->meshView, desc);
}

nvrhi::IBindingSet *pass_shared::fullscreen_set(
  nvrhi::ITexture *texture0,
  nvrhi::ITexture *texture1,
  nvrhi::IBuffer *buffer2,
  nvrhi::ISampler *sampler0,
  nvrhi::ISampler *sampler1
) const {
    nvrhi::ITexture *white = resources->fallback_texture(material_texture::BASE_COLOUR).texture.Get();
    nvrhi::IBuffer *fallbackBuffer = frame->view(view->viewIndex).exposureState;
    nvrhi::BindingSetDesc &desc = binding_desc();
    desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, frame->view_constants()));
    desc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture0 != nullptr ? texture0 : white));
    desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, texture1 != nullptr ? texture1 : white));
    desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, buffer2 != nullptr ? buffer2 : fallbackBuffer));
    desc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler0 != nullptr ? sampler0 : linearClampSampler));
    desc.addItem(nvrhi::BindingSetItem::Sampler(1, sampler1 != nullptr ? sampler1 : pointClampSampler));
    return bindingSets->get(layouts->fullscreen, desc);
}

nvrhi::IBindingSet *pass_shared::compute_set(nvrhi::IBindingLayout *layout, const nvrhi::BindingSetDesc &desc) const {
    return bindingSets->get(layout, desc);
}

void pass_shared::clear_targets_once(
  pass_context &context,
  nvrhi::IFramebuffer *framebuffer,
  bool hdr,
  bool depth
) const {
    if (framebuffer == nullptr) {
        return;
    }
    const nvrhi::FramebufferDesc &desc = framebuffer->getDesc();
    if (hdr && !view->hdrCleared && !desc.colorAttachments.empty()) {
        const math::vec4 &clear = view->hdrClear;
        nvrhi::utils::ClearColorAttachment(
          &context.command_list(),
          framebuffer,
          0,
          nvrhi::Color(clear.x, clear.y, clear.z, clear.w)
        );
        view->hdrCleared = true;
    }
    if (depth && !view->depthCleared && desc.depthAttachment.valid()) {
        // Reverse-Z: the far plane is depth 0.
        nvrhi::utils::ClearDepthStencilAttachment(&context.command_list(), framebuffer, 0.0f, 0);
        view->depthCleared = true;
    }
}

void conjure::renderer::draw_mesh_region(
  pass_context &context,
  const pass_shared &shared,
  nvrhi::IFramebuffer *framebuffer,
  const nvrhi::ViewportState &viewport,
  nvrhi::IBindingSet *viewSet,
  const draw_region &region,
  mesh_pass_kind kind,
  u32 drawFlags
) {
    nvrhi::ICommandList &commandList = context.command_list();
    render_stats &stats = context.stats();
    for (usize index = 0; index < region.batches.size(); ++index) {
        const draw_batch &batch = region.batches[index];
        const gpu_mesh *mesh = region.meshes[index];
        const gpu_material *material = region.materials[index];
        if (mesh == nullptr || material == nullptr || batch.submesh >= mesh->submeshes.size()) {
            continue;
        }

        graphics_variant variant = graphics_variant::COUNT;
        bool positionOnly = false;
        switch (kind) {
            case mesh_pass_kind::SHADOW: {
                if (batch.variant == draw_variant::BLENDED) {
                    continue;
                }
                variant = batch.variant == draw_variant::MASKED ? graphics_variant::SHADOW_DEPTH_MASKED
                  : graphics_variant::SHADOW_DEPTH;
                positionOnly = batch.variant != draw_variant::MASKED;
                break;
            }
            case mesh_pass_kind::PREPASS: {
                if (batch.variant == draw_variant::BLENDED) {
                    continue;
                }
                variant = batch.variant == draw_variant::MASKED ? graphics_variant::DEPTH_PREPASS_MASKED
                  : graphics_variant::DEPTH_PREPASS;
                break;
            }
            case mesh_pass_kind::FORWARD_OPAQUE: {
                if (batch.variant == draw_variant::BLENDED) {
                    continue;
                }
                variant = batch.variant == draw_variant::MASKED ? graphics_variant::FORWARD_MASKED
                  : graphics_variant::FORWARD_OPAQUE;
                break;
            }
            case mesh_pass_kind::FORWARD_BLEND: {
                if (batch.variant != draw_variant::BLENDED) {
                    continue;
                }
                variant = graphics_variant::FORWARD_BLEND;
                break;
            }
        }

        nvrhi::GraphicsState state = {};
        state.pipeline = shared.pipelines->graphics(variant, batch.doubleSided);
        state.framebuffer = framebuffer;
        state.viewport = viewport;
        state.addBindingSet(viewSet);
        state.addBindingSet(material->bindingSet);
        state.addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(mesh->stream0).setSlot(0).setOffset(0));
        if (!positionOnly) {
            state.addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(mesh->stream1).setSlot(1).setOffset(0));
        }
        state.setIndexBuffer(
          nvrhi::IndexBufferBinding().setBuffer(mesh->indices).setFormat(nvrhi::Format::R32_UINT).setOffset(0)
        );
        commandList.setGraphicsState(state);

        gpu::draw_constants constants = {};
        constants.firstInstance = region.firstInstance + batch.firstItem;
        constants.flags = drawFlags;
        commandList.setPushConstants(&constants, sizeof(constants));

        const submesh &part = mesh->submeshes[batch.submesh];
        nvrhi::DrawArguments arguments = {};
        arguments.vertexCount = part.indexCount;
        arguments.instanceCount = batch.instanceCount;
        arguments.startIndexLocation = part.firstIndex;
        arguments.startVertexLocation = 0;
        arguments.startInstanceLocation = 0;
        commandList.drawIndexed(arguments);

        ++stats.drawCalls;
        stats.instancesDrawn += batch.instanceCount;
        stats.trianglesDrawn += (part.indexCount / 3) * batch.instanceCount;
    }
}
