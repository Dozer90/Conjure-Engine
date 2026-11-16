# core/system - child processes, shared libraries and RGBA8 images (PNG through stb).
#
# Links core/jobs privately: the pipe readers of a child process are jobs::dedicated_threads, so this
# area creates no std::thread of its own (docs/design/10_core.md C2 and C4).
#
# The platform backends are selected here rather than with preprocessor guards so each file is a
# plain translation unit of one operating system.

set(CONJURE_CORE_SYSTEM_SOURCES
    dynamic_library.cpp
    image.cpp
    process.cpp
    system_internal.cpp
    system_internal.h
)

if(WIN32)
    list(APPEND CONJURE_CORE_SYSTEM_SOURCES
        dynamic_library_windows.cpp
        process_windows.cpp
        win32_utils.cpp
        win32_utils.h
    )
else()
    list(APPEND CONJURE_CORE_SYSTEM_SOURCES
        dynamic_library_posix.cpp
        process_posix.cpp
    )
endif()

conjure_add_core_area(system
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/dynamic_library.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/image.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/process.h
    SOURCES
        ${CONJURE_CORE_SYSTEM_SOURCES}
    PUBLIC_DEPS
        conjure::core_foundation
    PRIVATE_DEPS
        conjure::core_jobs
        conjure::thirdparty::stb
)
