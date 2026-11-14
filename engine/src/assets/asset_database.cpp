#include "conjure/assets/asset_database.h"
#include "conjure/assets/log_category.h"
#include "assets/database_snapshot.h"
#include "assets/fnv1a.h"
#include "assets/path_filters.h"

#include "conjure/core/assert.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/jobs/parallel_for.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <deque>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

namespace path_utils = conjure::core::vfs::path_utils;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    constexpr std::string_view SNAPSHOT_FILE = "db.bin";
    constexpr std::string_view IMPORT_LOG_FILE = "import_log.txt";

    std::vector<std::byte> to_bytes(std::string_view text) {
        std::vector<std::byte> bytes(text.size());
        for (usize index = 0; index < text.size(); ++index) {
            bytes[index] = static_cast<std::byte>(text[index]);
        }
        return bytes;
    }

    core::error not_open() {
        return core::error(core::error_code::INVALID_STATE, "asset_database::open() has not been called");
    }

    core::error io_error(std::string_view what, std::string_view path, const core::error &cause) {
        return make_asset_error(
          asset_error_code::IO_ERROR,
          std::format("cannot {} '{}': {}", what, path, cause.message())
        );
    }

    /// <summary>
    /// A uuid that is never written anywhere: records without a usable uuid (invalid or duplicate
    /// metas) need an index key for the lifetime of the database.
    /// </summary>
    asset_id transient_id() {
        return asset_id::generate();
    }

    /// <summary>
    /// Records that can never import until a scan() finds them in another state. NO_IMPORTER is
    /// not among them: the importer is taken from the .meta at import time, so a meta edit that
    /// names a registered importer brings the record back without a scan.
    /// </summary>
    bool status_blocks_import(asset_record_status status) noexcept {
        return status == asset_record_status::META_INVALID || status == asset_record_status::DUPLICATE_UUID ||
          status == asset_record_status::MISSING_SOURCE;
    }

    bool status_from_previous_import(asset_record_status status) noexcept {
        return status == asset_record_status::IMPORT_FAILED || status == asset_record_status::CYCLIC_DEPENDENCY;
    }

    /// <summary>
    /// Records whose uuid is a transient one (never persisted).
    /// </summary>
    bool is_transient(asset_record_status status) noexcept {
        return status == asset_record_status::META_INVALID || status == asset_record_status::DUPLICATE_UUID;
    }

    /// <summary>
    /// A normalised path relative to a mount root that names a file inside it. path_utils::join
    /// returns an absolute right-hand side unchanged and normalize keeps a leading "..", so either
    /// would reach outside the mount.
    /// </summary>
    bool stays_in_mount(std::string_view relative) noexcept {
        return !relative.empty() && relative != "." && relative != ".." && !core::str::starts_with(relative, "../") &&
          !path_utils::is_absolute(relative);
    }

    core::error leaves_mount(std::string_view assetPath) {
        return core::error(core::error_code::INVALID_ARGUMENT, std::format("'{}' leaves its mount", assetPath));
    }
}

std::string_view conjure::assets::to_string(asset_record_status status) noexcept {
    switch (status) {
        case asset_record_status::OK:
            return "OK";
        case asset_record_status::MISSING_SOURCE:
            return "MISSING_SOURCE";
        case asset_record_status::META_INVALID:
            return "META_INVALID";
        case asset_record_status::DUPLICATE_UUID:
            return "DUPLICATE_UUID";
        case asset_record_status::IMPORT_FAILED:
            return "IMPORT_FAILED";
        case asset_record_status::NO_IMPORTER:
            return "NO_IMPORTER";
        case asset_record_status::CYCLIC_DEPENDENCY:
            return "CYCLIC_DEPENDENCY";
    }
    return "UNKNOWN";
}

std::optional<asset_path_parts> conjure::assets::split_asset_path(std::string_view assetPath) noexcept {
    const usize scheme = assetPath.find("://");
    if (scheme == std::string_view::npos || scheme == 0) {
        return std::nullopt;
    }
    asset_path_parts parts = {};
    parts.mount = assetPath.substr(0, scheme);
    std::string_view rest = assetPath.substr(scheme + 3);
    const usize hash = rest.find('#');
    if (hash != std::string_view::npos) {
        parts.subPath = rest.substr(hash + 1);
        rest = rest.substr(0, hash);
    }
    if (rest.empty()) {
        return std::nullopt;
    }
    parts.relativePath = rest;
    return parts;
}

std::string conjure::assets::make_asset_path(
  std::string_view mount,
  std::string_view relativePath,
  std::string_view subPath
) {
    std::string path = {};
    path.reserve(mount.size() + 3 + relativePath.size() + 1 + subPath.size());
    path.append(mount);
    path.append("://");
    path.append(relativePath);
    if (!subPath.empty()) {
        path.push_back('#');
        path.append(subPath);
    }
    return path;
}

/*===========================================================================*/
/*===========================================================================*/
// Per-source import job

struct asset_database::source_job {
    struct produced {
        asset_id id = {};
        std::string subPath = {};
        std::string displayName = {};
        asset_type_id type = asset_type_id::INVALID;
        std::vector<asset_id> dependencies = {};
        u64 payloadHash = 0;
    };

    usize entryIndex = 0;
    asset_id id = {};
    std::string assetPath = {};
    std::string filePath = {};
    u32 mountIndex = 0;
    std::string relativePath = {};
    asset_meta meta = {};
    const asset_importer *importer = nullptr;
    std::unique_ptr<sub_asset_table> subAssets = nullptr;   // not movable itself (mutex)
    std::vector<produced> outputs = {};
    std::vector<std::string> sideFiles = {};
    std::vector<std::string> warnings = {};
    std::optional<core::error> failure = std::nullopt;
    u64 sourceHash = 0;
    u32 importerVersion = 0;
};

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

asset_database::asset_database(
  core::vfs::file_system &fileSystem,
  importer_registry &importers,
  const asset_type_registry &types,
  core::jobs::job_system *jobs
) :
    mFileSystem(&fileSystem),
    mImporters(&importers),
    mTypes(&types),
    mJobs(jobs) {}

asset_database::~asset_database() {}

/*===========================================================================*/
/*===========================================================================*/
// Setup

core::result<void> asset_database::add_mount(std::string_view mountName, std::string_view rootPath) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::add_mount is main-thread only");
    if (
      mountName.empty() || mountName.find("://") != std::string_view::npos || mountName.find(
        '/'
      ) != std::string_view::npos
    ) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("'{}' is not a valid mount name", mountName)
        );
    }
    if (mount_index(mountName).has_value()) {
        return core::error(core::error_code::ALREADY_EXISTS, std::format("mount '{}' already exists", mountName));
    }
    const std::unique_lock lock(mMutex);
    mMounts.push_back(asset_mount{std::string(mountName), path_utils::normalize(rootPath)});
    return core::status::success();
}

core::result<void> asset_database::open(std::string_view cacheRoot, std::string_view platform) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::open is main-thread only");
    if (platform.empty()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "platform must not be empty");
    }
    mCacheRoot = path_utils::normalize(cacheRoot);
    mPlatform = std::string(platform);
    mCache = std::make_unique<asset_cache>(*mFileSystem, mCacheRoot, mPlatform);

    core::result<void> created = mFileSystem->create_directories(mCacheRoot);
    if (!created.has_value()) {
        mCache.reset();
        return io_error("create cache root", mCacheRoot, created.get_error());
    }

    core::result<void> loaded = load_snapshot();
    if (!loaded.has_value()) {
        CONJURE_LOG_INFO(LOG_ASSETS, "asset database snapshot not used: {}", loaded.get_error().message());
    }
    return core::status::success();
}

/*===========================================================================*/
/*===========================================================================*/
// Scan

core::result<scan_report> asset_database::scan() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::scan is main-thread only");
    CONJURE_TRY(ensure_open());

    scan_report report = {};

    // The previous entries (snapshot or last scan) are looked up by uuid and, for records
    // without a persisted uuid, by path, so hashes, types and graphs survive a rescan.
    std::vector<entry> previous = {};
    {
        const std::shared_lock lock(mMutex);
        previous = mEntries;
    }
    core::flat_hash_map<asset_id, usize> previousById = {};
    core::flat_hash_map<std::string, usize> previousByPath = {};
    for (usize index = 0; index < previous.size(); ++index) {
        previousById.try_emplace(previous[index].record.id, index);
        const asset_record &record = previous[index].record;
        previousByPath.try_emplace(
          make_asset_path(mMounts[record.mountIndex].name, record.relativePath, record.subPath),
          index
        );
    }

    struct scanned_source {
        u32 mountIndex = 0;
        std::string relativePath = {};
        core::vfs::file_stat stat = {};
    };
    std::vector<scanned_source> sources = {};
    for (u32 mountIndex = 0; mountIndex < mMounts.size(); ++mountIndex) {
        const asset_mount &mount = mMounts[mountIndex];
        if (!mFileSystem->exists(mount.rootPath)) {
            continue;
        }
        const std::string prefix = mount.rootPath.empty() ? std::string() : mount.rootPath + "/";
        core::result<void> enumerated = mFileSystem->enumerate(
          mount.rootPath,
          core::vfs::enumerate_flags::RECURSIVE | core::vfs::enumerate_flags::FILES,
          [&](std::string_view path, const core::vfs::file_stat &stat) {
              if (!stat.isRegular || is_meta_path(path) || detail::is_temporary_path(path)) {
                  return;
              }
              std::string_view relative = path;
              if (core::str::starts_with(relative, prefix)) {
                  relative.remove_prefix(prefix.size());
              }
              sources.push_back(scanned_source{mountIndex, std::string(relative), stat});
          }
        );
        if (!enumerated.has_value()) {
            return io_error("enumerate mount", mount.rootPath, enumerated.get_error());
        }
    }
    std::sort(sources.begin(), sources.end(), [](const scanned_source &a, const scanned_source &b) {
        if (a.mountIndex != b.mountIndex) {
            return a.mountIndex < b.mountIndex;
        }
        return a.relativePath < b.relativePath;
    });
    report.filesScanned = static_cast<u32>(sources.size());
    core::flat_hash_set<std::string> scannedPaths = {};
    scannedPaths.reserve(sources.size());
    for (const scanned_source &source : sources) {
        scannedPaths.insert(make_asset_path(mMounts[source.mountIndex].name, source.relativePath));
    }

    std::vector<entry> next = {};
    core::flat_hash_map<asset_id, std::string> claimed = {};   // uuid -> first path that declared it
    std::vector<usize> toHash = {};                             // indices into 'next' (primaries)

    // A meta this scan created at a path whose asset had a persisted uuid. D1 makes the source a
    // new asset; a .meta lost through version control is the usual cause, so the identity that
    // broke is named after the walk unless its uuid turned up elsewhere (the source moved).
    struct replaced_identity {
        std::string assetPath = {};
        asset_id fresh = {};
        asset_id previous = {};
    };
    std::vector<replaced_identity> replaced = {};

    auto previous_for = [&](const asset_id &id, const std::string &assetPath) -> const entry * {
        const auto byId = previousById.find(id);
        if (byId != previousById.end()) {
            return &previous[byId->second];
        }
        const auto byPath = previousByPath.find(assetPath);
        if (byPath != previousByPath.end()) {
            return &previous[byPath->second];
        }
        return nullptr;
    };

    // True when 'id' belongs to a known primary that has left its old place: its record is
    // MISSING_SOURCE, its source is gone, or the meta there no longer declares the uuid. The meta
    // is read only in that last case, which needs a uuid change at a known path to get here.
    auto moved_here = [&](const asset_id &id, const std::string &assetPath) -> bool {
        const auto known = previousById.find(id);
        if (known == previousById.end()) {
            return false;
        }
        const asset_record &knownRecord = previous[known->second].record;
        const std::string knownPath = make_asset_path(mMounts[knownRecord.mountIndex].name, knownRecord.relativePath);
        if (knownRecord.is_sub_asset() || knownPath == assetPath) {
            return false;
        }
        if (knownRecord.status == asset_record_status::MISSING_SOURCE || !scannedPaths.contains(knownPath)) {
            return true;
        }
        const std::string knownMeta = meta_path_of(file_path_for(knownRecord.mountIndex, knownRecord.relativePath));
        const core::result<asset_meta> declared = read_asset_meta(*mFileSystem, knownMeta);
        return !declared.has_value() || declared.value().id != id;
    };

    for (const scanned_source &source : sources) {
        const asset_mount &mount = mMounts[source.mountIndex];
        const std::string filePath = file_path_for(source.mountIndex, source.relativePath);
        const std::string metaPath = meta_path_of(filePath);
        const std::string assetPath = make_asset_path(mount.name, source.relativePath);

        entry primary = {};
        primary.record.mountIndex = source.mountIndex;
        primary.record.relativePath = source.relativePath;
        primary.record.displayName = std::string(path_utils::stem(source.relativePath));
        primary.record.sourceSize = source.stat.size;
        primary.record.sourceWriteTime = static_cast<i64>(source.stat.writeTimeNanoseconds);

        core::result<asset_meta> metaRead = read_asset_meta(*mFileSystem, metaPath);
        std::optional<asset_meta> meta = std::nullopt;
        bool metaCreated = false;
        if (metaRead.has_value()) {
            meta = std::move(metaRead).value();
        } else if (metaRead.get_error().is(asset_error_code::NOT_FOUND)) {
            asset_meta fresh = {};
            fresh.id = asset_id::generate();
            const asset_importer *importer = mImporters->find_for_path(source.relativePath);
            if (importer != nullptr) {
                fresh.importerId = std::string(importer->id());
                fresh.importerVersion = importer->version();
                fresh.settings = importer->default_settings();
            }
            core::result<void> written = write_asset_meta(*mFileSystem, metaPath, fresh);
            if (written.has_value()) {
                ++report.metasCreated;
                metaCreated = true;
                const auto lost = previousByPath.find(assetPath);
                if (lost != previousByPath.end()) {
                    const asset_record &previousRecord = previous[lost->second].record;
                    if (!is_transient(previousRecord.status) &&
                        previousRecord.status != asset_record_status::MISSING_SOURCE) {
                        replaced.push_back(replaced_identity{assetPath, fresh.id, previousRecord.id});
                    }
                }
                meta = std::move(fresh);
            } else {
                // A read-only meta, folder or file system degrades this record only (design section
                // 6); its meta is still missing, so a later scan tries the write again.
                CONJURE_LOG_WARN(LOG_ASSETS, "{}: cannot write its meta: {}", assetPath, written.get_error().message());
            }
        } else {
            CONJURE_LOG_WARN(LOG_ASSETS, "{}: {}", assetPath, metaRead.get_error().message());
        }
        if (!meta.has_value()) {
            const entry *old = previous_for(asset_id(), assetPath);
            const bool keepTransient = old != nullptr && old->record.status == asset_record_status::META_INVALID;
            primary.record.id = keepTransient ? old->record.id : transient_id();
            primary.record.status = asset_record_status::META_INVALID;
            ++report.invalidMetas;
            next.push_back(std::move(primary));
            continue;
        }

        // A uuid edited by hand in an existing meta would orphan every reference: the in-memory
        // uuid is kept and the file is left alone for the user to fix. Two cases are not edits
        // (D1, the meta is the identity): a meta this scan created is a new asset, and the uuid of
        // a known asset whose source or meta left its old place is that asset moved here.
        const auto previousAtPath = previousByPath.find(assetPath);
        if (previousAtPath != previousByPath.end() && !metaCreated) {
            const asset_record &oldRecord = previous[previousAtPath->second].record;
            const bool persistedId = !is_transient(oldRecord.status) &&
              oldRecord.status != asset_record_status::MISSING_SOURCE;
            if (persistedId && oldRecord.id != meta->id && !moved_here(meta->id, assetPath)) {
                CONJURE_LOG_ERROR(
                  LOG_ASSETS,
                  "{}: meta uuid changed from {} to {}; keeping {}",
                  assetPath,
                  oldRecord.id,
                  meta->id,
                  oldRecord.id
                );
                meta->id = oldRecord.id;
            }
        }

        const auto claim = claimed.find(meta->id);
        if (claim != claimed.end()) {
            CONJURE_LOG_WARN(
              LOG_ASSETS,
              "{} declares uuid {} already owned by {}; excluded",
              assetPath,
              meta->id,
              claim->second
            );
            const entry *old = previous_for(asset_id(), assetPath);
            const bool keepTransient = old != nullptr && old->record.status == asset_record_status::DUPLICATE_UUID;
            primary.record.id = keepTransient ? old->record.id : transient_id();
            primary.record.status = asset_record_status::DUPLICATE_UUID;
            primary.record.importerId = meta->importerId;
            ++report.duplicateUuids;
            next.push_back(std::move(primary));
            continue;
        }
        claimed.try_emplace(meta->id, assetPath);

        // The importer named in the meta wins; a meta without one picks up an importer that was
        // registered since it was written.
        const asset_importer *importer = meta->importerId.empty() ? nullptr : mImporters->find(meta->importerId);
        if (importer == nullptr && meta->importerId.empty()) {
            importer = mImporters->find_for_path(source.relativePath);
            if (importer != nullptr) {
                asset_meta pinned = *meta;
                pinned.importerId = std::string(importer->id());
                pinned.importerVersion = importer->version();
                pinned.settings = importer->default_settings();
                core::result<void> written = write_asset_meta(*mFileSystem, metaPath, pinned);
                if (written.has_value()) {
                    *meta = std::move(pinned);
                } else {
                    // import() takes the importer from the meta on disk, which still names none:
                    // the record says so (NO_IMPORTER) until a later scan records it.
                    CONJURE_LOG_WARN(
                      LOG_ASSETS,
                      "{}: cannot record importer {} in its meta: {}",
                      assetPath,
                      importer->id(),
                      written.get_error().message()
                    );
                    importer = nullptr;
                }
            }
        }

        primary.record.id = meta->id;
        primary.record.importerId = meta->importerId;
        primary.record.importerVersion = importer != nullptr ? importer->version() : meta->importerVersion;
        primary.record.settingsHash = canonical_settings_hash(meta->settings.root());
        primary.record.status = importer != nullptr ? asset_record_status::OK : asset_record_status::NO_IMPORTER;
        if (importer == nullptr) {
            ++report.missingImporters;
        }

        // A new asset at a reused path inherits nothing from the asset that used the path before.
        const entry *old = metaCreated ? nullptr : previous_for(meta->id, assetPath);
        const bool primaryImported = old != nullptr && old->record.type != asset_type_id::INVALID;
        bool needsHash = true;
        if (old != nullptr) {
            primary.record.type = old->record.type;
            primary.record.payloadHash = old->record.payloadHash;
            primary.record.sourceHash = old->record.sourceHash;
            primary.dependencies = old->dependencies;
            primary.sideFiles = old->sideFiles;
            primary.importerSwitched = old->importerSwitched ||
              (primaryImported && old->record.importerId != primary.record.importerId);
            if (primary.record.status == asset_record_status::OK && status_from_previous_import(old->record.status)) {
                primary.record.status = old->record.status;
            }
            // The (size, write time) fast path covers the source file only; a source with side
            // files is always re-hashed because their changes are invisible in its own stat.
            const bool unchanged = old->record.sourceSize == primary.record.sourceSize &&
              old->record.sourceWriteTime == primary.record.sourceWriteTime && old->record.sourceHash != 0 &&
              old->record.status != asset_record_status::MISSING_SOURCE && old->sideFiles.empty();
            needsHash = !unchanged;
        } else {
            ++report.recordsAdded;
        }

        const usize primaryIndex = next.size();
        next.push_back(std::move(primary));
        if (needsHash) {
            toHash.push_back(primaryIndex);
        }

        for (const sub_asset_entry &sub : meta->subAssets) {
            if (!claimed.try_emplace(sub.id, assetPath).second) {
                CONJURE_LOG_WARN(LOG_ASSETS, "{}#{} duplicates uuid {}; excluded", assetPath, sub.subPath, sub.id);
                ++report.duplicateUuids;
                continue;
            }

            // The meta is append-only, the record table is not: a sub-asset the importer dropped
            // at its last import has neither a record nor a blob, and gets neither back until an
            // import produces it again. A fresh clone (no previous primary) creates every one.
            const entry *oldSub = previous_for(sub.id, make_asset_path(mount.name, source.relativePath, sub.subPath));
            if (oldSub == nullptr && primaryImported && !mCache->contains(sub.id)) {
                continue;
            }

            entry subEntry = {};
            subEntry.record.id = sub.id;
            subEntry.record.parentId = meta->id;
            subEntry.record.mountIndex = source.mountIndex;
            subEntry.record.relativePath = source.relativePath;
            subEntry.record.subPath = sub.subPath;
            subEntry.record.importerId = next[primaryIndex].record.importerId;
            subEntry.record.importerVersion = next[primaryIndex].record.importerVersion;
            subEntry.record.settingsHash = next[primaryIndex].record.settingsHash;
            subEntry.record.sourceSize = source.stat.size;
            subEntry.record.sourceWriteTime = next[primaryIndex].record.sourceWriteTime;
            subEntry.record.status = next[primaryIndex].record.status;
            subEntry.record.displayName = std::string(path_utils::filename(sub.subPath));
            if (oldSub != nullptr) {
                subEntry.record.type = oldSub->record.type;
                subEntry.record.payloadHash = oldSub->record.payloadHash;
                if (!oldSub->record.displayName.empty()) {
                    subEntry.record.displayName = oldSub->record.displayName;
                }
                subEntry.dependencies = oldSub->dependencies;
            }
            subEntry.record.sourceHash = next[primaryIndex].record.sourceHash;
            next.push_back(std::move(subEntry));
        }
    }

    for (const replaced_identity &item : replaced) {
        if (!claimed.contains(item.previous)) {
            CONJURE_LOG_WARN(
              LOG_ASSETS,
              "{}: its .meta was missing, so it is a new asset {}; {} is now MISSING_SOURCE and references to "
              "it break",
              item.assetPath,
              item.fresh,
              item.previous
            );
        }
    }

    // Sources that disappeared keep their records as MISSING_SOURCE so loaded payloads and
    // references stay meaningful.
    for (const entry &old : previous) {
        if (claimed.contains(old.record.id)) {
            continue;
        }
        if (is_transient(old.record.status)) {
            continue;   // transient records are rebuilt from the files that exist
        }
        entry missing = old;
        if (missing.record.status != asset_record_status::MISSING_SOURCE) {
            ++report.recordsRemoved;
        }
        missing.record.status = asset_record_status::MISSING_SOURCE;
        next.push_back(std::move(missing));
    }

    // Hash changed sources (plus their side files); jobs when a job system is available.
    report.hashed = static_cast<u32>(toHash.size());
    const std::function<void(usize)> hashOne = [&](usize position) {
        entry &target = next[toHash[position]];
        target.record.sourceHash = hash_source(target.record.mountIndex, target.record.relativePath, target.sideFiles);
    };
    if (mJobs != nullptr && toHash.size() > 1) {
        mJobs->parallel_for(toHash.size(), hashOne);
    } else {
        for (usize position = 0; position < toHash.size(); ++position) {
            hashOne(position);
        }
    }
    core::flat_hash_map<asset_id, usize> primaryIndex = {};
    for (usize index = 0; index < next.size(); ++index) {
        if (!next[index].record.is_sub_asset()) {
            primaryIndex.try_emplace(next[index].record.id, index);
        }
    }
    for (entry &item : next) {
        if (!item.record.is_sub_asset()) {
            continue;
        }
        const auto parent = primaryIndex.find(item.record.parentId);
        if (parent != primaryIndex.end()) {
            item.record.sourceHash = next[parent->second].record.sourceHash;
        }
    }

    {
        const std::unique_lock lock(mMutex);
        mEntries = std::move(next);
        rebuild_indexes_locked();
        rebuild_graphs_locked();
        detect_cycles_locked();
    }
    recompute_stale();
    report.stale = stale_ids();

    core::result<u32> temporaries = mCache->remove_temporaries();
    if (!temporaries.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", temporaries.get_error().message());
    }
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    CONJURE_LOG_INFO(
      LOG_ASSETS,
      "scanned {} source(s): {} added, {} missing, {} meta(s) created, {} stale",
      report.filesScanned,
      report.recordsAdded,
      report.recordsRemoved,
      report.metasCreated,
      report.stale.size()
    );
    return report;
}

std::vector<asset_id> asset_database::stale_ids() const {
    const std::shared_lock lock(mMutex);
    return mStale;
}

/*===========================================================================*/
/*===========================================================================*/
// Import

core::result<import_report> asset_database::import_stale(core::jobs::job_system &jobs) {
    const std::vector<asset_id> stale = stale_ids();
    return import(stale, jobs);
}

core::result<import_report> asset_database::import(std::span<const asset_id> ids, core::jobs::job_system &jobs) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::import is main-thread only");
    CONJURE_TRY(ensure_open());

    import_report report = {};
    std::vector<source_job> sourceJobs = {};
    {
        const std::shared_lock lock(mMutex);
        core::flat_hash_set<asset_id> seen = {};
        for (const asset_id &requested : ids) {
            const entry *found = find_entry_locked(requested);
            if (found == nullptr) {
                ++report.sourcesSkipped;
                continue;
            }
            if (found->record.is_sub_asset()) {
                found = find_entry_locked(found->record.parentId);
                if (found == nullptr) {
                    ++report.sourcesSkipped;
                    continue;
                }
            }
            if (!seen.insert(found->record.id).second) {
                continue;
            }
            if (status_blocks_import(found->record.status)) {
                ++report.sourcesSkipped;
                continue;
            }

            source_job job = {};
            job.entryIndex = static_cast<usize>(found - mEntries.data());
            job.id = found->record.id;
            job.mountIndex = found->record.mountIndex;
            job.relativePath = found->record.relativePath;
            job.assetPath = make_asset_path(mMounts[found->record.mountIndex].name, found->record.relativePath);
            job.filePath = file_path_for(found->record.mountIndex, found->record.relativePath);
            sourceJobs.push_back(std::move(job));
        }
    }

    // Metas are read on the main thread: a write_meta() after the merge must see the same table.
    // The importer named in the meta is authoritative (design 3.7: an importer change in the meta
    // marks the owner dirty), so it is resolved here rather than taken from the record.
    for (source_job &job : sourceJobs) {
        core::result<asset_meta> meta = read_asset_meta(*mFileSystem, meta_path_of(job.filePath));
        if (!meta.has_value()) {
            job.failure = meta.get_error();
            continue;
        }
        job.meta = std::move(meta).value();
        job.subAssets = std::make_unique<sub_asset_table>(job.meta.subAssets);
        job.importer = job.meta.importerId.empty() ? nullptr : mImporters->find(job.meta.importerId);
        if (job.importer == nullptr) {
            job.failure = make_asset_error(
              asset_error_code::NO_IMPORTER,
              std::format("importer '{}' named by the meta is not registered", job.meta.importerId)
            );
            continue;
        }
        job.importerVersion = job.importer->version();
    }

    mImporters->begin_imports();
    const asset_path_resolver resolver = [this](std::string_view assetPath) {
        return find_by_path(assetPath);
    };
    const std::function<void(usize)> importOne = [&](usize index) {
        source_job &job = sourceJobs[index];
        if (job.failure.has_value()) {
            return;
        }

        core::result<std::vector<std::byte>> bytes = mFileSystem->read_file(job.filePath);
        if (!bytes.has_value()) {
            job.failure = io_error("read source", job.filePath, bytes.get_error());
            return;
        }

        import_context context(
          *mFileSystem,
          job.assetPath,
          job.filePath,
          bytes.value(),
          job.meta.settings.root(),
          mPlatform,
          job.id,
          *job.subAssets,
          resolver
        );
        core::result<import_output> output = job.importer->import(context);
        job.warnings.assign(context.warnings().begin(), context.warnings().end());
        job.sideFiles.assign(context.side_files().begin(), context.side_files().end());
        if (!output.has_value()) {
            job.failure = make_asset_error(
              asset_error_code::IMPORT_FAILED,
              std::format("importer '{}' failed: {}", job.importer->id(), output.get_error().to_string())
            );
            return;
        }

        usize primaries = 0;
        for (const imported_asset &asset : output.value().assets) {
            if (asset.subPath.empty()) {
                ++primaries;
            }
            if (mTypes->find(asset.type) == nullptr) {
                job.failure = make_asset_error(
                  asset_error_code::TYPE_UNKNOWN,
                  std::format(
                    "importer '{}' produced unregistered type {:#x}",
                    job.importer->id(),
                    static_cast<u64>(asset.type)
                  )
                );
                return;
            }
        }
        if (primaries != 1) {
            job.failure = make_asset_error(
              asset_error_code::IMPORT_FAILED,
              std::format("importer '{}' produced {} primary assets instead of one", job.importer->id(), primaries)
            );
            return;
        }

        // The hash describes the bytes the importer read, not a second read that a concurrent
        // write could make disagree with the blob.
        job.sourceHash = hash_source_bytes(bytes.value(), job.sideFiles);
        const u64 settingsHash = canonical_settings_hash(job.meta.settings.root());
        for (imported_asset &asset : output.value().assets) {
            source_job::produced item = {};
            item.id = asset.subPath.empty() ? job.id : job.subAssets->id_for(asset.subPath);
            item.subPath = asset.subPath;
            item.displayName = asset.displayName;
            item.type = asset.type;
            item.dependencies = std::move(asset.dependencies);
            std::erase(item.dependencies, item.id);
            std::sort(item.dependencies.begin(), item.dependencies.end());
            item.dependencies.erase(
              std::unique(item.dependencies.begin(), item.dependencies.end()),
              item.dependencies.end()
            );

            cooked_blob_header header = {};
            header.type = asset.type;
            header.typeVersion = mTypes->find(asset.type)->version;
            header.importerVersion = job.importerVersion;
            header.sourceHash = job.sourceHash;
            header.settingsHash = settingsHash;
            header.id = item.id;
            const cooked_blob blob = make_cooked_blob(header, item.dependencies, std::move(asset.payload));
            item.payloadHash = blob.header.payloadHash;

            core::result<void> written = mCache->write(blob);
            if (!written.has_value()) {
                job.failure = written.get_error();
                return;
            }
            job.outputs.push_back(std::move(item));
        }
    };
    if (sourceJobs.size() > 1) {
        jobs.parallel_for(sourceJobs.size(), importOne);
    } else {
        for (usize index = 0; index < sourceJobs.size(); ++index) {
            importOne(index);
        }
    }
    mImporters->end_imports();

    CONJURE_TRY(apply_import_results(sourceJobs, report));
    recompute_stale();

    core::result<void> logged = write_import_log(report);
    if (!logged.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", logged.get_error().message());
    }
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    return report;
}

core::result<void> asset_database::apply_import_results(std::vector<source_job> &sourceJobs, import_report &report) {
    const std::unique_lock lock(mMutex);
    core::flat_hash_set<asset_id> droppedSubAssets = {};
    for (source_job &job : sourceJobs) {
        if (job.entryIndex >= mEntries.size() || mEntries[job.entryIndex].record.id != job.id) {
            ++report.sourcesSkipped;
            continue;
        }
        for (std::string &warning : job.warnings) {
            report.warnings.push_back(std::format("{}: {}", job.assetPath, warning));
        }

        // mChildren was built before this merge; the entries appended below are new sub-assets
        // of a successful job, which are in its producedIds, so the stale index is sufficient.
        const std::vector<usize> children = mChildren[job.entryIndex];

        if (job.failure.has_value()) {
            // A meta naming an unknown importer is a configuration state, not a broken import:
            // the record becomes NO_IMPORTER (browsable, not loadable) like at scan.
            const bool noImporter = job.failure->is(asset_error_code::NO_IMPORTER);
            const asset_record_status status = noImporter
              ? asset_record_status::NO_IMPORTER
              : asset_record_status::IMPORT_FAILED;
            entry &primary = mEntries[job.entryIndex];
            primary.record.status = status;
            if (job.importer != nullptr || noImporter) {
                // The meta was read: it names the importer the record now stands for.
                primary.record.importerId = job.meta.importerId;
                primary.record.importerVersion = noImporter ? job.meta.importerVersion : job.importerVersion;
            }
            if (job.importer != nullptr) {
                primary.importerSwitched = false;   // the switch was tried; IMPORT_FAILED waits for a change
            }
            for (const usize child : children) {
                mEntries[child].record.status = status;
                mEntries[child].record.importerId = primary.record.importerId;
                mEntries[child].record.importerVersion = primary.record.importerVersion;
            }

            // Side files asked for by the failed import join the known ones (never replace them: a
            // failure before the importer ran has none), so a missing one re-imports its owner
            // when it appears.
            primary.sideFiles.insert(primary.sideFiles.end(), job.sideFiles.begin(), job.sideFiles.end());
            std::sort(primary.sideFiles.begin(), primary.sideFiles.end());
            primary.sideFiles.erase(
              std::unique(primary.sideFiles.begin(), primary.sideFiles.end()),
              primary.sideFiles.end()
            );
            if (noImporter) {
                CONJURE_LOG_WARN(LOG_ASSETS, "import of {} skipped: {}", job.assetPath, job.failure->message());
            } else {
                CONJURE_LOG_ERROR(LOG_ASSETS, "import of {} failed: {}", job.assetPath, job.failure->to_string());
            }
            report.failures.push_back(import_failure{job.id, job.assetPath, job.failure->to_string()});
            ++report.sourcesFailed;
            continue;
        }

        ++report.sourcesImported;
        {
            entry &primary = mEntries[job.entryIndex];
            primary.record.sourceHash = job.sourceHash;
            primary.record.importerId = job.meta.importerId;
            primary.record.importerVersion = job.importerVersion;
            primary.record.settingsHash = canonical_settings_hash(job.meta.settings.root());
            primary.sideFiles = job.sideFiles;
            primary.importerSwitched = false;
            primary.record.status = asset_record_status::OK;
        }

        core::flat_hash_set<asset_id> producedIds = {};
        for (source_job::produced &item : job.outputs) {
            producedIds.insert(item.id);
            ++report.assetsProduced;
            entry *target = nullptr;
            if (item.subPath.empty()) {
                target = &mEntries[job.entryIndex];
            } else {
                target = find_entry_locked(item.id);
                if (target == nullptr) {
                    entry created = {};
                    created.record.id = item.id;
                    created.record.parentId = job.id;
                    mEntries.push_back(std::move(created));
                    target = &mEntries.back();
                }
            }
            const entry &owner = mEntries[job.entryIndex];
            const bool changed = target->record.payloadHash != item.payloadHash || target->record.type != item.type;
            target->record.mountIndex = owner.record.mountIndex;
            target->record.relativePath = owner.record.relativePath;
            target->record.subPath = item.subPath;
            target->record.importerId = owner.record.importerId;
            target->record.importerVersion = owner.record.importerVersion;
            target->record.sourceSize = owner.record.sourceSize;
            target->record.sourceWriteTime = owner.record.sourceWriteTime;
            target->record.sourceHash = owner.record.sourceHash;
            target->record.settingsHash = owner.record.settingsHash;
            target->record.type = item.type;
            target->record.payloadHash = item.payloadHash;
            target->record.status = asset_record_status::OK;
            if (!item.displayName.empty()) {
                target->record.displayName = item.displayName;
            } else if (target->record.displayName.empty()) {
                target->record.displayName = item.subPath.empty()
                  ? std::string(path_utils::stem(owner.record.relativePath))
                  : std::string(path_utils::filename(item.subPath));
            }
            target->dependencies = std::move(item.dependencies);
            if (changed) {
                report.changed.push_back(item.id);
            }
        }

        // Sub-assets the importer stopped producing keep their uuid in the meta (append-only) but
        // have no blob any more, so they leave the record table once every job is merged (erasing
        // here would shift the entry indices the other jobs still hold).
        for (const usize child : children) {
            if (!producedIds.contains(mEntries[child].record.id)) {
                droppedSubAssets.insert(mEntries[child].record.id);
            }
        }

        if (job.subAssets->changed()) {
            job.meta.subAssets = job.subAssets->entries();
            core::result<void> written = write_asset_meta(*mFileSystem, meta_path_of(job.filePath), job.meta);
            if (!written.has_value()) {
                CONJURE_LOG_WARN(LOG_ASSETS, "{}", written.get_error().message());
            }
        }
    }

    if (!droppedSubAssets.empty()) {
        std::erase_if(mEntries, [&](const entry &candidate) {
            return droppedSubAssets.contains(candidate.record.id);
        });

        // Their blobs go too: a scan or write_meta() tells a dropped sub-asset (no blob) from one
        // whose record was lost before a snapshot save (blob kept, record re-created).
        for (const asset_id &dropped : droppedSubAssets) {
            core::result<void> removed = mCache->remove(dropped);
            if (!removed.has_value()) {
                CONJURE_LOG_WARN(LOG_ASSETS, "{}", removed.get_error().message());
            }
        }
    }
    rebuild_indexes_locked();
    rebuild_graphs_locked();
    detect_cycles_locked();
    return core::status::success();
}

core::result<void> asset_database::write_import_log(const import_report &report) const {
    std::string text = {};
    text.append(
      std::format(
        "imported {} source(s), {} asset(s); {} failed; {} skipped\n",
        report.sourcesImported,
        report.assetsProduced,
        report.sourcesFailed,
        report.sourcesSkipped
      )
    );
    for (const import_failure &failure : report.failures) {
        text.append(std::format("FAILED {} {}: {}\n", failure.id, failure.assetPath, failure.message));
    }
    for (const std::string &warning : report.warnings) {
        text.append(std::format("WARN {}\n", warning));
    }
    const std::string path = path_utils::join(mCacheRoot, IMPORT_LOG_FILE);
    core::result<void> written = mFileSystem->write_file_atomic(path, to_bytes(text));
    if (!written.has_value()) {
        return io_error("write import log", path, written.get_error());
    }
    return core::status::success();
}

/*===========================================================================*/
/*===========================================================================*/
// Editing

core::result<asset_id> asset_database::add_file(std::string_view assetPath) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::add_file is main-thread only");
    CONJURE_TRY(ensure_open());

    const std::optional<asset_path_parts> parts = split_asset_path(assetPath);
    if (!parts.has_value() || !parts->subPath.empty()) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("'{}' is not a source asset path", assetPath)
        );
    }
    const std::optional<u32> mountIndex = mount_index(parts->mount);
    if (!mountIndex.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("unknown mount '{}'", parts->mount));
    }
    const std::string relative = path_utils::normalize(parts->relativePath);
    if (!stays_in_mount(relative)) {
        return leaves_mount(assetPath);
    }
    if (find_by_path(assetPath).has_value()) {
        return core::error(core::error_code::ALREADY_EXISTS, std::format("'{}' is already known", assetPath));
    }
    const std::string filePath = file_path_for(*mountIndex, relative);
    core::result<core::vfs::file_stat> stat = mFileSystem->stat(filePath);
    if (!stat.has_value() || !stat.value().isRegular) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("no source file at '{}'", assetPath));
    }

    bool metaCreated = false;
    core::result<asset_id> registered = register_source(*mountIndex, relative, metaCreated);
    if (!registered.has_value()) {
        return registered;
    }
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    return registered;
}

core::result<asset_id> asset_database::create_asset(
  std::string_view assetPath,
  std::string_view importerId,
  std::span<const std::byte> initialBytes
) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::create_asset is main-thread only");
    CONJURE_TRY(ensure_open());

    const std::optional<asset_path_parts> parts = split_asset_path(assetPath);
    if (!parts.has_value() || !parts->subPath.empty()) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("'{}' is not a source asset path", assetPath)
        );
    }
    const std::optional<u32> mountIndex = mount_index(parts->mount);
    if (!mountIndex.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("unknown mount '{}'", parts->mount));
    }
    const std::string relative = path_utils::normalize(parts->relativePath);
    if (!stays_in_mount(relative)) {
        return leaves_mount(assetPath);
    }
    const asset_importer *importer = mImporters->find(importerId);
    if (importer == nullptr) {
        return make_asset_error(
          asset_error_code::NO_IMPORTER,
          std::format("importer '{}' is not registered", importerId)
        );
    }
    if (find_by_path(assetPath).has_value()) {
        return core::error(core::error_code::ALREADY_EXISTS, std::format("'{}' is already known", assetPath));
    }
    const std::string filePath = file_path_for(*mountIndex, relative);
    if (mFileSystem->exists(filePath)) {
        return core::error(core::error_code::ALREADY_EXISTS, std::format("a file exists at '{}'", assetPath));
    }

    core::result<void> written = mFileSystem->write_file_atomic(filePath, initialBytes);
    if (!written.has_value()) {
        return io_error("write source", filePath, written.get_error());
    }
    asset_meta meta = {};
    meta.id = asset_id::generate();
    meta.importerId = std::string(importer->id());
    meta.importerVersion = importer->version();
    meta.settings = importer->default_settings();
    core::result<void> metaWritten = write_asset_meta(*mFileSystem, meta_path_of(filePath), meta);
    if (!metaWritten.has_value()) {
        // A source without its meta would get a fresh uuid at the next scan: the pair is created
        // together or not at all.
        core::result<void> removed = mFileSystem->remove(filePath);
        if (!removed.has_value()) {
            CONJURE_LOG_WARN(
              LOG_ASSETS,
              "cannot remove '{}' after its meta failed to write: {}",
              filePath,
              removed.get_error().message()
            );
        }
        return metaWritten.get_error();
    }

    bool metaCreated = false;
    core::result<asset_id> registered = register_source(*mountIndex, relative, metaCreated);
    if (!registered.has_value()) {
        return registered;
    }
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    return registered;
}

core::result<asset_id> asset_database::register_source(
  u32 mountIndex,
  std::string_view relativePath,
  bool &outMetaCreated
) {
    outMetaCreated = false;
    const std::string filePath = file_path_for(mountIndex, relativePath);
    const std::string metaPath = meta_path_of(filePath);

    core::result<core::vfs::file_stat> stat = mFileSystem->stat(filePath);
    if (!stat.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("no source file at '{}'", filePath));
    }

    core::result<asset_meta> metaRead = read_asset_meta(*mFileSystem, metaPath);
    asset_meta meta = {};
    if (metaRead.has_value()) {
        meta = std::move(metaRead).value();
    } else if (metaRead.get_error().is(asset_error_code::NOT_FOUND)) {
        meta.id = asset_id::generate();
        const asset_importer *byExtension = mImporters->find_for_path(relativePath);
        if (byExtension != nullptr) {
            meta.importerId = std::string(byExtension->id());
            meta.importerVersion = byExtension->version();
            meta.settings = byExtension->default_settings();
        }
        CONJURE_TRY(write_asset_meta(*mFileSystem, metaPath, meta));
        outMetaCreated = true;
    } else {
        return metaRead.get_error();
    }

    const asset_importer *importer = meta.importerId.empty() ? nullptr : mImporters->find(meta.importerId);
    entry primary = {};
    primary.record.id = meta.id;
    primary.record.mountIndex = mountIndex;
    primary.record.relativePath = std::string(relativePath);
    primary.record.displayName = std::string(path_utils::stem(relativePath));
    primary.record.importerId = meta.importerId;
    primary.record.importerVersion = importer != nullptr ? importer->version() : meta.importerVersion;
    primary.record.settingsHash = canonical_settings_hash(meta.settings.root());
    primary.record.sourceSize = stat.value().size;
    primary.record.sourceWriteTime = static_cast<i64>(stat.value().writeTimeNanoseconds);
    primary.record.status = importer != nullptr ? asset_record_status::OK : asset_record_status::NO_IMPORTER;

    {
        const std::shared_lock lock(mMutex);
        if (find_entry_locked(meta.id) != nullptr) {
            return make_asset_error(
              asset_error_code::DUPLICATE_UUID,
              std::format("uuid {} of '{}' is already in use", meta.id, filePath)
            );
        }
    }
    primary.record.sourceHash = hash_source(mountIndex, relativePath, {});

    std::vector<entry> subEntries = {};
    for (const sub_asset_entry &sub : meta.subAssets) {
        entry subEntry = {};
        subEntry.record = primary.record;
        subEntry.record.id = sub.id;
        subEntry.record.parentId = meta.id;
        subEntry.record.subPath = sub.subPath;
        subEntry.record.displayName = std::string(path_utils::filename(sub.subPath));
        subEntries.push_back(std::move(subEntry));
    }

    const asset_id id = meta.id;
    {
        const std::unique_lock lock(mMutex);
        mEntries.push_back(std::move(primary));
        for (entry &subEntry : subEntries) {
            mEntries.push_back(std::move(subEntry));
        }
        rebuild_indexes_locked();
        rebuild_graphs_locked();
    }
    recompute_stale();
    return id;
}

core::result<void> asset_database::move_asset(const asset_id &id, std::string_view newAssetPath) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::move_asset is main-thread only");
    CONJURE_TRY(ensure_open());

    const std::optional<asset_path_parts> parts = split_asset_path(newAssetPath);
    if (!parts.has_value() || !parts->subPath.empty()) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("'{}' is not a source asset path", newAssetPath)
        );
    }
    const std::optional<u32> mountIndex = mount_index(parts->mount);
    if (!mountIndex.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("unknown mount '{}'", parts->mount));
    }
    const std::string newRelative = path_utils::normalize(parts->relativePath);
    if (!stays_in_mount(newRelative)) {
        return leaves_mount(newAssetPath);
    }
    std::optional<asset_record> record = find(id);
    if (!record.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is unknown", id));
    }
    if (record->is_sub_asset()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "sub-assets move with their primary");
    }
    if (find_by_path(newAssetPath).has_value()) {
        return core::error(core::error_code::ALREADY_EXISTS, std::format("'{}' is already known", newAssetPath));
    }

    const std::string oldFile = file_path_for(record->mountIndex, record->relativePath);
    const std::string newFile = file_path_for(*mountIndex, newRelative);
    if (mFileSystem->exists(newFile)) {
        return core::error(core::error_code::ALREADY_EXISTS, std::format("a file exists at '{}'", newAssetPath));
    }
    const std::string_view parent = path_utils::parent(newFile);
    if (!parent.empty()) {
        core::result<void> created = mFileSystem->create_directories(parent);
        if (!created.has_value()) {
            return io_error("create directory", parent, created.get_error());
        }
    }

    // The .meta is the identity (D1), so it moves first: a source left behind without its meta
    // would get a fresh uuid at the next scan and orphan every reference. If the source rename
    // then fails, the meta goes back so the pair stays together.
    const std::string oldMeta = meta_path_of(oldFile);
    const std::string newMeta = meta_path_of(newFile);
    const bool hasMeta = mFileSystem->exists(oldMeta);
    if (hasMeta) {
        core::result<void> metaRenamed = mFileSystem->rename(oldMeta, newMeta);
        if (!metaRenamed.has_value()) {
            return io_error("move meta", oldMeta, metaRenamed.get_error());
        }
    }
    core::result<void> renamed = mFileSystem->rename(oldFile, newFile);
    if (!renamed.has_value()) {
        if (hasMeta) {
            core::result<void> restored = mFileSystem->rename(newMeta, oldMeta);
            if (!restored.has_value()) {
                CONJURE_LOG_ERROR(
                  LOG_ASSETS,
                  "cannot move '{}' back after a failed source move: {}",
                  newMeta,
                  restored.get_error().message()
                );
            }
        }
        return io_error("move source", oldFile, renamed.get_error());
    }

    {
        const std::unique_lock lock(mMutex);
        entry *primary = find_entry_locked(id);
        if (primary != nullptr) {
            const usize primaryIndex = static_cast<usize>(primary - mEntries.data());
            primary->record.mountIndex = *mountIndex;
            primary->record.relativePath = newRelative;
            primary->record.displayName = std::string(path_utils::stem(newRelative));
            for (const usize child : mChildren[primaryIndex]) {
                mEntries[child].record.mountIndex = *mountIndex;
                mEntries[child].record.relativePath = newRelative;
            }
        }
        rebuild_indexes_locked();
    }
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    return core::status::success();
}

core::result<void> asset_database::delete_asset(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::delete_asset is main-thread only");
    CONJURE_TRY(ensure_open());

    std::optional<asset_record> record = find(id);
    if (!record.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is unknown", id));
    }
    if (record->is_sub_asset()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "sub-assets are deleted with their primary");
    }

    const std::string filePath = file_path_for(record->mountIndex, record->relativePath);
    for (const std::string &path : {filePath, meta_path_of(filePath)}) {
        core::result<void> removed = mFileSystem->remove(path);
        if (!removed.has_value() && removed.get_error().generic_code() != core::error_code::NOT_FOUND) {
            return io_error("remove", path, removed.get_error());
        }
    }

    std::vector<asset_id> blobs = {};
    {
        const std::unique_lock lock(mMutex);
        for (const entry &candidate : mEntries) {
            if (candidate.record.id == id || candidate.record.parentId == id) {
                blobs.push_back(candidate.record.id);
            }
        }
        std::erase_if(mEntries, [&](const entry &candidate) {
            return candidate.record.id == id || candidate.record.parentId == id;
        });
        rebuild_indexes_locked();
        rebuild_graphs_locked();
        detect_cycles_locked();
    }
    recompute_stale();
    for (const asset_id &blob : blobs) {
        core::result<void> removed = mCache->remove(blob);
        if (!removed.has_value()) {
            CONJURE_LOG_WARN(LOG_ASSETS, "{}", removed.get_error().message());
        }
    }
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    return core::status::success();
}

core::result<asset_meta> asset_database::read_meta(const asset_id &id) const {
    std::optional<asset_record> record = find(id);
    if (!record.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is unknown", id));
    }
    if (record->is_sub_asset()) {
        record = find(record->parentId);
        if (!record.has_value()) {
            return make_asset_error(asset_error_code::NOT_FOUND, std::format("primary of {} is unknown", id));
        }
    }
    return read_asset_meta(*mFileSystem, meta_path_of(file_path_for(record->mountIndex, record->relativePath)));
}

core::result<void> asset_database::write_meta(const asset_id &id, const asset_meta &meta) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::write_meta is main-thread only");
    std::optional<asset_record> record = find(id);
    if (!record.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is unknown", id));
    }
    if (record->is_sub_asset()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "sub-assets share their primary's meta");
    }
    if (meta.id != id) {
        CONJURE_LOG_ERROR(
          LOG_ASSETS,
          "refusing to change the uuid of {} to {}: it would orphan every reference",
          id,
          meta.id
        );
        return core::error(core::error_code::INVALID_ARGUMENT, "a meta's uuid cannot change");
    }
    if (status_blocks_import(record->status)) {
        return core::error(
          core::error_code::INVALID_STATE,
          std::format("asset {} is {}", id, to_string(record->status))
        );
    }

    const std::string filePath = file_path_for(record->mountIndex, record->relativePath);
    CONJURE_TRY(write_asset_meta(*mFileSystem, meta_path_of(filePath), meta));

    const asset_importer *importer = meta.importerId.empty() ? nullptr : mImporters->find(meta.importerId);
    {
        const std::unique_lock lock(mMutex);
        entry *primary = find_entry_locked(id);
        if (primary == nullptr) {
            return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is unknown", id));
        }
        // A blob header names no importer, so the stale rule cannot see a switch between two
        // importers of the same version: the flag keeps the primary stale until it is imported.
        if (primary->record.type != asset_type_id::INVALID && primary->record.importerId != meta.importerId) {
            primary->importerSwitched = true;
        }
        primary->record.importerId = meta.importerId;
        primary->record.importerVersion = importer != nullptr ? importer->version() : meta.importerVersion;
        primary->record.settingsHash = canonical_settings_hash(meta.settings.root());
        if (importer == nullptr) {
            primary->record.status = asset_record_status::NO_IMPORTER;
        } else if (primary->record.status == asset_record_status::NO_IMPORTER) {
            primary->record.status = asset_record_status::OK;
        }
        const asset_record primaryRecord = primary->record;
        for (const usize child : mChildren[static_cast<usize>(primary - mEntries.data())]) {
            mEntries[child].record.importerId = primaryRecord.importerId;
            mEntries[child].record.importerVersion = primaryRecord.importerVersion;
            mEntries[child].record.settingsHash = primaryRecord.settingsHash;
            mEntries[child].record.status = primaryRecord.status;
        }
        // As at scan: a sub-asset the last import dropped stays out of the table (no record, no
        // blob); a primary that was never imported gets every sub-asset of its meta.
        const bool primaryImported = primaryRecord.type != asset_type_id::INVALID;
        for (const sub_asset_entry &sub : meta.subAssets) {
            if (find_entry_locked(sub.id) != nullptr) {
                continue;
            }
            if (primaryImported && !mCache->contains(sub.id)) {
                continue;
            }
            entry subEntry = {};
            subEntry.record = primaryRecord;
            subEntry.record.id = sub.id;
            subEntry.record.parentId = id;
            subEntry.record.subPath = sub.subPath;
            subEntry.record.type = asset_type_id::INVALID;
            subEntry.record.payloadHash = 0;
            subEntry.record.displayName = std::string(path_utils::filename(sub.subPath));
            mEntries.push_back(std::move(subEntry));
        }
        rebuild_indexes_locked();
        rebuild_graphs_locked();
    }
    recompute_stale();
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    return core::status::success();
}

core::result<void> asset_database::set_importer(const asset_id &id, std::string_view importerId) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::set_importer is main-thread only");
    const asset_importer *importer = mImporters->find(importerId);
    if (importer == nullptr) {
        return make_asset_error(
          asset_error_code::NO_IMPORTER,
          std::format("importer '{}' is not registered", importerId)
        );
    }
    CONJURE_TRY_ASSIGN(asset_meta meta, read_meta(id));
    if (meta.importerId == importer->id()) {
        return core::status::success();
    }
    // The sub-asset table stays: it is identity (D1), so the uuids of a model's meshes survive a
    // switch to another importer and back.
    meta.importerId = std::string(importer->id());
    meta.importerVersion = importer->version();
    meta.settings = importer->default_settings();
    return write_meta(id, meta);
}

core::result<void> asset_database::mark_source_missing(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::mark_source_missing is main-thread only");
    return set_source_status(id, false);
}

core::result<void> asset_database::mark_source_present(const asset_id &id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_database::mark_source_present is main-thread only");
    return set_source_status(id, true);
}

core::result<void> asset_database::save_snapshot() const {
    CONJURE_TRY(ensure_open());
    detail::database_snapshot snapshot = {};
    {
        const std::shared_lock lock(mMutex);
        for (const asset_mount &mount : mMounts) {
            snapshot.mountNames.push_back(mount.name);
        }
        for (const entry &item : mEntries) {
            if (is_transient(item.record.status)) {
                continue;   // transient ids must not outlive the process
            }
            detail::snapshot_record persisted = {};
            persisted.record = item.record;
            persisted.dependencies = item.dependencies;
            persisted.sideFiles = item.sideFiles;
            snapshot.records.push_back(std::move(persisted));
        }
    }
    const std::string path = path_utils::join(mCacheRoot, SNAPSHOT_FILE);
    core::result<void> written = mFileSystem->write_file_atomic(path, detail::write_database_snapshot(snapshot));
    if (!written.has_value()) {
        return io_error("write snapshot", path, written.get_error());
    }
    return core::status::success();
}

core::result<void> asset_database::load_snapshot() {
    const std::string path = path_utils::join(mCacheRoot, SNAPSHOT_FILE);
    core::result<std::vector<std::byte>> bytes = mFileSystem->read_file(path);
    if (!bytes.has_value()) {
        return make_asset_error(asset_error_code::CACHE_MISS, std::format("no snapshot at '{}'", path));
    }
    core::result<detail::database_snapshot> parsed = detail::parse_database_snapshot(bytes.value());
    if (!parsed.has_value()) {
        return parsed.get_error();
    }
    const detail::database_snapshot &snapshot = parsed.value();
    if (snapshot.mountNames.size() != mMounts.size()) {
        return make_asset_error(asset_error_code::CACHE_CORRUPT, "snapshot mounts differ from the configured mounts");
    }
    for (usize index = 0; index < mMounts.size(); ++index) {
        if (snapshot.mountNames[index] != mMounts[index].name) {
            return make_asset_error(
              asset_error_code::CACHE_CORRUPT,
              "snapshot mounts differ from the configured mounts"
            );
        }
    }

    std::vector<entry> entries = {};
    entries.reserve(snapshot.records.size());
    for (const detail::snapshot_record &persisted : snapshot.records) {
        entry item = {};
        item.record = persisted.record;
        item.dependencies = persisted.dependencies;
        item.sideFiles = persisted.sideFiles;
        entries.push_back(std::move(item));
    }
    {
        const std::unique_lock lock(mMutex);
        mEntries = std::move(entries);
        rebuild_indexes_locked();
        rebuild_graphs_locked();
    }
    recompute_stale();
    return core::status::success();
}

/*===========================================================================*/
/*===========================================================================*/
// Lookups

std::vector<asset_record> asset_database::records() const {
    const std::shared_lock lock(mMutex);
    std::vector<asset_record> copies = {};
    copies.reserve(mEntries.size());
    for (const entry &item : mEntries) {
        copies.push_back(item.record);
    }
    return copies;
}

usize asset_database::record_count() const {
    const std::shared_lock lock(mMutex);
    return mEntries.size();
}

std::optional<asset_record> asset_database::find(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    const entry *found = find_entry_locked(id);
    if (found == nullptr) {
        return std::nullopt;
    }
    return found->record;
}

bool asset_database::contains(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    return mIndexById.contains(id);
}

std::optional<asset_type_id> asset_database::type_of(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    const entry *found = find_entry_locked(id);
    if (found == nullptr || found->record.type == asset_type_id::INVALID) {
        return std::nullopt;
    }
    return found->record.type;
}

std::optional<asset_record_status> asset_database::status_of(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    const entry *found = find_entry_locked(id);
    if (found == nullptr) {
        return std::nullopt;
    }
    return found->record.status;
}

std::optional<asset_id> asset_database::find_by_path(std::string_view assetPath) const {
    const std::shared_lock lock(mMutex);
    const auto found = mIndexByPath.find(std::string(assetPath));
    if (found == mIndexByPath.end()) {
        return std::nullopt;
    }
    return mEntries[found->second].record.id;
}

std::optional<asset_id> asset_database::find_by_file_path(std::string_view filePath) const {
    const std::string normalized = path_utils::normalize(filePath);
    for (u32 mountIndex = 0; mountIndex < mMounts.size(); ++mountIndex) {
        const asset_mount &mount = mMounts[mountIndex];
        std::string_view relative = normalized;
        if (!mount.rootPath.empty()) {
            const std::string prefix = mount.rootPath + "/";
            if (!core::str::starts_with(relative, prefix)) {
                continue;
            }
            relative.remove_prefix(prefix.size());
        }
        const std::optional<asset_id> found = find_by_path(make_asset_path(mount.name, relative));
        if (found.has_value()) {
            return found;
        }
    }
    return std::nullopt;
}

std::optional<std::string> asset_database::asset_path_of(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    const entry *found = find_entry_locked(id);
    if (found == nullptr) {
        return std::nullopt;
    }
    return make_asset_path(mMounts[found->record.mountIndex].name, found->record.relativePath, found->record.subPath);
}

std::optional<std::string> asset_database::file_path_of(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    const entry *found = find_entry_locked(id);
    if (found == nullptr) {
        return std::nullopt;
    }
    return file_path_for(found->record.mountIndex, found->record.relativePath);
}

std::vector<asset_id> asset_database::dependencies_of(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    const entry *found = find_entry_locked(id);
    if (found == nullptr) {
        return {};
    }
    std::vector<asset_id> ids = {};
    ids.reserve(found->edgeCount);
    for (u32 edge = 0; edge < found->edgeCount; ++edge) {
        ids.push_back(mEntries[mForwardEdges[found->firstEdge + edge]].record.id);
    }
    return ids;
}

std::vector<asset_id> asset_database::dependents_of(const asset_id &id) const {
    const std::shared_lock lock(mMutex);
    const entry *found = find_entry_locked(id);
    if (found == nullptr) {
        return {};
    }
    std::vector<asset_id> ids = {};
    ids.reserve(found->reverseEdgeCount);
    for (u32 edge = 0; edge < found->reverseEdgeCount; ++edge) {
        ids.push_back(mEntries[mReverseEdges[found->firstReverseEdge + edge]].record.id);
    }
    return ids;
}

std::vector<asset_id> asset_database::owners_of_side_file(std::string_view assetPath) const {
    const std::shared_lock lock(mMutex);
    const auto found = mSideFileOwners.find(std::string(assetPath));
    if (found == mSideFileOwners.end()) {
        return {};
    }
    return found->second;
}

core::result<std::vector<asset_id>> asset_database::dependency_closure(std::span<const asset_id> roots) const {
    const std::shared_lock lock(mMutex);
    std::vector<asset_id> closure = {};
    core::flat_hash_set<usize> visited = {};
    std::vector<usize> stack = {};
    for (const asset_id &root : roots) {
        const entry *found = find_entry_locked(root);
        if (found == nullptr) {
            return make_asset_error(asset_error_code::NOT_FOUND, std::format("closure root {} is unknown", root));
        }
        stack.push_back(static_cast<usize>(found - mEntries.data()));
    }
    while (!stack.empty()) {
        const usize index = stack.back();
        stack.pop_back();
        if (!visited.insert(index).second) {
            continue;
        }
        const entry &item = mEntries[index];
        closure.push_back(item.record.id);
        for (u32 edge = 0; edge < item.edgeCount; ++edge) {
            stack.push_back(mForwardEdges[item.firstEdge + edge]);
        }
    }
    std::sort(closure.begin(), closure.end());
    return closure;
}

void asset_database::collect_dependents(std::span<const asset_id> roots, std::vector<asset_id> &outOrdered) const {
    const std::shared_lock lock(mMutex);
    core::flat_hash_set<usize> visited = {};
    std::deque<usize> frontier = {};
    for (const asset_id &root : roots) {
        const entry *found = find_entry_locked(root);
        if (found == nullptr) {
            continue;
        }
        const usize index = static_cast<usize>(found - mEntries.data());
        if (visited.insert(index).second) {
            frontier.push_back(index);
            outOrdered.push_back(found->record.id);
        }
    }
    while (!frontier.empty()) {
        const usize index = frontier.front();
        frontier.pop_front();
        const entry &item = mEntries[index];
        for (u32 edge = 0; edge < item.reverseEdgeCount; ++edge) {
            const usize dependent = mReverseEdges[item.firstReverseEdge + edge];
            if (visited.insert(dependent).second) {
                frontier.push_back(dependent);
                outOrdered.push_back(mEntries[dependent].record.id);
            }
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

std::string asset_database::file_path_for(u32 mountIndex, std::string_view relativePath) const {
    CONJURE_ASSERT(mountIndex < mMounts.size(), "mount index {} out of range", mountIndex);
    if (mountIndex >= mMounts.size()) {
        return std::string(relativePath);
    }
    return path_utils::normalize(path_utils::join(mMounts[mountIndex].rootPath, relativePath));
}

std::optional<u32> asset_database::mount_index(std::string_view mountName) const noexcept {
    for (u32 index = 0; index < mMounts.size(); ++index) {
        if (mMounts[index].name == mountName) {
            return index;
        }
    }
    return std::nullopt;
}

const asset_database::entry *asset_database::find_entry_locked(const asset_id &id) const noexcept {
    const auto found = mIndexById.find(id);
    if (found == mIndexById.end()) {
        return nullptr;
    }
    return &mEntries[found->second];
}

asset_database::entry *asset_database::find_entry_locked(const asset_id &id) noexcept {
    const auto found = mIndexById.find(id);
    if (found == mIndexById.end()) {
        return nullptr;
    }
    return &mEntries[found->second];
}

core::result<void> asset_database::ensure_open() const {
    if (mCache == nullptr) {
        return not_open();
    }
    return core::status::success();
}

core::result<void> asset_database::set_source_status(const asset_id &id, bool present) {
    CONJURE_TRY(ensure_open());
    bool changed = false;
    {
        const std::unique_lock lock(mMutex);
        entry *primary = find_entry_locked(id);
        if (primary != nullptr && primary->record.is_sub_asset()) {
            primary = find_entry_locked(primary->record.parentId);
        }
        if (primary == nullptr) {
            return make_asset_error(asset_error_code::NOT_FOUND, std::format("asset {} is unknown", id));
        }
        if (is_transient(primary->record.status)) {
            return core::error(
              core::error_code::INVALID_STATE,
              std::format("asset {} is {}", id, to_string(primary->record.status))
            );
        }

        asset_record_status status = primary->record.status;
        if (!present) {
            status = asset_record_status::MISSING_SOURCE;
        } else if (primary->record.status == asset_record_status::MISSING_SOURCE) {
            const std::string filePath = file_path_for(primary->record.mountIndex, primary->record.relativePath);
            core::result<core::vfs::file_stat> stat = mFileSystem->stat(filePath);
            if (!stat.has_value() || !stat.value().isRegular) {
                return make_asset_error(asset_error_code::NOT_FOUND, std::format("no source file at '{}'", filePath));
            }
            const asset_importer *importer = mImporters->find(primary->record.importerId);
            status = importer != nullptr ? asset_record_status::OK : asset_record_status::NO_IMPORTER;
            primary->record.sourceSize = stat.value().size;
            primary->record.sourceWriteTime = static_cast<i64>(stat.value().writeTimeNanoseconds);
            primary->record.sourceHash = hash_source(
              primary->record.mountIndex,
              primary->record.relativePath,
              primary->sideFiles
            );
        }
        changed = status != primary->record.status;
        primary->record.status = status;
        const usize primaryIndex = static_cast<usize>(primary - mEntries.data());
        for (const usize child : mChildren[primaryIndex]) {
            mEntries[child].record.status = status;
            mEntries[child].record.sourceHash = primary->record.sourceHash;
        }
    }
    if (!changed) {
        return core::status::success();
    }
    recompute_stale();
    core::result<void> saved = save_snapshot();
    if (!saved.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "{}", saved.get_error().message());
    }
    return core::status::success();
}

u64 asset_database::hash_source(
  u32 mountIndex,
  std::string_view relativePath,
  std::span<const std::string> sideFiles
) const {
    const std::string filePath = file_path_for(mountIndex, relativePath);
    core::result<std::vector<std::byte>> bytes = mFileSystem->read_file(filePath);
    if (!bytes.has_value()) {
        return 0;
    }
    return hash_source_bytes(bytes.value(), sideFiles);
}

u64 asset_database::hash_source_bytes(
  std::span<const std::byte> primaryBytes,
  std::span<const std::string> sideFiles
) const {
    u64 hash = detail::fnv1a64_bytes(primaryBytes);
    std::vector<std::string> sorted(sideFiles.begin(), sideFiles.end());
    std::sort(sorted.begin(), sorted.end());
    for (const std::string &sidePath : sorted) {
        hash = detail::fnv1a64(sidePath) ^ hash;
        const std::optional<asset_path_parts> parts = split_asset_path(sidePath);
        if (!parts.has_value()) {
            continue;
        }
        const std::optional<u32> sideMount = mount_index(parts->mount);
        if (!sideMount.has_value()) {
            continue;
        }
        core::result<std::vector<std::byte>> sideBytes = mFileSystem->read_file(
          file_path_for(*sideMount, parts->relativePath)
        );
        if (sideBytes.has_value()) {
            hash = detail::fnv1a64_bytes(sideBytes.value(), hash);
        }
    }
    return hash == 0 ? 1 : hash;   // 0 means "never hashed"
}

void asset_database::rebuild_indexes_locked() {
    mIndexById.clear();
    mIndexByPath.clear();
    mSideFileOwners.clear();
    mChildren.clear();
    mIndexById.reserve(mEntries.size());
    mIndexByPath.reserve(mEntries.size());
    mChildren.resize(mEntries.size());
    for (usize index = 0; index < mEntries.size(); ++index) {
        const entry &item = mEntries[index];
        mIndexById.try_emplace(item.record.id, index);
        const std::string path = make_asset_path(
          mMounts[item.record.mountIndex].name,
          item.record.relativePath,
          item.record.subPath
        );
        mIndexByPath.try_emplace(path, index);
        for (const std::string &sideFile : item.sideFiles) {
            mSideFileOwners[sideFile].push_back(item.record.id);
        }
    }
    for (usize index = 0; index < mEntries.size(); ++index) {
        const entry &item = mEntries[index];
        if (!item.record.is_sub_asset()) {
            continue;
        }
        const auto parent = mIndexById.find(item.record.parentId);
        if (parent != mIndexById.end()) {
            mChildren[parent->second].push_back(index);
        }
    }
    for (auto &[sideFile, owners] : mSideFileOwners) {
        std::sort(owners.begin(), owners.end());
        owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
    }
}

void asset_database::rebuild_graphs_locked() {
    mForwardEdges.clear();
    mReverseEdges.clear();
    std::vector<std::vector<usize>> reverse(mEntries.size());
    for (usize index = 0; index < mEntries.size(); ++index) {
        entry &item = mEntries[index];
        item.firstEdge = static_cast<u32>(mForwardEdges.size());
        item.edgeCount = 0;
        for (const asset_id &dependency : item.dependencies) {
            const auto found = mIndexById.find(dependency);
            if (found == mIndexById.end()) {
                continue;   // unknown dependency: resolved by the manager at load time
            }
            mForwardEdges.push_back(found->second);
            reverse[found->second].push_back(index);
            ++item.edgeCount;
        }
    }
    for (usize index = 0; index < mEntries.size(); ++index) {
        entry &item = mEntries[index];
        item.firstReverseEdge = static_cast<u32>(mReverseEdges.size());
        item.reverseEdgeCount = static_cast<u32>(reverse[index].size());
        mReverseEdges.insert(mReverseEdges.end(), reverse[index].begin(), reverse[index].end());
    }
}

void asset_database::detect_cycles_locked() {
    // Tarjan's strongly connected components in the iterative form (an explicit stack of
    // {node, next edge} frames), so a dependency chain as deep as the record table cannot
    // overflow the call stack. Every member of a component with more than one node, or with a
    // self edge, is in a cycle.
    const usize count = mEntries.size();
    std::vector<i32> index(count, -1);
    std::vector<i32> lowLink(count, 0);
    std::vector<bool> onStack(count, false);
    std::vector<bool> cyclic(count, false);
    std::vector<usize> stack = {};
    i32 nextIndex = 0;

    struct frame {
        usize node = 0;
        u32 nextEdge = 0;
    };
    std::vector<frame> frames = {};

    const auto has_self_edge = [&](usize node) {
        const entry &item = mEntries[node];
        for (u32 edge = 0; edge < item.edgeCount; ++edge) {
            if (mForwardEdges[item.firstEdge + edge] == node) {
                return true;
            }
        }
        return false;
    };

    const auto visit = [&](usize node) {
        index[node] = nextIndex;
        lowLink[node] = nextIndex;
        ++nextIndex;
        stack.push_back(node);
        onStack[node] = true;
        frames.push_back(frame{node, 0});
    };

    for (usize root = 0; root < count; ++root) {
        if (index[root] >= 0) {
            continue;
        }
        visit(root);
        while (!frames.empty()) {
            frame &current = frames.back();
            const entry &item = mEntries[current.node];
            if (current.nextEdge < item.edgeCount) {
                const usize next = mForwardEdges[item.firstEdge + current.nextEdge];
                ++current.nextEdge;
                if (index[next] < 0) {
                    visit(next);   // descend; 'current' may dangle after the push
                } else if (onStack[next]) {
                    lowLink[current.node] = std::min(lowLink[current.node], index[next]);
                }
                continue;
            }

            // Every edge of this node is explored: close its component, then fold its low link
            // into the parent frame exactly as the recursive return would.
            const usize node = current.node;
            if (lowLink[node] == index[node]) {
                std::vector<usize> component = {};
                while (true) {
                    const usize member = stack.back();
                    stack.pop_back();
                    onStack[member] = false;
                    component.push_back(member);
                    if (member == node) {
                        break;
                    }
                }
                if (component.size() > 1 || has_self_edge(node)) {
                    for (const usize member : component) {
                        cyclic[member] = true;
                    }
                }
            }
            frames.pop_back();
            if (!frames.empty()) {
                lowLink[frames.back().node] = std::min(lowLink[frames.back().node], lowLink[node]);
            }
        }
    }

    for (usize node = 0; node < count; ++node) {
        entry &item = mEntries[node];
        if (cyclic[node]) {
            if (item.record.status == asset_record_status::OK) {
                CONJURE_LOG_WARN(LOG_ASSETS, "asset {} is part of a dependency cycle", item.record.id);
            }
            if (
              item.record.status == asset_record_status::OK || item.record.status == asset_record_status::IMPORT_FAILED
            ) {
                item.record.status = asset_record_status::CYCLIC_DEPENDENCY;
            }
        } else if (item.record.status == asset_record_status::CYCLIC_DEPENDENCY) {
            item.record.status = asset_record_status::OK;
        }
    }
}

void asset_database::recompute_stale() {
    // The blob headers are read without any lock held: a scan of a large project would otherwise
    // block every lookup thread for the duration of thousands of header reads. Mutations are
    // main-thread only, so the record snapshot taken under the shared lock stays consistent with
    // the table until the result is published under the unique lock.
    struct blob_probe {
        asset_id id = {};
        cache_expectation expected = {};
    };
    struct stale_candidate {
        asset_id primary = {};
        std::vector<blob_probe> probes = {};
        bool stale = false;
    };
    std::vector<stale_candidate> candidates = {};
    {
        const std::shared_lock lock(mMutex);
        for (usize index = 0; index < mEntries.size(); ++index) {
            const entry &primary = mEntries[index];
            if (primary.record.is_sub_asset()) {
                continue;
            }
            const bool importable = primary.record.status == asset_record_status::OK ||
              primary.record.status == asset_record_status::IMPORT_FAILED ||
              primary.record.status == asset_record_status::CYCLIC_DEPENDENCY;
            if (!importable) {
                continue;
            }
            const asset_importer *importer = mImporters->find(primary.record.importerId);
            if (importer == nullptr) {
                continue;
            }

            stale_candidate candidate = {};
            candidate.primary = primary.record.id;
            candidate.stale = primary.importerSwitched;   // the blob header cannot tell two importers apart
            const auto probe = [&](const entry &item) {
                if (item.record.type == asset_type_id::INVALID) {
                    candidate.stale = true;   // never imported
                    return;
                }
                const asset_type_desc *type = mTypes->find(item.record.type);
                if (type == nullptr) {
                    candidate.stale = true;   // the type left: its blob cannot be trusted
                    return;
                }
                blob_probe blob = {};
                blob.id = item.record.id;
                blob.expected.type = item.record.type;
                blob.expected.typeVersion = type->version;
                blob.expected.importerVersion = importer->version();
                blob.expected.sourceHash = primary.record.sourceHash;
                blob.expected.settingsHash = primary.record.settingsHash;
                candidate.probes.push_back(blob);
            };
            probe(primary);
            for (const usize child : mChildren[index]) {
                probe(mEntries[child]);
            }
            candidates.push_back(std::move(candidate));
        }
    }

    std::vector<asset_id> stale = {};
    for (stale_candidate &candidate : candidates) {
        for (const blob_probe &blob : candidate.probes) {
            if (candidate.stale) {
                break;
            }
            core::result<cooked_blob_header> header = mCache->read_header(blob.id);
            candidate.stale = !header.has_value() || !asset_cache::is_valid(header.value(), blob.expected);
        }
        if (candidate.stale) {
            stale.push_back(candidate.primary);
        }
    }

    const std::unique_lock lock(mMutex);
    mStale = std::move(stale);
}
