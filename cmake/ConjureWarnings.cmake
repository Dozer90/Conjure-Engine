# Warning configuration shared by every Conjure target.
#
# Third-party code is never built with these flags: fetched dependencies are declared with
# SYSTEM so their headers are treated as external, and their own targets keep their own
# warning settings.

add_library(conjure_warnings INTERFACE)
add_library(conjure::warnings ALIAS conjure_warnings)

if(MSVC)
    target_compile_options(conjure_warnings INTERFACE
        /W4
        /permissive-
        /external:W0              # silence warnings from SYSTEM (/external:I) include directories
        /w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14545 /w14546
        /w14547 /w14549 /w14555 /w14619 /w14640 /w14826 /w14905 /w14906 /w14928
        /wd4201                   # nameless struct/union (used deliberately in math types)
    )
    if(CONJURE_WARNINGS_AS_ERRORS)
        target_compile_options(conjure_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(conjure_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(conjure_warnings INTERFACE
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
        )
    endif()
    if(CONJURE_WARNINGS_AS_ERRORS)
        target_compile_options(conjure_warnings INTERFACE -Werror)
    endif()
endif()

# Platform definitions every Conjure target needs.
add_library(conjure_platform_defines INTERFACE)
add_library(conjure::platform_defines ALIAS conjure_platform_defines)

target_compile_definitions(conjure_platform_defines INTERFACE
    $<$<CONFIG:Debug>:CONJURE_DEBUG=1>
    $<$<CONFIG:Development>:CONJURE_DEVELOPMENT=1>
    $<$<CONFIG:Distribution>:CONJURE_DISTRIBUTION=1>
    $<$<BOOL:${CONJURE_WITH_RENDERER}>:CONJURE_WITH_RENDERER=1>
    $<$<BOOL:${CONJURE_WITH_PHYSICS}>:CONJURE_WITH_PHYSICS=1>
    $<$<BOOL:${CONJURE_WITH_AUDIO}>:CONJURE_WITH_AUDIO=1>
    $<$<BOOL:${CONJURE_WITH_SCRIPTING}>:CONJURE_WITH_SCRIPTING=1>
    $<$<BOOL:${CONJURE_RHI_D3D12}>:CONJURE_RHI_D3D12=1>
    $<$<BOOL:${CONJURE_RHI_VULKAN}>:CONJURE_RHI_VULKAN=1>
)

if(WIN32)
    target_compile_definitions(conjure_platform_defines INTERFACE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()
