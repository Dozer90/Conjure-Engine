#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_cooker.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_pack.h"
#include "conjure/assets/asset_source.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

TEST_SUITE("assets/export_roundtrip") {

    TEST_CASE("export - a cooked pack loads through a runtime manager exactly like the editor cache") {
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            conjure::testing::log_capture logs = {};
            logs.allow_errors();
            pipeline_fixture editor(workers);
            asset_cooker cooker(*editor.database, editor.cache(), *editor.fileSystem, editor.jobs);
            editor.write_source("textures/wood.ta", "wood");
            editor.write_source("materials/table.ta", "dep-path ../textures/wood.ta\ntable");
            editor.write_source("models/room.ta", "dep-path ../materials/table.ta\nsub Chair\nroom");
            editor.write_source("unused.ta", "unused");
            CONJURE_REQUIRE_OK(editor.database->scan());
            CONJURE_REQUIRE_OK(editor.database->import_stale(editor.jobs));
            const asset_id room = editor.id_of("models/room.ta");
            const asset_id chair = editor.database->find_by_path("assets://models/room.ta#Chair").value();
            const asset_id table = editor.id_of("materials/table.ta");
            const asset_id wood = editor.id_of("textures/wood.ta");

            cook_settings settings = {};
            settings.outputDirectory = "export";
            settings.roots = {room, chair};
            core::result<cook_report> cooked = cooker.cook(settings);
            CONJURE_REQUIRE_OK(cooked);
            CHECK(cooked.value().assetCount == 4);

            // Editor-side payloads for comparison.
            core::result<asset_handle> editorRoom = editor.manager->load_blocking(room);
            CONJURE_REQUIRE_OK(editorRoom);
            const std::string editorContent = editorRoom.value().get<test_asset>()->content;

            // The runtime sees only the pack on a fresh file system.
            core::vfs::memory_file_system runtimeFiles = {};
            CONJURE_REQUIRE_OK(
              runtimeFiles.write_file(
                "content/content.cpak",
                editor.fileSystem->read_file("export/content.cpak").value()
              )
            );
            pack_reader reader = {};
            CONJURE_REQUIRE_OK(reader.open(runtimeFiles, "content/content.cpak"));
            core::jobs::job_system runtimeJobs = {};
            core::jobs::job_system_desc jobsDesc = {};
            jobsDesc.workerCount = workers;
            CONJURE_REQUIRE_OK(runtimeJobs.start(jobsDesc));
            {
                asset_manager_desc desc = {};
                desc.jobs = &runtimeJobs;
                desc.source = std::make_unique<pack_asset_source>(reader);
                asset_manager runtime(std::move(desc));
                register_test_types(runtime.types());

                CHECK(runtime.find("assets://models/room.ta") == room);
                CHECK(runtime.find("assets://models/room.ta#Chair") == chair);
                CHECK_FALSE(runtime.find("assets://unused.ta").has_value());
                std::vector<asset_id> order = {};
                static_cast<void>(runtime.subscribe([&order](const asset_event &event) {
                    order.push_back(event.id);
                }));

                asset_handle roomHandle = runtime.load(room);
                asset_handle chairHandle = runtime.load(chair);
                runtime.flush();
                REQUIRE(roomHandle.state() == asset_state::LOADED);
                CHECK(roomHandle.get<test_asset>()->content == editorContent);
                CHECK(chairHandle.get<test_asset>()->content == "Chair:room");
                CHECK(runtime.state_of(table) == asset_state::LOADED);
                CHECK(runtime.state_of(wood) == asset_state::LOADED);
                REQUIRE(order.size() == 4);
                const auto position = [&order](const asset_id &id) {
                    return std::find(order.begin(), order.end(), id) - order.begin();
                };
                CHECK(position(wood) < position(table));
                CHECK(position(table) < position(room));
                CHECK(position(chair) < 4);
                CHECK(runtime.stats().loaded == 4);

                asset_handle missing = runtime.load(editor.id_of("unused.ta"));
                runtime.flush();
                CHECK(missing.state() == asset_state::FAILED);

                roomHandle.reset();
                chairHandle.reset();
                missing.reset();
                CHECK(runtime.collect() == 4);
                runtime.shutdown();
            }
            runtimeJobs.stop();
        }
    }

    TEST_CASE("export - the cooked blobs equal the cache blobs byte for byte") {
        pipeline_fixture editor = {};
        asset_cooker cooker(*editor.database, editor.cache(), *editor.fileSystem, editor.jobs);
        editor.write_source("a.ta", "alpha");
        editor.write_source("b.ta", "dep-path a.ta\nbeta");
        CONJURE_REQUIRE_OK(editor.database->scan());
        cook_settings settings = {};
        settings.outputDirectory = "export";
        settings.includeAll = true;
        CONJURE_REQUIRE_OK(cooker.cook(settings));

        pack_reader reader = {};
        CONJURE_REQUIRE_OK(reader.open(*editor.fileSystem, "export/content.cpak"));
        for (const asset_record &record : editor.database->records()) {
            CAPTURE(record.relativePath);
            const pack_toc_entry *entry = reader.find(record.id);
            REQUIRE(entry != nullptr);
            core::result<cooked_blob> fromPack = reader.read(*entry);
            core::result<cooked_blob> fromCache = editor.cache().read(record.id);
            CONJURE_REQUIRE_OK(fromPack);
            CONJURE_REQUIRE_OK(fromCache);
            CHECK(fromPack.value().payload == fromCache.value().payload);
            CHECK(fromPack.value().dependencies == fromCache.value().dependencies);
            CHECK(fromPack.value().header.type == fromCache.value().header.type);
            CHECK(fromPack.value().header.payloadHash == fromCache.value().header.payloadHash);
            CHECK(fromPack.value().header.typeVersion == fromCache.value().header.typeVersion);
        }
    }
}
