#include "engine/subsystems/assets_subsystem.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/log_category.h"
#include "conjure/engine/project.h"

#include "conjure/assets/asset_source.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"

#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] core::error database_error(std::string_view what, const core::error &cause) {
        return core::make_error(engine_error::ASSET_DATABASE_FAILED, std::format("{}: {}", what, cause.to_string()));
    }

    /// <summary>
    /// The assets root as a path inside the project file system ("assets" by default). Empty when the root
    /// is not lexically inside the project directory (absolute, or escaping through ".."): the database is
    /// then built over the dedicated assets file system the application mounted as assets://, whose root is
    /// the whole assets directory.
    /// </summary>
    [[nodiscard]] std::string mount_root_of(const project *loaded) {
        if (loaded == nullptr) {
            return "assets";
        }
        const std::filesystem::path relative =
          loaded->assets_directory().lexically_relative(loaded->root_directory().lexically_normal());
        if (relative.empty() || relative.begin()->generic_string() == "..") {
            return "";
        }
        const std::string text = relative.lexically_normal().generic_string();
        return text == "." ? "" : text;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

assets_subsystem::assets_subsystem(engine_services &services) :
    mServices(&services) {}

assets_subsystem::~assets_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view assets_subsystem::name() const noexcept {
    return "assets";
}

core::result<void> assets_subsystem::startup(engine_context &context) {
    mImportedThisStartup = 0;

    assets::asset_manager_desc desc = {};
    desc.jobs = &mServices->jobs;
    mServices->assetManager = std::make_unique<assets::asset_manager>(std::move(desc));
    CONJURE_TRY(assets::register_binary_asset_type(mServices->assetManager->types(), &mServices->importers));

    if (mServices->cooked) {
        // Exported project: the pack is the only source; no database, no importers at work.
        if (mServices->cookedFileSystem == nullptr) {
            return core::make_error(engine_error::ASSET_DATABASE_FAILED, "a cooked project needs its cooked root");
        }
        mServices->packReader = std::make_unique<assets::pack_reader>();
        const core::result<void> opened = mServices->packReader->open(*mServices->cookedFileSystem, PACK_FILE_NAME);
        if (!opened.has_value()) {
            return database_error("opening the content pack", opened.get_error());
        }
        mServices->assetManager->swap_source(std::make_unique<assets::pack_asset_source>(*mServices->packReader));
        CONJURE_LOG_INFO(LOG_ENGINE, "assets: {} cooked asset(s) in the pack", mServices->packReader->toc().size());
    } else {
        if (mServices->projectFileSystem == nullptr) {
            return core::make_error(
              engine_error::ASSET_DATABASE_FAILED,
              "no project file system to build the database over"
            );
        }
        mAssetsMountRoot = mount_root_of(context.project);
        // An assets root inside the project is a directory of the project file system; one outside it (an
        // absolute or parent-relative assets_root) is the dedicated file system start_core() mounted as
        // assets://, so the database and the vfs agree on what "assets://" means. The cache then lives
        // under that root as well (.conjure/cache).
        core::vfs::file_system *databaseFileSystem = mServices->projectFileSystem.get();
        if (mAssetsMountRoot.empty() && context.project != nullptr) {
            if (mServices->assetsFileSystem == nullptr) {
                return core::make_error(
                  engine_error::ASSET_DATABASE_FAILED,
                  "the assets root outside the project is not mounted"
                );
            }
            databaseFileSystem = mServices->assetsFileSystem.get();
        }
        mServices->assetDatabase = std::make_unique<assets::asset_database>(
          *databaseFileSystem,
          mServices->importers,
          mServices->assetManager->types(),
          &mServices->jobs
        );
        const core::result<void> mounted = mServices->assetDatabase->add_mount("assets", mAssetsMountRoot);
        if (!mounted.has_value()) {
            return database_error("mounting the assets root", mounted.get_error());
        }
        if (!mAssetsMountRoot.empty()) {
            const core::result<void> created = databaseFileSystem->create_directories(mAssetsMountRoot);
            if (!created.has_value()) {
                return database_error("creating the assets root", created.get_error());
            }
        }
        const core::result<void> opened = mServices->assetDatabase->open(CACHE_ROOT, cache_platform());
        if (!opened.has_value()) {
            return database_error("opening the asset cache", opened.get_error());
        }
        mServices->assetManager->swap_source(
          std::make_unique<assets::cache_asset_source>(*mServices->assetDatabase->cache(), *mServices->assetDatabase)
        );

#if !CONJURE_DISTRIBUTION
        // The watcher's stat pass runs on a job thread every 500 ms and allocates its file table there, so
        // it exists only when asked for (assets_settings::hotReload, off in headless_defaults()): a headless
        // frame, a benchmark or a server never pays for it (09 item 15a).
        if (context.config->assets.hotReload) {
            mServices->fileWatcher =
              std::make_unique<assets::polling_file_watcher>(*databaseFileSystem, mServices->jobs);
            mServices->fileWatcher->add_root(mAssetsMountRoot);
            mServices->hotReload = std::make_unique<assets::hot_reload_controller>(
              *mServices->assetDatabase,
              *mServices->assetManager,
              *mServices->fileWatcher,
              mServices->jobs
            );
        } else {
            CONJURE_LOG_DEBUG(LOG_ENGINE, "assets: hot reload disabled by configuration");
        }
#endif
    }

    context.assets = mServices->assetManager.get();
    context.assetDatabase = mServices->assetDatabase.get();
    return core::result<void>::success();
}

core::result<void> assets_subsystem::post_startup(engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->assetDatabase == nullptr) {
        return core::result<void>::success();
    }

    // Every importer is registered by now (steps 10-15), so the scan sees the full extension table (R33).
    core::result<assets::scan_report> scanned = mServices->assetDatabase->scan();
    if (!scanned.has_value()) {
        return database_error("scanning the project sources", scanned.get_error());
    }
    core::result<assets::import_report> imported = mServices->assetDatabase->import_stale(mServices->jobs);
    if (!imported.has_value()) {
        return database_error("importing stale sources", imported.get_error());
    }
    mImportedThisStartup = static_cast<u32>(imported.value().changed.size());
    for (const assets::import_failure &failure : imported.value().failures) {
        CONJURE_LOG_WARN(LOG_ENGINE, "assets: '{}' failed to import: {}", failure.assetPath, failure.message);
    }
    for (const std::string &warning : imported.value().warnings) {
        CONJURE_LOG_WARN(LOG_ENGINE, "assets: {}", warning);
    }
    CONJURE_LOG_INFO(
      LOG_ENGINE,
      "assets: {} record(s), {} imported at startup",
      mServices->assetDatabase->record_count(),
      mImportedThisStartup
    );

    if (mServices->fileWatcher != nullptr) {
        const core::result<void> baseline = mServices->fileWatcher->snapshot();
        if (!baseline.has_value()) {
            CONJURE_LOG_WARN(LOG_ENGINE, "assets: file watcher baseline failed: {}", baseline.get_error().to_string());
        }
    }
    return core::result<void>::success();
}

void assets_subsystem::begin_frame(engine_context &context) {
    if (mServices->assetManager == nullptr) {
        return;
    }
    const u64 frameIndex = context.time != nullptr ? context.time->frameIndex : 0;
    mServices->assetManager->begin_frame(frameIndex);
    mServices->assetManager->update();
    if (mServices->hotReload != nullptr) {
        mServices->hotReload->update();
    }
}

void assets_subsystem::end_frame(engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->assetManager != nullptr) {
        mServices->assetManager->end_frame();
    }
}

void assets_subsystem::shutdown(engine_context &context) {
    context.assetDatabase = nullptr;
    context.assets = nullptr;
    mServices->hotReload.reset();
    mServices->fileWatcher.reset();
    if (mServices->assetManager != nullptr) {
        mServices->assetManager->shutdown();
    }
    mServices->assetManager.reset();
    mServices->assetDatabase.reset();
    mServices->packReader.reset();
}

std::string_view assets_subsystem::cache_platform() noexcept {
#if CONJURE_PLATFORM_WINDOWS
    return "windows";
#elif CONJURE_PLATFORM_MACOS
    return "macos";
#else
    return "linux";
#endif
}
