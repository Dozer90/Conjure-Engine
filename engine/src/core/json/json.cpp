#include "conjure/core/json/json.h"

#include "core/json/json_internal.h"

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::json;

/*===========================================================================*/
/*===========================================================================*/
// Free functions

result<document> conjure::core::json::parse(std::string_view text) {
    return document::parse(text);
}

std::string conjure::core::json::to_string(const value &source, bool canonical) {
    yyjson_mut_val *node = detail::as_node(detail::value_access::node(source));
    if (node == nullptr) {
        return {};
    }
    if (canonical) {
        return detail::write_canonical(node);
    }
    return detail::write_node(node, YYJSON_WRITE_NOFLAG);
}
