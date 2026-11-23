#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_cooker.h"
#include "conjure/assets/asset_pack.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/core/json/document.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    struct cook_fixture {
        pipeline_fixture pipeline;
        asset_cooker cooker;
        asset_id leaf = {};
        asset_id mid = {};
        asset_id top = {};
        asset_id alone = {};

        cook_fixture(u32 workers = 0, bool scanNow = true) :
            pipeline(workers),
            cooker(*pipeline.database, pipeline.cache(), *pipeline.fileSystem, pipeline.jobs) {
            pipeline.write_source("leaf.ta", "leaf");
            pipeline.write_source("mid.ta", "dep-path leaf.ta\nmid");
            pipeline.write_source("top.ta", "dep-path mid.ta\ntop");
            pipeline.write_source("alone.ta", "alone");
            if (scanNow) {
                scan();
            }
        }

        void scan() {
            CONJURE_REQUIRE_OK(pipeline.database->scan());
            leaf = pipeline.id_of("leaf.ta");
            mid = pipeline.id_of("mid.ta");
            top = pipeline.id_of("top.ta");
            alone = pipeline.id_of("alone.ta");
        }

        cook_settings settings_for(std::vector<asset_id> roots) const {
            cook_settings settings = {};
            settings.outputDirectory = "export";
            settings.platform = "test";
            settings.roots = std::move(roots);
            return settings;
        }

        pack_reader open_pack(std::string_view directory = "export") {
            pack_reader reader = {};
            CONJURE_REQUIRE_OK(reader.open(*pipeline.fileSystem, std::string(directory) + "/content.cpak"));
            return reader;
        }
    };

    bool has_warning(const cook_report &report, std::string_view text) {
        return std::find(report.warnings.begin(), report.warnings.end(), text) != report.warnings.end();
    }
}

TEST_SUITE("assets/asset_cooker") {

    TEST_CASE("asset_cooker - the closure from the roots excludes unreferenced assets") {
        cook_fixture fixture = {};
        core::result<cook_report> report = fixture.cooker.cook(fixture.settings_for({fixture.top}));
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().assetCount == 3);
        CHECK(report.value().failures.empty());
        CHECK(report.value().packBytes > 0);
        CHECK(fixture.pipeline.fileSystem->exists("export/content.cpak"));
        CHECK(fixture.pipeline.fileSystem->exists("export/manifest.json"));
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export/content.cpak.tmp"));

        pack_reader reader = fixture.open_pack();
        CHECK(reader.find(fixture.top) != nullptr);
        CHECK(reader.find(fixture.mid) != nullptr);
        CHECK(reader.find(fixture.leaf) != nullptr);
        CHECK(reader.find(fixture.alone) == nullptr);
        CHECK(reader.content_hash() == report.value().contentHash);
        CHECK(reader.name_of(*reader.find(fixture.top)) == "assets://top.ta");
        CHECK(reader.dependencies_of(*reader.find(fixture.mid)).size() == 1);
        CHECK(fixture.cooker.last_report().assetCount == 3);
    }

    TEST_CASE("asset_cooker - stale blobs are imported during the cook") {
        cook_fixture fixture = {};
        CHECK(fixture.pipeline.database->stale_ids().size() == 4);
        CHECK_FALSE(fixture.pipeline.cache().contains(fixture.leaf));
        core::result<cook_report> report = fixture.cooker.cook(fixture.settings_for({fixture.mid}));
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().assetCount == 2);
        CHECK(fixture.pipeline.cache().contains(fixture.leaf));
        CHECK(fixture.pipeline.cache().contains(fixture.mid));
        CHECK_FALSE(fixture.pipeline.cache().contains(fixture.alone));
    }

    TEST_CASE("asset_cooker - alwaysInclude and includeAll widen the set") {
        cook_fixture fixture = {};
        cook_settings settings = fixture.settings_for({fixture.leaf});
        settings.alwaysInclude = {fixture.alone};
        core::result<cook_report> report = fixture.cooker.cook(settings);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().assetCount == 2);
        pack_reader reader = fixture.open_pack();
        CHECK(reader.find(fixture.alone) != nullptr);
        CHECK(reader.find(fixture.mid) == nullptr);

        cook_settings all = fixture.settings_for({});
        all.includeAll = true;
        all.outputDirectory = "export_all";
        core::result<cook_report> everything = fixture.cooker.cook(all);
        CONJURE_REQUIRE_OK(everything);
        CHECK(everything.value().assetCount == 4);
        CHECK(fixture.open_pack("export_all").toc().size() == 4);
    }

    TEST_CASE("asset_cooker - engine assets are always roots") {
        cook_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->write_file("engine/builtin.ta", bytes_of("builtin")));
        CONJURE_REQUIRE_OK(fixture.pipeline.database->scan());
        const asset_id builtin = fixture.pipeline.database->find_by_path("engine://builtin.ta").value();
        core::result<cook_report> report = fixture.cooker.cook(fixture.settings_for({fixture.leaf}));
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().assetCount == 2);
        pack_reader reader = fixture.open_pack();
        CHECK(reader.find(builtin) != nullptr);
        CHECK(reader.name_of(*reader.find(builtin)) == "engine://builtin.ta");
    }

    TEST_CASE("asset_cooker - EXCLUDE_FROM_PACK types are left out") {
        pipeline_fixture pipeline(0, asset_type_flags::EXCLUDE_FROM_PACK);
        asset_cooker cooker(*pipeline.database, pipeline.cache(), *pipeline.fileSystem, pipeline.jobs);
        pipeline.write_source("script.ta", "script");
        CONJURE_REQUIRE_OK(pipeline.database->scan());
        cook_settings settings = {};
        settings.outputDirectory = "export";
        settings.includeAll = true;
        core::result<cook_report> report = cooker.cook(settings);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().assetCount == 0);
        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open(*pipeline.fileSystem, "export/content.cpak"));
        CHECK(reader.toc().empty());
    }

    TEST_CASE("asset_cooker - one failing asset writes nothing and lists every failure") {
        cook_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.pipeline.write_source("broken.ta", "fail\nx");
        fixture.pipeline.write_source("orphan.xyz", "no importer");
        CONJURE_REQUIRE_OK(fixture.pipeline.database->scan());
        cook_settings settings = fixture.settings_for({});
        settings.includeAll = true;
        core::result<cook_report> report = fixture.cooker.cook(settings);
        CONJURE_CHECK_ERROR(report, asset_error_code::IMPORT_FAILED);
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export/content.cpak"));
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export/manifest.json"));
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export"));

        // The stray orphan.xyz nothing names or references is a warning, not a failure (F021).
        const cook_report &last = fixture.cooker.last_report();
        REQUIRE(last.failures.size() == 1);
        CHECK(last.failures[0].assetPath == "assets://broken.ta");
        CHECK(has_warning(last, "'assets://orphan.xyz' skipped: record is NO_IMPORTER"));
        CHECK(last.assetCount == 0);
        CHECK(logs.count(core::log_level::ERR) >= 1);

        // The good assets were still imported, so a cook of only them succeeds afterwards.
        core::result<cook_report> partial = fixture.cooker.cook(fixture.settings_for({fixture.top}));
        CONJURE_REQUIRE_OK(partial);
        CHECK(partial.value().assetCount == 3);
    }

    TEST_CASE("asset_cooker - includeAll skips unnamed NO_IMPORTER and MISSING_SOURCE records with a warning") {
        cook_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.pipeline.write_source("orphan.xyz", "no importer");
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->remove("assets/alone.ta"));
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->remove("assets/alone.ta.meta"));
        cook_settings settings = fixture.settings_for({});
        settings.includeAll = true;
        core::result<cook_report> report = fixture.cooker.cook(settings);
        CONJURE_REQUIRE_OK(report);
        const std::optional<asset_record> alone = fixture.pipeline.database->find(fixture.alone);
        REQUIRE(alone.has_value());
        CHECK(alone->status == asset_record_status::MISSING_SOURCE);

        CHECK(report.value().failures.empty());
        CHECK(report.value().assetCount == 3);
        CHECK(report.value().warnings.size() == 2);
        CHECK(has_warning(report.value(), "'assets://alone.ta' skipped: record is MISSING_SOURCE"));
        CHECK(has_warning(report.value(), "'assets://orphan.xyz' skipped: record is NO_IMPORTER"));
        CHECK(logs.contains(core::log_level::WARN, "'assets://orphan.xyz' skipped"));
        pack_reader reader = fixture.open_pack();
        CHECK(reader.toc().size() == 3);
        CHECK(reader.find(fixture.top) != nullptr);
        CHECK(reader.find(fixture.alone) == nullptr);
    }

    TEST_CASE("regression - code sweep F311 an includeAll cook leaves a glTF-style .bin buffer out of the pack") {
        // The shape of a .gltf with an external buffer: robot.ta reads robot.bin as a side file. With the binary
        // importer registered, a .bin it claimed was imported again and packed as a binary asset of its own; now
        // no importer claims it, so the cook skips it like any other NO_IMPORTER stray and still succeeds.
        cook_fixture fixture(0, false);
        CONJURE_REQUIRE_OK(register_binary_asset_type(fixture.pipeline.manager->types(), &fixture.pipeline.importers));
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->write_file("assets/models/robot.bin", bytes_of("buffer")));
        fixture.pipeline.write_source("models/robot.ta", "side robot.bin\nrobot");
        fixture.scan();
        cook_settings settings = fixture.settings_for({});
        settings.includeAll = true;
        core::result<cook_report> report = fixture.cooker.cook(settings);
        CONJURE_REQUIRE_OK(report);

        const asset_id robot = fixture.pipeline.id_of("models/robot.ta");
        const asset_id buffer = fixture.pipeline.id_of("models/robot.bin");
        REQUIRE(fixture.pipeline.database->find(buffer).has_value());
        CHECK(fixture.pipeline.database->find(buffer)->status == asset_record_status::NO_IMPORTER);
        CHECK(report.value().failures.empty());
        const std::vector<std::string> expectedWarnings = {
          "'assets://models/robot.bin' skipped: record is NO_IMPORTER"
        };
        CHECK(report.value().warnings == expectedWarnings);
        CHECK(report.value().assetCount == 5);
        pack_reader reader = fixture.open_pack();
        CHECK(reader.find(robot) != nullptr);
        CHECK(reader.find(buffer) == nullptr);
    }

    TEST_CASE("asset_cooker - includeAll still fails on a stray that roots or alwaysInclude name") {
        cook_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.pipeline.write_source("orphan.xyz", "no importer");
        CONJURE_REQUIRE_OK(fixture.pipeline.database->scan());
        const asset_id orphan = fixture.pipeline.id_of("orphan.xyz");
        cook_settings settings = fixture.settings_for({});
        settings.includeAll = true;
        SUBCASE("a root") {
            settings.roots = {orphan};
        }
        SUBCASE("alwaysInclude") {
            settings.alwaysInclude = {orphan};
        }

        CONJURE_CHECK_ERROR(fixture.cooker.cook(settings), asset_error_code::IMPORT_FAILED);
        const cook_report &last = fixture.cooker.last_report();
        REQUIRE(last.failures.size() == 1);
        CHECK(last.failures[0].id == orphan);
        CHECK(last.failures[0].message == "record is NO_IMPORTER");
        CHECK(last.warnings.empty());
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export"));
    }

    TEST_CASE("asset_cooker - includeAll still fails on a deleted source a cooked asset depends on") {
        cook_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        CONJURE_REQUIRE_OK(fixture.cooker.cook(fixture.settings_for({fixture.top})));

        // 'top' stays up to date, so the closure still follows its recorded dependency on 'mid'.
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->remove("assets/mid.ta"));
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->remove("assets/mid.ta.meta"));
        cook_settings settings = fixture.settings_for({});
        settings.includeAll = true;
        settings.outputDirectory = "export_all";
        CONJURE_CHECK_ERROR(fixture.cooker.cook(settings), asset_error_code::IMPORT_FAILED);
        const cook_report &last = fixture.cooker.last_report();
        REQUIRE(last.failures.size() == 1);
        CHECK(last.failures[0].id == fixture.mid);
        CHECK(last.failures[0].message == "record is MISSING_SOURCE");
        CHECK(last.warnings.empty());
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export_all"));
    }

    TEST_CASE("asset_cooker - invalid settings and unknown roots fail before anything is written") {
        cook_fixture fixture = {};
        cook_settings empty = {};
        CONJURE_CHECK_ERROR(fixture.cooker.cook(empty), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(fixture.cooker.cook(fixture.settings_for({test_id(99)})), asset_error_code::NOT_FOUND);
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export"));
    }

    TEST_CASE("asset_cooker - the manifest matches the TOC") {
        cook_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.cooker.cook(fixture.settings_for({fixture.top})));
        pack_reader reader = fixture.open_pack();
        core::result<std::string> manifestText = fixture.pipeline.fileSystem->read_text_file("export/manifest.json");
        CONJURE_REQUIRE_OK(manifestText);
        core::result<core::json::document> manifest = core::json::document::parse(manifestText.value());
        CONJURE_REQUIRE_OK(manifest);
        const core::json::value entries = manifest.value().root();
        REQUIRE(entries.size() == reader.toc().size());
        usize index = 0;
        for (const core::json::value entry : entries) {
            const pack_toc_entry &tocEntry = reader.toc()[index];
            CHECK(entry["uuid"].as_string() == tocEntry.id.to_string());
            CHECK(entry["name"].as_string() == reader.name_of(tocEntry));
            CHECK(entry["size"].as_u64() == tocEntry.blobSize);
            CHECK(entry["dependencies"].size() == tocEntry.dependencyCount);
            ++index;
        }
        cook_settings noManifest = fixture.settings_for({fixture.top});
        noManifest.outputDirectory = "export2";
        noManifest.writeManifest = false;
        CONJURE_REQUIRE_OK(fixture.cooker.cook(noManifest));
        CHECK(fixture.pipeline.fileSystem->exists("export2/content.cpak"));
        CHECK_FALSE(fixture.pipeline.fileSystem->exists("export2/manifest.json"));
    }

    TEST_CASE("asset_cooker - two cooks of the same content are byte-identical") {
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            cook_fixture first(workers);
            cook_fixture second(workers, false);
            // Share the metas so both projects use the same uuids.
            CONJURE_REQUIRE_OK(first.pipeline.fileSystem->enumerate(
              "assets",
              core::vfs::enumerate_flags::RECURSIVE | core::vfs::enumerate_flags::FILES,
              [&](std::string_view path, const core::vfs::file_stat &) {
                  if (is_meta_path(path)) {
                      CONJURE_REQUIRE_OK(
                        second.pipeline.fileSystem->write_file(path, first.pipeline.fileSystem->read_file(path).value())
                      );
                  }
              }
            ));
            second.scan();
            CHECK(second.leaf == first.leaf);
            cook_settings settings = {};
            settings.outputDirectory = "export";
            settings.includeAll = true;

            core::result<cook_report> a = first.cooker.cook(settings);
            core::result<cook_report> b = second.cooker.cook(settings);
            CONJURE_REQUIRE_OK(a);
            CONJURE_REQUIRE_OK(b);
            CHECK(a.value().contentHash == b.value().contentHash);
            const auto same_file = [&first, &second](std::string_view path) {
                const std::vector<std::byte> left = first.pipeline.fileSystem->read_file(path).value();
                const std::vector<std::byte> right = second.pipeline.fileSystem->read_file(path).value();
                return left == right;
            };
            CHECK(same_file("export/content.cpak"));
            CHECK(same_file("export/manifest.json"));

            // Cooking the same project again, after an import of everything, is identical too.
            core::result<cook_report> again = first.cooker.cook(settings);
            CONJURE_REQUIRE_OK(again);
            CHECK(again.value().contentHash == a.value().contentHash);
            CHECK(same_file("export/content.cpak"));
        }
    }

    TEST_CASE("regression - a dependency swapped for a never-imported asset is imported during the cook") {
        pipeline_fixture pipeline = {};
        asset_cooker cooker(*pipeline.database, pipeline.cache(), *pipeline.fileSystem, pipeline.jobs);
        pipeline.write_source("leaf.ta", "leaf");
        pipeline.write_source("other.ta", "other");
        pipeline.write_source("top.ta", "dep-path leaf.ta\ntop");
        CONJURE_REQUIRE_OK(pipeline.database->scan());
        const asset_id leaf = pipeline.id_of("leaf.ta");
        const asset_id other = pipeline.id_of("other.ta");
        const asset_id top = pipeline.id_of("top.ta");
        CONJURE_REQUIRE_OK(pipeline.database->import(std::vector<asset_id>{top, leaf}, pipeline.jobs));

        // The closure keeps its size (two members) while 'other' replaces 'leaf'.
        pipeline.write_source("top.ta", "dep-path other.ta\ntop");
        cook_settings settings = {};
        settings.outputDirectory = "export";
        settings.roots = {top};
        core::result<cook_report> report = cooker.cook(settings);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().failures.empty());
        CHECK(report.value().assetCount == 2);
        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open(*pipeline.fileSystem, "export/content.cpak"));
        CHECK(reader.find(top) != nullptr);
        CHECK(reader.find(other) != nullptr);
        CHECK(reader.find(leaf) == nullptr);
    }

    TEST_CASE("regression - a dependency deleted and replaced before a cook does not fail it") {
        pipeline_fixture pipeline = {};
        conjure::testing::log_capture logs = {};
        asset_cooker cooker(*pipeline.database, pipeline.cache(), *pipeline.fileSystem, pipeline.jobs);
        pipeline.write_source("leaf.ta", "leaf");
        pipeline.write_source("other.ta", "other");
        pipeline.write_source("top.ta", "dep-path leaf.ta\ntop");
        CONJURE_REQUIRE_OK(pipeline.database->scan());
        const asset_id leaf = pipeline.id_of("leaf.ta");
        const asset_id other = pipeline.id_of("other.ta");
        const asset_id top = pipeline.id_of("top.ta");
        CONJURE_REQUIRE_OK(pipeline.database->import(std::vector<asset_id>{top, leaf}, pipeline.jobs));

        // The first pass still closes over the old graph, where 'leaf' is now MISSING_SOURCE.
        CONJURE_REQUIRE_OK(pipeline.fileSystem->remove("assets/leaf.ta"));
        CONJURE_REQUIRE_OK(pipeline.fileSystem->remove("assets/leaf.ta.meta"));
        pipeline.write_source("top.ta", "dep-path other.ta\ntop");
        cook_settings settings = {};
        settings.outputDirectory = "export";
        settings.roots = {top};
        core::result<cook_report> report = cooker.cook(settings);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().failures.empty());
        CHECK(report.value().assetCount == 2);
        CHECK(pipeline.database->find(leaf)->status == asset_record_status::MISSING_SOURCE);
        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open(*pipeline.fileSystem, "export/content.cpak"));
        CHECK(reader.find(other) != nullptr);
        CHECK(reader.find(leaf) == nullptr);
    }

    TEST_CASE("asset_cooker - identical payloads are stored once in the pack") {
        cook_fixture fixture = {};
        fixture.pipeline.write_source("copy1.ta", "same bytes");
        fixture.pipeline.write_source("copy2.ta", "same bytes");
        CONJURE_REQUIRE_OK(fixture.pipeline.database->scan());
        cook_settings settings = fixture.settings_for(
          {fixture.pipeline.id_of("copy1.ta"), fixture.pipeline.id_of("copy2.ta")}
        );
        core::result<cook_report> report = fixture.cooker.cook(settings);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().assetCount == 2);
        CHECK(report.value().duplicatesRemoved == 1);
    }
}
