# Writes shaders/index.json at build time (docs/design/02_renderer.md 3.12). Run by the custom
# command conjure_add_shaders() registers:
#
#   cmake -D CONJURE_SHADER_ENTRIES=<entries.txt> -D CONJURE_SHADER_INDEX=<index.json> -P ConjureShaderIndex.cmake
#
# The entries file (UTF-8) holds one tab-separated line per permutation:
#   name  entry  stage  defines  dxil  spirv  sourcePath  [dependencyPath ...]
# with dxil / spirv relative to the index directory (empty when that backend is off). sourceHash is
# the SHA-256 over the SHA-256 of the source file followed by the SHA-256 of every dependency file
# (the DEPENDENCIES of conjure_add_shaders, in order), so the index changes when the source or a
# listed header changes. A source or dependency that does not exist is a build error, never an
# empty hash.

if(NOT CONJURE_SHADER_ENTRIES OR NOT CONJURE_SHADER_INDEX)
    message(FATAL_ERROR "ConjureShaderIndex.cmake needs CONJURE_SHADER_ENTRIES and CONJURE_SHADER_INDEX")
endif()

# ENCODING UTF-8 keeps non-ASCII checkout paths intact; without it a string ends at the first
# non-ASCII byte and the source path would be truncated.
file(STRINGS "${CONJURE_SHADER_ENTRIES}" entryLines ENCODING UTF-8)

# Hashes one file, or fails the build naming it.
function(conjure_shader_file_hash path outHash)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "ConjureShaderIndex.cmake: shader file '${path}' does not exist")
    endif()
    file(SHA256 "${path}" hash)
    set(${outHash} "${hash}" PARENT_SCOPE)
endfunction()

set(json "{\n  \"version\": 1,\n  \"shaders\": [")
set(first TRUE)
foreach(line IN LISTS entryLines)
    if(line STREQUAL "")
        continue()
    endif()
    # Tabs are the field separator; a list conversion would eat empty fields, so split by regex. The
    # seventh capture holds the source path and, tab-separated after it, the dependency paths.
    if(NOT line MATCHES "^([^\t]*)\t([^\t]*)\t([^\t]*)\t([^\t]*)\t([^\t]*)\t([^\t]*)\t(.*)$")
        message(FATAL_ERROR "ConjureShaderIndex.cmake: malformed entry '${line}'")
    endif()
    set(name "${CMAKE_MATCH_1}")
    set(entry "${CMAKE_MATCH_2}")
    set(stage "${CMAKE_MATCH_3}")
    set(defines "${CMAKE_MATCH_4}")
    set(dxil "${CMAKE_MATCH_5}")
    set(spirv "${CMAKE_MATCH_6}")
    set(pathFields "${CMAKE_MATCH_7}")

    # Paths may contain ';', so the tab-separated tail is split with a regex loop, not list().
    set(paths "")
    while(pathFields MATCHES "^([^\t]*)\t(.*)$")
        list(APPEND paths "${CMAKE_MATCH_1}")
        set(pathFields "${CMAKE_MATCH_2}")
    endwhile()
    list(APPEND paths "${pathFields}")

    set(combined "")
    foreach(path IN LISTS paths)
        conjure_shader_file_hash("${path}" fileHash)
        string(APPEND combined "${fileHash}")
    endforeach()
    string(SHA256 sourceHash "${combined}")

    foreach(field IN ITEMS name entry stage defines dxil spirv sourceHash)
        string(REPLACE "\\" "\\\\" ${field} "${${field}}")
        string(REPLACE "\"" "\\\"" ${field} "${${field}}")
    endforeach()

    if(NOT first)
        string(APPEND json ",")
    endif()
    set(first FALSE)
    string(APPEND json "\n    { \"name\": \"${name}\", \"entry\": \"${entry}\", \"stage\": \"${stage}\", ")
    string(APPEND json "\"defines\": \"${defines}\", \"dxil\": \"${dxil}\", \"spirv\": \"${spirv}\", ")
    string(APPEND json "\"sourceHash\": \"${sourceHash}\" }")
endforeach()
string(APPEND json "\n  ]\n}\n")

# Only touch the index when its content changed so dependents do not rebuild needlessly.
set(existing "")
if(EXISTS "${CONJURE_SHADER_INDEX}")
    file(READ "${CONJURE_SHADER_INDEX}" existing)
endif()
if(NOT existing STREQUAL json)
    file(WRITE "${CONJURE_SHADER_INDEX}" "${json}")
endif()
