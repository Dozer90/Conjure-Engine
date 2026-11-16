// Assets scenario (code sweep F243): the first scan of a project folder on disk whose sources have no
// .meta yet, the case a fresh clone without its metas or a folder of newly dropped files produces.
// asset_database::scan creates one .meta per source through core::vfs::write_file_atomic, and since F243
// the native file system flushes every one of them to the storage device before it takes its name, so a
// power cut cannot leave a torn meta (the file an asset's identity lives in). The price is one device
// flush per source; this row is where it is measured.
//
// The row is a disk benchmark: its time is the device's flush latency times the source count, far above
// the processor work around it, and it differs by an order of magnitude between drives. The two phases
// put the flush on its own: the same number of .meta-sized files written with write_file_atomic and with
// write_file (no temporary, no flush) into the same folder.
//
// Every iteration scans a project of its own, built before the measured loop, because a scan is only a
// first scan once: nothing inside the timed body creates sources or removes metas.

#include "bench_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"
#include "conjure/bench/benchmark.h"
#include "conjure/core/clock.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/native_file_system.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::bench::support;

namespace {

    // One source per 100 entities of --entities: 100 sources per scan by default, so the default run
    // (220 scans) stays within a few minutes on a slow drive; --entities 100000 --iterations 5 --warmup 1
    // measures a 1,000-source project.
    constexpr u32 ENTITIES_PER_SOURCE = 100;
    constexpr u32 SOURCES_PER_FOLDER = 25;
    constexpr u32 MAX_WRITE_PHASE_SAMPLES = 20;
    constexpr std::string_view SOURCE_TEXT = "a source the benchmark never imports";
    constexpr std::string_view PLATFORM = "bench";

    [[nodiscard]] u32 source_count(const bench_context &context) noexcept {
        return std::max<u32>(context.entity_count() / ENTITIES_PER_SOURCE, 1);
    }

    [[nodiscard]] std::span<const std::byte> bytes_of(std::string_view text) noexcept {
        return std::as_bytes(std::span<const char>(text.data(), text.size()));
    }

    /// <summary>
    /// One project folder with its own database: "<name>/assets" holds the sources, "<name>/cache" the
    /// database's cache. Built untimed; scan() is the only call the measured body makes.
    /// </summary>
    class scan_project {
        private:
            std::unique_ptr<assets::asset_database> mDatabase = nullptr;

        public:
            scan_project() = default;

        public:
            /// <summary>
            /// Writes the sources and opens the database. False when the folder could not be prepared.
            /// </summary>
            [[nodiscard]] bool build(
              core::vfs::native_file_system &files,
              assets::importer_registry &importers,
              const assets::asset_type_registry &types,
              std::string_view name,
              u32 sources
            ) {
                const std::string assetsRoot = std::format("{}/assets", name);
                for (u32 index = 0; index < sources; ++index) {
                    const std::string path =
                      std::format("{}/group_{:03}/source_{:05}.dat", assetsRoot, index / SOURCES_PER_FOLDER, index);
                    if (!files.write_file(path, bytes_of(SOURCE_TEXT)).has_value()) {
                        return false;
                    }
                }

                mDatabase = std::make_unique<assets::asset_database>(files, importers, types);
                if (!mDatabase->add_mount("assets", assetsRoot).has_value()) {
                    return false;
                }
                return mDatabase->open(std::format("{}/cache", name), PLATFORM).has_value();
            }

            /// <summary>
            /// The first scan: one .meta per source. False when it failed or created another number.
            /// </summary>
            [[nodiscard]] bool scan(u32 expectedMetas) {
                const core::result<assets::scan_report> report = mDatabase->scan();
                return report.has_value() && report.value().metasCreated == expectedMetas;
            }
    };

    /// <summary>
    /// Times 'count' writes of 'payload' into a fresh folder, one sample per folder, through the atomic
    /// (flushed) or the plain write. An empty result means a write failed.
    /// </summary>
    [[nodiscard]] std::vector<f64> time_writes(
      core::vfs::native_file_system &files,
      std::string_view folder,
      std::span<const std::byte> payload,
      u32 count,
      u32 samples,
      bool atomic
    ) {
        std::vector<f64> milliseconds = {};
        milliseconds.reserve(samples);
        std::vector<std::string> paths = {};
        paths.reserve(count);
        for (u32 sample = 0; sample < samples; ++sample) {
            paths.clear();
            for (u32 index = 0; index < count; ++index) {
                paths.push_back(std::format("{}/sample_{:03}/source_{:05}.dat.meta", folder, sample, index));
            }
            if (!files.create_directories(std::format("{}/sample_{:03}", folder, sample)).has_value()) {
                return {};
            }

            bool written = true;
            const u64 start = core::ticks_now();
            for (const std::string &path : paths) {
                const core::result<void> outcome =
                  atomic ? files.write_file_atomic(path, payload) : files.write_file(path, payload);
                written = written && outcome.has_value();
            }
            const u64 end = core::ticks_now();
            if (!written) {
                return {};
            }
            milliseconds.push_back(core::ticks_to_seconds(end - start) * 1000.0);
        }
        return milliseconds;
    }
}

CONJURE_BENCHMARK(assets.first_scan_100) {
    const u32 sources = source_count(context);
    const u32 scans = context.options().warmupIterations + context.options().measuredIterations;

    const scratch_directory scratch = {};
    core::vfs::native_file_system files(scratch.path());
    assets::importer_registry importers = {};
    const assets::asset_type_registry types = {};

    std::vector<scan_project> projects(scans);
    for (u32 index = 0; index < scans; ++index) {
        if (!projects[index].build(files, importers, types, std::format("project_{:04}", index), sources)) {
            context.skip("the scratch directory refused the project folders");
            return;
        }
    }

    u32 next = 0;
    bool scanned = true;
    context.set_items(sources);
    context.measure([&]() {
        scanned = scanned && next < scans && projects[next].scan(sources);
        ++next;
    });
    if (!scanned) {
        context.fail("a measured scan failed or did not create one .meta per source");
    }

    // What the scan's meta writes cost on this drive, apart from everything else a scan does.
    assets::asset_meta meta = {};
    meta.id = assets::asset_id::generate();
    const std::string metaText = assets::serialize_asset_meta(meta);
    const u32 writeSamples = std::min(context.options().measuredIterations, MAX_WRITE_PHASE_SAMPLES);
    const std::vector<f64> atomicWrites = time_writes(files, "atomic", bytes_of(metaText), sources, writeSamples, true);
    const std::vector<f64> plainWrites = time_writes(files, "plain", bytes_of(metaText), sources, writeSamples, false);
    if (atomicWrites.size() != writeSamples || plainWrites.size() != writeSamples) {
        context.fail("a write of the META_WRITES phases failed");
        return;
    }
    context.add_phase("META_WRITES_ATOMIC", atomicWrites);
    context.add_phase("META_WRITES_PLAIN", plainWrites);
}
