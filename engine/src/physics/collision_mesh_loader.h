#pragma once

#include "physics/jolt_runtime.h"

#include "conjure/physics/collision_mesh_asset.h"

#include "conjure/assets/asset_loader.h"
#include "conjure/core/result.h"

#include <cstddef>
#include <span>

// The loader of conjure.physics.collision_mesh (docs/design/14_physics.md 2.7, R44): the cooked payload
// is parsed and the Jolt mesh shape built on the load job; finalise publishes the value unchanged. The
// loader keeps a Jolt runtime reference so shapes can be built before any physics_world exists.

namespace conjure::physics::detail {

    class collision_mesh_loader final : public assets::typed_asset_loader<collision_mesh_asset, collision_mesh_asset> {
        private:
            jolt_runtime::guard mRuntime = {};

        public:
            collision_mesh_loader();
            ~collision_mesh_loader() override;

        protected:
            [[nodiscard]] core::result<collision_mesh_asset> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<collision_mesh_asset> finalise_typed(
              const assets::finalise_context &context,
              collision_mesh_asset &&intermediate
            ) const override;
    };
}
