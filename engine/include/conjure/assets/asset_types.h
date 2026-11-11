#pragma once

#include "conjure/assets/assets_error.h"

#include "conjure/core/api.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <chrono>
#include <concepts>
#include <string_view>

// Identity, states and constants shared by every asset header (docs/design/05_assets.md 2.1).
//
// An asset is identified by a uuid (asset_id); its type by the 64-bit FNV-1a hash of the type name
// (asset_type_id). The hash is implemented here so blob headers and pack tables never depend on the
// shape of another module's hashing; the constants are those of core::hash and the results agree.

namespace conjure::assets {

    /// <summary>
    /// Stable identity of an asset or sub-asset. asset_id{} (the null uuid) means "no asset".
    /// </summary>
    using asset_id = core::uuid;

    /// <summary>
    /// Identity of an asset type: make_asset_type_id("conjure.renderer.texture"). Stored in blob
    /// headers and pack tables, so the value never depends on compiler or platform.
    /// </summary>
    enum class asset_type_id : u64 {
        INVALID = 0
    };

    namespace detail {
        inline constexpr u64 FNV1A64_OFFSET_BASIS = 0xCBF29CE484222325ull;
        inline constexpr u64 FNV1A64_PRIME = 0x00000100000001B3ull;

        /// <summary>
        /// 64-bit FNV-1a of the bytes of a string, usable in constant expressions.
        /// </summary>
        [[nodiscard]] constexpr u64 fnv1a64(std::string_view text) noexcept {
            u64 hash = FNV1A64_OFFSET_BASIS;
            for (const char character : text) {
                hash = (hash ^ static_cast<u8>(character)) * FNV1A64_PRIME;
            }
            return hash;
        }
    }

    /// <summary>
    /// Hashes a type name into its id. An empty name yields INVALID.
    /// </summary>
    [[nodiscard]] constexpr asset_type_id make_asset_type_id(std::string_view name) noexcept {
        if (name.empty()) {
            return asset_type_id::INVALID;
        }
        return static_cast<asset_type_id>(detail::fnv1a64(name));
    }

    /// <summary>
    /// Lifecycle of a slot. LOADED and FAILED are terminal for dependency gating.
    /// </summary>
    enum class asset_state : u8 {
        UNLOADED = 0,
        QUEUED,
        LOADING,
        FINALISING,
        LOADED,
        RELOADING,
        FAILED
    };

    /// <summary>
    /// True for LOADED and FAILED: the states a dependent may finalise against.
    /// </summary>
    [[nodiscard]] constexpr bool is_terminal(asset_state state) noexcept {
        return state == asset_state::LOADED || state == asset_state::FAILED;
    }

    /// <summary>
    /// Upper-case name of a state ("LOADING").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(asset_state state) noexcept;

    /// <summary>
    /// Dispatch order of queued loads; FIFO within a level.
    /// </summary>
    enum class asset_load_priority : u8 {
        LOW = 0,
        NORMAL,
        HIGH
    };

    /// <summary>
    /// Number of priority levels.
    /// </summary>
    inline constexpr usize ASSET_LOAD_PRIORITY_COUNT = 3;

    /// <summary>
    /// Per-type behaviour switches set at registration.
    /// </summary>
    enum class asset_type_flags : u32 {
        NONE = 0,
        RELOAD_WITH_DEPENDENCIES = 1 << 0,   // hot reload reloads instances when a dependency changed
        EXCLUDE_FROM_PACK = 1 << 1,          // never cooked into content.cpak (script sources)
        PINNED_FALLBACK = 1 << 2             // the fallback is created at registration, not lazily
    };
    CONJURE_ENUM_FLAGS(asset_type_flags);

    /// <summary>
    /// Frames a retired payload stays alive after the publish that replaced it.
    /// </summary>
    inline constexpr u64 RETIRE_FRAMES = 2;

    /// <summary>
    /// Upper bound on registered asset types.
    /// </summary>
    inline constexpr usize MAX_ASSET_TYPES = 256;

    /// <summary>
    /// Default number of load jobs the manager keeps in flight.
    /// </summary>
    inline constexpr u32 DEFAULT_MAX_JOBS_IN_FLIGHT = 8;

    /// <summary>
    /// Default interval of the polling file watcher.
    /// </summary>
    inline constexpr std::chrono::milliseconds DEFAULT_WATCH_INTERVAL(500);

    /// <summary>
    /// Format versions written into blobs, packs and .meta files. Bumping one invalidates every
    /// artefact of that kind.
    /// </summary>
    inline constexpr u32 COOKED_BLOB_FORMAT_VERSION = 1;
    inline constexpr u32 PACK_FORMAT_VERSION = 1;
    inline constexpr u32 META_SCHEMA_VERSION = 1;

    /// <summary>
    /// Compile-time description of an asset type, specialised through CONJURE_DECLARE_ASSET_TYPE.
    /// Provides `static constexpr std::string_view NAME` and `static constexpr asset_type_id id()`.
    /// </summary>
    template <typename AssetType>
    struct asset_type_traits;

    /// <summary>
    /// True when AssetType was declared with CONJURE_DECLARE_ASSET_TYPE.
    /// </summary>
    template <typename AssetType>
    concept declared_asset_type = requires {
        { asset_type_traits<AssetType>::NAME } -> std::convertible_to<std::string_view>;
        { asset_type_traits<AssetType>::id() } -> std::convertible_to<asset_type_id>;
    };
}

// Declares the traits of an asset type. Use at global scope, after the type is declared:
//
//   CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::texture, "conjure.renderer.texture");
#define CONJURE_DECLARE_ASSET_TYPE(asset_type, type_name)                                                   \
    template <>                                                                                             \
    struct conjure::assets::asset_type_traits<asset_type> {                                                 \
        static constexpr std::string_view NAME = type_name;                                                 \
        [[nodiscard]] static constexpr conjure::assets::asset_type_id id() noexcept {                       \
            return conjure::assets::make_asset_type_id(NAME);                                               \
        }                                                                                                   \
    }
