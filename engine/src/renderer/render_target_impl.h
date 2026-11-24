#pragma once

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/render_target.h"

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

// The private state of render_target, shared with renderer.cpp (a friend) so the renderer can bind
// the colour and depth textures without the public header naming NVRHI. The depth texture is imported
// into the render graph of the view that owns it in a frame (renderer.cpp, render_view_graph).

namespace conjure::renderer {

    struct render_target::impl {
        render_target_desc desc = {};
        nvrhi::TextureHandle colour = nullptr;
        nvrhi::TextureHandle depth = nullptr;
        gpu_texture_id colourId = {};
        u64 framesRendered = 0;
        // The renderer frame (renderer_state::frameIndex, from 1) whose first view into the target owns its
        // depth; 0 until a view has rendered into it. A later view of the same frame finds its own frame
        // here and renders with a transient depth buffer (render_target.h). Every frame that owns the
        // depth clears it, so a non-zero value also means the texture holds defined contents, which a
        // freshly created texture does not on every backend.
        u64 depthOwnerFrame = 0;
    };
}
