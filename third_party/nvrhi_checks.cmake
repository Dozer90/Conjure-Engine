# Configure check of the NVRHI pin (third_party/CMakeLists.txt). Kept apart from the file that downloads, so that the
# CTest probes of cmake/ConjureNoticesProbe.cmake (notices.pins_nvrhi_*) run it on stand-in files without fetching
# anything.

include_guard(GLOBAL)

# Fails the configure when the NVRHI CMakeLists.txt <nvrhiListFile> asks for the DirectX or Vulkan headers at a tag
# other than <directxTag> or <vulkanTag>, the tags third_party/CMakeLists.txt pins directx_headers and vulkan_headers
# at (code sweep F772). NVRHI fetches those headers itself, from git at the tags its CMakeLists.txt names; the pins
# are declared first, so FetchContent ignores NVRHI's declarations, and NVRHI would then compile against headers
# older or newer than the ones it was written for. The tags are read from the file, not from NVRHI's cache entries:
# a tree configured before NVRHI was bumped keeps the old values there, and a tree without a backend has none. The
# check stops the configure as well when the file no longer sets one of the two tags, so a change in how NVRHI fetches
# the headers is read before it is built.
#
# NVRHI's declarations fetch from git, which no hash checks, and conjure_check_third_party_pins() reads only the
# engine's own files. So the check also stops the configure when the file declares a dependency (FetchContent_Declare,
# matched without regard to case, outside whole-line comments) other than the two headers, which are pinned first,
# and rtxmu, which NVRHI fetches only with NVRHI_WITH_RTXMU, forced off by third_party/CMakeLists.txt; rtxmu with that
# option on stops it too. A new dependency of a newer NVRHI must be pinned in third_party/CMakeLists.txt before NVRHI
# is made available, and named here.
function(conjure_check_nvrhi_header_pins nvrhiListFile directxTag vulkanTag)
    if(NOT EXISTS "${nvrhiListFile}" OR IS_DIRECTORY "${nvrhiListFile}")
        message(FATAL_ERROR "third_party: '${nvrhiListFile}' is not NVRHI's CMakeLists.txt")
    endif()
    file(READ "${nvrhiListFile}" nvrhiListText)

    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" nvrhiCode "${nvrhiListText}")
    string(TOLOWER "${nvrhiCode}" nvrhiCode)
    string(REGEX MATCHALL "(^|[^a-z0-9_])fetchcontent_declare[ \t]*[(][ \t\r\n]*[a-z0-9_.+-]+" declarations
        "${nvrhiCode}")
    foreach(declaration IN LISTS declarations)
        string(REGEX REPLACE "^.*[(][ \t\r\n]*" "" dependency "${declaration}")
        if(dependency STREQUAL "rtxmu" AND NVRHI_WITH_RTXMU)
            message(FATAL_ERROR "third_party: NVRHI_WITH_RTXMU is on, so the pinned NVRHI fetches rtxmu from git "
                                "with no hash: turn it off (turning it on needs rtxmu pinned in "
                                "third_party/CMakeLists.txt before NVRHI, and this check changed; code sweep F772)")
        endif()
        if(NOT dependency MATCHES "^(directx_headers|vulkan_headers|rtxmu)$")
            message(FATAL_ERROR "third_party: the pinned NVRHI declares the dependency ${dependency}, which it would "
                                "fetch from git with no hash: pin its archive and SHA-256 in "
                                "third_party/CMakeLists.txt before NVRHI and name it in nvrhi_checks.cmake "
                                "(code sweep F772)")
        endif()
    endforeach()
    foreach(headers IN ITEMS directx vulkan)
        set(pinnedTag "${vulkanTag}")
        if(headers STREQUAL "directx")
            set(pinnedTag "${directxTag}")
        endif()
        string(TOUPPER "${headers}" headersOption)
        set(headersOption "NVRHI_${headersOption}_HEADERS_GIT_TAG")
        if(NOT nvrhiListText MATCHES "[Ss][Ee][Tt][ \t]*[(][ \t]*${headersOption}[ \t]+\"([^\"]*)\"")
            message(FATAL_ERROR "third_party: the pinned NVRHI no longer sets ${headersOption}: read how it fetches "
                                "the ${headers} headers now, which ${headers}_headers must pin (code sweep F772)")
        endif()
        if(NOT CMAKE_MATCH_1 STREQUAL "${pinnedTag}")
            message(FATAL_ERROR "third_party: the pinned NVRHI asks for the ${headers} headers at ${CMAKE_MATCH_1}, "
                                "and ${headers}_headers is pinned at ${pinnedTag}: pin the archive of the "
                                "commit ${CMAKE_MATCH_1} points to, with its SHA-256, and set ${headers}HeadersTag "
                                "(third_party/CMakeLists.txt)")
        endif()
    endforeach()
endfunction()
