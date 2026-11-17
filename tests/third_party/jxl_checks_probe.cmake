# One configure probe of third_party/jxl_checks.cmake and third_party/jxl.cmake (tests/third_party/CMakeLists.txt
# registers the cases). Included by a throwaway project without a language; CONJURE_JXL_PROBE is this file,
# CONJURE_JXL_THIRD_PARTY the engine's third_party/ directory, CONJURE_NOTICES_MODULE the engine's
# cmake/ConjureNotices.cmake and PROBE_CASE the case. A passing case prints "jxl probe: <case> passed"; every other
# case must stop the configure with the check's message.
#
# The stand-in of libjxl's tree is shaped like a real Windows configure of the pin: the libraries libjxl builds are
# GLOBAL imported targets, visible everywhere like the real ones, while an imported target that one of libjxl's
# directories creates through find_package() is visible only there and below (Threads::Threads, created in the root as
# libjxl's find_package(Threads) does, and in some cases a dependency nobody pinned in lib/). The cases:
#
#   - the build check cases (pinned, and failingCases, each of which changes one thing) add the stand-in tree with
#     add_subdirectory() as jxl.cmake adds the real one, then run conjure_jxl_check_tree and conjure_jxl_check_build
#     from this directory, where jxl.cmake runs them, so that the checks see the real scoping;
#   - incomplete_tree and missing_tree call conjure_jxl_check_tree on a partial or empty directory, adding nothing;
#   - the jxl.cmake cases (jxlCmakeCases) include jxl.cmake itself with FETCHCONTENT_FULLY_DISCONNECTED ON, so that
#     nothing is downloaded even if it tried. unpacked_tree and without_renderer write the stand-in where jxl.cmake
#     declares libjxl's tree, as if the four archives had been unpacked; the others name it with
#     FETCHCONTENT_SOURCE_DIR_LIBJXL, as a clone. unpacked_tree, source_override and without_renderer go through
#     jxl.cmake's add_subdirectory() and its checks; nested_override stops in conjure_jxl_check_overrides before
#     anything is declared, and clone_without_submodules in conjure_jxl_check_tree before the tree is added. The
#     cases include the engine's cmake/ConjureNotices.cmake first, so jxl.cmake registers its licence notices with
#     the real conjure_third_party_notice(), and the three that add the tree resolve them as the engine's configure
#     does: with CONJURE_WITH_RENDERER ON the four notices must be used (and so ship), without the renderer, which
#     alone links libjxl, none may be.

cmake_minimum_required(VERSION 3.28)

set(jxlCmakeCases unpacked_tree source_override without_renderer nested_override clone_without_submodules)
set(failingCases
    version_mismatch shared_library missing_library unpinned_dependency hidden_dependency hidden_alias compile_options
    cache_entry
)

# The stand-ins of libjxl's root directory: Highway and Brotli, the thread library, and lib/.
function(probe_add_root_stand_ins)
    add_library(Threads::Threads INTERFACE IMPORTED)
    set_target_properties(Threads::Threads PROPERTIES INTERFACE_COMPILE_OPTIONS "-pthread")
    foreach(target IN ITEMS hwy brotlicommon brotlidec brotlienc)
        add_library(${target} STATIC IMPORTED GLOBAL)
    endforeach()
    set_target_properties(brotlidec brotlienc PROPERTIES INTERFACE_LINK_LIBRARIES "brotlicommon")
    set_target_properties(hwy PROPERTIES INTERFACE_LINK_LIBRARIES "m")
    if(PROBE_CASE STREQUAL "unpinned_dependency")
        # Two levels down, behind a generator expression, as an upstream option would add it; GLOBAL like the target
        # libjxl's FindLCMS2 creates.
        add_library(lcms2 STATIC IMPORTED GLOBAL)
        set_target_properties(hwy PROPERTIES INTERFACE_LINK_LIBRARIES "m;$<LINK_ONLY:lcms2>")
    elseif(PROBE_CASE STREQUAL "compile_options")
        set_target_properties(hwy PROPERTIES INTERFACE_COMPILE_OPTIONS "/arch:AVX2")
    elseif(PROBE_CASE STREQUAL "cache_entry")
        # Highway's option(BUILD_SHARED_LIBS ...) without CMP0077 would write the engine's cache.
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
    endif()
    add_subdirectory(lib)
endfunction()

# The stand-ins of libjxl's lib/ directory, where libjxl creates its libraries.
function(probe_add_library_stand_ins)
    set(version "${CONJURE_JXL_VERSION}")
    if(PROBE_CASE STREQUAL "version_mismatch")
        set(version "0.12.0")
    endif()
    add_library(jxl_base INTERFACE IMPORTED GLOBAL)
    set_target_properties(jxl_base PROPERTIES INTERFACE_COMPILE_DEFINITIONS "JXL_STATIC_DEFINE")
    if(PROBE_CASE STREQUAL "shared_library")
        add_library(jxl_cms SHARED IMPORTED GLOBAL)
    elseif(NOT PROBE_CASE STREQUAL "missing_library")
        add_library(jxl_cms STATIC IMPORTED GLOBAL)
    endif()
    if(TARGET jxl_cms)
        set_target_properties(jxl_cms PROPERTIES VERSION "${version}" INTERFACE_LINK_LIBRARIES "jxl_base")
    endif()
    # libjxl's jxl links its PRIVATE dependencies, which a static library hands on as $<LINK_ONLY:...>.
    set(dependencies hwy Threads::Threads brotlidec brotlienc)
    if(PROBE_CASE STREQUAL "hidden_dependency")
        # What pkg_check_modules(... IMPORTED_TARGET) creates: visible in this directory only.
        add_library(PkgConfig::TCMallocMinimal INTERFACE IMPORTED)
        list(APPEND dependencies PkgConfig::TCMallocMinimal)
    elseif(PROBE_CASE STREQUAL "hidden_alias")
        # The same imported target behind an ALIAS, which is scoped to this directory too (CMake 3.18 and later) and
        # is not an imported target itself, so no IMPORTED_TARGETS list names it.
        add_library(PkgConfig::TCMallocMinimal INTERFACE IMPORTED)
        add_library(tcmalloc::minimal ALIAS PkgConfig::TCMallocMinimal)
        list(APPEND dependencies tcmalloc::minimal)
    endif()
    set(linkOnly "")
    foreach(dependency IN LISTS dependencies)
        list(APPEND linkOnly "$<LINK_ONLY:${dependency}>")
    endforeach()
    add_library(jxl STATIC IMPORTED GLOBAL)
    set_target_properties(jxl PROPERTIES
        VERSION "${version}"
        INTERFACE_LINK_LIBRARIES "jxl_base;jxl_cms;${linkOnly}"
    )
endfunction()

# Writes a stand-in of libjxl's tree to <root>: its two CMakeLists.txt, which create the targets above, libjxl's
# licence files, and the markers and licence files of the dependencies named in ARGN (highway, brotli, skcms).
function(probe_make_tree root)
    file(REMOVE_RECURSE "${root}")
    file(WRITE "${root}/CMakeLists.txt" "probe_add_root_stand_ins()\n")
    file(WRITE "${root}/lib/CMakeLists.txt" "probe_add_library_stand_ins()\n")
    file(WRITE "${root}/LICENSE" "")
    file(WRITE "${root}/PATENTS" "")
    foreach(dependency IN LISTS ARGN)
        set(directory "${root}/third_party/${dependency}")
        file(WRITE "${directory}/LICENSE" "")
        if(dependency STREQUAL "highway")
            file(WRITE "${directory}/CMakeLists.txt" "")
            file(WRITE "${directory}/LICENSE-BSD3" "")
        elseif(dependency STREQUAL "brotli")
            file(WRITE "${directory}/c/include/brotli/decode.h" "")
        elseif(dependency STREQUAL "skcms")
            file(WRITE "${directory}/skcms.h" "")
        endif()
    endforeach()
endfunction()

# The configure of libjxl that jxl.cmake runs, on the stand-in tree at <root>: <sharedLibsCachedBefore> as jxl.cmake
# computes it, then the tree and the build checks.
function(probe_configure_stand_in root)
    set(sharedLibsCachedBefore FALSE)
    if(DEFINED CACHE{BUILD_SHARED_LIBS})
        set(sharedLibsCachedBefore TRUE)
    endif()
    add_subdirectory("${root}" "${CMAKE_BINARY_DIR}/libjxl-build")
    conjure_jxl_check_tree("${root}" "${CMAKE_BINARY_DIR}" FALSE)
    conjure_jxl_check_build("${CONJURE_JXL_VERSION}" ${sharedLibsCachedBefore} "${root}")
endfunction()

# Sets <outVariable> to the libjxl version jxl.cmake pins, which names the directory it declares libjxl's tree in.
function(probe_pinned_version outVariable)
    set(pattern "^set\\(CONJURE_JXL_VERSION \"([^\"]+)\"")
    file(STRINGS "${CONJURE_JXL_THIRD_PARTY}/jxl.cmake" line REGEX "${pattern}")
    if(NOT line MATCHES "${pattern}")
        message(FATAL_ERROR "jxl probe: jxl.cmake sets no CONJURE_JXL_VERSION this probe can read")
    endif()
    set(${outVariable} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# Fails unless jxl.cmake declared Highway, Brotli and skcms in libjxl's third_party/ directory (<declared> TRUE:
# FETCHCONTENT_FULLY_DISCONNECTED marks a declared name populated without unpacking, so the source directory is what
# tells) or declared none of them (FALSE: in a clone, whose submodules a declaration would unpack over).
function(probe_check_nested declared)
    FetchContent_GetProperties(libjxl SOURCE_DIR libjxlDirectory)
    foreach(dependency IN ITEMS highway brotli skcms)
        FetchContent_GetProperties(libjxl_${dependency} SOURCE_DIR directory POPULATED populated)
        if(declared AND NOT (populated AND directory STREQUAL "${libjxlDirectory}/third_party/${dependency}"))
            message(FATAL_ERROR "jxl probe: jxl.cmake did not place libjxl_${dependency} in libjxl's tree (populated "
                                "'${populated}', at '${directory}')")
        endif()
        if(NOT declared AND populated)
            message(FATAL_ERROR "jxl probe: jxl.cmake populated libjxl_${dependency} into the clone ${directory}")
        endif()
    endforeach()
endfunction()

# Resolves the notices jxl.cmake registered through cmake/ConjureNotices.cmake's own _conjure_notice_resolve(), the
# step conjure_write_third_party_notices() takes for each notice: a notice is used only when its FETCH content was
# populated and USED holds, and its licence files are read relative to that content's source directory (a missing
# one of a used notice stops the configure there). Fails unless jxl.cmake registered exactly the RUNTIME notices of
# libjxl, Highway, Brotli and skcms, all reading libjxl's tree, and unless each is used as <expectUsed> says, a used
# one with every licence file it registered found in that tree.
function(probe_check_notices expectUsed)
    get_property(ids GLOBAL PROPERTY CONJURE_NOTICE_IDS)
    if(NOT ids STREQUAL "libjxl;highway;brotli;skcms")
        message(FATAL_ERROR "jxl probe: jxl.cmake registered the notices '${ids}', not libjxl, highway, brotli, skcms")
    endif()
    FetchContent_GetProperties(libjxl SOURCE_DIR libjxlDirectory)
    foreach(id IN LISTS ids)
        get_property(scope GLOBAL PROPERTY "CONJURE_NOTICE_${id}_SCOPE")
        get_property(fetch GLOBAL PROPERTY "CONJURE_NOTICE_${id}_FETCH")
        get_property(registeredFiles GLOBAL PROPERTY "CONJURE_NOTICE_${id}_LICENSE_FILES")
        if(NOT scope STREQUAL "RUNTIME" OR NOT fetch STREQUAL "libjxl" OR NOT registeredFiles)
            message(FATAL_ERROR "jxl probe: the notice ${id} has scope '${scope}', FETCH '${fetch}' and licence files "
                                "'${registeredFiles}'; it ships only as a RUNTIME notice that reads its licence files "
                                "from libjxl's tree")
        endif()
        _conjure_notice_resolve("${id}" used files hostFiles)
        if(expectUsed AND NOT used)
            message(FATAL_ERROR "jxl probe: the notice ${id} is not used, so the runtime notices would leave it out")
        endif()
        if(NOT expectUsed AND used)
            message(FATAL_ERROR "jxl probe: the notice ${id} is used in a tree without the renderer, which does not "
                                "link libjxl")
        endif()
        if(NOT used)
            continue()
        endif()
        list(LENGTH registeredFiles registeredCount)
        list(LENGTH files resolvedCount)
        if(NOT resolvedCount EQUAL registeredCount)
            message(FATAL_ERROR "jxl probe: the notice ${id} resolved '${files}' for the licence files "
                                "'${registeredFiles}'")
        endif()
        foreach(file IN LISTS files)
            string(REGEX REPLACE "^[^|]*\\|" "" filePath "${file}")
            cmake_path(IS_PREFIX libjxlDirectory "${filePath}" NORMALIZE insideTree)
            if(NOT insideTree)
                message(FATAL_ERROR "jxl probe: the notice ${id} reads ${filePath}, outside libjxl's tree "
                                    "${libjxlDirectory}")
            endif()
        endforeach()
    endforeach()
endfunction()

if(PROBE_CASE IN_LIST jxlCmakeCases)
    include(FetchContent)
    set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)
    if(PROBE_CASE STREQUAL "unpacked_tree" OR PROBE_CASE STREQUAL "without_renderer")
        probe_pinned_version(version)
        set(tree "${FETCHCONTENT_BASE_DIR}/libjxl-${version}-src")
    else()
        set(tree "${CMAKE_BINARY_DIR}/libjxl-src")
        set(FETCHCONTENT_SOURCE_DIR_LIBJXL "${tree}" CACHE PATH "" FORCE)
    endif()
    if(PROBE_CASE STREQUAL "clone_without_submodules")
        probe_make_tree("${tree}" highway brotli)
    else()
        probe_make_tree("${tree}" highway brotli skcms)
    endif()
    if(PROBE_CASE STREQUAL "nested_override")
        set(FETCHCONTENT_SOURCE_DIR_LIBJXL_HIGHWAY "${tree}/third_party/highway" CACHE PATH "" FORCE)
    endif()
    # What the engine's configure has done by the time third_party/ includes jxl.cmake: the notices module is loaded
    # and the renderer option set.
    include("${CONJURE_NOTICES_MODULE}")
    set(CONJURE_WITH_RENDERER ON)
    if(PROBE_CASE STREQUAL "without_renderer")
        set(CONJURE_WITH_RENDERER OFF)
    endif()

    include("${CONJURE_JXL_THIRD_PARTY}/jxl.cmake")
    if(NOT TARGET conjure::thirdparty::jxl)
        message(FATAL_ERROR "jxl probe: jxl.cmake did not create conjure::thirdparty::jxl")
    endif()
    if(PROBE_CASE STREQUAL "unpacked_tree")
        probe_check_nested(TRUE)
        probe_check_notices(TRUE)
    elseif(PROBE_CASE STREQUAL "source_override")
        probe_check_nested(FALSE)
        probe_check_notices(TRUE)
    elseif(PROBE_CASE STREQUAL "without_renderer")
        probe_check_notices(FALSE)
    endif()
    message(STATUS "jxl probe: ${PROBE_CASE} passed")
    return()
endif()

include("${CONJURE_JXL_THIRD_PARTY}/jxl_checks.cmake")
set(CONJURE_JXL_VERSION "0.13.0")
set(tree "${CMAKE_BINARY_DIR}/libjxl-src")

if(PROBE_CASE STREQUAL "pinned")
    probe_make_tree("${tree}" highway brotli skcms)
    probe_configure_stand_in("${tree}")
    # An entry that existed before libjxl was configured is the engine's own, not a leak.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
    conjure_jxl_check_build("${CONJURE_JXL_VERSION}" TRUE "${tree}")
    # A Threads::Threads the engine found before third_party/ is visible here, and its -pthread passes.
    add_library(Threads::Threads INTERFACE IMPORTED)
    set_target_properties(Threads::Threads PROPERTIES INTERFACE_COMPILE_OPTIONS "-pthread")
    conjure_jxl_check_build("${CONJURE_JXL_VERSION}" TRUE "${tree}")
    message(STATUS "jxl probe: pinned passed")
elseif(PROBE_CASE STREQUAL "incomplete_tree")
    probe_make_tree("${tree}" highway brotli)
    conjure_jxl_check_tree("${tree}" "${CMAKE_BINARY_DIR}" FALSE)
elseif(PROBE_CASE STREQUAL "missing_tree")
    file(REMOVE_RECURSE "${tree}")
    file(MAKE_DIRECTORY "${tree}")
    conjure_jxl_check_tree("${tree}" "${CMAKE_BINARY_DIR}" FALSE)
elseif(PROBE_CASE IN_LIST failingCases)
    probe_make_tree("${tree}" highway brotli skcms)
    probe_configure_stand_in("${tree}")
else()
    message(FATAL_ERROR "jxl probe: unknown PROBE_CASE '${PROBE_CASE}'")
endif()
