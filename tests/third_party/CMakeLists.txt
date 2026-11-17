# Third-party pins (third_party/): CTest probes of the configure checks that keep a pin in effect. Tests never include
# third-party headers (CONVENTIONS.md 6), so a pin is checked when the engine is configured, and these entries prove
# that each check stops the configure on what it guards against. Each probe configures a throwaway project without a
# language, afresh unless it repeats an earlier probe's configure, labelled "build;third_party". None downloads.
#
#   third_party.jxl_*   third_party/jxl_checks.cmake and jxl.cmake, through jxl_checks_probe.cmake: the pinned
#                       version, static libraries, the allowed link closure (imported targets of libjxl's directories
#                       and aliases of them included), no compile options passed on, no BUILD_SHARED_LIBS cache entry
#                       written (on a repeated configure too), libjxl's tree present and holding Highway, Brotli and
#                       skcms, a libjxl clone named by FETCHCONTENT_SOURCE_DIR_LIBJXL built as it stands, and the
#                       licence notices of all four, resolved by cmake/ConjureNotices.cmake, used with the archives
#                       unpacked and with a clone, and unused in a tree without the renderer

set(jxlProbeRoot "${CMAKE_CURRENT_BINARY_DIR}/jxl_checks_probe")
file(WRITE "${jxlProbeRoot}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.28)
project(conjure_jxl_checks_probe NONE)
include("${CONJURE_JXL_PROBE}")
]=])

# Registers third_party.<name>: probe case <case> passes when its configure prints <pattern>. CMake breaks the text of a
# FATAL_ERROR at spaces (and a path in it may hold one), so a pattern joins its words with ".*". With AFTER <first>,
# the entry configures again, without --fresh, the build tree the entry third_party.<first> of the same case left,
# and runs after it.
function(conjure_add_jxl_probe name case pattern)
    cmake_parse_arguments(PARSE_ARGV 3 probe "" "AFTER" "")
    set(fresh --fresh)
    if(probe_AFTER)
        set(fresh "")
    endif()
    add_test(NAME third_party.${name}
        COMMAND "${CMAKE_COMMAND}" ${fresh} -S "${jxlProbeRoot}/source" -B "${jxlProbeRoot}/build/${case}"
            -G "${CMAKE_GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCONJURE_JXL_PROBE=${CMAKE_CURRENT_SOURCE_DIR}/jxl_checks_probe.cmake"
            "-DCONJURE_JXL_THIRD_PARTY=${PROJECT_SOURCE_DIR}/third_party"
            "-DCONJURE_NOTICES_MODULE=${PROJECT_SOURCE_DIR}/cmake/ConjureNotices.cmake"
            "-DPROBE_CASE=${case}"
    )
    set_tests_properties(third_party.${name} PROPERTIES
        LABELS "build;third_party"
        TIMEOUT 60
        PASS_REGULAR_EXPRESSION "${pattern}"
    )
    if(probe_AFTER)
        set_tests_properties(third_party.${probe_AFTER} PROPERTIES FIXTURES_SETUP jxl_probe_${case})
        set_tests_properties(third_party.${name} PROPERTIES FIXTURES_REQUIRED jxl_probe_${case})
    endif()
endfunction()

conjure_add_jxl_probe(jxl_check_accepts_the_pinned_build pinned "jxl probe: pinned passed")
conjure_add_jxl_probe(jxl_version_mismatch_fails_configure version_mismatch
    "jxl.*declares.*version.*'0[.]12[.]0',.*but.*pins.*0[.]13[.]0"
)
conjure_add_jxl_probe(jxl_shared_library_fails_configure shared_library "jxl_cms.*is.*a.*SHARED_LIBRARY")
conjure_add_jxl_probe(jxl_missing_library_fails_configure missing_library "libjxl.*defines.*no.*target.*jxl_cms")
conjure_add_jxl_probe(jxl_unpinned_dependency_fails_configure unpinned_dependency
    "libjxl.*links.*lcms2,.*which.*third_party/jxl[.]cmake.*does.*not.*pin"
)
conjure_add_jxl_probe(jxl_hidden_imported_dependency_fails_configure hidden_dependency
    "libjxl.*links.*PkgConfig::TCMallocMinimal,.*which.*third_party/jxl[.]cmake.*does.*not.*pin:.*imported.*target"
)
conjure_add_jxl_probe(jxl_hidden_alias_fails_configure hidden_alias
    "libjxl.*links.*tcmalloc::minimal,.*which.*third_party/jxl[.]cmake.*does.*not.*pin:.*name.*with.*'::'"
)
conjure_add_jxl_probe(jxl_compile_options_fail_configure compile_options
    "libjxl's.*hwy.*passes.*the.*compile.*options.*'/arch:AVX2'"
)
conjure_add_jxl_probe(jxl_shared_libs_cache_entry_fails_configure cache_entry
    "configuring.*libjxl.*created.*the.*cache.*entry.*BUILD_SHARED_LIBS"
)
conjure_add_jxl_probe(jxl_shared_libs_cache_entry_fails_configure_again cache_entry
    "configuring.*libjxl.*created.*the.*cache.*entry.*BUILD_SHARED_LIBS"
    AFTER jxl_shared_libs_cache_entry_fails_configure
)
conjure_add_jxl_probe(jxl_incomplete_tree_fails_configure incomplete_tree
    "libjxl's.*tree.*lacks.*skcms/skcms[.]h.*deleted.*by.*hand.*unpacked.*again.*Delete.*libjxl[*]-subbuild/.*download"
)
conjure_add_jxl_probe(jxl_missing_tree_fails_configure missing_tree
    "libjxl's.*tree.*is.*missing:.*deleted.*by.*hand.*FULLY_DISCONNECTED.*Delete.*libjxl[*]-subbuild/.*download"
)
conjure_add_jxl_probe(jxl_unpacked_tree_ships_the_notices unpacked_tree "jxl probe: unpacked_tree passed")
conjure_add_jxl_probe(jxl_source_override_keeps_the_clone source_override "jxl probe: source_override passed")
conjure_add_jxl_probe(jxl_notices_unused_without_renderer without_renderer "jxl probe: without_renderer passed")
conjure_add_jxl_probe(jxl_nested_source_override_fails_configure nested_override
    "FETCHCONTENT_SOURCE_DIR_LIBJXL_HIGHWAY.*is.*not.*supported"
)
conjure_add_jxl_probe(jxl_clone_without_submodules_fails_configure clone_without_submodules
    "the.*libjxl.*clone.*lacks.*third_party/skcms/skcms[.]h.*git.*submodule.*update.*--init"
)
