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
        CHECK(order == std::vector<scene::scene_id>{second.value(), first.value()});
        CHECK(manager.loaded_scenes().empty());
        CHECK_FALSE(fixture.scene().contains(authored));
        CHECK(fixture.base.world.entity_count() == 0);
        CHECK(fixture.scene().roots().empty());
        CHECK(fixture.base.assets.stats().loaded == 0);
        // The manager keeps working afterwards.
        CONJURE_REQUIRE_OK(manager.load(fixture.sceneA));
        CHECK(fixture.scene().roots().size() == 2);
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
