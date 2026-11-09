#pragma once

#include "conjure/testing/doctest.h"

#include "conjure/assets/asset_cache.h"
#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Shared fixtures of the assets tests (docs/design/05_assets.md 7): a deterministic `test_asset`
// type whose source format is plain text, importers that declare dependencies and sub-assets from
// that text, a loader that can be told to fail or to block, and the pipeline_fixture wiring a
// memory file system, the registries, the database, the cache and a manager together.
//
// Source text format of a test asset (one directive per line, everything else is content):
//   dep <uuid>                 a dependency on another asset
//   dep-path <asset path>      a dependency resolved through import_context::find_asset_by_path
//   sub <name>                 a sub-asset named <name> whose payload is "<name>:" + the content
//   side <relative path>       a side file read through import_context::read_side_file
//   fail                       the importer fails
//   warn <text>                the importer adds a warning

namespace conjure::assets::testing {

    /// <summary>
    /// The published payload of a test asset.
    /// </summary>
    struct test_asset {
        std::string content = {};
        std::vector<asset_id> dependencies = {};
        u32 finaliseCount = 0;
    };

    /// <summary>
    /// A second type so type mismatches can be tested.
    /// </summary>
    struct other_asset {
        std::string content = {};
    };

    /// <summary>
    /// Shared switches read by test loaders. Reset by pipeline_fixture.
    /// </summary>
    struct loader_controls {
        std::atomic<bool> failLoadCpu = false;
        std::atomic<bool> failFinalise = false;
        std::atomic<bool> blockLoadCpu = false;    // load_cpu spins until released
        std::atomic<u32> loadCpuCalls = 0;
        std::atomic<u32> finaliseCalls = 0;
        std::atomic<i32> liveCountedAssets = 0;     // counted_asset instances alive (moved-from included)
    };

    [[nodiscard]] loader_controls &controls() noexcept;

    /// <summary>
    /// Payload wrapper that counts live instances so retirement can be observed.
    /// </summary>
    struct counted_asset;
}

CONJURE_DECLARE_ASSET_TYPE(conjure::assets::testing::test_asset, "conjure.test.asset");
CONJURE_DECLARE_ASSET_TYPE(conjure::assets::testing::other_asset, "conjure.test.other");
CONJURE_DECLARE_ASSET_TYPE(conjure::assets::testing::counted_asset, "conjure.test.counted");

namespace conjure::assets::testing {

    struct counted_asset {
        std::string content = {};

        counted_asset() {
            controls().liveCountedAssets.fetch_add(1, std::memory_order_relaxed);
        }
        explicit counted_asset(std::string text) :
            content(std::move(text)) {
            controls().liveCountedAssets.fetch_add(1, std::memory_order_relaxed);
        }
        counted_asset(const counted_asset &other) :
            content(other.content) {
            controls().liveCountedAssets.fetch_add(1, std::memory_order_relaxed);
        }
        counted_asset(counted_asset &&other) noexcept :
            content(std::move(other.content)) {
            controls().liveCountedAssets.fetch_add(1, std::memory_order_relaxed);
        }
        counted_asset &operator=(const counted_asset &) = default;
        counted_asset &operator=(counted_asset &&) noexcept = default;
        ~counted_asset() {
            controls().liveCountedAssets.fetch_sub(1, std::memory_order_relaxed);
        }
    };

    /// <summary>
    /// Imports the text format above into test_asset blobs. Extensions ".ta" and ".txt".
    /// </summary>
    class test_importer final : public asset_importer {
        public:
            static constexpr std::string_view ID = "conjure.test.text";

        private:
            u32 mVersion = 1;

        public:
            test_importer(u32 version = 1);
            ~test_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::json::document default_settings() const override;
            [[nodiscard]] core::result<import_output> import(import_context &context) const override;
    };

    /// <summary>
    /// Always fails. Extension ".bad".
    /// </summary>
    class failing_importer final : public asset_importer {
        public:
            static constexpr std::string_view ID = "conjure.test.failing";

        public:
            failing_importer();
            ~failing_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::result<import_output> import(import_context &context) const override;
    };

    /// <summary>
    /// Loader of test_asset: the payload text is the content; dependencies come from the context.
    /// </summary>
    class test_loader final : public typed_asset_loader<test_asset, test_asset> {
        public:
            test_loader();
            ~test_loader() override;

        protected:
            [[nodiscard]] core::result<test_asset> load_cpu_typed(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<test_asset> finalise_typed(
              const finalise_context &context,
              test_asset &&intermediate
            ) const override;
            [[nodiscard]] std::optional<test_asset> create_fallback_typed() const override;
    };

    /// <summary>
    /// Loader of other_asset (no fallback).
    /// </summary>
    class other_loader final : public typed_asset_loader<other_asset, other_asset> {
        public:
            other_loader();
            ~other_loader() override;

        protected:
            [[nodiscard]] core::result<other_asset> load_cpu_typed(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<other_asset> finalise_typed(
              const finalise_context &context,
              other_asset &&intermediate
            ) const override;
    };

    /// <summary>
    /// Loader of counted_asset.
    /// </summary>
    class counted_loader final : public typed_asset_loader<counted_asset, counted_asset> {
        public:
            counted_loader();
            ~counted_loader() override;

        protected:
            [[nodiscard]] core::result<counted_asset> load_cpu_typed(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<counted_asset> finalise_typed(
              const finalise_context &context,
              counted_asset &&intermediate
            ) const override;
    };

    /// <summary>
    /// Registers test_asset, other_asset and counted_asset (with the given flags for test_asset).
    /// </summary>
    void register_test_types(asset_type_registry &types, asset_type_flags testAssetFlags = asset_type_flags::NONE);

    /// <summary>
    /// Bytes of a string.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text);

    /// <summary>
    /// String of a byte range.
    /// </summary>
    [[nodiscard]] std::string text_of(std::span<const std::byte> bytes);

    /// <summary>
    /// A blob of test_asset type with the given content and dependencies, hashes computed.
    /// </summary>
    [[nodiscard]] cooked_blob make_test_blob(
      const asset_id &id,
      std::string_view content,
      std::vector<asset_id> dependencies = {},
      asset_type_id type = asset_type_traits<test_asset>::id()
    );

    /// <summary>
    /// A deterministic uuid for tests (never null).
    /// </summary>
    [[nodiscard]] asset_id test_id(u64 index) noexcept;

    /// <summary>
    /// Runs update() until nothing is pending, bounded to 'maxUpdates' calls; returns the calls.
    /// </summary>
    u32 settle(asset_manager &manager, u32 maxUpdates = 64);

    /// <summary>
    /// A complete editor-side pipeline over a memory file system.
    /// </summary>
    class pipeline_fixture {
        public:
            std::shared_ptr<core::vfs::memory_file_system> fileSystem =
              std::make_shared<core::vfs::memory_file_system>();
            core::jobs::job_system jobs = {};
            importer_registry importers = {};
            std::unique_ptr<asset_database> database = nullptr;
            std::unique_ptr<asset_manager> manager = nullptr;

        public:
            pipeline_fixture(u32 workerCount = 0, asset_type_flags testAssetFlags = asset_type_flags::NONE);
            ~pipeline_fixture();

            pipeline_fixture(const pipeline_fixture &) = delete;
            pipeline_fixture &operator=(const pipeline_fixture &) = delete;

        public:
            /// <summary>
            /// Writes a source under assets/ and returns its asset path ("assets://<relative>").
            /// </summary>
            std::string write_source(std::string_view relativePath, std::string_view text);

            /// <summary>
            /// scan() + import_stale(); fails the case on error.
            /// </summary>
            void scan_and_import();

            /// <summary>
            /// The uuid of a source by relative path (requires a scan). Fails the case when unknown.
            /// </summary>
            [[nodiscard]] asset_id id_of(std::string_view relativePath) const;

            [[nodiscard]] const asset_cache &cache() const noexcept {
                return *database->cache();
            }
    };
}
