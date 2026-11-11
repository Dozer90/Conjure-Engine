#pragma once

#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The export pack, content.cpak (docs/design/05_assets.md 2.10, 3.8): a 64-byte header, a TOC
// sorted by uuid, a dependency table, a string table of asset paths and the payloads, each
// 16-byte aligned with identical payloads stored once. No timestamps, no absolute paths: two packs
// built from the same input are byte-identical.

namespace conjure::assets {

    /// <summary>
    /// Fixed header at offset 0 of the pack.
    /// </summary>
    struct pack_header {
        static constexpr std::array<char, 4> MAGIC = {'C', 'P', 'A', 'K'};
        static constexpr usize SIZE = 64;
        static constexpr usize PAYLOAD_ALIGNMENT = 16;

        u32 formatVersion = PACK_FORMAT_VERSION;
        u32 flags = 0;
        u32 entryCount = 0;
        u64 tocOffset = 0;
        u64 dependencyTableOffset = 0;
        u64 stringTableOffset = 0;
        u64 blobsOffset = 0;
        u64 contentHash = 0;    // hash_bytes of every byte after the header
    };

    /// <summary>
    /// One TOC entry (72 bytes on disk).
    /// </summary>
    struct pack_toc_entry {
        static constexpr usize SIZE = 72;

        asset_id id = {};
        asset_type_id type = asset_type_id::INVALID;
        u32 typeVersion = 0;
        u32 flags = 0;
        u64 blobOffset = 0;     // from the start of the file
        u64 blobSize = 0;
        u64 payloadHash = 0;
        u32 dependencyStart = 0;
        u32 dependencyCount = 0;
        u32 nameOffset = 0;
        u32 nameLength = 0;
    };

    /// <summary>
    /// What pack_writer::write produced.
    /// </summary>
    struct pack_stats {
        u32 entryCount = 0;
        u64 packBytes = 0;
        u32 duplicatesRemoved = 0;
        u64 contentHash = 0;
    };

    /// <summary>
    /// Collects blobs and writes a pack. Not thread-safe; used by the cooker on the main thread.
    /// </summary>
    class CONJURE_API pack_writer {
        private:
            struct entry {
                cooked_blob blob = {};
                std::string assetPath = {};
            };

            std::vector<entry> mEntries = {};

        public:
            pack_writer();
            ~pack_writer();

            pack_writer(const pack_writer &) = delete;
            pack_writer &operator=(const pack_writer &) = delete;
            pack_writer(pack_writer &&) noexcept = default;
            pack_writer &operator=(pack_writer &&) noexcept = default;

        public:
            /// <summary>
            /// Adds a blob (re-verifying its payload hash) with the asset path recorded in the
            /// string table. CACHE_CORRUPT when the hash does not match, ALREADY_EXISTS for a uuid
            /// already added.
            /// </summary>
            [[nodiscard]] core::result<void> add(cooked_blob blob, std::string_view assetPath);

            [[nodiscard]] usize size() const noexcept {
                return mEntries.size();
            }

            /// <summary>
            /// Serialises the pack: sorts by uuid, deduplicates payloads by (hash, size, bytes).
            /// </summary>
            [[nodiscard]] std::vector<std::byte> build(pack_stats &outStats) const;

            /// <summary>
            /// Writes the pack through tmp + rename.
            /// </summary>
            [[nodiscard]] core::result<pack_stats> write(
              core::vfs::file_system &fileSystem,
              std::string_view packPath
            ) const;

            /// <summary>
            /// The manifest: a JSON array of {uuid, type, name, size, hash, dependencies[]} sorted
            /// by uuid, canonical form, newline terminated.
            /// </summary>
            [[nodiscard]] std::string build_manifest() const;

            /// <summary>
            /// Writes build_manifest() through tmp + rename.
            /// </summary>
            [[nodiscard]] core::result<void> write_manifest(
              core::vfs::file_system &fileSystem,
              std::string_view manifestPath
            ) const;
    };

    /// <summary>
    /// Reads a pack: the header, TOC, dependency and string tables are loaded and validated at
    /// open(); payloads are read positionally on demand from any thread.
    /// </summary>
    class CONJURE_API pack_reader {
        private:
            std::unique_ptr<core::vfs::file> mFile = nullptr;   // positional reads when opened from a file system
            std::vector<std::byte> mBuffer = {};                 // the whole pack when opened from memory
            pack_header mHeader = {};
            std::vector<pack_toc_entry> mToc = {};
            std::vector<asset_id> mDependencies = {};
            std::string mStrings = {};
            u64 mFileSize = 0;
            bool mOpen = false;

        public:
            pack_reader();
            ~pack_reader();

            pack_reader(const pack_reader &) = delete;
            pack_reader &operator=(const pack_reader &) = delete;
            pack_reader(pack_reader &&) noexcept = default;
            pack_reader &operator=(pack_reader &&) noexcept = default;

        public:
            /// <summary>
            /// Opens and validates a pack: magic, format version, table bounds, TOC order and the
            /// content hash of everything after the header. PACK_CORRUPT on any failure, NOT_FOUND
            /// (assets) when the file is missing.
            /// </summary>
            [[nodiscard]] core::result<void> open(core::vfs::file_system &fileSystem, std::string_view packPath);

            /// <summary>
            /// Parses an in-memory pack the same way; payloads are then served from the buffer.
            /// </summary>
            [[nodiscard]] core::result<void> open_bytes(std::vector<std::byte> bytes);

            [[nodiscard]] bool is_open() const noexcept {
                return mOpen;
            }

            /// <summary>
            /// Binary search over the sorted TOC.
            /// </summary>
            [[nodiscard]] const pack_toc_entry *find(const asset_id &id) const noexcept;

            /// <summary>
            /// Reads an entry's payload and rebuilds the blob (header fields the pack does not
            /// store are zero). PACK_CORRUPT when the payload hash does not match.
            /// </summary>
            [[nodiscard]] core::result<cooked_blob> read(const pack_toc_entry &entry) const;

            [[nodiscard]] std::span<const pack_toc_entry> toc() const noexcept {
                return mToc;
            }

            /// <summary>
            /// The dependencies of an entry.
            /// </summary>
            [[nodiscard]] std::span<const asset_id> dependencies_of(const pack_toc_entry &entry) const noexcept;

            /// <summary>
            /// The asset path recorded for an entry.
            /// </summary>
            [[nodiscard]] std::string_view name_of(const pack_toc_entry &entry) const noexcept;

            /// <summary>
            /// The entry whose recorded asset path equals 'assetPath', or nullptr.
            /// </summary>
            [[nodiscard]] const pack_toc_entry *find_by_name(std::string_view assetPath) const noexcept;

            [[nodiscard]] u64 content_hash() const noexcept {
                return mHeader.contentHash;
            }

            [[nodiscard]] const pack_header &header() const noexcept {
                return mHeader;
            }

        private:
            [[nodiscard]] core::result<void> read_range(u64 offset, std::span<std::byte> out) const;
            [[nodiscard]] core::result<void> parse_tables();
    };
}
