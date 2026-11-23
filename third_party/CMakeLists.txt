# Third-party dependencies.
#
# Every dependency is declared here, or in a file this one includes (jxl.cmake), exactly once, pinned to an archive
# verified by its SHA-256, and exposed to the engine through a conjure::thirdparty::<name> target. Engine code never
# references the raw upstream targets so that a dependency can be swapped or vendored in one place.
#
# Pins (code sweep F772): the declarations here name the archive of one commit, URL
# https://github.com/<owner>/<repo>/archive/<commit>.tar.gz with URL_HASH SHA256=<hash of that file>; the release tag,
# where the commit has one, is the comment "# tag <name>" on the line of FetchContent_Declare. jxl.cmake names the
# archive of libjxl's release tag (refs/tags/v<version>.tar.gz), whose bytes its hash holds just the same. A tag can be
# moved and a clone checks nothing, an archive is checked before it is unpacked: conjure_check_third_party_pins()
# (cmake/ConjureNotices.cmake) fails the configure, before anything is downloaded, when a declaration in this file or
# in a file it includes names no URL with URL_HASH SHA256=<64 hex digits>, or fetches from a version control system,
# with a DOWNLOAD_COMMAND, with ExternalProject_Add() or with the direct form of FetchContent_Populate(). It checks
# the hash, not the form of the URL. To bump a pin, take the commit the new tag points to (git ls-remote <repository>
# refs/tags/<tag>^{}, or refs/tags/<tag> for a lightweight tag), put it in the URL and the tag in the comment and the
# notice's VERSION, and record the hash of the archive: cmake -E sha256sum on the downloaded file, or the actual hash
# the configure prints when the old one no longer matches. GitHub generates these archives; should their bytes change
# again (they did once, early 2023, and were reverted), the configure stops on the hash instead of building something
# else.
#
# Dependencies are declared SYSTEM so their headers do not trigger our warnings, and
# EXCLUDE_FROM_ALL so only the parts the engine links are built.
#
# Every dependency also has a conjure_third_party_notice() beside its declaration (cmake/ConjureNotices.cmake,
# docs/design/06_engine_loop.md D15): its name, pin, licence and licence files. The notices are registered
# outside the option blocks, so every tree knows the whole list and a tree that does not fetch a dependency
# says so. VERSION repeats the tag of the comment, or the commit of the URL where there is no tag, and the configure
# fails when the two differ; jxl.cmake builds its URLs from variables, which its notices read as well. After a pin or
# a notice changed, regenerate the tracked THIRD_PARTY_NOTICES.md: python tools/third_party_notices.py --build-dir
# <tree> (docs/handbook/project-reference/third-party-dependencies/index.md, "Licence and third-party
# notices").

include(FetchContent)
include(ConjureNotices)
include("${CMAKE_CURRENT_SOURCE_DIR}/nvrhi_checks.cmake")
set(FETCHCONTENT_QUIET OFF)

# This file and the files it includes, itself or through jxl.cmake (named, as CMake globs nothing here, AGENTS.md 9:
# an included file that is added is added to this list too).
conjure_check_third_party_pins("${CMAKE_CURRENT_LIST_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/jxl.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/jxl_checks.cmake" "${CMAKE_CURRENT_SOURCE_DIR}/nvrhi_checks.cmake")

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
    URL      https://github.com/nothings/stb/archive/d14d46f6b44456eb88beac58cba833a4135d4c89.tar.gz
    URL_HASH SHA256=1b59a668a789a28a8fd4e01ff1413eec7eaeb28688bafdc66ec7068f4e7095e1
    SYSTEM
)
FetchContent_Declare(cgltf  # tag v1.14
    URL      https://github.com/jkuhlmann/cgltf/archive/52c23814dbb60c2fbb54750ddf41d342d432a498.tar.gz
    URL_HASH SHA256=b0d9edddaf691d903449c8a8e2c72ad564abe314531fbb49fb589b6289563eec
    SYSTEM
)
FetchContent_Declare(miniaudio  # tag 0.11.22
    URL      https://github.com/mackron/miniaudio/archive/350784a9467a79d0fa65802132668e5afbcf3777.tar.gz
    URL_HASH SHA256=28ab6d0d39c32a46d57b52d104046e8b3637bfc51d8bab07ec5ae5415dc8ee52
    SOURCE_SUBDIR cmake_not_used     # we compile the single file ourselves; skip upstream CMake
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
FetchContent_Declare(glm  # tag 1.0.1
    URL      https://github.com/g-truc/glm/archive/0af55ccecd98d4e5a8d1fad7de25ba429d60e863.tar.gz
    URL_HASH SHA256=e7f187d83523f505eb38dd25d297ea6c0d4ed856d733e808f18253f5a8fa88a0
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
FetchContent_Declare(yyjson  # tag 0.10.0
    URL      https://github.com/ibireme/yyjson/archive/9ddba001a4ea88e93b46932e5c5b87b222e19a5f.tar.gz
    URL_HASH SHA256=72ecea1847001e9a625e8ec979bfc65b34499277ef96f7d082cda3415bb38c53
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
    FetchContent_Declare(doctest  # tag v2.4.11
        URL      https://github.com/doctest/doctest/archive/ae7a13539fb71f270b87eb2e874fbac80bc8dda2.tar.gz
        URL_HASH SHA256=3882bb436ee458653e0562bf799af21a085bd6c64325a837b77d7edd867144dc
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
FetchContent_Declare(glfw  # tag 3.4
    URL      https://github.com/glfw/glfw/archive/7b6aead9fb88b3623e3b3725ebb42670cbe4c579.tar.gz
    URL_HASH SHA256=169d49096e339caabbc12e1762273c5cb9130a535a282ba46cf5c14ab6e55694
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
# NVRHI fetches the API headers of the backends it builds, from git at the tags its own CMakeLists.txt names.
# FetchContent keeps the first declaration of a name, so both are declared here before NVRHI, pinned like every other
# dependency, and NVRHI's own declarations of them change nothing. NVRHI was written against those tags: the check
# after it was made available (conjure_check_nvrhi_header_pins, nvrhi_checks.cmake) stops the configure when the
# pinned NVRHI names others than the two below, declares a dependency this file does not pin, or would fetch rtxmu
# (NVRHI_WITH_RTXMU on).
set(directxHeadersTag v1.717.0-preview)
set(vulkanHeadersTag v1.4.352)
conjure_third_party_notice(nvrhi
    NAME "NVRHI" VERSION bf0bbd9b666a7297492beb1999a4ddc0dfded143 LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/NVIDIA-RTX/NVRHI"
    FETCH nvrhi LICENSE_FILES LICENSE.txt
)
conjure_third_party_notice(directx_headers
    NAME "DirectX-Headers" VERSION ${directxHeadersTag} LICENSE_ID "MIT" SCOPE RUNTIME
    URL "https://github.com/microsoft/DirectX-Headers"
    NOTE "The Direct3D 12 headers the D3D12 backend of NVRHI is compiled against."
    FETCH directx_headers LICENSE_FILES LICENSE
)
conjure_third_party_notice(vulkan_headers
    NAME "Vulkan-Headers" VERSION ${vulkanHeadersTag} LICENSE_ID "Apache-2.0 OR MIT" SCOPE RUNTIME
    URL "https://github.com/KhronosGroup/Vulkan-Headers"
    NOTE "The Vulkan headers the Vulkan backend of NVRHI is compiled against."
    FETCH vulkan_headers LICENSE_FILES LICENSE.md LICENSES/Apache-2.0.txt LICENSES/MIT.txt
)
if(CONJURE_WITH_RENDERER)
    # Declared in every renderer tree; NVRHI makes available the ones of the backends it builds.
    FetchContent_Declare(directx_headers  # tag v1.717.0-preview
        URL      https://github.com/microsoft/DirectX-Headers/archive/d873b344dc540898868697245f100c2a67fe68d9.tar.gz
        URL_HASH SHA256=b94c898c2917c8d28d3d8e6b6f068d75ef04eb50044c148d1cfab5016e6e991a
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    FetchContent_Declare(vulkan_headers  # tag v1.4.352
        URL      https://github.com/KhronosGroup/Vulkan-Headers/archive/015e25c3c91b70eb1a754d36fb14c4ba6ad9b0b9.tar.gz
        URL_HASH SHA256=666700236101d288dea8bb080c6f8039467cb1d29ea1e1aae6f93a819f4f9029
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    FetchContent_Declare(nvrhi
        URL      https://github.com/NVIDIA-RTX/NVRHI/archive/bf0bbd9b666a7297492beb1999a4ddc0dfded143.tar.gz
        URL_HASH SHA256=059ce649063d7bfe8fd7cb2f4cdd05a606f5526f7060237f9e37ecf75bc5f9e2
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

    conjure_check_nvrhi_header_pins("${nvrhi_SOURCE_DIR}/CMakeLists.txt" "${directxHeadersTag}" "${vulkanHeadersTag}")

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
    FetchContent_Declare(glslang  # tag 15.1.0
        URL      https://github.com/KhronosGroup/glslang/archive/1062752a891c95b2bfeed9e356562d88f9df84ac.tar.gz
        URL_HASH SHA256=46a05bba7675a3ab1912a0908183b9eee8daa06bb8047943bac54a6f0de2cfce
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
    FetchContent_Declare(JoltPhysics  # tag v5.3.0
        URL      https://github.com/jrouwe/JoltPhysics/archive/0373ec0dd762e4bc2f6acdb08371ee84fa23c6db.tar.gz
        URL_HASH SHA256=834644d7e9dffee9be0220748015667649d3361611b01c7e6839a5eb58e89711
        SOURCE_SUBDIR Build
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
    # Jolt's GENERATE_DEBUG_SYMBOLS appends its debug-information flag to every configuration. With GCC and Clang
    # that is -g, which Debug and Development already carry (CMAKE_CXX_FLAGS_DEBUG, and Development derives from
    # RelWithDebInfo), so the option would only add DWARF to Distribution, and DWARF names the build machine's
    # absolute paths, which the Distribution path options of cmake/ConjureDistributionPaths.cmake do not reach
    # (code sweep F613; tools/check_distribution_paths.py). With MSVC it is /Zi, Jolt's only source of debug
    # information (its CMake policies predate CMAKE_MSVC_DEBUG_INFORMATION_FORMAT), and nothing of it reaches a
    # Distribution binary, which links without /DEBUG and so carries no PDB path.
    if(MSVC)
        set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "" FORCE)
    else()
        set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
    endif()
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
    FetchContent_Declare(imgui  # tag v1.91.9b-docking
        URL      https://github.com/ocornut/imgui/archive/4806a1924ff6181180bf5e4b8b79ab4394118875.tar.gz
        URL_HASH SHA256=cc35a88df717dff3c488e43b4b148dcfe968bd3ff6798f60751b106eb9bcee48
        SYSTEM
    )
    FetchContent_Declare(imguizmo
        URL      https://github.com/CedricGuillemet/ImGuizmo/archive/105e4fde45e4e74aae8973d98750820ef7aca7e2.tar.gz
        URL_HASH SHA256=d625a9f3207f15b7a5b7706269747676f17b7e4df00f898ced9480e5063d493a
        SOURCE_SUBDIR cmake_not_used     # only ImGuizmo.cpp is wanted; skip upstream CMake
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

#------------------------------------------------------------------------------
# Documentation site (docs/assets, tools/docs_theme)
#------------------------------------------------------------------------------
# tools/docs_assets.py imports four files of the Material for MkDocs theme pinned in tools/docs_requirements.txt into
# docs/assets as editable sources (docs/assets/material-version.txt names the version), and the site publishes them
# with the callout icons. No build fetches them and no binary contains them: they are tracked under their authors'
# licences, whose texts are kept in docs/assets/licenses/ so that the site publishes them too. The import drops the
# bundle's focus-visible polyfill (W3C Software and Document License, whose full text would have to ship with it),
# and the stylesheet uses the :focus-visible pseudo-class instead. A new vendored file needs its notice here first;
# tools/tests/test_docs_site.py checks that docs/assets holds no other.
conjure_third_party_notice(mkdocs_material
    NAME "Material for MkDocs" VERSION 9.6.14 LICENSE_ID "MIT" SCOPE BUILD
    URL "https://github.com/squidfunk/mkdocs-material"
    NOTE "The theme of the documentation site. docs/assets/stylesheets/material-theme.css and material-palette.css, \
docs/assets/javascripts/material-theme.js and workers/search-worker.js are reformatted copies of its stylesheets, \
its bundle (without its focus-visible polyfill) and its search worker, and tools/docs_theme/partials/nav.html and \
nav-item.html are modified copies of its templates. The bundle and the worker contain the libraries that follow \
under their own names, and material-theme.css contains the colour variables of material-design-color."
    LICENSE_FILES ../docs/assets/licenses/mkdocs-material.txt
)
conjure_third_party_notice(material_design_color
    NAME "material-design-color" VERSION 2.3.2 LICENSE_ID "MIT" SCOPE BUILD
    URL "https://github.com/mrmlnc/material-color"
    NOTE "The colour variables in docs/assets/stylesheets/material-theme.css (material-color.scss in the theme's \
source map), which keeps no notice of it."
    LICENSE_FILES ../docs/assets/licenses/material-design-color.txt
)
conjure_third_party_notice(lunr
    NAME "lunr.js" VERSION 2.3.9 LICENSE_ID "MIT" SCOPE BUILD
    URL "https://github.com/olivernn/lunr.js"
    NOTE "The search index of the documentation site, in docs/assets/javascripts/workers/search-worker.js. Its \
stemmer includes the Porter stemmer JavaScript code from http://tartarus.org/~martin/PorterStemmer/, as its header \
says."
    LICENSE_FILES ../docs/assets/licenses/lunr.txt
)
conjure_third_party_notice(rxjs
    NAME "RxJS" LICENSE_ID "Apache-2.0" SCOPE BUILD
    URL "https://github.com/ReactiveX/rxjs"
    NOTE "In docs/assets/javascripts/material-theme.js (Material for MkDocs 9.6.14 depends on rxjs ^7.8.2), which \
keeps no copyright line of it: Copyright (c) 2015-2018 Google, Inc., Netflix, Inc., Microsoft Corp. and contributors."
    LICENSE_FILES ../docs/assets/licenses/apache-2.0.txt
)
conjure_third_party_notice(clipboard_js
    NAME "clipboard.js" VERSION v2.0.11 LICENSE_ID "MIT" SCOPE BUILD
    URL "https://github.com/zenorocha/clipboard.js"
    NOTE "The copy buttons of code blocks, in docs/assets/javascripts/material-theme.js."
    LICENSE_FILES ../docs/assets/licenses/clipboard-js.txt
)
conjure_third_party_notice(escape_html
    NAME "escape-html" LICENSE_ID "MIT" SCOPE BUILD
    URL "https://github.com/component/escape-html"
    NOTE "In docs/assets/javascripts/material-theme.js."
    LICENSE_FILES ../docs/assets/licenses/escape-html.txt
)
conjure_third_party_notice(tslib
    NAME "tslib" LICENSE_ID "0BSD" SCOPE BUILD
    URL "https://github.com/microsoft/tslib"
    NOTE "Runtime helpers of the TypeScript compiler, in docs/assets/javascripts/material-theme.js."
    LICENSE_FILES ../docs/assets/licenses/tslib.txt
)
conjure_third_party_notice(material_design_icons
    NAME "Material Design Icons (Pictogrammers)" LICENSE_ID "Apache-2.0" SCOPE BUILD
    URL "https://github.com/Templarian/MaterialDesign"
    NOTE "docs/assets/icons/callout-caution.svg, callout-note.svg, callout-tip.svg and callout-warning.svg are its \
icons lightning-bolt-circle, pencil-circle, fire and alert, as Material for MkDocs ships them."
    LICENSE_FILES ../docs/assets/licenses/material-design-icons.txt ../docs/assets/licenses/apache-2.0.txt
)
conjure_third_party_notice(font_awesome
    NAME "Font Awesome Free" VERSION 6.7.2 LICENSE_ID "CC-BY-4.0 (icons)" SCOPE BUILD
    URL "https://fontawesome.com/license/free"
    NOTE "One icon embedded in docs/assets/stylesheets/material-theme.css with its attribution comment."
    LICENSE_FILES ../docs/assets/licenses/font-awesome-free.txt
)

conjure_write_third_party_notices()

set(BUILD_SHARED_LIBS "${CONJURE_SAVED_BUILD_SHARED_LIBS}")
