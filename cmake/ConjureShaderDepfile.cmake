# Rewrites the depfile a shader compiler wrote so CMake and the generator read every path in it whole
# (docs/design/02_renderer.md 3.12). Run by the custom commands conjure_add_shaders() registers, right after
# the compiler wrote the depfile and in the same working directory:
#
#   cmake -D CONJURE_DEPFILE=<file.d> -D CONJURE_DEPFILE_TARGET=<target> -P ConjureShaderDepfile.cmake
#
# CONJURE_DEPFILE_TARGET is the output path exactly as the compiler was given it (-Fo / -o), which is the
# target the depfile names before its ':'. Relative paths, the depfile's own included, are relative to the
# working directory.
#
# dxc writes "<target>: <path> \" and then one " <path> \" line per dependency without escaping anything, so a
# checkout under a directory such as "Git Repos" splits every path at its space into files that do not exist,
# and Ninja and make then compile every shader on every build. The script reads the depfile line by line: a
# line that names one existing file is one dependency; any other line is split at single spaces and its words
# are joined again, left to right, until they name an existing file, which also reads a compiler that puts
# several dependencies on one line (there, a path with a space whose first words already name another file
# is split after those words). It fails the build when a line cannot be split into existing files or
# when the depfile does not start with the target, so a compiler whose format changed is noticed instead of
# leaving every shader dirty.
#
# The rewritten depfile names the target and every dependency as an absolute path (CMake reads relative
# depfile paths against the current binary directory, not the command's working directory) with the escapes
# of the GCC depfile format that CMake and Ninja read: 2N+1 backslashes before a space for N backslashes and
# a space, "\#" for '#' and "$$" for '$'. On a Windows host every '\' becomes '/' first (dxc joins an
# include directory and a file name with '\'), so no backslash is left in front of a space there. A path with
# a tab or a line break is a build error: the format CMake reads has no escape for either. Under Ninja, CMake
# then rewrites the depfile once more (policy CMP0116) and hands a '#' or '$' on unescaped (CMake 4.3), so
# Ninja still splits a path holding one of those two characters, whatever this script writes.

if(NOT CONJURE_DEPFILE OR NOT CONJURE_DEPFILE_TARGET)
    message(FATAL_ERROR "ConjureShaderDepfile.cmake needs CONJURE_DEPFILE and CONJURE_DEPFILE_TARGET")
endif()

# In script mode CMAKE_CURRENT_BINARY_DIR is the working directory, the one the compiler ran in. The depfile
# itself is resolved against it once, like every path it names.
set(depfileBase "${CMAKE_CURRENT_BINARY_DIR}")
set(depfilePath "${CONJURE_DEPFILE}")
cmake_path(ABSOLUTE_PATH depfilePath BASE_DIRECTORY "${depfileBase}" NORMALIZE)


# Turns a path as the compiler spelled it into the absolute form the rewritten depfile names.
function(conjure_depfile_absolute path outAbsolute)
    set(absolute "${path}")
    if(CMAKE_HOST_WIN32)
        string(REPLACE "\\" "/" absolute "${absolute}")
    endif()
    cmake_path(ABSOLUTE_PATH absolute BASE_DIRECTORY "${depfileBase}" NORMALIZE)
    set(${outAbsolute} "${absolute}" PARENT_SCOPE)
endfunction()


# Sets outExists to TRUE when the path names an existing file (a directory is never a dependency).
function(conjure_depfile_names_file path outExists)
    set(exists FALSE)
    if(NOT path STREQUAL "")
        conjure_depfile_absolute("${path}" absolute)
        if(EXISTS "${absolute}" AND NOT IS_DIRECTORY "${absolute}")
            set(exists TRUE)
        endif()
    endif()
    set(${outExists} ${exists} PARENT_SCOPE)
endfunction()


# Escapes one absolute path for the GCC depfile format.
function(conjure_depfile_escape path outEscaped)
    if(path MATCHES "[\t\r\n]")
        message(FATAL_ERROR "ConjureShaderDepfile.cmake: '${path}' in ${CONJURE_DEPFILE} contains a tab or a line "
            "break, which a depfile cannot carry")
    endif()
    set(escaped "${path}")
    string(REPLACE "$" "$$" escaped "${escaped}")
    # A run of N backslashes before a space becomes 2N+1 of them: the run doubled and the escape itself.
    string(REGEX REPLACE "(\\\\*) " "\\1\\1\\\\ " escaped "${escaped}")
    string(REPLACE "#" "\\#" escaped "${escaped}")
    set(${outEscaped} "${escaped}" PARENT_SCOPE)
endfunction()


# Appends the dependencies one depfile line names to the variable outDependencies (a newline-separated text,
# not a list: a path may contain ';').
function(conjure_depfile_split_line line outDependencies)
    set(dependencies "${${outDependencies}}")
    conjure_depfile_names_file("${line}" wholeLine)
    if(wholeLine)
        string(APPEND dependencies "${line}\n")
        set(${outDependencies} "${dependencies}" PARENT_SCOPE)
        return()
    endif()

    set(rest "${line}")
    set(pending "")
    while(NOT rest STREQUAL "")
        set(word "${rest}")
        set(rest "")
        if(word MATCHES "^([^ ]*) (.*)$")
            set(word "${CMAKE_MATCH_1}")
            set(rest "${CMAKE_MATCH_2}")
        endif()
        if(pending STREQUAL "")
            set(pending "${word}")
        else()
            string(APPEND pending " ${word}")
        endif()
        conjure_depfile_names_file("${pending}" pendingExists)
        if(pendingExists)
            string(APPEND dependencies "${pending}\n")
            set(pending "")
        endif()
    endwhile()
    if(NOT pending STREQUAL "")
        message(FATAL_ERROR "ConjureShaderDepfile.cmake: ${CONJURE_DEPFILE} names '${pending}', which is not an "
            "existing file (the line was '${line}')")
    endif()
    set(${outDependencies} "${dependencies}" PARENT_SCOPE)
endfunction()


if(NOT EXISTS "${depfilePath}")
    message(FATAL_ERROR "ConjureShaderDepfile.cmake: the compiler wrote no depfile at ${CONJURE_DEPFILE}")
endif()
file(READ "${depfilePath}" content)
string(REPLACE "\r" "" content "${content}")

# The lines are walked with a regex rather than a list so a ';' in a path survives.
set(dependencies "")
set(sawTarget FALSE)
set(rest "${content}")
while(NOT rest STREQUAL "")
    set(line "${rest}")
    set(rest "")
    if(line MATCHES "^([^\n]*)\n(.*)$")
        set(line "${CMAKE_MATCH_1}")
        set(rest "${CMAKE_MATCH_2}")
    endif()
    # A line ends in " \" when another one follows; the leading blanks only indent it.
    string(REGEX REPLACE "[ \t]*\\\\$" "" line "${line}")
    string(REGEX REPLACE "^[ \t]+" "" line "${line}")
    if(line STREQUAL "")
        continue()
    endif()
    if(NOT sawTarget)
        string(LENGTH "${CONJURE_DEPFILE_TARGET}:" prefixLength)
        string(SUBSTRING "${line}" 0 ${prefixLength} prefix)
        if(NOT prefix STREQUAL "${CONJURE_DEPFILE_TARGET}:")
            message(FATAL_ERROR "ConjureShaderDepfile.cmake: ${CONJURE_DEPFILE} does not start with the target "
                "'${CONJURE_DEPFILE_TARGET}:' (its first line is '${line}')")
        endif()
        string(SUBSTRING "${line}" ${prefixLength} -1 line)
        string(REGEX REPLACE "^[ \t]+" "" line "${line}")
        set(sawTarget TRUE)
        if(line STREQUAL "")
            continue()
        endif()
    endif()
    conjure_depfile_split_line("${line}" dependencies)
endwhile()
if(NOT sawTarget)
    message(FATAL_ERROR "ConjureShaderDepfile.cmake: ${CONJURE_DEPFILE} is empty; it must name the target "
        "'${CONJURE_DEPFILE_TARGET}'")
endif()

conjure_depfile_absolute("${CONJURE_DEPFILE_TARGET}" targetAbsolute)
conjure_depfile_escape("${targetAbsolute}" targetEscaped)
set(rewritten "${targetEscaped}:")
while(dependencies MATCHES "^([^\n]*)\n(.*)$")
    set(dependency "${CMAKE_MATCH_1}")
    set(dependencies "${CMAKE_MATCH_2}")
    conjure_depfile_absolute("${dependency}" dependencyAbsolute)
    conjure_depfile_escape("${dependencyAbsolute}" dependencyEscaped)
    string(APPEND rewritten " \\\n  ${dependencyEscaped}")
endwhile()
file(WRITE "${depfilePath}" "${rewritten}\n")
