# Third-party dependencies.
#
# Every dependency is declared here, or in a file this one includes (jxl.cmake), exactly once, pinned to a
# tag or commit or to an archive URL with its SHA-256, and exposed to the
# engine through a conjure::thirdparty::<name> target. Engine code never references the raw
# upstream targets so that a dependency can be swapped or vendored in one place.
#
# Dependencies are declared SYSTEM so their headers do not trigger our warnings, and
# EXCLUDE_FROM_ALL so only the parts the engine links are built.
#
# Every dependency also has a conjure_third_party_notice() beside its declaration (cmake/ConjureNotices.cmake,
# docs/design/06_engine_loop.md D15): its name, pin, licence and licence files. The notices are registered
# outside the option blocks, so every tree knows the whole list and a tree that does not fetch a dependency
# says so. VERSION repeats the GIT_TAG and the configure fails when the two differ; a dependency pinned by URL and
# URL_HASH (jxl.cmake) has no GIT_TAG to compare, so its notices read the variables its URLs read. After a pin or a
# notice changed, regenerate the tracked THIRD_PARTY_NOTICES.md: python tools/third_party_notices.py --build-dir
# <tree> (docs/handbook/getting-started.md 11).

include(FetchContent)
include(ConjureNotices)
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
set(cgltfLinked FALSE)
if(CONJURE_WITH_RENDERER OR CONJURE_WITH_PHYSICS)
    set(cgltfLinked TRUE)
endif()
conjure_third_party_notice(stb
    NAME "stb" VERSION d14d46f6b44456eb88beac58cba833a4135d4c89 LICENSE_ID "MIT OR Unlicense" SCOPE RUNTIME
    URL "https://github.com/nothings/stb"
    NOTE "stb_image and stb_image_write."
    FETCH stb LICENSE_FILES LICENSE
)
# cgltf.h carries jsmn, whose licence asks for its own copyright line in every copy: licenses/jsmn.txt is the
# text of the comment in cgltf.h. The path is absolute because FETCH reads relative ones in the cgltf sources.
conjure_third_party_notice(cgltf
    NAME "cgltf" VERSION v1.14 LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/jkuhlmann/cgltf"
    NOTE "cgltf.h embeds the jsmn JSON tokenizer (https://github.com/zserge/jsmn), MIT, Copyright (c) 2010 Serge \
A. Zaitsev."
    FETCH cgltf USED ${cgltfLinked} LICENSE_FILES LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/licenses/jsmn.txt"
)
conjure_third_party_notice(miniaudio
    NAME "miniaudio" VERSION 0.11.22 LICENSE_ID "Unlicense OR MIT-0" SCOPE RUNTIME
    URL "https://github.com/mackron/miniaudio"
    NOTE "Compiled together with the stb_vorbis decoder of its extras folder, which is under the licence of stb."
    FETCH miniaudio LICENSE_FILES LICENSE
)
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
conjure_third_party_notice(glm
    NAME "GLM" VERSION 1.0.1 LICENSE_ID "MIT OR Happy Bunny License" SCOPE RUNTIME
    URL "https://github.com/g-truc/glm"
    FETCH glm LICENSE_FILES copying.txt
)
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
conjure_third_party_notice(yyjson
    NAME "yyjson" VERSION 0.10.0 LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/ibireme/yyjson"
    FETCH yyjson LICENSE_FILES LICENSE
)
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
# JPEG XL: libjxl with Highway, Brotli and skcms, pinned by URL and SHA-256 in jxl.cmake, which registers their
# four notices too (the declaration scan of conjure_write_third_party_notices() reads this file only)
#------------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/jxl.cmake")

#------------------------------------------------------------------------------
# doctest
#------------------------------------------------------------------------------
conjure_third_party_notice(doctest
    NAME "doctest" VERSION v2.4.11 LICENSE_ID "MIT" SCOPE BUILD
    URL "https://github.com/doctest/doctest"
    FETCH doctest LICENSE_FILES LICENSE.txt
)
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
conjure_third_party_notice(glfw
    NAME "GLFW" VERSION 3.4 LICENSE_ID "Zlib" SCOPE RUNTIME
    URL "https://github.com/glfw/glfw"
    FETCH glfw LICENSE_FILES LICENSE.md
)
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
conjure_third_party_notice(nvrhi
    NAME "NVRHI" VERSION bf0bbd9b666a7297492beb1999a4ddc0dfded143 LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/NVIDIA-RTX/NVRHI"
    FETCH nvrhi LICENSE_FILES LICENSE.txt
)
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

# NVRHI fetches the API headers of the backends it builds, at the tags its own cache variables name: a tree
# without a backend knows neither the headers nor their pin. Registered after NVRHI for that reason.
conjure_third_party_notice(directx_headers
    NAME "DirectX-Headers" VERSION "${NVRHI_DIRECTX_HEADERS_GIT_TAG}" LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/microsoft/DirectX-Headers"
    NOTE "The Direct3D 12 headers the D3D12 backend of NVRHI is compiled against."
    FETCH directx_headers LICENSE_FILES LICENSE
)
conjure_third_party_notice(vulkan_headers
    NAME "Vulkan-Headers" VERSION "${NVRHI_VULKAN_HEADERS_GIT_TAG}" LICENSE_ID "Apache-2.0 OR MIT" SCOPE RUNTIME
    URL "https://github.com/KhronosGroup/Vulkan-Headers"
    NOTE "The Vulkan headers the Vulkan backend of NVRHI is compiled against."
    FETCH vulkan_headers LICENSE_FILES LICENSE.md LICENSES/Apache-2.0.txt LICENSES/MIT.txt
)

# Shader code that follows published implementations (engine/shaders/shared/tonemap.hlsli). Nothing is fetched:
# the licence texts are kept in licenses/, each the MIT text under the copyright line its source carries.
set(shadersBuilt FALSE)
if(CONJURE_WITH_RENDERER)
    set(shadersBuilt TRUE)
endif()
conjure_third_party_notice(aces_fit
    NAME "ACES filmic fit (Baking Lab)" LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/TheRealMJP/BakingLab"
    NOTE "tonemap_aces of the engine's tonemap shader follows ACES.hlsl of Baking Lab by MJP and David Neubelt, a \
fit of the ACES reference transforms written by Stephen Hill."
    USED ${shadersBuilt} LICENSE_FILES licenses/bakinglab_aces.txt
)
conjure_third_party_notice(agx_minimal
    NAME "Minimal AgX" LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://iolite-engine.com/blog_posts/minimal_agx_implementation"
    NOTE "tonemap_agx of the engine's tonemap shader follows Benjamin Wrensch's minimal implementation of Troy \
Sobotka's AgX (https://github.com/sobotka/AgX), where its constants come from."
    USED ${shadersBuilt} LICENSE_FILES licenses/minimal_agx.txt
)

#------------------------------------------------------------------------------
# glslang (HLSL -> SPIR-V for the Vulkan backend)
#------------------------------------------------------------------------------
conjure_third_party_notice(glslang
    NAME "glslang" VERSION 15.1.0 LICENSE_ID "BSD-3-Clause and others (see the text)" SCOPE BUILD
    URL "https://github.com/KhronosGroup/glslang"
    NOTE "The compiler that turns the engine's HLSL into SPIR-V while the engine is built. Only its output ships."
    FETCH glslang LICENSE_FILES LICENSE.txt
)
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
conjure_third_party_notice(jolt
    NAME "Jolt Physics" VERSION v5.3.0 LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/jrouwe/JoltPhysics"
    FETCH JoltPhysics LICENSE_FILES LICENSE
)
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
    # docs/design/14_physics.md P16, ARCHITECTURE.md 9: cross-platform bit equality is opt-in; the engine's
    # own physics objects get the matching floating-point options in engine/src/physics/CMakeLists.txt.
    set(CROSS_PLATFORM_DETERMINISTIC ${CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC} CACHE BOOL "" FORCE)
    set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "" FORCE)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON CACHE BOOL "" FORCE)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
    set(ENABLE_OBJECT_STREAM ON CACHE BOOL "" FORCE)
    set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
    set(OBJECT_LAYER_BITS 16 CACHE STRING "" FORCE)
    # Asserts come from the per-configuration defines below, never from Jolt's global switch (which would
    # put them into Distribution too). Jolt's floating-point exception traps (MSVC only, Debug and Release
    # only, unmasked on the calling thread inside PhysicsSystem::Update) would make the engine's own
    # callbacks inside the step trap on one platform and configuration and nowhere else; the engine's
    # NaN handling is the same everywhere, so they stay off.
    set(USE_ASSERTS OFF CACHE BOOL "" FORCE)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
    # Instruction set (docs/design/14_physics.md 8): baseline x64 is SSE4.1/4.2 and nothing newer, so a
    # Distribution binary runs on every x64 CPU. Jolt's defaults enable AVX2, F16C, LZCNT, TZCNT and FMA
    # and propagate the matching compiler flags and JPH_USE_* defines PUBLIC, which would make every
    # physics object (hence conjure_shared and every executable) require an AVX2+FMA CPU. Keep this block
    # when the tag is bumped; the check after FetchContent_MakeAvailable fails the configure if an option
    # name changed upstream and the pin stopped taking effect.
    set(USE_SSE4_1 ON CACHE BOOL "" FORCE)
    set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
    set(USE_AVX OFF CACHE BOOL "" FORCE)
    set(USE_AVX2 OFF CACHE BOOL "" FORCE)
    set(USE_AVX512 OFF CACHE BOOL "" FORCE)
    set(USE_LZCNT OFF CACHE BOOL "" FORCE)
    set(USE_TZCNT OFF CACHE BOOL "" FORCE)
    set(USE_F16C OFF CACHE BOOL "" FORCE)
    set(USE_FMADD OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(JoltPhysics)
    add_library(conjure::thirdparty::jolt ALIAS Jolt)
    set_target_properties(Jolt PROPERTIES FOLDER "ThirdParty")
    # Jolt/Jolt.cmake emits the JPH_USE_* defines on MSVC only under the Visual Studio generators
    # (CMAKE_VS_PLATFORM_NAME is x86/x64); with Ninja and cl nothing is emitted and Jolt/Core/Core.h falls
    # back to the compiler's predefined macros, which without /arch means SSE2. cl compiles the SSE4
    # intrinsics without a flag, so the baseline is reached by naming the defines here (PUBLIC, as Jolt
    # does, so the library and the engine's physics objects agree).
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|X86|x86)$"
       AND NOT CMAKE_VS_PLATFORM_NAME MATCHES "^(x86|x64)$")
        target_compile_definitions(Jolt PUBLIC JPH_USE_SSE4_1 JPH_USE_SSE4_2)
    endif()
    # The pin must be visible on the target: an upstream rename of an option would silently restore the
    # AVX2 defaults, and the test executables cannot include Jolt headers to check (CONVENTIONS 6).
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|X86|x86|i386)$")
        get_target_property(_jolt_definitions Jolt INTERFACE_COMPILE_DEFINITIONS)
        foreach(_jolt_required IN ITEMS JPH_USE_SSE4_1 JPH_USE_SSE4_2)
            if(NOT "${_jolt_required}" IN_LIST _jolt_definitions)
                message(FATAL_ERROR "third_party: Jolt does not define ${_jolt_required}; the instruction-set pin "
                                    "of 14_physics.md 8 is not in effect (defines: ${_jolt_definitions})")
            endif()
        endforeach()
        foreach(_jolt_forbidden IN ITEMS JPH_USE_AVX JPH_USE_AVX2 JPH_USE_AVX512 JPH_USE_LZCNT JPH_USE_TZCNT
                                         JPH_USE_F16C JPH_USE_FMADD JPH_DOUBLE_PRECISION
                                         JPH_FLOATING_POINT_EXCEPTIONS_ENABLED)
            if("${_jolt_forbidden}" IN_LIST _jolt_definitions)
                message(FATAL_ERROR "third_party: Jolt defines ${_jolt_forbidden}, which the baseline-x64 pin of "
                                    "14_physics.md 8 forbids (defines: ${_jolt_definitions})")
            endif()
        endforeach()
        unset(_jolt_definitions)
    endif()
    # Jolt checks at RegisterTypes() that the library and every including translation unit agree on the
    # JPH_* feature defines (Jolt/Core/Core.h, JPH_VERSION_ID). Its own CMake knows Debug and Release only:
    # it defines _DEBUG / NDEBUG and the debug renderer per configuration, and Debug gets asserts because
    # Jolt/Core/IssueReporting.h derives JPH_ENABLE_ASSERTS from JPH_DEBUG (itself from _DEBUG). The
    # engine's Development configuration would get neither asserts nor the debug renderer; 14_physics.md 8
    # wants both in Debug and Development, Distribution gets neither. Naming JPH_ENABLE_ASSERTS for Debug
    # too makes the JPH_VERSION_ID feature bits identical in both configurations. PUBLIC keeps the library
    # and its consumers in agreement.
    target_compile_definitions(Jolt PUBLIC
        $<$<CONFIG:Development>:JPH_DEBUG_RENDERER>
        $<$<CONFIG:Debug,Development>:JPH_ENABLE_ASSERTS>
    )
endif()

#------------------------------------------------------------------------------
# Dear ImGui (docking) + ImGuizmo - editor only
#------------------------------------------------------------------------------
conjure_third_party_notice(imgui
    NAME "Dear ImGui" VERSION v1.91.9b-docking LICENSE_ID "MIT" SCOPE EDITOR
    URL "https://github.com/ocornut/imgui"
    NOTE "Embeds the ProggyClean font, Copyright (c) 2004, 2005 Tristan Grimmer, under the MIT licence \
(http://www.proggyfonts.net)."
    FETCH imgui LICENSE_FILES LICENSE.txt
)
conjure_third_party_notice(imguizmo
    NAME "ImGuizmo" VERSION 105e4fde45e4e74aae8973d98750820ef7aca7e2 LICENSE_ID "MIT" SCOPE EDITOR
    URL "https://github.com/CedricGuillemet/ImGuizmo"
    FETCH imguizmo LICENSE_FILES LICENSE
)
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

# .NET is found on the build machine (cmake/FindNetHost.cmake), not pinned, so its licence text cannot be part of
# the tracked notices: the runtime notices take LICENSE.txt and ThirdPartyNotices.txt from the root of the
# installation whose host pack is linked, six folders above the pack's native directory
# (<root>/packs/Microsoft.NETCore.App.Host.<rid>/<version>/runtimes/<rid>/native). On Windows that licence file
# is Microsoft's terms for the .NET binaries, elsewhere the MIT text.
set(dotnetLinked FALSE)
set(dotnetLicenceFiles "")
set(dotnetVersion "")
if(CONJURE_WITH_SCRIPTING AND NetHost_FOUND)
    set(dotnetLinked TRUE)
    set(dotnetVersion "${NetHost_VERSION}")
    cmake_path(SET dotnetRoot NORMALIZE "${NetHost_INCLUDE_DIR}/../../../../../..")
    set(dotnetLicenceFiles "${dotnetRoot}/LICENSE.txt" "${dotnetRoot}/ThirdPartyNotices.txt")
endif()
conjure_third_party_notice(dotnet
    NAME ".NET" LICENSE_ID "MIT, binaries under the terms of the installation" SCOPE RUNTIME
    URL "https://github.com/dotnet/runtime"
    NOTE "The .NET hosting library (nethost) that starts the script runtime, and in a self-contained export the \
.NET runtime under managed/. .NET is taken from the machine that builds the engine and exports the game, not \
pinned. Its source is under the MIT licence, and the licence file of the installation states the terms of its \
binaries, which on Windows are Microsoft's."
    USED ${dotnetLinked} HOST_PROVIDED HOST_VERSION "${dotnetVersion}" HOST_FILES ${dotnetLicenceFiles}
)

# The Visual C++ runtime DLLs an MSVC Distribution build places next to conjure_runtime (runtime/CMakeLists.txt,
# 06_engine_loop.md D14). They are Microsoft's files under Microsoft's terms: there is no licence text to print.
set(msvcRuntimeImported FALSE)
conjure_abi_runtime_facts(noticesDynamicCrt noticesAsan noticesUbsan)
if(MSVC AND noticesDynamicCrt)
    set(msvcRuntimeImported TRUE)
endif()
conjure_third_party_notice(msvc_runtime
    NAME "Microsoft Visual C++ Runtime" LICENSE_ID "Microsoft Software License Terms" SCOPE RUNTIME
    URL "https://visualstudio.microsoft.com/license-terms/"
    NOTE "vcruntime140*.dll and msvcp140*.dll, where a Distribution build placed them next to the runtime. They \
are Microsoft's files, unchanged, and are covered by the licence terms of Visual Studio, which name them among \
the files that may be distributed with a program, not by an open-source licence."
    USED ${msvcRuntimeImported}
)

conjure_write_third_party_notices()

set(BUILD_SHARED_LIBS "${CONJURE_SAVED_BUILD_SHARED_LIBS}")
