#include "renderer/passes/passes.h"

// Sprites (docs/design/02_renderer.md R12, 3.9): WORLD runs draw into the HDR target after the
// blended meshes (depth-tested, premultiplied alpha), SCREEN runs after tonemapping into the LDR
// target in pixel space. One drawIndexed per run; the sampler follows the texture's filter.

using namespace conjure;
using namespace conjure::renderer;

sprite_pass::sprite_pass(pass_shared &shared, sprite_space space) :
    mShared(shared),
    mSpace(space) {}

sprite_pass::~sprite_pass() {}

const char *sprite_pass::name() const noexcept {
    return mSpace == sprite_space::WORLD ? "world_sprites" : "screen_sprites";
}

void sprite_pass::setup(pass_builder &builder) {
    builder.read(mShared.graph.spriteVertices);
    if (mSpace == sprite_space::WORLD) {
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
    } else {
        builder.colour_target(mShared.graph.ldr, 0);
    }
}

void sprite_pass::execute(pass_context &context) {
    nvrhi::IFramebuffer *framebuffer = context.framebuffer();
    if (framebuffer == nullptr) {
        return;
    }
    if (mSpace == sprite_space::WORLD) {
        mShared.clear_targets_once(context, framebuffer, true, true);
    }
    const graphics_variant variant = mSpace == sprite_space::WORLD ? graphics_variant::WORLD_SPRITES
      : graphics_variant::SCREEN_SPRITES;
    const nvrhi::ViewportState viewport = mShared.viewport_state();
    for (const sprite_run &run : mShared.spriteRuns) {
        if (run.space != mSpace || run.indexCount == 0) {
            continue;
        }
        const gpu_texture &texture = mShared.resources->resolve_texture(run.texture);
        nvrhi::IBindingSet *set = mShared.fullscreen_set(
          texture.texture,
          nullptr,
          nullptr,
          mShared.resources->sampler(sampler_filter::LINEAR, texture.wrap),
          mShared.resources->sampler(sampler_filter::POINT, texture.wrap)
        );
        if (set == nullptr) {
            continue;
        }
        nvrhi::GraphicsState state = {};
        state.pipeline = mShared.pipelines->graphics(variant);
        state.framebuffer = framebuffer;
        state.viewport = viewport;
        state.addBindingSet(set);
        state.addVertexBuffer(
          nvrhi::VertexBufferBinding().setBuffer(mShared.frame->sprite_vertices()).setSlot(0).setOffset(0)
        );
        nvrhi::IndexBufferBinding indexBinding = {};
        indexBinding.setBuffer(mShared.frame->sprite_indices()).setFormat(nvrhi::Format::R32_UINT).setOffset(0);
        state.setIndexBuffer(indexBinding);
        context.command_list().setGraphicsState(state);

        gpu::draw_constants constants = {};
        constants.flags = (mSpace == sprite_space::SCREEN ? gpu::DRAW_FLAG_SCREEN_SPACE : 0u) |
          (texture.filter == sampler_filter::POINT ? gpu::DRAW_FLAG_POINT_SAMPLER : 0u);
        context.command_list().setPushConstants(&constants, sizeof(constants));

        nvrhi::DrawArguments arguments = {};
        arguments.vertexCount = run.indexCount;
        arguments.instanceCount = 1;
        arguments.startIndexLocation = run.firstIndex;
        context.command_list().drawIndexed(arguments);
        ++context.stats().drawCalls;
        context.stats().spritesDrawn += run.indexCount / 6;
        context.stats().trianglesDrawn += run.indexCount / 3;
    }
}
