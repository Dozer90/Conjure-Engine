# conjure_scripting - the .NET host, the native/managed ABI tables, script components, hot reload and the
# managed build runner (docs/design/03_scripting.md). Depends publicly on conjure::core, conjure::ecs and
# conjure::assets (never on scene, R29); the .NET hosting headers and static libnethost are private.

if(NOT CONJURE_WITH_SCRIPTING)
    message(STATUS "scripting: CONJURE_WITH_SCRIPTING is OFF, module skipped")
    return()
endif()

if(NOT TARGET conjure::ecs OR NOT TARGET conjure::assets)
    message(STATUS "scripting: conjure::ecs or conjure::assets is not available, module skipped")
    return()
endif()

set(CONJURE_SCRIPTING_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/dotnet_script_host.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/hot_reload.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/log_category.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/managed_build.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/native_api.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/native_api_hash.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/null_script_host.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/script_class_registry.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/script_component.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/script_fields.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/script_host.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/script_runtime.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/script_system.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/scripting.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/scripting_error.h
)

# The X-macro table is an input of native_api.h, not a header that compiles alone (so it stays out of
# PUBLIC_HEADERS, which conjure_header_check compiles one by one).
set(CONJURE_SCRIPTING_TABLE ${PROJECT_SOURCE_DIR}/engine/include/conjure/scripting/native_api.inl)
set_source_files_properties(${CONJURE_SCRIPTING_TABLE} PROPERTIES HEADER_FILE_ONLY TRUE)

# How nethost is linked. The static libnethost.lib the host pack ships is built against the static release
# CRT (/MT, _ITERATOR_DEBUG_LEVEL 0); linking it into the engine's /MD and /MDd objects is an LNK2038
# mismatch on MSVC. There the module links the nethost import library and the matching nethost.dll is copied
# next to the executables. Other platforms link libnethost.a statically through conjure::thirdparty::nethost.
set(CONJURE_SCRIPTING_NETHOST_DEPS conjure::thirdparty::nethost)
if(MSVC)
    # NetHost::NetHost is imported into third_party's directory scope; re-running the find module here
    # yields the same pack (its variables are what this directory needs).
    find_package(NetHost REQUIRED)
    get_filename_component(_nethost_native "${NetHost_LIBRARY}" DIRECTORY)
    set(CONJURE_SCRIPTING_NETHOST_IMPORT_LIBRARY "${_nethost_native}/nethost.lib")
    set(CONJURE_SCRIPTING_NETHOST_DLL "${_nethost_native}/nethost.dll")
    if(NOT EXISTS "${CONJURE_SCRIPTING_NETHOST_IMPORT_LIBRARY}" OR NOT EXISTS "${CONJURE_SCRIPTING_NETHOST_DLL}")
        message(FATAL_ERROR "scripting: nethost.lib / nethost.dll not found beside ${NetHost_LIBRARY}")
    endif()
    add_library(conjure_scripting_nethost_shared INTERFACE)
    target_include_directories(conjure_scripting_nethost_shared SYSTEM INTERFACE "${_nethost_native}")
    target_link_libraries(conjure_scripting_nethost_shared INTERFACE "${CONJURE_SCRIPTING_NETHOST_IMPORT_LIBRARY}")
    set(CONJURE_SCRIPTING_NETHOST_DEPS conjure_scripting_nethost_shared)
    # Executables land in bin (CMakeLists.txt) plus a per-configuration folder on multi-config generators.
    if(CMAKE_CONFIGURATION_TYPES)
        set(CONJURE_SCRIPTING_RUNTIME_DIR "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
    else()
        set(CONJURE_SCRIPTING_RUNTIME_DIR "${PROJECT_BINARY_DIR}/bin")
    endif()
endif()

conjure_add_module(scripting
    PUBLIC_HEADERS
        ${CONJURE_SCRIPTING_PUBLIC_HEADERS}
    SOURCES
        ${CONJURE_SCRIPTING_TABLE}
        dotnet_script_host.cpp
        hostfxr_api.h
        hostfxr_runtime.cpp
        hostfxr_runtime.h
        hot_reload.cpp
        log_category.cpp
        managed_build.cpp
        native_api.cpp
        native_api_core.cpp
        null_script_host.cpp
        script_class_registry.cpp
        script_component.cpp
        script_fields.cpp
        script_host.cpp
        script_runtime.cpp
        script_system.cpp
        scripting_error.cpp
    PUBLIC_DEPS
        conjure::core
        conjure::ecs
        conjure::assets
    PRIVATE_DEPS
        ${CONJURE_SCRIPTING_NETHOST_DEPS}
)

if(MSVC)
    # The MSVC build links the nethost import library (see the note above conjure_add_module), so the
    # executables need nethost.dll beside them. The copy is a declared output: a removed bin/ is repopulated
    # even while conjure_scripting is up to date, and anything that needs the DLL at run time (the test
    # executables through conjure_scripting, the export layout of 06_engine_loop.md 3.6) can depend on
    # conjure_scripting_nethost.
    set(CONJURE_SCRIPTING_NETHOST_COPY "${CONJURE_SCRIPTING_RUNTIME_DIR}/nethost.dll")
    add_custom_command(
        OUTPUT "${CONJURE_SCRIPTING_NETHOST_COPY}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CONJURE_SCRIPTING_NETHOST_DLL}" "${CONJURE_SCRIPTING_NETHOST_COPY}"
        DEPENDS "${CONJURE_SCRIPTING_NETHOST_DLL}"
        COMMENT "scripting: copying nethost.dll next to the executables"
        VERBATIM
    )
    add_custom_target(conjure_scripting_nethost DEPENDS "${CONJURE_SCRIPTING_NETHOST_COPY}")
    set_target_properties(conjure_scripting_nethost PROPERTIES FOLDER "Engine")
    add_dependencies(conjure_scripting conjure_scripting_nethost)
endif()
