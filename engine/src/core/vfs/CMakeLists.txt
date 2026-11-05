# core/vfs - the virtual file system: file_system interface, native and in-memory backends,
# scheme-based mount table and lexical path helpers.

set(CONJURE_VFS_PLATFORM_SOURCES native_platform_posix.cpp)
if(WIN32)
    set(CONJURE_VFS_PLATFORM_SOURCES native_platform_windows.cpp)
endif()

conjure_add_core_area(vfs
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/vfs/file_system.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/vfs/memory_file_system.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/vfs/mount_table.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/vfs/native_file_system.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/vfs/path_utils.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/vfs/vfs.h
    SOURCES
        file_system.cpp
        memory_file_system.cpp
        mount_table.cpp
        native_file_system.cpp
        native_platform.cpp
        native_platform.h
        path_utils.cpp
        vfs_internal.cpp
        vfs_internal.h
        ${CONJURE_VFS_PLATFORM_SOURCES}
    PUBLIC_DEPS
        conjure::core_foundation
)
