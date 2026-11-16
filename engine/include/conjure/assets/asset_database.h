#pragma once

#include "conjure/assets/asset_cache.h"
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The editor/dev asset database (docs/design/05_assets.md 2.7, 3.6, 4.6): mounts, scan, .meta
// creation, content-hash staleness, parallel import into the cache, dependency graphs and the
// db.bin snapshot. Every mutating call is main-thread only; lookups take a shared lock and are
// safe from any thread, including importer jobs.

namespace conjure::assets {

    /// <summary>
    /// Why a record may not be loadable.
    /// </summary>
    enum class asset_record_status : u8 {
        OK = 0,
        MISSING_SOURCE,         // the source file disappeared; a loaded payload stays loaded
        META_INVALID,           // the sidecar is corrupt; never overwritten
        DUPLICATE_UUID,         // another source (first in sorted path order) owns this uuid
        IMPORT_FAILED,          // the last import failed; the previous blob, if any, is kept
        NO_IMPORTER,            // no importer for the extension; browsable, never loadable
        CYCLIC_DEPENDENCY       // member of a dependency cycle found at import
    };

    /// <summary>
    /// Upper-case name of a status.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(asset_record_status status) noexcept;

    /// <summary>
    /// One asset or sub-asset known to the database.
    /// </summary>
    struct asset_record {
        asset_id id = {};
        asset_id parentId = {};                        // primary asset of a sub-asset; null for primaries
        asset_type_id type = asset_type_id::INVALID;   // known once imported (or from the snapshot)
        u32 mountIndex = 0;
        std::string relativePath = {};                 // forward slashes, relative to the mount root
        std::string subPath = {};                      // "" for the primary
        std::string importerId = {};
        std::string displayName = {};
        u32 importerVersion = 0;
        u64 sourceSize = 0;
        i64 sourceWriteTime = 0;                       // fast-path hint only
        u64 sourceHash = 0;                            // source bytes + side files, sorted by path
        u64 settingsHash = 0;
        u64 payloadHash = 0;                           // of the last blob produced; 0 when never imported
        asset_record_status status = asset_record_status::OK;

        /// <summary>
        /// True for a sub-asset.
        /// </summary>
        [[nodiscard]] bool is_sub_asset() const noexcept {
            return !subPath.empty();
        }
    };

    /// <summary>
    /// A root directory of sources, addressed as "<name>://<relative path>".
    /// </summary>
    struct asset_mount {
        std::string name = {};       // "assets", "engine"
        std::string rootPath = {};   // directory inside the database's file system
    };

    /// <summary>
    /// The pieces of "mount://relative/path#sub/path".
    /// </summary>
    struct asset_path_parts {
        std::string_view mount = {};
        std::string_view relativePath = {};
        std::string_view subPath = {};
    };

    /// <summary>
    /// Splits an asset path. nullopt when there is no "://" or the mount or path is empty.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<asset_path_parts> split_asset_path(std::string_view assetPath) noexcept;

    /// <summary>
    /// Builds "mount://relative#sub" (the '#' part only when subPath is not empty).
    /// </summary>
    [[nodiscard]] CONJURE_API std::string make_asset_path(
      std::string_view mount,
      std::string_view relativePath,
      std::string_view subPath = {}
    );

    /// <summary>
    /// What scan() found.
    /// </summary>
    struct scan_report {
        u32 filesScanned = 0;
        u32 recordsAdded = 0;
        u32 recordsRemoved = 0;
        u32 metasCreated = 0;
        u32 duplicateUuids = 0;
        u32 invalidMetas = 0;
        u32 missingImporters = 0;
        u32 hashed = 0;                      // sources whose hash was recomputed
        std::vector<asset_id> stale = {};    // primaries whose blobs must be (re)imported
    };

    /// <summary>
    /// One source that failed to import.
    /// </summary>
    struct import_failure {
        asset_id id = {};
        std::string assetPath = {};
        std::string message = {};
    };

    /// <summary>
    /// What import() did.
    /// </summary>
    struct import_report {
        u32 sourcesImported = 0;
        u32 assetsProduced = 0;
        u32 sourcesFailed = 0;
        u32 sourcesSkipped = 0;                       // unknown ids or records that cannot import
        std::vector<asset_id> changed = {};           // assets whose payload hash changed or that are new
        std::vector<import_failure> failures = {};
        std::vector<std::string> warnings = {};
    };

    /// <summary>
    /// The project's sources, their identities and their cooked state.
    /// </summary>
    class CONJURE_API asset_database {
        private:
            struct entry {
                asset_record record = {};
                std::vector<asset_id> dependencies = {};   // sorted, unique
                std::vector<std::string> sideFiles = {};   // asset paths, primaries only
                bool importerSwitched = false;             // meta names another importer; stale until imported
                u32 firstEdge = 0;                         // CSR into mForwardEdges
                u32 edgeCount = 0;
                u32 firstReverseEdge = 0;                  // CSR into mReverseEdges
                u32 reverseEdgeCount = 0;
            };

            struct source_job;

            core::vfs::file_system *mFileSystem = nullptr;
            importer_registry *mImporters = nullptr;
            const asset_type_registry *mTypes = nullptr;
            core::jobs::job_system *mJobs = nullptr;

            std::vector<asset_mount> mMounts = {};
            std::unique_ptr<asset_cache> mCache = nullptr;
            std::string mCacheRoot = {};
            std::string mPlatform = {};

            mutable std::shared_mutex mMutex = {};
            std::vector<entry> mEntries = {};                                      // guarded by mMutex
            core::flat_hash_map<asset_id, usize> mIndexById = {};                 // guarded by mMutex
            core::flat_hash_map<std::string, usize> mIndexByPath = {};             // "mount://rel#sub"
            core::flat_hash_map<std::string, std::vector<asset_id>> mSideFileOwners = {};
            std::vector<std::vector<usize>> mChildren = {};                        // sub-asset entries per entry
            std::vector<usize> mForwardEdges = {};
            std::vector<usize> mReverseEdges = {};
            std::vector<asset_id> mStale = {};

        public:
            /// <summary>
            /// 'jobs' (optional) parallelises source hashing during scan(); without it hashing runs
            /// inline. Imports take their job system as an argument.
            /// </summary>
            asset_database(
              core::vfs::file_system &fileSystem,
              importer_registry &importers,
              const asset_type_registry &types,
              core::jobs::job_system *jobs = nullptr
            );
            ~asset_database();

            asset_database(const asset_database &) = delete;
            asset_database &operator=(const asset_database &) = delete;

        public:
            /*---------------------------------------------------------------*/
            // Setup

            /// <summary>
            /// Adds a mount. ALREADY_EXISTS for a repeated name, INVALID_ARGUMENT for an empty one.
            /// Mounts are scanned in the order they were added.
            /// </summary>
            [[nodiscard]] core::result<void> add_mount(std::string_view mountName, std::string_view rootPath);

            [[nodiscard]] std::span<const asset_mount> mounts() const noexcept {
                return mMounts;
            }

            /// <summary>
            /// Opens the cache under cacheRoot for a platform and loads the db.bin snapshot when
            /// it is valid. Must precede scan().
            /// </summary>
            [[nodiscard]] core::result<void> open(std::string_view cacheRoot, std::string_view platform);

            [[nodiscard]] bool is_open() const noexcept {
                return mCache != nullptr;
            }

            /// <summary>
            /// The cache opened by open(), or nullptr.
            /// </summary>
            [[nodiscard]] const asset_cache *cache() const noexcept {
                return mCache.get();
            }

            [[nodiscard]] const asset_type_registry &types() const noexcept {
                return *mTypes;
            }

            [[nodiscard]] core::vfs::file_system &vfs() const noexcept {
                return *mFileSystem;
            }

            [[nodiscard]] std::string_view platform() const noexcept {
                return mPlatform;
            }

            /*---------------------------------------------------------------*/
            // Scan and import (main thread)

            /// <summary>
            /// Walks every mount in sorted order, creates missing .meta files, applies the
            /// duplicate-uuid policy, hashes changed sources, marks removed sources MISSING_SOURCE,
            /// computes the stale set, deletes leftover *.tmp blobs and saves the snapshot. A .meta
            /// that cannot be written leaves its record META_INVALID (retried by the next scan)
            /// instead of failing the scan. INVALID_STATE before open().
            /// </summary>
            [[nodiscard]] core::result<scan_report> scan();

            /// <summary>
            /// The stale primaries computed by the last scan() or import().
            /// </summary>
            [[nodiscard]] std::vector<asset_id> stale_ids() const;

            /// <summary>
            /// Imports every stale primary.
            /// </summary>
            [[nodiscard]] core::result<import_report> import_stale(core::jobs::job_system &jobs);

            /// <summary>
            /// Imports the sources of the given ids (sub-asset ids import their primary), one job
            /// per source, writes blobs into the cache and merges the results. A failing source is
            /// recorded IMPORT_FAILED and keeps its previous blob; the call itself fails only with
            /// INVALID_STATE before open().
            /// </summary>
            [[nodiscard]] core::result<import_report> import(
              std::span<const asset_id> ids,
              core::jobs::job_system &jobs
            );

            /*---------------------------------------------------------------*/
            // Editing (main thread)

            /// <summary>
            /// Registers a source that exists on disk but was not scanned yet, creating its .meta.
            /// NOT_FOUND when the file is missing, ALREADY_EXISTS when it is known, INVALID_ARGUMENT
            /// when the path leaves its mount ("assets://../x", an absolute path).
            /// </summary>
            [[nodiscard]] core::result<asset_id> add_file(std::string_view assetPath);

            /// <summary>
            /// Writes a new source and its .meta with the given importer, then registers it. The
            /// source is removed again when its .meta cannot be written; INVALID_ARGUMENT when the
            /// path leaves its mount or one of its names is not portable
            /// (core::vfs::path_utils::is_portable_name), before anything is written.
            /// </summary>
            [[nodiscard]] core::result<asset_id> create_asset(
              std::string_view assetPath,
              std::string_view importerId,
              std::span<const std::byte> initialBytes
            );

            /// <summary>
            /// Renames the source and its .meta; the uuid and every sub-asset uuid are kept.
            /// INVALID_ARGUMENT when the new path leaves its mount or one of its names is not
            /// portable (core::vfs::path_utils::is_portable_name), before anything is moved.
            /// </summary>
            [[nodiscard]] core::result<void> move_asset(const asset_id &id, std::string_view newAssetPath);

            /// <summary>
            /// Deletes the source, its .meta, its blobs and its records.
            /// </summary>
            [[nodiscard]] core::result<void> delete_asset(const asset_id &id);

            /// <summary>
            /// Marks a primary (and its sub-assets) MISSING_SOURCE without a full scan: the hot
            /// reload controller calls it when the watcher reports the source file removed, so
            /// later loads fail with NOT_FOUND while an already loaded payload stays loaded.
            /// NOT_FOUND for an unknown id; a sub-asset id marks its primary.
            /// </summary>
            [[nodiscard]] core::result<void> mark_source_missing(const asset_id &id);

            /// <summary>
            /// Reverses mark_source_missing() once the source file exists again (the file was
            /// moved back or re-created): the status returns to OK (or NO_IMPORTER), the size and
            /// write time are refreshed and the source is stale if its content changed. NOT_FOUND
            /// for an unknown id or when the file is still absent.
            /// </summary>
            [[nodiscard]] core::result<void> mark_source_present(const asset_id &id);

            /// <summary>
            /// Reads the .meta of a primary (or of a sub-asset's primary).
            /// </summary>
            [[nodiscard]] core::result<asset_meta> read_meta(const asset_id &id) const;

            /// <summary>
            /// Writes the .meta of a primary and refreshes the record (settings hash, importer).
            /// A changed uuid is refused with INVALID_ARGUMENT and logged: it would orphan references.
            /// A sub-asset the last import dropped (no blob) gets no record back from the meta. An
            /// imported primary switched to another importer is stale until its next import, even
            /// when both importers share a version and settings.
            /// </summary>
            [[nodiscard]] core::result<void> write_meta(const asset_id &id, const asset_meta &meta);

            /// <summary>
            /// Makes a registered importer the one a primary's .meta names ("import as"): the way to
            /// import a file with an importer that reads its format without claiming its extension, such
            /// as a .glb as collision geometry instead of a render model. The meta takes the importer's
            /// version and default settings, since the previous importer's settings mean nothing to it,
            /// and the asset is stale until its next import; its uuid is kept. Naming the importer the
            /// meta already has changes nothing. NO_IMPORTER for an id that is not registered; otherwise
            /// the errors of read_meta and write_meta.
            /// </summary>
            [[nodiscard]] core::result<void> set_importer(const asset_id &id, std::string_view importerId);

            /// <summary>
            /// Persists the records to <cacheRoot>/db.bin. Purely a cache; rebuilt when missing.
            /// </summary>
            [[nodiscard]] core::result<void> save_snapshot() const;

            /*---------------------------------------------------------------*/
            // Lookups (any thread, shared lock)

            [[nodiscard]] std::vector<asset_record> records() const;
            [[nodiscard]] usize record_count() const;
            [[nodiscard]] std::optional<asset_record> find(const asset_id &id) const;

            /// <summary>
            /// Allocation-free lookups for the load path (cache_asset_source asks on every job):
            /// whether a record exists, its cooked type (nullopt when unknown or never imported)
            /// and its status.
            /// </summary>
            [[nodiscard]] bool contains(const asset_id &id) const;
            [[nodiscard]] std::optional<asset_type_id> type_of(const asset_id &id) const;
            [[nodiscard]] std::optional<asset_record_status> status_of(const asset_id &id) const;

            [[nodiscard]] std::optional<asset_id> find_by_path(std::string_view assetPath) const;

            /// <summary>
            /// The primary whose source is the given file-system path ("assets/a.png").
            /// </summary>
            [[nodiscard]] std::optional<asset_id> find_by_file_path(std::string_view filePath) const;

            /// <summary>
            /// "mount://relative#sub" of a record.
            /// </summary>
            [[nodiscard]] std::optional<std::string> asset_path_of(const asset_id &id) const;

            /// <summary>
            /// File-system path of a record's source.
            /// </summary>
            [[nodiscard]] std::optional<std::string> file_path_of(const asset_id &id) const;

            [[nodiscard]] std::vector<asset_id> dependencies_of(const asset_id &id) const;
            [[nodiscard]] std::vector<asset_id> dependents_of(const asset_id &id) const;
            [[nodiscard]] std::vector<asset_id> owners_of_side_file(std::string_view assetPath) const;

            /// <summary>
            /// The roots plus everything they depend on, transitively, in uuid order. NOT_FOUND
            /// (assets) when a root is unknown.
            /// </summary>
            [[nodiscard]] core::result<std::vector<asset_id>> dependency_closure(std::span<const asset_id> roots) const;

            /// <summary>
            /// Transitive dependents including the roots, depth ascending, deduplicated.
            /// </summary>
            void collect_dependents(std::span<const asset_id> roots, std::vector<asset_id> &outOrdered) const;

        private:
            [[nodiscard]] std::string file_path_for(u32 mountIndex, std::string_view relativePath) const;
            [[nodiscard]] std::optional<u32> mount_index(std::string_view mountName) const noexcept;
            [[nodiscard]] const entry *find_entry_locked(const asset_id &id) const noexcept;
            [[nodiscard]] entry *find_entry_locked(const asset_id &id) noexcept;
            [[nodiscard]] core::result<void> ensure_open() const;
            [[nodiscard]] u64 hash_source(
              u32 mountIndex,
              std::string_view relativePath,
              std::span<const std::string> sideFiles
            ) const;
            [[nodiscard]] u64 hash_source_bytes(
              std::span<const std::byte> primaryBytes,
              std::span<const std::string> sideFiles
            ) const;
            void rebuild_indexes_locked();
            void rebuild_graphs_locked();
            void detect_cycles_locked();
            void recompute_stale();
            [[nodiscard]] core::result<void> set_source_status(const asset_id &id, bool present);
            [[nodiscard]] core::result<asset_id> register_source(
              u32 mountIndex,
              std::string_view relativePath,
              bool &outMetaCreated
            );
            [[nodiscard]] core::result<void> apply_import_results(std::vector<source_job> &jobs, import_report &report);
            [[nodiscard]] core::result<void> write_import_log(const import_report &report) const;
            [[nodiscard]] core::result<void> load_snapshot();
    };
}
