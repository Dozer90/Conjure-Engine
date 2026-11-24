#include "bindings/entry_support.h"

// entity_spawn (wave 3 GAP-CSHARP 2.3): many entities with their components in one call. The entry is in the
// version 3 table, but this build does not implement it yet: it answers NOT_SUPPORTED, which Conjure.Managed
// reports as NotSupportedException.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

int32_t conjure::bindings::entries::entity_spawn(const conjure_spawn_desc *, uint64_t *) {
    constexpr native_function_id ID = native_function_id::entity_spawn;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return fail_entry(ID, bindings_error::NOT_SUPPORTED, "not implemented in this build");
}
