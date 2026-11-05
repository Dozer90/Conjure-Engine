# core/strings - hashed string identifiers, ASCII/UTF-8 string helpers, bit_vector and radix sort.

conjure_add_core_area(strings
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/bit_vector.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/radix_sort.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/string_id.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/string_utils.h
    SOURCES
        bit_vector.cpp
        radix_sort.cpp
        string_id.cpp
        string_utils.cpp
    PUBLIC_DEPS
        conjure::core_foundation
)
