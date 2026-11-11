#pragma once

#include "conjure/assets/asset_cache.h"
#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <string>
#include <vector>

// Export cooking (docs/design/05_assets.md 3.8, 4.7): closure over the dependency graph from the
// roots, import of stale members, one deterministic content.cpak plus manifest.json. All or
// nothing: a single failing asset writes no file.

namespace conjure::assets {

    struct cook_settings {
        std::string outputDirectory = {};      // where content.cpak and manifest.json go
        std::string platform = {};             // informational; the database's cache platform is used
        std::vector<asset_id> roots = {};
        std::vector<asset_id> alwaysInclude = {};
        bool includeAll = false;               // every record instead of the closure of the roots
        bool writeManifest = true;
    };

    struct cook_failure {
        asset_id id = {};
        std::string assetPath = {};
        std::string message = {};
    };

    struct cook_report {
        u32 assetCount = 0;
        u64 packBytes = 0;
        u32 duplicatesRemoved = 0;
        u64 contentHash = 0;
        std::vector<cook_failure> failures = {};
        std::vector<std::string> warnings = {};
    };

    /// <summary>
    /// Produces the export pack from the database and the cache. Main thread.
    /// </summary>
    class CONJURE_API asset_cooker {
        private:
            asset_database *mDatabase = nullptr;
            const asset_cache *mCache = nullptr;
            core::vfs::file_system *mFileSystem = nullptr;   // destination of the pack
            core::jobs::job_system *mJobs = nullptr;
            cook_report mLastReport = {};

        public:
            asset_cooker(
              asset_database &database,
              const asset_cache &cache,
              core::vfs::file_system &fileSystem,
              core::jobs::job_system &jobs
            );
            ~asset_cooker();

            asset_cooker(const asset_cooker &) = delete;
            asset_cooker &operator=(const asset_cooker &) = delete;

        public:
            /// <summary>
            /// Scans, computes the closure (roots + alwaysInclude + every engine:// asset, or all
            /// records), drops EXCLUDE_FROM_PACK types, imports stale members, verifies every blob
            /// and writes the pack. IMPORT_FAILED with every failure listed in the report when any
            /// member cannot be cooked; nothing is written then.
            /// </summary>
            [[nodiscard]] core::result<cook_report> cook(const cook_settings &settings);

            /// <summary>
            /// The report of the last cook(), successful or not.
            /// </summary>
            [[nodiscard]] const cook_report &last_report() const noexcept {
                return mLastReport;
            }
    };
}
