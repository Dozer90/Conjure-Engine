#include "conjure/scripting/script_host.h"

#include "scripting/hostfxr_runtime.h"
#include "scripting/scripting_internal.h"

#include "conjure/scripting/dotnet_script_host.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/json/document.h"

#include <array>
#include <string_view>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    constexpr std::array<std::string_view, 3> ANCHOR_FILES = {
        "Conjure.Bootstrap.dll",
        "Conjure.Bootstrap.runtimeconfig.json",
        "Conjure.Managed.dll",
    };
}

script_host::script_host() {}

script_host::~script_host() {}

std::unique_ptr<script_host> conjure::scripting::create_script_host(script_backend backend) {
    if (backend == script_backend::NULL_HOST) {
        return std::make_unique<null_script_host>();
    }
    return std::make_unique<dotnet_script_host>();
}

bool conjure::scripting::is_runtime_available() noexcept {
    static const bool sAvailable = []() noexcept {
        return hostfxr_runtime::locate_hostfxr({}, {}).has_value();
    }();
    return sAvailable;
}

host_capabilities conjure::scripting::query_host_capabilities() noexcept {
    // Both paths are implemented on every desktop platform; a self-contained layout still needs the
    // runtime pack at publish time, which the export dialog reports through the publish result.
    host_capabilities capabilities = {};
    capabilities.frameworkDependent = true;
    capabilities.selfContained = true;
    return capabilities;
}

core::result<host_mode> conjure::scripting::detect_host_mode(const std::filesystem::path &managedDirectory) {
    const std::filesystem::path configPath = managedDirectory / "Conjure.Bootstrap.runtimeconfig.json";
    core::result<core::json::document> parsed = core::json::document::parse_file(configPath);
    if (!parsed.has_value()) {
        return core::make_error(
          scripting_error::RUNTIME_CONFIG_INVALID,
          "'{}': {}",
          detail::path_to_utf8(configPath),
          parsed.get_error().message()
        );
    }
    const core::json::value options = parsed.value().root()["runtimeOptions"];
    if (!options.is_object()) {
        return core::make_error(
          scripting_error::RUNTIME_CONFIG_INVALID,
          "'{}' has no runtimeOptions object",
          detail::path_to_utf8(configPath)
        );
    }
    if (options["includedFrameworks"].is_array()) {
        return host_mode::SELF_CONTAINED;
    }
    if (!options["framework"].is_object() && !options["frameworks"].is_array()) {
        return core::make_error(
          scripting_error::RUNTIME_CONFIG_INVALID,
          "'{}' names neither a framework nor includedFrameworks",
          detail::path_to_utf8(configPath)
        );
    }
    return host_mode::FRAMEWORK_DEPENDENT;
}

core::result<void> conjure::scripting::validate_managed_directory(const std::filesystem::path &managedDirectory) {
    if (managedDirectory.empty()) {
        return core::make_error(scripting_error::MANAGED_SDK_MISSING, "no managed directory configured");
    }
    for (const std::string_view anchor : ANCHOR_FILES) {
        std::error_code probe = {};
        if (!std::filesystem::is_regular_file(managedDirectory / anchor, probe)) {
            return core::make_error(
              scripting_error::MANAGED_SDK_MISSING,
              "'{}' is missing from '{}'",
              anchor,
              detail::path_to_utf8(managedDirectory)
            );
        }
    }
    return core::result<void>::success();
}

std::string_view conjure::scripting::to_string(script_host_status status) noexcept {
    switch (status) {
        case script_host_status::UNINITIALISED:
            return "UNINITIALISED";
        case script_host_status::READY:
            return "READY";
        case script_host_status::DISABLED:
            return "DISABLED";
    }
    return "UNINITIALISED";
}

std::string_view conjure::scripting::to_string(script_backend backend) noexcept {
    switch (backend) {
        case script_backend::DOTNET:
            return "DOTNET";
        case script_backend::NULL_HOST:
            return "NULL_HOST";
    }
    return "NULL_HOST";
}

std::string_view conjure::scripting::to_string(host_mode mode) noexcept {
    switch (mode) {
        case host_mode::FRAMEWORK_DEPENDENT:
            return "FRAMEWORK_DEPENDENT";
        case host_mode::SELF_CONTAINED:
            return "SELF_CONTAINED";
    }
    return "FRAMEWORK_DEPENDENT";
}
