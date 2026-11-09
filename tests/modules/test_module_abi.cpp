#include "conjure/testing/doctest.h"

#include "conjure/modules/module_abi.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/platform_detect.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    struct field_mutation {
        std::string_view name = {};
        void (*mutate)(conjure_module_abi_signature &) = nullptr;
    };

    constexpr std::array<field_mutation, 14> MUTATIONS = {{
        {"structSize", [](conjure_module_abi_signature &s) { s.structSize += 8; }},
        {"magic", [](conjure_module_abi_signature &s) { s.magic ^= 1; }},
        {"abiVersion", [](conjure_module_abi_signature &s) { s.abiVersion += 1; }},
        {"compilerId", [](conjure_module_abi_signature &s) { s.compilerId += 1; }},
        {"compilerMajor", [](conjure_module_abi_signature &s) { s.compilerMajor += 1; }},
        {"standardLibraryId", [](conjure_module_abi_signature &s) { s.standardLibraryId += 1; }},
        {"buildConfig", [](conjure_module_abi_signature &s) { s.buildConfig += 1; }},
        {"architecture", [](conjure_module_abi_signature &s) { s.architecture += 1; }},
        {"runtimeFlags", [](conjure_module_abi_signature &s) { s.runtimeFlags ^= 2; }},
        {"featureFlags", [](conjure_module_abi_signature &s) { s.featureFlags ^= 1; }},
        {"engineVersionMajor", [](conjure_module_abi_signature &s) { s.engineVersionMajor += 1; }},
        {"engineVersionMinor", [](conjure_module_abi_signature &s) { s.engineVersionMinor += 1; }},
        {"engineVersionPatch", [](conjure_module_abi_signature &s) { s.engineVersionPatch += 1; }},
        {"headerHash", [](conjure_module_abi_signature &s) { s.headerHash ^= 0xFFull; }},
    }};
}

TEST_SUITE("modules/module_abi") {

    TEST_CASE("module_abi - the host signature equals make_abi_signature and describes this build") {
        const conjure_module_abi_signature &host = host_abi_signature();
        const conjure_module_abi_signature made = make_abi_signature();
        CHECK(compare_abi(made, host).has_value());
        CHECK(compare_abi(host, made).has_value());

        CHECK(host.magic == CONJURE_MODULE_ABI_MAGIC);
        CHECK(host.structSize == sizeof(conjure_module_abi_signature));
        CHECK(host.abiVersion == CONJURE_MODULE_ABI_VERSION);
        CHECK(host.headerHash == static_cast<std::uint64_t>(CONJURE_ABI_HEADER_HASH));
        CHECK(host.compilerMajor == ABI_SIGNATURE_COMPILER_MAJOR);
        CHECK(host.compilerMajor == static_cast<std::uint32_t>(CONJURE_ABI_SIGNATURE_COMPILER_MAJOR));
        CHECK(host.compilerMajor > 0);
        CHECK(host.runtimeFlags == ABI_SIGNATURE_RUNTIME_FLAGS);
        CHECK(((host.runtimeFlags & ABI_RUNTIME_DYNAMIC_CRT) != 0) == (CONJURE_ABI_SIGNATURE_DYNAMIC_CRT != 0));
        CHECK(((host.runtimeFlags & ABI_RUNTIME_ASAN) != 0) == (CONJURE_ABI_SIGNATURE_ASAN != 0));
        CHECK(((host.runtimeFlags & ABI_RUNTIME_UBSAN) != 0) == (CONJURE_ABI_RUNTIME_UBSAN != 0));
        CHECK((host.runtimeFlags & ~(ABI_RUNTIME_DYNAMIC_CRT | ABI_RUNTIME_ASAN | ABI_RUNTIME_UBSAN)) == 0);
        CHECK(host.buildConfig == static_cast<std::uint32_t>(host_build_config()));
        CHECK(host.featureFlags == static_cast<std::uint32_t>(host_feature_flags()));
        CHECK(host.engineVersionMajor == host_engine_version().majorVersion);
        CHECK(host.engineVersionMinor == host_engine_version().minorVersion);
        CHECK(host.engineVersionPatch == host_engine_version().patchVersion);

        if constexpr (CONJURE_COMPILER_MSVC == 1) {
            CHECK(host.compilerId == ABI_COMPILER_MSVC);
            CHECK(host.standardLibraryId == ABI_STDLIB_MSVC);
        } else if constexpr (CONJURE_COMPILER_APPLECLANG == 1) {
            CHECK(host.compilerId == ABI_COMPILER_APPLECLANG);
        } else if constexpr (CONJURE_COMPILER_CLANG == 1) {
            CHECK(host.compilerId == ABI_COMPILER_CLANG);
        } else {
            CHECK(host.compilerId == ABI_COMPILER_GCC);
            CHECK(host.standardLibraryId == ABI_STDLIB_LIBSTDCXX);
        }
        if constexpr (CONJURE_ARCH_ARM64 == 1) {
            CHECK(host.architecture == ABI_ARCH_ARM64);
        } else {
            CHECK(host.architecture == ABI_ARCH_X64);
        }
        CHECK(&host_abi_signature() == &host);
    }

    TEST_CASE("module_abi - each field mutated on its own is rejected naming that field") {
        const conjure_module_abi_signature &host = host_abi_signature();
        for (const field_mutation &mutation : MUTATIONS) {
            CAPTURE(mutation.name);
            conjure_module_abi_signature mutated = host;
            mutation.mutate(mutated);
            const result<void> compared = compare_abi(mutated, host);
            CONJURE_CHECK_ERROR(compared, module_error::ABI_MISMATCH);
            const std::string &message = compared.get_error().message();
            CHECK(message.find(std::string("'") + std::string(mutation.name) + "'") != std::string::npos);
            CHECK(message.find("expected") != std::string::npos);
            CHECK(message.find("actual") != std::string::npos);
            CHECK(message.find("rebuild") != std::string::npos);
        }
    }

    TEST_CASE("module_abi - a smaller structSize is rejected before any other field is read") {
        conjure_module_abi_signature mutated = host_abi_signature();
        mutated.structSize = 8;
        mutated.magic = 0;
        mutated.abiVersion = 42;
        mutated.headerHash = 0;
        const result<void> compared = compare_abi(mutated, host_abi_signature());
        CONJURE_CHECK_ERROR(compared, module_error::ABI_MISMATCH);
        CHECK(compared.get_error().message().find("'structSize'") != std::string::npos);
        CHECK(compared.get_error().message().find("'magic'") == std::string::npos);
    }

    TEST_CASE("module_abi - a different header hash is rejected even when everything else matches") {
        conjure_module_abi_signature stale = host_abi_signature();
        stale.headerHash = ~stale.headerHash;
        const result<void> compared = compare_abi(stale, host_abi_signature());
        CONJURE_CHECK_ERROR(compared, module_error::ABI_MISMATCH);
        CHECK(compared.get_error().message().find("'headerHash'") != std::string::npos);
        CHECK(compared.get_error().message().find("0x") != std::string::npos);
    }

    TEST_CASE("module_abi - make_module_info carries the host signature and the given name") {
        static constexpr const char *NAME = "com.test.module";
        constexpr conjure_module_info info = make_module_info(NAME);
        CHECK(info.name == NAME);
        CHECK(std::string_view(info.name) == "com.test.module");
        CHECK(compare_abi(info.abi, host_abi_signature()).has_value());
        CHECK(info.abi.structSize == sizeof(conjure_module_abi_signature));
    }

    TEST_CASE("module_abi - the signature is a plain C struct of fixed-width fields") {
        CHECK(std::is_standard_layout_v<conjure_module_abi_signature>);
        CHECK(std::is_trivially_copyable_v<conjure_module_abi_signature>);
        CHECK(std::is_standard_layout_v<conjure_module_info>);
        CHECK(sizeof(conjure_module_abi_signature) == 13 * sizeof(std::uint32_t) + 4 + sizeof(std::uint64_t));
        CHECK(std::string_view(CONJURE_MODULE_DESCRIBE_SYMBOL) == "conjure_module_describe");
        CHECK(std::string_view(CONJURE_MODULE_ENTRY_SYMBOL) == "conjure_module_entry");
    }
}
