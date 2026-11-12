# Build configurations.
#
#   Debug         - no optimisation, full debug info, asserts, editor, dev tooling.
#   Development   - optimised with debug info, asserts (CONJURE_VERIFY + CONJURE_ASSERT), editor,
#                   profiling. The day-to-day configuration for engine and game developers.
#   Distribution  - fully optimised, no asserts, no editor, no dev tooling. What ships to players.
#
# This file is included before project() so the configuration list is in place when the
# compiler flags are initialised.
#
# The editor is never part of a Distribution build (docs/design/04_editor.md 8). A multi-config generator
# builds every configuration from one set of targets, so such a tree offers Distribution only when
# CONJURE_BUILD_EDITOR is OFF; the default (editor on) gives Debug and Development, and the single-config
# *-dist presets remain the way to build what ships. CONJURE_BUILD_EDITOR is read from the cache here because
# the option() itself is declared after project(); an entry that is not in the cache yet means the default ON.

get_property(CONJURE_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(CONJURE_IS_MULTI_CONFIG)
    if(DEFINED CONJURE_BUILD_EDITOR AND NOT CONJURE_BUILD_EDITOR)
        set(CMAKE_CONFIGURATION_TYPES "Debug;Development;Distribution" CACHE STRING "" FORCE)
    else()
        set(CMAKE_CONFIGURATION_TYPES "Debug;Development" CACHE STRING "" FORCE)
    endif()
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Development" CACHE STRING "Build type" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Development;Distribution")
endif()

# MSVC debug information (CMP0141, NEW under cmake_minimum_required 3.28): /Zi no longer sits in the
# CMAKE_<LANG>_FLAGS_<CONFIG> defaults but comes from this variable, whose built-in default names only Debug and
# RelWithDebInfo, and Development derives its flags from RelWithDebInfo's (conjure_derive_config_flags), so both
# configurations are named here. Distribution produces no PDB. Set before project(), the value initialises
# MSVC_DEBUG_INFORMATION_FORMAT on every target, fetched ones included; a value given on the command line wins.
if(NOT DEFINED CMAKE_MSVC_DEBUG_INFORMATION_FORMAT)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,Development>:ProgramDatabase>")
endif()

# Derive the custom configurations from the standard ones once the compiler is known.
# CMAKE_<LANG>_FLAGS_<CONFIG> are initialised by project(), so the root CMakeLists.txt
# calls conjure_derive_config_flags() immediately after project().
function(conjure_derive_config_flags)
    foreach(lang IN ITEMS C CXX)
        if(NOT CMAKE_${lang}_FLAGS_DEVELOPMENT)
            set(CMAKE_${lang}_FLAGS_DEVELOPMENT "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO}"
                CACHE STRING "Flags used by the ${lang} compiler during Development builds." FORCE)
        endif()
        if(NOT CMAKE_${lang}_FLAGS_DISTRIBUTION)
            set(CMAKE_${lang}_FLAGS_DISTRIBUTION "${CMAKE_${lang}_FLAGS_RELEASE}"
                CACHE STRING "Flags used by the ${lang} compiler during Distribution builds." FORCE)
        endif()
    endforeach()

    foreach(kind IN ITEMS EXE SHARED MODULE STATIC)
        if(NOT CMAKE_${kind}_LINKER_FLAGS_DEVELOPMENT)
            set(CMAKE_${kind}_LINKER_FLAGS_DEVELOPMENT "${CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO}"
                CACHE STRING "Flags used by the linker during Development builds." FORCE)
        endif()
        if(NOT CMAKE_${kind}_LINKER_FLAGS_DISTRIBUTION)
            set(CMAKE_${kind}_LINKER_FLAGS_DISTRIBUTION "${CMAKE_${kind}_LINKER_FLAGS_RELEASE}"
                CACHE STRING "Flags used by the linker during Distribution builds." FORCE)
        endif()
    endforeach()

    # Imported targets that only know the standard configurations map onto ours.
    set(CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT "RelWithDebInfo;Release;" CACHE STRING "" FORCE)
    set(CMAKE_MAP_IMPORTED_CONFIG_DISTRIBUTION "Release;RelWithDebInfo;" CACHE STRING "" FORCE)
endfunction()
