#include "engine/subsystems/model_prefab_converter.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

#include "engine/subsystems/renderer_components.h"

#include "conjure/core/json/document.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/string_id.h"
#include "conjure/renderer/limits.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"

#include <format>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::engine;

namespace {

    /// <summary>
    /// Adds a reflected component value to a prefab node as JSON.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] core::result<void> add_component(
      scene::prefab_builder &builder,
      u32 node,
      std::string_view componentName,
      const ComponentType &value
    ) {
        CONJURE_TRY_ASSIGN(const core::json::document json, core::reflect::to_json_document(value));
        return builder.add_component_json(node, core::string_id(componentName), json.root());
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

model_prefab_converter::model_prefab_converter(const ecs::component_registry &registry) :
    mRegistry(&registry) {}

model_prefab_converter::~model_prefab_converter() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<scene::prefab_document> model_prefab_converter::convert(
  const renderer::model_asset &model,
  std::string_view name
) {
    mStats = {};
    scene::prefab_builder builder(*mRegistry);
    const std::string prefabName = name.empty() ? std::string(model.name()) : std::string(name);

    const std::span<const renderer::model_node> nodes = model.nodes();
    if (nodes.empty()) {
        const u32 root = builder.add_node(renderer::INVALID_INDEX, prefabName.empty() ? "model" : prefabName);
        CONJURE_UNUSED(root);
        mStats.nodes = 1;
        return builder.build(prefabName);
    }

    // Model nodes are stored parent-first (the importer writes them in traversal order), so the builder
    // indices equal the model indices; the mapping stays explicit in case a model breaks that rule.
    std::vector<u32> builderIndex(nodes.size(), renderer::INVALID_INDEX);
    for (usize index = 0; index < nodes.size(); ++index) {
        const renderer::model_node &node = nodes[index];
        u32 parent = renderer::INVALID_INDEX;
        if (node.parent != renderer::INVALID_INDEX && node.parent < nodes.size()) {
            parent = builderIndex[node.parent];
        }
        const std::string nodeName = node.name.empty() ? std::format("node_{}", index) : node.name;
        const scene::local_transform transform = scene::local_transform::from_transform(node.localTransform);
        const u32 added = builder.add_node(parent, nodeName, transform);
        builderIndex[index] = added;
        ++mStats.nodes;

        if (node.mesh != renderer::INVALID_INDEX && node.mesh < model.meshes().size()) {
            renderer::mesh_renderer_component meshRenderer = {};
            meshRenderer.mesh = assets::asset_ref<renderer::mesh_asset>(model.meshes()[node.mesh]);
            for (u32 slot = 0; slot < renderer::MAX_MATERIAL_SLOTS; ++slot) {
                const u32 material = node.materials[slot];
                if (material != renderer::INVALID_INDEX && material < model.materials().size()) {
                    const assets::asset_id &materialId = model.materials()[material];
                    meshRenderer.materials[slot] = assets::asset_ref<renderer::material_asset>(materialId);
                }
            }
            CONJURE_TRY(add_component(builder, added, renderer::MESH_RENDERER_COMPONENT_TYPE_NAME, meshRenderer));
            ++mStats.meshRenderers;
        }
        if (node.hasLight) {
            renderer::light_component light = {};
            light.lightData = node.lightData;
            CONJURE_TRY(add_component(builder, added, renderer::LIGHT_COMPONENT_TYPE_NAME, light));
            ++mStats.lights;
        }
        if (node.hasCamera) {
            renderer::camera_component camera = {};
            camera.cameraData = node.cameraData;
            CONJURE_TRY(add_component(builder, added, renderer::CAMERA_COMPONENT_TYPE_NAME, camera));
            ++mStats.cameras;
        }
    }
    return builder.build(prefabName);
}

#endif
