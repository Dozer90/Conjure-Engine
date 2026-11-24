#include "conjure/testing/doctest.h"
#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/core/jobs/job_system.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    /// <summary>
    /// test_importer under another id that claims no extension and upper-cases by default: it reads the same
    /// sources but only gets one whose .meta names it, like the collision-mesh importer and a .glb.
    /// </summary>
    class shout_importer final : public asset_importer {
        public:
            static constexpr std::string_view ID = "conjure.test.shout";

        private:
            test_importer mInner = {};

        public:
            [[nodiscard]] std::string_view id() const noexcept override {
                return ID;
            }
            [[nodiscard]] u32 version() const noexcept override {
                return 3;
            }
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
                return {};
            }
            [[nodiscard]] core::json::document default_settings() const override {
                return core::json::object({{"upper", true}});
            }
            [[nodiscard]] core::result<import_output> import(import_context &context) const override {
                return mInner.import(context);
            }
    };
}

TEST_SUITE("assets/asset_database") {

    TEST_CASE("split_asset_path and make_asset_path - round trip and reject malformed paths") {
        const std::optional<asset_path_parts> full = split_asset_path("assets://models/robot.gltf#mesh/Body");
        REQUIRE(full.has_value());
        CHECK(full->mount == "assets");
        CHECK(full->relativePath == "models/robot.gltf");
        CHECK(full->subPath == "mesh/Body");
        CHECK(
          make_asset_path(full->mount, full->relativePath, full->subPath) == "assets://models/robot.gltf#mesh/Body"
        );

        const std::optional<asset_path_parts> primary = split_asset_path("engine://a.ta");
        REQUIRE(primary.has_value());
        CHECK(primary->subPath.empty());
        CHECK(make_asset_path("engine", "a.ta") == "engine://a.ta");
        CHECK_FALSE(split_asset_path("no-scheme").has_value());
        CHECK_FALSE(split_asset_path("://x").has_value());
        CHECK_FALSE(split_asset_path("assets://").has_value());
        CHECK(to_string(asset_record_status::NO_IMPORTER) == "NO_IMPORTER");
    }

    TEST_CASE("asset_database - mounts are validated and scan needs open") {
        pipeline_fixture fixture = {};
        CONJURE_CHECK_ERROR(fixture.database->add_mount("assets", "elsewhere"), core::error_code::ALREADY_EXISTS);
        CONJURE_CHECK_ERROR(fixture.database->add_mount("", "x"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(fixture.database->add_mount("bad://", "x"), core::error_code::INVALID_ARGUMENT);
        CHECK(fixture.database->mounts().size() == 2);
        CHECK(fixture.database->is_open());
        CHECK(fixture.database->platform() == "test");

        asset_type_registry types = {};
        importer_registry importers = {};
        asset_database closed(*fixture.fileSystem, importers, types);
        CONJURE_CHECK_ERROR(closed.scan(), core::error_code::INVALID_STATE);
        CONJURE_CHECK_ERROR(closed.import_stale(fixture.jobs), core::error_code::INVALID_STATE);
        CONJURE_CHECK_ERROR(closed.add_file("assets://x.ta"), core::error_code::INVALID_STATE);
        CONJURE_CHECK_ERROR(closed.save_snapshot(), core::error_code::INVALID_STATE);
        CONJURE_CHECK_ERROR(closed.open("cache", ""), core::error_code::INVALID_ARGUMENT);
        CHECK_FALSE(closed.is_open());
    }

    TEST_CASE("asset_database - scan creates metas and uuids and a rescan is stable") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.write_source("dir/b.ta", "B");
        fixture.write_source("notes.xyz", "no importer");

        core::result<scan_report> first = fixture.database->scan();
        CONJURE_REQUIRE_OK(first);
        CHECK(first.value().filesScanned == 3);
        CHECK(first.value().metasCreated == 3);
        CHECK(first.value().recordsAdded == 3);
        CHECK(first.value().missingImporters == 1);
        CHECK(first.value().stale.size() == 2);
        CHECK(fixture.fileSystem->exists("assets/a.ta.meta"));
        CHECK(fixture.fileSystem->exists("assets/dir/b.ta.meta"));
        CHECK(fixture.fileSystem->exists("assets/notes.xyz.meta"));

        core::result<asset_meta> meta = read_asset_meta(*fixture.fileSystem, "assets/a.ta.meta");
        CONJURE_REQUIRE_OK(meta);
        CHECK(meta.value().importerId == "conjure.test.text");
        CHECK(meta.value().importerVersion == 1);
        CHECK(meta.value().settings.root()["upper"].is_bool());
        const asset_id idA = fixture.id_of("a.ta");
        CHECK(meta.value().id == idA);

        const std::optional<asset_record> record = fixture.database->find(idA);
        REQUIRE(record.has_value());
        CHECK(record->relativePath == "a.ta");
        CHECK(record->mountIndex == 0);
        CHECK(record->status == asset_record_status::OK);
        CHECK(record->importerId == "conjure.test.text");
        CHECK(record->sourceSize == 1);
        CHECK(record->sourceHash != 0);
        CHECK(record->displayName == "a");
        CHECK(record->type == asset_type_id::INVALID);
        CHECK(fixture.database->find(fixture.id_of("notes.xyz"))->status == asset_record_status::NO_IMPORTER);

        core::result<scan_report> second = fixture.database->scan();
        CONJURE_REQUIRE_OK(second);
        CHECK(second.value().metasCreated == 0);
        CHECK(second.value().recordsAdded == 0);
        CHECK(second.value().hashed == 0);
        CHECK(fixture.id_of("a.ta") == idA);
        CHECK(fixture.database->record_count() == 3);
    }

    TEST_CASE("asset_database - path, file path and sub path lookups") {
        pipeline_fixture fixture = {};
        fixture.write_source("robot.ta", "sub Body\nsub Head\nrobot");
        fixture.scan_and_import();
        const asset_id robot = fixture.id_of("robot.ta");
        const std::optional<asset_id> body = fixture.database->find_by_path("assets://robot.ta#Body");
        REQUIRE(body.has_value());
        CHECK(*body != robot);
        const std::optional<asset_record> bodyRecord = fixture.database->find(*body);
        REQUIRE(bodyRecord.has_value());
        CHECK(bodyRecord->is_sub_asset());
        CHECK(bodyRecord->parentId == robot);
        CHECK(bodyRecord->subPath == "Body");
        CHECK(bodyRecord->type == asset_type_traits<test_asset>::id());
        CHECK(bodyRecord->displayName == "Body");
        CHECK(fixture.database->asset_path_of(*body) == "assets://robot.ta#Body");
        CHECK(fixture.database->asset_path_of(robot) == "assets://robot.ta");
        CHECK(fixture.database->file_path_of(*body) == "assets/robot.ta");
        CHECK(fixture.database->find_by_file_path("assets/robot.ta") == robot);
        CHECK_FALSE(fixture.database->find_by_file_path("assets/other.ta").has_value());
        CHECK_FALSE(fixture.database->find_by_path("assets://robot.ta#Nope").has_value());
        CHECK_FALSE(fixture.database->asset_path_of(test_id(99)).has_value());

        // Sub-asset uuids are persisted in the meta and stable across a rescan and re-import.
        core::result<asset_meta> meta = fixture.database->read_meta(*body);
        CONJURE_REQUIRE_OK(meta);
        REQUIRE(meta.value().find_sub_asset("Body") != nullptr);
        CHECK(meta.value().find_sub_asset("Body")->id == *body);
        fixture.write_source("robot.ta", "sub Body\nsub Head\nrobot v2");
        fixture.scan_and_import();
        CHECK(fixture.database->find_by_path("assets://robot.ta#Body") == body);
        CHECK(fixture.database->record_count() == 3);
    }

    TEST_CASE("asset_database - duplicate uuid policy keeps the first path in sorted order") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("b.ta", "B");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("b.ta");
        core::result<std::string> meta = fixture.fileSystem->read_text_file("assets/b.ta.meta");
        CONJURE_REQUIRE_OK(meta);
        fixture.write_source("a.ta", "A");
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/a.ta.meta", bytes_of(meta.value())));

        core::result<scan_report> report = fixture.database->scan();
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().duplicateUuids == 1);
        CHECK(fixture.id_of("a.ta") == id);
        const std::optional<asset_id> second = fixture.database->find_by_path("assets://b.ta");
        REQUIRE(second.has_value());
        CHECK(*second != id);
        CHECK(fixture.database->find(*second)->status == asset_record_status::DUPLICATE_UUID);
        CHECK(fixture.database->find(id)->relativePath == "a.ta");
        CHECK(logs.contains(core::log_level::WARN, "already owned"));

        // The duplicate is never rewritten and its transient id is stable across rescans.
        CHECK(fixture.fileSystem->read_text_file("assets/b.ta.meta").value() == meta.value());
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->find_by_path("assets://b.ta") == second);
        core::result<import_report> imported = fixture.database->import(std::vector<asset_id>{*second}, fixture.jobs);
        CONJURE_REQUIRE_OK(imported);
        CHECK(imported.value().sourcesSkipped == 1);
        CHECK(imported.value().sourcesImported == 0);
    }

    TEST_CASE("asset_database - a moved file keeps its uuid") {
        pipeline_fixture fixture = {};
        fixture.write_source("old/a.ta", "A");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("old/a.ta");

        CONJURE_REQUIRE_OK(fixture.database->move_asset(id, "assets://new/renamed.ta"));
        CHECK(fixture.fileSystem->exists("assets/new/renamed.ta"));
        CHECK(fixture.fileSystem->exists("assets/new/renamed.ta.meta"));
        CHECK_FALSE(fixture.fileSystem->exists("assets/old/a.ta"));
        CHECK(fixture.id_of("new/renamed.ta") == id);
        CHECK_FALSE(fixture.database->find_by_path("assets://old/a.ta").has_value());
        CHECK(fixture.database->find(id)->displayName == "renamed");

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("new/renamed.ta") == id);
        CHECK(fixture.database->record_count() == 1);
        CHECK(fixture.database->stale_ids().empty());

        CONJURE_CHECK_ERROR(fixture.database->move_asset(test_id(5), "assets://x.ta"), assets_error::NOT_FOUND);
        CONJURE_CHECK_ERROR(fixture.database->move_asset(id, "nowhere"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(fixture.database->move_asset(id, "other://x.ta"), assets_error::NOT_FOUND);
        CONJURE_CHECK_ERROR(
          fixture.database->move_asset(id, "assets://new/renamed.ta"),
          core::error_code::ALREADY_EXISTS
        );
    }

    TEST_CASE("regression - move_asset moves the meta first and leaves the pair together when it cannot") {
        pipeline_fixture fixture = {};
        fixture.write_source("old/a.ta", "A");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("old/a.ta");

        // A directory squats on the destination meta path: the meta rename fails before anything
        // moved, so the source, its meta and the record are untouched (the uuid stays reachable).
        CONJURE_REQUIRE_OK(fixture.fileSystem->create_directories("assets/new/renamed.ta.meta"));
        CONJURE_CHECK_ERROR(fixture.database->move_asset(id, "assets://new/renamed.ta"), assets_error::IO_ERROR);
        CHECK(fixture.fileSystem->exists("assets/old/a.ta"));
        CHECK(fixture.fileSystem->exists("assets/old/a.ta.meta"));
        CHECK_FALSE(fixture.fileSystem->exists("assets/new/renamed.ta"));
        CHECK(fixture.database->find(id)->relativePath == "old/a.ta");
        CHECK(fixture.id_of("old/a.ta") == id);

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("old/a.ta") == id);
        CHECK(fixture.database->record_count() == 1);
    }

    TEST_CASE("regression - atomic-write temporaries in the assets tree are not sources") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/a.ta.meta.tmp-0123456789abcdef", bytes_of("{")));
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/b.ta.tmp", bytes_of("half written")));
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/c.tmp-ffff", bytes_of("half written")));

        core::result<scan_report> report = fixture.database->scan();
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().filesScanned == 1);
        CHECK(report.value().metasCreated == 1);
        CHECK(fixture.database->record_count() == 1);
        CHECK_FALSE(fixture.fileSystem->exists("assets/a.ta.meta.tmp-0123456789abcdef.meta"));
        CHECK_FALSE(fixture.fileSystem->exists("assets/b.ta.tmp.meta"));
        CHECK_FALSE(fixture.database->find_by_path("assets://a.ta.meta.tmp-0123456789abcdef").has_value());
        CHECK_FALSE(fixture.database->find_by_path("assets://b.ta.tmp").has_value());
        CHECK_FALSE(fixture.database->find_by_file_path("assets/c.tmp-ffff").has_value());
    }

    TEST_CASE("asset_database - contains, type_of and status_of agree with find") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.write_source("nope.xyz", "no importer");
        CONJURE_REQUIRE_OK(fixture.database->scan());
        const asset_id a = fixture.id_of("a.ta");
        const asset_id nope = fixture.id_of("nope.xyz");

        CHECK(fixture.database->contains(a));
        CHECK_FALSE(fixture.database->contains(test_id(99)));
        CHECK_FALSE(fixture.database->type_of(a).has_value());   // not imported yet
        CHECK(fixture.database->status_of(a) == asset_record_status::OK);
        CHECK(fixture.database->status_of(nope) == asset_record_status::NO_IMPORTER);
        CHECK_FALSE(fixture.database->status_of(test_id(99)).has_value());
        CHECK_FALSE(fixture.database->type_of(test_id(99)).has_value());

        CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
        CHECK(fixture.database->type_of(a) == asset_type_traits<test_asset>::id());
        CHECK(fixture.database->type_of(a) == fixture.database->find(a)->type);
        CHECK_FALSE(fixture.database->type_of(nope).has_value());
    }

    TEST_CASE("asset_database - mark_source_missing and mark_source_present switch a record without a scan") {
        pipeline_fixture fixture = {};
        fixture.write_source("robot.ta", "sub Body\nrobot");
        fixture.scan_and_import();
        const asset_id robot = fixture.id_of("robot.ta");
        const asset_id body = fixture.database->find_by_path("assets://robot.ta#Body").value();
        CHECK(fixture.database->stale_ids().empty());

        CONJURE_REQUIRE_OK(fixture.fileSystem->remove("assets/robot.ta"));
        CONJURE_REQUIRE_OK(fixture.database->mark_source_missing(body));   // a sub-asset marks its primary
        CHECK(fixture.database->status_of(robot) == asset_record_status::MISSING_SOURCE);
        CHECK(fixture.database->status_of(body) == asset_record_status::MISSING_SOURCE);
        CHECK(fixture.database->stale_ids().empty());
        core::result<import_report> imported = fixture.database->import(std::vector<asset_id>{robot}, fixture.jobs);
        CONJURE_REQUIRE_OK(imported);
        CHECK(imported.value().sourcesSkipped == 1);
        CONJURE_CHECK_ERROR(fixture.database->mark_source_present(robot), assets_error::NOT_FOUND);
        CONJURE_CHECK_ERROR(fixture.database->mark_source_missing(test_id(99)), assets_error::NOT_FOUND);

        // Back with new content: OK again, the children follow and the changed source is stale.
        fixture.write_source("robot.ta", "sub Body\nrobot v2");
        CONJURE_REQUIRE_OK(fixture.database->mark_source_present(robot));
        CHECK(fixture.database->status_of(robot) == asset_record_status::OK);
        CHECK(fixture.database->status_of(body) == asset_record_status::OK);
        CHECK(fixture.database->stale_ids() == std::vector<asset_id>{robot});
        CONJURE_REQUIRE_OK(fixture.database->mark_source_present(robot));   // idempotent
        CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
        CHECK(text_of(fixture.cache().read(body).value().payload) == "Body:robot v2");

        // The snapshot carries the status, like a scan would.
        CONJURE_REQUIRE_OK(fixture.database->mark_source_missing(robot));
        asset_database reopened(*fixture.fileSystem, fixture.importers, fixture.manager->types());
        CONJURE_REQUIRE_OK(reopened.add_mount("assets", "assets"));
        CONJURE_REQUIRE_OK(reopened.add_mount("engine", "engine"));
        CONJURE_REQUIRE_OK(reopened.open(".conjure/cache", "test"));
        CHECK(reopened.status_of(robot) == asset_record_status::MISSING_SOURCE);
    }

    TEST_CASE("regression - cycle detection handles a long chain with and without a closing edge") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        constexpr u32 LENGTH = 200;
        for (u32 index = 0; index < LENGTH; ++index) {
            fixture.write_source("chain/" + std::to_string(index) + ".ta", "link");
        }
        CONJURE_REQUIRE_OK(fixture.database->scan());
        std::vector<asset_id> ids = {};
        for (u32 index = 0; index < LENGTH; ++index) {
            ids.push_back(fixture.id_of("chain/" + std::to_string(index) + ".ta"));
        }

        // 0 -> 1 -> 2 -> ... -> LENGTH-1 -> 0: one strongly connected component of LENGTH members.
        for (u32 index = 0; index < LENGTH; ++index) {
            const asset_id next = ids[(index + 1) % LENGTH];
            fixture.write_source("chain/" + std::to_string(index) + ".ta", "dep " + next.to_string() + "\nlink");
        }
        fixture.scan_and_import();
        for (u32 index = 0; index < LENGTH; ++index) {
            CAPTURE(index);
            CHECK(fixture.database->status_of(ids[index]) == asset_record_status::CYCLIC_DEPENDENCY);
        }

        // Cutting the closing edge turns it into a plain chain: nothing is cyclic any more.
        fixture.write_source("chain/" + std::to_string(LENGTH - 1) + ".ta", "end");
        fixture.scan_and_import();
        for (u32 index = 0; index < LENGTH; ++index) {
            CAPTURE(index);
            CHECK(fixture.database->status_of(ids[index]) == asset_record_status::OK);
        }
        const std::vector<asset_id> roots = {ids[0]};
        core::result<std::vector<asset_id>> closure = fixture.database->dependency_closure(roots);
        CONJURE_REQUIRE_OK(closure);
        CHECK(closure.value().size() == LENGTH);
        std::vector<asset_id> dependents = {};
        fixture.database->collect_dependents(std::vector<asset_id>{ids[LENGTH - 1]}, dependents);
        CHECK(dependents.size() == LENGTH);
    }

    TEST_CASE("asset_database - a removed source becomes MISSING_SOURCE and its record stays") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("a.ta");
        CONJURE_REQUIRE_OK(fixture.fileSystem->remove("assets/a.ta"));
        CONJURE_REQUIRE_OK(fixture.fileSystem->remove("assets/a.ta.meta"));

        core::result<scan_report> report = fixture.database->scan();
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().recordsRemoved == 1);
        const std::optional<asset_record> record = fixture.database->find(id);
        REQUIRE(record.has_value());
        CHECK(record->status == asset_record_status::MISSING_SOURCE);
        CHECK(fixture.database->stale_ids().empty());
        core::result<import_report> imported = fixture.database->import(std::vector<asset_id>{id}, fixture.jobs);
        CONJURE_REQUIRE_OK(imported);
        CHECK(imported.value().sourcesSkipped == 1);

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->find(id)->status == asset_record_status::MISSING_SOURCE);
    }

    TEST_CASE("asset_database - a uuid change in an existing meta is refused") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id original = fixture.id_of("a.ta");

        core::result<asset_meta> meta = fixture.database->read_meta(original);
        CONJURE_REQUIRE_OK(meta);
        asset_meta edited = meta.value();
        edited.id = test_id(77);
        CONJURE_CHECK_ERROR(fixture.database->write_meta(original, edited), core::error_code::INVALID_ARGUMENT);
        CHECK(logs.count(core::log_level::ERR) == 1);

        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.fileSystem, "assets/a.ta.meta", edited));
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("a.ta") == original);
        CHECK_FALSE(fixture.database->find(test_id(77)).has_value());
        CHECK(logs.count(core::log_level::ERR) == 2);
        CHECK(logs.contains(core::log_level::ERR, "uuid changed"));
    }

    TEST_CASE("asset_database - forward and reverse graphs, collect_dependents order and dedup") {
        pipeline_fixture fixture = {};
        fixture.write_source("leaf.ta", "leaf");
        fixture.write_source("mid.ta", "dep-path leaf.ta\nmid");
        fixture.write_source("top.ta", "dep-path mid.ta\ndep-path leaf.ta\ntop");
        fixture.write_source("alone.ta", "alone");
        fixture.scan_and_import();
        const asset_id leaf = fixture.id_of("leaf.ta");
        const asset_id mid = fixture.id_of("mid.ta");
        const asset_id top = fixture.id_of("top.ta");

        CHECK(fixture.database->dependencies_of(leaf).empty());
        CHECK(fixture.database->dependencies_of(mid) == std::vector<asset_id>{leaf});
        std::vector<asset_id> topDeps = fixture.database->dependencies_of(top);
        std::sort(topDeps.begin(), topDeps.end());
        std::vector<asset_id> expected = {mid, leaf};
        std::sort(expected.begin(), expected.end());
        CHECK(topDeps == expected);

        std::vector<asset_id> leafDependents = fixture.database->dependents_of(leaf);
        std::sort(leafDependents.begin(), leafDependents.end());
        std::vector<asset_id> expectedDependents = {mid, top};
        std::sort(expectedDependents.begin(), expectedDependents.end());
        CHECK(leafDependents == expectedDependents);
        CHECK(fixture.database->dependents_of(top).empty());
        CHECK(fixture.database->dependents_of(test_id(99)).empty());

        std::vector<asset_id> ordered = {};
        fixture.database->collect_dependents(std::vector<asset_id>{leaf}, ordered);
        REQUIRE(ordered.size() == 3);
        CHECK(ordered[0] == leaf);
        CHECK(std::find(ordered.begin(), ordered.end(), mid) < std::find(ordered.begin(), ordered.end(), top));
        CHECK(std::count(ordered.begin(), ordered.end(), top) == 1);

        ordered.clear();
        fixture.database->collect_dependents(std::vector<asset_id>{mid, leaf}, ordered);
        CHECK(ordered.size() == 3);
        CHECK(ordered[0] == mid);
        CHECK(ordered[1] == leaf);
        CHECK(ordered[2] == top);
    }

    TEST_CASE("asset_database - dependency_closure covers the reachable set and rejects unknown roots") {
        pipeline_fixture fixture = {};
        fixture.write_source("leaf.ta", "leaf");
        fixture.write_source("mid.ta", "dep-path leaf.ta\nmid");
        fixture.write_source("top.ta", "dep-path mid.ta\ntop");
        fixture.write_source("alone.ta", "alone");
        fixture.scan_and_import();
        const asset_id leaf = fixture.id_of("leaf.ta");
        const asset_id mid = fixture.id_of("mid.ta");
        const asset_id top = fixture.id_of("top.ta");

        core::result<std::vector<asset_id>> closure = fixture.database->dependency_closure(std::vector<asset_id>{top});
        CONJURE_REQUIRE_OK(closure);
        std::vector<asset_id> expected = {top, mid, leaf};
        std::sort(expected.begin(), expected.end());
        CHECK(closure.value() == expected);
        CHECK(std::is_sorted(closure.value().begin(), closure.value().end()));

        core::result<std::vector<asset_id>> fromMid = fixture.database->dependency_closure(std::vector<asset_id>{mid});
        CONJURE_REQUIRE_OK(fromMid);
        CHECK(fromMid.value().size() == 2);
        CONJURE_CHECK_ERROR(
          fixture.database->dependency_closure(std::vector<asset_id>{test_id(99)}),
          assets_error::NOT_FOUND
        );
        core::result<std::vector<asset_id>> empty = fixture.database->dependency_closure({});
        CONJURE_REQUIRE_OK(empty);
        CHECK(empty.value().empty());
    }

    TEST_CASE("asset_database - dependency cycles are detected and flagged") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("a.ta", "a");
        fixture.write_source("b.ta", "b");
        fixture.write_source("c.ta", "c");
        fixture.scan_and_import();
        const asset_id a = fixture.id_of("a.ta");
        const asset_id b = fixture.id_of("b.ta");
        const asset_id c = fixture.id_of("c.ta");

        fixture.write_source("a.ta", "dep " + b.to_string() + "\na");
        fixture.write_source("b.ta", "dep " + a.to_string() + "\nb");
        fixture.write_source("c.ta", "dep " + a.to_string() + "\nc");
        fixture.scan_and_import();
        CHECK(fixture.database->find(a)->status == asset_record_status::CYCLIC_DEPENDENCY);
        CHECK(fixture.database->find(b)->status == asset_record_status::CYCLIC_DEPENDENCY);
        CHECK(fixture.database->find(c)->status == asset_record_status::OK);
        CHECK(logs.contains(core::log_level::WARN, "dependency cycle"));

        fixture.write_source("b.ta", "b alone");
        fixture.scan_and_import();
        CHECK(fixture.database->find(a)->status == asset_record_status::OK);
        CHECK(fixture.database->find(b)->status == asset_record_status::OK);
    }

    TEST_CASE("asset_database - side files are hashed and their owners are known") {
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/models/robot.bin", bytes_of("bin")));
        fixture.write_source("models/robot.ta", "side robot.bin\nrobot");
        fixture.write_source("models/other.ta", "side robot.bin\nother");
        fixture.scan_and_import();
        const asset_id robot = fixture.id_of("models/robot.ta");
        const asset_id other = fixture.id_of("models/other.ta");

        std::vector<asset_id> owners = fixture.database->owners_of_side_file("assets://models/robot.bin");
        std::sort(owners.begin(), owners.end());
        std::vector<asset_id> expected = {robot, other};
        std::sort(expected.begin(), expected.end());
        CHECK(owners == expected);
        CHECK(fixture.database->owners_of_side_file("assets://models/nope.bin").empty());

        // The side file is part of the source hash: changing it makes both owners stale, nothing else.
        const u64 hashBefore = fixture.database->find(robot)->sourceHash;
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/models/robot.bin", bytes_of("bin v2")));
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->find(robot)->sourceHash != hashBefore);
        std::vector<asset_id> stale = fixture.database->stale_ids();
        std::sort(stale.begin(), stale.end());
        CHECK(stale == expected);
    }

    TEST_CASE("asset_database - the snapshot round-trips the in-memory records") {
        pipeline_fixture fixture = {};
        fixture.write_source("leaf.ta", "leaf");
        fixture.write_source("top.ta", "dep-path leaf.ta\nsub Part\ntop");
        fixture.scan_and_import();
        const std::vector<asset_record> before = fixture.database->records();
        CHECK(fixture.fileSystem->exists(".conjure/cache/db.bin"));

        asset_database reopened(*fixture.fileSystem, fixture.importers, fixture.manager->types(), &fixture.jobs);
        CONJURE_REQUIRE_OK(reopened.add_mount("assets", "assets"));
        CONJURE_REQUIRE_OK(reopened.add_mount("engine", "engine"));
        CONJURE_REQUIRE_OK(reopened.open(".conjure/cache", "test"));
        std::vector<asset_record> after = reopened.records();
        REQUIRE(after.size() == before.size());
        for (const asset_record &record : before) {
            CAPTURE(record.relativePath);
            const std::optional<asset_record> loaded = reopened.find(record.id);
            REQUIRE(loaded.has_value());
            CHECK(loaded->parentId == record.parentId);
            CHECK(loaded->type == record.type);
            CHECK(loaded->relativePath == record.relativePath);
            CHECK(loaded->subPath == record.subPath);
            CHECK(loaded->importerId == record.importerId);
            CHECK(loaded->displayName == record.displayName);
            CHECK(loaded->sourceHash == record.sourceHash);
            CHECK(loaded->settingsHash == record.settingsHash);
            CHECK(loaded->payloadHash == record.payloadHash);
            CHECK(loaded->status == record.status);
            CHECK(reopened.dependencies_of(record.id) == fixture.database->dependencies_of(record.id));
        }
        CHECK(reopened.stale_ids().empty());

        // A rescan after the snapshot hashes nothing: size and write time match.
        core::result<scan_report> rescan = reopened.scan();
        CONJURE_REQUIRE_OK(rescan);
        CHECK(rescan.value().hashed == 0);
        CHECK(rescan.value().recordsAdded == 0);
    }

    TEST_CASE("asset_database - a snapshot for other mounts or a corrupt snapshot is ignored") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();

        conjure::testing::log_capture logs = {};
        asset_database otherMounts(*fixture.fileSystem, fixture.importers, fixture.manager->types());
        CONJURE_REQUIRE_OK(otherMounts.add_mount("content", "assets"));
        CONJURE_REQUIRE_OK(otherMounts.open(".conjure/cache", "test"));
        CHECK(otherMounts.record_count() == 0);
        CHECK(logs.contains(core::log_level::INFO, "snapshot not used"));

        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file(".conjure/cache/db.bin", bytes_of("garbage")));
        asset_database corrupt(*fixture.fileSystem, fixture.importers, fixture.manager->types());
        CONJURE_REQUIRE_OK(corrupt.add_mount("assets", "assets"));
        CONJURE_REQUIRE_OK(corrupt.add_mount("engine", "engine"));
        CONJURE_REQUIRE_OK(corrupt.open(".conjure/cache", "test"));
        CHECK(corrupt.record_count() == 0);
        CONJURE_REQUIRE_OK(corrupt.scan());
        CHECK(corrupt.record_count() == 1);
    }

    TEST_CASE("asset_database - add_file, create_asset and delete_asset") {
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.database->scan());
        fixture.write_source("late.ta", "late");
        CONJURE_CHECK_ERROR(fixture.database->add_file("assets://missing.ta"), assets_error::NOT_FOUND);
        CONJURE_CHECK_ERROR(fixture.database->add_file("bogus"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(fixture.database->add_file("other://late.ta"), assets_error::NOT_FOUND);
        core::result<asset_id> added = fixture.database->add_file("assets://late.ta");
        CONJURE_REQUIRE_OK(added);
        CHECK(fixture.id_of("late.ta") == added.value());
        CHECK(fixture.fileSystem->exists("assets/late.ta.meta"));
        CHECK(fixture.database->stale_ids() == std::vector<asset_id>{added.value()});
        CONJURE_CHECK_ERROR(fixture.database->add_file("assets://late.ta"), core::error_code::ALREADY_EXISTS);

        core::result<asset_id> created = fixture.database->create_asset(
          "assets://made/new.ta",
          "conjure.test.text",
          bytes_of("made")
        );
        CONJURE_REQUIRE_OK(created);
        CHECK(fixture.fileSystem->exists("assets/made/new.ta"));
        CHECK(fixture.database->find(created.value())->importerId == "conjure.test.text");
        CONJURE_CHECK_ERROR(
          fixture.database->create_asset("assets://made/new.ta", "conjure.test.text", {}),
          core::error_code::ALREADY_EXISTS
        );
        CONJURE_CHECK_ERROR(
          fixture.database->create_asset("assets://made/x.ta", "conjure.test.nope", {}),
          assets_error::NO_IMPORTER
        );
        CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
        CHECK(fixture.cache().contains(created.value()));

        CONJURE_REQUIRE_OK(fixture.database->delete_asset(created.value()));
        CHECK_FALSE(fixture.fileSystem->exists("assets/made/new.ta"));
        CHECK_FALSE(fixture.fileSystem->exists("assets/made/new.ta.meta"));
        CHECK_FALSE(fixture.cache().contains(created.value()));
        CHECK_FALSE(fixture.database->find(created.value()).has_value());
        CONJURE_CHECK_ERROR(fixture.database->delete_asset(created.value()), assets_error::NOT_FOUND);
    }

    TEST_CASE("asset_database - write_meta refreshes the settings hash and marks the source stale") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "lower");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("a.ta");
        CHECK(fixture.database->stale_ids().empty());
        const u64 settingsBefore = fixture.database->find(id)->settingsHash;

        core::result<asset_meta> meta = fixture.database->read_meta(id);
        CONJURE_REQUIRE_OK(meta);
        asset_meta edited = meta.value();
        edited.settings = core::json::object({{"upper", true}});
        CONJURE_REQUIRE_OK(fixture.database->write_meta(id, edited));
        CHECK(fixture.database->find(id)->settingsHash != settingsBefore);
        CHECK(fixture.database->stale_ids() == std::vector<asset_id>{id});
        CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
        CHECK(text_of(fixture.cache().read(id).value().payload) == "LOWER");
        CONJURE_CHECK_ERROR(fixture.database->write_meta(test_id(9), edited), assets_error::NOT_FOUND);
        CONJURE_CHECK_ERROR(fixture.database->read_meta(test_id(9)), assets_error::NOT_FOUND);
    }

    TEST_CASE("asset_database - set_importer imports a source with an importer that does not claim its extension") {
        pipeline_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.importers.register_importer(std::make_unique<shout_importer>()));
        fixture.write_source("a.ta", "lower");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("a.ta");
        // By extension the source belongs to the importer that claims ".ta".
        CHECK(fixture.database->find(id)->importerId == test_importer::ID);
        CHECK(text_of(fixture.cache().read(id).value().payload) == "lower");

        // "Import as": the meta names the other importer with its version and default settings, the uuid stays
        // and the asset is stale until it is imported again.
        CONJURE_REQUIRE_OK(fixture.database->set_importer(id, shout_importer::ID));
        CHECK(fixture.database->find(id)->importerId == shout_importer::ID);
        CHECK(fixture.database->find(id)->importerVersion == 3);
        core::result<asset_meta> meta = fixture.database->read_meta(id);
        CONJURE_REQUIRE_OK(meta);
        CHECK(meta.value().id == id);
        CHECK(meta.value().importerId == shout_importer::ID);
        CHECK(meta.value().importerVersion == 3);
        CHECK(meta.value().settings.root()["upper"].as_bool(false));
        CHECK(fixture.database->stale_ids() == std::vector<asset_id>{id});
        CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
        CHECK(text_of(fixture.cache().read(id).value().payload) == "LOWER");

        // Naming the importer the meta already has keeps the settings the user gave it.
        asset_meta edited = meta.value();
        edited.settings = core::json::object({{"upper", false}});
        CONJURE_REQUIRE_OK(fixture.database->write_meta(id, edited));
        CONJURE_REQUIRE_OK(fixture.database->set_importer(id, shout_importer::ID));
        CHECK_FALSE(fixture.database->read_meta(id).value().settings.root()["upper"].as_bool(true));

        // The meta wins at the next scan: the extension's importer does not take the file back.
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->find(id)->importerId == shout_importer::ID);

        CONJURE_CHECK_ERROR(fixture.database->set_importer(id, "conjure.test.nope"), assets_error::NO_IMPORTER);
        CHECK(fixture.database->find(id)->importerId == shout_importer::ID);
        const core::result<void> unknownAsset = fixture.database->set_importer(test_id(9), shout_importer::ID);
        CONJURE_CHECK_ERROR(unknownAsset, assets_error::NOT_FOUND);
    }

    TEST_CASE("asset_database - corrupt metas are never overwritten and loads of them fail") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("broken.ta", "x");
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("assets/broken.ta.meta", bytes_of("{ nope")));
        core::result<scan_report> report = fixture.database->scan();
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().invalidMetas == 1);
        const asset_id id = fixture.id_of("broken.ta");
        CHECK(fixture.database->find(id)->status == asset_record_status::META_INVALID);
        CHECK(fixture.fileSystem->read_text_file("assets/broken.ta.meta").value() == "{ nope");
        CHECK(fixture.database->stale_ids().empty());
        CHECK(logs.contains(core::log_level::WARN, "broken.ta"));

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("broken.ta") == id);   // the transient id is stable across rescans
    }

    TEST_CASE("asset_database - lookups from jobs during a scan are consistent [parallel]") {
        pipeline_fixture fixture(4);
        for (u32 index = 0; index < 40; ++index) {
            fixture.write_source("many/" + std::to_string(index) + ".ta", "content " + std::to_string(index));
        }
        fixture.scan_and_import();
        const asset_id probe = fixture.id_of("many/7.ta");

        std::atomic<bool> stop = false;
        std::atomic<u32> lookups = 0;
        std::atomic<u32> mismatches = 0;
        const core::jobs::job_handle readers = fixture.jobs.schedule([&]() {
            while (!stop.load(std::memory_order_acquire)) {
                const std::optional<asset_record> record = fixture.database->find(probe);
                if (!record.has_value() || record->relativePath != "many/7.ta") {
                    mismatches.fetch_add(1, std::memory_order_relaxed);
                }
                static_cast<void>(fixture.database->records());
                static_cast<void>(fixture.database->find_by_path("assets://many/3.ta"));
                lookups.fetch_add(1, std::memory_order_relaxed);
            }
        });
        for (u32 round = 0; round < 5; ++round) {
            fixture.write_source("many/" + std::to_string(round) + ".ta", "changed " + std::to_string(round));
            CONJURE_REQUIRE_OK(fixture.database->scan());
            CONJURE_REQUIRE_OK(fixture.database->import_stale(fixture.jobs));
        }
        stop.store(true, std::memory_order_release);
        fixture.jobs.wait(readers);
        CHECK(mismatches.load() == 0);
        CHECK(lookups.load() > 0);
    }

    TEST_CASE("regression - a source re-created with a fresh meta at a known path takes the meta's uuid") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id original = fixture.id_of("a.ta");
        CONJURE_REQUIRE_OK(fixture.fileSystem->remove("assets/a.ta"));
        CONJURE_REQUIRE_OK(fixture.fileSystem->remove("assets/a.ta.meta"));
        fixture.write_source("a.ta", "a new asset");

        core::result<scan_report> report = fixture.database->scan();
        CONJURE_REQUIRE_OK(report);
        const core::result<asset_meta> meta = read_asset_meta(*fixture.fileSystem, "assets/a.ta.meta");
        CONJURE_REQUIRE_OK(meta);
        CHECK(meta.value().id != original);
        CHECK(fixture.id_of("a.ta") == meta.value().id);
        CHECK(report.value().metasCreated == 1);
        CHECK(report.value().recordsAdded == 1);
        CHECK(fixture.database->find(original)->status == asset_record_status::MISSING_SOURCE);
        const std::optional<asset_record> fresh = fixture.database->find(meta.value().id);
        REQUIRE(fresh.has_value());
        CHECK(fresh->type == asset_type_id::INVALID);   // inherits nothing from the previous asset
        CHECK(fixture.database->stale_ids() == std::vector<asset_id>{meta.value().id});
        CHECK_FALSE(logs.contains(core::log_level::ERR, "uuid changed"));

        // The broken identity is named once, by both uuids.
        CHECK(logs.contains(core::log_level::WARN, "its .meta was missing, so it is a new asset"));
        CHECK(logs.contains(core::log_level::WARN, std::format("{}", original)));
        CHECK(logs.contains(core::log_level::WARN, std::format("{}", meta.value().id)));
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(logs.count(core::log_level::WARN) == 1);
    }

    TEST_CASE("regression - a source and meta moved onto a known path keep their own uuid") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("a.ta", "A");
        fixture.write_source("b.ta", "B");
        fixture.scan_and_import();
        const asset_id a = fixture.id_of("a.ta");
        const asset_id b = fixture.id_of("b.ta");
        CONJURE_REQUIRE_OK(fixture.fileSystem->remove("assets/a.ta"));
        CONJURE_REQUIRE_OK(fixture.fileSystem->remove("assets/a.ta.meta"));
        CONJURE_REQUIRE_OK(fixture.fileSystem->rename("assets/b.ta", "assets/a.ta"));
        CONJURE_REQUIRE_OK(fixture.fileSystem->rename("assets/b.ta.meta", "assets/a.ta.meta"));

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("a.ta") == b);
        CHECK(fixture.database->find(b)->status == asset_record_status::OK);
        CHECK(fixture.database->find(b)->type == asset_type_traits<test_asset>::id());   // the moved asset's state
        CHECK(fixture.database->find(a)->status == asset_record_status::MISSING_SOURCE);
        CHECK_FALSE(fixture.database->find_by_path("assets://b.ta").has_value());
        CHECK_FALSE(logs.contains(core::log_level::ERR, "uuid changed"));
        CHECK_FALSE(logs.contains(core::log_level::WARN, "its .meta was missing"));
    }

    TEST_CASE("regression - a missing meta at a known path is not reported when its uuid moved elsewhere") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id a = fixture.id_of("a.ta");
        CONJURE_REQUIRE_OK(fixture.fileSystem->rename("assets/a.ta", "assets/b.ta"));
        CONJURE_REQUIRE_OK(fixture.fileSystem->rename("assets/a.ta.meta", "assets/b.ta.meta"));
        fixture.write_source("a.ta", "a new asset");

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("b.ta") == a);
        CHECK(fixture.id_of("a.ta") != a);
        CHECK_FALSE(logs.contains(core::log_level::WARN, "its .meta was missing"));
    }

    TEST_CASE("regression - a meta hand-edited to the uuid of another live asset is still refused") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.write_source("a.ta", "A");
        fixture.write_source("b.ta", "B");
        fixture.scan_and_import();
        const asset_id a = fixture.id_of("a.ta");
        const asset_id b = fixture.id_of("b.ta");
        core::result<asset_meta> meta = read_asset_meta(*fixture.fileSystem, "assets/a.ta.meta");
        CONJURE_REQUIRE_OK(meta);
        asset_meta edited = meta.value();
        edited.id = b;
        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.fileSystem, "assets/a.ta.meta", edited));

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("a.ta") == a);
        CHECK(fixture.id_of("b.ta") == b);
        CHECK(fixture.database->find(a)->status == asset_record_status::OK);
        CHECK(fixture.database->find(b)->status == asset_record_status::OK);
        CHECK(logs.count(core::log_level::ERR) == 1);
        CHECK(logs.contains(core::log_level::ERR, "uuid changed"));
    }

    TEST_CASE("asset_database - a snapshot whose record count exceeds its bytes is ignored without reserving for it") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("a.ta", "A");

        // "CJDB", format version 1, no mounts, then a record count of 2^24 and nothing else.
        const std::vector<std::byte> forged = {
          std::byte{'C'}, std::byte{'J'}, std::byte{'D'}, std::byte{'B'},
          std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
          std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
          std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}
        };
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file(".conjure/cache/db.bin", forged));
        asset_database forgedCount(*fixture.fileSystem, fixture.importers, fixture.manager->types());
        CONJURE_REQUIRE_OK(forgedCount.add_mount("assets", "assets"));
        CONJURE_REQUIRE_OK(forgedCount.add_mount("engine", "engine"));

        conjure::testing::allocation_scope scope = {};
        CONJURE_REQUIRE_OK(forgedCount.open(".conjure/cache", "test"));
        const u64 allocatedBytes = scope.allocated_bytes();
        CHECK(allocatedBytes < 16ull * 1024 * 1024);
        CHECK(forgedCount.record_count() == 0);
        CHECK(logs.contains(core::log_level::INFO, "snapshot not used"));
        CONJURE_REQUIRE_OK(forgedCount.scan());
        CHECK(forgedCount.record_count() == 1);
    }

    TEST_CASE("regression - add_file, create_asset and move_asset refuse paths that leave the mount") {
        pipeline_fixture fixture = {};
        fixture.write_source("a.ta", "A");
        fixture.scan_and_import();
        const asset_id a = fixture.id_of("a.ta");
        CONJURE_REQUIRE_OK(fixture.fileSystem->write_file("present.ta", bytes_of("outside the mount")));
        CONJURE_CHECK_ERROR(fixture.database->add_file("assets://../present.ta"), core::error_code::INVALID_ARGUMENT);
        CHECK_FALSE(fixture.fileSystem->exists("present.ta.meta"));

        const std::vector<std::string> escaping = {
          "assets://../outside.ta",
          "assets://a/../../outside.ta",
          "assets:///abs.ta",
          "assets://C:/abs.ta",
          "assets://..",
        };
        for (const std::string &path : escaping) {
            CAPTURE(path);
            CONJURE_CHECK_ERROR(fixture.database->add_file(path), core::error_code::INVALID_ARGUMENT);
            CONJURE_CHECK_ERROR(
              fixture.database->create_asset(path, test_importer::ID, bytes_of("x")),
              core::error_code::INVALID_ARGUMENT
            );
            CONJURE_CHECK_ERROR(fixture.database->move_asset(a, path), core::error_code::INVALID_ARGUMENT);
        }
        CHECK_FALSE(fixture.fileSystem->exists("outside.ta"));
        CHECK_FALSE(fixture.fileSystem->exists("outside.ta.meta"));
        CHECK(fixture.fileSystem->exists("assets/a.ta"));
        CHECK(fixture.fileSystem->exists("assets/a.ta.meta"));
        CHECK(fixture.database->asset_path_of(a) == "assets://a.ta");

        // A path that only passes through a parent directory stays inside and is fine.
        const std::string_view inside = "assets://sub/../inside.ta";
        CONJURE_REQUIRE_OK(fixture.database->create_asset(inside, test_importer::ID, bytes_of("x")));
        CHECK(fixture.fileSystem->exists("assets/inside.ta"));
    }

    TEST_CASE("regression - code sweep F753: create_asset and move_asset refuse a name that is not portable first") {
        // The file system refuses to create such a name, but move_asset renames the .meta first, and the meta
        // of "x." is "x..meta", a name it takes: the meta moved, the source was refused and the meta had to be
        // moved back (IO_ERROR, and a stranded meta when the way back was refused too). The destination's
        // directory had been created by then as well. Both calls now check every segment before anything is
        // created, moved or written, and answer with the code of a bad argument.
        pipeline_fixture fixture = {};
        fixture.write_source("old/a.ta", "A");
        fixture.scan_and_import();
        const asset_id id = fixture.id_of("old/a.ta");

        const std::vector<std::string> unportable = {
          "assets://x.",
          "assets://fresh/x.",
          "assets://fresh/x.ta ",
          "assets://fresh ./x.ta",
          "assets://fresh/nul.ta",
          "assets://fresh/what?.ta",
          "assets://fresh/a:b.ta",
          "assets://fresh/tab\there.ta",
        };
        for (const std::string &path : unportable) {
            CAPTURE(path);
            CONJURE_CHECK_ERROR(fixture.database->move_asset(id, path), core::error_code::INVALID_ARGUMENT);
            CONJURE_CHECK_ERROR(
              fixture.database->create_asset(path, test_importer::ID, bytes_of("x")),
              core::error_code::INVALID_ARGUMENT
            );
        }
        const core::result<void> moved = fixture.database->move_asset(id, "assets://fresh/x.");
        REQUIRE_FALSE(moved.has_value());
        CHECK_MESSAGE(moved.get_error().message().find("'x.'") != std::string::npos, moved.get_error().message());

        // Nothing was created on the way, not even the directory a destination named, and the pair is where
        // it was.
        CHECK_FALSE(fixture.fileSystem->exists("assets/fresh"));
        CHECK_FALSE(fixture.fileSystem->exists("assets/x..meta"));
        CHECK(fixture.fileSystem->exists("assets/old/a.ta"));
        CHECK(fixture.fileSystem->exists("assets/old/a.ta.meta"));
        CHECK(fixture.database->asset_path_of(id) == "assets://old/a.ta");
        CHECK(fixture.database->record_count() == 1);

        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.id_of("old/a.ta") == id);
        CHECK(fixture.database->record_count() == 1);

        // A name that only resembles one of them is fine.
        CONJURE_REQUIRE_OK(fixture.database->move_asset(id, "assets://fresh/nulls.x..ta"));
        CHECK(fixture.fileSystem->exists("assets/fresh/nulls.x..ta.meta"));
        CHECK(fixture.id_of("fresh/nulls.x..ta") == id);
    }

    TEST_CASE("regression - a meta that cannot be written leaves its source META_INVALID and the scan succeeds") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.write_source("a.ta", "A");
        fixture.fileSystem->set_read_only(true);
        core::result<scan_report> report = fixture.database->scan();
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().invalidMetas == 1);
        CHECK(fixture.database->find(fixture.id_of("a.ta"))->status == asset_record_status::META_INVALID);
        CHECK_FALSE(fixture.fileSystem->exists("assets/a.ta.meta"));
        CHECK(logs.contains(core::log_level::WARN, "cannot write its meta"));

        fixture.fileSystem->set_read_only(false);
        CONJURE_REQUIRE_OK(fixture.database->scan());
        const core::result<asset_meta> meta = read_asset_meta(*fixture.fileSystem, "assets/a.ta.meta");
        CONJURE_REQUIRE_OK(meta);
        CHECK(fixture.id_of("a.ta") == meta.value().id);
        CHECK(fixture.database->find(meta.value().id)->status == asset_record_status::OK);

        // A meta naming no importer cannot be given one while the files are read-only: NO_IMPORTER,
        // which is what an import would conclude from the meta on disk, until a writable scan.
        asset_meta bare = {};
        bare.id = test_id(5);
        fixture.write_source("b.ta", "B");
        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.fileSystem, "assets/b.ta.meta", bare));
        fixture.fileSystem->set_read_only(true);
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->find(test_id(5))->status == asset_record_status::NO_IMPORTER);
        CHECK(logs.contains(core::log_level::WARN, "cannot record importer"));
        fixture.fileSystem->set_read_only(false);
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CHECK(fixture.database->find(test_id(5))->status == asset_record_status::OK);
        CHECK(fixture.database->read_meta(test_id(5)).value().importerId == test_importer::ID);
    }

    TEST_CASE("regression - create_asset removes its source when the meta cannot be written") {
        pipeline_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.database->scan());
        CONJURE_REQUIRE_OK(fixture.fileSystem->create_directories("assets/new.ta.meta"));
        CHECK_FALSE(fixture.database->create_asset("assets://new.ta", test_importer::ID, bytes_of("new")).has_value());
        CHECK_FALSE(fixture.fileSystem->exists("assets/new.ta"));
        CHECK_FALSE(fixture.database->find_by_path("assets://new.ta").has_value());
    }
}
