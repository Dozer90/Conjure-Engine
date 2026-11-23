# FindNetHost.cmake
#
# Locates the .NET hosting headers and the static nethost library that ship with the .NET SDK
# (pack Microsoft.NETCore.App.Host.<rid>). Produces the imported target NetHost::NetHost.
#
# Variables honoured:
#   DOTNET_ROOT        (env or cache)  root of a .NET installation
#   CONJURE_DOTNET_MIN_MAJOR            minimum major version of the host pack (default 10: the .NET the
#                                       managed projects target, managed/Directory.Build.props, and the
#                                       SDK major global.json pins)
#
# Result variables:
#   NetHost_FOUND, NetHost_INCLUDE_DIR, NetHost_LIBRARY, NetHost_VERSION, NetHost_RID,
#   NetHost_DOTNET_EXECUTABLE

if(NOT CONJURE_DOTNET_MIN_MAJOR)
    set(CONJURE_DOTNET_MIN_MAJOR 10)
endif()

find_program(NetHost_DOTNET_EXECUTABLE NAMES dotnet
    HINTS "$ENV{DOTNET_ROOT}" "${DOTNET_ROOT}" "$ENV{ProgramFiles}/dotnet" "/usr/share/dotnet" "/usr/lib/dotnet" "/usr/local/share/dotnet"
)

set(_nethost_roots "")
if(DEFINED ENV{DOTNET_ROOT})
    list(APPEND _nethost_roots "$ENV{DOTNET_ROOT}")
endif()
if(DOTNET_ROOT)
    list(APPEND _nethost_roots "${DOTNET_ROOT}")
endif()
if(NetHost_DOTNET_EXECUTABLE)
    get_filename_component(_dotnet_dir "${NetHost_DOTNET_EXECUTABLE}" DIRECTORY)
    get_filename_component(_dotnet_dir "${_dotnet_dir}" REALPATH)
    list(APPEND _nethost_roots "${_dotnet_dir}")
endif()
list(APPEND _nethost_roots "$ENV{ProgramFiles}/dotnet" "/usr/share/dotnet" "/usr/lib/dotnet" "/usr/local/share/dotnet")
list(REMOVE_DUPLICATES _nethost_roots)

# Runtime identifier for the host machine.
if(WIN32)
    set(_nethost_os "win")
elseif(APPLE)
    set(_nethost_os "osx")
else()
    set(_nethost_os "linux")
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(_nethost_arch "arm64")
else()
    set(_nethost_arch "x64")
endif()
set(NetHost_RID "${_nethost_os}-${_nethost_arch}")

set(NetHost_VERSION "")
set(NetHost_INCLUDE_DIR "")
set(NetHost_LIBRARY "")

foreach(root IN LISTS _nethost_roots)
    set(_pack_dir "${root}/packs/Microsoft.NETCore.App.Host.${NetHost_RID}")
    if(NOT IS_DIRECTORY "${_pack_dir}")
        continue()
    endif()
    file(GLOB _versions RELATIVE "${_pack_dir}" "${_pack_dir}/*")
    foreach(version IN LISTS _versions)
        if(NOT version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            continue()
        endif()
        set(_candidate_major "${CMAKE_MATCH_1}")
        if(_candidate_major LESS CONJURE_DOTNET_MIN_MAJOR)
            continue()
        endif()
        set(_native "${_pack_dir}/${version}/runtimes/${NetHost_RID}/native")
        if(NOT EXISTS "${_native}/nethost.h")
            continue()
        endif()
        if(WIN32)
            set(_lib "${_native}/libnethost.lib")
        elseif(APPLE)
            set(_lib "${_native}/libnethost.a")
        else()
            set(_lib "${_native}/libnethost.a")
        endif()
        if(NOT EXISTS "${_lib}")
            continue()
        endif()
        # Prefer the newest version that satisfies the minimum, but stay on the lowest major
        # that satisfies it so the runtime we target matches what Conjure.Managed is built for.
        if(NOT NetHost_VERSION)
            set(NetHost_VERSION "${version}")
            set(NetHost_INCLUDE_DIR "${_native}")
            set(NetHost_LIBRARY "${_lib}")
        else()
            string(REGEX MATCH "^[0-9]+" _found_major "${NetHost_VERSION}")
            if(_candidate_major EQUAL _found_major AND version VERSION_GREATER NetHost_VERSION)
                set(NetHost_VERSION "${version}")
                set(NetHost_INCLUDE_DIR "${_native}")
                set(NetHost_LIBRARY "${_lib}")
            elseif(_candidate_major LESS _found_major)
                set(NetHost_VERSION "${version}")
                set(NetHost_INCLUDE_DIR "${_native}")
                set(NetHost_LIBRARY "${_lib}")
            endif()
        endif()
    endforeach()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NetHost
    REQUIRED_VARS NetHost_INCLUDE_DIR NetHost_LIBRARY
    VERSION_VAR NetHost_VERSION
    FAIL_MESSAGE "Could not find the .NET host pack (nethost.h / libnethost). Install the .NET ${CONJURE_DOTNET_MIN_MAJOR}+ SDK or set DOTNET_ROOT, or configure with -DCONJURE_WITH_SCRIPTING=OFF."
)

if(NetHost_FOUND AND NOT TARGET NetHost::NetHost)
    add_library(NetHost::NetHost STATIC IMPORTED)
    set_target_properties(NetHost::NetHost PROPERTIES
        IMPORTED_LOCATION "${NetHost_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${NetHost_INCLUDE_DIR}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${NetHost_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS "NETHOST_USE_AS_STATIC"
    )
    if(UNIX AND NOT APPLE)
        set_property(TARGET NetHost::NetHost APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${CMAKE_DL_LIBS} pthread)
    endif()
    message(STATUS "Found .NET host pack ${NetHost_VERSION} (${NetHost_RID}) at ${NetHost_INCLUDE_DIR}")
endif()

mark_as_advanced(NetHost_DOTNET_EXECUTABLE)
