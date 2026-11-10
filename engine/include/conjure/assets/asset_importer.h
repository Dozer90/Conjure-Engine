#pragma once

#include "conjure/assets/asset_meta.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Importers turn source bytes into cooked payloads on worker threads (docs/design/05_assets.md
// 4.4). They are const, deterministic and stateless; everything they may touch comes through the
// import_context.

namespace conjure::assets {

    /// <summary>
    /// Thread-safe table of sub-asset uuids for one source: ids already in the .meta are returned
    /// as recorded, new sub paths are allocated under a mutex and merged back on the main thread.
    /// </summary>
    class sub_asset_table {
        private:
            mutable std::mutex mMutex = {};
            std::vector<sub_asset_entry> mEntries = {};
            bool mChanged = false;

        public:
            sub_asset_table();
            explicit sub_asset_table(std::vector<sub_asset_entry> existing);
            ~sub_asset_table();

            sub_asset_table(const sub_asset_table &) = delete;
            sub_asset_table &operator=(const sub_asset_table &) = delete;

        public:
            /// <summary>
            /// The uuid for a sub path, allocating a new one on first use.
            /// </summary>
            [[nodiscard]] asset_id id_for(std::string_view subPath);

            /// <summary>
            /// Every entry, sorted by sub path.
            /// </summary>
            [[nodiscard]] std::vector<sub_asset_entry> entries() const;

            /// <summary>
            /// True when id_for allocated at least one new uuid.
            /// </summary>
            [[nodiscard]] bool changed() const;
    };

    /// <summary>
    /// Resolves a mount-relative source path to the uuid of its primary asset.
    /// </summary>
    using asset_path_resolver = std::function<std::optional<asset_id>(std::string_view assetPath)>;

    /// <summary>
    /// Everything an importer may read or ask for during one import.
    /// </summary>
    class import_context {
        private:
            core::vfs::file_system *mFileSystem = nullptr;
            std::string mAssetPath = {};                   // "assets://models/robot.gltf"
            std::string mFilePath = {};                    // path of the source inside the file system
            std::span<const std::byte> mSourceBytes = {};
            core::json::value mSettings = {};
            std::string mPlatform = {};
            asset_id mPrimaryId = {};
            sub_asset_table *mSubAssets = nullptr;
            asset_path_resolver mResolver = {};
            std::vector<std::string> mSideFiles = {};      // asset paths of side files read
            std::vector<std::string> mWarnings = {};

        public:
            import_context(
              core::vfs::file_system &fileSystem,
              std::string assetPath,
              std::string filePath,
              std::span<const std::byte> sourceBytes,
              core::json::value settings,
              std::string platform,
              const asset_id &primaryId,
              sub_asset_table &subAssets,
              asset_path_resolver resolver
            );
            ~import_context();

            import_context(const import_context &) = delete;
            import_context &operator=(const import_context &) = delete;

        public:
            /// <summary>
            /// The source's asset path, "assets://models/robot.gltf".
            /// </summary>
            [[nodiscard]] std::string_view source_path() const noexcept {
                return mAssetPath;
            }

            /// <summary>
            /// The whole source file.
            /// </summary>
            [[nodiscard]] std::span<const std::byte> source_bytes() const noexcept {
                return mSourceBytes;
            }

            /// <summary>
            /// The importer settings from the .meta (an object; invalid when the meta had none).
            /// </summary>
            [[nodiscard]] const core::json::value &settings() const noexcept {
                return mSettings;
            }

            /// <summary>
            /// Target platform: "windows", "linux" or "macos".
            /// </summary>
            [[nodiscard]] std::string_view platform() const noexcept {
                return mPlatform;
            }

            /// <summary>
            /// Uuid of the primary asset (the source file itself).
            /// </summary>
            [[nodiscard]] const asset_id &primary_id() const noexcept {
                return mPrimaryId;
            }

            /// <summary>
            /// Reads a file next to the source (relative to the source's directory) and records it
            /// as a side file: hashed into the source hash and watched for changes.
            /// </summary>
            [[nodiscard]] core::result<std::vector<std::byte>> read_side_file(std::string_view relativePath);

            /// <summary>
            /// Stable uuid for a named sub-asset; allocated on first use, persisted to the meta.
            /// </summary>
            [[nodiscard]] asset_id sub_asset_id(std::string_view subPath);

            /// <summary>
            /// Uuid of another source of the project by asset path ("assets://textures/a.png") or
            /// by a path relative to this source's directory ("textures/a.png").
            /// </summary>
            [[nodiscard]] std::optional<asset_id> find_asset_by_path(std::string_view path) const;

            /// <summary>
            /// Records a non-fatal message that ends up in the import report and the import log.
            /// </summary>
            void add_warning(std::string message);

            /// <summary>
            /// Asset paths of every side file read so far.
            /// </summary>
            [[nodiscard]] std::span<const std::string> side_files() const noexcept {
                return mSideFiles;
            }

            [[nodiscard]] std::span<const std::string> warnings() const noexcept {
                return mWarnings;
            }
    };

    /// <summary>
    /// One cooked asset produced by an import: the primary (empty sub path) or a sub-asset.
    /// </summary>
    struct imported_asset {
        std::string subPath = {};                  // "" for the primary
        std::string displayName = {};
        asset_type_id type = asset_type_id::INVALID;
        std::vector<asset_id> dependencies = {};   // sorted, unique, no self reference
        std::vector<std::byte> payload = {};
    };

    /// <summary>
    /// Result of an import: exactly one entry has an empty sub path.
    /// </summary>
    struct import_output {
        std::vector<imported_asset> assets = {};
    };

    /// <summary>
    /// Converts source bytes into cooked payloads. Called on worker threads; must be deterministic
    /// (no clocks, no hash-map iteration order in the output) and touch no state beyond the context.
    /// </summary>
    class asset_importer {
        public:
            asset_importer();
            virtual ~asset_importer();

            asset_importer(const asset_importer &) = delete;
            asset_importer &operator=(const asset_importer &) = delete;

        public:
            /// <summary>
            /// Stable identifier recorded in every .meta ("conjure.renderer.gltf").
            /// </summary>
            [[nodiscard]] virtual std::string_view id() const noexcept = 0;

            /// <summary>
            /// Bump to invalidate every blob this importer produced.
            /// </summary>
            [[nodiscard]] virtual u32 version() const noexcept = 0;

            /// <summary>
            /// Lower-case extensions with the dot (".png"). Matched case-insensitively.
            /// </summary>
            [[nodiscard]] virtual std::span<const std::string_view> extensions() const noexcept = 0;

            /// <summary>
            /// Settings written into a new .meta. The default is an empty object.
            /// </summary>
            [[nodiscard]] virtual core::json::document default_settings() const;

            /// <summary>
            /// Worker thread. Const, deterministic, touches no shared state beyond the context.
            /// </summary>
            [[nodiscard]] virtual core::result<import_output> import(import_context &context) const = 0;
    };

    /// <summary>
    /// The registered importers. Registration on the main thread; find* from any thread between
    /// registrations. Pointers stay valid until the importer is unregistered.
    /// </summary>
    class importer_registry {
        private:
            std::vector<std::unique_ptr<asset_importer>> mImporters = {};
            std::vector<const asset_importer *> mView = {};
            std::atomic<u32> mImportsInFlight = 0;

        public:
            importer_registry();
            ~importer_registry();

            importer_registry(const importer_registry &) = delete;
            importer_registry &operator=(const importer_registry &) = delete;

        public:
            /// <summary>
            /// ALREADY_EXISTS when an importer with the same id is registered; INVALID_ARGUMENT
            /// for a null importer or an empty id.
            /// </summary>
            [[nodiscard]] core::result<void> register_importer(std::unique_ptr<asset_importer> importer);

            /// <summary>
            /// NOT_FOUND (core) for an unknown id; INVALID_STATE while imports are in flight.
            /// </summary>
            [[nodiscard]] core::result<void> unregister_importer(std::string_view id);

            [[nodiscard]] const asset_importer *find(std::string_view id) const noexcept;

            /// <summary>
            /// The importer handling an extension, matched ignoring case; the dot is optional.
            /// </summary>
            [[nodiscard]] const asset_importer *find_for_extension(std::string_view extension) const noexcept;

            /// <summary>
            /// The importer whose registered extension is the longest suffix of the path's file name,
            /// ignoring case, so a ".scene.json" importer wins over a ".json" one for "main.scene.json"
            /// (R45). nullptr when no extension matches. An extension registered without its dot ("json")
            /// matches the same names as the dotted spelling. A suffix never consumes the whole file
            /// name: a leading dot is part of the name (path_utils), so ".scene.json" matches ".json"
            /// and ".json" matches nothing.
            /// </summary>
            [[nodiscard]] const asset_importer *find_for_path(std::string_view path) const noexcept;

            /// <summary>
            /// Every importer in registration order.
            /// </summary>
            [[nodiscard]] std::span<const asset_importer *const> importers() const noexcept;

            /// <summary>
            /// Marks the start and end of an import batch; unregister_importer is refused between
            /// them. Used by asset_database.
            /// </summary>
            void begin_imports() noexcept;
            void end_imports() noexcept;

            [[nodiscard]] u32 imports_in_flight() const noexcept {
                return mImportsInFlight.load(std::memory_order_acquire);
            }
    };
}
