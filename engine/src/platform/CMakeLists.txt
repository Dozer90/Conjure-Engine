# conjure_platform - window, event stream and input snapshot (docs/design/11_platform.md).
#
# The null backend is always built; the glfw backend (GLFW 3.4, a private dependency) only when
# CONJURE_PLATFORM_GLFW is ON. CONJURE_PLATFORM_HAS_GLFW tells platform.cpp which backends exist.

set(CONJURE_PLATFORM_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/engine/include/conjure/platform")

set(CONJURE_PLATFORM_PUBLIC_HEADERS
    "${CONJURE_PLATFORM_INCLUDE_DIR}/backend.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/button_action.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/event.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/gamepad.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/input.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/input_state.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/key.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/log_category.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/monitor.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/mouse_button.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/native_handle.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/null_backend.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/platform.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/platform_error.h"
    "${CONJURE_PLATFORM_INCLUDE_DIR}/window.h"
)

set(CONJURE_PLATFORM_SOURCES
    enum_names.cpp
    event.cpp
    input.cpp
    input_state.cpp
    null_backend.cpp
    platform.cpp
    platform_error.cpp
)

set(CONJURE_PLATFORM_PRIVATE_DEPS "")
set(CONJURE_PLATFORM_HAS_GLFW 0)
if(CONJURE_PLATFORM_GLFW)
    list(APPEND CONJURE_PLATFORM_SOURCES
        glfw/glfw_context.cpp
        glfw/glfw_context.h
        glfw/glfw_native.cpp
        glfw/glfw_native.h
        glfw/glfw_vulkan.cpp
        glfw/glfw_window.cpp
        glfw/glfw_window.h
    )
    set(CONJURE_PLATFORM_PRIVATE_DEPS conjure::thirdparty::glfw)
    set(CONJURE_PLATFORM_HAS_GLFW 1)
endif()

conjure_add_module(platform
    PUBLIC_HEADERS
        ${CONJURE_PLATFORM_PUBLIC_HEADERS}
    SOURCES
        ${CONJURE_PLATFORM_SOURCES}
    PUBLIC_DEPS
        conjure::core
    PRIVATE_DEPS
        ${CONJURE_PLATFORM_PRIVATE_DEPS}
    PRIVATE_DEFINES
        CONJURE_PLATFORM_HAS_GLFW=${CONJURE_PLATFORM_HAS_GLFW}
)
