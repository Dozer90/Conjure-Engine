#include "conjure/assets/asset_cooker.h"
#include "conjure/assets/asset_pack.h"
#include "conjure/assets/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::string_view PACK_FILE = "content.cpak";
    constexpr std::string_view MANIFEST_FILE = "manifest.json";
    constexpr std::string_view ENGINE_MOUNT = "engine";

    /// <summary>
    /// Only records that have (or can get) a blob of their own can be cooked.
    /// </summary>
    bool can_cook(asset_record_status status) noexcept {
        return status == asset_record_status::OK || status == asset_record_status::IMPORT_FAILED;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

asset_cooker::asset_cooker(
  asset_database &database,
  const asset_cache &cache,
  core::vfs::file_system &fileSystem,
  core::jobs::job_system &jobs
) :
    mDatabase(&database),
    mCache(&cache),
    mFileSystem(&fileSystem),
    mJobs(&jobs) {}

asset_cooker::~asset_cooker() {}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<cook_report> asset_cooker::cook(const cook_settings &settings) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_cooker::cook is main-thread only");
    mLastReport = cook_report();
    cook_report &report = mLastReport;

    if (settings.outputDirectory.empty()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "cook_settings::outputDirectory is empty");
    }

    core::result<scan_report> scanned = mDatabase->scan();
    if (!scanned.has_value()) {
        return std::move(scanned).get_error();
    }

    // Roots: the requested ones, the always-included ones and every engine asset; or everything.
    std::vector<asset_id> roots = {};
    const std::vector<asset_record> records = mDatabase->records();
    if (settings.includeAll) {
        for (const asset_record &record : records) {
            roots.push_back(record.id);
        }
    } else {
        roots.insert(roots.end(), settings.roots.begin(), settings.roots.end());
        roots.insert(roots.end(), settings.alwaysInclude.begin(), settings.alwaysInclude.end());
        const std::span<const asset_mount> mounts = mDatabase->mounts();
        for (const asset_record &record : records) {
            if (record.mountIndex < mounts.size() && mounts[record.mountIndex].name == ENGINE_MOUNT) {
                roots.push_back(record.id);
            }
        }
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

    // Dependencies are only known once a source is imported, so the closure and the import of its
    // stale members alternate until no member is left to import. Each primary is imported at most
    // once per cook (importedThisCook), so the loop ends. Only the final closure's members can
    // fail as non-cookable.
    const asset_type_registry &types = mDatabase->types();
    std::vector<asset_id> members = {};
    std::vector<cook_failure> memberFailures = {};
    core::flat_hash_set<asset_id> importedThisCook = {};
    while (true) {
        core::result<std::vector<asset_id>> closure = mDatabase->dependency_closure(roots);
        if (!closure.has_value()) {
            report.failures.push_back(cook_failure{asset_id(), {}, closure.get_error().to_string()});
            return std::move(closure).get_error();
        }

        // Drop types excluded from packs and records that can never import; the latter are failures.
        members.clear();
        memberFailures.clear();
        std::vector<asset_id> toImport = {};
        const std::vector<asset_id> stale = mDatabase->stale_ids();
        for (const asset_id &id : closure.value()) {
            const std::optional<asset_record> record = mDatabase->find(id);
            if (!record.has_value()) {
                memberFailures.push_back(cook_failure{id, {}, "record vanished during cook"});
                continue;
            }
            const asset_type_desc *type = types.find(record->type);
            if (type != nullptr && has_flag(type->flags, asset_type_flags::EXCLUDE_FROM_PACK)) {
                continue;
            }
            if (!can_cook(record->status)) {
                const std::optional<std::string> path = mDatabase->asset_path_of(id);
                memberFailures.push_back(
                  cook_failure{id, path.value_or(std::string()), std::format("record is {}", to_string(record->status))}
                );
                continue;
            }
            members.push_back(id);
            const asset_id primary = record->is_sub_asset() ? record->parentId : id;
            if (std::find(stale.begin(), stale.end(), primary) != stale.end() && !importedThisCook.contains(primary)) {
                toImport.push_back(primary);
            }
        }
        std::sort(toImport.begin(), toImport.end());
        toImport.erase(std::unique(toImport.begin(), toImport.end()), toImport.end());
        if (toImport.empty()) {
            break;
        }
        for (const asset_id &primary : toImport) {
            importedThisCook.insert(primary);
        }
        core::result<import_report> imported = mDatabase->import(toImport, *mJobs);
        if (!imported.has_value()) {
            return std::move(imported).get_error();
        }
        for (const import_failure &failure : imported.value().failures) {
            report.failures.push_back(cook_failure{failure.id, failure.assetPath, failure.message});
        }
        report.warnings.insert(
          report.warnings.end(),
          imported.value().warnings.begin(),
          imported.value().warnings.end()
        );
    }

    // Import failures are already listed ahead of these, so for an asset whose import failed the
    // import message is the one kept by the per-id deduplication below.
    report.failures.insert(report.failures.end(), memberFailures.begin(), memberFailures.end());

    // Every member must have a valid blob now; types unregistered since import are failures too.
    pack_writer writer = {};
    for (const asset_id &id : members) {
        core::result<cooked_blob> blob = mCache->read(id);
        const std::optional<std::string> path = mDatabase->asset_path_of(id);
        if (!blob.has_value()) {
            report.failures.push_back(cook_failure{id, path.value_or(std::string()), blob.get_error().to_string()});
            continue;
        }
        if (types.find(blob.value().header.type) == nullptr) {
            report.failures.push_back(cook_failure{id, path.value_or(std::string()), "blob type is not registered"});
            continue;
        }
        const asset_type_desc *type = types.find(blob.value().header.type);
        if (has_flag(type->flags, asset_type_flags::EXCLUDE_FROM_PACK)) {
            continue;
        }
        core::result<void> added = writer.add(std::move(blob).value(), path.value_or(std::string()));
        if (!added.has_value()) {
            report.failures.push_back(cook_failure{id, path.value_or(std::string()), added.get_error().to_string()});
        }
    }

    if (!report.failures.empty()) {
        std::stable_sort(report.failures.begin(), report.failures.end(), [](
          const cook_failure &a,
          const cook_failure &b
        ) {
            return a.id < b.id;
        });
        report.failures.erase(
          std::unique(report.failures.begin(), report.failures.end(), [](const cook_failure &a, const cook_failure &b) {
              return a.id == b.id;   // the earliest message of an asset is kept
          }),
          report.failures.end()
        );
        for (const cook_failure &failure : report.failures) {
            CONJURE_LOG_ERROR(LOG_ASSETS, "cook: {} {}: {}", failure.id, failure.assetPath, failure.message);
        }
        return make_asset_error(
          asset_error_code::IMPORT_FAILED,
          std::format("{} asset(s) could not be cooked; nothing was written", report.failures.size())
        );
    }

    const std::string outputDirectory = core::vfs::path_utils::normalize(settings.outputDirectory);
    core::result<void> created = mFileSystem->create_directories(outputDirectory);
    if (!created.has_value()) {
        return make_asset_error(
          asset_error_code::IO_ERROR,
          std::format("cannot create '{}': {}", outputDirectory, created.get_error().message())
        );
    }
    core::result<pack_stats> written = writer.write(
      *mFileSystem,
      core::vfs::path_utils::join(outputDirectory, PACK_FILE)
    );
    if (!written.has_value()) {
        return std::move(written).get_error();
    }
    if (settings.writeManifest) {
        CONJURE_TRY(writer.write_manifest(*mFileSystem, core::vfs::path_utils::join(outputDirectory, MANIFEST_FILE)));
    }

    report.assetCount = written.value().entryCount;
    report.packBytes = written.value().packBytes;
    report.duplicatesRemoved = written.value().duplicatesRemoved;
    report.contentHash = written.value().contentHash;
    CONJURE_LOG_INFO(
      LOG_ASSETS,
      "cooked {} asset(s) into '{}' ({} bytes, {} duplicate payload(s) removed)",
      report.assetCount,
      outputDirectory,
      report.packBytes,
      report.duplicatesRemoved
    );
    return report;
}
