#include "conjure/modules/module_types.h"

#include <array>
#include <format>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::modules;

namespace {
    struct feature_name {
        module_feature_flags flag = module_feature_flags::NONE;
        std::string_view name = {};
    };

    constexpr std::array<feature_name, 5> FEATURE_NAMES = {{
        {module_feature_flags::RENDERER, "renderer"},
        {module_feature_flags::PHYSICS, "physics"},
        {module_feature_flags::AUDIO, "audio"},
        {module_feature_flags::SCRIPTING, "scripting"},
        {module_feature_flags::EDITOR, "editor"},
    }};

    constexpr std::array<module_platform, 3> PLATFORMS = {
        module_platform::WINDOWS,
        module_platform::LINUX,
        module_platform::MACOS,
    };
}


/*===========================================================================*/
/*===========================================================================*/
// Names

std::string_view conjure::modules::to_string(module_kind kind) noexcept {
    switch (kind) {
        case module_kind::ENGINE:
            return "engine";
        case module_kind::PLUGIN:
            return "plugin";
        case module_kind::GAME:
            return "game";
    }
    return "";
}

std::string_view conjure::modules::to_string(module_host_kind host) noexcept {
    switch (host) {
        case module_host_kind::EDITOR:
            return "editor";
        case module_host_kind::RUNTIME:
            return "runtime";
        case module_host_kind::TEST:
            return "test";
    }
    return "";
}

std::string_view conjure::modules::to_string(module_state state) noexcept {
    switch (state) {
        case module_state::DISCOVERED:
            return "DISCOVERED";
        case module_state::SKIPPED:
            return "SKIPPED";
        case module_state::RESOLVED:
            return "RESOLVED";
        case module_state::LOADED:
            return "LOADED";
        case module_state::FAILED:
            return "FAILED";
        case module_state::UNLOADED:
            return "UNLOADED";
        case module_state::FAULTED:
            return "FAULTED";
    }
    return "";
}

std::string_view conjure::modules::to_string(unload_policy policy) noexcept {
    switch (policy) {
        case unload_policy::REFUSE_IF_IN_USE:
            return "REFUSE_IF_IN_USE";
        case unload_policy::REMOVE_INSTANCES:
            return "REMOVE_INSTANCES";
    }
    return "";
}

std::string_view conjure::modules::to_string(reload_scope scope) noexcept {
    switch (scope) {
        case reload_scope::FULL:
            return "FULL";
        case reload_scope::MANAGED_ONLY:
            return "MANAGED_ONLY";
    }
    return "";
}

std::string_view conjure::modules::to_string(build_config config) noexcept {
    switch (config) {
        case build_config::DEBUG:
            return "debug";
        case build_config::DEVELOPMENT:
            return "development";
        case build_config::DISTRIBUTION:
            return "distribution";
    }
    return "";
}

std::string_view conjure::modules::to_string(module_platform platform) noexcept {
    switch (platform) {
        case module_platform::WINDOWS:
            return "windows";
        case module_platform::LINUX:
            return "linux";
        case module_platform::MACOS:
            return "macos";
    }
    return "";
}

std::string_view conjure::modules::to_string(module_architecture architecture) noexcept {
    switch (architecture) {
        case module_architecture::X64:
            return "x64";
        case module_architecture::ARM64:
            return "arm64";
    }
    return "";
}

std::string conjure::modules::to_string(const binary_triplet &triplet) {
    return std::format(
      "{}-{}-{}",
      to_string(triplet.platform),
      to_string(triplet.architecture),
      to_string(triplet.config)
    );
}

std::string conjure::modules::to_string(module_id id) {
    if (!id.is_valid()) {
        return "invalid";
    }
    return std::format("{}:{}", id.index, id.generation);
}

std::string conjure::modules::to_string(module_feature_flags flags) {
    std::string text = {};
    for (const feature_name &entry : FEATURE_NAMES) {
        if (!has_flag(flags, entry.flag)) {
            continue;
        }
        if (!text.empty()) {
            text.append(", ");
        }
        text.append(entry.name);
    }
    return text;
}

std::string conjure::modules::to_string(module_platform_flags flags) {
    std::string text = {};
    for (const module_platform platform : PLATFORMS) {
        if (!has_flag(flags, to_flag(platform))) {
            continue;
        }
        if (!text.empty()) {
            text.append(", ");
        }
        text.append(to_string(platform));
    }
    return text;
}


/*===========================================================================*/
/*===========================================================================*/
// Parsing

std::optional<module_kind> conjure::modules::parse_module_kind(std::string_view text) noexcept {
    if (text == "engine") {
        return module_kind::ENGINE;
    }
    if (text == "plugin") {
        return module_kind::PLUGIN;
    }
    if (text == "game") {
        return module_kind::GAME;
    }
    return std::nullopt;
}

std::optional<module_platform> conjure::modules::parse_module_platform(std::string_view text) noexcept {
    for (const module_platform platform : PLATFORMS) {
        if (text == to_string(platform)) {
            return platform;
        }
    }
    return std::nullopt;
}

std::optional<module_feature_flags> conjure::modules::parse_module_feature(std::string_view text) noexcept {
    for (const feature_name &entry : FEATURE_NAMES) {
        if (text == entry.name) {
            return entry.flag;
        }
    }
    return std::nullopt;
}

std::optional<build_config> conjure::modules::parse_build_config(std::string_view text) noexcept {
    if (text == "debug") {
        return build_config::DEBUG;
    }
    if (text == "development") {
        return build_config::DEVELOPMENT;
    }
    if (text == "distribution") {
        return build_config::DISTRIBUTION;
    }
    return std::nullopt;
}
