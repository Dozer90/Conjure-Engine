# JPEG XL (ISO/IEC 18181): libjxl, the reference implementation, and the three libraries it compiles from source.
# Included from third_party/CMakeLists.txt, which has loaded FetchContent, set CMAKE_POLICY_VERSION_MINIMUM and forced
# BUILD_SHARED_LIBS OFF for the whole directory.
#
# Pins (each archive is checked against its SHA-256 before it is unpacked):
#
#   libjxl   v0.13.0, the release tarball (tag commit 5a7da834); BSD-3-Clause plus Google's patent grant (PATENTS)
#   Highway  1.2.0, commit 457c8917; Apache-2.0 OR BSD-3-Clause
#   Brotli   v1.2.1, commit db24a98a; MIT
#   skcms    2025_09_16, commit 96d9171c; BSD-3-Clause
#
# The three are the commits libjxl v0.13.0 names in its deps.sh and .gitmodules.
#
# Bumping libjxl: take the new tag's tarball and the commits its deps.sh names, download each archive once to compute
# its SHA-256 (GitHub archive bytes depend on the URL form, so hash the exact URL), change CONJURE_JXL_VERSION, the
# three CONJURE_JXL_*_COMMIT and the four URL_HASH together (the URLs and the licence notices at the end read the
# version and the commits) along with the release names in the notices' NOTE and the list above, read the new tag's
# option list in its top-level CMakeLists.txt, and regenerate THIRD_PARTY_NOTICES.md (tools/third_party_notices.py).
#
# The GitHub archive of libjxl carries no submodules, and libjxl's third_party/CMakeLists.txt looks for Highway,
# Brotli and skcms only inside its own tree (third_party/highway, brotli, skcms), so the three are unpacked there.
# libjxl's tree is versioned (libjxl-<version>-src): a new libjxl version unpacks into a new directory, and as the
# directory is part of each nested declaration, the three are unpacked into it again even when their own pin stayed.
# Never PROVISION_DEPENDENCIES: it runs deps.sh at configure time, which downloads without checking any hash.
#
# FETCHCONTENT_SOURCE_DIR_LIBJXL may name a libjxl clone of the pinned tag to work on the decoder: nothing is unpacked
# into it (an unpack step deletes its target directory first, which would take the clone's submodules and their
# edits), so the clone brings Highway, Brotli and skcms as its own submodules and the three are not declared. An
# override of one of the three stops the configure (conjure_jxl_check_overrides).
#
# Only the static libraries are configured: no tool, test, documentation, benchmark, example, JNI, plugin, viewer,
# fuzzer or man page, no tcmalloc, sjpeg, OpenEXR, libpng or lcms2. skcms is the colour management system; JPEG
# reconstruction (lossless JPEG recompression) and metadata boxes stay on, libjxl's defaults, although the engine
# decodes pixels only and asks for neither (libjxl then skips those boxes). libjxl compiles itself as C++17 in its own
# directory and adds no warning flags under MSVC; its directory is SYSTEM and EXCLUDE_FROM_ALL, so nothing of it is
# built unless an engine target links conjure::thirdparty::jxl, and its headers never trigger our warnings.
#
# conjure::thirdparty::jxl is libjxl's jxl library: the encoder and the decoder, with jxl_cms (skcms) linked
# publicly. Never link libjxl's decoder-only jxl_dec into a binary that also holds jxl: the two carry the same symbols.
# jxl_threads (libjxl's std::thread runner) is configured but not exposed: the engine runs work on core::jobs, and a
# JxlParallelRunner over the job system is the way to decode on several threads.

include("${CMAKE_CURRENT_LIST_DIR}/jxl_checks.cmake")

# The libjxl version the declarations below pin: its tag names the archive, conjure_jxl_check_build() compares it with
# the version libjxl's own CMake declares, and as a cache entry it tells the rest of the build which version it links.
set(CONJURE_JXL_VERSION "0.13.0" CACHE INTERNAL "libjxl version pinned by third_party/jxl.cmake")
# The commits of the three libraries, read by their declarations and their licence notices.
set(CONJURE_JXL_HIGHWAY_COMMIT 457c891775a7397bdb0376bb1031e6e027af1c48)
set(CONJURE_JXL_BROTLI_COMMIT db24a98aed38a16de78cc05ca15cdd8dc2a0d92d)
set(CONJURE_JXL_SKCMS_COMMIT 96d9171c94b937a1b5f0293de7309ac16311b722)

# A function, so that BUILD_SHARED_LIBS, BUILD_TESTING and the policy and find_package switches below reach libjxl's
# directory (add_subdirectory copies the calling scope) and nothing else of the engine.
function(conjure_add_libjxl)
    conjure_jxl_check_overrides()
    FetchContent_Declare(libjxl
        URL           https://github.com/libjxl/libjxl/archive/refs/tags/v${CONJURE_JXL_VERSION}.tar.gz
        URL_HASH      SHA256=24d18c9af2d1f5a17730b3f8d3f70a5c16e5ac303f5d9d5fe9dc00cf7ca82af2
        SOURCE_DIR    "${FETCHCONTENT_BASE_DIR}/libjxl-${CONJURE_JXL_VERSION}-src"
        SOURCE_SUBDIR cmake_not_used      # populated only; added below once its dependencies are in its tree
    )
    FetchContent_MakeAvailable(libjxl)

    set(overridden FALSE)
    if(FETCHCONTENT_SOURCE_DIR_LIBJXL)
        set(overridden TRUE)
    else()
        # Unpacking libjxl replaces its whole tree, so the three are placed into it only afterwards.
        set(libjxlThirdParty "${libjxl_SOURCE_DIR}/third_party")
        FetchContent_Declare(libjxl_highway
            URL           https://github.com/google/highway/archive/${CONJURE_JXL_HIGHWAY_COMMIT}.tar.gz
            URL_HASH      SHA256=5124b0501c98d9930dbb065bfa1a5bbbd59ce0f12facb7e1e33aaef01a5f1f1a
            SOURCE_DIR    "${libjxlThirdParty}/highway"
            SOURCE_SUBDIR cmake_not_used      # libjxl's third_party/CMakeLists.txt adds it
        )
        FetchContent_Declare(libjxl_brotli
            URL           https://github.com/google/brotli/archive/${CONJURE_JXL_BROTLI_COMMIT}.tar.gz
            URL_HASH      SHA256=113b9685dfc14dd2f3360d077283266eec2fbdfb0fe3e5600f11be151da0d68e
            SOURCE_DIR    "${libjxlThirdParty}/brotli"
            SOURCE_SUBDIR cmake_not_used
        )
        FetchContent_Declare(libjxl_skcms
            URL           https://github.com/google/skcms/archive/${CONJURE_JXL_SKCMS_COMMIT}.tar.gz
            URL_HASH      SHA256=9bb4b5bba0b7c04f6c2bce9ff713d61e23c9a20c4945161ae16290498ad74627
            SOURCE_DIR    "${libjxlThirdParty}/skcms"
            SOURCE_SUBDIR cmake_not_used
        )
        FetchContent_MakeAvailable(libjxl_highway libjxl_brotli libjxl_skcms)
    endif()
    conjure_jxl_check_tree("${libjxl_SOURCE_DIR}" "${FETCHCONTENT_BASE_DIR}" ${overridden})

    # libjxl's scope only (the engine's own BUILD_SHARED_LIBS and testing stay as they are). Highway declares CMake
    # 3.10, before option() honoured a normal variable (CMP0077) and before mark_as_advanced() left a missing cache
    # entry alone (CMP0102): without the two defaults below its option() calls would ignore BUILD_SHARED_LIBS and
    # BUILD_TESTING here, and its mark_as_advanced(BUILD_SHARED_LIBS) would create the entry in the engine's cache.
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0102 NEW)
    set(PROVISION_DEPENDENCIES OFF)
    # The dependencies come from the pins above or nowhere: libjxl falls back to an installed Highway, Brotli or
    # lcms2 when its tree lacks one, and looks for libpng for its tools, which this build never uses.
    set(CMAKE_DISABLE_FIND_PACKAGE_HWY TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_Brotli TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_LCMS2 TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_PNG TRUE)
    # The version string of libjxl's (unbuilt) tools; without it tools/CMakeLists.txt asks git for one, and the git
    # checkout around the build tree is the engine's.
    set(JPEGXL_VERSION "${CONJURE_JXL_VERSION}")

    foreach(option IN ITEMS
            TOOLS DEVTOOLS DOXYGEN MANPAGES BENCHMARK EXAMPLES JNI SJPEG OPENEXR VIEWERS PLUGINS FUZZERS TCMALLOC
            COVERAGE)
        set(JPEGXL_ENABLE_${option} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(JPEGXL_ENABLE_SKCMS ON CACHE BOOL "" FORCE)
    set(JPEGXL_ENABLE_TRANSCODE_JPEG ON CACHE BOOL "" FORCE)
    set(JPEGXL_ENABLE_BOXES ON CACHE BOOL "" FORCE)
    foreach(option IN ITEMS STATIC BUNDLE_LIBPNG TEST_TOOLS ENABLE_LTO FORCE_NEON WARNINGS_AS_ERRORS
            FORCE_SYSTEM_BROTLI FORCE_SYSTEM_GTEST FORCE_SYSTEM_LCMS2 FORCE_SYSTEM_HWY)
        set(JPEGXL_${option} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(HWY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    set(sharedLibsCachedBefore FALSE)
    if(DEFINED CACHE{BUILD_SHARED_LIBS})
        set(sharedLibsCachedBefore TRUE)
    endif()
    add_subdirectory("${libjxl_SOURCE_DIR}" "${libjxl_BINARY_DIR}" EXCLUDE_FROM_ALL SYSTEM)
    conjure_jxl_check_build("${CONJURE_JXL_VERSION}" ${sharedLibsCachedBefore} "${libjxl_SOURCE_DIR}")

    add_library(conjure_thirdparty_jxl INTERFACE)
    add_library(conjure::thirdparty::jxl ALIAS conjure_thirdparty_jxl)
    target_link_libraries(conjure_thirdparty_jxl INTERFACE jxl)

    # Every target of libjxl's directories (Highway's and Brotli's included) in one IDE folder.
    conjure_jxl_directories(directories "${libjxl_SOURCE_DIR}")
    foreach(directory IN LISTS directories)
        get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            set_target_properties(${target} PROPERTIES FOLDER "ThirdParty/libjxl")
        endforeach()
    endforeach()
endfunction()

conjure_add_libjxl()

# Licence notices (code sweep F200): each library's own licence file from its pinned archive, libjxl's PATENTS with
# its licence, each VERSION read from the pin; THIRD_PARTY_NOTICES.md is regenerated from them
# (tools/third_party_notices.py). All four are read from libjxl's tree (FETCH libjxl), which holds Highway, Brotli and
# skcms whether the archives above were unpacked into it or a clone's submodules bring them. A FETCH of their own
# names would not be populated for a clone, which declares none of them, and a notice whose FETCH content is not
# populated is left out of the runtime notices. libjxl is configured in every tree but linked only by conjure_renderer,
# so the four are used where the renderer is built.
conjure_third_party_notice(libjxl
    NAME "libjxl" VERSION v${CONJURE_JXL_VERSION} LICENSE_ID "BSD-3-Clause" SCOPE RUNTIME
    URL "https://github.com/libjxl/libjxl"
    NOTE "The JPEG XL reference implementation: encoder, decoder and colour management. Google's patent grant \
in PATENTS covers it."
    FETCH libjxl USED ${CONJURE_WITH_RENDERER} LICENSE_FILES LICENSE PATENTS
)
conjure_third_party_notice(highway
    NAME "Highway" VERSION ${CONJURE_JXL_HIGHWAY_COMMIT} LICENSE_ID "Apache-2.0 OR BSD-3-Clause"
    SCOPE RUNTIME URL "https://github.com/google/highway"
    NOTE "Release 1.2.0, the SIMD library libjxl is compiled with."
    FETCH libjxl USED ${CONJURE_WITH_RENDERER}
    LICENSE_FILES third_party/highway/LICENSE third_party/highway/LICENSE-BSD3
)
conjure_third_party_notice(brotli
    NAME "Brotli" VERSION ${CONJURE_JXL_BROTLI_COMMIT} LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/google/brotli"
    NOTE "Release v1.2.1, compiled into libjxl, which uses it for JPEG reconstruction data and compressed \
metadata boxes."
    FETCH libjxl USED ${CONJURE_WITH_RENDERER} LICENSE_FILES third_party/brotli/LICENSE
)
conjure_third_party_notice(skcms
    NAME "skcms" VERSION ${CONJURE_JXL_SKCMS_COMMIT} LICENSE_ID "BSD-3-Clause" SCOPE RUNTIME
    URL "https://github.com/google/skcms"
    NOTE "Release 2025_09_16, the colour management system compiled into libjxl."
    FETCH libjxl USED ${CONJURE_WITH_RENDERER} LICENSE_FILES third_party/skcms/LICENSE
)
