# conjure_modules - manifests, ABI check, load/unload/reload, registration records and consumers
# (docs/design/07_modules.md). Depends on conjure::core only.
#
# The umbrella shared library conjure_shared and the generated abi_identity.h that module_abi.h
# includes come from conjure_finalise_engine() (cmake/ConjureSharedLibrary.cmake), which the root
# CMakeLists.txt calls right after add_subdirectory(engine); it also adds the conjure_abi_identity
# dependency to this target.

set(CONJURE_MODULES_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_abi.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_consumer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_context.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_entry.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_error.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_events.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_manager.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_manifest.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_registrar.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_registration_set.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_registry.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_resolver.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_service.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_types.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/module_version.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/modules/modules.h
)

# The long-path helpers (long_path.h) have one backend per platform family.
set(CONJURE_MODULES_PLATFORM_SOURCES long_path_posix.cpp)
if(WIN32)
    set(CONJURE_MODULES_PLATFORM_SOURCES long_path_windows.cpp)
endif()

conjure_add_module(modules
    PUBLIC_HEADERS
        ${CONJURE_MODULES_PUBLIC_HEADERS}
    SOURCES
        log_category.h
        log_category.cpp
        long_path.h
        ${CONJURE_MODULES_PLATFORM_SOURCES}
        manifest_paths.h
        module_abi.cpp
        module_consumer.cpp
        module_context.cpp
        module_error.cpp
        module_loader.h
        module_loader.cpp
        module_manager.cpp
        module_manifest.cpp
        module_record.h
        module_registrar.cpp
        module_registry.cpp
        module_resolver.cpp
        module_service.cpp
        module_types.cpp
        module_version.cpp
        shadow_copy.h
        shadow_copy.cpp
    PUBLIC_DEPS
        conjure::core
)
