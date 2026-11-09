# Helpers for declaring Conjure engine modules, executables and test modules.
#
#   conjure_add_module(<name>
#       SOURCES         <files...>          private sources and headers under engine/src/<name>
#       PUBLIC_HEADERS  <files...>          headers under engine/include/conjure/<name>
#       PUBLIC_DEPS     <targets...>        dependencies that leak into the public headers
#       PRIVATE_DEPS    <targets...>        dependencies used only in .cpp files
#       PUBLIC_DEFINES  <defs...>
#       PRIVATE_DEFINES <defs...>
#   )
#
# Creates the static library conjure_<name> with alias conjure::<name>.
#
#   conjure_add_executable(<name> [WIN32_GUI] SOURCES ... DEPS ...)
#
# Test modules are declared with conjure_add_test_module() from cmake/ConjureTesting.cmake.
#
# Every target gets the shared warning, sanitizer and platform-define interface targets. Library
# targets record their PUBLIC_HEADERS in the target property CONJURE_PUBLIC_HEADERS, which
# conjure_header_check() (ConjureTesting.cmake) compiles one translation unit at a time.

function(conjure_add_module name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES PUBLIC_HEADERS PUBLIC_DEPS PRIVATE_DEPS PUBLIC_DEFINES PRIVATE_DEFINES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_module(${name}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "conjure_add_module(${name}): SOURCES is required")
    endif()

    set(target "conjure_${name}")
    add_library(${target} STATIC ${ARG_SOURCES} ${ARG_PUBLIC_HEADERS})
    add_library(conjure::${name} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC
            "${PROJECT_SOURCE_DIR}/engine/include"
            "${PROJECT_BINARY_DIR}/generated/include"
        PRIVATE
            "${PROJECT_SOURCE_DIR}/engine/src"
    )

    target_link_libraries(${target}
        PUBLIC
            conjure::platform_defines
            ${ARG_PUBLIC_DEPS}
        PRIVATE
            conjure::warnings
            conjure::sanitizers
            ${ARG_PRIVATE_DEPS}
    )

    target_compile_definitions(${target}
        PUBLIC ${ARG_PUBLIC_DEFINES}
        PRIVATE CONJURE_MODULE_NAME="${name}" ${ARG_PRIVATE_DEFINES}
    )

    set_target_properties(${target} PROPERTIES
        FOLDER "Engine"
        OUTPUT_NAME "conjure_${name}"
    )

    set_property(TARGET ${target} PROPERTY CONJURE_PUBLIC_HEADERS "${ARG_PUBLIC_HEADERS}")

    conjure_register_static_library(${target})
    source_group(TREE "${PROJECT_SOURCE_DIR}/engine" FILES ${ARG_SOURCES} ${ARG_PUBLIC_HEADERS})
endfunction()


# Registers a static library in the global list consumed by the umbrella shared library
# (conjure_shared whole-archives every entry) and by header checks.
function(conjure_register_static_library target)
    set_property(GLOBAL APPEND PROPERTY CONJURE_STATIC_LIBRARIES "${target}")
endfunction()


# Declares one area of the core module as the static library conjure_core_<area> with alias
# conjure::core_<area>. Same arguments as conjure_add_module(). Areas depend on each other through
# PUBLIC_DEPS (e.g. conjure::core_foundation); every area implicitly depends on nothing else, so
# list what you use.
function(conjure_add_core_area area)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES PUBLIC_HEADERS PUBLIC_DEPS PRIVATE_DEPS PUBLIC_DEFINES PRIVATE_DEFINES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_core_area(${area}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "conjure_add_core_area(${area}): SOURCES is required")
    endif()

    set(target "conjure_core_${area}")
    add_library(${target} STATIC ${ARG_SOURCES} ${ARG_PUBLIC_HEADERS})
    add_library(conjure::core_${area} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC
            "${PROJECT_SOURCE_DIR}/engine/include"
            "${PROJECT_BINARY_DIR}/generated/include"
        PRIVATE
            "${PROJECT_SOURCE_DIR}/engine/src"
    )

    target_link_libraries(${target}
        PUBLIC
            conjure::platform_defines
            ${ARG_PUBLIC_DEPS}
        PRIVATE
            conjure::warnings
            conjure::sanitizers
            ${ARG_PRIVATE_DEPS}
    )

    target_compile_definitions(${target}
        PUBLIC ${ARG_PUBLIC_DEFINES}
        PRIVATE CONJURE_MODULE_NAME="core" ${ARG_PRIVATE_DEFINES}
    )

    set_target_properties(${target} PROPERTIES
        FOLDER "Engine/core"
        OUTPUT_NAME "conjure_core_${area}"
    )

    set_property(TARGET ${target} PROPERTY CONJURE_PUBLIC_HEADERS "${ARG_PUBLIC_HEADERS}")

    conjure_register_static_library(${target})
    source_group(TREE "${PROJECT_SOURCE_DIR}/engine" FILES ${ARG_SOURCES} ${ARG_PUBLIC_HEADERS})
endfunction()


function(conjure_add_executable name)
    set(options WIN32_GUI)
    set(oneValueArgs FOLDER)
    set(multiValueArgs SOURCES DEPS DEFINES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_executable(${name}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    add_executable(${name} ${ARG_SOURCES})

    target_link_libraries(${name}
        PRIVATE
            conjure::platform_defines
            conjure::warnings
            conjure::sanitizers
            ${ARG_DEPS}
    )
    target_compile_definitions(${name} PRIVATE ${ARG_DEFINES})

    if(NOT ARG_FOLDER)
        set(ARG_FOLDER "Executables")
    endif()
    set_target_properties(${name} PROPERTIES FOLDER "${ARG_FOLDER}")

    # GUI applications hide the console window in Distribution only; developers keep stdout.
    if(ARG_WIN32_GUI AND WIN32)
        set_target_properties(${name} PROPERTIES
            WIN32_EXECUTABLE $<CONFIG:Distribution>
        )
        if(MSVC)
            target_link_options(${name} PRIVATE
                $<$<CONFIG:Distribution>:/ENTRY:mainCRTStartup>
            )
        endif()
    endif()

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ARG_SOURCES})
endfunction()


function(conjure_print_configuration_summary)
    message(STATUS "")
    message(STATUS "Conjure ${PROJECT_VERSION} configuration")
    message(STATUS "  Build type ............ ${CMAKE_BUILD_TYPE}${CMAKE_CONFIGURATION_TYPES}")
    message(STATUS "  Compiler .............. ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "  Renderer / RHI ........ ${CONJURE_WITH_RENDERER} (D3D12=${CONJURE_RHI_D3D12}, Vulkan=${CONJURE_RHI_VULKAN})")
    message(STATUS "  Physics ............... ${CONJURE_WITH_PHYSICS}")
    message(STATUS "  Audio ................. ${CONJURE_WITH_AUDIO}")
    message(STATUS "  Scripting ............. ${CONJURE_WITH_SCRIPTING} (managed build=${CONJURE_BUILD_MANAGED})")
    message(STATUS "  Editor ................ ${CONJURE_BUILD_EDITOR}")
    message(STATUS "  Runtime ............... ${CONJURE_BUILD_RUNTIME}")
    message(STATUS "  Tests / Benchmarks .... ${CONJURE_BUILD_TESTS} / ${CONJURE_BUILD_BENCHMARKS}")
    message(STATUS "  Samples ............... ${CONJURE_BUILD_SAMPLES}")
    message(STATUS "  Warnings as errors .... ${CONJURE_WARNINGS_AS_ERRORS}")
    message(STATUS "  Sanitizers ............ ASan=${CONJURE_ENABLE_ASAN} UBSan=${CONJURE_ENABLE_UBSAN} TSan=${CONJURE_ENABLE_TSAN}")
    message(STATUS "")
endfunction()


# Game modules / plugins (docs/design/07_modules.md 5).
#
#   conjure_add_game_module(<target>
#       MANIFEST      <module.json>         the module's manifest; its directory is the module root
#       SOURCES       <files...>
#       [NAME         <manifest name>]      defaults to the manifest's "name" member
#       [OUTPUT_ROOT  <directory>]          module root for the build output (default: the manifest's
#                                           directory); the binary lands in <root>/bin/<platform>-<arch>-<config>/
#       [DEPS         <targets...>]
#       [DEFINES      <defs...>]
#       [EDITOR_UI]                         links the ImGui interface target for editor panels
#   )
#
# Creates a SHARED library linking conjure::shared privately, with hidden visibility, the define
# CONJURE_MODULE_NAME="<name>" that CONJURE_DECLARE_MODULE() embeds in the binary, an output name
# equal to the manifest's "binary" member (default: the target name) and a $ORIGIN run path. The
# binary is placed where module_binary_path() derives it: <root>/bin/<platform>-<arch>-<config>/.
#
# The host platform and architecture names come from cmake/ConjureSharedLibrary.cmake
# (CONJURE_HOST_PLATFORM_NAME, CONJURE_HOST_ARCH_NAME).

include(ConjureSharedLibrary)

function(conjure_add_game_module target)
    set(options EDITOR_UI)
    set(oneValueArgs NAME MANIFEST OUTPUT_ROOT)
    set(multiValueArgs SOURCES DEPS DEFINES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_game_module(${target}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "conjure_add_game_module(${target}): SOURCES is required")
    endif()
    if(NOT ARG_MANIFEST)
        message(FATAL_ERROR "conjure_add_game_module(${target}): MANIFEST is required")
    endif()
    get_filename_component(manifestPath "${ARG_MANIFEST}" ABSOLUTE)
    if(NOT EXISTS "${manifestPath}")
        message(FATAL_ERROR "conjure_add_game_module(${target}): manifest '${manifestPath}' does not exist")
    endif()

    file(READ "${manifestPath}" manifestText)
    set(binaryName "${target}")
    string(JSON manifestName ERROR_VARIABLE jsonError GET "${manifestText}" name)
    if(jsonError)
        message(FATAL_ERROR "conjure_add_game_module(${target}): '${manifestPath}' has no \"name\": ${jsonError}")
    endif()
    string(JSON manifestBinary ERROR_VARIABLE jsonError GET "${manifestText}" binary)
    if(NOT jsonError AND manifestBinary)
        set(binaryName "${manifestBinary}")
    endif()
    if(NOT ARG_NAME)
        set(ARG_NAME "${manifestName}")
    endif()

    get_filename_component(manifestDirectory "${manifestPath}" DIRECTORY)
    if(ARG_OUTPUT_ROOT)
        get_filename_component(outputRoot "${ARG_OUTPUT_ROOT}" ABSOLUTE)
    else()
        set(outputRoot "${manifestDirectory}")
    endif()
    set(outputDirectory "${outputRoot}/bin/${CONJURE_HOST_PLATFORM_NAME}-${CONJURE_HOST_ARCH_NAME}-$<LOWER_CASE:$<CONFIG>>")

    add_library(${target} SHARED ${ARG_SOURCES} "${manifestPath}")
    set_source_files_properties("${manifestPath}" PROPERTIES HEADER_FILE_ONLY TRUE)

    target_link_libraries(${target}
        PRIVATE
            conjure::shared
            conjure::platform_defines
            conjure::warnings
            conjure::sanitizers
            ${ARG_DEPS}
    )
    if(ARG_EDITOR_UI AND TARGET conjure::thirdparty::imgui_interface)
        target_link_libraries(${target} PRIVATE conjure::thirdparty::imgui_interface)
    endif()
    target_compile_definitions(${target}
        PRIVATE
            CONJURE_MODULE_NAME="${ARG_NAME}"
            ${ARG_DEFINES}
    )
    if(TARGET conjure_abi_identity)
        add_dependencies(${target} conjure_abi_identity)
    endif()

    set_target_properties(${target} PROPERTIES
        FOLDER "GameModules"
        OUTPUT_NAME "${binaryName}"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        RUNTIME_OUTPUT_DIRECTORY "${outputDirectory}"
        LIBRARY_OUTPUT_DIRECTORY "${outputDirectory}"
        ARCHIVE_OUTPUT_DIRECTORY "${outputDirectory}"
        PDB_OUTPUT_DIRECTORY "${outputDirectory}"
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
        BUILD_RPATH_USE_ORIGIN TRUE
    )
    if(APPLE)
        set_target_properties(${target} PROPERTIES BUILD_RPATH "@loader_path" INSTALL_RPATH "@loader_path")
    endif()

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ARG_SOURCES})
endfunction()
