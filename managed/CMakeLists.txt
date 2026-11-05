# Managed (C#) assemblies - populated in implementation wave 3/4.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Conjure.Managed/Conjure.Managed.csproj")
    message(STATUS "managed: projects not present yet, skipping")
    return()
endif()
