# Build configurations.
#
#   Debug         - no optimisation, full debug info, asserts, editor, dev tooling.
#   Development   - optimised with debug info, asserts (CONJURE_VERIFY + CONJURE_ASSERT), editor,
#                   profiling. The day-to-day configuration for engine and game developers.
#   Distribution  - fully optimised, no asserts, no editor, no dev tooling. What ships to players.
#
# This file is included before project() so the configuration list is in place when the
# compiler flags are initialised.

get_property(CONJURE_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(CONJURE_IS_MULTI_CONFIG)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Development;Distribution" CACHE STRING "" FORCE)
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Development" CACHE STRING "Build type" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Development;Distribution")
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
