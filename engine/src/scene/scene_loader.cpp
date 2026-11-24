#include "scene/scene_loader.h"

#include "scene/archive_tools.h"
#include "scene/scene_json.h"

#include "conjure/assets/assets_error.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace conjure::scene::detail {

namespace {

    [[nodiscard]] std::string_view text_of(std::span<const std::byte> bytes) noexcept {
        return std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }

    [[nodiscard]] core::error load_failure(const core::error &inner) {
        return core::make_error(assets::assets_error::LOAD_FAILED, inner.to_string());
    }
}

/*===========================================================================*/
/*===========================================================================*/
// scene_asset_loader

scene_asset_loader::scene_asset_loader(const ecs::component_registry &registry) :
    mRegistry(&registry) {}

scene_asset_loader::~scene_asset_loader() {}

core::result<scene_document> scene_asset_loader::load_cpu_typed(
  const assets::load_context &context,
  std::span<const std::byte> payloadBytes
) const {
    CONJURE_UNUSED(context);
    core::result<scene_document> parsed = parse_scene_text(text_of(payloadBytes), *mRegistry);
    if (!parsed.has_value()) {
        return load_failure(parsed.get_error());
    }
    return std::move(parsed).value();
}

core::result<scene_asset> scene_asset_loader::finalise_typed(
  const assets::finalise_context &context,
  scene_document &&intermediate
) const {
    CONJURE_UNUSED(context);
    return std::move(intermediate);
}


/*===========================================================================*/
/*===========================================================================*/
// prefab_asset_loader

prefab_asset_loader::prefab_asset_loader(const ecs::component_registry &registry) :
    mRegistry(&registry) {}

prefab_asset_loader::~prefab_asset_loader() {}

core::result<prefab_asset> prefab_asset_loader::load_cpu_typed(
  const assets::load_context &context,
  std::span<const std::byte> payloadBytes
) const {
    core::result<prefab_document> parsed = parse_prefab_text(text_of(payloadBytes), *mRegistry);
    if (!parsed.has_value()) {
        return load_failure(parsed.get_error());
    }
    prefab_asset asset = {};
    asset.name = std::move(parsed.value().name);
    asset.archive = std::move(parsed.value().archive);
    asset.id = context.id;
    const scene_component_ids ids = scene_component_ids::resolve(*mRegistry);
    asset.rootGuid = entry_guid(*asset.archive, 0, ids).value_or(core::uuid());
    asset.dependencies.assign(context.dependencies.begin(), context.dependencies.end());
    std::sort(asset.dependencies.begin(), asset.dependencies.end());
    return asset;
}

core::result<prefab_asset> prefab_asset_loader::finalise_typed(
  const assets::finalise_context &context,
  prefab_asset &&intermediate
) const {
    CONJURE_UNUSED(context);
    return std::move(intermediate);
}
}
