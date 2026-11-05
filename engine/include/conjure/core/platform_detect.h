#pragma once

// Compile-time platform, compiler and build-configuration detection. This is the only header
// allowed to inspect compiler-defined macros such as _WIN32 or __clang__; everything else uses
// the CONJURE_* macros defined here.

#include <version>   // standard-library identification macros

/*===========================================================================*/
/*===========================================================================*/
// Platform

#if defined(_WIN32)
    #define CONJURE_PLATFORM_WINDOWS 1
    #define CONJURE_PLATFORM_MACOS 0
    #define CONJURE_PLATFORM_LINUX 0
    #define CONJURE_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
    #define CONJURE_PLATFORM_WINDOWS 0
    #define CONJURE_PLATFORM_MACOS 1
    #define CONJURE_PLATFORM_LINUX 0
    #define CONJURE_PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define CONJURE_PLATFORM_WINDOWS 0
    #define CONJURE_PLATFORM_MACOS 0
    #define CONJURE_PLATFORM_LINUX 1
    #define CONJURE_PLATFORM_NAME "Linux"
#else
    #error "Conjure: unsupported platform"
#endif

#define CONJURE_PLATFORM_POSIX (CONJURE_PLATFORM_MACOS || CONJURE_PLATFORM_LINUX)

/*===========================================================================*/
/*===========================================================================*/
// Compiler

#if defined(__clang__)
    #define CONJURE_COMPILER_CLANG 1
    #define CONJURE_COMPILER_MSVC 0
    #define CONJURE_COMPILER_GCC 0
    #if defined(__apple_build_version__)
        #define CONJURE_COMPILER_APPLECLANG 1
    #else
        #define CONJURE_COMPILER_APPLECLANG 0
    #endif
#elif defined(_MSC_VER)
    #define CONJURE_COMPILER_CLANG 0
    #define CONJURE_COMPILER_MSVC 1
    #define CONJURE_COMPILER_GCC 0
    #define CONJURE_COMPILER_APPLECLANG 0
#elif defined(__GNUC__)
    #define CONJURE_COMPILER_CLANG 0
    #define CONJURE_COMPILER_MSVC 0
    #define CONJURE_COMPILER_GCC 1
    #define CONJURE_COMPILER_APPLECLANG 0
#else
    #error "Conjure: unsupported compiler"
#endif

/*===========================================================================*/
/*===========================================================================*/
// Architecture and standard library

#if defined(_M_X64) || defined(__x86_64__)
    #define CONJURE_ARCH_X64 1
    #define CONJURE_ARCH_ARM64 0
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define CONJURE_ARCH_X64 0
    #define CONJURE_ARCH_ARM64 1
#else
    #error "Conjure: unsupported architecture (64-bit x86 or ARM required)"
#endif

#if defined(_MSVC_STL_VERSION)
    #define CONJURE_STDLIB_MSVC 1
    #define CONJURE_STDLIB_LIBSTDCXX 0
    #define CONJURE_STDLIB_LIBCXX 0
#elif defined(_LIBCPP_VERSION)
    #define CONJURE_STDLIB_MSVC 0
    #define CONJURE_STDLIB_LIBSTDCXX 0
    #define CONJURE_STDLIB_LIBCXX 1
#elif defined(__GLIBCXX__)
    #define CONJURE_STDLIB_MSVC 0
    #define CONJURE_STDLIB_LIBSTDCXX 1
    #define CONJURE_STDLIB_LIBCXX 0
#else
    #define CONJURE_STDLIB_MSVC 0
    #define CONJURE_STDLIB_LIBSTDCXX 0
    #define CONJURE_STDLIB_LIBCXX 0
#endif

/*===========================================================================*/
/*===========================================================================*/
// Build configuration. Exactly one of CONJURE_DEBUG / CONJURE_DEVELOPMENT / CONJURE_DISTRIBUTION
// is 1. The build system defines the active one; Development is the fallback so stray compiles
// still produce a usable, assert-enabled binary.

#if !defined(CONJURE_DEBUG) && !defined(CONJURE_DEVELOPMENT) && !defined(CONJURE_DISTRIBUTION)
    #define CONJURE_DEVELOPMENT 1
#endif

#if !defined(CONJURE_DEBUG)
    #define CONJURE_DEBUG 0
#endif
#if !defined(CONJURE_DEVELOPMENT)
    #define CONJURE_DEVELOPMENT 0
#endif
#if !defined(CONJURE_DISTRIBUTION)
    #define CONJURE_DISTRIBUTION 0
#endif

#if (CONJURE_DEBUG + CONJURE_DEVELOPMENT + CONJURE_DISTRIBUTION) != 1
    #error "Conjure: exactly one build configuration macro must be defined"
#endif

// Asserts and development tooling exist in every configuration except Distribution.
#define CONJURE_ASSERTS_ENABLED (!CONJURE_DISTRIBUTION)
#define CONJURE_DEV_TOOLS_ENABLED (!CONJURE_DISTRIBUTION)

#if CONJURE_DEBUG
    #define CONJURE_CONFIGURATION_NAME "Debug"
#elif CONJURE_DEVELOPMENT
    #define CONJURE_CONFIGURATION_NAME "Development"
#else
    #define CONJURE_CONFIGURATION_NAME "Distribution"
#endif

/*===========================================================================*/
/*===========================================================================*/
// Compiler helpers

#if CONJURE_COMPILER_MSVC
    #define CONJURE_FORCE_INLINE __forceinline
    #define CONJURE_NO_INLINE __declspec(noinline)
    #define CONJURE_DEBUG_BREAK() __debugbreak()
    #define CONJURE_FUNCTION_NAME __FUNCSIG__
    #define CONJURE_RESTRICT __restrict
    #define CONJURE_UNREACHABLE_HINT() __assume(0)
#else
    #define CONJURE_FORCE_INLINE inline __attribute__((always_inline))
    #define CONJURE_NO_INLINE __attribute__((noinline))
    #define CONJURE_DEBUG_BREAK() __builtin_trap()
    #define CONJURE_FUNCTION_NAME __PRETTY_FUNCTION__
    #define CONJURE_RESTRICT __restrict__
    #define CONJURE_UNREACHABLE_HINT() __builtin_unreachable()
#endif

// Symbol visibility for shared-library modules (plugins).
#if CONJURE_PLATFORM_WINDOWS
    #define CONJURE_SYMBOL_EXPORT __declspec(dllexport)
    #define CONJURE_SYMBOL_IMPORT __declspec(dllimport)
#else
    #define CONJURE_SYMBOL_EXPORT __attribute__((visibility("default")))
    #define CONJURE_SYMBOL_IMPORT
#endif

#define CONJURE_UNUSED(x) static_cast<void>(x)

#define CONJURE_CONCAT_IMPL(a, b) a##b
#define CONJURE_CONCAT(a, b) CONJURE_CONCAT_IMPL(a, b)
#define CONJURE_STRINGIFY_IMPL(x) #x
#define CONJURE_STRINGIFY(x) CONJURE_STRINGIFY_IMPL(x)

// Unique identifier for macro-local variables.
#define CONJURE_UNIQUE_NAME(base) CONJURE_CONCAT(base, __LINE__)
