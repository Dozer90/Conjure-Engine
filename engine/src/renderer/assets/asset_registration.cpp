#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/gltf_importer.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/texture_asset.h"

#include <memory>

// renderer::register_asset_types (docs/design/02_renderer.md 4.2, R10, R34): the five asset types
// with their CPU-only loaders and the four importers. Materials and models reload when a dependency
// changes so hot-reloaded textures and meshes reach the GPU mirrors through the usual events.

using namespace conjure;

core::result<void> conjure::renderer::renderer::register_asset_types(
  assets::asset_type_registry &types,
  assets::importer_registry &importers,
  core::jobs::job_system *jobs
) {
    CONJURE_TRY(
      types.register_type(assets::make_asset_type_desc<mesh_asset>(std::make_unique<mesh_loader>(), MESH_BLOB_VERSION))
    );
    CONJURE_TRY(types.register_type(
      assets::make_asset_type_desc<texture_asset>(std::make_unique<texture_loader>(), TEXTURE_BLOB_VERSION)
    ));
    CONJURE_TRY(types.register_type(assets::make_asset_type_desc<material_asset>(
      std::make_unique<material_loader>(),
      MATERIAL_IMPORTER_VERSION,
      assets::asset_type_flags::RELOAD_WITH_DEPENDENCIES
    )));
    CONJURE_TRY(types.register_type(assets::make_asset_type_desc<model_asset>(
      std::make_unique<model_loader>(),
      MODEL_FORMAT_VERSION,
      assets::asset_type_flags::RELOAD_WITH_DEPENDENCIES
    )));
    CONJURE_TRY(types.register_type(
      assets::make_asset_type_desc<environment_asset>(std::make_unique<environment_loader>(), ENVIRONMENT_BLOB_VERSION)
    ));
    CONJURE_TRY(importers.register_importer(std::make_unique<gltf_asset_importer>()));
    CONJURE_TRY(importers.register_importer(std::make_unique<texture_importer>()));
    CONJURE_TRY(importers.register_importer(std::make_unique<material_importer>()));
    CONJURE_TRY(importers.register_importer(std::make_unique<environment_importer>(jobs)));
    return core::result<void>();
}
