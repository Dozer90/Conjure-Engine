# conjure_core - foundation used by every other module.
#
# Core is split into areas so they can be developed and tested independently. Each area is a static
# library conjure_core_<area> (alias conjure::core_<area>) declared in its own directory with
# conjure_add_core_area(); the umbrella conjure::core links them all and is what other modules use.
# Area order matters only for the dependencies declared inside each area's CMakeLists.txt.
#
# The area list below is the complete plan from docs/design/10_core.md. Every listed area must have its
# directory: a missing CMakeLists.txt stops the configure instead of surfacing later as unrelated link errors.

set(CONJURE_CORE_AREAS
    foundation      # platform_detect, types, api, build_info, hash, error/result, assert, log, uuid, enum_flags
    math            # glm aliases, transform, aabb, frustum, ray, rect
    containers      # slot_map, small_vector, stable_vector, flat_hash_map, flat_map
    strings         # string_id, string utilities, bit_vector, radix_sort
    memory          # arena, pool_allocator, frame_allocator, allocation_counters, tracked allocation
    time            # clock, steady_clock, manual_clock, sleep_until, random, event, delegate, event_bus
    jobs            # job_system, parallel_for, main-thread queue, dedicated_thread
    json            # yyjson wrapper: value, document, writer, parse, canonical to_string
    vfs             # file_system interface, native and memory file systems, mounts
    system          # process, dynamic_library, image
    reflect         # type_id, type_info, field_info, type_registry, json conversion
)

# Generated headers (build_info.h) live here and are visible to every area through the umbrella.
set(CONJURE_GENERATED_INCLUDE_DIR "${PROJECT_BINARY_DIR}/generated/include")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/foundation/build_info.h.in"
    "${CONJURE_GENERATED_INCLUDE_DIR}/conjure/core/build_info.h"
    @ONLY
)

add_library(conjure_core INTERFACE)
add_library(conjure::core ALIAS conjure_core)
target_include_directories(conjure_core INTERFACE
    "${PROJECT_SOURCE_DIR}/engine/include"
    "${CONJURE_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(conjure_core INTERFACE conjure::platform_defines)

foreach(area IN LISTS CONJURE_CORE_AREAS)
    set(areaDir "${CMAKE_CURRENT_SOURCE_DIR}/${area}")
    if(NOT EXISTS "${areaDir}/CMakeLists.txt")
        message(FATAL_ERROR "core: CONJURE_CORE_AREAS lists '${area}' but ${areaDir}/CMakeLists.txt does not exist")
    endif()
    add_subdirectory("${areaDir}")
    target_link_libraries(conjure_core INTERFACE conjure::core_${area})
endforeach()
