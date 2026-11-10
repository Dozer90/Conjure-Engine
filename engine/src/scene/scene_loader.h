#pragma once

#include "conjure/scene/prefab.h"
#include "conjure/scene/scene_serializer.h"

#include "conjure/assets/asset_loader.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"

#include <cstddef>
#include <span>

// The asset loaders of conjure.scene.scene and conjure.scene.prefab (docs/design/13_scene.md 4.7): the
// cooked payload is canonical JSON, parsed on the load job against the component registry (lookups are
// thread-safe); finalise hands the parsed document through unchanged.

namespace conjure::scene::detail {

    class scene_asset_loader final : public assets::typed_asset_loader<scene_asset, scene_document> {
        private:
            const ecs::component_registry *mRegistry = nullptr;

        public:
            explicit scene_asset_loader(const ecs::component_registry &registry);
            ~scene_asset_loader() override;

        protected:
            [[nodiscard]] core::result<scene_document> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<scene_asset> finalise_typed(
              const assets::finalise_context &context,
              scene_document &&intermediate
            ) const override;
    };

    class prefab_asset_loader final : public assets::typed_asset_loader<prefab_asset, prefab_asset> {
        private:
            const ecs::component_registry *mRegistry = nullptr;

        public:
            explicit prefab_asset_loader(const ecs::component_registry &registry);
            ~prefab_asset_loader() override;

        protected:
            [[nodiscard]] core::result<prefab_asset> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<prefab_asset> finalise_typed(
              const assets::finalise_context &context,
              prefab_asset &&intermediate
            ) const override;
    };
}
