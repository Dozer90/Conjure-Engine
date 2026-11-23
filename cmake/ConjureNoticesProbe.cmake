# Probe of cmake/ConjureNotices.cmake: the CTest entries notices.configure_* and notices.ship_* that
# conjure_add_third_party_notices_tests() registers run this file. No compiler and no network are involved.
#
#   Project mode: a throwaway project (LANGUAGES NONE, version 1.2.3) includes this file with PROBE_CASE naming what
#   to register. It writes its own fixture files (NOTICE, LICENSE, a stand-in dependency that FetchContent takes
#   from a local folder, licence files of every shape the module reads, files of declarations for
#   conjure_check_third_party_pins() and stand-ins of NVRHI's CMakeLists.txt for conjure_check_nvrhi_header_pins() of
#   third_party/nvrhi_checks.cmake), registers notices and either checks the two files
#   conjure_write_third_party_notices() produced and the declarations the pin checks accept ("runtime_tree",
#   "editor_tree", "ship") or stops on the FATAL_ERROR the case is about (every other case; its CTest entry passes on
#   the text of that error).
#
#   Script mode (cmake -DPROBE_SOURCE=... -DPROBE_BINARY=... -DPROBE_GENERATOR=... -P this file): the "ship" case
#   end to end. It configures the probe, builds the target the notices are shipped next to, and checks that the
#   file is there, that it returns after it was deleted and that it follows a configure that changed its content.
#   Under a multi-config generator it builds Debug and looks in the per-configuration folder.

#------------------------------------------------------------------------------
# Script mode: configure and build the "ship" case
#------------------------------------------------------------------------------
if(CMAKE_SCRIPT_MODE_FILE)
    foreach(required IN ITEMS PROBE_SOURCE PROBE_BINARY PROBE_GENERATOR)
        if(NOT DEFINED ${required})
            message(FATAL_ERROR "notices probe: ${required} is not set")
        endif()
    endforeach()
    set(shippedDirectory "${PROBE_BINARY}/bin")
    set(buildArguments --build "${PROBE_BINARY}" --target probe_runtime)
    if(PROBE_GENERATOR MATCHES "Multi-Config|Visual Studio|Xcode")
        string(APPEND shippedDirectory "/Debug")
        list(APPEND buildArguments --config Debug)
    endif()
    set(shippedFile "${shippedDirectory}/THIRD_PARTY_NOTICES.txt")

    # Runs one cmake command of the probe and stops on its failure.
    function(probe_run label)
        execute_process(COMMAND "${CMAKE_COMMAND}" ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output
            ERROR_VARIABLE output)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "notices probe: ${label} failed (${result}):\n${output}")
        endif()
    endfunction()

    set(configureArguments -S "${PROBE_SOURCE}" -B "${PROBE_BINARY}" -G "${PROBE_GENERATOR}"
        "-DCONJURE_NOTICES_PROBE=${CMAKE_SCRIPT_MODE_FILE}" -DPROBE_CASE=ship)
    if(PROBE_MAKE_PROGRAM)
        list(APPEND configureArguments "-DCMAKE_MAKE_PROGRAM=${PROBE_MAKE_PROGRAM}")
    endif()

    # Every run starts from nothing: a build tree of an earlier run already holds the file this run waits for.
    file(REMOVE_RECURSE "${PROBE_BINARY}")
    probe_run("the configure" ${configureArguments} "-DPROBE_ALPHA_TEXT=FIRST ALPHA TEXT")
    if(EXISTS "${shippedFile}")
        message(FATAL_ERROR "notices probe: ${shippedFile} exists before anything was built")
    endif()
    probe_run("the build" ${buildArguments})
    if(NOT EXISTS "${shippedFile}" OR IS_DIRECTORY "${shippedFile}")
        message(FATAL_ERROR "notices probe: building the target did not place ${shippedFile}")
    endif()
    file(READ "${shippedFile}" shipped)
    file(READ "${PROBE_BINARY}/notices/THIRD_PARTY_NOTICES.txt" written)
    if(NOT shipped STREQUAL written OR NOT shipped MATCHES "FIRST ALPHA TEXT")
        message(FATAL_ERROR "notices probe: ${shippedFile} is not the file the configure wrote")
    endif()

    # A deleted file is a missing build output: the next build brings it back.
    file(REMOVE "${shippedFile}")
    probe_run("the build after the file was deleted" ${buildArguments})
    if(NOT EXISTS "${shippedFile}")
        message(FATAL_ERROR "notices probe: ${shippedFile} did not return with the next build")
    endif()

    # A configure that changes the notices changes the shipped file with the next build.
    probe_run("the second configure" ${configureArguments} "-DPROBE_ALPHA_TEXT=SECOND ALPHA TEXT")
    probe_run("the build after the notices changed" ${buildArguments})
    file(READ "${shippedFile}" shipped)
    if(NOT shipped MATCHES "SECOND ALPHA TEXT" OR shipped MATCHES "FIRST ALPHA TEXT")
        message(FATAL_ERROR "notices probe: ${shippedFile} did not follow the configure that changed the notices")
    endif()
    message(STATUS "notices probe: ship passed")
    return()
endif()

#------------------------------------------------------------------------------
# Project mode: fixtures
#------------------------------------------------------------------------------
include("${CMAKE_CURRENT_LIST_DIR}/ConjureNotices.cmake")

if(NOT DEFINED PROBE_ALPHA_TEXT)
    set(PROBE_ALPHA_TEXT "ALPHA LICENCE TEXT")
endif()
get_property(CONJURE_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

set(fixtures "${PROJECT_SOURCE_DIR}")
file(WRITE "${fixtures}/NOTICE" "PROBE NOTICE\nCopyright the probe contributors\n")
file(WRITE "${fixtures}/LICENSE" "\n            PROBE LICENCE TITLE\nprobe licence body\n")
# A byte order mark, blank lines before an indented first line, white space after the last one.
string(ASCII 239 187 191 byteOrderMark)
file(WRITE "${fixtures}/dependency/LICENSE" "${byteOrderMark}\n \n   Indented Title\n${PROBE_ALPHA_TEXT}\n\n \t\n")
file(WRITE "${fixtures}/dependency/docs/EXTRA.txt" "ALPHA EXTRA TERMS\n")
file(WRITE "${fixtures}/licenses/zeta.txt" "ZETA LICENCE TEXT; with [brackets] and a \\ backslash\n")
file(WRITE "${fixtures}/host/LICENSE.txt" "HOST LICENCE OF THIS MACHINE\n")
file(WRITE "${fixtures}/panels/LICENSE.txt" "PANELS LICENCE TEXT\n")
file(WRITE "${fixtures}/tester/LICENSE.txt" "TESTER LICENCE TEXT\n")

# The stand-in dependency: declared like a pinned one (the archive of a commit, its SHA-256 and the release tag as
# the comment beside the name), taken from the fixture folder instead of the network. A second one is declared by its
# commit alone and never made available, like a dependency of a backend this tree does not build. It is spelled as
# CMake reads it too, in lower case with a blank before the '(', with a ')' in the comment on its first line: the pin
# check, the VERSION check (archive_mismatch) and the declaration scan of the notices (declaration_without_notice)
# each find it. An older declaration of each is kept above it as a comment, which no check may read as its pin (the
# VERSION check of the runtime_tree and editor_tree cases would compare the commit 8888...).
set(probeAbsentCommit 9999999999999999999999999999999999999999)
# FetchContent_Declare(probe_dependency  # tag v0.9
FetchContent_Declare(probe_dependency  # tag v1.0
    URL      https://example.invalid/probe/dependency/archive/1111111111111111111111111111111111111111.tar.gz
    URL_HASH SHA256=1111111111111111111111111111111111111111111111111111111111111111
)
# fetchcontent_declare (probe_absent URL https://example.invalid/8888888888888888888888888888888888888888.tar.gz
fetchcontent_declare (probe_absent  # pinned by its commit alone (no release tag)
    URL      https://example.invalid/probe/absent/archive/9999999999999999999999999999999999999999.tar.gz
    URL_HASH SHA256=9999999999999999999999999999999999999999999999999999999999999999
)
set(FETCHCONTENT_SOURCE_DIR_PROBE_DEPENDENCY "${fixtures}/dependency")
FetchContent_MakeAvailable(probe_dependency)

# Files of declarations for conjure_check_third_party_pins(), which reads them and runs none. The commands are spelled
# through variables here, so that neither that check nor the declaration scan of the notices, both of which read
# this file too, takes the text of a fixture for a call of the probe.
set(declare "FetchContent_Declare")
set(declareLowerCase "fetchcontent_declare")
set(populate "FetchContent_Populate")
set(externalProject "ExternalProject_Add")
set(language "cmake_language")
string(REPEAT "a" 64 hashA)
string(REPEAT "B" 64 hashUpper)
string(REPEAT "c" 63 hashShort)
string(REPEAT "d" 32 hashMd5)
set(archive "https://example.invalid/probe/pinned/archive/2222222222222222222222222222222222222222.tar.gz")
file(WRITE "${fixtures}/pins/empty.cmake" "# Declares nothing.\n")
# Every form the check accepts: a declaration commented out whole (by git, which would fail), comments after the
# arguments that name options the check refuses, upper-case hex digits, two declarations in a file, a ';' between
# arguments, a URL list of two mirrors and the hash on the line of the URL, a declaration in lower case with a blank
# before the '(', FetchContent_Populate(<name>) of a declared name (with blanks around it), a function whose name
# only ends in a command the check reads, a quoted URL and hash, and bracket comments (one spanning lines and holding
# a ')' and a closing bracket of another level, one inside a declaration) that name options the check refuses.
file(WRITE "${fixtures}/pins/accepted.cmake"
    "# ${declare}(probe_commented GIT_REPOSITORY https://example.invalid/commented.git GIT_TAG main)\n"
    "${declare}(probe_first  # tag v1.0\n"
    "    URL      ${archive}  # was GIT_TAG v1.0, before code sweep F772\n"
    "    URL_HASH SHA256=${hashA}  # DOWNLOAD_COMMAND is not an argument here\n"
    "    SOURCE_SUBDIR cmake_not_used\n"
    ")\n"
    "${declare}(probe_second;URL ${archive} https://mirror.invalid/second.tar.gz URL_HASH SHA256=${hashUpper})\n"
    "${declareLowerCase} (probe_third URL ${archive} URL_HASH SHA256=${hashA})\n"
    "${populate}(probe_first)\n"
    "${populate} ( probe_second )\n"
    "conjure_${declare}(probe_wrapped GIT_REPOSITORY https://example.invalid/wrapped.git)\n"
    "${declare}(probe_quoted_hash URL \"${archive}\" URL_HASH \"SHA256=${hashA}\")\n"
    "#[=[ ${declare}(probe_bracketed GIT_REPOSITORY https://example.invalid/bracketed.git\n"
    "    GIT_TAG main) ]] is no end ]=]\n"
    "${declare}(probe_fourth #[==[ GIT_TAG v1.0 ]==] URL ${archive} URL_HASH SHA256=${hashA})\n"
)
# One fixture for each refusal.
file(WRITE "${fixtures}/pins/git.cmake"
    "${declare}(probe_git\n"
    "    GIT_REPOSITORY https://example.invalid/probe_git.git\n"
    "    GIT_TAG        v1.0\n"
    ")\n"
)
file(WRITE "${fixtures}/pins/git_beside_hash.cmake"
    "${declare}(probe_mixed URL ${archive} URL_HASH SHA256=${hashA} GIT_SHALLOW TRUE)\n")
file(WRITE "${fixtures}/pins/download_command.cmake"
    "${declare}(probe_command URL ${archive} URL_HASH SHA256=${hashA}\n    DOWNLOAD_COMMAND curl -O ${archive})\n")
file(WRITE "${fixtures}/pins/no_url.cmake" "${declare}(probe_no_url URL_HASH SHA256=${hashA})\n")
file(WRITE "${fixtures}/pins/no_hash.cmake" "${declare}(probe_unhashed\n    URL ${archive}\n)\n")
file(WRITE "${fixtures}/pins/md5_hash.cmake" "${declare}(probe_md5 URL ${archive} URL_HASH MD5=${hashMd5})\n")
file(WRITE "${fixtures}/pins/short_hash.cmake" "${declare}(probe_short URL ${archive} URL_HASH SHA256=${hashShort})\n")
file(WRITE "${fixtures}/pins/lower_case.cmake"
    "${declareLowerCase} (probe_lower GIT_REPOSITORY https://example.invalid/lower.git GIT_TAG v1.0)\n")
file(WRITE "${fixtures}/pins/populate.cmake"
    "${populate}(probe_populate\n    URL ${archive}\n    URL_HASH SHA256=${hashA}\n)\n")
file(WRITE "${fixtures}/pins/external_project.cmake"
    "${externalProject}(probe_external URL ${archive} URL_HASH SHA256=${hashA})\n")
# CMake reads a quoted keyword as the keyword, and the arguments after a bracket comment on its line.
file(WRITE "${fixtures}/pins/quoted_keyword.cmake" "${declare}(probe_quoted URL ${archive} URL_HASH SHA256=${hashA}"
    " \"GIT_REPOSITORY\" https://example.invalid/quoted.git \"GIT_TAG\" main)\n")
file(WRITE "${fixtures}/pins/bracket_comment.cmake"
    "${declare}(probe_bracket #[[pinned]] GIT_REPOSITORY https://example.invalid/bracket.git"
    " URL ${archive} URL_HASH SHA256=${hashA})\n")
# A "#[[" inside a line comment opens no bracket comment: the declaration on the next line is read.
file(WRITE "${fixtures}/pins/bracket_in_line_comment.cmake"
    "# the old pin: #[[\n${declare}(probe_after GIT_REPOSITORY https://example.invalid/after.git GIT_TAG v1.0)\n"
    "# ]]\n")
file(WRITE "${fixtures}/pins/cmake_language.cmake"
    "${language}(CALL ${declare} probe_language GIT_REPOSITORY https://example.invalid/language.git)\n")

# Stand-ins for NVRHI's CMakeLists.txt for conjure_check_nvrhi_header_pins() (third_party/nvrhi_checks.cmake): the
# tags as NVRHI sets them (the second spelled in upper case with blanks, as CMake accepts it) with the declarations
# NVRHI makes (one in lower case, and an unknown one commented out), a Vulkan tag other than the pinned one, a file
# that no longer sets the DirectX tag, and one that declares a dependency the check does not know.
include("${CMAKE_CURRENT_LIST_DIR}/../third_party/nvrhi_checks.cmake")
set(nvrhiDirectxTag "v1.717.0-preview")
set(nvrhiVulkanTag "v1.4.352")
set(nvrhiVulkanLine "    set(NVRHI_VULKAN_HEADERS_GIT_TAG \"${nvrhiVulkanTag}\" CACHE STRING \"Vulkan-Headers tag\")\n")
set(nvrhiDirectxLine
    "    SET( NVRHI_DIRECTX_HEADERS_GIT_TAG \"${nvrhiDirectxTag}\" CACHE STRING \"DirectX-Headers git tag\")\n")
set(nvrhiDeclarations
    "        ${declare}(\n            vulkan_headers\n            GIT_REPOSITORY https://example.invalid/vk.git)\n"
    "        ${declare}(directx_headers GIT_REPOSITORY https://example.invalid/dx.git)\n"
    "    # ${declare}(nvapi GIT_REPOSITORY https://example.invalid/nvapi.git)\n"
    "        ${declareLowerCase} (rtxmu GIT_REPOSITORY https://example.invalid/rtxmu.git)\n")
file(WRITE "${fixtures}/nvrhi/matching/CMakeLists.txt" "${nvrhiVulkanLine}" "${nvrhiDirectxLine}"
    ${nvrhiDeclarations})
file(WRITE "${fixtures}/nvrhi/unknown_dependency/CMakeLists.txt" "${nvrhiVulkanLine}" "${nvrhiDirectxLine}"
    ${nvrhiDeclarations} "    ${declare}(nvapi GIT_REPOSITORY https://example.invalid/nvapi.git)\n")
file(WRITE "${fixtures}/nvrhi/differing/CMakeLists.txt"
    "    set(NVRHI_VULKAN_HEADERS_GIT_TAG \"v1.4.353\" CACHE STRING \"Vulkan-Headers git tag\")\n"
    "    set(NVRHI_DIRECTX_HEADERS_GIT_TAG \"${nvrhiDirectxTag}\" CACHE STRING \"DirectX-Headers git tag\")\n")
file(WRITE "${fixtures}/nvrhi/unset/CMakeLists.txt" "${nvrhiVulkanLine}"
    "    set(NVRHI_DIRECTX_HEADERS_GIT_REPOSITORY \"https://example.invalid/directx.git\" CACHE STRING \"\")\n")

#------------------------------------------------------------------------------
# Project mode: the cases that stop the configure
#------------------------------------------------------------------------------
set(refusedPinFixtures
    git git_beside_hash download_command no_url no_hash md5_hash short_hash lower_case populate external_project
    quoted_keyword bracket_comment bracket_in_line_comment cmake_language)
string(REGEX REPLACE "^pins_" "" refusedPins "${PROBE_CASE}")
if(PROBE_CASE STREQUAL "pin_mismatch")
    conjure_third_party_notice(alpha NAME "Alpha" VERSION v2.0 LICENSE_ID "MIT" SCOPE RUNTIME
        FETCH probe_dependency LICENSE_FILES LICENSE)
elseif(PROBE_CASE STREQUAL "archive_mismatch")
    # Without a tag comment the pin is the commit the archive is named after, not a release name.
    conjure_third_party_notice(absent NAME "Absent" VERSION v9 LICENSE_ID "MIT" SCOPE RUNTIME
        FETCH probe_absent LICENSE_FILES LICENSE)
elseif(PROBE_CASE MATCHES "^pins_" AND refusedPins IN_LIST refusedPinFixtures)
    conjure_check_third_party_pins("${fixtures}/pins/empty.cmake" "${fixtures}/pins/${refusedPins}.cmake")
elseif(PROBE_CASE MATCHES "^pins_nvrhi_(differing|unset|unknown_dependency)$")
    conjure_check_nvrhi_header_pins("${fixtures}/nvrhi/${CMAKE_MATCH_1}/CMakeLists.txt" "${nvrhiDirectxTag}"
        "${nvrhiVulkanTag}")
elseif(PROBE_CASE STREQUAL "pins_nvrhi_rtxmu_enabled")
    set(NVRHI_WITH_RTXMU ON)
    conjure_check_nvrhi_header_pins("${fixtures}/nvrhi/matching/CMakeLists.txt" "${nvrhiDirectxTag}"
        "${nvrhiVulkanTag}")
elseif(PROBE_CASE STREQUAL "pins_nvrhi_missing_file")
    conjure_check_nvrhi_header_pins("${fixtures}/nvrhi/not_there/CMakeLists.txt" "${nvrhiDirectxTag}"
        "${nvrhiVulkanTag}")
elseif(PROBE_CASE STREQUAL "pins_missing_file")
    conjure_check_third_party_pins("${fixtures}/pins/not_there.cmake")
elseif(PROBE_CASE STREQUAL "pins_no_files")
    conjure_check_third_party_pins()
elseif(PROBE_CASE STREQUAL "missing_licence_file")
    conjure_third_party_notice(alpha NAME "Alpha" VERSION v1.0 LICENSE_ID "MIT" SCOPE RUNTIME
        FETCH probe_dependency LICENSE_FILES COPYING)
elseif(PROBE_CASE STREQUAL "registered_twice")
    conjure_third_party_notice(alpha NAME "Alpha" LICENSE_ID "MIT" SCOPE RUNTIME NOTE "One.")
    conjure_third_party_notice(alpha NAME "Alpha again" LICENSE_ID "MIT" SCOPE RUNTIME NOTE "Two.")
elseif(PROBE_CASE STREQUAL "unknown_scope")
    conjure_third_party_notice(alpha NAME "Alpha" LICENSE_ID "MIT" SCOPE SERVER NOTE "One.")
elseif(PROBE_CASE STREQUAL "no_terms")
    conjure_third_party_notice(alpha NAME "Alpha" LICENSE_ID "MIT" SCOPE RUNTIME URL "https://example.org/alpha")
elseif(PROBE_CASE STREQUAL "host_files_without_flag")
    conjure_third_party_notice(alpha NAME "Alpha" LICENSE_ID "MIT" SCOPE RUNTIME NOTE "One."
        HOST_FILES "${fixtures}/host/LICENSE.txt")
elseif(PROBE_CASE STREQUAL "host_provided_with_pin")
    conjure_third_party_notice(alpha NAME "Alpha" VERSION v1.0 LICENSE_ID "MIT" SCOPE RUNTIME NOTE "One."
        HOST_PROVIDED)
elseif(PROBE_CASE STREQUAL "line_break_in_text")
    conjure_third_party_notice(alpha NAME "Alpha" LICENSE_ID "MIT" SCOPE RUNTIME NOTE "One.\nTwo.")
elseif(PROBE_CASE STREQUAL "written_twice")
    # Both declared dependencies are named, so the first call passes.
    conjure_third_party_notice(alpha NAME "Alpha" VERSION v1.0 LICENSE_ID "MIT" SCOPE RUNTIME
        FETCH probe_dependency LICENSE_FILES LICENSE)
    conjure_third_party_notice(absent NAME "Absent" VERSION ${probeAbsentCommit} LICENSE_ID "MIT" SCOPE RUNTIME
        FETCH probe_absent LICENSE_FILES LICENSE)
    conjure_write_third_party_notices()
    conjure_write_third_party_notices()
elseif(PROBE_CASE STREQUAL "declaration_without_notice")
    # probe_absent is declared above and no notice names it: its licence would never reach the notices.
    conjure_third_party_notice(alpha NAME "Alpha" VERSION v1.0 LICENSE_ID "MIT" SCOPE RUNTIME
        FETCH probe_dependency LICENSE_FILES LICENSE)
elseif(PROBE_CASE STREQUAL "ship_before_write")
    add_custom_target(probe_runtime)
    conjure_ship_third_party_notices(probe_runtime)
endif()
if(NOT PROBE_CASE MATCHES "^(runtime_tree|editor_tree|ship)$")
    conjure_write_third_party_notices()
    message(FATAL_ERROR "notices probe: the case '${PROBE_CASE}' configured without the error it is about")
endif()

#------------------------------------------------------------------------------
# Project mode: a complete registration, and what the configure wrote from it
#------------------------------------------------------------------------------
# The probe's own declarations pin as the engine's must, and every form of the accepted fixture passes, beside a
# file that declares nothing. NVRHI's stand-in that names the pinned header tags passes too.
conjure_check_third_party_pins("${CMAKE_CURRENT_LIST_FILE}" "${fixtures}/pins/accepted.cmake"
    "${fixtures}/pins/empty.cmake")
conjure_check_nvrhi_header_pins("${fixtures}/nvrhi/matching/CMakeLists.txt" "${nvrhiDirectxTag}" "${nvrhiVulkanTag}")

set(CONJURE_BUILD_EDITOR OFF)
if(PROBE_CASE STREQUAL "editor_tree")
    set(CONJURE_BUILD_EDITOR ON)
endif()

# Registered out of name order, with upper and lower case: the notices sort by name without regard to case.
conjure_third_party_notice(zeta
    NAME "zeta" LICENSE_ID "Zlib" SCOPE RUNTIME
    NOTE "A file kept in the tree."
    LICENSE_FILES licenses/zeta.txt
)
conjure_third_party_notice(alpha
    NAME "Alpha" VERSION v1.0 LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://example.org/alpha"
    NOTE "A note of more than one hundred columns, which the runtime notices break at a space so that no line of \
it runs past the width of the rules around the entry."
    FETCH probe_dependency LICENSE_FILES LICENSE docs/EXTRA.txt
)
conjure_third_party_notice(absent
    NAME "Absent" VERSION ${probeAbsentCommit} LICENSE_ID "MIT" SCOPE RUNTIME
    FETCH probe_absent LICENSE_FILES LICENSE
)
conjure_third_party_notice(unlinked
    NAME "Unlinked" LICENSE_ID "MIT" SCOPE RUNTIME
    USED FALSE LICENSE_FILES "${fixtures}/nowhere/LICENSE"
)
conjure_third_party_notice(panels
    NAME "Panels" VERSION 3 LICENSE_ID "MIT" SCOPE EDITOR
    LICENSE_FILES "${fixtures}/panels/LICENSE.txt"
)
conjure_third_party_notice(tester
    NAME "Tester" LICENSE_ID "MIT" SCOPE BUILD
    LICENSE_FILES "${fixtures}/tester/LICENSE.txt"
)
conjure_third_party_notice(host
    NAME "host runtime" LICENSE_ID "MIT, binaries under other terms" SCOPE RUNTIME
    URL "https://example.org/host"
    NOTE "Found on the build machine."
    HOST_PROVIDED HOST_VERSION 8.0.1 HOST_FILES "${fixtures}/host/LICENSE.txt" "${fixtures}/host/NOT_THERE.txt"
)
conjure_third_party_notice(vendor
    NAME "Vendor Runtime" LICENSE_ID "Vendor terms" SCOPE RUNTIME
    URL "https://example.org/vendor"
    NOTE "Files of the vendor, under the vendor's terms."
)
conjure_write_third_party_notices()

get_property(noticesFile GLOBAL PROPERTY CONJURE_NOTICES_FILE)
get_property(manifestFile GLOBAL PROPERTY CONJURE_NOTICES_MANIFEST)
if(NOT noticesFile STREQUAL "${PROJECT_BINARY_DIR}/notices/THIRD_PARTY_NOTICES.txt")
    message(FATAL_ERROR "notices probe: the runtime notices are at '${noticesFile}'")
endif()
if(NOT manifestFile STREQUAL "${PROJECT_BINARY_DIR}/notices/third_party_notices.json")
    message(FATAL_ERROR "notices probe: the manifest is at '${manifestFile}'")
endif()
file(READ "${noticesFile}" notices)
file(READ "${manifestFile}" manifest)

# Fails unless every argument is found in the notices, each after the one before it. The arguments are read one by
# one (ARGV<n>), not as a list: a needle may hold ';' and brackets.
function(probe_expect_in_order)
    set(position 0)
    math(EXPR last "${ARGC} - 1")
    foreach(index RANGE ${last})
        set(needle "${ARGV${index}}")
        string(SUBSTRING "${notices}" ${position} -1 rest)
        string(FIND "${rest}" "${needle}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "notices probe: '${needle}' is missing, or out of order, in:\n${notices}")
        endif()
        string(LENGTH "${needle}" length)
        math(EXPR position "${position} + ${found} + ${length}")
    endforeach()
endfunction()

# Fails when one of the arguments is found in the notices.
function(probe_expect_absent)
    math(EXPR last "${ARGC} - 1")
    foreach(index RANGE ${last})
        string(FIND "${notices}" "${ARGV${index}}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR "notices probe: '${ARGV${index}}' must not be in:\n${notices}")
        endif()
    endforeach()
endfunction()

# Sets <outVariable> to the index of the manifest entry <id>; fails when there is none.
function(probe_manifest_index outVariable id)
    string(JSON count LENGTH "${manifest}" notices)
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
        string(JSON entryId GET "${manifest}" notices ${index} id)
        if(entryId STREQUAL id)
            set(${outVariable} ${index} PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "notices probe: the manifest has no entry '${id}'")
endfunction()

# Fails unless the member of the manifest entry <id> (further keys in ARGN) is <expected>; a JSON boolean reads ON
# or OFF.
function(probe_expect_manifest id member expected)
    probe_manifest_index(index "${id}")
    string(JSON value GET "${manifest}" notices ${index} ${member} ${ARGN})
    if(NOT value STREQUAL expected)
        message(FATAL_ERROR "notices probe: manifest ${id}.${member} ${ARGN} is '${value}', not '${expected}'")
    endif()
endfunction()

# Fails unless the list member of the manifest entry <id> has <expected> elements.
function(probe_expect_manifest_count id member expected)
    probe_manifest_index(index "${id}")
    string(JSON count LENGTH "${manifest}" notices ${index} ${member})
    if(NOT count EQUAL expected)
        message(FATAL_ERROR "notices probe: manifest ${id}.${member} has ${count} element(s), not ${expected}")
    endif()
endfunction()

# Conjure's own notice and licence open the file; the licence keeps the indentation of its first line.
probe_expect_in_order(
    "Third-party notices"
    "   1. Conjure Engine 1.2.3 (Apache-2.0)"
    "1. Conjure Engine 1.2.3\n   Licence: Apache-2.0\n"
    "PROBE NOTICE\nCopyright the probe contributors\n\n            PROBE LICENCE TITLE\nprobe licence body\n"
)
# Alpha: the pin in the title, the source, the note broken at 100 columns, both files under their labels, the
# byte order mark and the blank lines around the text gone, the indentation of the first line kept.
probe_expect_in_order(
    "2. Alpha v1.0\n   Licence: MIT\n   Source:  https://example.org/alpha\n"
    "\nA note of more than one hundred columns, which the runtime notices break at a space so that no line\n"
    "of it runs past the width of the rules around the entry.\n"
    "\n[LICENSE]\n\n   Indented Title\n${PROBE_ALPHA_TEXT}\n\n[docs/EXTRA.txt]\n\nALPHA EXTRA TERMS\n"
)
probe_expect_absent("${byteOrderMark}" "Absent" "Unlinked" "Tester" "TESTER LICENCE TEXT" "NOT_THERE")
if(PROBE_CASE STREQUAL "editor_tree")
    # A tree that builds the editor links its libraries into the shared library and names them, in name order.
    probe_expect_in_order(
        "   2. Alpha v1.0 (MIT)\n   3. host runtime 8.0.1 (MIT, binaries under other terms)\n   4. Panels 3 (MIT)\n"
        "   5. Vendor Runtime (Vendor terms)\n   6. zeta (Zlib)\n"
        "4. Panels 3\n   Licence: MIT\n"
        "PANELS LICENCE TEXT\n"
    )
    probe_expect_manifest(panels shipped ON)
else()
    probe_expect_in_order(
        "   2. Alpha v1.0 (MIT)\n   3. host runtime 8.0.1 (MIT, binaries under other terms)\n"
        "   4. Vendor Runtime (Vendor terms)\n   5. zeta (Zlib)\n"
    )
    probe_expect_absent("Panels" "PANELS LICENCE TEXT")
    probe_expect_manifest(panels shipped OFF)
endif()
# Software of the build machine: the version found there, the files that exist, one file so no label.
probe_expect_in_order(
    "3. host runtime 8.0.1\n   Licence: MIT, binaries under other terms\n   Source:  https://example.org/host\n"
    "Found on the build machine.\n"
    "The text below comes from the installation on the machine that built these files.\n"
    "\nHOST LICENCE OF THIS MACHINE\n"
)
# No licence file at all: the entry says where the terms are.
probe_expect_in_order(
    "Vendor Runtime\n   Licence: Vendor terms\n"
    "Files of the vendor, under the vendor's terms.\n"
    "The terms are published at https://example.org/vendor and are not reproduced here.\n"
)
# A file kept in the tree, named relative to the registering directory; list and bracket characters survive.
probe_expect_in_order(
    "zeta\n   Licence: Zlib\n"
    "A file kept in the tree.\n"
    "\nZETA LICENCE TEXT; with [brackets] and a \\ backslash\n"
)

# The manifest is JSON, lists every registered notice in name order and says what this tree does with each.
string(JSON manifestVersion GET "${manifest}" version)
string(JSON noticeCount LENGTH "${manifest}" notices)
string(JSON firstId GET "${manifest}" notices 0 id)
string(JSON lastId GET "${manifest}" notices 7 id)
if(NOT manifestVersion EQUAL 1 OR NOT noticeCount EQUAL 8)
    message(FATAL_ERROR "notices probe: unexpected manifest:\n${manifest}")
endif()
if(NOT firstId STREQUAL "absent" OR NOT lastId STREQUAL "zeta")
    message(FATAL_ERROR "notices probe: the manifest is not in name order:\n${manifest}")
endif()
probe_expect_manifest(alpha name "Alpha")
probe_expect_manifest(alpha version "v1.0")
probe_expect_manifest(alpha license "MIT")
probe_expect_manifest(alpha scope "runtime")
probe_expect_manifest(alpha url "https://example.org/alpha")
probe_expect_manifest(alpha used ON)
probe_expect_manifest(alpha shipped ON)
probe_expect_manifest(alpha hostProvided OFF)
probe_expect_manifest(alpha files "docs/EXTRA.txt" 1 label)
probe_expect_manifest(alpha files "${fixtures}/dependency/docs/EXTRA.txt" 1 path)
probe_expect_manifest(absent used OFF)
probe_expect_manifest(absent shipped OFF)
probe_expect_manifest(absent version "${probeAbsentCommit}")
probe_expect_manifest(unlinked used OFF)
probe_expect_manifest(tester used ON)
probe_expect_manifest(tester shipped OFF)
probe_expect_manifest(tester scope "build")
probe_expect_manifest(panels used ON)
probe_expect_manifest(panels scope "editor")
probe_expect_manifest(host hostProvided ON)
probe_expect_manifest(host version "")
probe_expect_manifest(host hostFiles "LICENSE.txt" 0 label)
probe_expect_manifest(vendor used ON)
probe_expect_manifest(zeta files "${fixtures}/licenses/zeta.txt" 0 path)
# An absolute licence file is labelled relative to the registering directory, never by a path of this machine.
probe_expect_manifest(panels files "panels/LICENSE.txt" 0 label)
probe_expect_manifest(panels files "${fixtures}/panels/LICENSE.txt" 0 path)
probe_expect_manifest(tester files "tester/LICENSE.txt" 0 label)
# A notice the tree does not use lists no licence file, and only files that exist are listed.
probe_expect_manifest_count(absent files 0)
probe_expect_manifest_count(unlinked files 0)
probe_expect_manifest_count(vendor files 0)
probe_expect_manifest_count(alpha files 2)
probe_expect_manifest_count(alpha hostFiles 0)
probe_expect_manifest_count(host hostFiles 1)

if(PROBE_CASE STREQUAL "ship")
    add_custom_target(probe_runtime)
    set_target_properties(probe_runtime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    conjure_ship_third_party_notices(probe_runtime)
endif()
message(STATUS "notices probe: ${PROBE_CASE} passed")
