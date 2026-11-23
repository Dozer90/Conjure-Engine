# Source paths in Distribution binaries (code sweep F613): the compile options that remove the checkout from the
# paths a Distribution binary embeds. cmake/ConjureWarnings.cmake applies them to the tree; this file only computes
# them, so cmake/ConjureDistributionPathsProbe.cmake can run it in script mode for every compiler and root layout.
#
# __FILE__ (CONJURE_LOG, CONJURE_ASSERT, the C assert macro) and std::source_location expand to the path the
# compiler was given, and CMake gives it absolute paths, so a binary would carry the checkout path of the machine
# that built it. Distribution removes the roots below from those paths, so a shipped binary names
# engine/src/core/log.cpp instead. Debug and Development keep absolute paths: log records, assert messages and
# doctest failures stay clickable in an IDE there.
#
#   MSVC        /d1trimfile:<root>/               undocumented, accepted by the supported cl (19.40 and later);
#                                                 cl matches the prefix ignoring case and the kind of separator,
#                                                 and std::source_location follows it
#   clang-cl    /clang:-fmacro-prefix-map=<root>/=
#   GCC, Clang  -fmacro-prefix-map=<root>/=       macros only (__FILE__, __builtin_FILE); debug information keeps
#                                                 its absolute paths, so Distribution compiles without any: its
#                                                 flags derive from Release (no -g, ConjureConfigs.cmake), and
#                                                 Jolt's own -g is turned off there (third_party/CMakeLists.txt)
#
# The roots are the top-level source and build directories (CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR: generated
# sources, fetched dependencies under _deps) and Conjure's own (PROJECT_SOURCE_DIR and PROJECT_BINARY_DIR of the
# list file that calls the function, Conjure's root CMakeLists.txt). The last two differ from the first two only when
# a parent project adds Conjure with add_subdirectory(), and matter when Conjure's checkout lies outside the parent's
# directories. A root that equals or lies inside another is dropped, so only the outer ones are mapped. On a Windows
# host the roots are compared without regard to case. A root that contains '=' cannot be expressed in a GCC or Clang
# prefix map, so it is left unmapped with a warning and tools/check_distribution_paths.py fails on the binaries. The
# options apply to C, C++ and Objective-C sources only; resource and assembler compilers do not know them.

include_guard(GLOBAL)

# Sets <outVariable> to the list of compile options (generator expressions, empty outside Distribution) that remove
# the source and build roots from the paths __FILE__ and std::source_location embed in a Distribution binary. Reads
# CMAKE_SOURCE_DIR, CMAKE_BINARY_DIR, PROJECT_SOURCE_DIR, PROJECT_BINARY_DIR, CMAKE_CXX_COMPILER_ID,
# CMAKE_CXX_COMPILER_FRONTEND_VARIANT and CMAKE_HOST_WIN32.
function(conjure_distribution_path_options outVariable)
    set(candidates "")
    foreach(candidate IN ITEMS "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" "${PROJECT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}")
        if(NOT candidate STREQUAL "")
            list(APPEND candidates "${candidate}")
        endif()
    endforeach()
    # Windows paths compare without regard to case (a drive letter may come in either case).
    set(keys "")
    foreach(candidate IN LISTS candidates)
        cmake_path(NORMAL_PATH candidate OUTPUT_VARIABLE key)
        # Without its trailing separators (a root such as "/" or "C:/" keeps its own), so that one directory
        # spelled with and without them gives one key.
        string(REGEX REPLACE "([^/:])/+$" "\\1" key "${key}")
        if(CMAKE_HOST_WIN32)
            string(TOLOWER "${key}" key)
        endif()
        list(APPEND keys "${key}")
    endforeach()
    # A candidate is dropped when another one contains it, or is the same directory and comes first.
    set(roots "")
    list(LENGTH candidates count)
    if(count EQUAL 0)
        set(${outVariable} "" PARENT_SCOPE)
        return()
    endif()
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
        list(GET keys ${index} key)
        set(covered OFF)
        foreach(other RANGE ${last})
            list(GET keys ${other} otherKey)
            if(other EQUAL index)
                continue()
            endif()
            cmake_path(IS_PREFIX otherKey "${key}" NORMALIZE inside)
            if(inside AND (NOT otherKey STREQUAL key OR other LESS index))
                set(covered ON)
                break()
            endif()
        endforeach()
        if(NOT covered)
            list(GET candidates ${index} root)
            list(APPEND roots "${root}")
        endif()
    endforeach()

    set(clangCl OFF)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(clangCl ON)
    endif()

    set(options "")
    foreach(root IN LISTS roots)
        cmake_path(NORMAL_PATH root)
        string(REGEX REPLACE "/+$" "" root "${root}")
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND root MATCHES "=")
            message(WARNING "Conjure: '${root}' contains '=', which a -fmacro-prefix-map cannot express; "
                            "Distribution binaries keep this root in their source paths and "
                            "tools/check_distribution_paths.py fails on them (code sweep F613)")
            continue()
        endif()
        # A path may hold characters that end or split a generator expression ('>' first, so the '>' of an
        # inserted $<COMMA> is not replaced again).
        string(REPLACE ">" "$<ANGLE-R>" root "${root}")
        string(REPLACE "," "$<COMMA>" root "${root}")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            set(option "/d1trimfile:${root}/")
        elseif(clangCl)
            set(option "/clang:-fmacro-prefix-map=${root}/=")
        else()
            set(option "-fmacro-prefix-map=${root}/=")
        endif()
        list(APPEND options
            "$<$<AND:$<CONFIG:Distribution>,$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>>:${option}>")
    endforeach()
    set(${outVariable} "${options}" PARENT_SCOPE)
endfunction()
