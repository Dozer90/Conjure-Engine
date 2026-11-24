#pragma once

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"

#include <string>
#include <string_view>

// Umbrella header for the json area. Include this unless you need only one of the pieces.

namespace conjure::core::json {

    /// <summary>
    /// Parses strict JSON text into a new document. Same as document::parse().
    /// </summary>
    [[nodiscard]] CONJURE_API result<document> parse(std::string_view text);

    /// <summary>
    /// Serialises one value (any node of a document) minified, with object members in their
    /// stored order; the same text document::to_string() gives for a root. Returns an empty
    /// string for an invalid value, and an empty string (with a logged warning) when a string
    /// in the value holds invalid UTF-8. Use to_canonical_string() for text that is hashed or
    /// compared.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(const value &source);

    /// <summary>
    /// Serialises one value (any node of a document) in canonical form: object members sorted by
    /// key (byte order) at every depth, minified, shortest round-trip reals. The same text
    /// document::to_canonical_string() gives for a root, and identical on every platform for the
    /// same content. Returns an empty string for an invalid value, and an empty string (with a
    /// logged warning) when a string in the value holds invalid UTF-8 or memory runs out.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_canonical_string(const value &source);
}
