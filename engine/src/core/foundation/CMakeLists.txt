# core/foundation - the part of core every other core area builds on.
#
# The platform backends are selected here rather than with preprocessor guards so each file is a
# plain translation unit of one operating system.

set(CONJURE_CORE_FOUNDATION_SOURCES
    assert.cpp
    console_output.h
    error.cpp
    local_time.h
    log.cpp
    uuid.cpp
)

if(WIN32)
    list(APPEND CONJURE_CORE_FOUNDATION_SOURCES
        local_time_windows.cpp
    )
else()
    list(APPEND CONJURE_CORE_FOUNDATION_SOURCES
        local_time_posix.cpp
    )
endif()

conjure_add_core_area(foundation
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/api.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/assert.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/enum_flags.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/error.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/hash.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/log.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/platform_detect.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/result.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/types.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/uuid.h
    SOURCES
        ${CONJURE_CORE_FOUNDATION_SOURCES}
)

find_package(Threads REQUIRED)
target_link_libraries(conjure_core_foundation PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
