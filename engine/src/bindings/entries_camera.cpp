#include "bindings/entry_support.h"

// camera_* (wave 3 GAP-CSHARP 2.2): the main camera and screen-to-world picking. The group is in the version 3
// table and compiled in with the renderer, but this build does not implement its entries yet: each answers
// NOT_SUPPORTED (camera_main returns 0, the null entity, and records the same error), which Conjure.Managed
// reports as NotSupportedException. Built only with CONJURE_WITH_RENDERER; the stubs cover the group otherwise.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    constexpr std::string_view NOT_IMPLEMENTED = "not implemented in this build";
}

uint64_t conjure::bindings::entries::camera_main() {
    constexpr native_function_id ID = native_function_id::camera_main;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    static_cast<void>(fail_entry(ID, bindings_error::NOT_SUPPORTED, NOT_IMPLEMENTED));
    return 0;
}

int32_t conjure::bindings::entries::camera_screen_ray(uint64_t, const conjure_vec2 *, conjure_vec3 *, conjure_vec3 *) {
    constexpr native_function_id ID = native_function_id::camera_screen_ray;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return fail_entry(ID, bindings_error::NOT_SUPPORTED, NOT_IMPLEMENTED);
}

int32_t conjure::bindings::entries::camera_screen_to_world(uint64_t, const conjure_vec3 *, conjure_vec3 *) {
    constexpr native_function_id ID = native_function_id::camera_screen_to_world;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return fail_entry(ID, bindings_error::NOT_SUPPORTED, NOT_IMPLEMENTED);
}

int32_t conjure::bindings::entries::camera_world_to_screen(uint64_t, const conjure_vec3 *, conjure_vec3 *) {
    constexpr native_function_id ID = native_function_id::camera_world_to_screen;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return fail_entry(ID, bindings_error::NOT_SUPPORTED, NOT_IMPLEMENTED);
}
