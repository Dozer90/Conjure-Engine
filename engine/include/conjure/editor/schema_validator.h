#pragma once

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

// The hand-written JSON Schema subset (docs/design/04_editor.md 2.1): type (one name or an array of names),
// properties, required, additionalProperties (default false at the top level of a command's args, true below),
// items, enum, minimum, maximum, minItems, maxItems, oneOf (shallow), plus the documentation keywords
// description, title and default. Any other keyword fails compilation, so a typo in a descriptor is caught at
// registration rather than silently accepted.

namespace conjure::editor {

    /// <summary>
    /// A parsed and checked schema.
    /// </summary>
    class CONJURE_API compiled_schema {
        private:
            core::json::document mSchema = core::json::document::object();

        public:
            compiled_schema();
            ~compiled_schema();

            compiled_schema(const compiled_schema &) = default;
            compiled_schema &operator=(const compiled_schema &) = default;
            compiled_schema(compiled_schema &&) noexcept = default;
            compiled_schema &operator=(compiled_schema &&) noexcept = default;

        public:
            /// <summary>
            /// Parses the text and checks every keyword recursively. INVALID_ARGS names the offending
            /// pointer ("/properties/count/type") for an unknown keyword, a non-object schema, a type name
            /// outside the subset or an ill-typed keyword value.
            /// </summary>
            [[nodiscard]] static core::result<compiled_schema> compile(std::string_view schemaJson);

            /// <summary>
            /// Checks a value against the schema. INVALID_ARGS whose message is "&lt;path&gt;: &lt;problem&gt;"
            /// with the path rooted at rootName ("args.count: expected integer").
            /// </summary>
            [[nodiscard]] core::result<void> validate(
              const core::json::value &instance,
              std::string_view rootName = "args"
            ) const;

            /// <summary>
            /// The schema document.
            /// </summary>
            [[nodiscard]] const core::json::document &schema() const noexcept {
                return mSchema;
            }
    };

    /// <summary>
    /// compile() then validate(): for callers that validate once.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> validate_args(
      std::string_view schemaJson,
      const core::json::value &args
    );

    /// <summary>
    /// compile() alone, for callers that only need the check.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<compiled_schema> compile_schema(std::string_view schemaJson);
}
