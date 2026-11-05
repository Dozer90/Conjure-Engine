# core/reflect - type ids, struct/enum descriptions, the type registry and type_info-driven JSON.
#
# core_time is linked for event<>, which type_registry::on_registered() returns; time depends only on
# foundation so no cycle is introduced.

conjure_add_core_area(reflect
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/reflect/field_info.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/reflect/json_conversion.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/reflect/reflect.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/reflect/type_id.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/reflect/type_info.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/reflect/type_registry.h
    SOURCES
        field_info.cpp
        json_conversion.cpp
        pointer_index.cpp
        pointer_index.h
        reflect.cpp
        type_info.cpp
        type_registry.cpp
    PUBLIC_DEPS
        conjure::core_foundation
        conjure::core_json
        conjure::core_strings
        conjure::core_math
        conjure::core_containers
        conjure::core_time
)
