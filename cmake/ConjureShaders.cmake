# Shader build (docs/design/02_renderer.md 3.12, R14).
#
#   conjure_add_shaders(
#       TARGET       <name>                 custom target that builds every blob and the index
#       OUTPUT_DIR   <dir>                  where <permutation>.dxil / .spv and index.json land
#       [SOURCE_DIR  <dir>]                 where the sources live (default: the calling directory)
#       [INCLUDE_DIRS <dirs...>]            -I directories (engine/shaders/shared)
#       [DEPENDENCIES <files...>]           headers every shader depends on: rebuild triggers for generators
#                                           without depfiles, and part of sourceHash (below)
#       SHADERS      "source;entry;stage;defines" ...
#   )
#
# Each SHADERS entry names one HLSL source, an entry point, a stage (vertex | pixel | compute) and a
# define string: a '|'-separated list of NAME=v1,v2 axes whose cartesian product is enumerated, each
# permutation becoming one blob per enabled backend. Per permutation:
#
#   DXIL  (CONJURE_RHI_D3D12)   dxc -T <vs|ps|cs>_6_5 -E <entry> -HV 2018 -Zpr -WX -D CONJURE_D3D12=1
#                               -D<defines> -I <dirs> -Fo <out>.dxil, plus -Zi -Qembed_debug in Debug/Development
#                               and -O3 in Distribution; a second dxc run with -MF <out>.d writes the depfile
#                               (dxc 1.8 and 1.9 treat -MF / -MD as dependency-only modes and compile nothing)
#   SPIR-V (CONJURE_RHI_VULKAN) glslang -D -S <vert|frag|comp> -e <entry> -V --target-env vulkan1.2 --hlsl-iomap
#                               --auto-map-locations with NVRHI's default binding shifts (t:0 s:128 b:256 u:384,
#                               SSBO 0) -DCONJURE_VULKAN=1 -D<defines> -I<dirs> --depfile <out>.d -o <out>.spv
#                               (-g in Debug, -Os otherwise); or dxc -spirv with the equivalent -fvk-*-shift
#                               options when CONJURE_SPIRV_COMPILER=dxc
#
# The function writes <OUTPUT_DIR>/index.json at build time (so sourceHash follows the sources)
# with one entry { name, entry, stage, defines, dxil, spirv, sourceHash } per permutation; blob
# paths are relative to OUTPUT_DIR and empty for a backend that is off. sourceHash is the SHA-256 of
# the source file's SHA-256 followed by the SHA-256 of every DEPENDENCIES file, in list order, so it
# changes when the source or a listed header changes; headers found only through depfiles are not
# part of it, which is why engine/shaders lists every .hlsli explicitly. rhi::shader_library reads it.
#
# Depfiles: under the Ninja and Makefile generators every blob's custom command carries DEPFILE <out>.d, so a
# header found through #include rebuilds the blobs that include it. dxc writes its depfile without escaping a
# space, and a checkout path with a space in it would split every path into files that do not exist and leave
# every blob dirty on every build, so each dxc depfile (DXIL, and SPIR-V through dxc -spirv) is rewritten by
# ConjureShaderDepfile.cmake in the same custom command: every path absolute and escaped as CMake's depfile
# reader expects. glslang (15.1.0) escapes its own depfile and is left alone. CMake hands a '#' or '$' in a
# path on to Ninja unescaped (CMake 4.3), so a header with one of those in its path still leaves its blobs
# dirty under Ninja. Other generators (Visual Studio) rebuild a blob only when its source or a DEPENDENCIES
# file changes.
#
# Compilers: dxc is looked up through CONJURE_DXC_EXECUTABLE, then the Windows SDK
# (Windows Kits/10/bin/<version>/x64, newest first), the Vulkan SDK and PATH. glslang comes from the
# glslang-standalone target when third_party fetched it (CONJURE_GLSLANG_EXECUTABLE), otherwise from
# CONJURE_GLSLANG_EXECUTABLE / PATH. A compiler a backend needs but nobody can find is a configure error.

include_guard(GLOBAL)

set(CONJURE_SPIRV_COMPILER "glslang" CACHE STRING "HLSL to SPIR-V compiler for the Vulkan backend: glslang or dxc")
set_property(CACHE CONJURE_SPIRV_COMPILER PROPERTY STRINGS glslang dxc)

set(CONJURE_SHADER_INDEX_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/ConjureShaderIndex.cmake")
set(CONJURE_SHADER_DEPFILE_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/ConjureShaderDepfile.cmake")


# Finds dxc.exe / dxc, preferring an explicit CONJURE_DXC_EXECUTABLE, then the newest Windows SDK.
function(conjure_find_dxc)
    if(CONJURE_DXC_EXECUTABLE AND EXISTS "${CONJURE_DXC_EXECUTABLE}")
        return()
    endif()

    set(searchDirectories "")

    # Windows Kits: the registry root first, then the default installation path of either Program Files.
    set(kitRoots "")
    if(WIN32)
        get_filename_component(registryRoot
            "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
        if(registryRoot AND IS_DIRECTORY "${registryRoot}")
            list(APPEND kitRoots "${registryRoot}")
        endif()
        foreach(variable IN ITEMS "ProgramFiles(x86)" "ProgramFiles")
            if(DEFINED ENV{${variable}})
                list(APPEND kitRoots "$ENV{${variable}}/Windows Kits/10")
            endif()
        endforeach()
        if(CMAKE_WINDOWS_KITS_10_DIR)
            list(APPEND kitRoots "${CMAKE_WINDOWS_KITS_10_DIR}")
        endif()
    endif()
    list(REMOVE_DUPLICATES kitRoots)

    foreach(kitRoot IN LISTS kitRoots)
        # The SDK ships one bin directory per version; the newest one carries the newest dxc.
        file(GLOB versionDirectories LIST_DIRECTORIES true "${kitRoot}/bin/10.*")
        list(SORT versionDirectories COMPARE NATURAL ORDER DESCENDING)
        foreach(versionDirectory IN LISTS versionDirectories)
            if(IS_DIRECTORY "${versionDirectory}/x64")
                list(APPEND searchDirectories "${versionDirectory}/x64")
            endif()
        endforeach()
    endforeach()

    if(DEFINED ENV{VULKAN_SDK})
        list(APPEND searchDirectories "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
    endif()

    find_program(CONJURE_DXC_EXECUTABLE
        NAMES dxc dxc.exe
        HINTS ${searchDirectories}
        DOC "DirectX Shader Compiler used by conjure_add_shaders()"
    )
endfunction()


# Finds the glslang standalone compiler: the fetched target when present, else an executable.
function(conjure_find_glslang outExecutable outDependency)
    if(TARGET glslang-standalone)
        set(${outExecutable} "$<TARGET_FILE:glslang-standalone>" PARENT_SCOPE)
        set(${outDependency} glslang-standalone PARENT_SCOPE)
        return()
    endif()
    if(CONJURE_GLSLANG_EXECUTABLE AND NOT CONJURE_GLSLANG_EXECUTABLE MATCHES "^\\$<" AND EXISTS "${CONJURE_GLSLANG_EXECUTABLE}")
        set(${outExecutable} "${CONJURE_GLSLANG_EXECUTABLE}" PARENT_SCOPE)
        set(${outDependency} "" PARENT_SCOPE)
        return()
    endif()
    find_program(CONJURE_GLSLANG_FOUND_EXECUTABLE
        NAMES glslang glslangValidator
        HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
        DOC "glslang standalone compiler used by conjure_add_shaders()"
    )
    set(${outExecutable} "${CONJURE_GLSLANG_FOUND_EXECUTABLE}" PARENT_SCOPE)
    set(${outDependency} "" PARENT_SCOPE)
endfunction()


# Expands "NAME=v1,v2|OTHER=a" into every "NAME=v1|OTHER=a" combination, axis order preserved.
# An empty define string yields one empty permutation, carried as the token __NONE__ because a
# CMake list cannot hold an empty element.
function(conjure_shader_permutations defines outPermutations)
    set(result "__NONE__")
    if(NOT defines STREQUAL "")
        string(REPLACE "|" ";" axes "${defines}")
        foreach(axis IN LISTS axes)
            if(NOT axis MATCHES "^([A-Za-z_][A-Za-z0-9_]*)=([^=]+)$")
                message(FATAL_ERROR "conjure_add_shaders: define axis '${axis}' is not NAME=value[,value...]")
            endif()
            set(axisName "${CMAKE_MATCH_1}")
            string(REPLACE "," ";" axisValues "${CMAKE_MATCH_2}")
            set(next "")
            foreach(existing IN LISTS result)
                foreach(value IN LISTS axisValues)
                    if(existing STREQUAL "__NONE__")
                        list(APPEND next "${axisName}=${value}")
                    else()
                        list(APPEND next "${existing}|${axisName}=${value}")
                    endif()
                endforeach()
            endforeach()
            set(result "${next}")
        endforeach()
    endif()
    set(${outPermutations} "${result}" PARENT_SCOPE)
endfunction()


function(conjure_add_shaders)
    set(options "")
    set(oneValueArgs TARGET OUTPUT_DIR SOURCE_DIR)
    set(multiValueArgs INCLUDE_DIRS DEPENDENCIES SHADERS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_shaders: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    foreach(required IN ITEMS TARGET OUTPUT_DIR SHADERS)
        if(NOT ARG_${required})
            message(FATAL_ERROR "conjure_add_shaders: ${required} is required")
        endif()
    endforeach()
    if(NOT ARG_SOURCE_DIR)
        set(ARG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set(wantDxil ${CONJURE_RHI_D3D12})
    set(wantSpirv ${CONJURE_RHI_VULKAN})
    if(NOT wantDxil AND NOT wantSpirv)
        message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): neither CONJURE_RHI_D3D12 nor CONJURE_RHI_VULKAN is on")
    endif()

    # Compilers.
    set(spirvWithDxc FALSE)
    if(wantSpirv AND CONJURE_SPIRV_COMPILER STREQUAL "dxc")
        set(spirvWithDxc TRUE)
    elseif(wantSpirv AND NOT CONJURE_SPIRV_COMPILER STREQUAL "glslang")
        message(FATAL_ERROR "CONJURE_SPIRV_COMPILER must be glslang or dxc, got '${CONJURE_SPIRV_COMPILER}'")
    endif()
    if(wantDxil OR spirvWithDxc)
        conjure_find_dxc()
        if(NOT CONJURE_DXC_EXECUTABLE)
            message(FATAL_ERROR
                "conjure_add_shaders(${ARG_TARGET}): dxc was not found. Install the Windows SDK (dxc.exe ships in "
                "Windows Kits/10/bin/<version>/x64) or the Vulkan SDK, or set CONJURE_DXC_EXECUTABLE.")
        endif()
        message(STATUS "shaders: dxc at ${CONJURE_DXC_EXECUTABLE}")
    endif()
    set(glslangExecutable "")
    set(glslangDependency "")
    if(wantSpirv AND NOT spirvWithDxc)
        conjure_find_glslang(glslangExecutable glslangDependency)
        if(NOT glslangExecutable)
            message(FATAL_ERROR
                "conjure_add_shaders(${ARG_TARGET}): glslang was not found. Enable the fetched glslang in third_party, "
                "install the Vulkan SDK, set CONJURE_GLSLANG_EXECUTABLE, or set CONJURE_SPIRV_COMPILER=dxc.")
        endif()
        message(STATUS "shaders: glslang at ${glslangExecutable}")
    endif()

    # Ninja and Makefile generators consume depfiles; the others fall back to the listed DEPENDENCIES.
    # The dxc commands of a depfile generator also run ConjureShaderDepfile.cmake, so they depend on it.
    set(useDepfile FALSE)
    set(depfileScriptDependency "")
    if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
        set(useDepfile TRUE)
        set(depfileScriptDependency "${CONJURE_SHADER_DEPFILE_SCRIPT}")
    endif()

    set(includeArgumentsDxc "")
    set(includeArgumentsGlslang "")
    foreach(includeDirectory IN LISTS ARG_INCLUDE_DIRS)
        list(APPEND includeArgumentsDxc -I "${includeDirectory}")
        list(APPEND includeArgumentsGlslang "-I${includeDirectory}")
    endforeach()

    # Configuration-dependent flags: one generator expression per flag (an empty expansion is dropped
    # by COMMAND_EXPAND_LISTS) because a ';' inside an expression would be split before evaluation.
    set(dxcConfigFlags
        "$<$<CONFIG:Distribution>:-O3>"
        "$<$<NOT:$<CONFIG:Distribution>>:-Zi>"
        "$<$<NOT:$<CONFIG:Distribution>>:-Qembed_debug>"
    )
    set(glslangConfigFlags "$<IF:$<CONFIG:Debug>,-g,-Os>")

    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")
    file(RELATIVE_PATH outputRelative "${PROJECT_BINARY_DIR}" "${ARG_OUTPUT_DIR}")
    if(outputRelative MATCHES "^\\.\\." OR IS_ABSOLUTE "${outputRelative}")
        message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): OUTPUT_DIR must lie inside the build directory")
    endif()

    # The index script receives every DEPENDENCIES path tab-separated after the source path; a path
    # with a tab in it cannot be carried that way.
    set(dependencyFields "")
    foreach(dependency IN LISTS ARG_DEPENDENCIES)
        if(dependency MATCHES "\t")
            message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): dependency path '${dependency}' contains a tab")
        endif()
        if(NOT EXISTS "${dependency}")
            message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): dependency '${dependency}' does not exist")
        endif()
        string(APPEND dependencyFields "\t${dependency}")
    endforeach()

    set(blobOutputs "")
    set(sourceFiles "")
    set(indexLines "")
    foreach(entry IN LISTS ARG_SHADERS)
        if(NOT entry MATCHES "^([^;]+);([^;]+);([^;]+);(.*)$")
            message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): entry '${entry}' is not source;entry;stage;defines")
        endif()
        set(sourceName "${CMAKE_MATCH_1}")
        set(entryPoint "${CMAKE_MATCH_2}")
        set(stage "${CMAKE_MATCH_3}")
        set(defines "${CMAKE_MATCH_4}")

        if(stage STREQUAL "vertex")
            set(dxcProfile "vs_6_5")
            set(glslangStage "vert")
        elseif(stage STREQUAL "pixel")
            set(dxcProfile "ps_6_5")
            set(glslangStage "frag")
        elseif(stage STREQUAL "compute")
            set(dxcProfile "cs_6_5")
            set(glslangStage "comp")
        else()
            message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): stage '${stage}' must be vertex, pixel or compute")
        endif()

        set(sourcePath "${ARG_SOURCE_DIR}/${sourceName}")
        if(NOT EXISTS "${sourcePath}")
            message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): shader source '${sourcePath}' does not exist")
        endif()
        if(sourcePath MATCHES "\t")
            message(FATAL_ERROR "conjure_add_shaders(${ARG_TARGET}): source path '${sourcePath}' contains a tab")
        endif()
        list(APPEND sourceFiles "${sourcePath}")
        get_filename_component(shaderName "${sourceName}" NAME_WE)

        conjure_shader_permutations("${defines}" permutations)
        foreach(permutation IN LISTS permutations)
            set(defineArguments "")
            set(permutationDefines "")
            set(permutationSuffix "")
            if(NOT permutation STREQUAL "__NONE__")
                set(permutationDefines "${permutation}")
                string(REPLACE "|" ";" definePairs "${permutation}")
                foreach(pair IN LISTS definePairs)
                    list(APPEND defineArguments "-D${pair}")
                    string(REPLACE "=" "_" pairToken "${pair}")
                    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" pairToken "${pairToken}")
                    string(APPEND permutationSuffix ".${pairToken}")
                endforeach()
            endif()
            set(blobStem "${shaderName}.${entryPoint}${permutationSuffix}")

            set(dxilRelative "")
            if(wantDxil)
                set(dxilRelative "${blobStem}.dxil")
                set(dxilOutput "${ARG_OUTPUT_DIR}/${dxilRelative}")
                set(dxilArguments
                    -T ${dxcProfile} -E ${entryPoint} -HV 2018 -Zpr -WX
                    -D CONJURE_D3D12=1 ${defineArguments}
                    ${includeArgumentsDxc}
                    ${dxcConfigFlags}
                )
                # dxc's -MF / -MD write the dependency list instead of compiling, so the depfile is a
                # second invocation of the same command line; dxc escapes nothing in it, so
                # ConjureShaderDepfile.cmake rewrites it before CMake reads it.
                set(depfileCommand "")
                set(depfileOption "")
                if(useDepfile)
                    set(depfileCommand
                        COMMAND "${CONJURE_DXC_EXECUTABLE}" ${dxilArguments}
                            -Fo "${outputRelative}/${dxilRelative}" -MF "${outputRelative}/${blobStem}.dxil.d"
                            "${sourcePath}"
                        COMMAND "${CMAKE_COMMAND}"
                            -D "CONJURE_DEPFILE=${outputRelative}/${blobStem}.dxil.d"
                            -D "CONJURE_DEPFILE_TARGET=${outputRelative}/${dxilRelative}"
                            -P "${CONJURE_SHADER_DEPFILE_SCRIPT}"
                    )
                    set(depfileOption DEPFILE "${dxilOutput}.d")
                endif()
                add_custom_command(
                    OUTPUT "${dxilOutput}"
                    COMMAND "${CONJURE_DXC_EXECUTABLE}" ${dxilArguments}
                        -Fo "${outputRelative}/${dxilRelative}"
                        "${sourcePath}"
                    ${depfileCommand}
                    DEPENDS "${sourcePath}" ${ARG_DEPENDENCIES} ${depfileScriptDependency}
                    ${depfileOption}
                    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
                    COMMENT "dxc ${blobStem} (${dxcProfile})"
                    VERBATIM
                    COMMAND_EXPAND_LISTS
                )
                list(APPEND blobOutputs "${dxilOutput}")
            endif()

            set(spirvRelative "")
            if(wantSpirv)
                set(spirvRelative "${blobStem}.spv")
                set(spirvOutput "${ARG_OUTPUT_DIR}/${spirvRelative}")
                set(depfileOption "")
                if(useDepfile)
                    set(depfileOption DEPFILE "${spirvOutput}.d")
                endif()
                if(spirvWithDxc)
                    set(spirvArguments
                        -spirv -fspv-target-env=vulkan1.2
                        -fvk-t-shift 0 all -fvk-s-shift 128 all -fvk-b-shift 256 all -fvk-u-shift 384 all
                        -T ${dxcProfile} -E ${entryPoint} -HV 2018 -Zpr -WX
                        -D CONJURE_VULKAN=1 ${defineArguments}
                        ${includeArgumentsDxc}
                        "$<IF:$<CONFIG:Distribution>,-O3,-Zi>"
                    )
                    set(depfileCommand "")
                    if(useDepfile)
                        set(depfileCommand
                            COMMAND "${CONJURE_DXC_EXECUTABLE}" ${spirvArguments}
                                -Fo "${outputRelative}/${spirvRelative}" -MF "${outputRelative}/${blobStem}.spv.d"
                                "${sourcePath}"
                            COMMAND "${CMAKE_COMMAND}"
                                -D "CONJURE_DEPFILE=${outputRelative}/${blobStem}.spv.d"
                                -D "CONJURE_DEPFILE_TARGET=${outputRelative}/${spirvRelative}"
                                -P "${CONJURE_SHADER_DEPFILE_SCRIPT}"
                        )
                    endif()
                    add_custom_command(
                        OUTPUT "${spirvOutput}"
                        COMMAND "${CONJURE_DXC_EXECUTABLE}" ${spirvArguments}
                            -Fo "${outputRelative}/${spirvRelative}"
                            "${sourcePath}"
                        ${depfileCommand}
                        DEPENDS "${sourcePath}" ${ARG_DEPENDENCIES} ${depfileScriptDependency}
                        ${depfileOption}
                        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
                        COMMENT "dxc -spirv ${blobStem} (${dxcProfile})"
                        VERBATIM
                        COMMAND_EXPAND_LISTS
                    )
                else()
                    set(depfileArguments "")
                    if(useDepfile)
                        set(depfileArguments --depfile "${outputRelative}/${blobStem}.spv.d")
                    endif()
                    add_custom_command(
                        OUTPUT "${spirvOutput}"
                        COMMAND "${glslangExecutable}"
                            -D -S ${glslangStage} -e ${entryPoint} -V --target-env vulkan1.2
                            --hlsl-iomap --auto-map-locations
                            --shift-texture-binding 0 --shift-sampler-binding 128
                            --shift-ubo-binding 256 --shift-uav-binding 384 --shift-ssbo-binding 0
                            -DCONJURE_VULKAN=1 ${defineArguments}
                            ${includeArgumentsGlslang}
                            "${glslangConfigFlags}"
                            ${depfileArguments}
                            -o "${outputRelative}/${spirvRelative}"
                            "${sourcePath}"
                        DEPENDS "${sourcePath}" ${ARG_DEPENDENCIES} ${glslangDependency}
                        ${depfileOption}
                        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
                        COMMENT "glslang ${blobStem} (${glslangStage})"
                        VERBATIM
                        COMMAND_EXPAND_LISTS
                    )
                endif()
                list(APPEND blobOutputs "${spirvOutput}")
            endif()

            # One tab-separated line per permutation for the index script (no field may contain a tab):
            # name, entry, stage, defines, dxil, spirv, source path, then every dependency path.
            list(APPEND indexLines
                "${shaderName}\t${entryPoint}\t${stage}\t${permutationDefines}\t${dxilRelative}\t${spirvRelative}\t${sourcePath}${dependencyFields}")
        endforeach()
    endforeach()

    # The entries file is written through a staging copy so an unchanged list does not rebuild the index.
    set(entriesFile "${CMAKE_CURRENT_BINARY_DIR}/${ARG_TARGET}_entries.txt")
    string(REPLACE ";" "\n" entriesText "${indexLines}")
    file(WRITE "${entriesFile}.in" "${entriesText}\n")
    configure_file("${entriesFile}.in" "${entriesFile}" COPYONLY)

    set(indexOutput "${ARG_OUTPUT_DIR}/index.json")
    add_custom_command(
        OUTPUT "${indexOutput}"
        COMMAND "${CMAKE_COMMAND}"
            -D "CONJURE_SHADER_ENTRIES=${entriesFile}"
            -D "CONJURE_SHADER_INDEX=${indexOutput}"
            -P "${CONJURE_SHADER_INDEX_SCRIPT}"
        DEPENDS "${entriesFile}" "${CONJURE_SHADER_INDEX_SCRIPT}" ${sourceFiles} ${ARG_DEPENDENCIES}
        COMMENT "shader index ${outputRelative}/index.json"
        VERBATIM
    )

    add_custom_target(${ARG_TARGET} ALL
        DEPENDS ${blobOutputs} "${indexOutput}"
        SOURCES ${sourceFiles} ${ARG_DEPENDENCIES}
    )
    set_target_properties(${ARG_TARGET} PROPERTIES
        FOLDER "Engine/shaders"
        CONJURE_SHADER_OUTPUT_DIR "${ARG_OUTPUT_DIR}"
    )
    list(LENGTH blobOutputs blobCount)
    message(STATUS "shaders: ${ARG_TARGET} builds ${blobCount} blobs into ${ARG_OUTPUT_DIR}")
endfunction()
