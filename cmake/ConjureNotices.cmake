# Third-party licence notices (docs/design/06_engine_loop.md D15, code sweep F200).
#
# Every piece of third-party software the build uses is registered once, next to its pin in
# third_party/CMakeLists.txt or a file it includes (jxl.cmake), with conjure_third_party_notice(). At the end of
# third_party/CMakeLists.txt conjure_write_third_party_notices() turns the registrations into two files of the build
# tree:
#
#   <build>/notices/third_party_notices.json   the manifest: every registered notice, whether this tree uses it and
#                                              the licence files it found. tools/third_party_notices.py builds
#                                              and checks the tracked THIRD_PARTY_NOTICES.md from it.
#   <build>/notices/THIRD_PARTY_NOTICES.txt    the runtime notices: Conjure's own NOTICE and LICENSE, then the
#                                              licence text of everything this tree links into conjure_shared and
#                                              conjure_runtime or places next to them.
#
# conjure_ship_third_party_notices(<target>) copies the runtime notices next to an executable as a declared build
# output (runtime/CMakeLists.txt does for conjure_runtime), and export step 5 copies the file from there into a
# game whenever it copies the runtime (engine/src/engine/export_pipeline.cpp).
#
#   conjure_third_party_notice(<id>
#       NAME       <display name>
#       LICENSE_ID <SPDX expression, or a short name for terms SPDX does not list>
#       SCOPE      <RUNTIME | EDITOR | BUILD>
#       [VERSION <pin>]                 the GIT_TAG of the dependency; checked against the FetchContent_Declare of
#                                       <FETCH> when that call is in the file that registers the notice
#       [URL     <where the software comes from>]
#       [NOTE    <one or more sentences printed with the entry>]
#       [FETCH   <FetchContent name>]   LICENSE_FILES are relative to its source directory, and the notice is used
#                                       by this tree only when the content was populated
#       [USED    <bool>]                default TRUE; FALSE marks software this configuration does not link
#       [LICENSE_FILES <file>...]       the licence text as its authors publish it; a used notice whose file is
#                                       missing fails the configure. Without FETCH: relative to the registering
#                                       directory, or absolute (with FETCH too: a text kept in the tree). An
#                                       absolute file is labelled relative to the registering directory, or by its
#                                       file name outside it.
#       [HOST_PROVIDED]                software found on the build machine instead of pinned (the .NET
#                                       installation): no FETCH, VERSION or LICENSE_FILES, and its licence text
#                                       is never part of the tracked THIRD_PARTY_NOTICES.md
#       [HOST_VERSION <version>]        with HOST_PROVIDED: the version found on this machine
#       [HOST_FILES <file>...]          with HOST_PROVIDED: absolute paths of the licence files of that
#                                       installation; those that exist are written into the runtime notices
#   )
#
# Scopes: RUNTIME is linked into conjure_shared or conjure_runtime, or shipped as a file next to them. EDITOR is
# linked into conjure_shared and conjure_editor in a tree that builds the editor, so it is part of the runtime
# notices of such a tree and of no Distribution build. BUILD is a build or test tool and reaches no binary that
# ships.
#
# The runtime notices are written with the platform's line endings (file(WRITE) is a text-mode write), so the file
# opens in any editor of the platform the game was built for. Licence files are read as bytes: UTF-8 stays UTF-8.

include_guard(GLOBAL)

include(FetchContent)

# Registers one notice (see the file comment). Nothing is read here: whether the notice is used and where its
# licence files are is decided in conjure_write_third_party_notices(), after every dependency was made available.
function(conjure_third_party_notice id)
    # An empty VERSION or HOST_VERSION is meant (a pin this tree does not know): CMake 3.31 and later would warn
    # about the keyword without a value unless the policy says that it defines an empty variable.
    if(POLICY CMP0174)
        cmake_policy(SET CMP0174 NEW)
    endif()
    set(oneValueArgs NAME LICENSE_ID SCOPE VERSION URL NOTE FETCH USED HOST_VERSION)
    set(multiValueArgs LICENSE_FILES HOST_FILES)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "HOST_PROVIDED" "${oneValueArgs}" "${multiValueArgs}")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_third_party_notice(${id}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT id MATCHES "^[a-z][a-z0-9_]*$")
        message(FATAL_ERROR "conjure_third_party_notice(${id}): the id is lower-case letters, digits and '_'")
    endif()
    foreach(required IN ITEMS NAME LICENSE_ID SCOPE)
        if(NOT ARG_${required})
            message(FATAL_ERROR "conjure_third_party_notice(${id}): ${required} is required")
        endif()
    endforeach()
    if(NOT ARG_SCOPE MATCHES "^(RUNTIME|EDITOR|BUILD)$")
        message(FATAL_ERROR "conjure_third_party_notice(${id}): SCOPE is RUNTIME, EDITOR or BUILD, not '${ARG_SCOPE}'")
    endif()
    # The texts become one line of the runtime notices and a JSON string of the manifest each.
    foreach(text IN ITEMS NAME LICENSE_ID VERSION URL NOTE HOST_VERSION)
        if("${ARG_${text}}" MATCHES "[\r\n;|]")
            message(FATAL_ERROR "conjure_third_party_notice(${id}): ${text} must not hold a line break, ';' or '|'")
        endif()
    endforeach()
    if(ARG_HOST_PROVIDED AND (ARG_FETCH OR ARG_VERSION OR ARG_LICENSE_FILES))
        message(FATAL_ERROR
            "conjure_third_party_notice(${id}): HOST_PROVIDED software is not pinned, so it has no FETCH, VERSION "
            "or LICENSE_FILES")
    endif()
    if(NOT ARG_HOST_PROVIDED AND (ARG_HOST_FILES OR ARG_HOST_VERSION))
        message(FATAL_ERROR "conjure_third_party_notice(${id}): HOST_FILES and HOST_VERSION need HOST_PROVIDED")
    endif()
    if(NOT ARG_LICENSE_FILES AND NOT ARG_NOTE)
        message(FATAL_ERROR
            "conjure_third_party_notice(${id}): a notice without LICENSE_FILES needs a NOTE that says which terms "
            "apply")
    endif()
    # USED followed by nothing is what an unset or empty variable expands to: not used.
    if("USED" IN_LIST ARG_KEYWORDS_MISSING_VALUES)
        set(ARG_USED FALSE)
    elseif(NOT DEFINED ARG_USED)
        set(ARG_USED TRUE)
    endif()

    get_property(ids GLOBAL PROPERTY CONJURE_NOTICE_IDS)
    if("${id}" IN_LIST ids)
        message(FATAL_ERROR "conjure_third_party_notice(${id}): registered twice")
    endif()
    set_property(GLOBAL APPEND PROPERTY CONJURE_NOTICE_IDS "${id}")
    set(fields NAME LICENSE_ID SCOPE VERSION URL NOTE FETCH USED HOST_PROVIDED HOST_VERSION LICENSE_FILES HOST_FILES)
    foreach(field IN LISTS fields)
        set_property(GLOBAL PROPERTY "CONJURE_NOTICE_${id}_${field}" "${ARG_${field}}")
    endforeach()
    set_property(GLOBAL PROPERTY "CONJURE_NOTICE_${id}_LIST_FILE" "${CMAKE_CURRENT_LIST_FILE}")
    set_property(GLOBAL PROPERTY "CONJURE_NOTICE_${id}_DIRECTORY" "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

# Sets <outVariable> to the text of a licence file as the notices print it: no byte order mark, LF line ends, no
# blank lines before the first line (its indentation is kept: the Apache licence centres its title) and no white
# space after the last one.
function(_conjure_notice_read_text outVariable path)
    file(READ "${path}" text)
    string(ASCII 239 187 191 byteOrderMark)
    string(FIND "${text}" "${byteOrderMark}" markPosition)
    if(markPosition EQUAL 0)
        string(SUBSTRING "${text}" 3 -1 text)
    endif()
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REPLACE "\r" "\n" text "${text}")
    string(REGEX REPLACE "^([ \t]*\n)+" "" text "${text}")
    string(REGEX REPLACE "[ \t\n]+$" "" text "${text}")
    set(${outVariable} "${text}" PARENT_SCOPE)
endfunction()

# Sets <outVariable> to <text> broken into lines of at most <width> columns at its spaces (a longer word stays
# whole). The text is a registered NOTE: one line without ';', so its words can be walked as a list.
function(_conjure_notice_wrap outVariable text width)
    string(REPLACE " " ";" words "${text}")
    set(wrapped "")
    set(line "")
    foreach(word IN LISTS words)
        if(line STREQUAL "")
            set(line "${word}")
            continue()
        endif()
        string(LENGTH "${line} ${word}" length)
        if(length GREATER width)
            string(APPEND wrapped "${line}\n")
            set(line "${word}")
        else()
            string(APPEND line " ${word}")
        endif()
    endforeach()
    set(${outVariable} "${wrapped}${line}" PARENT_SCOPE)
endfunction()

# Sets <outVariable> to <text> as the inside of a JSON string. The registered texts hold no line break (checked on
# registration) and paths are CMake paths, so the two characters JSON reserves are all there is to escape.
function(_conjure_notice_json_string outVariable text)
    string(REPLACE "\\" "\\\\" text "${text}")
    string(REPLACE "\"" "\\\"" text "${text}")
    set(${outVariable} "${text}" PARENT_SCOPE)
endfunction()

# Fails the configure when the notice names a VERSION other than the GIT_TAG its dependency is declared with. Only
# a FetchContent_Declare in the file that registered the notice can be compared; a dependency another dependency
# declares (NVRHI fetches the Vulkan and DirectX headers) passes its pin as VERSION directly, and one pinned by URL
# and URL_HASH (third_party/jxl.cmake) has no GIT_TAG to compare, so its notices read the variables its URLs read.
function(_conjure_notice_check_pin id fetch version listFile)
    file(READ "${listFile}" listText)
    string(REGEX MATCH
        "FetchContent_Declare\\(${fetch}[ \t\r\n]+[^)]*GIT_TAG[ \t]+([^ \t\r\n)]+)" declaration "${listText}")
    if(NOT declaration)
        return()
    endif()
    if(NOT CMAKE_MATCH_1 STREQUAL version)
        message(FATAL_ERROR
            "conjure_third_party_notice(${id}): VERSION '${version}' is not the GIT_TAG '${CMAKE_MATCH_1}' that "
            "${listFile} declares for ${fetch}. Change the VERSION with the pin, then regenerate "
            "THIRD_PARTY_NOTICES.md (python tools/third_party_notices.py --build-dir <build tree>).")
    endif()
endfunction()

# Resolves one registered notice against this configuration: whether the tree uses it, and the licence files that
# exist. <outFiles> and <outHostFiles> are lists of "<label>|<absolute path>", the label being the path as it
# was registered (an absolute one relative to the registering directory, or its file name outside it).
function(_conjure_notice_resolve id outUsed outFiles outHostFiles)
    foreach(field IN ITEMS NAME VERSION FETCH USED LICENSE_FILES HOST_FILES LIST_FILE DIRECTORY)
        get_property(${field} GLOBAL PROPERTY "CONJURE_NOTICE_${id}_${field}")
    endforeach()

    set(used FALSE)
    if(USED)
        set(used TRUE)
    endif()
    set(baseDirectory "${DIRECTORY}")
    if(FETCH)
        _conjure_notice_check_pin("${id}" "${FETCH}" "${VERSION}" "${LIST_FILE}")
        FetchContent_GetProperties(${FETCH} SOURCE_DIR fetchedDirectory POPULATED fetchPopulated)
        if(NOT fetchPopulated)
            set(used FALSE)
        endif()
        set(baseDirectory "${fetchedDirectory}")
    endif()

    set(files "")
    if(used)
        foreach(file IN LISTS LICENSE_FILES)
            cmake_path(ABSOLUTE_PATH file BASE_DIRECTORY "${baseDirectory}" NORMALIZE OUTPUT_VARIABLE path)
            if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
                message(FATAL_ERROR
                    "conjure_third_party_notice(${id}): the licence file '${path}' of ${NAME} does not exist. A "
                    "new pin may have renamed it: name the file that carries the licence text now.")
            endif()
            # A file kept in the tree (absolute, since FETCH reads relative paths in the fetched sources) is named
            # from the registering directory, never by a path of this machine: the label is part of the tracked
            # THIRD_PARTY_NOTICES.md.
            set(label "${file}")
            if(IS_ABSOLUTE "${file}")
                cmake_path(IS_PREFIX DIRECTORY "${path}" NORMALIZE insideDirectory)
                if(insideDirectory)
                    cmake_path(RELATIVE_PATH path BASE_DIRECTORY "${DIRECTORY}" OUTPUT_VARIABLE label)
                else()
                    cmake_path(GET path FILENAME label)
                endif()
            endif()
            list(APPEND files "${label}|${path}")
        endforeach()
    endif()

    set(hostFiles "")
    if(used)
        foreach(file IN LISTS HOST_FILES)
            if(EXISTS "${file}" AND NOT IS_DIRECTORY "${file}")
                cmake_path(GET file FILENAME label)
                list(APPEND hostFiles "${label}|${file}")
            endif()
        endforeach()
        if(HOST_FILES AND NOT hostFiles)
            message(WARNING
                "third-party notices: none of the licence files of ${NAME} was found on this machine (looked for "
                "${HOST_FILES}). The runtime notices name the software and where its terms are published, without "
                "the text.")
        endif()
    endif()

    set(${outUsed} "${used}" PARENT_SCOPE)
    set(${outFiles} "${files}" PARENT_SCOPE)
    set(${outHostFiles} "${hostFiles}" PARENT_SCOPE)
endfunction()

# Writes <content> to <path> through a scratch copy, so that a configure that changes nothing leaves the file's
# time stamp, and with it every build step that depends on the file, alone.
function(_conjure_notice_write_if_changed path content)
    file(WRITE "${path}.in" "${content}")
    configure_file("${path}.in" "${path}" COPYONLY)
endfunction()

# Writes the manifest and the runtime notices of this configuration (see the file comment) and publishes their
# paths as the global properties CONJURE_NOTICES_MANIFEST and CONJURE_NOTICES_FILE. Call it once, after every
# dependency was made available, from the file that declares them: a FetchContent_Declare there (not in a file it
# includes) that no notice names as FETCH fails the configure.
function(conjure_write_third_party_notices)
    get_property(written GLOBAL PROPERTY CONJURE_NOTICES_FILE SET)
    if(written)
        message(FATAL_ERROR "conjure_write_third_party_notices: called twice")
    endif()
    set(ownNotice "${PROJECT_SOURCE_DIR}/NOTICE")
    set(ownLicence "${PROJECT_SOURCE_DIR}/LICENSE")
    foreach(file IN ITEMS "${ownNotice}" "${ownLicence}")
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "third-party notices: '${file}' is missing; the runtime notices open with it")
        endif()
    endforeach()

    # Entries in the order of their names, without regard to case; the manifest keeps the order for the tool.
    get_property(ids GLOBAL PROPERTY CONJURE_NOTICE_IDS)
    set(keys "")
    foreach(id IN LISTS ids)
        get_property(name GLOBAL PROPERTY "CONJURE_NOTICE_${id}_NAME")
        string(TOLOWER "${name}" key)
        list(APPEND keys "${key}|${id}")
    endforeach()
    list(SORT keys)

    # A rule of '=': licence texts draw their own rules with '-'.
    set(rule "================================================================================")
    set(contents "   1. Conjure Engine ${PROJECT_VERSION} (Apache-2.0)\n")
    _conjure_notice_read_text(ownNoticeText "${ownNotice}")
    _conjure_notice_read_text(ownLicenceText "${ownLicence}")
    set(body "${rule}\n1. Conjure Engine ${PROJECT_VERSION}\n   Licence: Apache-2.0\n${rule}\n\n")
    string(APPEND body "${ownNoticeText}\n\n${ownLicenceText}\n")
    set(manifestEntries "")
    set(shippedCount 1)

    foreach(key IN LISTS keys)
        string(REGEX REPLACE "^.*\\|" "" id "${key}")
        foreach(field IN ITEMS NAME LICENSE_ID SCOPE VERSION URL NOTE HOST_PROVIDED HOST_VERSION)
            get_property(${field} GLOBAL PROPERTY "CONJURE_NOTICE_${id}_${field}")
        endforeach()
        _conjure_notice_resolve("${id}" used files hostFiles)
        set(hostProvided FALSE)
        if(HOST_PROVIDED)
            set(hostProvided TRUE)
        endif()

        set(shipped FALSE)
        if(used AND (SCOPE STREQUAL "RUNTIME" OR (SCOPE STREQUAL "EDITOR" AND CONJURE_BUILD_EDITOR)))
            set(shipped TRUE)
        endif()

        # The manifest entry.
        string(TOLOWER "${SCOPE}" scopeName)
        set(jsonFlags "")
        foreach(flag IN ITEMS used shipped hostProvided)
            if(${flag})
                list(APPEND jsonFlags "\"${flag}\": true")
            else()
                list(APPEND jsonFlags "\"${flag}\": false")
            endif()
        endforeach()
        list(JOIN jsonFlags ", " jsonFlags)
        set(entry "    {\n      \"id\": \"${id}\"")
        foreach(member IN ITEMS name:NAME version:VERSION license:LICENSE_ID url:URL note:NOTE)
            string(REGEX REPLACE "^.*:" "" field "${member}")
            string(REGEX REPLACE ":.*$" "" member "${member}")
            _conjure_notice_json_string(value "${${field}}")
            string(APPEND entry ",\n      \"${member}\": \"${value}\"")
        endforeach()
        string(APPEND entry ",\n      \"scope\": \"${scopeName}\",\n      ${jsonFlags}")
        foreach(group IN ITEMS files hostFiles)
            set(jsonFiles "")
            foreach(file IN LISTS ${group})
                string(REGEX REPLACE "\\|.*$" "" label "${file}")
                string(REGEX REPLACE "^[^|]*\\|" "" path "${file}")
                _conjure_notice_json_string(label "${label}")
                _conjure_notice_json_string(path "${path}")
                list(APPEND jsonFiles "{\"label\": \"${label}\", \"path\": \"${path}\"}")
            endforeach()
            list(JOIN jsonFiles ",\n        " jsonFiles)
            if(jsonFiles)
                string(APPEND entry ",\n      \"${group}\": [\n        ${jsonFiles}\n      ]")
            else()
                string(APPEND entry ",\n      \"${group}\": []")
            endif()
        endforeach()
        string(APPEND entry "\n    }")
        list(APPEND manifestEntries "${entry}")

        if(NOT shipped)
            continue()
        endif()

        # The entry of the runtime notices.
        math(EXPR shippedCount "${shippedCount} + 1")
        set(title "${NAME}")
        if(VERSION)
            string(APPEND title " ${VERSION}")
        elseif(HOST_VERSION)
            string(APPEND title " ${HOST_VERSION}")
        endif()
        if(shippedCount LESS 10)
            string(APPEND contents " ")
        endif()
        string(APPEND contents "  ${shippedCount}. ${title} (${LICENSE_ID})\n")
        string(APPEND body "\n${rule}\n${shippedCount}. ${title}\n   Licence: ${LICENSE_ID}\n")
        if(URL)
            string(APPEND body "   Source:  ${URL}\n")
        endif()
        string(APPEND body "${rule}\n")
        if(NOTE)
            _conjure_notice_wrap(wrappedNote "${NOTE}" 100)
            string(APPEND body "\n${wrappedNote}\n")
        endif()
        if(hostFiles)
            string(APPEND body "\nThe text below comes from the installation on the machine that built these files.\n")
        endif()
        set(texts ${files} ${hostFiles})
        list(LENGTH texts textCount)
        foreach(file IN LISTS texts)
            string(REGEX REPLACE "\\|.*$" "" label "${file}")
            string(REGEX REPLACE "^[^|]*\\|" "" path "${file}")
            _conjure_notice_read_text(text "${path}")
            if(textCount GREATER 1)
                string(APPEND body "\n[${label}]\n")
            endif()
            string(APPEND body "\n${text}\n")
        endforeach()
        if(textCount EQUAL 0 AND URL)
            string(APPEND body "\nThe terms are published at ${URL} and are not reproduced here.\n")
        endif()
    endforeach()

    # Every dependency the calling file pins needs a notice that names it as FETCH: one declared without would
    # pass every other check while its licence never reaches the notices. Names are compared as FetchContent
    # does, without regard to case; whole-line comments are not declarations. Only the calling file is scanned, not
    # a file it includes: third_party/jxl.cmake declares libjxl and, inside libjxl's tree, libjxl_highway,
    # libjxl_brotli and libjxl_skcms, whose notices all name FETCH libjxl (a clone declares none of the three), so a
    # scan of it would have to accept those three names; tests/third_party checks that its four notices ship.
    set(fetchNames "")
    foreach(id IN LISTS ids)
        get_property(fetch GLOBAL PROPERTY "CONJURE_NOTICE_${id}_FETCH")
        if(fetch)
            string(TOLOWER "${fetch}" fetch)
            list(APPEND fetchNames "${fetch}")
        endif()
    endforeach()
    file(READ "${CMAKE_CURRENT_LIST_FILE}" declaringText)
    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" declaringText "${declaringText}")
    string(REGEX MATCHALL "FetchContent_Declare\\([ \t\r\n]*[A-Za-z0-9_.+-]+" declarations "${declaringText}")
    foreach(declaration IN LISTS declarations)
        string(REGEX REPLACE "^FetchContent_Declare\\([ \t\r\n]*" "" declared "${declaration}")
        string(TOLOWER "${declared}" declaredKey)
        if(NOT declaredKey IN_LIST fetchNames)
            message(FATAL_ERROR
                "third-party notices: ${CMAKE_CURRENT_LIST_FILE} declares the dependency ${declared} and no "
                "conjure_third_party_notice() names it as FETCH. Register its licence beside the pin, then "
                "regenerate THIRD_PARTY_NOTICES.md (python tools/third_party_notices.py --build-dir <build tree>).")
        endif()
    endforeach()

    set(header "Third-party notices\n===================\n\n")
    string(APPEND header
        "The Conjure engine files in this folder (conjure_runtime, the conjure_shared library, the shaders and, in\n"
        "a game with scripts, the Conjure assemblies under managed/) contain or were built from the software\n"
        "listed below. Each entry names the software, where it comes from and its licence, followed by the\n"
        "licence text as its authors publish it. The build of the engine that produced these files wrote this\n"
        "list, so it names what that build linked. Software the game adds on its own (game modules, assets,\n"
        "NuGet packages) is not part of it.\n\n")
    # The two names are spelled here and nowhere else in the build. This file is included once (include_guard), so
    # a variable set at its top would exist only in the directory that included it first. Export step 5 looks for
    # the first name (export_pipeline::NOTICES_FILE_NAME), tools/third_party_notices.py for the second.
    set(noticesFile "${PROJECT_BINARY_DIR}/notices/THIRD_PARTY_NOTICES.txt")
    set(manifestFile "${PROJECT_BINARY_DIR}/notices/third_party_notices.json")
    _conjure_notice_write_if_changed("${noticesFile}" "${header}${contents}\n${body}")

    list(JOIN manifestEntries ",\n" manifestEntries)
    _conjure_notice_json_string(sourceDirectory "${PROJECT_SOURCE_DIR}")
    set(manifest "{\n  \"version\": 1,\n  \"sourceDirectory\": \"${sourceDirectory}\",\n")
    string(APPEND manifest "  \"notices\": [\n${manifestEntries}\n  ]\n}\n")
    _conjure_notice_write_if_changed("${manifestFile}" "${manifest}")

    set_property(GLOBAL PROPERTY CONJURE_NOTICES_FILE "${noticesFile}")
    set_property(GLOBAL PROPERTY CONJURE_NOTICES_MANIFEST "${manifestFile}")
    list(LENGTH ids registeredCount)
    math(EXPR thirdPartyCount "${shippedCount} - 1")
    message(STATUS "third-party notices: Conjure's own and ${thirdPartyCount} of the ${registeredCount} registered "
                   "entries are in ${noticesFile}")
endfunction()

# Places the runtime notices next to <target>'s executable as a declared output of the build: the file is there
# when the target is built, returns when it was deleted, and follows a configure that changed its content.
function(conjure_ship_third_party_notices target)
    get_property(noticesFile GLOBAL PROPERTY CONJURE_NOTICES_FILE)
    if(NOT noticesFile)
        message(FATAL_ERROR
            "conjure_ship_third_party_notices(${target}): conjure_write_third_party_notices() has not run")
    endif()
    # Executables land in bin (CMakeLists.txt) plus a per-configuration folder on multi-config generators; a
    # target-dependent expression is not allowed in OUTPUT.
    get_target_property(outputDirectory ${target} RUNTIME_OUTPUT_DIRECTORY)
    if(NOT outputDirectory)
        message(FATAL_ERROR "conjure_ship_third_party_notices(${target}): the target has no runtime output directory")
    endif()
    if(CONJURE_IS_MULTI_CONFIG)
        string(APPEND outputDirectory "/$<CONFIG>")
    endif()
    cmake_path(GET noticesFile FILENAME fileName)
    set(shippedFile "${outputDirectory}/${fileName}")
    add_custom_command(
        OUTPUT "${shippedFile}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${noticesFile}" "${shippedFile}"
        DEPENDS "${noticesFile}"
        COMMENT "notices: writing ${fileName} next to ${target}"
        VERBATIM
    )
    add_custom_target(${target}_notices DEPENDS "${shippedFile}")
    set_target_properties(${target}_notices PROPERTIES FOLDER "Engine")
    add_dependencies(${target} ${target}_notices)
endfunction()

# Writes the throwaway project of one probe case, a project without a language that includes
# cmake/ConjureNoticesProbe.cmake, into a folder of its own (the cases run side by side and write their fixtures
# next to it) and sets <outVariable> to that folder.
function(_conjure_notice_probe_root outVariable case)
    set(root "${PROJECT_BINARY_DIR}/notices_probe/${case}")
    file(WRITE "${root}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.28)
project(conjure_notices_probe VERSION 1.2.3 LANGUAGES NONE)
include("${CONJURE_NOTICES_PROBE}")
]=])
    set(${outVariable} "${root}" PARENT_SCOPE)
endfunction()

# Registers one configure probe of this file as the CTest entry notices.<name>: the probe project is configured
# afresh on every run with PROBE_CASE=<case>, and the entry passes when the configure prints <pattern>. For a case
# that must stop the configure the pattern is the text of its FATAL_ERROR; CMake breaks such a text at spaces, so a
# pattern joins its words with ".*".
function(_conjure_notice_add_probe name case pattern)
    _conjure_notice_probe_root(root "${case}")
    add_test(NAME notices.${name}
        COMMAND "${CMAKE_COMMAND}" --fresh -S "${root}/source" -B "${root}/build"
            -G "${CMAKE_GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCONJURE_NOTICES_PROBE=${PROJECT_SOURCE_DIR}/cmake/ConjureNoticesProbe.cmake" "-DPROBE_CASE=${case}"
    )
    set_tests_properties(notices.${name} PROPERTIES LABELS "build" TIMEOUT 120 PASS_REGULAR_EXPRESSION "${pattern}")
endfunction()

# Registers the CTest entries of the notices (call it from a directory below tests/, where testing is enabled):
#
#   notices.configure_*, notices.*_fails_configure   what conjure_write_third_party_notices() writes for a tree
#       with and without the editor, and every registration this file refuses (cmake/ConjureNoticesProbe.cmake)
#   notices.ship_places_and_restores_the_file         conjure_ship_third_party_notices() through a real build of
#       the probe: the file appears, returns after it was deleted and follows a changed configure
#   notices.up_to_date     the tracked THIRD_PARTY_NOTICES.md against the licence files this tree fetched (a
#       section of software the tree does not fetch is compared by the notice it was written for, not its text)
#   notices.tool           the self-test of tools/third_party_notices.py
#
# The last two need a Python interpreter and are left out without one, as the tool entries of tests/CMakeLists.txt.
function(conjure_add_third_party_notices_tests)
    get_property(manifest GLOBAL PROPERTY CONJURE_NOTICES_MANIFEST)
    if(NOT manifest OR TEST notices.configure_runtime_tree)
        return()
    endif()

    _conjure_notice_add_probe(configure_runtime_tree runtime_tree "notices probe: runtime_tree passed")
    _conjure_notice_add_probe(configure_editor_tree editor_tree "notices probe: editor_tree passed")
    _conjure_notice_add_probe(pin_mismatch_fails_configure pin_mismatch
        "VERSION.*'v2[.]0'.*is.*not.*the.*GIT_TAG.*'v1[.]0'")
    _conjure_notice_add_probe(missing_licence_file_fails_configure missing_licence_file
        "licence.*file.*dependency/COPYING'.*of.*Alpha.*does.*not.*exist")
    _conjure_notice_add_probe(notice_registered_twice_fails_configure registered_twice
        "notice\\(alpha\\):.*registered.*twice")
    _conjure_notice_add_probe(unknown_scope_fails_configure unknown_scope
        "SCOPE.*is.*RUNTIME,.*EDITOR.*or.*BUILD,.*not.*'SERVER'")
    _conjure_notice_add_probe(notice_without_terms_fails_configure no_terms
        "without.*LICENSE_FILES.*needs.*a.*NOTE")
    _conjure_notice_add_probe(host_files_without_flag_fail_configure host_files_without_flag
        "HOST_FILES.*and.*HOST_VERSION.*need.*HOST_PROVIDED")
    _conjure_notice_add_probe(host_provided_with_pin_fails_configure host_provided_with_pin
        "HOST_PROVIDED.*software.*is.*not.*pinned")
    _conjure_notice_add_probe(line_break_in_text_fails_configure line_break_in_text
        "NOTE.*must.*not.*hold.*a.*line.*break")
    _conjure_notice_add_probe(second_write_fails_configure written_twice
        "conjure_write_third_party_notices:.*called.*twice")
    _conjure_notice_add_probe(ship_before_write_fails_configure ship_before_write
        "conjure_write_third_party_notices\\(\\).*has.*not.*run")
    _conjure_notice_add_probe(declaration_without_notice_fails_configure declaration_without_notice
        "declares.*the.*dependency.*probe_absent.*and.*no.*conjure_third_party_notice\\(\\).*names.*it")

    # The "ship" case is configured and built by the probe itself, in script mode.
    _conjure_notice_probe_root(shipRoot ship)
    add_test(NAME notices.ship_places_and_restores_the_file
        COMMAND "${CMAKE_COMMAND}" "-DPROBE_SOURCE=${shipRoot}/source" "-DPROBE_BINARY=${shipRoot}/build"
            "-DPROBE_GENERATOR=${CMAKE_GENERATOR}" "-DPROBE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            -P "${PROJECT_SOURCE_DIR}/cmake/ConjureNoticesProbe.cmake"
    )
    set_tests_properties(notices.ship_places_and_restores_the_file PROPERTIES
        LABELS "build" TIMEOUT 300 PASS_REGULAR_EXPRESSION "notices probe: ship passed"
    )

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(NOT Python3_Interpreter_FOUND)
        message(STATUS "third-party notices: Python 3 not found, notices.up_to_date and notices.tool not registered")
        return()
    endif()
    add_test(NAME notices.up_to_date
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/third_party_notices.py"
                --check --manifest "${manifest}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(notices.up_to_date PROPERTIES LABELS "generated" TIMEOUT 120)
    add_test(NAME notices.tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_third_party_notices.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(notices.tool PROPERTIES LABELS "generated" TIMEOUT 120)
endfunction()
