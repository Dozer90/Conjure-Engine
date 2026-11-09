#pragma once

#include "conjure/assets/asset_cache.h"
#include "conjure/assets/asset_cooker.h"
#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_extension_desc.h"
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/asset_pack.h"
#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/assets_error.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/assets/file_watcher.h"
#include "conjure/assets/hot_reload.h"
#include "conjure/assets/log_category.h"

// Umbrella header of the assets module (conjure::assets). Include this unless you need only one
// of the pieces. See engine/src/assets/README.md and docs/design/05_assets.md.
