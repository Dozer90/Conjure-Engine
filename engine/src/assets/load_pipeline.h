#pragma once

#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <vector>

// The worker-thread half of the load pipeline (docs/design/05_assets.md 3.3, read_and_load_cpu),
// shared by the asynchronous jobs and by load_blocking. The blob read itself is the manager's
// (it happens under the source lock); this stage takes the read result.

namespace conjure::assets::detail {

    /// <summary>
    /// Output of the worker stage: the blob header and dependencies (valid when the blob was
    /// read) and the loader's CPU output or the failure.
    /// </summary>
    struct load_stage_result {
        cooked_blob_header header = {};
        std::vector<asset_id> dependencies = {};
        core::result<asset_payload> output = make_null_payload();
    };

    /// <summary>
    /// Checks the blob's type against the slot's (INVALID accepts any type), finds the loader
    /// and runs load_cpu. A failed read is passed through as the stage's failure. Any thread.
    /// </summary>
    [[nodiscard]] load_stage_result load_cpu_stage(
      const asset_type_registry &types,
      const asset_id &id,
      asset_type_id expectedType,
      core::result<cooked_blob> &&blob
    );
}
