# Configure checks of the libjxl pin (third_party/jxl.cmake). Kept apart from the file that downloads, so that the
# CTest probes of tests/third_party/CMakeLists.txt run them on stand-in directories and targets without fetching
# anything. The checks are configure-time on purpose: tests never include third-party headers (CONVENTIONS.md 6).

include_guard(GLOBAL)

# Fails the configure when a FetchContent source override names one of the three libraries third_party/jxl.cmake
# unpacks into libjxl's tree. libjxl builds Highway, Brotli and skcms from its own third_party/ directory only, so an
# override of one would leave that directory without it. FETCHCONTENT_SOURCE_DIR_LIBJXL (a libjxl clone) is supported:
# jxl.cmake then unpacks nothing into the clone, whose submodules bring the three (conjure_jxl_check_tree).
function(conjure_jxl_check_overrides)
    foreach(name IN ITEMS LIBJXL_HIGHWAY LIBJXL_BROTLI LIBJXL_SKCMS)
        if(FETCHCONTENT_SOURCE_DIR_${name})
            message(FATAL_ERROR
                "third_party: FETCHCONTENT_SOURCE_DIR_${name} is not supported: libjxl builds Highway, Brotli and "
                "skcms from its own third_party/ directory only. Clear it; to build a local libjxl, set "
                "FETCHCONTENT_SOURCE_DIR_LIBJXL to a clone of the pinned tag with its submodules.")
        endif()
    endforeach()
endfunction()

# Fails the configure unless the tree at <libjxlSourceDir> holds libjxl and, in its third_party/ directory, Highway,
# Brotli and skcms. <overridden> is TRUE for the clone FETCHCONTENT_SOURCE_DIR_LIBJXL names, whose submodules bring
# the three; otherwise jxl.cmake unpacked them there and <fetchBaseDir> is FETCHCONTENT_BASE_DIR.
#
# FetchContent unpacks a tree only when its download step runs, and that step depends on its script, its URL and its
# stamps, never on the tree. A tree deleted by hand therefore stays missing, and so does every tree of a configure
# with FETCHCONTENT_FULLY_DISCONNECTED ON before it was unpacked. libjxl's tree unpacked again at the same version
# (its sub-build or stamps removed, or its URL changed) has lost the three, whose steps do not run again. The
# remedy keeps the archives: with the stamp directories gone the steps run again (FetchContent configures and builds
# each sub-build on every configure), and an archive in its sub-build whose SHA-256 matches is unpacked without a
# download.
function(conjure_jxl_check_tree libjxlSourceDir fetchBaseDir overridden)
    set(stamps "${fetchBaseDir}/libjxl*-subbuild/*-populate-prefix/src/*-populate-stamp")
    if(NOT EXISTS "${libjxlSourceDir}/CMakeLists.txt")
        if(overridden)
            message(FATAL_ERROR "third_party: FETCHCONTENT_SOURCE_DIR_LIBJXL names ${libjxlSourceDir}, which holds no "
                                "CMakeLists.txt; point it at a libjxl clone")
        endif()
        message(FATAL_ERROR
            "third_party: libjxl's tree ${libjxlSourceDir} is missing: it was deleted by hand (FetchContent's stamps "
            "still say it was unpacked), or the configure ran with FETCHCONTENT_FULLY_DISCONNECTED ON, which unpacks "
            "nothing. Delete the stamp directories ${stamps} and configure with FETCHCONTENT_FULLY_DISCONNECTED OFF: "
            "an archive already in its sub-build is unpacked again without a download.")
    endif()
    foreach(marker IN ITEMS highway/CMakeLists.txt brotli/c/include/brotli/decode.h skcms/skcms.h)
        if(NOT EXISTS "${libjxlSourceDir}/third_party/${marker}")
            if(overridden)
                message(FATAL_ERROR
                    "third_party: the libjxl clone ${libjxlSourceDir} lacks third_party/${marker}; run 'git submodule "
                    "update --init third_party/highway third_party/brotli third_party/skcms' in it (third_party/"
                    "jxl.cmake unpacks nothing into a FETCHCONTENT_SOURCE_DIR_LIBJXL tree)")
            endif()
            message(FATAL_ERROR
                "third_party: libjxl's tree lacks ${libjxlSourceDir}/third_party/${marker}: it was deleted by hand, "
                "or libjxl was unpacked again after its dependencies were placed in it. Delete the stamp directories "
                "${stamps} and configure again: the archives kept in the sub-builds are unpacked again without a "
                "download.")
        endif()
    endforeach()
endfunction()

# The source directories of the tree added with add_subdirectory(<rootDirectory>): the root and every directory
# below it (Highway's and Brotli's included), in <outVariable>.
function(conjure_jxl_directories outVariable rootDirectory)
    set(directories "")
    set(pending "${rootDirectory}")
    while(pending)
        list(POP_FRONT pending directory)
        list(APPEND directories "${directory}")
        get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
        list(APPEND pending ${subdirectories})
    endwhile()
    set(${outVariable} "${directories}" PARENT_SCOPE)
endfunction()

# Fails the configure unless the targets libjxl's CMake created in the tree added from <libjxlSourceDir> are what
# third_party/jxl.cmake pins. It runs in third_party/, the directory that added libjxl's tree:
#
#   - jxl (encoder and decoder) and jxl_cms declare VERSION <expectedVersion>. libjxl sets it in its own
#     lib/CMakeLists.txt, so it names the source that was unpacked, whatever the URL said.
#   - both are static libraries.
#   - the link closure of jxl holds only libjxl's own targets, Highway, Brotli and the system thread library. Any other
#     target (lcms2 from libjxl's FindLCMS2, ...) came in through an option upstream renamed or added: a dependency
#     nobody pinned. An imported target that find_package() or pkg_check_modules() creates in one of libjxl's
#     directories (PNG::PNG, PkgConfig::TCMallocMinimal, ...) is visible there and below but not here, unless it was
#     made GLOBAL: if(TARGET) is false for it, as for a plain library name such as m or atomic, which passes. The
#     IMPORTED_TARGETS of libjxl's directories tell the two apart, and such a target fails by its name, as its
#     compile options cannot be read from here. An ALIAS such a directory makes of one (tcmalloc::minimal, scoped to
#     that directory since CMake 3.18) is in no IMPORTED_TARGETS list, so any other name with '::' that is no target
#     here fails too. Threads::Threads (libjxl's own find_package(Threads)) is allowed either way, and so are the
#     '::@' markers CMake puts around a link item added from another directory.
#   - no target of the closure but Threads::Threads (whose -pthread is the thread library's own flag) passes compile
#     options to its consumers, so no instruction-set flag reaches the engine objects that link libjxl (baseline x64,
#     docs/design/14_physics.md 8). Highway picks its SIMD code at run time and needs none.
#   - <sharedLibsCachedBefore> is FALSE and the cache now has a BUILD_SHARED_LIBS entry: a libjxl directory wrote the
#     engine's own switch instead of reading the scoped variable third_party/jxl.cmake set. The entry is removed
#     before the configure stops: CMake saves the cache of a failed configure, and the next one would otherwise find
#     the entry already there and take it for the engine's own.
function(conjure_jxl_check_build expectedVersion sharedLibsCachedBefore libjxlSourceDir)
    set(allowedTargets jxl jxl_cms jxl_base hwy brotlienc brotlidec brotlicommon Threads::Threads)
    foreach(target IN ITEMS jxl jxl_cms)
        if(NOT TARGET ${target})
            message(FATAL_ERROR "third_party: libjxl defines no target ${target}; the pin in third_party/jxl.cmake "
                                "expects it (did the tag rename its libraries?)")
        endif()
        get_target_property(version ${target} VERSION)
        if(NOT version STREQUAL expectedVersion)
            message(FATAL_ERROR "third_party: libjxl's ${target} declares version '${version}', but "
                                "third_party/jxl.cmake pins ${expectedVersion} (CONJURE_JXL_VERSION); the archive and "
                                "the pin disagree")
        endif()
        get_target_property(type ${target} TYPE)
        if(NOT type STREQUAL "STATIC_LIBRARY")
            message(FATAL_ERROR "third_party: libjxl's ${target} is a ${type}; third_party/jxl.cmake pins static "
                                "libraries (BUILD_SHARED_LIBS OFF in libjxl's scope)")
        endif()
    endforeach()

    conjure_jxl_directories(directories "${libjxlSourceDir}")
    set(importedInTree "")
    foreach(directory IN LISTS directories)
        get_property(imported DIRECTORY "${directory}" PROPERTY IMPORTED_TARGETS)
        list(APPEND importedInTree ${imported})
    endforeach()

    set(pending jxl)
    set(visited "")
    while(pending)
        list(POP_FRONT pending item)
        # A static library's PRIVATE dependencies surface as $<LINK_ONLY:x>; other generator expressions are not
        # target names.
        string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" item "${item}")
        if(item MATCHES "^\\$<" OR item IN_LIST visited)
            continue()
        endif()
        list(APPEND visited "${item}")
        if(NOT TARGET "${item}")
            if(item STREQUAL "Threads::Threads" OR item MATCHES "^::@")
                continue()
            endif()
            if(item IN_LIST importedInTree)
                message(FATAL_ERROR "third_party: libjxl links ${item}, which third_party/jxl.cmake does not pin: an "
                                    "imported target one of libjxl's directories created (find_package or "
                                    "pkg_check_modules), whose compile options cannot be read from third_party/; an "
                                    "upstream option changed")
            endif()
            if(item MATCHES "::")
                message(FATAL_ERROR "third_party: libjxl links ${item}, which third_party/jxl.cmake does not pin: a "
                                    "name with '::' that is no target in third_party/, such as an ALIAS one of "
                                    "libjxl's directories made of an imported target it created; an upstream option "
                                    "changed")
            endif()
            continue()
        endif()
        if(NOT item IN_LIST allowedTargets)
            message(FATAL_ERROR "third_party: libjxl links ${item}, which third_party/jxl.cmake does not pin (allowed: "
                                "${allowedTargets}); an upstream option changed")
        endif()
        if(NOT item STREQUAL "Threads::Threads")
            get_target_property(options ${item} INTERFACE_COMPILE_OPTIONS)
            if(options)
                message(FATAL_ERROR "third_party: libjxl's ${item} passes the compile options '${options}' to every "
                                    "target that links it; the engine objects must keep the baseline instruction set")
            endif()
        endif()
        get_target_property(type ${item} TYPE)
        set(dependencies "")
        if(NOT type STREQUAL "INTERFACE_LIBRARY")
            get_target_property(direct ${item} LINK_LIBRARIES)
            if(direct)
                list(APPEND dependencies ${direct})
            endif()
        endif()
        get_target_property(interface ${item} INTERFACE_LINK_LIBRARIES)
        if(interface)
            list(APPEND dependencies ${interface})
        endif()
        list(APPEND pending ${dependencies})
    endwhile()

    if(NOT sharedLibsCachedBefore AND DEFINED CACHE{BUILD_SHARED_LIBS})
        unset(BUILD_SHARED_LIBS CACHE)
        message(FATAL_ERROR "third_party: configuring libjxl created the cache entry BUILD_SHARED_LIBS; libjxl and its "
                            "dependencies must read the variable third_party/jxl.cmake scopes to them, never write the "
                            "engine's own")
    endif()
endfunction()
