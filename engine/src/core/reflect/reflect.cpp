#include "conjure/core/reflect/reflect.h"

#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/type_info.h"

#include "conjure/core/assert.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

void conjure::core::reflect::detail::finish_struct_description(reflect::type_info &info) {
    const result<void> valid = validate_layout(info);
    if (!valid.has_value()) {
        CONJURE_LOG_ERROR(LOG_CORE, "reflect: invalid description: {}", valid.get_error());
        CONJURE_ASSERT(false, "CONJURE_REFLECT_STRUCT: {}", valid.get_error().message());
    }
}

void conjure::core::reflect::detail::fill_default_values(reflect::type_info &info, const void *defaults) {
    const std::byte *base = static_cast<const std::byte *>(defaults);
    for (field_info &field : info.fields) {
        // The field_ref points at the default instance; a const_cast is needed only because field_ref
        // is also used for writes. write_json() never modifies the data.
        const field_ref ref = {const_cast<std::byte *>(base + field.offset), &field, false};
        json::writer out = {};
        const result<void> written = write_json(ref, out);
        if (!written.has_value()) {
            // A custom kind of unsupported size has no textual default; leave it empty.
            field.defaultValueJson.clear();
            continue;
        }

        // The writer emits nested object members in declaration order; the default is documented as
        // canonical (sorted keys) so it compares equal to to_canonical_string() of the same value.
        std::string text = out.finish();
        const result<json::document> parsed = json::document::parse(text);
        if (parsed.has_value()) {
            field.defaultValueJson = parsed->to_canonical_string();
        } else {
            field.defaultValueJson = std::move(text);
        }
    }
}
