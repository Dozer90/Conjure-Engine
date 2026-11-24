#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/serialization.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_fields.h"
#include "conjure/scripting/script_runtime.h"

#include <array>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    [[nodiscard]] script_entry make_entry(u64 classHash, std::string_view fieldsJson = "{}") {
        script_entry entry = {};
        entry.classHash = classHash;
        entry.fields = json(fieldsJson);
        return entry;
    }
}

TEST_SUITE("scripting/script_component") {

    TEST_CASE("script_component - is trivially copyable and 4 bytes") {
        CHECK(sizeof(script_component) == 4);
        CHECK(std::is_trivially_copyable_v<script_component>);
        CHECK(std::is_standard_layout_v<script_component>);
        const script_component empty = {};
        CHECK(empty.setHandle == INVALID_SET_HANDLE);
        CHECK(INVALID_SET_HANDLE == 0);
        CHECK(INVALID_SLOT == -1);
        CHECK(ecs::component_name<script_component>::VALUE == SCRIPT_COMPONENT_NAME);
    }

    TEST_CASE("script_store - add and remove entries") {
        script_store store = {};
        const u32 handle = store.create_set(ecs::entity{3, 1});
        CHECK(handle != INVALID_SET_HANDLE);
        CHECK(store.size() == 1);
        REQUIRE(store.find(handle) != nullptr);
        CHECK(store.find(handle)->owner == ecs::entity{3, 1});
        CHECK(store.find(handle)->entries.empty());

        const script_entry_id first = store.add_entry(handle, make_entry(11, R"({"a":1})"));
        const script_entry_id second = store.add_entry(handle, make_entry(22));
        REQUIRE(first.is_valid());
        REQUIRE(second.is_valid());
        CHECK(first.entryIndex == 0);
        CHECK(second.entryIndex == 1);
        CHECK(first.setHandle == handle);
        REQUIRE(store.find_entry(first) != nullptr);
        CHECK(store.find_entry(first)->classHash == 11);
        CHECK(store.find_entry(first)->fields.root()["a"].as_i64() == 1);
        CHECK_FALSE(store.find_entry(first)->is_live());

        CHECK(store.remove_entry(first));
        CHECK(store.find(handle)->entries.size() == 1);
        CHECK(store.find_entry(first)->classHash == 22);     // the second entry shifted into index 0
        CHECK(store.find_entry(second) == nullptr);
        CHECK_FALSE(store.remove_entry(second));
        CHECK_FALSE(store.add_entry(INVALID_SET_HANDLE, make_entry(1)).is_valid());
        CHECK(store.find_entry({}) == nullptr);
    }

    TEST_CASE("script_store - eight scripts per entity at most") {
        log_capture logs = {};
        script_store store = {};
        const u32 handle = store.create_set({});
        for (u32 index = 0; index < MAX_SCRIPTS_PER_ENTITY; ++index) {
            CAPTURE(index);
            CHECK(store.add_entry(handle, make_entry(index + 1)).is_valid());
        }
        CHECK_FALSE(store.add_entry(handle, make_entry(99)).is_valid());
        CHECK(store.find(handle)->entries.size() == MAX_SCRIPTS_PER_ENTITY);
        CHECK(logs.count(core::log_level::WARN) == 1);
    }

    TEST_CASE("script_store - set handle is invalid after free and never reused as-is") {
        script_store store = {};
        const u32 handle = store.create_set({});
        CHECK(store.contains(handle));
        CHECK(store.free_set(handle));
        CHECK_FALSE(store.contains(handle));
        CHECK(store.find(handle) == nullptr);
        CHECK_FALSE(store.free_set(handle));
        CHECK(store.size() == 0);

        const u32 reused = store.create_set({});
        CHECK(script_store::handle_index(reused) == script_store::handle_index(handle));
        CHECK(reused != handle);
        CHECK(script_store::handle_generation(reused) == script_store::handle_generation(handle) + 1);
        CHECK(store.find(handle) == nullptr);
        CHECK(store.find(reused) != nullptr);
        CHECK_FALSE(store.free_set(INVALID_SET_HANDLE));
        CHECK(store.find(script_store::make_handle(500, 1)) == nullptr);
        CHECK(script_store::make_handle(5, 3) == ((3u << 20) | 5u));
        CHECK(script_store::handle_generation(script_store::make_handle(5, 0xFFF)) == 0xFFF);
    }

    TEST_CASE("script_store - reverse map follows slots through map, unmap and remove") {
        script_store store = {};
        const u32 handle = store.create_set({});
        const script_entry_id first = store.add_entry(handle, make_entry(1));
        const script_entry_id second = store.add_entry(handle, make_entry(2));
        const script_entry_id third = store.add_entry(handle, make_entry(3));
        store.map_slot(10, first);
        store.map_slot(11, second);
        store.map_slot(12, third);
        CHECK(store.mapped_slot_count() == 3);
        CHECK(store.find_entry(second)->managedSlot == 11);
        CHECK(store.resolve_slot(11) == second);
        CHECK_FALSE(store.resolve_slot(99).has_value());
        CHECK_FALSE(store.resolve_slot(-1).has_value());

        CHECK(store.remove_entry(first));
        CHECK(store.mapped_slot_count() == 2);
        CHECK_FALSE(store.resolve_slot(10).has_value());
        REQUIRE(store.resolve_slot(11).has_value());
        CHECK(store.resolve_slot(11)->entryIndex == 0);
        CHECK(store.find_entry(*store.resolve_slot(11))->classHash == 2);
        CHECK(store.resolve_slot(12)->entryIndex == 1);
        CHECK(store.find_entry(*store.resolve_slot(12))->classHash == 3);

        store.unmap_slot(12);
        CHECK_FALSE(store.resolve_slot(12).has_value());
        CHECK(store.find_entry(*store.resolve_slot(11))->is_live());
        CHECK_FALSE(store.find(handle)->entries[1].is_live());
        store.unmap_slot(-5);
        store.unmap_slot(1000);

        CHECK(store.free_set(handle));
        CHECK(store.mapped_slot_count() == 0);
        CHECK_FALSE(store.resolve_slot(11).has_value());
    }

    TEST_CASE("script_store - mapped_slot_count follows map, remap, unmap, remove_entry, free_set and clear") {
        script_store store = {};
        // The counter must always equal the number of valid records in the reverse map.
        const auto validRecords = [&store]() {
            usize valid = 0;
            for (i32 slot = 0; slot < 16; ++slot) {
                if (store.resolve_slot(slot).has_value()) {
                    ++valid;
                }
            }
            return valid;
        };
        const u32 handle = store.create_set({});
        const script_entry_id first = store.add_entry(handle, make_entry(1));
        const script_entry_id second = store.add_entry(handle, make_entry(2));
        const script_entry_id third = store.add_entry(handle, make_entry(3));
        CHECK(store.mapped_slot_count() == 0);

        store.map_slot(4, first);
        store.map_slot(5, second);
        CHECK(store.mapped_slot_count() == 2);
        store.map_slot(5, second);                  // the same mapping again
        CHECK(store.mapped_slot_count() == 2);
        store.map_slot(5, third);                   // slot 5 now names the third entry; the second still claims it
        CHECK(store.mapped_slot_count() == 2);
        CHECK(store.mapped_slot_count() == validRecords());

        store.unmap_slot(5);
        CHECK(store.mapped_slot_count() == 1);
        store.unmap_slot(5);                        // nothing left to unmap
        store.unmap_slot(12);                       // never mapped
        CHECK(store.mapped_slot_count() == 1);
        CHECK(store.mapped_slot_count() == validRecords());

        // Removing the first entry unmaps slot 4 and shifts the second down, whose stale claim on slot 5 is
        // written back into the reverse map.
        REQUIRE(store.find(handle)->entries[1].is_live());
        CHECK(store.remove_entry(first));
        CHECK(store.mapped_slot_count() == 1);
        CHECK(store.mapped_slot_count() == validRecords());
        REQUIRE(store.resolve_slot(5).has_value());
        CHECK(store.resolve_slot(5)->entryIndex == 0);
        CHECK(store.find_entry(*store.resolve_slot(5))->classHash == 2);

        CHECK(store.free_set(handle));
        CHECK(store.mapped_slot_count() == 0);
        CHECK(store.mapped_slot_count() == validRecords());

        const u32 other = store.create_set({});
        store.map_slot(9, store.add_entry(other, make_entry(4)));
        store.map_slot(10, store.add_entry(other, make_entry(5)));
        CHECK(store.mapped_slot_count() == 2);
        store.clear();
        CHECK(store.mapped_slot_count() == 0);
        CHECK(store.mapped_slot_count() == validRecords());
    }

    TEST_CASE("script_store - clone_set deep copies entries without slots") {
        script_store store = {};
        const u32 source = store.create_set(ecs::entity{1, 1});
        const script_entry_id id = store.add_entry(source, make_entry(7, R"({"hp": 3})"));
        store.map_slot(4, id);
        store.find_entry(id)->flags =
          script_entry_flags::CREATED | script_entry_flags::STARTED | script_entry_flags::CLASS_MISSING;

        const u32 copy = store.clone_set(source, ecs::entity{2, 1});
        REQUIRE(copy != INVALID_SET_HANDLE);
        CHECK(copy != source);
        const script_set *cloned = store.find(copy);
        REQUIRE(cloned != nullptr);
        CHECK(cloned->owner == ecs::entity{2, 1});
        REQUIRE(cloned->entries.size() == 1);
        CHECK(cloned->entries[0].classHash == 7);
        CHECK(cloned->entries[0].fields.root()["hp"].as_i64() == 3);
        CHECK_FALSE(cloned->entries[0].is_live());
        CHECK(cloned->entries[0].has_flag(script_entry_flags::CLASS_MISSING));
        CHECK_FALSE(cloned->entries[0].has_flag(script_entry_flags::STARTED));
        CHECK_FALSE(cloned->entries[0].has_flag(script_entry_flags::CREATED));     // a copy runs OnCreate
        CHECK(store.find_entry(id)->is_live());
        CHECK(store.clone_set(INVALID_SET_HANDLE) == INVALID_SET_HANDLE);

        u32 visited = 0;
        store.for_each_set([&visited](u32, script_set &) {
            ++visited;
        });
        CHECK(visited == 2);
        store.clear();
        CHECK(store.size() == 0);
        CHECK(store.mapped_slot_count() == 0);
    }

    TEST_CASE("register_script_component - registers once with custom copy and destruct hooks") {
        ecs::component_registry registry = {};
        const core::result<ecs::component_id> first = register_script_component(registry);
        CONJURE_REQUIRE_OK(first);
        const core::result<ecs::component_id> second = register_script_component(registry);
        CONJURE_REQUIRE_OK(second);
        CHECK(first.value() == second.value());
        CHECK(registry.id_of<script_component>() == first.value());
        const ecs::component_info &info = registry.info(first.value());
        CHECK(info.size == 4);
        CHECK(info.alignment == 4);
        CHECK_FALSE(info.is_trivial());
        CHECK_FALSE(info.is_tag());
        CHECK(info.ops.copy != nullptr);
        CHECK(info.ops.destruct != nullptr);
        CHECK(info.ops.relocate == nullptr);
        CHECK(info.ops.constructDefault == nullptr);
        const ecs::component_ops ops = script_component_ops();
        CHECK(ops.copy == info.ops.copy);
        const ecs::component_serializer serializer = script_component_serializer();
        CHECK(serializer.writeJson != nullptr);
        CHECK(serializer.readJson != nullptr);
        CHECK(serializer.remapEntities != nullptr);
    }

    TEST_CASE("script_component - copying an entity deep copies the script set through the runtime") {
        scripting_fixture fixture = {};
        const ecs::entity source = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(source, 77, json(R"({"hp": 9})").root());
        REQUIRE(id.is_valid());

        const std::array<ecs::entity, 1> captured = {source};
        core::result<ecs::entity_archive> archive = ecs::entity_archive::capture(fixture.world, captured);
        CONJURE_REQUIRE_OK(archive);
        const std::shared_ptr<const ecs::entity_archive> shared =
          std::make_shared<const ecs::entity_archive>(std::move(archive).value());
        const core::result<ecs::entity> copy = fixture.world.instantiate(shared);
        CONJURE_REQUIRE_OK(copy);

        const script_component *original = std::as_const(fixture.world).get<script_component>(source);
        const script_component *duplicate = std::as_const(fixture.world).get<script_component>(copy.value());
        REQUIRE(original != nullptr);
        REQUIRE(duplicate != nullptr);
        CHECK(original->setHandle != duplicate->setHandle);
        const script_set *copiedSet = fixture.runtime.store().find(duplicate->setHandle);
        REQUIRE(copiedSet != nullptr);
        CHECK(copiedSet->owner == copy.value());
        REQUIRE(copiedSet->entries.size() == 1);
        CHECK(copiedSet->entries[0].classHash == 77);
        CHECK(copiedSet->entries[0].fields.root()["hp"].as_i64() == 9);
        CHECK(fixture.runtime.store().size() == 3);     // source, archive copy, instantiated copy

        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(copy.value()));
        CHECK(fixture.runtime.store().find(duplicate->setHandle) == nullptr);
        CHECK(fixture.runtime.store().find(original->setHandle) != nullptr);
    }

    TEST_CASE("script_component - hooks without a runtime drop scripts instead of crashing") {
        log_capture logs = {};
        REQUIRE(script_runtime::active() == nullptr);
        script_component source = {};
        source.setHandle = script_store::make_handle(1, 1);
        script_component target = {};
        script_component_ops().copy(&target, &source, 1);
        CHECK(target.setHandle == INVALID_SET_HANDLE);
        CHECK(logs.count(core::log_level::WARN) == 1);
        script_component_ops().destruct(&source, 1);
        CHECK(source.setHandle == INVALID_SET_HANDLE);

        core::json::writer out = {};
        CONJURE_CHECK_OK(script_component_serializer().writeJson(&source, out));
        CHECK(out.finish() == R"({"scripts":[]})");
        const core::json::document in = json(R"({"scripts":[]})");
        CONJURE_CHECK_ERROR(
          script_component_serializer().readJson(&target, in.root()),
          core::error_code::INVALID_STATE
        );
        script_component_serializer().remapEntities(&target, ecs::entity_remap());
    }

    TEST_CASE("script_component - writing a set without a runtime fails instead of dropping its scripts") {
        REQUIRE(script_runtime::active() == nullptr);
        script_component source = {};
        source.setHandle = script_store::make_handle(1, 1);
        core::json::writer out = {};
        CONJURE_CHECK_ERROR(script_component_serializer().writeJson(&source, out), core::error_code::INVALID_STATE);

        // A component without a set has nothing to lose: it still writes the empty form.
        const script_component empty = {};
        core::json::writer emptyOut = {};
        CONJURE_CHECK_OK(script_component_serializer().writeJson(&empty, emptyOut));
        CHECK(emptyOut.finish() == R"({"scripts":[]})");
    }

    TEST_CASE("script_component - the serializer writes and reads the script set") {
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.Linked", {}, {
          make_field("target", script_field_kind::ENTITY, "0"),
          make_field("hp", script_field_kind::INT32, "1"),
        })});
        const ecs::entity owner = fixture.create_entity();
        const ecs::entity friendEntity = fixture.create_entity();
        core::json::document fields = core::json::document::object();
        fields.set("target", friendEntity.to_u64());
        fields.set("hp", 5);
        const script_entry_id id = fixture.runtime.add_script(owner, hash_of("Game.Linked"), fields.root());
        REQUIRE(id.is_valid());

        const script_component *component = std::as_const(fixture.world).get<script_component>(owner);
        REQUIRE(component != nullptr);
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(script_component_serializer().writeJson(component, out));
        const core::json::document written = json(out.finish());
        REQUIRE(written.root()["scripts"].size() == 1);
        CHECK(written.root()["scripts"][0]["class"].as_string() == "Game.Linked");
        CHECK(written.root()["scripts"][0]["hash"].as_u64() == hash_of("Game.Linked"));
        CHECK(written.root()["scripts"][0]["fields"]["hp"].as_i64() == 5);

        // Reading into a zero handle creates a new set; the owner is adopted by the ADDED observer later.
        script_component fresh = {};
        CONJURE_REQUIRE_OK(script_component_serializer().readJson(&fresh, written.root()));
        REQUIRE(fresh.setHandle != INVALID_SET_HANDLE);
        const script_set *readSet = fixture.runtime.store().find(fresh.setHandle);
        REQUIRE(readSet != nullptr);
        CHECK(readSet->owner.is_null());
        REQUIRE(readSet->entries.size() == 1);
        CHECK(readSet->entries[0].classHash == hash_of("Game.Linked"));
        CHECK(entity_from_json(readSet->entries[0].fields.root()["target"]) == friendEntity);

        // Remapping: a real target that is a remap target becomes archive-local, and back again.
        ecs::entity_remap map(std::vector<ecs::entity>{friendEntity});
        script_component_serializer().remapEntities(&fresh, map);
        CHECK(entity_from_json(readSet->entries[0].fields.root()["target"]) == ecs::entity::archive_local(0));
        script_component_serializer().remapEntities(&fresh, map);
        CHECK(entity_from_json(readSet->entries[0].fields.root()["target"]) == friendEntity);

        // Malformed input is refused and the set untouched.
        const core::json::document bad = json(R"({"scripts":[{"fields":{}}]})");
        CONJURE_CHECK_ERROR(
          script_component_serializer().readJson(&fresh, bad.root()),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(readSet->entries.size() == 1);
        const core::json::document notObject = json(R"([1])");
        CONJURE_CHECK_ERROR(
          script_component_serializer().readJson(&fresh, notObject.root()),
          core::error_code::INVALID_ARGUMENT
        );
        script_component_ops().destruct(&fresh, 1);
        CHECK(fixture.runtime.store().find(fresh.setHandle) == nullptr);
    }

    TEST_CASE("script_component - component_to_json and component_from_json go through the registered serializer") {
        // register_script_component hands script_component_serializer() to the registry through
        // component_type_desc::serializer (R29), so the live-component JSON path (inspector, AI
        // protocol, R40) writes and reads the script set instead of the bare handle.
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.Saved", {}, {make_field("hp", script_field_kind::INT32, "1")})});
        const ecs::entity owner = fixture.create_entity();
        const core::json::document fields = json(R"({"hp": 4})");
        const script_entry_id id = fixture.runtime.add_script(owner, hash_of("Game.Saved"), fields.root());
        REQUIRE(id.is_valid());
        const ecs::component_id scriptId = fixture.registry.id_of<script_component>();
        const ecs::component_info &info = fixture.registry.info(scriptId);
        CHECK(info.reflection == nullptr);
        CHECK(info.serializer.writeJson == script_component_serializer().writeJson);
        CHECK(info.serializer.readJson == script_component_serializer().readJson);
        CHECK(info.serializer.remapEntities == script_component_serializer().remapEntities);

        core::json::writer out = {};
        CONJURE_REQUIRE_OK(ecs::component_to_json(fixture.world, owner, scriptId, out));
        const core::json::document written = json(out.finish());
        REQUIRE(written.root()["scripts"].size() == 1);
        CHECK(written.root()["scripts"][0]["fields"]["hp"].as_i64() == 4);

        // Adding the component first lets the ADDED observer create the set with its owner; reading
        // the JSON then replaces that set's entries in place, so the owner survives the read.
        const ecs::entity restored = fixture.create_entity();
        CONJURE_REQUIRE_OK(fixture.world.add<script_component>(restored));
        CONJURE_REQUIRE_OK(ecs::component_from_json(fixture.world, restored, scriptId, written.root()));
        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(restored);
        REQUIRE(scripts.size() == 1);
        CHECK(fixture.runtime.entry(scripts[0])->classHash == hash_of("Game.Saved"));
        CHECK(fixture.runtime.entry(scripts[0])->fields.root()["hp"].as_i64() == 4);
        const script_set *restoredSet = fixture.runtime.store().find(scripts[0].setHandle);
        REQUIRE(restoredSet != nullptr);
        CHECK(restoredSet->owner == restored);
        CHECK(restoredSet->owner != owner);
        CHECK(scripts[0].setHandle != id.setHandle);

        // The adopted set is live in PLAY mode like any other: one managed instance per entry.
        fixture.runtime.set_mode(script_mode::PLAY);
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.entry(scripts[0])->is_live());
        CHECK(fixture.runtime.entry(id)->is_live());
        CHECK(fixture.null_host().live_slot_count() == 2);
        fixture.runtime.set_mode(script_mode::EDIT);
    }

    TEST_CASE("script_component - an entity_archive round trip through JSON restores class, fields and owner") {
        // capture -> write_archive_json -> read_archive_json -> instantiate: the scene file and prefab
        // path (R29), with the set written as JSON and rebuilt by readJson on the way back in.
        scripting_fixture fixture = {};
        fixture.load_fake_classes({make_class("Game.Saved", {}, {
          make_field("hp", script_field_kind::INT32, "1"),
          make_field("buddy", script_field_kind::ENTITY, "0"),
        })});
        const ecs::entity source = fixture.create_entity();
        const ecs::entity buddy = fixture.create_entity();    // captured with the source: must follow the copy
        core::json::document fields = core::json::document::object();
        fields.set("hp", 4);
        fields.set("buddy", buddy.to_u64());
        const script_entry_id id = fixture.runtime.add_script(source, hash_of("Game.Saved"), fields.root());
        REQUIRE(id.is_valid());

        const std::array<ecs::entity, 2> captured = {source, buddy};
        core::result<ecs::entity_archive> archive = ecs::entity_archive::capture(fixture.world, captured);
        CONJURE_REQUIRE_OK(archive);
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(ecs::write_archive_json(archive.value(), out));
        const std::string text = out.finish();
        const std::string_view expected =
          "\"conjure.scripting.script_component\":{\"scripts\":[{\"class\":\"Game.Saved\"";
        CHECK(text.find(expected) != std::string::npos);
        CHECK(text.find("\"hp\":4") != std::string::npos);
        CHECK(text.find("setHandle") == std::string::npos);
        // Review 3 regression: capture rewrites the script's entity field to the archive-local index
        // through the serializer hook, exactly as reflected fields are rewritten.
        CHECK(text.find(std::format("\"buddy\":{}", ecs::entity::archive_local(1).to_u64())) != std::string::npos);
        CHECK(text.find(std::format("\"buddy\":{}", buddy.to_u64())) == std::string::npos);

        core::result<ecs::entity_archive> parsed = ecs::read_archive_json(json(text).root(), fixture.registry);
        CONJURE_REQUIRE_OK(parsed);
        CONJURE_REQUIRE_OK(parsed.value().validate());
        CHECK(parsed.value().entity_count() == 2);
        const std::shared_ptr<const ecs::entity_archive> shared =
          std::make_shared<const ecs::entity_archive>(std::move(parsed).value());
        std::vector<ecs::entity> roots(1);
        std::vector<ecs::entity> created = {};
        CONJURE_REQUIRE_OK(fixture.world.instantiate_batch(shared, roots, nullptr, &created));
        REQUIRE(created.size() == 2);
        const ecs::entity copy = created[0];
        CHECK(copy != source);

        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(copy);
        REQUIRE(scripts.size() == 1);
        CHECK(scripts[0].setHandle != id.setHandle);
        const script_entry *entry = fixture.runtime.entry(scripts[0]);
        REQUIRE(entry != nullptr);
        CHECK(entry->classHash == hash_of("Game.Saved"));
        CHECK(entry->fields.root()["hp"].as_i64() == 4);
        // The copy's script points at the copied buddy, not the authoring one.
        CHECK(entity_from_json(entry->fields.root()["buddy"]) == created[1]);
        CHECK(created[1] != buddy);
        CHECK_FALSE(entry->has_flag(script_entry_flags::CLASS_MISSING));
        const script_set *copiedSet = fixture.runtime.store().find(scripts[0].setHandle);
        REQUIRE(copiedSet != nullptr);
        CHECK(copiedSet->owner == copy);
        CHECK(copiedSet->owner != source);
        // The source is untouched by the round trip; each archive owns a copy of the set (ops.copy at
        // capture, readJson at parse), so neither aliases a live one.
        CHECK(fixture.runtime.entry(id)->fields.root()["hp"].as_i64() == 4);
        CHECK(fixture.runtime.store().find(id.setHandle)->owner == source);
        CHECK(fixture.runtime.store().size() == 4);     // source, captured archive, parsed archive, instance
    }
}
