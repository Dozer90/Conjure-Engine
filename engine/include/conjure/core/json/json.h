#pragma once

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"

#include "conjure/core/result.h"

#include <string>
#include <string_view>

// Umbrella header for the json area. Include this unless you need only one of the pieces.

namespace conjure::core::json {

    /// <summary>
    /// Parses strict JSON text into a new document. Same as document::parse().
    /// </summary>
    [[nodiscard]] result<document> parse(std::string_view text);

    /// <summary>
    /// Serialises one value (any node of a document). Canonical output sorts object members by
    /// key; both forms are minified. An invalid value yields an empty string.
    /// </summary>
    [[nodiscard]] std::string to_string(const value &source, bool canonical = false);
}
