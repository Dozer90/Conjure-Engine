#pragma once

#include "conjure/bindings/bindings_error.inl"

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <cstdint>
#include <string_view>

// Status codes of the bindings-owned native entries and the main-thread last-error buffer
// (docs/design/15_bindings.md 2.2, B4, 3.4). Every int32_t entry returns >= 0 on success and
// -static_cast<int32_t>(bindings_error) on failure; uint64_t entries return 0 (NULL_ENTITY) on failure.
// Every failure goes through fail(), which formats "<entry>: <detail>" into a fixed 512-byte buffer
// (never allocating) that diagnostics_last_error hands to managed code when it asks.

namespace conjure::bindings {

    /// <summary>
    /// The status codes (bindings_error.inl, append-only).
    /// </summary>
    enum class bindings_error : u16 {
#define CONJURE_BINDINGS_STATUS_ENUMERATOR(name) name,
        CONJURE_BINDINGS_FOR_EACH_STATUS(CONJURE_BINDINGS_STATUS_ENUMERATOR)
#undef CONJURE_BINDINGS_STATUS_ENUMERATOR
        COUNT
    };

    /// <summary>
    /// Number of status codes (COUNT itself excluded).
    /// </summary>
    inline constexpr u32 BINDINGS_ERROR_COUNT = static_cast<u32>(bindings_error::COUNT);

    /// <summary>
    /// Capacity of the last-error message buffer, terminator included (at most 511 message bytes).
    /// </summary>
    inline constexpr u32 LAST_ERROR_CAPACITY = 512;

    /// <summary>
    /// The stable upper-case enumerator name of a code ("ENTITY_NOT_ALIVE"); "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view bindings_error_name(bindings_error code) noexcept;

    /// <summary>
    /// The ABI status of a code: -static_cast<int32_t>(code), so NONE is 0 (B4).
    /// </summary>
    [[nodiscard]] constexpr int32_t to_status(bindings_error code) noexcept {
        return -static_cast<int32_t>(code);
    }

    /// <summary>
    /// The code behind a negative status; NONE for a success value, INTERNAL for an unknown code (INT32_MIN
    /// included).
    /// </summary>
    [[nodiscard]] constexpr bindings_error from_status(int32_t status) noexcept {
        if (status >= 0) {
            return bindings_error::NONE;
        }
        // Negated in unsigned arithmetic: -INT32_MIN overflows int32_t, 0u - 0x80000000u is 0x80000000u.
        const u32 code = 0u - static_cast<u32>(status);
        if (code >= BINDINGS_ERROR_COUNT) {
            return bindings_error::INTERNAL;
        }
        return static_cast<bindings_error>(code);
    }

    /// <summary>
    /// Records "<entry>: <detail>" (detail may be empty) as the last error and returns to_status(code).
    /// Never allocates; a message longer than 511 bytes is truncated. Main thread only.
    /// </summary>
    [[nodiscard]] CONJURE_API int32_t fail(
      bindings_error code,
      std::string_view entry,
      std::string_view detail = {}
    ) noexcept;

    /// <summary>
    /// Maps a core::error of another module (2.2: ecs ENTITY_NOT_ALIVE / STRUCTURE_LOCKED / WRONG_THREAD /
    /// COMPONENT_NOT_PRESENT, assets NOT_FOUND / TYPE_MISMATCH, physics NO_BODY / NOT_A_CHARACTER,
    /// physics WRONG_BODY_KIND (to INVALID_ARGUMENT), scene INVALID_ENTITY / SCENE_NOT_LOADED /
    /// PREFAB_NOT_LOADED, everything else INTERNAL) and records its to_string() as the detail.
    /// </summary>
    [[nodiscard]] CONJURE_API int32_t fail(const core::error &failure, std::string_view entry) noexcept;

    /// <summary>
    /// The code a core::error maps to (the table of fail(const core::error &, ...)).
    /// </summary>
    [[nodiscard]] CONJURE_API bindings_error map_error(const core::error &failure) noexcept;

    /// <summary>
    /// The code of the last recorded failure (NONE after clear_last_error).
    /// </summary>
    [[nodiscard]] CONJURE_API bindings_error last_error_code() noexcept;

    /// <summary>
    /// The message of the last recorded failure (empty after clear_last_error). The view stays valid
    /// until the next fail() or clear_last_error().
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view last_error_message() noexcept;

    /// <summary>
    /// Forgets the last failure.
    /// </summary>
    CONJURE_API void clear_last_error() noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::bindings::bindings_error, "bindings", conjure::bindings::bindings_error_name);
