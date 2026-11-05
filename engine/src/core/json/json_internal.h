#pragma once

// Private helpers shared by the json area's sources. This is the only place that includes yyjson;
// the public headers carry opaque pointers.

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"

#include "conjure/core/types.h"

#include <yyjson.h>

#include <string>
#include <string_view>

namespace conjure::core::json::detail {

    /// <summary>
    /// Casts an opaque node handle back to yyjson.
    /// </summary>
    [[nodiscard]] inline yyjson_mut_val *as_node(void *handle) noexcept {
        return static_cast<yyjson_mut_val *>(handle);
    }

    /// <summary>
    /// Casts an opaque document handle back to yyjson.
    /// </summary>
    [[nodiscard]] inline yyjson_mut_doc *as_document(void *handle) noexcept {
        return static_cast<yyjson_mut_doc *>(handle);
    }

    /// <summary>
    /// The string bytes of a string or key node.
    /// </summary>
    [[nodiscard]] inline std::string_view node_string(yyjson_mut_val *node) noexcept {
        return std::string_view(yyjson_mut_get_str(node), yyjson_mut_get_len(node));
    }

    /// <summary>
    /// The data pointer of a key or string view as yyjson expects it. A default-constructed view
    /// has no data pointer; yyjson would reject it instead of treating it as the empty string.
    /// </summary>
    [[nodiscard]] inline const char *key_data(std::string_view text) noexcept {
        return text.data() == nullptr ? "" : text.data();
    }

    /// <summary>
    /// Creates a string node whose bytes are copied into the document.
    /// </summary>
    [[nodiscard]] inline yyjson_mut_val *make_string(yyjson_mut_doc *target, std::string_view text) {
        return yyjson_mut_strncpy(target, key_data(text), text.size());
    }

    /// <summary>
    /// Creates a number node: non-negative integers unsigned, negative ones signed.
    /// </summary>
    [[nodiscard]] inline yyjson_mut_val *make_integer(yyjson_mut_doc *target, i64 number) {
        if (number >= 0) {
            return yyjson_mut_uint(target, static_cast<u64>(number));
        }
        return yyjson_mut_sint(target, number);
    }

    /// <summary>
    /// Creates a real node, or a null node (after asserting) for a non-finite number.
    /// </summary>
    [[nodiscard]] yyjson_mut_val *make_real(yyjson_mut_doc *target, f64 number);

    /// <summary>
    /// Deep-copies 'source' into 'target' with every object's members sorted by key (byte order,
    /// stable for duplicate keys). Walks with an explicit stack, so any depth is safe. Returns
    /// nullptr when memory runs out.
    /// </summary>
    [[nodiscard]] yyjson_mut_val *sorted_copy(yyjson_mut_doc *target, yyjson_mut_val *source);

    /// <summary>
    /// True when the immutable tree under 'root' nests containers no deeper than 'limit' levels
    /// (a scalar root is depth 0, "[]" is depth 1). Walks iteratively, so any depth is safe.
    /// </summary>
    [[nodiscard]] bool within_nesting_limit(yyjson_val *root, usize limit);

    /// <summary>
    /// True when 'text' is well-formed UTF-8 by yyjson's rules: no overlong forms, no surrogate
    /// code points, nothing above U+10FFFF, no truncated sequences.
    /// </summary>
    [[nodiscard]] bool is_valid_utf8(std::string_view text) noexcept;

    /// <summary>
    /// Serialises a node with yyjson flags. Logs and returns an empty string on failure.
    /// </summary>
    [[nodiscard]] std::string write_node(yyjson_mut_val *node, u32 flags);

    /// <summary>
    /// Serialises a node in canonical form (sorted keys, minified).
    /// </summary>
    [[nodiscard]] std::string write_canonical(yyjson_mut_val *node);

    /// <summary>
    /// Formats a finite double exactly as yyjson's writer does (shortest round-trip, "-0.0",
    /// "1.0", "1e21"). Returns the number of characters written to 'buffer' (at least 32 bytes).
    /// </summary>
    [[nodiscard]] usize format_real(f64 number, char *buffer) noexcept;

    /// <summary>
    /// The yyjson type of a node, or NULL_VALUE for nullptr.
    /// </summary>
    [[nodiscard]] json_type type_of(yyjson_mut_val *node) noexcept;
}
