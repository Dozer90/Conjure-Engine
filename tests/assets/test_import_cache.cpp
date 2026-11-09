#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_cache.h"
#include "conjure/assets/asset_database.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    std::vector<std::byte> blob_bytes(const pipeline_fixture &fixture, const asset_id &id) {
        core::result<std::vector<std::byte>> bytes = fixture.fileSystem->read_file(fixture.cache().blob_path(id));
        CONJURE_REQUIRE_OK(bytes);
        return bytes.value();
    }
}

TEST_SUITE("assets/import_cache") {

    TEST_CASE("import - unchanged input imports nothing") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.write_source("b.ta", "B");
        CONJURE_REQUIRE_OK(fixture.database->scan());
        core::result<import_report> first = fixture.database->import_stale(fixture.jobs);
        CONJURE_REQUIRE_OK(first);
        CHECK(first.value().sourcesImported == 2);
        CHECK(first.value().assetsProduced == 2);
        CHECK(first.value().changed.size() == 2);
        CHECK(fixture.fileSystem->exists(".conjure/cache/import_log.txt"));

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->stale_ids().empty());
        core::result<import_report> second = fixture.database->import_stale(fixture.jobs);
        CONJURE_REQUIRE_OK(second);
        CHECK(second.value().sourcesImported == 0);
        CHECK(second.value().changed.empty());
    }

    TEST_CASE("import - stale on source, settings, importer version, type version and platform change") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("a.ta");

        SUBCASE("source change") {
            fixture.write_source("a.ta", "A changed");
            CONJURE_REQUIRE_OK(fixture.database->scan());
            CHECK(fixture.database->stale_ids() == std::vector<asset_id>{id});
        }
        SUBCASE("settings change") {
            asset_meta meta = fixture.database->read_meta(id).value();
            meta.settings = core::json::object({{"upper", true}});
            CONJURE_REQUIRE_OK(write_asset_meta(*fixture.fileSystem, "assets/a.ta.meta", meta));
            CONJURE_REQUIRE_OK(fixture.database->scan());
            CHECK(fixture.database->stale_ids() == std::vector<asset_id>{id});
        }
        SUBCASE("importer version change") {
            CONJURE_REQUIRE_OK(fixture.importers.unregister_importer(std::string(test_importer::ID)));
            CONJURE_REQUIRE_OK(fixture.importers.register_importer(std::make_unique<test_importer>(2)));
            CONJURE_REQUIRE_OK(fixture.database->scan());
            CHECK(fixture.database->stale_ids() == std::vector<asset_id>{id});
            CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
            CHECK(fixture.cache().read_header(id).value().importerVersion == 2);
            CHECK(fixture.database->find(id)->importerVersion == 2);
        }
        SUBCASE("type version change") {
            asset_type_registry types = {};
            CONJURE_REQUIRE_OK(
              types.register_type(make_asset_type_desc<test_asset>(std::make_unique<test_loader>(), 2))
            );
            asset_database bumped(*fixture.fileSystem, fixture.importers, types);
            CONJURE_REQUIRE_OK(bumped.add_mount("assets", "assets"));
            CONJURE_REQUIRE_OK(bumped.add_mount("engine", "engine"));
            CONJURE_REQUIRE_OK(bumped.open(".conjure/cache", "test"));
            CONJURE_REQUIRE_OK(bumped.scan());
            CHECK(bumped.stale_ids() == std::vector<asset_id>{id});
        }
        SUBCASE("platform change") {
            asset_database other(*fixture.fileSystem, fixture.importers, fixture.manager->types());
            CONJURE_REQUIRE_OK(other.add_mount("assets", "assets"));
            CONJURE_REQUIRE_OK(other.add_mount("engine", "engine"));
            CONJURE_REQUIRE_OK(other.open(".conjure/cache", "otherplatform"));
            CONJURE_REQUIRE_OK(other.scan());
            CHECK(other.stale_ids() == std::vector<asset_id>{id});
            CHECK(fixture.database->stale_ids().empty());
        }
        SUBCASE("unchanged") {
            CONJURE_REQUIRE_OK(fixture.database->scan());
            CHECK(fixture.database->stale_ids().empty());
        }
    }

    TEST_CASE("import - a failed import keeps the previous blob and flags the record") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.write_source("a.ta", "good");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("a.ta");
        const std::vector<std::byte> before = blob_bytes(fixture, id);

        fixture.write_source("a.ta", "fail\nbroken");
        CONJURE_REQUIRE_OK(fixture.database->scan());
        core::result<import_report> report = fixture.database->import_stale(fixture.jobs);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().sourcesFailed == 1);
        REQUIRE(report.value().failures.size() == 1);
        CHECK(report.value().failures[0].id == id);
        CHECK(report.value().failures[0].assetPath == "assets://a.ta");
        CHECK(fixture.database->find(id)->status == asset_record_status::IMPORT_FAILED);
        CHECK(blob_bytes(fixture, id) == before);
        CHECK(logs.contains(core::log_level::ERR, "assets://a.ta"));

        core::result<std::string> log = fixture.fileSystem->read_text_file(".conjure/cache/import_log.txt");
        CONJURE_REQUIRE_OK(log);
        CHECK(log.value().find("FAILED") != std::string::npos);

        // The failed source stays stale and recovers once fixed.
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->stale_ids() == std::vector<asset_id>{id});
        fixture.write_source("a.ta", "fixed");
        fixture.scan_and_import();
        CHECK(fixture.database->find(id)->status == asset_record_status::OK);
        CHECK(text_of(fixture.cache().read(id).value().payload) == "fixed");
    }

    TEST_CASE("import - warnings, unknown types and bad dependency paths are reported") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.write_source("warn.ta", "warn careful\nfine");
        fixture.write_source("badref.ta", "dep-path missing.ta\nx");
        fixture.write_source("boom.bad", "x");
        CONJURE_REQUIRE_OK(fixture.database->scan());
        core::result<import_report> report = fixture.database->import_stale(fixture.jobs);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().sourcesImported == 1);
        CHECK(report.value().sourcesFailed == 2);
        REQUIRE(report.value().warnings.size() == 1);
        CHECK(report.value().warnings[0] == "assets://warn.ta: careful");
        CHECK(fixture.database->find(fixture.id_of("boom.bad"))->status == asset_record_status::IMPORT_FAILED);
        CHECK(fixture.database->find(fixture.id_of("badref.ta"))->status == asset_record_status::IMPORT_FAILED);
    }

    TEST_CASE("import - sub-asset ids are stable across two imports and dropped sub-assets leave") {
        pipeline_fixture fixture = {};
        fixture.write_source("robot.ta", "sub Body\nsub Head\nrobot");
        fixture.scan_and_import();
        const asset_id body = fixture.database->find_by_path("assets://robot.ta#Body").value();
        const asset_id head = fixture.database->find_by_path("assets://robot.ta#Head").value();
        CHECK(fixture.cache().contains(body));
        CHECK(fixture.cache().contains(head));

        fixture.write_source("robot.ta", "sub Body\nrobot v2");
        fixture.scan_and_import();
        CHECK(fixture.database->find_by_path("assets://robot.ta#Body") == body);
        CHECK_FALSE(fixture.database->find_by_path("assets://robot.ta#Head").has_value());
        CHECK(text_of(fixture.cache().read(body).value().payload) == "Body:robot v2");

        // The meta is append-only: Head keeps its uuid for when it comes back.
        const asset_meta meta = fixture.database->read_meta(fixture.id_of("robot.ta")).value();
        REQUIRE(meta.find_sub_asset("Head") != nullptr);
        CHECK(meta.find_sub_asset("Head")->id == head);
        fixture.write_source("robot.ta", "sub Body\nsub Head\nrobot v3");
        fixture.scan_and_import();
        CHECK(fixture.database->find_by_path("assets://robot.ta#Head") == head);
    }

    TEST_CASE("import - the blob header records the hashes the stale check compares") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("a.ta");
        const asset_record record = fixture.database->find(id).value();
        const cooked_blob_header header = fixture.cache().read_header(id).value();
        CHECK(header.sourceHash == record.sourceHash);
        CHECK(header.settingsHash == record.settingsHash);
        CHECK(header.importerVersion == 1);
        CHECK(header.typeVersion == 1);
        CHECK(header.type == asset_type_traits<test_asset>::id());
        CHECK(header.payloadHash == record.payloadHash);
        CHECK(record.type == asset_type_traits<test_asset>::id());
    }

    TEST_CASE("import - a corrupt cached blob is stale and re-imported") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("a.ta");
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file(fixture.cache().blob_path(id), bytes_of("junk")));
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->stale_ids() == std::vector<asset_id>{id});
        CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
        CHECK(text_of(fixture.cache().read(id).value().payload) == "A");
    }

    TEST_CASE("import - parallel import of 1000 sources equals the serial result byte for byte [parallel]") {
        constexpr u32 COUNT = 1000;
        pipeline_fixture serial(0);
        pipeline_fixture parallel(4);
        for (u32 index = 0; index < COUNT; ++index) {
            const std::string name = "many/" + std::to_string(index) + ".ta";
            const std::string text = "sub Part\ncontent " + std::to_string(index * 7919);
            serial.write_source(name, text);
            parallel.write_source(name, text);
        }
        serial.scan_and_import();
        // Share the metas so both fixtures use the same uuids.
        CONJURE_REQUIRE_OK(serial.fileSystem->enumerate(
          "assets",
          core::vfs::enumerate_flags::RECURSIVE | core::vfs::enumerate_flags::FILES,
          [&](std::string_view path, const core::vfs::file_stat &) {
              if (is_meta_path(path)) {
                  CONJURE_REQUIRE_OK(parallel.fileSystem->write_file(path, serial.fileSystem->read_file(path).value()));
              }
          }
        ));
        CONJURE_REQUIRE_OK(parallel.database->scan());
        core::result<import_report> report = parallel.database->import_stale(parallel.jobs);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().sourcesImported == COUNT);
        CHECK(report.value().sourcesFailed == 0);
        CHECK(parallel.database->record_count() == COUNT * 2);

        u32 compared = 0;
        for (const asset_record &record : serial.database->records()) {
            CAPTURE(record.relativePath);
            REQUIRE(parallel.database->find(record.id).has_value());
            CHECK(blob_bytes(serial, record.id) == blob_bytes(parallel, record.id));
            ++compared;
        }
        CHECK(compared == COUNT * 2);
    }
}
