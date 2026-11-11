#pragma once

#include "conjure/core/platform_detect.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/scene/scene_serializer.h"

#include <string_view>

// model_asset -> prefab (docs/design/09_cross_area_resolutions.md section 3, owed to wave 4): every model
// node becomes a prefab node under its parent with the node's local transform; a node with a mesh carries a
// mesh_renderer_component referencing the model's mesh and material assets, a node with a light a
// light_component, a node with a camera a camera_component. The component values travel as JSON through
// prefab_builder::add_component_json, so the converter needs the renderer components registered in the
// registry it is given (renderer_subsystem does that at startup step 13).

namespace conjure::engine {

    struct model_prefab_stats {
        u32 nodes = 0;
        u32 meshRenderers = 0;
        u32 lights = 0;
        u32 cameras = 0;
    };

    class model_prefab_converter {
        private:
            const ecs::component_registry *mRegistry = nullptr;
            model_prefab_stats mStats = {};

        public:
            explicit model_prefab_converter(const ecs::component_registry &registry);
            ~model_prefab_converter();

            model_prefab_converter(const model_prefab_converter &) = delete;
            model_prefab_converter &operator=(const model_prefab_converter &) = delete;

        public:
            /// <summary>
            /// Builds the prefab document named 'name' (the model's own name when empty). A model without nodes
            /// yields a prefab with a single empty root. Errors are the builder's (UNKNOWN_COMPONENT when the
            /// renderer components are not registered, BUILDER_INVALID for a node whose parent follows it).
            /// </summary>
            [[nodiscard]] core::result<scene::prefab_document> convert(
              const renderer::model_asset &model,
              std::string_view name = {}
            );

            /// <summary>
            /// What the last convert() produced.
            /// </summary>
            [[nodiscard]] const model_prefab_stats &stats() const noexcept {
                return mStats;
            }
    };
}

#endif
