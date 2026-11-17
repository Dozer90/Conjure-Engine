# Probe of runtime/cmake/ConjureMsvcRuntime.cmake: the CTest entry runtime.msvc_runtime_folder_rules runs this file
# in script mode (cmake -DPROBE_ROOT=<scratch directory> -P this file). It builds fake redistributable folders
# under PROBE_ROOT and holds the rule that decides which of them a Distribution build ships to them. No compiler,
# no Visual Studio and no network are involved, so the rule is checked on every platform, not only on the one
# Windows machine whose installed toolsets happen to exercise it.

cmake_minimum_required(VERSION 3.28)

if(NOT CMAKE_SCRIPT_MODE_FILE)
    message(FATAL_ERROR "msvc runtime probe: run this file with cmake -P")
endif()
if(NOT DEFINED PROBE_ROOT OR PROBE_ROOT STREQUAL "")
    message(FATAL_ERROR "msvc runtime probe: PROBE_ROOT is not set")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/ConjureMsvcRuntime.cmake")

# Every run starts from nothing: the folders of an earlier run are fixtures of that run.
file(REMOVE_RECURSE "${PROBE_ROOT}")

# Writes the fake redistributable folder <root>/<version>/x64/Microsoft.VC<toolset>.CRT holding the files named
# in ARGN (empty ones) and sets <outVariable> to it.
function(probe_crt_directory outVariable root version toolset)
    set(directory "${root}/${version}/x64/Microsoft.VC${toolset}.CRT")
    file(MAKE_DIRECTORY "${directory}")
    foreach(name IN LISTS ARGN)
        file(WRITE "${directory}/${name}" "")
    endforeach()
    set(${outVariable} "${directory}" PARENT_SCOPE)
endfunction()

# Fails unless <actual> is <expected>.
function(probe_expect_equal what actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "msvc runtime probe: ${what}: '${actual}', expected '${expected}'")
    endif()
endfunction()

# Fails unless conjure_msvc_accept_crt_directory answers <expected> for the folder and the toolset.
function(probe_expect_accept what directory toolsetVersion expected)
    conjure_msvc_accept_crt_directory(accepted "${directory}" "${toolsetVersion}")
    if(expected AND NOT accepted)
        message(FATAL_ERROR "msvc runtime probe: ${what}: '${directory}' was refused for toolset ${toolsetVersion}")
    endif()
    if(accepted AND NOT expected)
        message(FATAL_ERROR "msvc runtime probe: ${what}: '${directory}' was accepted for toolset ${toolsetVersion}")
    endif()
endfunction()

# Sets <outVariable> to the sorted file names of the paths in ARGN.
function(probe_file_names outVariable)
    set(names "")
    foreach(path IN LISTS ARGN)
        cmake_path(GET path FILENAME name)
        list(APPEND names "${name}")
    endforeach()
    list(SORT names)
    set(${outVariable} "${names}" PARENT_SCOPE)
endfunction()

#------------------------------------------------------------------------------
# The toolset version of a compiler
#------------------------------------------------------------------------------
conjure_msvc_toolset_version(version 1944)
probe_expect_equal("the toolset of cl 19.44" "${version}" "14.44")
conjure_msvc_toolset_version(version 1951)
probe_expect_equal("the toolset of cl 19.51" "${version}" "14.51")
conjure_msvc_toolset_version(version 1900)
probe_expect_equal("the toolset of cl 19.00" "${version}" "14.00")
# Not of the 19.x line, or no MSVC at all (the variable is empty for GCC and Clang): no toolset, so no folder.
conjure_msvc_toolset_version(version 1800)
probe_expect_equal("the toolset of cl 18.00" "${version}" "")
conjure_msvc_toolset_version(version 2010)
probe_expect_equal("the toolset of a compiler past the 19.x line" "${version}" "")
conjure_msvc_toolset_version(version "")
probe_expect_equal("the toolset of no MSVC compiler" "${version}" "")

#------------------------------------------------------------------------------
# Which folder is accepted
#------------------------------------------------------------------------------
set(releaseLibraries vcruntime140.dll vcruntime140_1.dll msvcp140.dll msvcp140_atomic_wait.dll)
probe_crt_directory(folder1444 "${PROBE_ROOT}/vs2022" 14.44.35112 143 ${releaseLibraries})
probe_crt_directory(folder1451 "${PROBE_ROOT}/vs18" 14.51.36231 145 ${releaseLibraries} vcruntime140_threads.dll)

# A folder as new as the compiler's toolset, or newer, is its redistributable (the runtime is binary compatible
# forwards: a newer one serves an older build).
probe_expect_accept("the toolset's own folder" "${folder1444}" 14.44 TRUE)
probe_expect_accept("a folder newer than the toolset" "${folder1444}" 14.40 TRUE)
probe_expect_accept("a folder with a trailing separator" "${folder1444}/" 14.44 TRUE)
probe_expect_accept("the newest folder for the newest toolset" "${folder1451}" 14.51 TRUE)
# The defect the rule exists for: the folder of an older Visual Studio answered for a newer compiler.
probe_expect_accept("a folder older than the toolset" "${folder1444}" 14.51 FALSE)
# The minor is a number, not a text: 14.9 is older than 14.44.
probe_crt_directory(folder149 "${PROBE_ROOT}/old" 14.9.1 141 ${releaseLibraries})
probe_expect_accept("a one-digit minor against a two-digit one" "${folder149}" 14.44 FALSE)
# Without a toolset version (no MSVC of the 19.x line) nothing can be compared, so nothing is accepted.
probe_expect_accept("a folder without a toolset to compare with" "${folder1444}" "" FALSE)

# A path that does not show the folder's version is refused whatever it holds.
set(unversioned "${PROBE_ROOT}/copied/x64/Microsoft.VC143.CRT")
file(MAKE_DIRECTORY "${unversioned}")
file(WRITE "${unversioned}/vcruntime140.dll" "")
probe_expect_accept("a folder without a version in its path" "${unversioned}" 14.44 FALSE)
set(twoPartVersion "${PROBE_ROOT}/short/14.44/x64/Microsoft.VC143.CRT")
file(MAKE_DIRECTORY "${twoPartVersion}")
file(WRITE "${twoPartVersion}/vcruntime140.dll" "")
probe_expect_accept("a folder whose version has no build number" "${twoPartVersion}" 14.44 FALSE)
# The version is read from the folder layout, not from any number in the path.
set(otherLayout "${PROBE_ROOT}/14.51.36231/x64/elsewhere")
file(MAKE_DIRECTORY "${otherLayout}")
file(WRITE "${otherLayout}/vcruntime140.dll" "")
probe_expect_accept("a folder that is not named Microsoft.VC<toolset>.CRT" "${otherLayout}" 14.44 FALSE)

# A folder of the right version that does not hold the runtime is no redistributable folder: one that is empty,
# one that holds other libraries only, and one that does not exist.
probe_crt_directory(emptyFolder "${PROBE_ROOT}/empty" 14.51.36231 145)
probe_expect_accept("an empty folder" "${emptyFolder}" 14.44 FALSE)
probe_crt_directory(otherLibraries "${PROBE_ROOT}/other" 14.51.36231 145 msvcp140.dll concrt140.dll)
probe_expect_accept("a folder without vcruntime140.dll" "${otherLibraries}" 14.44 FALSE)
probe_expect_accept("a folder that does not exist" "${PROBE_ROOT}/nowhere/14.51.36231/x64/Microsoft.VC145.CRT" 14.44
    FALSE)

#------------------------------------------------------------------------------
# Which candidate is taken, and which of its files
#------------------------------------------------------------------------------
# Next to the libraries that ship: the Concurrency Runtime and the C++/CX library, which nothing in the engine
# imports, and files that only resemble a runtime library.
file(WRITE "${folder1451}/concrt140.dll" "")
file(WRITE "${folder1451}/vccorlib140.dll" "")
file(WRITE "${folder1451}/msvcp140.lib" "")
file(WRITE "${folder1451}/my_vcruntime140.dll" "")
set(expected1451 msvcp140.dll msvcp140_atomic_wait.dll vcruntime140.dll vcruntime140_1.dll vcruntime140_threads.dll)
set(expected1444 msvcp140.dll msvcp140_atomic_wait.dll vcruntime140.dll vcruntime140_1.dll)

# Fails unless every path in ARGN lies directly in <directory>.
function(probe_expect_folder what directory)
    foreach(library IN LISTS ARGN)
        cmake_path(GET library PARENT_PATH libraryDirectory)
        probe_expect_equal("${what}: the folder of '${library}'" "${libraryDirectory}" "${directory}")
    endforeach()
endfunction()

# Without VCToolsRedistDir the installer's answer is taken when it is as new as the toolset.
conjure_msvc_select_crt_libraries(libraries 14.44 INSTALLER "${folder1444}" "${folder1451}")
probe_file_names(names ${libraries})
probe_expect_equal("the libraries of the installer's answer" "${names}" "${expected1444}")
probe_expect_folder("the installer's answer" "${folder1444}" ${libraries})
# With it, the answer is held to the environment's folder: one of the toolset's version that is older than the
# folder vcvars names comes from another installation and gives way.
conjure_msvc_select_crt_libraries(libraries 14.44 INSTALLER "${folder1444}" ENVIRONMENT "${folder1451}")
probe_expect_folder("an installer's answer older than the environment's" "${folder1451}" ${libraries})

# The fallback: the installer's answer is older than the compiler (what CMake 3.31 answers for a v145 compiler
# next to Visual Studio 2022), so the folder of the compiler's own environment is taken.
conjure_msvc_select_crt_libraries(libraries 14.51 INSTALLER "${folder1444}" ENVIRONMENT "${folder1451}")
probe_file_names(names ${libraries})
probe_expect_equal("the libraries of the environment's folder" "${names}" "${expected1451}")
probe_expect_folder("the fallback" "${folder1451}" ${libraries})

# The environment's folder is the toolset's own whatever its minor: Visual Studio versions the redistributable
# apart from the tools, so a folder 14.40.x paired with toolset 14.41 is taken, not refused as older.
probe_crt_directory(folder1440 "${PROBE_ROOT}/redist" 14.40.33807 143 ${releaseLibraries})
conjure_msvc_select_crt_libraries(libraries 14.41 ENVIRONMENT "${folder1440}")
probe_file_names(names ${libraries})
probe_expect_equal("the environment folder 14.40.x for toolset 14.41" "${names}" "${expected1444}")
probe_expect_folder("the environment folder 14.40.x for toolset 14.41" "${folder1440}" ${libraries})
# The installer's answer is then held to the environment's version, not to the toolset's: one as new as the
# environment's folder is taken, an older one gives way to it.
probe_crt_directory(folder1440b "${PROBE_ROOT}/installed" 14.40.33807 143 ${releaseLibraries})
conjure_msvc_select_crt_libraries(libraries 14.41 INSTALLER "${folder1440b}" ENVIRONMENT "${folder1440}")
probe_expect_folder("an installer's answer as new as the environment's folder" "${folder1440b}" ${libraries})
probe_crt_directory(folder1438 "${PROBE_ROOT}/vs2022old" 14.38.33135 143 ${releaseLibraries})
conjure_msvc_select_crt_libraries(libraries 14.41 INSTALLER "${folder1438}" ENVIRONMENT "${folder1440}")
probe_expect_folder("an installer's answer older than the environment's folder" "${folder1440}" ${libraries})
# An environment folder whose path does not show its version is not taken, and the installer's answer is held to
# the toolset again.
conjure_msvc_select_crt_libraries(libraries 14.44 INSTALLER "${folder1444}" ENVIRONMENT "${unversioned}")
probe_expect_folder("an unreadable environment folder" "${folder1444}" ${libraries})
conjure_msvc_select_crt_libraries(libraries 14.44 ENVIRONMENT "${unversioned}" "${folder1440}")
probe_expect_folder("the first environment folder that shows a version" "${folder1440}" ${libraries})

# Nothing passes: an empty list, on which runtime/CMakeLists.txt warns and copies nothing. An older library is
# never the answer of last resort.
conjure_msvc_select_crt_libraries(libraries 14.52 INSTALLER "${folder1444}" "${folder1451}" "${unversioned}")
probe_expect_equal("the libraries when every installer answer is older or unreadable" "${libraries}" "")
conjure_msvc_select_crt_libraries(libraries 14.52 INSTALLER "${folder1444}" ENVIRONMENT "${unversioned}")
probe_expect_equal("the libraries when the environment's folder is unreadable" "${libraries}" "")
conjure_msvc_select_crt_libraries(libraries 14.44)
probe_expect_equal("the libraries without a candidate" "${libraries}" "")
conjure_msvc_select_crt_libraries(libraries "" INSTALLER "${folder1444}" ENVIRONMENT "${folder1451}")
probe_expect_equal("the libraries without a toolset version" "${libraries}" "")

message(STATUS "msvc runtime probe: passed")
