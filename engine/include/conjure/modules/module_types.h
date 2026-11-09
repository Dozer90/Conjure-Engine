#pragma once

#include "conjure/core/api.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <compare>
#include <format>
#include <optional>
#include <string>
#include <string_view>

// Handles, enumerations and flag sets of the module system (docs/design/07_modules.md section 2).

namespace conjure::modules {

    /// <summary>
    /// 32-bit module handle. The generation increments on every unload so a stale id is detected
    /// instead of addressing the slot's next occupant.
    /// </summary>
    struct module_id {
        static constexpr u16 INVALID_INDEX = 0xFFFF;

        u16 index = INVALID_INDEX;
        u16 generation = 0;

        /// <summary>
        /// True for every id handed out by a module_manager; false for the default id.
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return index != INVALID_INDEX;
        }

        [[nodiscard]] constexpr auto operator<=>(const module_id &) const noexcept = default;
    };

    enum class module_kind : u8 {
        ENGINE,     // a built-in engine library registered as a manifest-only static module
        PLUGIN,     // a shared or static module that is not the game
        GAME        // the single project module, always loaded last
    };

    enum class module_host_kind : u8 {
        EDITOR,
        RUNTIME,
        TEST
    };

    enum class module_state : u8 {
        DISCOVERED,     // manifest accepted, not resolved yet
        SKIPPED,        // the manifest excludes the host platform; never loaded, never an error
        RESOLVED,       // part of a resolved load order, about to load
        LOADED,         // entry ran, every consumer attached
        FAILED,         // the last load attempt failed; last_error() says why; a retry is allowed
        UNLOADED,       // was LOADED once; may be loaded again
        FAULTED         // an unload went wrong half-way; the binary stays resident, nothing else works
    };

    enum class unload_policy : u8 {
        REFUSE_IF_IN_USE,   // consumers veto with IN_USE when anything still uses the module
        REMOVE_INSTANCES    // consumers strip every instance the module owns
    };

    enum class reload_scope : u8 {
        FULL,           // close and reopen the binary, every consumer detaches and restores
        MANAGED_ONLY    // only consumers with participates_in_managed_reload() take part
    };

    enum class build_config : u8 {
        DEBUG = 1,
        DEVELOPMENT = 2,
        DISTRIBUTION = 3
    };

    /// <summary>
    /// What a module entry function returns through the C boundary.
    /// </summary>
    enum class module_entry_status : u32 {
        OK = 0,
        FAILED = 1
    };

    enum class module_platform : u8 {
        WINDOWS,
        LINUX,
        MACOS
    };

    enum class module_architecture : u8 {
        X64,
        ARM64
    };

    enum class module_platform_flags : u8 {
        NONE = 0,
        WINDOWS = 1 << 0,
        LINUX = 1 << 1,
        MACOS = 1 << 2,
        ALL = WINDOWS | LINUX | MACOS
    };
    CONJURE_ENUM_FLAGS(module_platform_flags);

    /// <summary>
    /// Engine features a module may require ("requires" in module.json). Mirrors the
    /// CONJURE_WITH_* options and CONJURE_BUILD_EDITOR.
    /// </summary>
    enum class module_feature_flags : u32 {
        NONE = 0,
        RENDERER = 1 << 0,
        PHYSICS = 1 << 1,
        AUDIO = 1 << 2,
        SCRIPTING = 1 << 3,
        EDITOR = 1 << 4
    };
    CONJURE_ENUM_FLAGS(module_feature_flags);

    /// <summary>
    /// Names the bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;/ directory a module binary lives in.
    /// </summary>
    struct binary_triplet {
        module_platform platform = module_platform::WINDOWS;
        module_architecture architecture = module_architecture::X64;
        build_config config = build_config::DEVELOPMENT;

        [[nodiscard]] constexpr auto operator<=>(const binary_triplet &) const noexcept = default;
    };

    /*-----------------------------------------------------------------------*/
    // Host facts, all decided at compile time from platform_detect.h and the build options.

    [[nodiscard]] constexpr module_platform host_platform() noexcept {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            return module_platform::WINDOWS;
        } else if constexpr (CONJURE_PLATFORM_MACOS == 1) {
            return module_platform::MACOS;
        } else {
            return module_platform::LINUX;
        }
    }

    [[nodiscard]] constexpr module_architecture host_architecture() noexcept {
        if constexpr (CONJURE_ARCH_ARM64 == 1) {
            return module_architecture::ARM64;
        } else {
            return module_architecture::X64;
        }
    }

    [[nodiscard]] constexpr build_config host_build_config() noexcept {
        if constexpr (CONJURE_DEBUG == 1) {
            return build_config::DEBUG;
        } else if constexpr (CONJURE_DISTRIBUTION == 1) {
            return build_config::DISTRIBUTION;
        } else {
            return build_config::DEVELOPMENT;
        }
    }

    /// <summary>
    /// The flag bit of a platform.
    /// </summary>
    [[nodiscard]] constexpr module_platform_flags to_flag(module_platform platform) noexcept {
        switch (platform) {
            case module_platform::WINDOWS:
                return module_platform_flags::WINDOWS;
            case module_platform::LINUX:
                return module_platform_flags::LINUX;
            case module_platform::MACOS:
                return module_platform_flags::MACOS;
        }
        return module_platform_flags::NONE;
    }

    [[nodiscard]] constexpr module_platform_flags host_platform_flag() noexcept {
        return to_flag(host_platform());
    }

    /// <summary>
    /// The features compiled into this engine build (CONJURE_WITH_RENDERER, ..., CONJURE_BUILD_EDITOR).
    /// </summary>
    [[nodiscard]] constexpr module_feature_flags host_feature_flags() noexcept {
        module_feature_flags flags = module_feature_flags::NONE;
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        flags |= module_feature_flags::RENDERER;
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        flags |= module_feature_flags::PHYSICS;
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        flags |= module_feature_flags::AUDIO;
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        flags |= module_feature_flags::SCRIPTING;
#endif
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
        flags |= module_feature_flags::EDITOR;
#endif
        return flags;
    }

    /// <summary>
    /// The host platform and architecture combined with the given configuration.
    /// </summary>
    [[nodiscard]] constexpr binary_triplet host_binary_triplet(build_config config = host_build_config()) noexcept {
        return binary_triplet{host_platform(), host_architecture(), config};
    }

    /*-----------------------------------------------------------------------*/
    // Names: the manifest spellings ("windows", "x64", "debug", "renderer", "plugin") and the
    // enumerator names for diagnostics.

    [[nodiscard]] CONJURE_API std::string_view to_string(module_kind kind) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(module_host_kind host) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(module_state state) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(unload_policy policy) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(reload_scope scope) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(build_config config) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(module_platform platform) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(module_architecture architecture) noexcept;

    /// <summary>
    /// "windows-x64-debug".
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(const binary_triplet &triplet);

    /// <summary>
    /// "index:generation", or "invalid" for the default id.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(module_id id);

    /// <summary>
    /// The manifest names of the set bits joined with ", " ("renderer, physics"); "" for NONE.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(module_feature_flags flags);
    [[nodiscard]] CONJURE_API std::string to_string(module_platform_flags flags);

    /// <summary>
    /// Parses the manifest spellings; nullopt for anything else (case-sensitive).
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<module_kind> parse_module_kind(std::string_view text) noexcept;
    [[nodiscard]] CONJURE_API std::optional<module_platform> parse_module_platform(std::string_view text) noexcept;
    [[nodiscard]] CONJURE_API std::optional<module_feature_flags> parse_module_feature(std::string_view text) noexcept;
    [[nodiscard]] CONJURE_API std::optional<build_config> parse_build_config(std::string_view text) noexcept;
}

template <>
struct std::formatter<conjure::modules::module_id> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::modules::module_id &value, FormatContext &context) const {
        return std::formatter<std::string>::format(conjure::modules::to_string(value), context);
    }
};

template <>
struct std::formatter<conjure::modules::module_state> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(conjure::modules::module_state value, FormatContext &context) const {
        return std::formatter<std::string_view>::format(conjure::modules::to_string(value), context);
    }
};

template <>
struct std::formatter<conjure::modules::binary_triplet> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::modules::binary_triplet &value, FormatContext &context) const {
        return std::formatter<std::string>::format(conjure::modules::to_string(value), context);
    }
};
