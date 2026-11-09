#pragma once

#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"

#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Blob providers (docs/design/05_assets.md D12, 4.6). The load pipeline reads blobs through this
// interface only, so the editor (cache), the runtime (pack), plugin packs (composite) and tests
// (memory) all exercise the same code.

namespace conjure::assets {

    class asset_cache;
    class asset_database;
    class pack_reader;

    /// <summary>
    /// A provider of validated cooked blobs. read() may be called from any thread.
    /// </summary>
    class asset_source {
        public:
            asset_source();
            virtual ~asset_source();

            asset_source(const asset_source &) = delete;
            asset_source &operator=(const asset_source &) = delete;

        public:
            [[nodiscard]] virtual bool contains(const asset_id &id) const noexcept = 0;
            [[nodiscard]] virtual std::optional<asset_type_id> type_of(const asset_id &id) const noexcept = 0;
            [[nodiscard]] virtual std::optional<asset_id> find_by_path(std::string_view assetPath) const = 0;

            /// <summary>
            /// Any thread. Returns the validated blob (header, dependencies, payload). NOT_FOUND
            /// for an unknown uuid, CACHE_MISS / CACHE_CORRUPT / PACK_CORRUPT as the backend reports.
            /// </summary>
            [[nodiscard]] virtual core::result<cooked_blob> read(const asset_id &id) const = 0;
    };

    /// <summary>
    /// Blobs held in memory. Tests add blobs directly; reads copy.
    /// </summary>
    class memory_asset_source final : public asset_source {
        private:
            struct entry {
                cooked_blob blob = {};
                std::string assetPath = {};
            };

            mutable std::mutex mMutex = {};
            core::flat_hash_map<asset_id, entry> mBlobs = {};
            core::flat_hash_map<std::string, asset_id> mIdsByPath = {};

        public:
            memory_asset_source();
            ~memory_asset_source() override;

        public:
            /// <summary>
            /// Adds or replaces the blob of its header's uuid, optionally under an asset path.
            /// </summary>
            void add(cooked_blob blob, std::string_view assetPath = {});

            /// <summary>
            /// Removes a blob. Returns false when there was none.
            /// </summary>
            bool remove(const asset_id &id);

            [[nodiscard]] usize size() const;

            [[nodiscard]] bool contains(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_type_id> type_of(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_id> find_by_path(std::string_view assetPath) const override;
            [[nodiscard]] core::result<cooked_blob> read(const asset_id &id) const override;
    };

    /// <summary>
    /// Editor/dev source: identities from the database, blobs from the cache. A record whose
    /// status prevents loading (META_INVALID, NO_IMPORTER, CYCLIC_DEPENDENCY, MISSING_SOURCE)
    /// fails read() with that code; a missing blob is CACHE_MISS.
    /// </summary>
    class cache_asset_source final : public asset_source {
        private:
            const asset_cache *mCache = nullptr;
            const asset_database *mDatabase = nullptr;

        public:
            cache_asset_source(const asset_cache &cache, const asset_database &database);
            ~cache_asset_source() override;

        public:
            [[nodiscard]] bool contains(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_type_id> type_of(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_id> find_by_path(std::string_view assetPath) const override;
            [[nodiscard]] core::result<cooked_blob> read(const asset_id &id) const override;
    };

    /// <summary>
    /// Runtime source over an opened pack.
    /// </summary>
    class pack_asset_source final : public asset_source {
        private:
            const pack_reader *mReader = nullptr;

        public:
            explicit pack_asset_source(const pack_reader &reader);
            ~pack_asset_source() override;

        public:
            [[nodiscard]] bool contains(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_type_id> type_of(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_id> find_by_path(std::string_view assetPath) const override;
            [[nodiscard]] core::result<cooked_blob> read(const asset_id &id) const override;
    };

    /// <summary>
    /// Resolves through several sources in insertion order (the project pack first, then plugin
    /// packs). Sources are added on the main thread before loads start.
    /// </summary>
    class composite_asset_source final : public asset_source {
        private:
            std::vector<std::unique_ptr<asset_source>> mSources = {};

        public:
            composite_asset_source();
            ~composite_asset_source() override;

        public:
            void add(std::unique_ptr<asset_source> source);

            [[nodiscard]] usize size() const noexcept {
                return mSources.size();
            }

            [[nodiscard]] bool contains(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_type_id> type_of(const asset_id &id) const noexcept override;
            [[nodiscard]] std::optional<asset_id> find_by_path(std::string_view assetPath) const override;
            [[nodiscard]] core::result<cooked_blob> read(const asset_id &id) const override;
    };
}
