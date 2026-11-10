#include "conjure/renderer/render_target.h"
#include "renderer/render_target_impl.h"

using namespace conjure;
using namespace conjure::renderer;

render_target::render_target() :
    mImpl(std::make_unique<impl>()) {}

render_target::~render_target() {}

const render_target_desc &render_target::desc() const noexcept {
    return mImpl->desc;
}

math::uvec2 render_target::size() const noexcept {
    return mImpl->desc.size;
}

gpu_texture_id render_target::colour_texture() const noexcept {
    return mImpl->colourId;
}

bool render_target::has_depth() const noexcept {
    return mImpl->depth != nullptr;
}

u64 render_target::frames_rendered() const noexcept {
    return mImpl->framesRendered;
}

bool conjure::renderer::is_render_target_colour_format(rhi::texture_format format) noexcept {
    switch (format) {
        case rhi::texture_format::RGBA8_UNORM:
        case rhi::texture_format::RGBA8_SRGB:
        case rhi::texture_format::BGRA8_SRGB:
        case rhi::texture_format::RGBA16_FLOAT:
        case rhi::texture_format::R11G11B10_FLOAT: {
            return true;
        }
        default: {
            return false;
        }
    }
}
