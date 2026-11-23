# Warning configuration shared by every Conjure target, and the Distribution path options that every target of the
# tree is compiled with (see the last section).
#
# Third-party code is never built with the warning flags: fetched dependencies are declared with
# SYSTEM so their headers are treated as external, and their own targets keep their own
# warning settings.

add_library(conjure_warnings INTERFACE)
add_library(conjure::warnings ALIAS conjure_warnings)

if(MSVC)
    target_compile_options(conjure_warnings INTERFACE
        /W4
        /permissive-
        /external:W0              # silence warnings from SYSTEM (/external:I) include directories
        /w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14545 /w14546
        /w14547 /w14549 /w14555 /w14619 /w14640 /w14826 /w14905 /w14906 /w14928
        /wd4201                   # nameless struct/union (used deliberately in math types)
        # Exported classes hold standard-library members: C4251/C4275 are the documented noise of
        # every DLL-exporting C++ project (docs/design/07_modules.md 5) and carry no information
        # here because conjure_shared and its consumers are built with the same toolchain.
        /wd4251 /wd4275
    )
    if(CONJURE_WARNINGS_AS_ERRORS)
        target_compile_options(conjure_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(conjure_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(conjure_warnings INTERFACE
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
        )
    endif()
    if(CONJURE_WARNINGS_AS_ERRORS)
        target_compile_options(conjure_warnings INTERFACE -Werror)
    endif()
endif()

# Platform definitions every Conjure target needs.
add_library(conjure_platform_defines INTERFACE)
add_library(conjure::platform_defines ALIAS conjure_platform_defines)

target_compile_definitions(conjure_platform_defines INTERFACE
    $<$<CONFIG:Debug>:CONJURE_DEBUG=1>
    $<$<CONFIG:Development>:CONJURE_DEVELOPMENT=1>
    $<$<CONFIG:Distribution>:CONJURE_DISTRIBUTION=1>
    $<$<BOOL:${CONJURE_WITH_RENDERER}>:CONJURE_WITH_RENDERER=1>
    $<$<BOOL:${CONJURE_WITH_PHYSICS}>:CONJURE_WITH_PHYSICS=1>
    $<$<BOOL:${CONJURE_WITH_AUDIO}>:CONJURE_WITH_AUDIO=1>
    $<$<BOOL:${CONJURE_WITH_SCRIPTING}>:CONJURE_WITH_SCRIPTING=1>
    $<$<BOOL:${CONJURE_RHI_D3D12}>:CONJURE_RHI_D3D12=1>
    $<$<BOOL:${CONJURE_RHI_VULKAN}>:CONJURE_RHI_VULKAN=1>
    # The editor never exists in Distribution, whatever the option says (root CMakeLists.txt);
    # modules::host_feature_flags() mirrors this define.
    $<$<AND:$<BOOL:${CONJURE_BUILD_EDITOR}>,$<NOT:$<CONFIG:Distribution>>>:CONJURE_BUILD_EDITOR=1>
)

if(WIN32)
    target_compile_definitions(conjure_platform_defines INTERFACE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

#------------------------------------------------------------------------------
# Source paths in Distribution binaries (code sweep F613)
#------------------------------------------------------------------------------
# Distribution compiles every C, C++ and Objective-C source of the tree with the options of
# cmake/ConjureDistributionPaths.cmake, which remove the source and build directories from the paths __FILE__ and
# std::source_location embed; Debug and Development keep absolute paths. They are added with add_compile_options to
# the directory that includes this file (the root CMakeLists.txt, before third_party/), not to
# conjure::platform_defines, so they reach every target of this tree, third-party code included. A target that
# replaces its directory's compile options loses them. In a project that adds Conjure with add_subdirectory(), the
# roots are its top-level directories (CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR are the parent's) and Conjure's own
# (PROJECT_SOURCE_DIR and PROJECT_BINARY_DIR, where they lie outside the parent's), mapped for Conjure's targets only:
# the parent's own targets get none of the options unless it adds them itself. tools/check_distribution_paths.py
# proves the result on the built Distribution binaries, and the case "regression - code sweep F613 ..." of
# tests/core/foundation/test_log.cpp on a test compiled with them.
include(ConjureDistributionPaths)
conjure_distribution_path_options(_conjureDistributionPathOptions)
add_compile_options(${_conjureDistributionPathOptions})
unset(_conjureDistributionPathOptions)
