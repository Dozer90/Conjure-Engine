#pragma once

#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

// The cooked blob: what importers produce, what the cache stores and what loaders consume
// (docs/design/05_assets.md 2.8). The header is 80 bytes, little-endian, written field by field;
// the dependency list follows it and the payload starts at the next 16-byte boundary.

namespace conjure::assets {

    /// <summary>
    /// Fixed header of a cooked blob.
    /// </summary>
    struct cooked_blob_header {
        static constexpr std::array<char, 4> MAGIC = {'C', 'J', 'A', 'B'};
        static constexpr usize SIZE = 80;
        static constexpr usize PAYLOAD_ALIGNMENT = 16;

        u32 formatVersion = COOKED_BLOB_FORMAT_VERSION;
        asset_type_id type = asset_type_id::INVALID;
        u32 typeVersion = 0;
        u32 importerVersion = 0;
        u64 sourceHash = 0;
        u64 settingsHash = 0;
        u64 payloadHash = 0;
        u64 payloadSize = 0;
        u32 dependencyCount = 0;
        u32 flags = 0;
        asset_id id = {};

        /// <summary>
        /// Offset of the payload from the start of the blob: the header and the dependency list
        /// rounded up to 16 bytes.
        /// </summary>
        [[nodiscard]] constexpr u64 payload_offset() const noexcept {
            const u64 unaligned = SIZE + u64{16} * dependencyCount;
            return (unaligned + (PAYLOAD_ALIGNMENT - 1)) & ~u64{PAYLOAD_ALIGNMENT - 1};
        }

        /// <summary>
        /// Total size of the serialised blob. Only meaningful for a consistent header (one built
        /// by make_cooked_blob or write_cooked_blob); validation of untrusted bytes compares the
        /// parts separately so a corrupt payloadSize cannot wrap the sum.
        /// </summary>
        [[nodiscard]] constexpr u64 total_size() const noexcept {
            return payload_offset() + payloadSize;
        }
    };

    /// <summary>
    /// A blob with its bytes. Owns everything it holds.
    /// </summary>
    struct cooked_blob {
        cooked_blob_header header = {};
        std::vector<asset_id> dependencies = {};
        std::vector<std::byte> payload = {};
    };

    /// <summary>
    /// 64-bit content hash used for payloads, source files and pack contents.
    /// </summary>
    [[nodiscard]] CONJURE_API u64 hash_bytes(std::span<const std::byte> bytes) noexcept;

    /// <summary>
    /// Builds a consistent blob from its parts: payloadHash, payloadSize and dependencyCount are
    /// derived from the payload and the dependency list.
    /// </summary>
    [[nodiscard]] CONJURE_API cooked_blob make_cooked_blob(
      cooked_blob_header header,
      std::vector<asset_id> dependencies,
      std::vector<std::byte> payload
    );

    /// <summary>
    /// Serialises a blob. The header's derived fields are recomputed first so the output is
    /// always self-consistent.
    /// </summary>
    [[nodiscard]] CONJURE_API std::vector<std::byte> write_cooked_blob(const cooked_blob &blob);

    /// <summary>
    /// Parses only the header. CACHE_CORRUPT for a short buffer, bad magic or unknown format.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<cooked_blob_header> parse_cooked_blob_header(
      std::span<const std::byte> bytes
    );

    /// <summary>
    /// Parses and validates a blob: magic, format version, declared sizes against the buffer and
    /// the payload hash. Anything wrong is CACHE_CORRUPT.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<cooked_blob> parse_cooked_blob(std::span<const std::byte> bytes);
}
