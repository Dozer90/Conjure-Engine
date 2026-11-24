# The depfile rewrite of cmake/ConjureShaderDepfile.cmake case by case (CTest entry build.shader_depfile_rewrite,
# registered by tests/renderer/CMakeLists.txt):
#
#   cmake -D CONJURE_TEST_ROOT=<scratch directory> -D CONJURE_DEPFILE_SCRIPT=<ConjureShaderDepfile.cmake>
#         -P shader_depfile_rewrite.cmake
#
# Every case writes a depfile the way a compiler would, runs the script with <root>/work as its working
# directory and compares the rewritten file byte for byte, or expects the script to fail with a message. The
# files the depfiles name are created under <root>, whose own name has a space, so every expected path has an
# escaped one. The script stops at the first case that does not hold and names it.

if(NOT CONJURE_TEST_ROOT OR NOT CONJURE_DEPFILE_SCRIPT)
    message(FATAL_ERROR "shader_depfile_rewrite.cmake needs CONJURE_TEST_ROOT and CONJURE_DEPFILE_SCRIPT")
endif()

set(root "${CONJURE_TEST_ROOT}/rewrite root")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}/work")
# The script resolves relative paths against its working directory, which Linux and macOS report with every
# link resolved (a macOS /tmp is /private/tmp), so the expected paths start from the resolved root there. Windows
# reports the working directory as it was given.
if(NOT CMAKE_HOST_WIN32)
    file(REAL_PATH "${root}" root)
endif()
set(work "${root}/work")

# The escaped spelling of the root as the rewritten depfile names it.
string(REPLACE " " "\\ " rootEscaped "${root}")


# Creates an empty file and any directory it needs.
function(conjure_test_make_file path)
    get_filename_component(directory "${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    file(WRITE "${path}" "")
endfunction()


# Runs the script over a depfile holding `content` with `target` as CONJURE_DEPFILE_TARGET, then sets
# outResult, outMessage (stdout and stderr) and outRewritten (the depfile afterwards) in the caller.
function(conjure_test_run_rewrite caseName content target outResult outMessage outRewritten)
    set(depfile "${work}/${caseName}.d")
    file(REMOVE "${depfile}")
    if(NOT content STREQUAL "<no depfile>")
        file(WRITE "${depfile}" "${content}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -D "CONJURE_DEPFILE=${caseName}.d" -D "CONJURE_DEPFILE_TARGET=${target}"
            -P "${CONJURE_DEPFILE_SCRIPT}"
        WORKING_DIRECTORY "${work}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(rewritten "")
    if(EXISTS "${depfile}")
        file(READ "${depfile}" rewritten)
    endif()
    set(${outResult} "${result}" PARENT_SCOPE)
    set(${outMessage} "${output}${error}" PARENT_SCOPE)
    set(${outRewritten} "${rewritten}" PARENT_SCOPE)
endfunction()


# Expects the rewrite to succeed and leave exactly `expected` in the depfile.
function(conjure_test_expect_rewrite caseName content target expected)
    conjure_test_run_rewrite("${caseName}" "${content}" "${target}" result messageText rewritten)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "case '${caseName}': the rewrite failed (${result}):\n${messageText}")
    endif()
    if(NOT rewritten STREQUAL expected)
        message(FATAL_ERROR "case '${caseName}': the depfile reads\n${rewritten}\ninstead of\n${expected}")
    endif()
    message(STATUS "case '${caseName}': passed")
endfunction()


# Expects a run of the script that ended with `result` to have failed with a message matching `pattern`. CMake
# wraps the text of an error at spaces, so every space in the pattern also matches a line break.
function(conjure_test_check_failure caseName result messageText pattern)
    string(REPLACE " " "[ \n]+" pattern "${pattern}")
    if(result EQUAL 0)
        message(FATAL_ERROR "case '${caseName}': the script succeeded although it should have failed:\n${messageText}")
    endif()
    if(NOT messageText MATCHES "${pattern}")
        message(FATAL_ERROR "case '${caseName}': the message does not match '${pattern}':\n${messageText}")
    endif()
    message(STATUS "case '${caseName}': failed as expected")
endfunction()


# Expects the rewrite to fail with a message matching `pattern` (see conjure_test_check_failure).
function(conjure_test_expect_failure caseName content target pattern)
    conjure_test_run_rewrite("${caseName}" "${content}" "${target}" result messageText rewritten)
    if(result EQUAL 0)
        message(FATAL_ERROR "case '${caseName}': the rewrite succeeded and wrote\n${rewritten}")
    endif()
    conjure_test_check_failure("${caseName}" "${result}" "${messageText}" "${pattern}")
endfunction()


conjure_test_make_file("${root}/src dir/main shader.hlsl")
conjure_test_make_file("${root}/src dir/inc dir/x y.hlsli")
conjure_test_make_file("${root}/src dir/plain.hlsli")
conjure_test_make_file("${work}/rel dir/r.hlsli")

# dxc's layout: the target with the first dependency, then one dependency per line, nothing escaped. dxc on
# Windows joins an include directory and a file name with '\', which becomes '/'; elsewhere it writes '/'.
set(includeSeparator "/")
if(CMAKE_HOST_WIN32)
    set(includeSeparator "\\")
endif()
string(CONCAT dxcLayout
    "out/a b.dxil: ${root}/src dir/main shader.hlsl \\\n"
    " ${root}/src dir/inc dir${includeSeparator}x y.hlsli \\\n"
    " rel dir/r.hlsli\n"
)
string(CONCAT dxcExpected
    "${rootEscaped}/work/out/a\\ b.dxil: \\\n"
    "  ${rootEscaped}/src\\ dir/main\\ shader.hlsl \\\n"
    "  ${rootEscaped}/src\\ dir/inc\\ dir/x\\ y.hlsli \\\n"
    "  ${rootEscaped}/work/rel\\ dir/r.hlsli\n"
)
conjure_test_expect_rewrite(dxc_layout "${dxcLayout}" "out/a b.dxil" "${dxcExpected}")

# The same depfile with Windows line ends reads the same.
string(REPLACE "\n" "\r\n" dxcLayoutCrlf "${dxcLayout}")
conjure_test_expect_rewrite(crlf_line_ends "${dxcLayoutCrlf}" "out/a b.dxil" "${dxcExpected}")

# Several dependencies on one line, some with spaces, are told apart by the files that exist.
set(oneLine "out/a.dxil: ${root}/src dir/plain.hlsli ${root}/src dir/inc dir/x y.hlsli  rel dir/r.hlsli\n")
string(CONCAT oneLineExpected
    "${rootEscaped}/work/out/a.dxil: \\\n"
    "  ${rootEscaped}/src\\ dir/plain.hlsli \\\n"
    "  ${rootEscaped}/src\\ dir/inc\\ dir/x\\ y.hlsli \\\n"
    "  ${rootEscaped}/work/rel\\ dir/r.hlsli\n"
)
conjure_test_expect_rewrite(several_on_one_line "${oneLine}" "out/a.dxil" "${oneLineExpected}")

# A line that names one existing file whole is taken whole, although a shorter prefix of it is a file too,
# and two spaces in a row stay two escaped spaces.
conjure_test_make_file("${root}/pre")
conjure_test_make_file("${root}/pre  fix.hlsli")
conjure_test_expect_rewrite(whole_line_first
    "out/a.dxil: \\\n ${root}/pre  fix.hlsli\n"
    "out/a.dxil"
    "${rootEscaped}/work/out/a.dxil: \\\n  ${rootEscaped}/pre\\ \\ fix.hlsli\n"
)

# '#' and '$' are escaped as \# and $$, and a ';' is kept as it is (no list splits a path).
conjure_test_make_file("${root}/odd/h#a$sh;semi.hlsli")
conjure_test_expect_rewrite(special_characters
    "out/a.dxil: ${root}/odd/h#a$sh;semi.hlsli\n"
    "out/a.dxil"
    "${rootEscaped}/work/out/a.dxil: \\\n  ${rootEscaped}/odd/h\\#a$$sh;semi.hlsli\n"
)

# A depfile that names only its target stays a target without dependencies, also without a final line end.
conjure_test_expect_rewrite(target_only "out/a.dxil:" "out/a.dxil" "${rootEscaped}/work/out/a.dxil:\n")

# A path outside ASCII (a checkout under an accented user name) is read, found and written back byte for byte:
# "Jos" and U+00E9 as UTF-8, precomposed.
string(ASCII 195 169 accent)
conjure_test_make_file("${root}/Jos${accent} dir/h.hlsli")
conjure_test_expect_rewrite(non_ascii_path
    "out/a.dxil: \\\n ${root}/Jos${accent} dir/h.hlsli\n"
    "out/a.dxil"
    "${rootEscaped}/work/out/a.dxil: \\\n  ${rootEscaped}/Jos${accent}\\ dir/h.hlsli\n"
)

# An absolute target stays where it is.
conjure_test_expect_rewrite(absolute_target
    "${root}/abs out/a.dxil: rel dir/r.hlsli\n"
    "${root}/abs out/a.dxil"
    "${rootEscaped}/abs\\ out/a.dxil: \\\n  ${rootEscaped}/work/rel\\ dir/r.hlsli\n"
)

# A '\' is a file name character only where it is not a separator: before a space it is written as three
# backslashes (2N+1 for N=1). Windows has no such file name, so the case runs on Linux and macOS only.
if(CMAKE_HOST_WIN32)
    message(STATUS "case 'backslash_before_space': not run, a Windows file name cannot hold a '\\'")
else()
    conjure_test_make_file("${root}/back\\ slash.hlsli")
    conjure_test_expect_rewrite(backslash_before_space
        "out/a.dxil: \\\n ${root}/back\\ slash.hlsli\n"
        "out/a.dxil"
        "${rootEscaped}/work/out/a.dxil: \\\n  ${rootEscaped}/back\\\\\\ slash.hlsli\n"
    )
endif()

# A tab cannot be escaped in a depfile. Windows has no file name with a tab, so the case runs on Linux and
# macOS only.
if(CMAKE_HOST_WIN32)
    message(STATUS "case 'tab_in_path': not run, a Windows file name cannot hold a tab")
else()
    conjure_test_make_file("${root}/ta\tb.hlsli")
    conjure_test_expect_failure(tab_in_path
        "out/a.dxil: \\\n ${root}/ta\tb.hlsli\n"
        "out/a.dxil"
        "contains a tab or a line"
    )
endif()

# The script needs both of its variables and fails without either, although the depfile it would read is valid.
file(WRITE "${work}/missing_variable.d" "out/a.dxil: rel dir/r.hlsli\n")
foreach(missing IN ITEMS CONJURE_DEPFILE CONJURE_DEPFILE_TARGET)
    set(definitions "CONJURE_DEPFILE=missing_variable.d" "CONJURE_DEPFILE_TARGET=out/a.dxil")
    list(FILTER definitions EXCLUDE REGEX "^${missing}=")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -D ${definitions} -P "${CONJURE_DEPFILE_SCRIPT}"
        WORKING_DIRECTORY "${work}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    conjure_test_check_failure("missing_variable ${missing}" "${result}" "${output}${error}"
        "needs CONJURE_DEPFILE and CONJURE_DEPFILE_TARGET")
endforeach()

conjure_test_expect_failure(no_depfile "<no depfile>" "out/a.dxil" "the compiler wrote no depfile at")
conjure_test_expect_failure(empty_depfile "\n \\\n" "out/a.dxil" "is empty; it must name the target")
conjure_test_expect_failure(other_target
    "out/b.dxil: rel dir/r.hlsli\n"
    "out/a.dxil"
    "does not start with the target"
)
conjure_test_expect_failure(missing_dependency
    "out/a.dxil: \\\n rel dir/r.hlsli \\\n ${root}/src dir/gone.hlsli\n"
    "out/a.dxil"
    "which is not an existing file"
)
# A directory is not a dependency, so a line naming one cannot be split into files.
conjure_test_expect_failure(directory_is_no_dependency
    "out/a.dxil: ${root}/src dir\n"
    "out/a.dxil"
    "which is not an existing file"
)

message(STATUS "shader depfile rewrite: every case passed")
