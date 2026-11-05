# Sample projects - populated in implementation wave 5.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/feature_test/CMakeLists.txt")
    message(STATUS "samples: not present yet, skipping")
    return()
endif()
add_subdirectory(feature_test)
