#pragma once

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/render_target.h"

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

// The private state of render_target, shared with renderer.cpp (a friend) so the renderer can bind
// the colour and depth textures without the public header naming NVRHI.

namespace conjure::renderer {

    struct render_target::impl {
        render_target_desc desc = {};
        nvrhi::TextureHandle colour = nullptr;
        nvrhi::TextureHandle depth = nullptr;
        gpu_texture_id colourId = {};
        u64 framesRendered = 0;
    };
}
