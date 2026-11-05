# core/containers - slot_map, small_vector, stable_vector, flat_hash_map/set, flat_map.

conjure_add_core_area(containers
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/containers.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/detail.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/flat_hash_map.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/flat_map.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/slot_key_format.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/slot_map.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/small_vector.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/containers/stable_vector.h
    SOURCES
        containers.cpp
    PUBLIC_DEPS
        conjure::core_foundation
)
