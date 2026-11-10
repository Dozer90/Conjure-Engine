#include "physics/collision_mesh_loader.h"

#include "physics/collision_mesh_internal.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/platform_detect.h"

#include <memory>
#include <utility>

namespace conjure::physics::detail {

collision_mesh_loader::collision_mesh_loader() {}

collision_mesh_loader::~collision_mesh_loader() {}

core::result<collision_mesh_asset> collision_mesh_loader::load_cpu_typed(
  const assets::load_context &context,
  std::span<const std::byte> payloadBytes
) const {
    CONJURE_UNUSED(context);
    core::result<collision_mesh_asset> parsed = collision_mesh_asset::from_payload(payloadBytes);
    if (!parsed.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::LOAD_FAILED, parsed.get_error().to_string());
    }
    collision_mesh_asset mesh = std::move(parsed).value();

    core::result<JPH::RefConst<JPH::Shape>> shape = build_mesh_shape(mesh);
    if (!shape.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::LOAD_FAILED, shape.get_error().to_string());
    }
    std::shared_ptr<collision_mesh_shapes> shapes = std::make_shared<collision_mesh_shapes>();
    shapes->meshShape = std::move(shape).value();
    mesh.set_shapes(std::move(shapes));
    return mesh;
}

core::result<collision_mesh_asset> collision_mesh_loader::finalise_typed(
  const assets::finalise_context &context,
  collision_mesh_asset &&intermediate
) const {
    CONJURE_UNUSED(context);
    return std::move(intermediate);
}
}
