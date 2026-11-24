#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/file_watcher.h"
#include "conjure/assets/hot_reload.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    constexpr std::array<std::string_view, 1> ALTERNATE_EXTENSIONS = {".alt"};

    /// <summary>
    /// test_importer under another id, with its version and default settings: a switch between the
    /// two leaves every blob header input unchanged. The meta names it by id, so its extension
    /// does not matter.
    /// </summary>
    class alternate_text_importer final : public asset_importer {
        public:
            static constexpr std::string_view ID = "conjure.test.text.alternate";

        private:
            test_importer mInner = {};

        public:
            alternate_text_importer();
            ~alternate_text_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::json::document default_settings() const override;
            [[nodiscard]] core::result<import_output> import(import_context &context) const override;
    };

    alternate_text_importer::alternate_text_importer() {}

    alternate_text_importer::~alternate_text_importer() {}

    std::string_view alternate_text_importer::id() const noexcept {
        return ID;
    }

    u32 alternate_text_importer::version() const noexcept {
        return mInner.version();
    }

    std::span<const std::string_view> alternate_text_importer::extensions() const noexcept {
        return ALTERNATE_EXTENSIONS;
    }

    core::json::document alternate_text_importer::default_settings() const {
        return mInner.default_settings();
    }

    core::result<import_output> alternate_text_importer::import(import_context &context) const {
        return mInner.import(context);
    }

    /// <summary>
    /// texture -> material (flagged RELOAD_WITH_DEPENDENCIES) -> consumer (unflagged) over the
    /// pipeline fixture, with a watcher and controller wired up and everything loaded.
    /// </summary>
    struct reload_fixture {
        pipeline_fixture pipeline;
        polling_file_watcher watcher;
        hot_reload_controller controller;
        asset_id texture = {};
        asset_id material = {};
        asset_id consumer = {};
        asset_handle textureHandle = {};
        asset_handle materialHandle = {};
        asset_handle consumerHandle = {};
        std::vector<asset_event> events = {};

        reload_fixture() :
            pipeline(0, asset_type_flags::NONE),
            watcher(*pipeline.fileSystem, pipeline.jobs, 0.0),
            controller(*pipeline.database, *pipeline.manager, watcher, pipeline.jobs) {
            pipeline.write_source("texture.ta", "pixels v1");
            pipeline.write_source("material.ta", "dep-path texture.ta\nshiny");
            pipeline.write_source("consumer.ta", "dep-path material.ta\nconsumer");
            pipeline.scan_and_import();
            texture = pipeline.id_of("texture.ta");
            material = pipeline.id_of("material.ta");
            consumer = pipeline.id_of("consumer.ta");
            watcher.add_root("assets");
            CONJURE_REQUIRE_OK(watcher.snapshot());
            static_cast<void>(pipeline.manager->subscribe([this](const asset_event &event) {
                events.push_back(event);
                events.back().error = nullptr;
            }));
            textureHandle = pipeline.manager->load(texture);
            materialHandle = pipeline.manager->load(material);
            consumerHandle = pipeline.manager->load(consumer);
            pipeline.manager->flush();
            REQUIRE(textureHandle.state() == asset_state::LOADED);
            REQUIRE(materialHandle.state() == asset_state::LOADED);
            REQUIRE(consumerHandle.state() == asset_state::LOADED);
            events.clear();
        }

        /// <summary>
        /// Two watcher passes (debounce) plus a controller update; returns the controller's reloads.
        /// </summary>
        u32 tick() {
            CONJURE_REQUIRE_OK(watcher.scan_now());
            CONJURE_REQUIRE_OK(watcher.scan_now());
            controller.update();
            pipeline.manager->flush();
            return controller.reloads_this_frame();
        }

        usize count(asset_event_kind kind, const asset_id &id) const {
            usize total = 0;
            for (const asset_event &event : events) {
                if (event.kind == kind && event.id == id) {
                    ++total;
                }
            }
            return total;
        }
    };
}

TEST_SUITE("assets/hot_reload") {

    TEST_CASE("hot_reload - a modified leaf is reloaded with a new pointer and version, dependents see it") {
        reload_fixture fixture = {};
        const test_asset *oldTexture = fixture.textureHandle.get<test_asset>();
        const test_asset *oldMaterial = fixture.materialHandle.get<test_asset>();
        const test_asset *oldConsumer = fixture.consumerHandle.get<test_asset>();
        REQUIRE(oldTexture != nullptr);

        fixture.pipeline.write_source("texture.ta", "pixels v2");
        const u32 reloads = fixture.tick();
        CHECK(reloads == 1);
        CHECK(fixture.controller.last_report().changesSeen == 1);
        CHECK(fixture.controller.last_report().sourcesReimported == 1);
        CHECK(fixture.controller.last_report().reloaded == std::vector<asset_id>{fixture.texture});

        CHECK(fixture.textureHandle.get<test_asset>() != oldTexture);
        CHECK(fixture.textureHandle.get<test_asset>()->content == "pixels v2");
        CHECK(fixture.textureHandle.version() == 2);
        CHECK(fixture.count(asset_event_kind::RELOADED, fixture.texture) == 1);

        // Unflagged dependents keep their payload and observe the new version through their refs.
        CHECK(fixture.materialHandle.get<test_asset>() == oldMaterial);
        CHECK(fixture.consumerHandle.get<test_asset>() == oldConsumer);
        CHECK(fixture.materialHandle.version() == 1);
        const asset_ref<test_asset> textureRef = fixture.pipeline.manager->bind<test_asset>(fixture.texture);
        CHECK(textureRef.version() == 2);
        CHECK(fixture.events.size() == 1);
    }

    TEST_CASE("hot_reload - a flagged type is reloaded once, after its changed dependency") {
        pipeline_fixture pipeline(0, asset_type_flags::RELOAD_WITH_DEPENDENCIES);
        polling_file_watcher watcher(*pipeline.fileSystem, pipeline.jobs, 0.0);
        hot_reload_controller controller(*pipeline.database, *pipeline.manager, watcher, pipeline.jobs);
        pipeline.write_source("texture.ta", "pixels v1");
        pipeline.write_source("material.ta", "dep-path texture.ta\nshiny");
        pipeline.write_source("consumer.ta", "dep-path material.ta\nconsumer");
        pipeline.scan_and_import();
        const asset_id texture = pipeline.id_of("texture.ta");
        const asset_id material = pipeline.id_of("material.ta");
        const asset_id consumer = pipeline.id_of("consumer.ta");
        watcher.add_root("assets");
        CONJURE_REQUIRE_OK(watcher.snapshot());
        asset_handle consumerHandle = pipeline.manager->load(consumer);
        pipeline.manager->flush();
        std::vector<asset_id> reloaded = {};
        static_cast<void>(pipeline.manager->subscribe([&reloaded](const asset_event &event) {
            if (event.kind == asset_event_kind::RELOADED) {
                reloaded.push_back(event.id);
            }
        }));

        pipeline.write_source("texture.ta", "pixels v2");
        CONJURE_REQUIRE_OK(watcher.scan_now());
        CONJURE_REQUIRE_OK(watcher.scan_now());
        controller.update();
        CHECK(controller.reloads_this_frame() == 3);
        pipeline.manager->flush();
        REQUIRE(reloaded.size() == 3);
        CHECK(reloaded[0] == texture);
        CHECK(reloaded[1] == material);
        CHECK(reloaded[2] == consumer);
        CHECK(pipeline.manager->state_of(material) == asset_state::LOADED);
        CHECK(pipeline.manager->bind<test_asset>(material).version() == 2);

        // Nothing changed: nothing reloads.
        controller.update();
        pipeline.manager->flush();
        CHECK(controller.reloads_this_frame() == 0);
        CHECK(reloaded.size() == 3);
    }

    TEST_CASE("hot_reload - a settings change in the meta re-imports and reloads the owner") {
        reload_fixture fixture = {};
        asset_meta meta = fixture.pipeline.database->read_meta(fixture.texture).value();
        meta.settings = core::json::object({{"upper", true}});
        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.pipeline.fileSystem, "assets/texture.ta.meta", meta));
        CHECK(fixture.tick() == 1);
        CHECK(fixture.textureHandle.get<test_asset>()->content == "PIXELS V1");
        CHECK(fixture.count(asset_event_kind::RELOADED, fixture.texture) == 1);
    }

    TEST_CASE("hot_reload - a side-file change reloads every owner") {
        reload_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->write_file("assets/shared.bin", bytes_of("A")));
        fixture.pipeline.write_source("texture.ta", "side shared.bin\npixels");
        fixture.pipeline.write_source("other.ta", "side shared.bin\nother");
        fixture.pipeline.scan_and_import();
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        const asset_id other = fixture.pipeline.id_of("other.ta");
        asset_handle otherHandle = fixture.pipeline.manager->load(other);
        fixture.pipeline.manager->reload(fixture.texture);
        fixture.pipeline.manager->flush();
        fixture.events.clear();
        CHECK(fixture.textureHandle.get<test_asset>()->content == "pixels|A");

        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->write_file("assets/shared.bin", bytes_of("B")));
        const u32 reloads = fixture.tick();
        CHECK(reloads == 2);
        CHECK(fixture.textureHandle.get<test_asset>()->content == "pixels|B");
        CHECK(otherHandle.get<test_asset>()->content == "other|B");
        CHECK(fixture.count(asset_event_kind::RELOADED, fixture.texture) == 1);
        CHECK(fixture.count(asset_event_kind::RELOADED, other) == 1);
    }

    TEST_CASE("hot_reload - removing a source keeps the payload loaded and marks the record MISSING_SOURCE") {
        reload_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.pipeline.write_source("spare.ta", "spare");
        fixture.pipeline.scan_and_import();
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        const asset_id spare = fixture.pipeline.id_of("spare.ta");
        const test_asset *old = fixture.textureHandle.get<test_asset>();

        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->remove("assets/texture.ta"));
        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->remove("assets/spare.ta"));
        CHECK(fixture.tick() == 0);
        CHECK(fixture.textureHandle.state() == asset_state::LOADED);
        CHECK(fixture.textureHandle.get<test_asset>() == old);
        CHECK(fixture.events.empty());

        // Design 3.7 / section 6: REMOVED -> MISSING_SOURCE without waiting for a scan(); a fresh
        // load fails NOT_FOUND while the resident payload is untouched.
        CHECK(fixture.pipeline.database->find(fixture.texture)->status == asset_record_status::MISSING_SOURCE);
        CHECK(fixture.pipeline.database->find(spare)->status == asset_record_status::MISSING_SOURCE);
        CONJURE_CHECK_ERROR(fixture.pipeline.manager->load_blocking(spare), assets_error::NOT_FOUND);
        fixture.pipeline.manager->reload(fixture.texture);
        fixture.pipeline.manager->flush();
        CHECK(fixture.textureHandle.get<test_asset>() == old);
        CHECK(fixture.count(asset_event_kind::RELOAD_FAILED, fixture.texture) == 1);

        // The file comes back: the record returns to service and reloads with the new content.
        fixture.pipeline.write_source("texture.ta", "pixels back");
        CHECK(fixture.tick() == 1);
        CHECK(fixture.pipeline.database->find(fixture.texture)->status == asset_record_status::OK);
        CHECK(fixture.textureHandle.get<test_asset>()->content == "pixels back");
        CHECK(fixture.count(asset_event_kind::RELOADED, fixture.texture) == 1);
    }

    TEST_CASE("regression - an importer change in the meta is honoured by hot reload") {
        reload_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        const test_asset *old = fixture.textureHandle.get<test_asset>();

        // Switch to an importer that fails: the re-import runs it (and fails), the record says so.
        asset_meta meta = fixture.pipeline.database->read_meta(fixture.texture).value();
        meta.importerId = std::string(failing_importer::ID);
        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.pipeline.fileSystem, "assets/texture.ta.meta", meta));
        CHECK(fixture.tick() == 0);
        std::optional<asset_record> record = fixture.pipeline.database->find(fixture.texture);
        REQUIRE(record.has_value());
        CHECK(record->status == asset_record_status::IMPORT_FAILED);
        CHECK(record->importerId == failing_importer::ID);
        CHECK(fixture.textureHandle.get<test_asset>() == old);
        CHECK(logs.contains(core::log_level::ERR, "failing importer refused"));

        // An importer nobody registered: NO_IMPORTER, browsable but not loadable.
        meta.importerId = "conjure.test.nope";
        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.pipeline.fileSystem, "assets/texture.ta.meta", meta));
        CHECK(fixture.tick() == 0);
        record = fixture.pipeline.database->find(fixture.texture);
        CHECK(record->status == asset_record_status::NO_IMPORTER);
        CHECK(record->importerId == "conjure.test.nope");
        CHECK(fixture.textureHandle.get<test_asset>() == old);

        // Back to the real importer with a settings change: the owner re-imports and reloads.
        meta.importerId = std::string(test_importer::ID);
        meta.settings = core::json::object({{"upper", true}});
        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.pipeline.fileSystem, "assets/texture.ta.meta", meta));
        CHECK(fixture.tick() == 1);
        record = fixture.pipeline.database->find(fixture.texture);
        CHECK(record->status == asset_record_status::OK);
        CHECK(record->importerId == test_importer::ID);
        CHECK(fixture.textureHandle.get<test_asset>()->content == "PIXELS V1");
    }

    TEST_CASE("regression - an importer switch through write_meta re-imports even at the same version") {
        reload_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.pipeline.importers.register_importer(std::make_unique<alternate_text_importer>()));
        asset_meta meta = fixture.pipeline.database->read_meta(fixture.texture).value();
        meta.importerId = std::string(alternate_text_importer::ID);
        CONJURE_REQUIRE_OK(fixture.pipeline.database->write_meta(fixture.texture, meta));

        // Version, settings and source are what the blob was built from; the switch alone makes it
        // stale, so import_stale and a cook see it as well as hot reload.
        CHECK(fixture.pipeline.database->stale_ids() == std::vector<asset_id>{fixture.texture});
        fixture.tick();
        CHECK(fixture.controller.last_report().sourcesReimported == 1);
        const std::optional<asset_record> record = fixture.pipeline.database->find(fixture.texture);
        REQUIRE(record.has_value());
        CHECK(record->status == asset_record_status::OK);
        CHECK(record->importerId == alternate_text_importer::ID);
        CHECK(fixture.pipeline.database->stale_ids().empty());

        // The import cleared the switch: nothing re-imports again.
        fixture.tick();
        CHECK(fixture.controller.last_report().sourcesReimported == 0);
    }

    TEST_CASE("regression - an importer switch made while the editor was closed is re-imported at startup") {
        // Startup is scan() + import_stale(): the scan compares the meta with the previous record.
        reload_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.pipeline.importers.register_importer(std::make_unique<alternate_text_importer>()));
        asset_meta meta = fixture.pipeline.database->read_meta(fixture.texture).value();
        meta.importerId = std::string(alternate_text_importer::ID);
        CONJURE_REQUIRE_OK(write_asset_meta(*fixture.pipeline.fileSystem, "assets/texture.ta.meta", meta));

        core::result<scan_report> scanned = fixture.pipeline.database->scan();
        CONJURE_REQUIRE_OK(scanned);
        CHECK(scanned.value().stale == std::vector<asset_id>{fixture.texture});
        core::result<import_report> imported = fixture.pipeline.database->import_stale(fixture.pipeline.jobs);
        CONJURE_REQUIRE_OK(imported);
        CHECK(imported.value().sourcesImported == 1);
        CHECK(fixture.pipeline.database->stale_ids().empty());
    }

    TEST_CASE("regression - a source changed while its slot is LOADING is reloaded once the load finishes [parallel]") {
        pipeline_fixture pipeline(4, asset_type_flags::NONE);
        polling_file_watcher watcher(*pipeline.fileSystem, pipeline.jobs, 0.0);
        hot_reload_controller controller(*pipeline.database, *pipeline.manager, watcher, pipeline.jobs);
        pipeline.write_source("slow.ta", "v1");
        pipeline.scan_and_import();
        const asset_id slow = pipeline.id_of("slow.ta");
        watcher.add_root("assets");
        CONJURE_REQUIRE_OK(watcher.snapshot());
        std::vector<asset_event_kind> kinds = {};
        static_cast<void>(pipeline.manager->subscribe([&kinds](const asset_event &event) {
            kinds.push_back(event.kind);
        }));

        // The load job reads the v1 blob and parks in load_cpu; the source then changes.
        controls().blockLoadCpu = true;
        asset_handle handle = pipeline.manager->load(slow);
        pipeline.manager->update();
        while (controls().loadCpuCalls.load() == 0) {
            std::this_thread::yield();
        }
        REQUIRE(handle.state() == asset_state::LOADING);
        pipeline.write_source("slow.ta", "v2");
        CONJURE_REQUIRE_OK(watcher.scan_now());
        CONJURE_REQUIRE_OK(watcher.scan_now());
        controller.update();
        CHECK(controller.last_report().sourcesReimported == 1);
        CHECK(controller.reloads_this_frame() == 0);
        CHECK(controller.last_report().deferred == std::vector<asset_id>{slow});
        CHECK(controller.deferred_count() == 1);

        controls().blockLoadCpu = false;
        pipeline.manager->flush();
        REQUIRE(handle.state() == asset_state::LOADED);
        CHECK(handle.get<test_asset>()->content == "v1");   // the blob read before the change

        controller.update();
        CHECK(controller.reloads_this_frame() == 1);
        CHECK(controller.last_report().reloaded == std::vector<asset_id>{slow});
        CHECK(controller.deferred_count() == 0);
        pipeline.manager->flush();
        CHECK(handle.get<test_asset>()->content == "v2");
        CHECK(handle.version() == 2);
        CHECK(kinds == std::vector<asset_event_kind>{asset_event_kind::LOADED, asset_event_kind::RELOADED});

        controller.update();
        CHECK(controller.reloads_this_frame() == 0);
    }

    TEST_CASE("hot_reload - a failed re-import keeps the old payload and the slot stays loaded") {
        reload_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        const test_asset *old = fixture.textureHandle.get<test_asset>();
        fixture.pipeline.write_source("texture.ta", "fail\nbroken");
        CHECK(fixture.tick() == 0);
        CHECK(fixture.textureHandle.state() == asset_state::LOADED);
        CHECK(fixture.textureHandle.get<test_asset>() == old);
        CHECK(fixture.pipeline.database->find(fixture.texture)->status == asset_record_status::IMPORT_FAILED);
        CHECK(logs.contains(core::log_level::ERR, "texture.ta"));

        // A failed reload (the blob is fine, the loader fails) fires RELOAD_FAILED and keeps the payload.
        fixture.pipeline.write_source("texture.ta", "pixels v3");
        controls().failLoadCpu = true;
        CHECK(fixture.tick() == 1);
        controls().failLoadCpu = false;
        CHECK(fixture.textureHandle.get<test_asset>() == old);
        CHECK(fixture.count(asset_event_kind::RELOAD_FAILED, fixture.texture) == 1);
        CHECK(fixture.count(asset_event_kind::RELOADED, fixture.texture) == 0);
    }

    TEST_CASE("hot_reload - a -> b -> c flagged chain reloads each exactly once, in order") {
        pipeline_fixture pipeline(0, asset_type_flags::RELOAD_WITH_DEPENDENCIES);
        polling_file_watcher watcher(*pipeline.fileSystem, pipeline.jobs, 0.0);
        hot_reload_controller controller(*pipeline.database, *pipeline.manager, watcher, pipeline.jobs);
        pipeline.write_source("a.ta", "a1");
        pipeline.write_source("b.ta", "dep-path a.ta\nb");
        pipeline.write_source("c.ta", "dep-path b.ta\ndep-path a.ta\nc");
        pipeline.scan_and_import();
        const asset_id a = pipeline.id_of("a.ta");
        const asset_id b = pipeline.id_of("b.ta");
        const asset_id c = pipeline.id_of("c.ta");
        watcher.add_root("assets");
        CONJURE_REQUIRE_OK(watcher.snapshot());
        asset_handle top = pipeline.manager->load(c);
        pipeline.manager->flush();
        std::vector<asset_id> reloaded = {};
        static_cast<void>(pipeline.manager->subscribe([&reloaded](const asset_event &event) {
            if (event.kind == asset_event_kind::RELOADED) {
                reloaded.push_back(event.id);
            }
        }));

        pipeline.write_source("a.ta", "a2");
        CONJURE_REQUIRE_OK(watcher.scan_now());
        CONJURE_REQUIRE_OK(watcher.scan_now());
        controller.update();
        CHECK(controller.last_report().reloaded == std::vector<asset_id>{a, b, c});
        pipeline.manager->flush();
        REQUIRE(reloaded.size() == 3);
        CHECK(reloaded == std::vector<asset_id>{a, b, c});
        CHECK(pipeline.manager->bind<test_asset>(a).get()->content == "a2");
    }

    TEST_CASE("hot_reload - a CACHE_MISS failure self-heals on the next update") {
        reload_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        fixture.pipeline.write_source("late.ta", "late");
        CONJURE_REQUIRE_OK(fixture.pipeline.database->scan());   // known to the database, never imported
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        const asset_id late = fixture.pipeline.id_of("late.ta");
        asset_handle handle = fixture.pipeline.manager->load(late);
        fixture.pipeline.manager->flush();
        REQUIRE(handle.state() == asset_state::FAILED);
        CHECK(fixture.pipeline.manager->last_error(late)->is(assets_error::CACHE_MISS));

        fixture.controller.update();
        fixture.pipeline.manager->flush();
        CHECK(fixture.controller.last_report().sourcesReimported == 1);
        CHECK(fixture.controller.reloads_this_frame() == 1);
        CHECK(handle.state() == asset_state::LOADED);
        CHECK(handle.get<test_asset>()->content == "late");
    }

    TEST_CASE("hot_reload - a CACHE_MISS failure whose import fails is not re-imported on every update") {
        reload_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.pipeline.write_source("broken.ta", "fail\nbroken");
        CONJURE_REQUIRE_OK(fixture.pipeline.database->scan());   // known to the database, never imported
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        const asset_id broken = fixture.pipeline.id_of("broken.ta");
        asset_handle handle = fixture.pipeline.manager->load(broken);
        fixture.pipeline.manager->flush();
        REQUIRE(handle.state() == asset_state::FAILED);
        CHECK(fixture.pipeline.manager->last_error(broken)->is(assets_error::CACHE_MISS));

        // The first update tries the repair, which fails: the record says so.
        fixture.controller.update();
        fixture.pipeline.manager->flush();
        CHECK(fixture.pipeline.database->find(broken)->status == asset_record_status::IMPORT_FAILED);
        CHECK(logs.count(core::log_level::ERR) == 1);
        const u32 loadCpuCalls = controls().loadCpuCalls.load();

        // Nothing changed since: no import, no retry, no log line, however many frames pass.
        for (u32 frame = 0; frame < 2; ++frame) {
            CAPTURE(frame);
            fixture.controller.update();
            fixture.pipeline.manager->flush();
            CHECK(fixture.controller.last_report().sourcesReimported == 0);
            CHECK(fixture.controller.reloads_this_frame() == 0);
            CHECK(logs.count(core::log_level::ERR) == 1);
            CHECK(controls().loadCpuCalls.load() == loadCpuCalls);
            CHECK(handle.state() == asset_state::FAILED);
        }

        // Fixing the source repairs the slot.
        fixture.pipeline.write_source("broken.ta", "fixed");
        fixture.tick();
        CHECK(handle.state() == asset_state::LOADED);
        REQUIRE(handle.get<test_asset>() != nullptr);
        CHECK(handle.get<test_asset>()->content == "fixed");
    }

    TEST_CASE("hot_reload - the meta written by an import does not trigger a second import") {
        reload_fixture fixture = {};
        fixture.pipeline.write_source("fresh.ta", "fresh");
        CHECK(fixture.tick() == 0);
        CHECK(fixture.controller.last_report().sourcesReimported == 1);

        // The watcher now reports fresh.ta.meta ADDED: the import it describes already ran.
        CHECK(fixture.tick() == 0);
        CHECK(fixture.controller.last_report().changesSeen == 1);
        CHECK(fixture.controller.last_report().sourcesReimported == 0);
        CHECK(fixture.tick() == 0);
        CHECK(fixture.controller.last_report().sourcesReimported == 0);
    }

    TEST_CASE("regression - a side file missing at import re-imports its owner when it appears") {
        reload_fixture fixture = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        fixture.pipeline.write_source("model.ta", "side model.bin\nmodel");
        fixture.pipeline.scan_and_import();
        CONJURE_REQUIRE_OK(fixture.watcher.snapshot());
        const asset_id model = fixture.pipeline.id_of("model.ta");
        CHECK(fixture.pipeline.database->find(model)->status == asset_record_status::IMPORT_FAILED);
        CHECK(fixture.pipeline.database->owners_of_side_file("assets://model.bin") == std::vector<asset_id>{model});

        CONJURE_REQUIRE_OK(fixture.pipeline.fileSystem->write_file("assets/model.bin", bytes_of("mesh")));
        fixture.tick();
        CHECK(fixture.controller.last_report().sourcesReimported >= 1);
        CHECK(fixture.pipeline.database->find(model)->status == asset_record_status::OK);
        REQUIRE(fixture.pipeline.cache().contains(model));
        CHECK(text_of(fixture.pipeline.cache().read(model).value().payload) == "model|mesh");
    }

    TEST_CASE("hot_reload - an added source is registered and importable") {
        reload_fixture fixture = {};
        fixture.pipeline.write_source("fresh.ta", "fresh");
        CHECK(fixture.tick() == 0);
        const std::optional<asset_id> fresh = fixture.pipeline.database->find_by_path("assets://fresh.ta");
        REQUIRE(fresh.has_value());
        CHECK(fixture.pipeline.cache().contains(*fresh));
        core::result<asset_handle> handle = fixture.pipeline.manager->load_blocking(*fresh);
        CONJURE_REQUIRE_OK(handle);
        CHECK(handle.value().get<test_asset>()->content == "fresh");
    }
}
