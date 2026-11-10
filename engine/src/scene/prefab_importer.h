#pragma once

#include "conjure/assets/asset_importer.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"

#include <span>
#include <string_view>

// The .scene.json and .prefab.json importers (docs/design/13_scene.md 4.7): parse, validate the header,
// read the archive against the registry, collect every non-null asset_ref uuid as a dependency, warn per
// unresolved component name and re-emit canonical JSON as the cooked payload.

namespace conjure::scene::detail {

    class scene_importer final : public assets::asset_importer {
        private:
            const ecs::component_registry *mRegistry = nullptr;

        public:
            static constexpr u32 VERSION = 1;

        public:
            explicit scene_importer(const ecs::component_registry &registry);
            ~scene_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };

    class prefab_importer final : public assets::asset_importer {
        private:
            const ecs::component_registry *mRegistry = nullptr;

        public:
            static constexpr u32 VERSION = 1;

        public:
            explicit prefab_importer(const ecs::component_registry &registry);
            ~prefab_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };
}
