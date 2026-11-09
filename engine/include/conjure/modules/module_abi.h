#pragma once

#include "conjure/modules/abi_identity.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/api.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"

#include <cstdint>

// The C front door of a shared module (docs/design/07_modules.md D2 and 2.2).
//
// A shared module exports exactly two C-linkage symbols: conjure_module_describe() returns a plain
// C struct holding the ABI signature and the module name, and is the only module code that runs
// before the signature has been compared field by field with the host's; conjure_module_entry()
// runs after the comparison passed and from there the module uses ordinary C++ engine headers.
//
// The generated header conjure/modules/abi_identity.h (cmake/ConjureSharedLibrary.cmake) supplies
// CONJURE_ABI_HEADER_HASH (hash of every public engine header) and the engine build's compiler
// major and runtime facts (CONJURE_ABI_COMPILER_MAJOR, CONJURE_ABI_RUNTIME_DYNAMIC_CRT,
// CONJURE_ABI_RUNTIME_ASAN, CONJURE_ABI_RUNTIME_UBSAN); the signature never reads compiler-defined
// macros itself (only core/platform_detect.h may).
//
// The design (07_modules.md 2.2) wants compilerMajor and the CRT / ASan bits to describe the
// translation unit being compiled, not the engine build: a module built against the installed SDK
// ships the generated header frozen, so a different compiler major, a static CRT or a missing ASan
// would otherwise report the engine's values and pass compare_abi(). Whenever platform_detect.h
// defines CONJURE_COMPILER_VERSION_MAJOR, CONJURE_RUNTIME_DYNAMIC_CRT and CONJURE_SANITIZER_ADDRESS
// (requested from core; see engine/src/modules/README.md) they take precedence below; UBSan has no
// reliable compile-time macro and always comes from the generated header.

#define CONJURE_MODULE_ABI_VERSION 1u
#define CONJURE_MODULE_ABI_MAGIC 0x444D4A43u                        // 'CJMD'
#define CONJURE_MODULE_DESCRIBE_SYMBOL "conjure_module_describe"
#define CONJURE_MODULE_ENTRY_SYMBOL "conjure_module_entry"

#if defined(CONJURE_COMPILER_VERSION_MAJOR)
    #define CONJURE_ABI_SIGNATURE_COMPILER_MAJOR CONJURE_COMPILER_VERSION_MAJOR
#else
    #define CONJURE_ABI_SIGNATURE_COMPILER_MAJOR CONJURE_ABI_COMPILER_MAJOR
#endif
#if defined(CONJURE_RUNTIME_DYNAMIC_CRT)
    #define CONJURE_ABI_SIGNATURE_DYNAMIC_CRT CONJURE_RUNTIME_DYNAMIC_CRT
#else
    #define CONJURE_ABI_SIGNATURE_DYNAMIC_CRT CONJURE_ABI_RUNTIME_DYNAMIC_CRT
#endif
#if defined(CONJURE_SANITIZER_ADDRESS)
    #define CONJURE_ABI_SIGNATURE_ASAN CONJURE_SANITIZER_ADDRESS
#else
    #define CONJURE_ABI_SIGNATURE_ASAN CONJURE_ABI_RUNTIME_ASAN
#endif
#define CONJURE_ABI_SIGNATURE_UBSAN CONJURE_ABI_RUNTIME_UBSAN

// Exports the two entry symbols from a module binary whatever the engine's own linkage is.
#define CONJURE_MODULE_EXPORT CONJURE_SYMBOL_EXPORT

namespace conjure::modules {
    class module_context;
}

extern "C" {
    /// <summary>
    /// Everything that must match exactly between a module binary and the host before any C++
    /// object crosses the boundary. Pure C layout; compared field by field by compare_abi().
    /// </summary>
    struct conjure_module_abi_signature {
        std::uint32_t magic = 0;                // CONJURE_MODULE_ABI_MAGIC
        std::uint32_t structSize = 0;           // sizeof(conjure_module_abi_signature)
        std::uint32_t abiVersion = 0;           // CONJURE_MODULE_ABI_VERSION
        std::uint32_t compilerId = 0;           // 1 = MSVC, 2 = GCC, 3 = Clang, 4 = AppleClang
        std::uint32_t compilerMajor = 0;        // _MSC_VER / 100, __GNUC__, __clang_major__
        std::uint32_t standardLibraryId = 0;    // 1 = MSVC STL, 2 = libstdc++, 3 = libc++
        std::uint32_t buildConfig = 0;          // build_config
        std::uint32_t architecture = 0;         // 1 = x64, 2 = arm64
        std::uint32_t runtimeFlags = 0;         // bit0 dynamic CRT, bit1 ASan, bit2 UBSan
        std::uint32_t featureFlags = 0;         // module_feature_flags compiled in
        std::uint32_t engineVersionMajor = 0;   // exact match required
        std::uint32_t engineVersionMinor = 0;
        std::uint32_t engineVersionPatch = 0;
        std::uint64_t headerHash = 0;           // CONJURE_ABI_HEADER_HASH
    };

    /// <summary>
    /// What conjure_module_describe() returns: the signature plus the module name, which must
    /// equal the manifest name.
    /// </summary>
    struct conjure_module_info {
        conjure_module_abi_signature abi = {};
        const char *name = "";
    };

    typedef const conjure_module_info *(*conjure_module_describe_fn)(void);
    typedef std::uint32_t (*conjure_module_entry_fn)(conjure::modules::module_context *context);
}

namespace conjure::modules {

    inline constexpr std::uint32_t ABI_COMPILER_MSVC = 1;
    inline constexpr std::uint32_t ABI_COMPILER_GCC = 2;
    inline constexpr std::uint32_t ABI_COMPILER_CLANG = 3;
    inline constexpr std::uint32_t ABI_COMPILER_APPLECLANG = 4;

    inline constexpr std::uint32_t ABI_STDLIB_UNKNOWN = 0;
    inline constexpr std::uint32_t ABI_STDLIB_MSVC = 1;
    inline constexpr std::uint32_t ABI_STDLIB_LIBSTDCXX = 2;
    inline constexpr std::uint32_t ABI_STDLIB_LIBCXX = 3;

    inline constexpr std::uint32_t ABI_ARCH_X64 = 1;
    inline constexpr std::uint32_t ABI_ARCH_ARM64 = 2;

    inline constexpr std::uint32_t ABI_RUNTIME_DYNAMIC_CRT = 1u << 0;
    inline constexpr std::uint32_t ABI_RUNTIME_ASAN = 1u << 1;
    inline constexpr std::uint32_t ABI_RUNTIME_UBSAN = 1u << 2;

    /// <summary>
    /// The compiler major the signature of this translation unit reports (see the header comment).
    /// </summary>
    inline constexpr std::uint32_t ABI_SIGNATURE_COMPILER_MAJOR =
      static_cast<std::uint32_t>(CONJURE_ABI_SIGNATURE_COMPILER_MAJOR);

    /// <summary>
    /// The runtime flags the signature of this translation unit reports: bit0 dynamic CRT, bit1
    /// ASan, bit2 UBSan.
    /// </summary>
    inline constexpr std::uint32_t ABI_SIGNATURE_RUNTIME_FLAGS =
      ((CONJURE_ABI_SIGNATURE_DYNAMIC_CRT) != 0 ? ABI_RUNTIME_DYNAMIC_CRT : 0u)
      | ((CONJURE_ABI_SIGNATURE_ASAN) != 0 ? ABI_RUNTIME_ASAN : 0u)
      | ((CONJURE_ABI_SIGNATURE_UBSAN) != 0 ? ABI_RUNTIME_UBSAN : 0u);

    /// <summary>
    /// The signature of the translation unit that calls it. CONJURE_TEST_ABI_VERSION_OVERRIDE and
    /// CONJURE_TEST_ENGINE_VERSION_OVERRIDE (a major version) exist for the mismatch fixtures only.
    /// </summary>
    [[nodiscard]] constexpr conjure_module_abi_signature make_abi_signature() noexcept {
        conjure_module_abi_signature signature = {};
        signature.magic = CONJURE_MODULE_ABI_MAGIC;
        signature.structSize = static_cast<std::uint32_t>(sizeof(conjure_module_abi_signature));
#if defined(CONJURE_TEST_ABI_VERSION_OVERRIDE)
        signature.abiVersion = static_cast<std::uint32_t>(CONJURE_TEST_ABI_VERSION_OVERRIDE);
#else
        signature.abiVersion = CONJURE_MODULE_ABI_VERSION;
#endif
        if constexpr (CONJURE_COMPILER_MSVC == 1) {
            signature.compilerId = ABI_COMPILER_MSVC;
        } else if constexpr (CONJURE_COMPILER_APPLECLANG == 1) {
            signature.compilerId = ABI_COMPILER_APPLECLANG;
        } else if constexpr (CONJURE_COMPILER_CLANG == 1) {
            signature.compilerId = ABI_COMPILER_CLANG;
        } else {
            signature.compilerId = ABI_COMPILER_GCC;
        }
        signature.compilerMajor = ABI_SIGNATURE_COMPILER_MAJOR;
        if constexpr (CONJURE_STDLIB_MSVC == 1) {
            signature.standardLibraryId = ABI_STDLIB_MSVC;
        } else if constexpr (CONJURE_STDLIB_LIBSTDCXX == 1) {
            signature.standardLibraryId = ABI_STDLIB_LIBSTDCXX;
        } else if constexpr (CONJURE_STDLIB_LIBCXX == 1) {
            signature.standardLibraryId = ABI_STDLIB_LIBCXX;
        } else {
            signature.standardLibraryId = ABI_STDLIB_UNKNOWN;
        }
        signature.buildConfig = static_cast<std::uint32_t>(host_build_config());
        signature.architecture = host_architecture() == module_architecture::ARM64 ? ABI_ARCH_ARM64 : ABI_ARCH_X64;
        signature.runtimeFlags = ABI_SIGNATURE_RUNTIME_FLAGS;
        signature.featureFlags = static_cast<std::uint32_t>(host_feature_flags());
#if defined(CONJURE_TEST_ENGINE_VERSION_OVERRIDE)
        signature.engineVersionMajor = static_cast<std::uint32_t>(CONJURE_TEST_ENGINE_VERSION_OVERRIDE);
#else
        signature.engineVersionMajor = host_engine_version().majorVersion;
#endif
        signature.engineVersionMinor = host_engine_version().minorVersion;
        signature.engineVersionPatch = host_engine_version().patchVersion;
        signature.headerHash = static_cast<std::uint64_t>(CONJURE_ABI_HEADER_HASH);
        return signature;
    }

    /// <summary>
    /// The describe() payload of a module named 'name' (a string literal that outlives the binary).
    /// </summary>
    [[nodiscard]] constexpr conjure_module_info make_module_info(const char *name) noexcept {
        conjure_module_info info = {};
        info.abi = make_abi_signature();
        info.name = name;
        return info;
    }

    /// <summary>
    /// The signature the engine itself was built with.
    /// </summary>
    [[nodiscard]] CONJURE_API const conjure_module_abi_signature &host_abi_signature() noexcept;

    /// <summary>
    /// Compares every field, structSize first (a smaller struct is rejected before any other field
    /// is read), then magic, abiVersion and the rest in declaration order. ABI_MISMATCH names the
    /// first differing field with the expected and actual values and asks for a rebuild.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> compare_abi(
      const conjure_module_abi_signature &moduleSignature,
      const conjure_module_abi_signature &hostSignature
    );
}
