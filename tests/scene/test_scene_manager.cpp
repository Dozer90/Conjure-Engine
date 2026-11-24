#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "scene_test_fixture.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

#include <optional>
#include <string>
#include <vector>

using namespace conjure;
using namespace scene_test;
using scene::scene_error;

namespace {

    /// <summary>
    /// A scene file text with 'count' roots named "<prefix><i>", each with one child, asset references
    /// to the given blob and the given settings; guids are deterministic from 'guidBase'.
    /// </summary>
    [[nodiscard]] std::string make_scene_text(
      std::string_view name,
      std::string_view prefix,
      u32 count,
      u64 guidBase,
      const assets::asset_id &blob,
      std::string_view settings
    ) {
        std::string roots = {};
        std::string entities = {};
        for (u32 index = 0; index < count; ++index) {
            const std::string rootGuid = test_uuid(guidBase + index * 2).to_string();
            const std::string childGuid = test_uuid(guidBase + index * 2 + 1).to_string();
            if (index > 0) {
                roots += ",";
                entities += ",";
            }
            roots += "\"" + rootGuid + "\"";
            entities += "{\"components\":{\"conjure.scene.name\":{\"value\":\"" + std::string(prefix)
              + std::to_string(index) + "\"},\"conjure.scene.guid\":{\"value\":\"" + rootGuid + "\"},"
              + "\"test.asset_holder\":{\"asset\":\"" + blob.to_string() + "\"}}},";
            entities += "{\"components\":{\"conjure.scene.parent\":{\"value\":" + std::to_string(index * 2)
              + ",\"siblingIndex\":0},\"conjure.scene.name\":{\"value\":\"child\"},\"conjure.scene.guid\":{\"value\":\""
              + childGuid + "\"}}}";
        }
        return "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"" + std::string(name) + "\",\"settings\":"
          + std::string(settings) + ",\"roots\":[" + roots + "],\"archive\":{\"entities\":[" + entities + "]}}";
    }

    /// <summary>
    /// Adds a cooked scene blob to the fixture's source.
    /// </summary>
    void add_scene_blob(
      scene_fixture &fixture,
      const assets::asset_id &id,
      const std::string &text,
      std::vector<assets::asset_id> dependencies
    ) {
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<scene::scene_asset>::id();
        header.typeVersion = 1;
        header.importerVersion = 1;
        header.id = id;
        const std::byte *begin = reinterpret_cast<const std::byte *>(text.data());
        std::vector<std::byte> payload(begin, begin + text.size());
        fixture.source->add(assets::make_cooked_blob(header, std::move(dependencies), std::move(payload)));
    }

    struct manager_fixture {
        scene_fixture base = {};
        assets::importer_registry importers = {};
        assets::asset_id blob = test_uuid(900);
        assets::asset_id sceneA = test_uuid(901);
        assets::asset_id sceneB = test_uuid(902);

        manager_fixture() {
            REQUIRE(scene::register_asset_types(base.assets.types(), importers, base.registry).has_value());
            base.add_binary_blob(blob, "mesh");
            add_scene_blob(base, sceneA, make_scene_text("alpha", "a", 2, 10, blob, "{\"ambient\":0.5}"), {blob});
            add_scene_blob(base, sceneB, make_scene_text("beta", "b", 3, 20, blob, "{\"ambient\":0.9}"), {blob});
        }

        [[nodiscard]] scene::scene_manager &manager() noexcept {
            return base.manager;
        }

        [[nodiscard]] scene::scene &scene() noexcept {
            return base.active();
        }
    };
}

TEST_SUITE("scene/scene_manager") {

    TEST_CASE("scene_manager - load replace sets settings and roots") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const ecs::entity authored = fixture.base.create("authored");
        std::vector<scene::scene_id> loadedEvents = {};
        const core::event_subscription subscription = manager.on_loaded().subscribe([&](scene::scene_id id) {
            loadedEvents.push_back(id);
        });

        const core::result<scene::scene_id> loaded = manager.load(fixture.sceneA, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(loaded);
        const scene::scene_id id = loaded.value();
        CHECK(id.is_valid());
        CHECK_FALSE(id == scene::SCENE_ID_AUTHORED);
        CHECK(manager.is_loaded(id));
        CHECK(manager.find_loaded(fixture.sceneA) == std::optional<scene::scene_id>(id));
        CHECK(loadedEvents == std::vector<scene::scene_id>{id});
        // Replace drops the authored entity, takes the settings and the name.
        CHECK_FALSE(fixture.scene().contains(authored));
        CHECK(fixture.scene().name() == "alpha");
        CHECK(fixture.scene().settings().root()["ambient"].as_f64() == doctest::Approx(0.5));
        REQUIRE(fixture.scene().roots().size() == 2);
        CHECK(fixture.scene().name_of(fixture.scene().roots()[0].entity).view() == "a0");
        CHECK(fixture.scene().roots()[0].owner == id);
        CHECK(fixture.scene().roots()[1].owner == id);
        CHECK(fixture.scene().guid_of(fixture.scene().roots()[0].entity).value() == test_uuid(10));
        const std::optional<scene::loaded_scene_info> info = manager.info(id);
        REQUIRE(info.has_value());
        CHECK(info->asset == fixture.sceneA);
        CHECK(info->name == "alpha");
        CHECK(info->roots.size() == 2);
        CHECK(manager.loaded_scenes().size() == 1);
        // The blob the scene references is resident (dependency of the scene asset) and bound.
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::LOADED);
        const test_asset_holder *holder = fixture.base.world.get<test_asset_holder>(fixture.scene().roots()[0].entity);
        REQUIRE(holder != nullptr);
        CHECK(holder->asset.is_bound());
        REQUIRE(holder->asset.get() != nullptr);
        CHECK(holder->asset.get()->bytes.size() == 4);

        // A second REPLACE unloads the first.
        const core::result<scene::scene_id> replaced = manager.load(fixture.sceneB, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(replaced);
        CHECK_FALSE(manager.is_loaded(id));
        CHECK(manager.is_loaded(replaced.value()));
        CHECK(fixture.scene().name() == "beta");
        CHECK(fixture.scene().roots().size() == 3);
        CHECK(fixture.base.world.entity_count() == 6);
        CHECK(manager.loaded_scenes().size() == 1);
        CONJURE_CHECK_ERROR(manager.load(test_uuid(999)), scene_error::LOAD_FAILED);
        CONJURE_CHECK_ERROR(manager.load(fixture.blob), scene_error::LOAD_FAILED);
        fixture.base.step();
    }

    TEST_CASE("scene_manager - additive load keeps existing entities") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const ecs::entity authored = fixture.base.create("authored");
        fixture.scene().set_name("hand made");
        const core::result<scene::scene_id> first = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(first);
        const core::result<scene::scene_id> second = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(second);
        CHECK(fixture.scene().contains(authored));
        CHECK(fixture.scene().name() == "hand made");
        CHECK(fixture.scene().settings().root().size() == 0);
        REQUIRE(fixture.scene().roots().size() == 6);
        CHECK(fixture.scene().roots()[0].owner == scene::SCENE_ID_AUTHORED);
        CHECK(fixture.scene().roots()[1].owner == first.value());
        CHECK(fixture.scene().roots()[3].owner == second.value());
        CHECK(manager.loaded_scenes().size() == 2);
        CHECK(manager.loaded_scenes()[0].id == first.value());
        CHECK(manager.loaded_scenes()[1].id == second.value());
        CHECK(fixture.base.world.entity_count() == 11);
        fixture.base.step();
    }

    TEST_CASE("scene_manager - additive guid conflict regenerates with one WARN") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> first = manager.load(fixture.sceneA, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(first);
        testing::log_capture logs = {};
        const core::result<scene::scene_id> again = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(again);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "4 guid collision(s)"));
        CHECK(fixture.scene().roots().size() == 4);
        CHECK(fixture.scene().guids().size() == 8);
        CHECK(fixture.scene().find_guid(test_uuid(10)).is_null() == false);
        CHECK(manager.find_loaded(fixture.sceneA) == std::optional<scene::scene_id>(first.value()));
        CHECK(manager.loaded_scenes().size() == 2);
        fixture.base.step();
    }

    TEST_CASE("scene_manager - request_load applies at apply_pending_loads when complete [parallel]") {
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            core::jobs::job_system jobs = {};
            core::jobs::job_system_desc desc = {};
            desc.workerCount = workers;
            REQUIRE(jobs.start(desc).has_value());
            {
                manager_fixture fixture = {};
                scene::scene_manager &manager = fixture.manager();
                const scene::load_request_id request = manager.request_load(fixture.sceneA, scene::load_mode::ADDITIVE);
                CHECK(request.is_valid());
                CHECK(manager.pending_count() == 1);
                CHECK(manager.is_load_pending(fixture.sceneA));
                CHECK_FALSE(manager.is_load_pending(fixture.sceneB));
                // Nothing is applied before the asset is resident.
                manager.apply_pending_loads();
                CHECK(manager.pending_count() == 1);
                CHECK(fixture.scene().roots().empty());
                fixture.base.assets.flush();
                // The FRAME_BEGIN system applies it.
                fixture.base.step();
                CHECK(manager.pending_count() == 0);
                CHECK_FALSE(manager.is_load_pending(fixture.sceneA));
                CHECK(fixture.scene().roots().size() == 2);
                CHECK(manager.find_loaded(fixture.sceneA).has_value());
                CHECK(fixture.scene().name().empty());
            }
            jobs.stop();
        }
    }

    TEST_CASE("scene_manager - requests apply in order") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        std::vector<scene::scene_id> loadedEvents = {};
        const core::event_subscription subscription = manager.on_loaded().subscribe([&](scene::scene_id id) {
            loadedEvents.push_back(id);
        });
        const scene::load_request_id firstRequest = manager.request_load(fixture.sceneB, scene::load_mode::ADDITIVE);
        const scene::load_request_id secondRequest = manager.request_load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CHECK_FALSE(firstRequest == secondRequest);
        CHECK(manager.pending_count() == 2);
        fixture.base.assets.flush();
        manager.apply_pending_loads();
        CHECK(manager.pending_count() == 0);
        REQUIRE(loadedEvents.size() == 2);
        const std::vector<scene::loaded_scene_info> scenes = manager.loaded_scenes();
        REQUIRE(scenes.size() == 2);
        CHECK(scenes[0].asset == fixture.sceneB);
        CHECK(scenes[1].asset == fixture.sceneA);
        CHECK(scenes[0].id == loadedEvents[0]);
        REQUIRE(fixture.scene().roots().size() == 5);
        CHECK(fixture.scene().name_of(fixture.scene().roots()[0].entity).view() == "b0");
        CHECK(fixture.scene().name_of(fixture.scene().roots()[3].entity).view() == "a0");
        fixture.base.step();
    }

    TEST_CASE("scene_manager - failed asset drops request and fires on_load_failed") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        std::vector<scene::load_request_id> failures = {};
        std::vector<std::string> messages = {};
        const core::event_subscription subscription =
          manager.on_load_failed().subscribe([&](scene::load_request_id id, const core::error &failure) {
              failures.push_back(id);
              messages.push_back(failure.to_string());
          });
        testing::log_capture logs = {};
        logs.allow_errors();
        const scene::load_request_id missing = manager.request_load(test_uuid(998), scene::load_mode::ADDITIVE);
        const scene::load_request_id good = manager.request_load(fixture.sceneA, scene::load_mode::ADDITIVE);
        // An asset of the wrong type is not a scene either.
        const scene::load_request_id wrongType = manager.request_load(fixture.blob, scene::load_mode::ADDITIVE);
        fixture.base.assets.flush();
        manager.apply_pending_loads();
        CHECK(manager.pending_count() == 0);
        REQUIRE(failures.size() == 2);
        CHECK(failures[0] == missing);
        CHECK(failures[1] == wrongType);
        CHECK(messages[0].find("LOAD_FAILED") != std::string::npos);
        // Two failures of ours plus the assets module's own type-mismatch report.
        CHECK(logs.count(core::log_level::ERR) >= 2);
        CHECK(manager.find_loaded(fixture.sceneA).has_value());
        CHECK(fixture.scene().roots().size() == 2);
        CONJURE_UNUSED(good);
        fixture.base.step();
    }

    TEST_CASE("regression - code sweep F627 request_load replaces by default, as load and load_file do") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const ecs::entity authored = fixture.base.create("authored");
        const core::result<scene::scene_id> earlier = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(earlier);
        REQUIRE(fixture.scene().roots().size() == 3);
        std::optional<scene::scene_id> loaded = std::nullopt;

        SUBCASE("request_load") {
            const scene::load_request_id request = manager.request_load(fixture.sceneB);
            REQUIRE(request.is_valid());
            fixture.base.assets.flush();
            manager.apply_pending_loads();
            CHECK(manager.pending_count() == 0);
            loaded = manager.find_loaded(fixture.sceneB);
        }
        SUBCASE("load") {
            const core::result<scene::scene_id> result = manager.load(fixture.sceneB);
            CONJURE_REQUIRE_OK(result);
            loaded = result.value();
        }
        SUBCASE("load_file") {
            core::vfs::memory_file_system fs = {};
            const std::string text = make_scene_text("beta", "b", 3, 20, fixture.blob, "{\"ambient\":0.9}");
            CONJURE_REQUIRE_OK(fs.write_text_file("scenes/beta.scene.json", text));
            const core::result<scene::scene_id> result = manager.load_file(fs, "scenes/beta.scene.json");
            CONJURE_REQUIRE_OK(result);
            loaded = result.value();
        }

        // The earlier scene and the authored entity are gone; the new scene's name and settings apply.
        REQUIRE(loaded.has_value());
        CHECK(manager.is_loaded(*loaded));
        CHECK_FALSE(manager.is_loaded(earlier.value()));
        CHECK(manager.loaded_scenes().size() == 1);
        CHECK_FALSE(fixture.scene().contains(authored));
        CHECK(fixture.scene().name() == "beta");
        CHECK(fixture.scene().settings().root()["ambient"].as_f64() == doctest::Approx(0.9));
        CHECK(fixture.scene().roots().size() == 3);
        CHECK(fixture.base.world.entity_count() == 6);
        fixture.base.step();
    }

    TEST_CASE("scene_manager - request_load with ADDITIVE keeps the loaded scenes and the authored entities") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> earlier = manager.load(fixture.sceneA, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(earlier);
        // Made after the REPLACE load, which would have dropped it.
        const ecs::entity authored = fixture.base.create("authored");

        const scene::load_request_id request = manager.request_load(fixture.sceneB, scene::load_mode::ADDITIVE);
        REQUIRE(request.is_valid());
        fixture.base.assets.flush();
        manager.apply_pending_loads();

        CHECK(manager.pending_count() == 0);
        CHECK(manager.is_loaded(earlier.value()));
        REQUIRE(manager.find_loaded(fixture.sceneB).has_value());
        CHECK(manager.loaded_scenes().size() == 2);
        CHECK(fixture.scene().contains(authored));
        CHECK(fixture.scene().name() == "alpha");
        CHECK(fixture.scene().settings().root()["ambient"].as_f64() == doctest::Approx(0.5));
        CHECK(fixture.scene().roots().size() == 6);
        fixture.base.step();
    }

    TEST_CASE("scene_manager - unload destroys only that scene") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const ecs::entity authored = fixture.base.create("authored");
        const core::result<scene::scene_id> first = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(first);
        const core::result<scene::scene_id> second = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(second);
        CHECK(fixture.base.world.entity_count() == 11);
        CONJURE_REQUIRE_OK(manager.unload(first.value()));
        CHECK_FALSE(manager.is_loaded(first.value()));
        CHECK(manager.is_loaded(second.value()));
        CHECK(fixture.base.world.entity_count() == 7);
        CHECK(fixture.scene().contains(authored));
        REQUIRE(fixture.scene().roots().size() == 4);
        CHECK(fixture.scene().name_of(fixture.scene().roots()[1].entity).view() == "b0");
        CHECK(manager.loaded_scenes().size() == 1);
        CONJURE_CHECK_ERROR(manager.unload(first.value()), scene_error::SCENE_NOT_LOADED);
        CONJURE_CHECK_ERROR(manager.unload(scene::scene_id{}), scene_error::SCENE_NOT_LOADED);
        // request_unload applies at the next FRAME_BEGIN; unknown ids are ignored.
        manager.request_unload(second.value());
        manager.request_unload(first.value());
        CHECK(manager.is_loaded(second.value()));
        fixture.base.step();
        CHECK_FALSE(manager.is_loaded(second.value()));
        CHECK(fixture.base.world.entity_count() == 1);
        CHECK(manager.loaded_scenes().empty());
    }

    TEST_CASE("scene_manager - on_before_unload fires before entities die with roots span") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> loaded = manager.load(fixture.sceneB, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(loaded);
        std::vector<std::string> order = {};
        std::vector<ecs::entity> seenRoots = {};
        const core::event_subscription before =
          manager.on_before_unload().subscribe([&](scene::scene_id id, std::span<const ecs::entity> roots) {
              CHECK(id == loaded.value());
              order.push_back("before_unload");
              for (const ecs::entity root : roots) {
                  CHECK(fixture.scene().contains(root));
                  seenRoots.push_back(root);
              }
          });
        const ecs::observer_id destroyed = fixture.base.world.observe_destroyed(
          [&](ecs::world &, ecs::entity, ecs::command_buffer &) {
              if (order.empty() || order.back() != "destroyed") {
                  order.push_back("destroyed");
              }
          });
        const u32 residentBefore = fixture.base.assets.stats().loaded;
        CHECK(residentBefore == 2);
        CONJURE_REQUIRE_OK(manager.unload(loaded.value()));
        fixture.base.world.unobserve(destroyed);
        REQUIRE(order.size() == 2);
        CHECK(order[0] == "before_unload");
        CHECK(order[1] == "destroyed");
        CHECK(seenRoots.size() == 3);
        CHECK(fixture.base.world.entity_count() == 0);
        // collect() ran after the handles dropped: the scene asset and its dependency are gone.
        CHECK(fixture.base.assets.stats().loaded == 0);
        CHECK(fixture.base.assets.state_of(fixture.sceneB) == assets::asset_state::UNLOADED);
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::UNLOADED);
    }

    TEST_CASE("scene_manager - unload_all reverse order then clear") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const ecs::entity authored = fixture.base.create("authored");
        const core::result<scene::scene_id> first = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(first);
        const core::result<scene::scene_id> second = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(second);
        std::vector<scene::scene_id> order = {};
        const core::event_subscription subscription =
          manager.on_before_unload().subscribe([&](scene::scene_id id, std::span<const ecs::entity>) {
              order.push_back(id);
          });
        manager.unload_all();
        // The authored entity left over after the records gets the event too, as SCENE_ID_AUTHORED.
        CHECK(order == std::vector<scene::scene_id>{second.value(), first.value(), scene::SCENE_ID_AUTHORED});
        CHECK(manager.loaded_scenes().empty());
        CHECK_FALSE(fixture.scene().contains(authored));
        CHECK(fixture.base.world.entity_count() == 0);
        CHECK(fixture.scene().roots().empty());
        CHECK(fixture.base.assets.stats().loaded == 0);
        // The manager keeps working afterwards.
        CONJURE_REQUIRE_OK(manager.load(fixture.sceneA));
        CHECK(fixture.scene().roots().size() == 2);
    }

    TEST_CASE("scene_manager - unload_all fires on_before_unload for authored entities") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const ecs::entity authored = fixture.base.create("authored");
        const ecs::entity child = fixture.base.create("child", authored);
        std::vector<scene::scene_id> ids = {};
        std::vector<ecs::entity> seenRoots = {};
        const core::event_subscription subscription =
          manager.on_before_unload().subscribe([&](scene::scene_id id, std::span<const ecs::entity> roots) {
              ids.push_back(id);
              for (const ecs::entity root : roots) {
                  // Still alive: handlers (scripting's destroy_all) run before the clear.
                  CHECK(fixture.scene().contains(root));
                  seenRoots.push_back(root);
              }
          });

        // No record is loaded: the authored content alone gets the event before it dies.
        manager.unload_all();
        CHECK(ids == std::vector<scene::scene_id>{scene::SCENE_ID_AUTHORED});
        CHECK(seenRoots == std::vector<ecs::entity>{authored});
        CHECK_FALSE(fixture.scene().contains(authored));
        CHECK_FALSE(fixture.scene().contains(child));
        CHECK(fixture.base.world.entity_count() == 0);

        // An empty world has nothing to announce.
        manager.unload_all();
        CHECK(ids.size() == 1);
    }

    TEST_CASE("regression - unload survives an on_before_unload handler that loads a scene") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> first = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(first);
        // A one-shot handler loading another scene: the record list grows (and moves) under unload.
        std::optional<scene::scene_id> second = std::nullopt;
        bool fired = false;
        const core::event_subscription subscription =
          manager.on_before_unload().subscribe([&](scene::scene_id, std::span<const ecs::entity>) {
              if (fired) {
                  return;
              }
              fired = true;
              const core::result<scene::scene_id> loaded = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
              CHECK(loaded.has_value());
              if (loaded.has_value()) {
                  second = loaded.value();
              }
          });

        CONJURE_REQUIRE_OK(manager.unload(first.value()));
        REQUIRE(second.has_value());
        CHECK_FALSE(manager.is_loaded(first.value()));
        CHECK(manager.is_loaded(*second));
        CHECK(manager.loaded_scenes().size() == 1);
        // Scene a's two roots and their children are gone; scene b's three pairs remain.
        CHECK(fixture.base.world.entity_count() == 6);
        CHECK(fixture.scene().roots().size() == 3);
    }

    TEST_CASE("regression - a load survives an on_structure_changed handler that loads a scene") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        core::vfs::memory_file_system fs = {};
        const std::string text = make_scene_text("filed", "f", 2, 30, fixture.blob, "{}");
        CONJURE_REQUIRE_OK(fs.write_text_file("scenes/filed.scene.json", text));
        // A one-shot handler loading another scene while the file's entities are being linked in.
        std::optional<scene::scene_id> nested = std::nullopt;
        bool fired = false;
        const core::event_subscription subscription = fixture.scene().on_structure_changed().subscribe([&]() {
            if (fired) {
                return;
            }
            fired = true;
            const core::result<scene::scene_id> loaded = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
            CHECK(loaded.has_value());
            if (loaded.has_value()) {
                nested = loaded.value();
            }
        });

        const core::result<scene::scene_id> filed =
          manager.load_file(fs, "scenes/filed.scene.json", scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(filed);
        REQUIRE(nested.has_value());
        CHECK(manager.is_loaded(filed.value()));
        CHECK(manager.is_loaded(*nested));
        const std::optional<scene::loaded_scene_info> info = manager.info(filed.value());
        REQUIRE(info.has_value());
        CHECK(info->path == "scenes/filed.scene.json");
        CHECK(info->roots.size() == 2);
        CHECK(manager.loaded_scenes().size() == 2);
        // Unloading the file's scene leaves scene b's three pairs alone.
        CONJURE_REQUIRE_OK(manager.unload(filed.value()));
        CHECK(manager.is_loaded(*nested));
        CHECK(fixture.base.world.entity_count() == 6);
    }

    TEST_CASE("regression - a load unloaded by an on_structure_changed handler spares the slot's new scene") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        core::vfs::memory_file_system fs = {};
        const std::string text = make_scene_text("filed", "f", 2, 30, fixture.blob, "{}");
        CONJURE_REQUIRE_OK(fs.write_text_file("scenes/filed.scene.json", text));
        std::vector<scene::scene_id> loadedEvents = {};
        const core::event_subscription loadedSubscription = manager.on_loaded().subscribe([&](scene::scene_id id) {
            loadedEvents.push_back(id);
        });
        // A one-shot handler that unloads everything, the file's scene in flight included, then loads scene b,
        // which takes over the file scene's freed record slot.
        std::optional<scene::scene_id> nested = std::nullopt;
        bool fired = false;
        const core::event_subscription subscription = fixture.scene().on_structure_changed().subscribe([&]() {
            if (fired) {
                return;
            }
            fired = true;
            manager.unload_all();
            const core::result<scene::scene_id> loaded = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
            CHECK(loaded.has_value());
            if (loaded.has_value()) {
                nested = loaded.value();
            }
        });

        CONJURE_CHECK_ERROR(
          manager.load_file(fs, "scenes/filed.scene.json", scene::load_mode::ADDITIVE),
          scene_error::SCENE_NOT_LOADED
        );
        REQUIRE(nested.has_value());
        CHECK(nested->index == 0);
        CHECK(manager.is_loaded(*nested));
        // The file's load neither announced itself nor took over scene b's record.
        CHECK(loadedEvents == std::vector<scene::scene_id>{*nested});
        const std::vector<scene::loaded_scene_info> scenes = manager.loaded_scenes();
        REQUIRE(scenes.size() == 1);
        CHECK(scenes.front().id == *nested);
        CHECK(scenes.front().path.empty());
        CHECK(scenes.front().roots.size() == 3);
        CHECK(fixture.base.world.entity_count() == 6);
        CONJURE_REQUIRE_OK(manager.unload(*nested));
        CHECK(manager.loaded_scenes().empty());
        CHECK(fixture.base.world.entity_count() == 0);
    }

    TEST_CASE("regression - unload_all retires a scene an on_before_unload handler loads") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        // Loads scene b once, from the first event that names 'trigger'.
        scene::scene_id trigger = {};
        std::optional<scene::scene_id> late = std::nullopt;
        const core::event_subscription subscription =
          manager.on_before_unload().subscribe([&](scene::scene_id id, std::span<const ecs::entity>) {
              if (late.has_value() || !(id == trigger)) {
                  return;
              }
              const core::result<scene::scene_id> loaded = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
              CHECK(loaded.has_value());
              if (loaded.has_value()) {
                  late = loaded.value();
              }
          });

        SUBCASE("loaded while a record unloads") {
            const core::result<scene::scene_id> first = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
            CONJURE_REQUIRE_OK(first);
            trigger = first.value();
        }
        SUBCASE("loaded while the authored content unloads") {
            const ecs::entity authored = fixture.base.create("authored");
            REQUIRE(fixture.scene().contains(authored));
            trigger = scene::SCENE_ID_AUTHORED;
        }

        manager.unload_all();
        REQUIRE(late.has_value());
        CHECK_FALSE(manager.is_loaded(*late));
        CHECK(manager.loaded_scenes().empty());
        CHECK_FALSE(manager.find_loaded(fixture.sceneB).has_value());
        CHECK(fixture.scene().roots().empty());
        CHECK(fixture.base.world.entity_count() == 0);

        // The manager stays usable: scene b loads again into a fresh record.
        const core::result<scene::scene_id> again = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(again);
        CHECK(manager.loaded_scenes().size() == 1);
        CHECK(fixture.base.world.entity_count() == 6);
    }

    TEST_CASE("scene_manager - load_file from memory vfs") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        core::vfs::memory_file_system fs = {};
        const std::string text = make_scene_text("filed", "f", 2, 30, fixture.blob, "{\"gravity\":-1}");
        CONJURE_REQUIRE_OK(fs.write_text_file("scenes/filed.scene.json", text));
        const core::result<scene::scene_id> loaded = manager.load_file(fs, "scenes/filed.scene.json");
        CONJURE_REQUIRE_OK(loaded);
        CHECK(fixture.scene().name() == "filed");
        CHECK(fixture.scene().settings().root()["gravity"].as_f64() == doctest::Approx(-1.0));
        CHECK(fixture.scene().roots().size() == 2);
        const std::optional<scene::loaded_scene_info> info = manager.info(loaded.value());
        REQUIRE(info.has_value());
        CHECK(info->path == "scenes/filed.scene.json");
        CHECK(info->asset.is_null());
        CHECK_FALSE(manager.find_loaded(assets::asset_id()).has_value());
        // The referenced blob was requested as a batch and is resident once the manager has updated.
        fixture.base.assets.flush();
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::LOADED);
        CONJURE_REQUIRE_OK(manager.unload(loaded.value()));
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::UNLOADED);
        CONJURE_CHECK_ERROR(manager.load_file(fs, "scenes/missing.scene.json"), core::error_code::NOT_FOUND);
        CONJURE_REQUIRE_OK(fs.write_text_file("scenes/bad.scene.json", "{\"format\":\"wrong\",\"version\":1}"));
        CONJURE_CHECK_ERROR(manager.load_file(fs, "scenes/bad.scene.json"), scene_error::BAD_FORMAT);
    }

    TEST_CASE("scene_manager - collect called after unload") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> loaded = manager.load(fixture.sceneA, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(fixture.base.assets.stats().loaded == 2);
        // A handle somebody else holds keeps its asset across the unload.
        assets::asset_handle keep = fixture.base.assets.make_handle(fixture.blob);
        CONJURE_REQUIRE_OK(manager.unload(loaded.value()));
        CHECK(fixture.base.assets.state_of(fixture.sceneA) == assets::asset_state::UNLOADED);
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::LOADED);
        keep.reset();
        CHECK(fixture.base.assets.collect() == 1);
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::UNLOADED);
    }

    TEST_CASE("scene_manager - reparented root changes owner") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> first = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(first);
        const core::result<scene::scene_id> second = manager.load(fixture.sceneB, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(second);
        const ecs::entity rootA = fixture.scene().roots()[0].entity;
        const ecs::entity rootB = fixture.scene().roots()[2].entity;
        CHECK(fixture.scene().root_owner(rootA) == std::optional<scene::scene_id>(first.value()));
        CHECK(fixture.scene().root_owner(rootB) == std::optional<scene::scene_id>(second.value()));

        // Moving b's root under a's root makes it part of scene a; unloading b leaves it alive. The
        // records report the roots as they are now, not as the load left them.
        CONJURE_REQUIRE_OK(fixture.scene().set_parent(rootB, rootA));
        CHECK_FALSE(fixture.scene().root_owner(rootB).has_value());
        REQUIRE(manager.info(second.value()).has_value());
        CHECK(manager.info(second.value())->roots.size() == 2);
        CHECK(manager.loaded_scenes()[1].roots.size() == 2);
        CONJURE_REQUIRE_OK(manager.unload(second.value()));
        CHECK_FALSE(manager.info(second.value()).has_value());
        CHECK(fixture.scene().contains(rootB));
        CHECK(fixture.scene().tree().parent_of(rootB) == rootA);
        CHECK(fixture.base.world.entity_count() == 4 + 2);

        // Detaching it again makes it a root of scene a (the tree it came from).
        CONJURE_REQUIRE_OK(fixture.scene().set_parent(rootB, ecs::NULL_ENTITY));
        CHECK(fixture.scene().root_owner(rootB) == std::optional<scene::scene_id>(first.value()));
        CONJURE_REQUIRE_OK(manager.unload(first.value()));
        CHECK_FALSE(fixture.scene().contains(rootB));
        CHECK(fixture.base.world.entity_count() == 0);
    }

    TEST_CASE("regression - raw remove parent keeps the loaded scene owner") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> loaded = manager.load(fixture.sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(loaded);
        const ecs::entity rootA = fixture.scene().roots()[0].entity;
        REQUIRE(fixture.scene().tree().children_of(rootA).size() == 1);
        const ecs::entity child = fixture.scene().tree().children_of(rootA)[0];

        // A script detaching the child through the raw ECS: it becomes a root of the same scene.
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.base.world.remove<scene::parent>(child));
        fixture.base.step();
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(fixture.scene().root_owner(child) == std::optional<scene::scene_id>(loaded.value()));
        REQUIRE(manager.info(loaded.value()).has_value());
        CHECK(manager.info(loaded.value())->roots.size() == 3);
        CONJURE_CHECK_OK(fixture.scene().validate());

        CONJURE_REQUIRE_OK(manager.unload(loaded.value()));
        CHECK_FALSE(fixture.scene().contains(child));
        CHECK(fixture.base.world.entity_count() == 0);
    }

    TEST_CASE("regression - bad file under REPLACE keeps the current scene") {
        manager_fixture fixture = {};
        scene::scene_manager &manager = fixture.manager();
        const core::result<scene::scene_id> loaded = manager.load(fixture.sceneA, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(loaded);
        const ecs::entity authored = fixture.base.create("authored");
        core::vfs::memory_file_system fs = {};

        SUBCASE("a guid listed twice") {
            const std::string duplicate = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"dup\","
              "\"settings\":{},\"roots\":[],\"archive\":{\"entities\":["
              "{\"components\":{\"conjure.scene.guid\":{\"value\":\"" + test_uuid(500).to_string() + "\"}}},"
              "{\"components\":{\"conjure.scene.guid\":{\"value\":\"" + test_uuid(500).to_string() + "\"}}}]}}";
            CONJURE_REQUIRE_OK(fs.write_text_file("scenes/dup.scene.json", duplicate));
            CONJURE_CHECK_ERROR(manager.load_file(fs, "scenes/dup.scene.json"), scene_error::DUPLICATE_GUID);
        }
        SUBCASE("a parent loop") {
            const std::string loop = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"loop\",\"settings\":{},"
              "\"roots\":[],\"archive\":{\"entities\":["
              "{\"components\":{\"conjure.scene.parent\":{\"value\":1,\"siblingIndex\":0}}},"
              "{\"components\":{\"conjure.scene.parent\":{\"value\":0,\"siblingIndex\":0}}}]}}";
            CONJURE_REQUIRE_OK(fs.write_text_file("scenes/loop.scene.json", loop));
            CONJURE_CHECK_ERROR(manager.load_file(fs, "scenes/loop.scene.json"), scene_error::HIERARCHY_CYCLE);
        }
        SUBCASE("a layer outside 0..31") {
            const std::string layered = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"layered\","
              "\"settings\":{},\"roots\":[],\"archive\":{\"entities\":["
              "{\"components\":{\"conjure.scene.layer\":{\"index\":32}}}]}}";
            CONJURE_REQUIRE_OK(fs.write_text_file("scenes/layered.scene.json", layered));
            CONJURE_CHECK_ERROR(manager.load_file(fs, "scenes/layered.scene.json"), scene_error::BAD_FORMAT);
        }

        // Nothing was unloaded or created: the loaded scene, its settings and the authored entity stay.
        CHECK(manager.is_loaded(loaded.value()));
        CHECK(manager.loaded_scenes().size() == 1);
        CHECK(fixture.scene().name() == "alpha");
        CHECK(fixture.scene().settings().root()["ambient"].as_f64() == doctest::Approx(0.5));
        CHECK(fixture.scene().contains(authored));
        CHECK(fixture.scene().roots().size() == 3);
        CHECK(fixture.base.world.entity_count() == 5);
        CHECK(fixture.base.assets.state_of(fixture.sceneA) == assets::asset_state::LOADED);
        CONJURE_CHECK_OK(fixture.scene().validate());
    }
}
