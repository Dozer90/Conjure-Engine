# Third-party dependencies.
#
# Every dependency is declared here exactly once, pinned to a tag or commit, and exposed to the
# engine through a conjure::thirdparty::<name> target. Engine code never references the raw
# upstream targets so that a dependency can be swapped or vendored in one place.
#
# Dependencies are declared SYSTEM so their headers do not trigger our warnings, and
# EXCLUDE_FROM_ALL so only the parts the engine links are built.

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Several pinned dependencies still declare cmake_minimum_required < 3.5, which CMake 4 rejects
# outright. This directory-scoped setting keeps them configurable (ignored by CMake < 3.31).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Remember the caller's settings so upstream projects cannot leak changes back to us.
set(CONJURE_SAVED_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)

#------------------------------------------------------------------------------
# Header-only / single-file libraries with no upstream CMake support
#------------------------------------------------------------------------------
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        d14d46f6b44456eb88beac58cba833a4135d4c89
    GIT_SHALLOW    FALSE
    SYSTEM
)
FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
    SYSTEM
)
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.22
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake_not_used     # we compile the single file ourselves; skip upstream CMake
    SYSTEM
)
FetchContent_MakeAvailable(stb cgltf)

add_library(conjure_thirdparty_stb STATIC src/stb_impl.c)
add_library(conjure::thirdparty::stb ALIAS conjure_thirdparty_stb)
target_include_directories(conjure_thirdparty_stb SYSTEM PUBLIC "${stb_SOURCE_DIR}")
set_target_properties(conjure_thirdparty_stb PROPERTIES FOLDER "ThirdParty")

add_library(conjure_thirdparty_cgltf STATIC src/cgltf_impl.c)
add_library(conjure::thirdparty::cgltf ALIAS conjure_thirdparty_cgltf)
target_include_directories(conjure_thirdparty_cgltf SYSTEM PUBLIC "${cgltf_SOURCE_DIR}")
set_target_properties(conjure_thirdparty_cgltf PROPERTIES FOLDER "ThirdParty")

if(CONJURE_WITH_AUDIO)
    FetchContent_MakeAvailable(miniaudio)
    add_library(conjure_thirdparty_miniaudio STATIC src/miniaudio_impl.c)
    add_library(conjure::thirdparty::miniaudio ALIAS conjure_thirdparty_miniaudio)
    target_include_directories(conjure_thirdparty_miniaudio SYSTEM PUBLIC "${miniaudio_SOURCE_DIR}")
    set_target_properties(conjure_thirdparty_miniaudio PROPERTIES FOLDER "ThirdParty")
    if(UNIX AND NOT APPLE)
        target_link_libraries(conjure_thirdparty_miniaudio PUBLIC ${CMAKE_DL_LIBS} pthread m)
    endif()
endif()

#------------------------------------------------------------------------------
# glm
#------------------------------------------------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

add_library(conjure_thirdparty_glm INTERFACE)
add_library(conjure::thirdparty::glm ALIAS conjure_thirdparty_glm)
target_link_libraries(conjure_thirdparty_glm INTERFACE glm::glm)
target_compile_definitions(conjure_thirdparty_glm INTERFACE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE     # right-handed, zero-to-one depth (D3D12 and Vulkan)
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_SILENT_WARNINGS
    GLM_FORCE_EXPLICIT_CTOR
)

#------------------------------------------------------------------------------
# yyjson
#------------------------------------------------------------------------------
FetchContent_Declare(yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG        0.10.0
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)
set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YYJSON_BUILD_FUZZER OFF CACHE BOOL "" FORCE)
set(YYJSON_BUILD_MISC OFF CACHE BOOL "" FORCE)
set(YYJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(YYJSON_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yyjson)
add_library(conjure::thirdparty::yyjson ALIAS yyjson)
set_target_properties(yyjson PROPERTIES FOLDER "ThirdParty")

#------------------------------------------------------------------------------
# doctest
#------------------------------------------------------------------------------
if(CONJURE_BUILD_TESTS OR CONJURE_BUILD_BENCHMARKS)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)
    add_library(conjure_thirdparty_doctest INTERFACE)
    add_library(conjure::thirdparty::doctest ALIAS conjure_thirdparty_doctest)
    target_link_libraries(conjure_thirdparty_doctest INTERFACE doctest::doctest)

    # doctest_discover_tests() (one CTest entry per TEST_CASE, used by conjure_add_test_module).
    # The function is global but the script path it records is a directory-scoped variable, so it is
    # re-published through the cache for the tests/ directories.
    include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
    set(_DOCTEST_DISCOVER_TESTS_SCRIPT "${_DOCTEST_DISCOVER_TESTS_SCRIPT}" CACHE INTERNAL "doctestAddTests.cmake path")
endif()

#------------------------------------------------------------------------------
# GLFW (needs X11/Wayland development packages on Linux, so only when the glfw window backend
# is wanted; CONJURE_PLATFORM_GLFW=OFF yields a null-only platform library for servers)
#------------------------------------------------------------------------------
if(CONJURE_PLATFORM_GLFW)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)
if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(glfw)
add_library(conjure::thirdparty::glfw ALIAS glfw)
set_target_properties(glfw PROPERTIES FOLDER "ThirdParty")
endif()

#------------------------------------------------------------------------------
# NVRHI (graphics API abstraction)
#------------------------------------------------------------------------------
if(CONJURE_WITH_RENDERER)
    FetchContent_Declare(nvrhi
        GIT_REPOSITORY https://github.com/NVIDIA-RTX/NVRHI.git
        GIT_TAG        bf0bbd9b666a7297492beb1999a4ddc0dfded143
        GIT_SHALLOW    FALSE
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    set(NVRHI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(NVRHI_INSTALL OFF CACHE BOOL "" FORCE)
    set(NVRHI_WITH_DX11 OFF CACHE BOOL "" FORCE)
    set(NVRHI_WITH_DX12 ${CONJURE_RHI_D3D12} CACHE BOOL "" FORCE)
    set(NVRHI_WITH_VULKAN ${CONJURE_RHI_VULKAN} CACHE BOOL "" FORCE)
    set(NVRHI_WITH_SHADER_COMPILER OFF CACHE BOOL "" FORCE)
    set(NVRHI_WITH_NVAPI OFF CACHE BOOL "" FORCE)
    set(NVRHI_WITH_RTXMU OFF CACHE BOOL "" FORCE)
    set(NVRHI_WITH_AFTERMATH OFF CACHE BOOL "" FORCE)
    set(NVRHI_WITH_VALIDATION ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nvrhi)

    add_library(conjure_thirdparty_nvrhi INTERFACE)
    add_library(conjure::thirdparty::nvrhi ALIAS conjure_thirdparty_nvrhi)
    target_link_libraries(conjure_thirdparty_nvrhi INTERFACE nvrhi)
    if(CONJURE_RHI_D3D12)
        target_link_libraries(conjure_thirdparty_nvrhi INTERFACE nvrhi_d3d12 d3d12 dxgi dxguid)
    endif()
    if(CONJURE_RHI_VULKAN)
        target_link_libraries(conjure_thirdparty_nvrhi INTERFACE nvrhi_vk)
    endif()
    foreach(nvrhiTarget IN ITEMS nvrhi nvrhi_d3d12 nvrhi_vk)
        if(TARGET ${nvrhiTarget})
            set_target_properties(${nvrhiTarget} PROPERTIES FOLDER "ThirdParty")
        endif()
    endforeach()
endif()

#------------------------------------------------------------------------------
# glslang (HLSL -> SPIR-V for the Vulkan backend)
#------------------------------------------------------------------------------
if(CONJURE_WITH_RENDERER AND CONJURE_RHI_VULKAN)
    FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG        15.1.0
        GIT_SHALLOW    TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
    set(ENABLE_SPIRV ON CACHE BOOL "" FORCE)
    set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "" FORCE)
    set(ENABLE_GLSLANG_JS OFF CACHE BOOL "" FORCE)
    set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glslang)
    set(CONJURE_GLSLANG_EXECUTABLE "$<TARGET_FILE:glslang-standalone>" CACHE INTERNAL "")
endif()

#------------------------------------------------------------------------------
# Jolt Physics
#------------------------------------------------------------------------------
if(CONJURE_WITH_PHYSICS)
    FetchContent_Declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG        v5.3.0
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  Build
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
    set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "" FORCE)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON CACHE BOOL "" FORCE)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
    set(ENABLE_OBJECT_STREAM ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(JoltPhysics)
    add_library(conjure::thirdparty::jolt ALIAS Jolt)
    set_target_properties(Jolt PROPERTIES FOLDER "ThirdParty")
endif()

#------------------------------------------------------------------------------
# Dear ImGui (docking) + ImGuizmo - editor only
#------------------------------------------------------------------------------
if(CONJURE_BUILD_EDITOR)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.9b-docking
        GIT_SHALLOW    TRUE
        SYSTEM
    )
    FetchContent_Declare(imguizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG        105e4fde45e4e74aae8973d98750820ef7aca7e2
        GIT_SHALLOW    FALSE
        SOURCE_SUBDIR  cmake_not_used     # only ImGuizmo.cpp is wanted; skip upstream CMake
        SYSTEM
    )
    FetchContent_MakeAvailable(imgui imguizmo)

    add_library(conjure_thirdparty_imgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp"
    )
    add_library(conjure::thirdparty::imgui ALIAS conjure_thirdparty_imgui)
    target_include_directories(conjure_thirdparty_imgui SYSTEM PUBLIC
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
        "${imgui_SOURCE_DIR}/misc/cpp"
        "${imguizmo_SOURCE_DIR}/src"
    )
    target_compile_definitions(conjure_thirdparty_imgui PUBLIC
        IMGUI_DEFINE_MATH_OPERATORS
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        IMGUI_DISABLE_OBSOLETE_KEYIO
    )
    target_link_libraries(conjure_thirdparty_imgui PUBLIC glfw)
    set_target_properties(conjure_thirdparty_imgui PROPERTIES FOLDER "ThirdParty")
endif()

#------------------------------------------------------------------------------
# .NET hosting (nethost / hostfxr) - scripting only
#------------------------------------------------------------------------------
if(CONJURE_WITH_SCRIPTING)
    find_package(NetHost REQUIRED)
    add_library(conjure_thirdparty_nethost INTERFACE)
    add_library(conjure::thirdparty::nethost ALIAS conjure_thirdparty_nethost)
    target_link_libraries(conjure_thirdparty_nethost INTERFACE NetHost::NetHost)
endif()

set(BUILD_SHARED_LIBS "${CONJURE_SAVED_BUILD_SHARED_LIBS}")
