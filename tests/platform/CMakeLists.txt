# conjure_platform_tests: one file per public header plus the topic files docs/design/11_platform.md
# section 10 names, and test_content_scale.cpp over the private content_scale.h of the glfw backend's
# scale rules. Everything runs headless on the null backend; the glfw half of the conformance
# suite is tagged [window] and skips (exit 77) without CONJURE_TEST_WINDOW=1.

if(NOT TARGET conjure::platform)
    return()
endif()

conjure_add_test_module(platform
    SOURCES
        platform_test_support.h
        test_backend.cpp
        test_backend_conformance.cpp
        test_backend_selection.cpp
        test_button_action.cpp
        test_content_scale.cpp
        test_event.cpp
        test_gamepad.cpp
        test_input.cpp
        test_input_state.cpp
        test_key.cpp
        test_log_category.cpp
        test_monitor.cpp
        test_mouse_button.cpp
        test_native_handle.cpp
        test_null_backend.cpp
        test_platform.cpp
        test_platform_error.cpp
        test_window.cpp
        test_window_events.cpp
)

conjure_header_check(conjure_platform)
