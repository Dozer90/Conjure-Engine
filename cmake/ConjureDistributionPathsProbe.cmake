# Probe of cmake/ConjureDistributionPaths.cmake (code sweep F613), run in script mode:
#
#   cmake -P cmake/ConjureDistributionPathsProbe.cmake
#
# It computes the Distribution path options for every compiler family and root layout the function tells apart,
# with stand-in roots and compiler ids, and compares them with the options expected. No compiler, no project and no
# file system access are involved, so it runs alike on every host: the host's own case rule is replaced by the
# CMAKE_HOST_WIN32 each case sets. It prints "distribution paths probe: passed" and exits 0, or names every case that
# failed and exits 1. The case with a root that holds '=' also prints the configure warning of the function, which
# tools/tests/test_check_distribution_paths.py (the runner of this probe) checks for.

include("${CMAKE_CURRENT_LIST_DIR}/ConjureDistributionPaths.cmake")

set_property(GLOBAL PROPERTY PROBE_FAILURES "")

# Computes the options for one compiler (<compilerId>, <frontendVariant>), host case rule (<hostWindows>), pair of
# top-level directories (<sourceDir>, <binaryDir>) and pair of Conjure directories (<projectSourceDir>,
# <projectBinaryDir>: the same as the top-level ones unless a parent project adds Conjure), and compares them with
# the options given after the directories, each written without its Distribution guard.
function(probe_subproject_case label compilerId frontendVariant hostWindows sourceDir binaryDir projectSourceDir
    projectBinaryDir)
    set(CMAKE_CXX_COMPILER_ID "${compilerId}")
    set(CMAKE_CXX_COMPILER_FRONTEND_VARIANT "${frontendVariant}")
    set(CMAKE_HOST_WIN32 ${hostWindows})
    set(CMAKE_SOURCE_DIR "${sourceDir}")
    set(CMAKE_BINARY_DIR "${binaryDir}")
    set(PROJECT_SOURCE_DIR "${projectSourceDir}")
    set(PROJECT_BINARY_DIR "${projectBinaryDir}")
    conjure_distribution_path_options(options)
    set(expected "")
    foreach(option IN LISTS ARGN)
        list(APPEND expected "$<$<AND:$<CONFIG:Distribution>,$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>>:${option}>")
    endforeach()
    if(NOT options STREQUAL expected)
        # One entry of the failure list: the ';' between options would split it.
        string(REPLACE ";" " " expectedText "${expected}")
        string(REPLACE ";" " " optionsText "${options}")
        set_property(GLOBAL APPEND PROPERTY PROBE_FAILURES "${label}: expected [${expectedText}], got [${optionsText}]")
    endif()
endfunction()

# probe_subproject_case() for Conjure configured as the top-level project.
function(probe_case label compilerId frontendVariant hostWindows sourceDir binaryDir)
    probe_subproject_case("${label}" "${compilerId}" "${frontendVariant}" "${hostWindows}" "${sourceDir}"
        "${binaryDir}" "${sourceDir}" "${binaryDir}" ${ARGN})
endfunction()

# Nesting: only the outer root is mapped.
probe_case("MSVC, the build directory inside the source directory" MSVC MSVC ON
    "C:/Work/Git Repos/conjure" "C:/Work/Git Repos/conjure/build/windows-msvc-dist"
    "/d1trimfile:C:/Work/Git Repos/conjure/")
probe_case("GCC, the source directory inside the build directory" GNU GNU OFF
    "/home/ci/out/conjure" "/home/ci/out"
    "-fmacro-prefix-map=/home/ci/out/=")
probe_case("GCC, two separate roots" GNU GNU OFF
    "/home/ci/conjure" "/tmp/build/conjure"
    "-fmacro-prefix-map=/home/ci/conjure/=" "-fmacro-prefix-map=/tmp/build/conjure/=")
probe_case("GCC, a build directory that only starts like the source directory is no child of it" GNU GNU OFF
    "/w/conjure" "/w/conjure_build"
    "-fmacro-prefix-map=/w/conjure/=" "-fmacro-prefix-map=/w/conjure_build/=")
probe_case("GCC, a trailing separator and dot segments are normalised away" GNU GNU OFF
    "/w/conjure/" "/w/conjure/./build/../build"
    "-fmacro-prefix-map=/w/conjure/=")

# Conjure added by a parent project with add_subdirectory(): Conjure's own directories are roots as well, unless
# they lie inside the parent's.
probe_subproject_case("GCC, Conjure's checkout beside the parent's is a root of its own" GNU GNU OFF
    "/w/game" "/w/game/build" "/w/conjure" "/w/game/build/conjure"
    "-fmacro-prefix-map=/w/game/=" "-fmacro-prefix-map=/w/conjure/=")
probe_subproject_case("GCC, Conjure's build directory outside every other root is mapped too" GNU GNU OFF
    "/w/game" "/w/game/build" "/w/conjure" "/tmp/conjure_build"
    "-fmacro-prefix-map=/w/game/=" "-fmacro-prefix-map=/w/conjure/=" "-fmacro-prefix-map=/tmp/conjure_build/=")
probe_subproject_case("MSVC, Conjure inside the parent's checkout adds no root" MSVC MSVC ON
    "C:/Work/Game" "C:/Work/Game/build" "c:/work/game/external/conjure" "C:/Work/Game/build/external/conjure"
    "/d1trimfile:C:/Work/Game/")
probe_subproject_case("GCC, a parent checkout inside Conjure's directory is covered by it" GNU GNU OFF
    "/w/conjure/samples/game" "/w/conjure/samples/game/build" "/w/conjure" "/w/conjure/samples/game/build/c"
    "-fmacro-prefix-map=/w/conjure/=")

# The case rule of the host: Windows paths nest whatever the case of their letters, POSIX paths do not.
probe_case("MSVC on a Windows host, roots that differ in case still nest" MSVC MSVC ON
    "C:/Work/Conjure" "c:/work/conjure/build/dist"
    "/d1trimfile:C:/Work/Conjure/")
probe_case("Clang on a POSIX host, roots that differ in case are two roots" Clang GNU OFF
    "/home/Ci/conjure" "/home/ci/conjure/build"
    "-fmacro-prefix-map=/home/Ci/conjure/=" "-fmacro-prefix-map=/home/ci/conjure/build/=")

# The spelling of each compiler family.
probe_case("clang-cl takes the GCC map through /clang:" Clang MSVC ON
    "C:/src/conjure" "C:/src/conjure/build"
    "/clang:-fmacro-prefix-map=C:/src/conjure/=")
probe_case("AppleClang takes the GCC map" AppleClang GNU OFF
    "/Users/ci/conjure" "/Users/ci/conjure/build"
    "-fmacro-prefix-map=/Users/ci/conjure/=")

# Characters a generator expression or a prefix map cannot hold as they are.
probe_case("',' and '>' are escaped for the generator expression" GNU GNU OFF
    "/src/a,b>c" "/src/a,b>c/build"
    "-fmacro-prefix-map=/src/a$<COMMA>b$<ANGLE-R>c/=")
probe_case("GCC leaves a root with '=' unmapped" GNU GNU OFF
    "/src/a=b" "/build/conjure"
    "-fmacro-prefix-map=/build/conjure/=")
probe_case("MSVC maps a root with '='" MSVC MSVC ON
    "C:/src/a=b" "C:/src/a=b/build"
    "/d1trimfile:C:/src/a=b/")
probe_case("the file system root as the source directory" GNU GNU OFF
    "/" "/build"
    "-fmacro-prefix-map=/=")

get_property(failures GLOBAL PROPERTY PROBE_FAILURES)
if(failures)
    list(JOIN failures "\n  " failureText)
    message(FATAL_ERROR "distribution paths probe: failed:\n  ${failureText}")
endif()
message(STATUS "distribution paths probe: passed")
