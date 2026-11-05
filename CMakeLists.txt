cmake_minimum_required(VERSION 3.28)

# Configuration types must be declared before project() so that the custom
# Development and Distribution configurations exist for every target,
# including fetched third-party ones.
include(cmake/ConjureConfigs.cmake)

project(Conjure
    VERSION 0.1.0
    DESCRIPTION "Conjure - a data-oriented 2D/3D game engine"
    LANGUAGES C CXX
)

conjure_derive_config_flags()

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a build directory (see CMakePresets.json).")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

#------------------------------------------------------------------------------
# Options
#------------------------------------------------------------------------------
option(CONJURE_WITH_RENDERER  "Build the renderer and RHI modules"            ON)
option(CONJURE_WITH_PHYSICS   "Build the physics module (Jolt)"               ON)
option(CONJURE_WITH_AUDIO     "Build the audio module (miniaudio)"            ON)
option(CONJURE_WITH_SCRIPTING "Build the C# scripting host (.NET)"            ON)
option(CONJURE_BUILD_EDITOR   "Build the editor executable"                   ON)
option(CONJURE_BUILD_RUNTIME  "Build the runtime executable"                  ON)
option(CONJURE_BUILD_TESTS    "Build unit and integration tests"              ON)
option(CONJURE_BUILD_BENCHMARKS "Build benchmark executables"                 ON)
option(CONJURE_BUILD_SAMPLES  "Build sample projects"                         ON)
option(CONJURE_BUILD_MANAGED  "Build the C# managed assemblies with dotnet"   ON)
option(CONJURE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"         ON)
option(CONJURE_ENABLE_ASAN    "Enable AddressSanitizer (GCC/Clang/MSVC)"      OFF)
option(CONJURE_ENABLE_UBSAN   "Enable UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

if(WIN32)
    option(CONJURE_RHI_D3D12  "Enable the D3D12 RHI backend"   ON)
    option(CONJURE_RHI_VULKAN "Enable the Vulkan RHI backend"  OFF)
else()
    set(CONJURE_RHI_D3D12 OFF CACHE INTERNAL "D3D12 is Windows only")
    option(CONJURE_RHI_VULKAN "Enable the Vulkan RHI backend"  ON)
endif()

if(CONJURE_WITH_RENDERER AND NOT CONJURE_RHI_D3D12 AND NOT CONJURE_RHI_VULKAN)
    message(FATAL_ERROR "CONJURE_WITH_RENDERER requires at least one of CONJURE_RHI_D3D12 / CONJURE_RHI_VULKAN")
endif()

# The editor is never part of a Distribution build. With a multi-config generator the editor
# target still exists; it is excluded from the Distribution configuration per target instead.
if(NOT CONJURE_IS_MULTI_CONFIG AND CMAKE_BUILD_TYPE STREQUAL "Distribution" AND CONJURE_BUILD_EDITOR)
    message(STATUS "Distribution build: editor disabled")
    set(CONJURE_BUILD_EDITOR OFF)
endif()

#------------------------------------------------------------------------------
# Global compiler settings
#------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# All build products in predictable places so the editor, runtime, shaders and
# managed assemblies can find each other without installation.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

if(MSVC)
    # Multi-processor compilation, UTF-8 sources, correct __cplusplus, strict preprocessor.
    add_compile_options(/MP /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc /bigobj)
endif()

include(ConjureWarnings)
include(ConjureModule)
include(ConjureTesting)
include(ConjureSanitizers)

#------------------------------------------------------------------------------
# Third-party dependencies (declared once, pinned)
#------------------------------------------------------------------------------
add_subdirectory(third_party)

#------------------------------------------------------------------------------
# Engine, tools and executables
#------------------------------------------------------------------------------
add_subdirectory(engine)

if(CONJURE_BUILD_MANAGED AND CONJURE_WITH_SCRIPTING)
    add_subdirectory(managed)
endif()

if(CONJURE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(CONJURE_BUILD_RUNTIME)
    add_subdirectory(runtime)
endif()

if(CONJURE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CONJURE_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

conjure_print_configuration_summary()
