#include "conjure/physics/physics_debug_draw.h"

#include "conjure/core/platform_detect.h"

namespace conjure::physics {

physics_debug_draw::physics_debug_draw() {}

physics_debug_draw::~physics_debug_draw() {}

void physics_debug_draw::text(const math::vec3 &position, std::string_view label, u32 rgba, f32 height) {
    CONJURE_UNUSED(position);
    CONJURE_UNUSED(label);
    CONJURE_UNUSED(rgba);
    CONJURE_UNUSED(height);
}
}
