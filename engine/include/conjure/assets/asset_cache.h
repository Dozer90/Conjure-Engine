#pragma once

#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <string>
#include <string_view>

// The cooked blob cache under <cacheRoot>/<platform>/blobs/<xx>/<uuid>.cab
// (docs/design/05_assets.md 2.9, 4.6). Every write goes to a .tmp sibling and is renamed into
// place; reads validate every field, and anything failing validation is reported as corrupt so
// the database re-imports it.

namespace conjure::assets {

    /// <summary>
    /// What a blob must match to be considered up to date.
    /// </summary>
    struct cache_expectation {
        asset_type_id type = asset_type_id::INVALID;
        u32 typeVersion = 0;
        u32 importerVersion = 0;
        u64 sourceHash = 0;
        u64 settingsHash = 0;
    };

    /// <summary>
    /// Blob storage for one platform. Safe to call from any thread: the file system does the
    /// synchronisation, and every write is atomic.
    /// </summary>
    class asset_cache {
        private:
            core::vfs::file_system *mFileSystem = nullptr;
            std::string mRoot = {};         // <cacheRoot>/<platform>
            std::string mBlobRoot = {};     // <cacheRoot>/<platform>/blobs
            std::string mPlatform = {};

        public:
            asset_cache(core::vfs::file_system &fileSystem, std::string_view cacheRoot, std::string_view platform);
            ~asset_cache();

            asset_cache(const asset_cache &) = delete;
            asset_cache &operator=(const asset_cache &) = delete;

        public:
            [[nodiscard]] std::string_view root() const noexcept {
                return mRoot;
            }

            [[nodiscard]] std::string_view platform() const noexcept {
                return mPlatform;
            }

            [[nodiscard]] core::vfs::file_system &vfs() const noexcept {
                return *mFileSystem;
            }

            /// <summary>
            /// Path of the blob of a uuid: blobs/<first two hex chars>/<uuid>.cab under the root.
            /// </summary>
            [[nodiscard]] std::string blob_path(const asset_id &id) const;

            /// <summary>
            /// True when a blob file exists for the uuid (not validated).
            /// </summary>
            [[nodiscard]] bool contains(const asset_id &id) const;

            /// <summary>
            /// Reads and validates only the header. CACHE_MISS when absent, CACHE_CORRUPT when the
            /// header is malformed or names another uuid.
            /// </summary>
            [[nodiscard]] core::result<cooked_blob_header> read_header(const asset_id &id) const;

            /// <summary>
            /// Reads and fully validates a blob. CACHE_MISS when absent, CACHE_CORRUPT on any
            /// validation failure (including a header that names another uuid).
            /// </summary>
            [[nodiscard]] core::result<cooked_blob> read(const asset_id &id) const;

            /// <summary>
            /// Writes a blob through <path>.tmp + rename. IO_ERROR when the file system refuses.
            /// </summary>
            [[nodiscard]] core::result<void> write(const cooked_blob &blob) const;

            /// <summary>
            /// True when the header matches the expectation and the current blob format.
            /// </summary>
            [[nodiscard]] static bool is_valid(
              const cooked_blob_header &header,
              const cache_expectation &expected
            ) noexcept;

            /// <summary>
            /// Deletes a blob. Succeeds when there was none.
            /// </summary>
            [[nodiscard]] core::result<void> remove(const asset_id &id) const;

            /// <summary>
            /// Deletes every *.tmp left by an interrupted write. Returns how many were removed.
            /// </summary>
            [[nodiscard]] core::result<u32> remove_temporaries() const;

            /// <summary>
            /// Deletes every blob of this platform.
            /// </summary>
            [[nodiscard]] core::result<void> clear() const;
    };
}
