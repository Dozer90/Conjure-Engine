if(NOT TARGET conjure::core)
    return()
endif()

conjure_add_test_module(core
    SOURCES
        test_assert.cpp
        test_enum_flags.cpp
        test_error.cpp
        test_log.cpp
        test_result.cpp
        test_uuid.cpp
)
