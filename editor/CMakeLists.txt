# conjure_editor - populated in implementation wave 4.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    message(STATUS "editor: sources not present yet, skipping")
    return()
endif()
