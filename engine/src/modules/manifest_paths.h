#pragma once

#include "conjure/core/types.h"

#include <string_view>

// The path rules of a module manifest (docs/api/modules/writing-a-module.md, "The manifest"). Both
// checks are textual and treat '/' and '\' alike, so a path is accepted or refused the same way on
// every platform. module_manifest.cpp applies them to "binary", "managed" and "content";
// module_registrar.cpp applies stays_inside_root() to the assemblies an entry registers, because
// export_layout() writes those into the exported manifest, which has to parse again.

namespace conjure::modules::detail {

    /// <summary>
    /// True for a file name without a directory part: no '/', '\' or ':', and neither "." nor "..".
    /// "binary" is decorated and joined to the binary directory, so it must not carry a directory.
    /// </summary>
    [[nodiscard]] constexpr bool is_base_name(std::string_view text) noexcept {
        if (text == "." || text == "..") {
            return false;
        }
        return text.find_first_of("/\\:") == std::string_view::npos;
    }

    /// <summary>
    /// True for a non-empty relative path that stays inside the root it is joined to: no leading
    /// separator, no ':' (a drive or a stream) and no ".." segment. "managed" and "content" entries
    /// are joined to the module root and copied on export.
    /// </summary>
    [[nodiscard]] constexpr bool stays_inside_root(std::string_view text) noexcept {
        if (text.empty() || text.front() == '/' || text.front() == '\\' || text.find(':') != std::string_view::npos) {
            return false;
        }
        usize segmentStart = 0;
        while (segmentStart <= text.size()) {
            const usize separator = text.find_first_of("/\\", segmentStart);
            const usize segmentEnd = separator == std::string_view::npos ? text.size() : separator;
            if (text.substr(segmentStart, segmentEnd - segmentStart) == "..") {
                return false;
            }
            segmentStart = segmentEnd + 1;
        }
        return true;
    }
}
