# The umbrella shared library conjure_shared and the ABI identity header (docs/design/07_modules.md
# D1, 3.8 and 5; docs/ARCHITECTURE.md 2.1).
#
#   conjure_finalize_engine()
#
#     Called once by the root CMakeLists.txt right after add_subdirectory(engine), so every engine
#     module has been declared (the function is idempotent). It creates:
#
#       conjure_abi_identity   custom target writing ${PROJECT_BINARY_DIR}/generated/include/conjure/
#                              modules/abi_identity.h: CONJURE_ABI_HEADER_HASH (hash over every public
#                              header of every engine module, recomputed at build time whenever one
#                              changes), CONJURE_ABI_COMPILER_MAJOR and the runtime facts
#                              CONJURE_ABI_RUNTIME_DYNAMIC_CRT / _ASAN / _UBSAN of the engine build.
#                              conjure_modules and conjure_shared depend on it.
#       conjure_shared         SHARED library (alias conjure::shared) whole-archiving every static
#                              library in the global property CONJURE_STATIC_LIBRARIES, so one copy of
#                              every engine symbol and global exists in the process. Executables and
#                              game modules link conjure::shared and nothing else from the engine.
#                              Compiles with CONJURE_SHARED_EXPORTS; consumers get CONJURE_SHARED_IMPORTS,
#                              the engine include directories, the interface compile definitions of every
#                              engine library, conjure::platform_defines and glm (/wd4251 /wd4275 come from
#                              conjure::warnings, which every target already carries).
#
#     Symbol export: on MSVC a module definition file listing every external symbol of the whole-
#     archived objects is generated at build time (the same mechanism as CMake's
#     WINDOWS_EXPORT_ALL_SYMBOLS, which does not look inside static libraries). Everything the static
#     libraries define is therefore exported, whether or not it carries CONJURE_API; CONJURE_API on a
#     class or function (the modules library marks every out-of-line public symbol) adds the dllimport
#     side so consumers call through the import thunk directly and the exported data symbols resolve.
#     GCC and Clang export every default-visibility symbol, the compiler default in this tree.
#
#     A DLL may hold at most 65535 exports. Exporting every symbol of every static library spends
#     that budget quickly (26,739 with the 17 wave-1/2 libraries), so the .def generation is followed
#     by a count that fails the build with a message naming the remedy (CONJURE_API-marked exports,
#     07_modules.md 5) instead of letting the linker fail with LNK1189.
#
#   Script mode (cmake -P ConjureSharedLibrary.cmake -D...): writes abi_identity.h, or checks the
#   export count of a generated .def file. Used only by the custom commands below.

include_guard(GLOBAL)

#------------------------------------------------------------------------------
# Script mode: check the export count of a module definition file
#------------------------------------------------------------------------------
if(CMAKE_SCRIPT_MODE_FILE AND DEFINED CONJURE_EXPORT_COUNT_CHECK)
    set(exportLimit 65535)
    file(STRINGS "${CONJURE_EXPORT_COUNT_CHECK}" exportLines REGEX "^[ \t]+")
    list(LENGTH exportLines exportCount)
    math(EXPR warningThreshold "${exportLimit} * 4 / 5")
    if(exportCount GREATER exportLimit)
        message(FATAL_ERROR
            "conjure_shared exports ${exportCount} symbols; a Windows DLL may export at most ${exportLimit}. "
            "The umbrella whole-archives every engine library and exports every symbol they define "
            "(cmake/ConjureSharedLibrary.cmake, docs/design/07_modules.md D1 and 5): switch to CONJURE_API-marked "
            "exports (CONJURE_SHARED_EXPORTS on the static libraries, hidden visibility in conjure_add_module) "
            "or trim the exported surface before adding more libraries."
        )
    elseif(exportCount GREATER warningThreshold)
        message(WARNING
            "conjure_shared exports ${exportCount} of the ${exportLimit} symbols a Windows DLL may hold; plan the "
            "switch to CONJURE_API-marked exports (docs/design/07_modules.md 5) before the limit is reached."
        )
    else()
        message(STATUS "conjure_shared exports ${exportCount} symbols (limit ${exportLimit})")
    endif()
    return()
endif()

#------------------------------------------------------------------------------
# Script mode: generate abi_identity.h
#------------------------------------------------------------------------------
if(CMAKE_SCRIPT_MODE_FILE AND DEFINED CONJURE_ABI_IDENTITY_OUTPUT)
    file(STRINGS "${CONJURE_ABI_IDENTITY_HEADER_LIST}" abiHeaders)
    set(digests "")
    foreach(header IN LISTS abiHeaders)
        file(RELATIVE_PATH relative "${CONJURE_ABI_IDENTITY_INCLUDE_ROOT}" "${header}")
        file(SHA256 "${header}" digest)
        string(APPEND digests "${relative}\n${digest}\n")
    endforeach()
    string(SHA256 combined "${digests}")
    string(SUBSTRING "${combined}" 0 16 headerHash)
    string(TOUPPER "${headerHash}" headerHash)

    set(content "#pragma once\n\n")
    string(APPEND content
        "// Generated by cmake/ConjureSharedLibrary.cmake. Do not edit; it is rewritten whenever a public\n"
        "// engine header changes. See docs/design/07_modules.md 3.8 and engine/src/modules/README.md.\n\n"
    )
    string(APPEND content "#define CONJURE_ABI_HEADER_HASH 0x${headerHash}ull\n")
    string(APPEND content "#define CONJURE_ABI_COMPILER_MAJOR ${CONJURE_ABI_IDENTITY_COMPILER_MAJOR}u\n")
    string(APPEND content "#define CONJURE_ABI_RUNTIME_DYNAMIC_CRT ${CONJURE_ABI_IDENTITY_DYNAMIC_CRT}\n")
    string(APPEND content "#define CONJURE_ABI_RUNTIME_ASAN ${CONJURE_ABI_IDENTITY_ASAN}\n")
    string(APPEND content "#define CONJURE_ABI_RUNTIME_UBSAN ${CONJURE_ABI_IDENTITY_UBSAN}\n")

    # Only touch the output when it changed so dependents are not recompiled after every build.
    set(existing "")
    if(EXISTS "${CONJURE_ABI_IDENTITY_OUTPUT}")
        file(READ "${CONJURE_ABI_IDENTITY_OUTPUT}" existing)
    endif()
    if(NOT existing STREQUAL content)
        get_filename_component(outputDirectory "${CONJURE_ABI_IDENTITY_OUTPUT}" DIRECTORY)
        file(MAKE_DIRECTORY "${outputDirectory}")
        file(WRITE "${CONJURE_ABI_IDENTITY_OUTPUT}" "${content}")
    endif()
    return()
endif()

#------------------------------------------------------------------------------
# Host facts shared with conjure_add_game_module (ConjureModule.cmake)
#------------------------------------------------------------------------------
if(WIN32)
    set(CONJURE_HOST_PLATFORM_NAME "windows")
elseif(APPLE)
    set(CONJURE_HOST_PLATFORM_NAME "macos")
else()
    set(CONJURE_HOST_PLATFORM_NAME "linux")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    set(CONJURE_HOST_ARCH_NAME "arm64")
else()
    set(CONJURE_HOST_ARCH_NAME "x64")
endif()


function(conjure_abi_runtime_facts dynamicCrtVariable asanVariable ubsanVariable)
    # The engine build's runtime facts (07_modules.md 2.2), each 0 or 1. MSVC defaults to /MD; GCC
    # and Clang link the dynamic standard library unless the toolchain says otherwise.
    set(dynamicCrt 1)
    if(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
        set(dynamicCrt 0)
    endif()
    set(asan 0)
    if(CONJURE_ENABLE_ASAN)
        set(asan 1)
    endif()
    set(ubsan 0)
    if(CONJURE_ENABLE_UBSAN)
        set(ubsan 1)
    endif()
    set(${dynamicCrtVariable} "${dynamicCrt}" PARENT_SCOPE)
    set(${asanVariable} "${asan}" PARENT_SCOPE)
    set(${ubsanVariable} "${ubsan}" PARENT_SCOPE)
endfunction()


function(conjure_add_abi_identity libraries)
    set(generatedRoot "${PROJECT_BINARY_DIR}/generated/include")
    set(output "${generatedRoot}/conjure/modules/abi_identity.h")
    set(stagingDirectory "${PROJECT_BINARY_DIR}/generated/abi_identity")

    set(headers "")
    foreach(library IN LISTS libraries)
        get_target_property(publicHeaders ${library} CONJURE_PUBLIC_HEADERS)
        if(publicHeaders)
            list(APPEND headers ${publicHeaders})
        endif()
    endforeach()
    list(SORT headers)
    list(REMOVE_DUPLICATES headers)

    # The list goes through a staging file so a configure that changes nothing does not retrigger
    # the hash (configure_file COPYONLY only rewrites when the content differs).
    set(listFile "${stagingDirectory}/headers.txt")
    string(REPLACE ";" "\n" listContent "${headers}")
    file(WRITE "${listFile}.in" "${listContent}\n")
    configure_file("${listFile}.in" "${listFile}" COPYONLY)

    string(REGEX MATCH "^[0-9]+" compilerMajor "${CMAKE_CXX_COMPILER_VERSION}")
    if(NOT compilerMajor)
        set(compilerMajor 0)
    endif()
    conjure_abi_runtime_facts(dynamicCrt asan ubsan)

    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}"
            -DCONJURE_ABI_IDENTITY_OUTPUT=${output}
            -DCONJURE_ABI_IDENTITY_HEADER_LIST=${listFile}
            -DCONJURE_ABI_IDENTITY_INCLUDE_ROOT=${PROJECT_SOURCE_DIR}/engine/include
            -DCONJURE_ABI_IDENTITY_COMPILER_MAJOR=${compilerMajor}
            -DCONJURE_ABI_IDENTITY_DYNAMIC_CRT=${dynamicCrt}
            -DCONJURE_ABI_IDENTITY_ASAN=${asan}
            -DCONJURE_ABI_IDENTITY_UBSAN=${ubsan}
            -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
        DEPENDS ${headers} "${listFile}" "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
        COMMENT "Generating conjure/modules/abi_identity.h"
        VERBATIM
    )
    add_custom_target(conjure_abi_identity DEPENDS "${output}")
    set_target_properties(conjure_abi_identity PROPERTIES FOLDER "Engine")
endfunction()


function(conjure_finalize_engine)
    if(TARGET conjure_shared)
        return()
    endif()

    get_property(libraries GLOBAL PROPERTY CONJURE_STATIC_LIBRARIES)
    if(NOT libraries)
        message(FATAL_ERROR
            "conjure_finalize_engine: no engine library registered with conjure_register_static_library"
        )
    endif()

    conjure_add_abi_identity("${libraries}")
    if(TARGET conjure_modules)
        add_dependencies(conjure_modules conjure_abi_identity)
    endif()

    # The library needs one source of its own; everything else arrives through whole-archive links.
    set(stubDirectory "${PROJECT_BINARY_DIR}/generated/conjure_shared")
    set(stub "${stubDirectory}/conjure_shared.cpp")
    file(WRITE "${stub}.in"
        "// Generated by cmake/ConjureSharedLibrary.cmake: conjure_shared is assembled from the engine's\n"
        "// static libraries and has no code of its own.\n"
    )
    configure_file("${stub}.in" "${stub}" COPYONLY)

    add_library(conjure_shared SHARED "${stub}")
    add_library(conjure::shared ALIAS conjure_shared)
    add_dependencies(conjure_shared conjure_abi_identity)

    foreach(library IN LISTS libraries)
        target_link_libraries(conjure_shared PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,${library}>")
        # Transitive plain links to the same library (module -> core) must agree with the feature.
        set_property(TARGET conjure_shared PROPERTY "LINK_LIBRARY_OVERRIDE_${library}" WHOLE_ARCHIVE)
        target_compile_definitions(conjure_shared
            INTERFACE "$<TARGET_PROPERTY:${library},INTERFACE_COMPILE_DEFINITIONS>"
        )
    endforeach()

    target_include_directories(conjure_shared
        PUBLIC
            "${PROJECT_SOURCE_DIR}/engine/include"
            "${PROJECT_BINARY_DIR}/generated/include"
    )
    target_link_libraries(conjure_shared
        PUBLIC
            conjure::platform_defines
        PRIVATE
            conjure::warnings
            conjure::sanitizers
    )
    if(TARGET conjure::thirdparty::glm)
        target_link_libraries(conjure_shared PUBLIC conjure::thirdparty::glm)
    endif()
    target_compile_definitions(conjure_shared
        PRIVATE CONJURE_SHARED_EXPORTS
        INTERFACE CONJURE_SHARED_IMPORTS
    )

    if(MSVC)
        set(definitionFile "${stubDirectory}/exports.def")
        set(objectList "${stubDirectory}/exports.def.objs")
        set(objects "")
        foreach(library IN LISTS libraries)
            list(APPEND objects "$<TARGET_OBJECTS:${library}>")
        endforeach()
        file(GENERATE OUTPUT "${objectList}" CONTENT "$<JOIN:${objects},\n>\n")
        # The count runs right after the generation (see the header comment): a table past the DLL
        # limit fails here with the remedy instead of in the linker with LNK1189.
        add_custom_command(
            OUTPUT "${definitionFile}"
            COMMAND "${CMAKE_COMMAND}" -E __create_def "${definitionFile}" "${objectList}"
            COMMAND "${CMAKE_COMMAND}"
                -DCONJURE_EXPORT_COUNT_CHECK=${definitionFile}
                -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
            DEPENDS ${objects} "${objectList}" "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
            COMMENT "Generating and checking the conjure_shared export table"
            VERBATIM
        )
        target_sources(conjure_shared PRIVATE "${definitionFile}")
    endif()

    set_target_properties(conjure_shared PROPERTIES
        FOLDER "Engine"
        OUTPUT_NAME "conjure_shared"
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
        BUILD_RPATH_USE_ORIGIN TRUE
    )
    if(APPLE)
        set_target_properties(conjure_shared PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path"
            INSTALL_NAME_DIR "@rpath"
            BUILD_WITH_INSTALL_NAME_DIR TRUE
        )
    endif()
endfunction()
