#include "assets/load_pipeline.h"

#include "conjure/assets/asset_loader.h"

#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::detail;

load_stage_result conjure::assets::detail::load_cpu_stage(
  const asset_type_registry &types,
  const asset_id &id,
  asset_type_id expectedType,
  core::result<cooked_blob> &&blob
) {
    load_stage_result stage = {};

    if (!blob.has_value()) {
        stage.output = std::move(blob).get_error();
        return stage;
    }
    stage.header = blob.value().header;
    stage.dependencies = std::move(blob.value().dependencies);

    if (expectedType != asset_type_id::INVALID && stage.header.type != expectedType) {
        stage.output = core::make_error(
          assets_error::TYPE_MISMATCH,
          std::format(
            "asset {} is cooked as type {:#x} but the slot is bound to type {:#x}",
            id,
            static_cast<u64>(stage.header.type),
            static_cast<u64>(expectedType)
          )
        );
        return stage;
    }

    const asset_loader *loader = types.find_loader(stage.header.type);
    if (loader == nullptr) {
        stage.output = core::make_error(
          assets_error::NO_LOADER,
          std::format("no loader registered for type {:#x} of asset {}", static_cast<u64>(stage.header.type), id)
        );
        return stage;
    }

    load_context context = {};
    context.id = id;
    context.type = stage.header.type;
    context.dependencies = stage.dependencies;
    core::result<asset_payload> output = loader->load_cpu(context, blob.value().payload);
    if (!output.has_value()) {
        const core::error &failure = output.get_error();
        if (failure.is_category<assets_error>()) {
            stage.output = failure;
        } else {
            stage.output = core::make_error(
              assets_error::LOAD_FAILED,
              std::format("load_cpu of {} failed: {}", id, failure.to_string())
            );
        }
        return stage;
    }
    stage.output = std::move(output).value();
    return stage;
}
