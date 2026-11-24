# Incremental shader builds from a path with spaces (CTest entry build.shader_depfile_no_rebuild, registered by
# tests/renderer/CMakeLists.txt for the Ninja and Makefile generators, the ones that read depfiles):
#
#   cmake -D CONJURE_PROBE_ROOT=<scratch directory> -D CONJURE_PROBE_CMAKE_DIR=<repository>/cmake
#         -D CONJURE_PROBE_GENERATOR=<generator> -D CONJURE_PROBE_MAKE_PROGRAM=<ninja or make>
#         -D CONJURE_RHI_D3D12=<ON|OFF> -D CONJURE_RHI_VULKAN=<ON|OFF> -D CONJURE_SPIRV_COMPILER=<glslang|dxc>
#         [-D CONJURE_DXC_EXECUTABLE=<dxc>] [-D CONJURE_GLSLANG_EXECUTABLE=<glslang>] -P shader_depfile_probe.cmake
#
# A throwaway project (LANGUAGES NONE) under "<root>/probe source" compiles one pixel shader, "probe
# shader.hlsl", with conjure_add_shaders() for every backend the calling build compiles shaders for. Like
# engine/shaders, the call sits in a subdirectory ("shader dir"), so CMake's current binary directory is not
# the directory the compilers run in. The shader includes "spaced header.hlsli" from the include directory
# "inc dir" and "local header.hlsli" from its own directory, and neither header is a DEPENDENCIES entry, so
# only the depfile tells the generator about them. The probe passes when, in order: the first build compiles
# the shader; a second build compiles nothing (a depfile path split at a space names files that do not exist
# and leaves the shader dirty forever); every depfile names both headers as one path each (their spaces
# escaped); and, for each header in turn, a build after the header changed compiles the shader again and the
# build after that compiles nothing.
#
# Only a tree that compiles a blob with dxc (CONJURE_RHI_D3D12, or CONJURE_SPIRV_COMPILER=dxc) runs
# ConjureShaderDepfile.cmake. In a glslang-only tree the probe checks glslang's own depfile and says so.

foreach(required IN ITEMS CONJURE_PROBE_ROOT CONJURE_PROBE_CMAKE_DIR CONJURE_PROBE_GENERATOR
        CONJURE_PROBE_MAKE_PROGRAM)
    if(NOT ${required})
        message(FATAL_ERROR "shader_depfile_probe.cmake needs ${required}")
    endif()
endforeach()

set(sourceDirectory "${CONJURE_PROBE_ROOT}/probe source")
set(shaderDirectory "${sourceDirectory}/shader dir")
set(buildDirectory "${CONJURE_PROBE_ROOT}/probe build")
set(spacedHeader "${shaderDirectory}/inc dir/spaced header.hlsli")
set(localHeader "${shaderDirectory}/local header.hlsli")
file(REMOVE_RECURSE "${CONJURE_PROBE_ROOT}")

file(WRITE "${sourceDirectory}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.28)
project(conjure_shader_depfile_probe NONE)
include("${CONJURE_PROBE_CMAKE_DIR}/ConjureShaders.cmake")
add_subdirectory("shader dir")
]=])
file(WRITE "${shaderDirectory}/CMakeLists.txt" [=[
conjure_add_shaders(
    TARGET probe_shaders
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/shaders"
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/inc dir"
    SHADERS "probe shader.hlsl;ps_main;pixel;"
)
]=])
file(WRITE "${shaderDirectory}/probe shader.hlsl" [=[
#include "spaced header.hlsli"
#include "local header.hlsli"

float4 ps_main() : SV_Target
{
    return probe_spaced() + probe_local();
}
]=])
file(WRITE "${spacedHeader}" "float4 probe_spaced() { return float4(0.25, 0.25, 0.25, 1.0); }\n")
file(WRITE "${localHeader}" "float4 probe_local() { return float4(0.5, 0.5, 0.5, 0.0); }\n")

set(configureArguments
    -G "${CONJURE_PROBE_GENERATOR}"
    "-DCMAKE_MAKE_PROGRAM=${CONJURE_PROBE_MAKE_PROGRAM}"
    -DCMAKE_BUILD_TYPE=Debug
    "-DCONJURE_PROBE_CMAKE_DIR=${CONJURE_PROBE_CMAKE_DIR}"
    "-DCONJURE_RHI_D3D12=${CONJURE_RHI_D3D12}"
    "-DCONJURE_RHI_VULKAN=${CONJURE_RHI_VULKAN}"
    "-DCONJURE_SPIRV_COMPILER=${CONJURE_SPIRV_COMPILER}"
)
if(CONJURE_DXC_EXECUTABLE)
    list(APPEND configureArguments "-DCONJURE_DXC_EXECUTABLE=${CONJURE_DXC_EXECUTABLE}")
endif()
if(CONJURE_GLSLANG_EXECUTABLE)
    list(APPEND configureArguments "-DCONJURE_GLSLANG_EXECUTABLE=${CONJURE_GLSLANG_EXECUTABLE}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${sourceDirectory}" -B "${buildDirectory}" ${configureArguments}
    RESULT_VARIABLE configureResult
    OUTPUT_VARIABLE configureOutput
    ERROR_VARIABLE configureError
)
if(NOT configureResult EQUAL 0)
    message(FATAL_ERROR "the probe project does not configure (${configureResult}):\n"
        "${configureOutput}${configureError}")
endif()

# The blobs the probe builds, the depfile beside each one and the compiler that writes it.
set(depfiles "")
set(compilers "")
set(dxcExercised FALSE)
if(CONJURE_RHI_D3D12)
    list(APPEND depfiles "${buildDirectory}/shaders/probe shader.ps_main.dxil.d")
    list(APPEND compilers "dxc")
    set(dxcExercised TRUE)
endif()
if(CONJURE_RHI_VULKAN)
    list(APPEND depfiles "${buildDirectory}/shaders/probe shader.ps_main.spv.d")
    if(CONJURE_SPIRV_COMPILER STREQUAL "dxc")
        list(APPEND compilers "dxc -spirv")
        set(dxcExercised TRUE)
    else()
        list(APPEND compilers "${CONJURE_SPIRV_COMPILER}")
    endif()
endif()
if(depfiles STREQUAL "")
    message(FATAL_ERROR "neither CONJURE_RHI_D3D12 nor CONJURE_RHI_VULKAN is on: the probe has no shader to build")
endif()
list(JOIN compilers ", " compilerNames)
if(NOT dxcExercised)
    message(STATUS "shader depfile probe: no blob of this tree is compiled by dxc, so the ConjureShaderDepfile.cmake "
        "rewrite is not exercised here (only the depfile of ${compilerNames} is)")
endif()


# Builds the probe and sets outCompiled to the number of shader compiles the build printed. Every compile
# prints the COMMENT of its custom command, "dxc probe shader.ps_main (ps_6_5)", "glslang probe
# shader.ps_main (frag)" or "dxc -spirv probe shader.ps_main (ps_6_5)"; the index line does not match.
function(conjure_probe_build step outCompiled)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${buildDirectory}" --config Debug
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "build ${step} of the probe failed (${result}):\n${output}${error}")
    endif()
    string(REGEX MATCHALL "probe shader[.]ps_main [(]" compiles "${output}")
    list(LENGTH compiles compiled)
    message(STATUS "build ${step}: ${compiled} shader compile(s)")
    set(${outCompiled} ${compiled} PARENT_SCOPE)
    set(probeBuildOutput "${output}" PARENT_SCOPE)
endfunction()


list(LENGTH depfiles blobCount)

conjure_probe_build("1 (from scratch)" compiled)
if(NOT compiled EQUAL blobCount)
    message(FATAL_ERROR "the first build compiled ${compiled} shader(s), not ${blobCount}:\n${probeBuildOutput}")
endif()

conjure_probe_build("2 (nothing changed)" compiled)
if(NOT compiled EQUAL 0)
    message(FATAL_ERROR "the second build compiled ${compiled} shader(s) although nothing changed:\n"
        "${probeBuildOutput}")
endif()

# Every depfile names both headers as one path each once it is read the way CMake and Ninja read a depfile:
# a '\' before a line end continues the line, "\ " is a space inside a path, "\:" a colon (glslang escapes the
# one of a drive letter) and any other blank separates two paths. An unescaped space in a header's path
# splits it and the header is not found.
foreach(depfile IN LISTS depfiles)
    if(NOT EXISTS "${depfile}")
        message(FATAL_ERROR "the builds wrote no depfile at ${depfile}")
    endif()
    file(READ "${depfile}" depfileText)
    set(words "${depfileText}")
    string(REPLACE "\r" "" words "${words}")
    string(REPLACE "\\\n" " " words "${words}")
    string(REPLACE "\\ " "<space>" words "${words}")
    string(REPLACE "\\:" ":" words "${words}")
    string(REGEX REPLACE "[ \t\n]+" ";" words "${words}")
    set(paths "")
    foreach(word IN LISTS words)
        string(REPLACE "<space>" " " path "${word}")
        if(CMAKE_HOST_WIN32)
            string(REPLACE "\\" "/" path "${path}")
        endif()
        list(APPEND paths "${path}")
    endforeach()
    foreach(header IN ITEMS "${spacedHeader}" "${localHeader}")
        list(FIND paths "${header}" headerIndex)
        if(headerIndex EQUAL -1)
            message(FATAL_ERROR "${depfile} does not name '${header}' as one path:\n${depfileText}")
        endif()
    endforeach()
endforeach()

# Each header in turn: the build after it changed compiles the shader again, the one after that nothing. The
# pause keeps a file system that stores whole seconds from giving the touched header the time of the blob.
set(step 3)
foreach(header IN ITEMS "${spacedHeader}" "${localHeader}")
    get_filename_component(headerName "${header}" NAME)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1.1)
    file(TOUCH "${header}")
    conjure_probe_build("${step} ('${headerName}' changed)" compiled)
    if(NOT compiled EQUAL blobCount)
        message(FATAL_ERROR "build ${step}, after '${header}' changed, compiled ${compiled} shader(s), not "
            "${blobCount}:\n${probeBuildOutput}")
    endif()
    math(EXPR step "${step} + 1")

    conjure_probe_build("${step} (nothing changed)" compiled)
    if(NOT compiled EQUAL 0)
        message(FATAL_ERROR "build ${step} compiled ${compiled} shader(s) although nothing changed since '${header}' "
            "did:\n${probeBuildOutput}")
    endif()
    math(EXPR step "${step} + 1")
endforeach()

message(STATUS "shader depfile probe (${compilerNames}): no shader compiles again until a header it includes "
    "changes")
