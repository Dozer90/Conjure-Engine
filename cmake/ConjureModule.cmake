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
#   conjure_add_test_module(<module> SOURCES ... [DEPS ...])
#
# Every target gets the shared warning, sanitizer and platform-define interface targets.

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


function(conjure_add_test_module module)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES DEPS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_test_module(${module}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "conjure_add_test_module(${module}): SOURCES is required")
    endif()

    set(target "conjure_${module}_tests")
    add_executable(${target} ${ARG_SOURCES})

    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/engine/src")

    target_link_libraries(${target}
        PRIVATE
            conjure::${module}
            conjure::test_support
            conjure::platform_defines
            conjure::warnings
            conjure::sanitizers
            ${ARG_DEPS}
    )

    set_target_properties(${target} PROPERTIES FOLDER "Tests")

    add_test(NAME ${target}
        COMMAND ${target} --reporters=console --no-intro=true
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
    )
    set_tests_properties(${target} PROPERTIES
        LABELS "unit;${module}"
        TIMEOUT 300
    )

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
    message(STATUS "  Sanitizers ............ ASan=${CONJURE_ENABLE_ASAN} UBSan=${CONJURE_ENABLE_UBSAN}")
    message(STATUS "")
endfunction()
