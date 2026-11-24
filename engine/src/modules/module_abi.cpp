#include "conjure/modules/module_abi.h"

#include "conjure/modules/module_error.h"

#include <cstdint>
#include <format>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    constexpr std::string_view REBUILD_HINT = "rebuild the module against this engine build";

    error mismatch(std::string_view field, std::uint64_t expected, std::uint64_t actual) {
        return core::make_error(
          module_error::ABI_MISMATCH,
          "ABI field '{}' differs: expected {}, actual {}; {}",
          field,
          expected,
          actual,
          REBUILD_HINT
        );
    }

    error mismatch_hex(std::string_view field, std::uint64_t expected, std::uint64_t actual) {
        return core::make_error(
          module_error::ABI_MISMATCH,
          "ABI field '{}' differs: expected 0x{:X}, actual 0x{:X}; {}",
          field,
          expected,
          actual,
          REBUILD_HINT
        );
    }
}

const conjure_module_abi_signature &conjure::modules::host_abi_signature() noexcept {
    static constexpr conjure_module_abi_signature HOST = make_abi_signature();
    return HOST;
}

result<void> conjure::modules::compare_abi(
  const conjure_module_abi_signature &moduleSignature,
  const conjure_module_abi_signature &hostSignature
) {
    // structSize first: a module built against another layout must be refused before any field
    // that may not exist in its struct is read.
    if (moduleSignature.structSize != hostSignature.structSize) {
        return mismatch("structSize", hostSignature.structSize, moduleSignature.structSize);
    }
    if (moduleSignature.magic != hostSignature.magic) {
        return mismatch_hex("magic", hostSignature.magic, moduleSignature.magic);
    }
    if (moduleSignature.abiVersion != hostSignature.abiVersion) {
        return mismatch("abiVersion", hostSignature.abiVersion, moduleSignature.abiVersion);
    }
    if (moduleSignature.compilerId != hostSignature.compilerId) {
        return mismatch("compilerId", hostSignature.compilerId, moduleSignature.compilerId);
    }
    if (moduleSignature.compilerMajor != hostSignature.compilerMajor) {
        return mismatch("compilerMajor", hostSignature.compilerMajor, moduleSignature.compilerMajor);
    }
    if (moduleSignature.standardLibraryId != hostSignature.standardLibraryId) {
        return mismatch("standardLibraryId", hostSignature.standardLibraryId, moduleSignature.standardLibraryId);
    }
    if (moduleSignature.buildConfig != hostSignature.buildConfig) {
        return mismatch("buildConfig", hostSignature.buildConfig, moduleSignature.buildConfig);
    }
    if (moduleSignature.architecture != hostSignature.architecture) {
        return mismatch("architecture", hostSignature.architecture, moduleSignature.architecture);
    }
    if (moduleSignature.runtimeFlags != hostSignature.runtimeFlags) {
        return mismatch_hex("runtimeFlags", hostSignature.runtimeFlags, moduleSignature.runtimeFlags);
    }
    if (moduleSignature.featureFlags != hostSignature.featureFlags) {
        return mismatch_hex("featureFlags", hostSignature.featureFlags, moduleSignature.featureFlags);
    }
    if (moduleSignature.engineVersionMajor != hostSignature.engineVersionMajor) {
        return mismatch("engineVersionMajor", hostSignature.engineVersionMajor, moduleSignature.engineVersionMajor);
    }
    if (moduleSignature.engineVersionMinor != hostSignature.engineVersionMinor) {
        return mismatch("engineVersionMinor", hostSignature.engineVersionMinor, moduleSignature.engineVersionMinor);
    }
    if (moduleSignature.engineVersionPatch != hostSignature.engineVersionPatch) {
        return mismatch("engineVersionPatch", hostSignature.engineVersionPatch, moduleSignature.engineVersionPatch);
    }
    if (moduleSignature.headerHash != hostSignature.headerHash) {
        return mismatch_hex("headerHash", hostSignature.headerHash, moduleSignature.headerHash);
    }
    return {};
}
