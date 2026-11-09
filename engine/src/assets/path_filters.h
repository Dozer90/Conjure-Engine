#pragma once

#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <string_view>

// Paths the scan, the watcher and hot reload must never treat as sources (docs/design/05_assets.md
// section 6: "*.tmp ignored"). Atomic writes name their temporaries "<file>.tmp" (the cache, the
// pack) or "<file>.tmp-<uuid>" (core::vfs::write_file_atomic, used for every .meta), so a crash
// mid-write can leave either form next to a source.

namespace conjure::assets::detail {

    /// <summary>
    /// True when the last segment of the path is an atomic-write temporary: it ends in ".tmp" or
    /// carries a ".tmp-" marker ("a.ta.meta.tmp-7f3c...").
    /// </summary>
    [[nodiscard]] inline bool is_temporary_path(std::string_view path) noexcept {
        const std::string_view name = core::vfs::path_utils::filename(path);
        return core::str::ends_with(name, ".tmp") || name.find(".tmp-") != std::string_view::npos;
    }
}
