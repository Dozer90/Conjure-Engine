# Which Visual C++ redistributable folder a Windows Distribution build ships (docs/design/06_engine_loop.md D14,
# code sweep F790). runtime/CMakeLists.txt includes this file and copies the libraries it names next to
# conjure_runtime; runtime/cmake/ConjureMsvcRuntimeProbe.cmake holds every rule below to fake folders, on any
# platform (the CTest entry runtime.msvc_runtime_folder_rules, registered by tests/runtime/CMakeLists.txt).
#
# InstallRequiredSystemLibraries locates the folder (Microsoft.VC<toolset>.CRT). A CMake older than the compiler
# can answer with the folder of another, older Visual Studio installation (3.31 does for a v145 compiler when
# Visual Studio 2022 is installed beside it). A library next to the executable is loaded in preference to the
# installed redistributable, and one older than the toolset that built the game is not supported by it, so that
# answer must be at least as new as the folder the compiler's own environment names (VCToolsRedistDir, set by
# vcvars and tools/build.py), or as the toolset when there is none; an older or unreadable answer gives way to
# the environment's folder. That folder is taken on its word when its path shows a version: vcvars pairs it with
# the active toolset, and Visual Studio versions the redistributable apart from the tools
# (Microsoft.VCRedistVersion.default.txt beside Microsoft.VCToolsVersion.default.txt), so its minor can be lower
# than the toolset's. Without an acceptable folder nothing is copied and the configure warns.

include_guard(GLOBAL)

# Sets <outVariable> to "<major>.<minor>" of the toolset that compiles (14.44 for cl 19.44, the MSVC_VERSION
# 1944), or to "" when <msvcVersion> is not of the 19.x line.
function(conjure_msvc_toolset_version outVariable msvcVersion)
    set(${outVariable} "" PARENT_SCOPE)
    if(NOT "${msvcVersion}" MATCHES "^19([0-9][0-9])$")
        return()
    endif()
    set(${outVariable} "14.${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# Sets <outVariable> to "<major>.<minor>" of the redistributable CRT folder <directory>
# (<...>/<major>.<minor>.<build>/<arch>/Microsoft.VC<toolset>.CRT holding vcruntime140.dll), or to "" when the
# folder holds no vcruntime140.dll or its path does not show its version.
function(conjure_msvc_crt_directory_version outVariable directory)
    set(${outVariable} "" PARENT_SCOPE)
    if(NOT EXISTS "${directory}/vcruntime140.dll")
        return()
    endif()
    if(NOT directory MATCHES "/([0-9]+\\.[0-9]+)\\.[0-9]+/[^/]+/Microsoft\\.VC[0-9]+\\.CRT/?$")
        return()
    endif()
    set(${outVariable} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# Sets <outVariable> to TRUE when <directory> is a redistributable CRT folder (conjure_msvc_crt_directory_version)
# at least as new as <minimumVersion>. A folder whose path does not show its version is refused: what cannot be
# compared is not shipped.
function(conjure_msvc_accept_crt_directory outVariable directory minimumVersion)
    set(${outVariable} FALSE PARENT_SCOPE)
    if(NOT minimumVersion)
        return()
    endif()
    conjure_msvc_crt_directory_version(directoryVersion "${directory}")
    if(NOT directoryVersion OR directoryVersion VERSION_LESS minimumVersion)
        return()
    endif()
    set(${outVariable} TRUE PARENT_SCOPE)
endfunction()

# conjure_msvc_select_crt_libraries(<outVariable> <toolsetVersion> [INSTALLER <folder>...]
#                                   [ENVIRONMENT <folder>...])
# Sets <outVariable> to the vcruntime140*.dll and msvcp140*.dll of the folder to ship, or to an empty list when
# none is acceptable. The ENVIRONMENT folders are those under VCToolsRedistDir: the first whose path shows a
# version is the toolset's own, taken without comparing it with <toolsetVersion>. The INSTALLER folders, the
# answers of InstallRequiredSystemLibraries, are preferred in their order when one is at least as new as that
# environment folder, or as <toolsetVersion> when there is none. Without a toolset version (no MSVC of the 19.x
# line) nothing is acceptable.
function(conjure_msvc_select_crt_libraries outVariable toolsetVersion)
    set(${outVariable} "" PARENT_SCOPE)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "INSTALLER;ENVIRONMENT")
    if(NOT toolsetVersion)
        return()
    endif()
    set(environmentDirectory "")
    set(minimumVersion "${toolsetVersion}")
    foreach(candidate IN LISTS arg_ENVIRONMENT)
        conjure_msvc_crt_directory_version(candidateVersion "${candidate}")
        if(candidateVersion)
            set(environmentDirectory "${candidate}")
            set(minimumVersion "${candidateVersion}")
            break()
        endif()
    endforeach()
    set(chosenDirectory "${environmentDirectory}")
    foreach(candidate IN LISTS arg_INSTALLER)
        conjure_msvc_accept_crt_directory(accepted "${candidate}" "${minimumVersion}")
        if(accepted)
            set(chosenDirectory "${candidate}")
            break()
        endif()
    endforeach()
    if(NOT chosenDirectory)
        return()
    endif()
    # Not a source list: which libraries exist is the toolset's to decide (14.38 has a vcruntime140_threads.dll,
    # 14.29 has none), and export step 5 ships whatever matches the same two patterns. The folder is the release
    # one; the debug runtime lies in a folder of its own.
    file(GLOB libraries LIST_DIRECTORIES false
        "${chosenDirectory}/vcruntime140*.dll" "${chosenDirectory}/msvcp140*.dll")
    set(${outVariable} "${libraries}" PARENT_SCOPE)
endfunction()

# Sets <outVariable> to the vcruntime140*.dll and msvcp140*.dll of this toolchain's redistributable folder, or to
# an empty list when no acceptable folder is found (conjure_msvc_select_crt_libraries): the folder
# InstallRequiredSystemLibraries answers with, or else the one under VCToolsRedistDir.
function(conjure_msvc_find_crt_libraries outVariable)
    set(${outVariable} "" PARENT_SCOPE)
    conjure_msvc_toolset_version(toolsetVersion "${MSVC_VERSION}")

    # The module only fills CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS here: no install rule is wanted, and the release
    # libraries alone are asked for (no debug, UCRT, MFC or OpenMP ones).
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS TRUE)
    set(CMAKE_INSTALL_DEBUG_LIBRARIES FALSE)
    set(CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY FALSE)
    set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
    set(CMAKE_INSTALL_MFC_LIBRARIES FALSE)
    set(CMAKE_INSTALL_OPENMP_LIBRARIES FALSE)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS "")
    include(InstallRequiredSystemLibraries)

    set(installerDirectories "")
    foreach(library IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        cmake_path(SET library NORMALIZE "${library}")
        cmake_path(GET library FILENAME libraryName)
        string(TOLOWER "${libraryName}" libraryName)
        if(libraryName STREQUAL "vcruntime140.dll")
            cmake_path(GET library PARENT_PATH libraryDirectory)
            list(APPEND installerDirectories "${libraryDirectory}")
        endif()
    endforeach()
    set(environmentDirectories "")
    if(DEFINED ENV{VCToolsRedistDir})
        # The variable ends in a separator; appended and normalised, the path has single ones. The glob is not a
        # source list: the toolset number in the folder's name is the toolchain's to choose.
        cmake_path(SET redistDirectory "$ENV{VCToolsRedistDir}")
        string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" redistArchitecture)
        cmake_path(APPEND redistDirectory "${redistArchitecture}")
        cmake_path(NORMAL_PATH redistDirectory)
        file(GLOB environmentDirectories LIST_DIRECTORIES true "${redistDirectory}/Microsoft.VC*.CRT")
    endif()

    conjure_msvc_select_crt_libraries(libraries "${toolsetVersion}"
        INSTALLER ${installerDirectories} ENVIRONMENT ${environmentDirectories})
    set(${outVariable} "${libraries}" PARENT_SCOPE)
endfunction()
