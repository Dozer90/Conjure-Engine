# conjure_core - foundation used by every other module.
#
# Wave 1 adds: memory, containers, jobs, events, filesystem, json, reflection, math, time,
# string utilities, hashing. Keep this list sorted by sub-area.

conjure_add_module(core
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/assert.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/enum_flags.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/error.h
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
    PUBLIC_DEPS
        conjure::thirdparty::glm
)

find_package(Threads REQUIRED)
target_link_libraries(conjure_core PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
