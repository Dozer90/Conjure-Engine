# core/foundation - the part of core every other core area builds on.

conjure_add_core_area(foundation
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/api.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/assert.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/enum_flags.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/error.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/hash.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/log.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/non_copyable.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/platform_detect.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/result.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/types.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/uuid.h
    SOURCES
        assert.cpp
        error.cpp
        log.cpp
        uuid.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(conjure_core_foundation PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
