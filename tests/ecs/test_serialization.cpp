#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/json.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/log.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    core::json::document parse(std::string_view text) {
        core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE_MESSAGE(parsed.has_value(), text);
        return std::move(parsed).value();
    }

    std::string archive_text(const entity_archive &archive) {
        core::json::writer out = {};
        const core::result<void> written = write_archive_json(archive, out);
        REQUIRE(written.has_value());
        return out.finish();
    }

    std::string canonical(std::string_view text) {
        return parse(text).to_canonical_string();
    }
}

TEST_SUITE("ecs/serialization") {

    TEST_CASE("serialization - round trip trivial nested entity and math fields") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> outsider = world.create_entity();
        const core::result<entity> a = world.create_entity(fixture.mask<position, nested_component, name_component>());
        const core::result<entity> b = world.create_entity(fixture.mask<link, health, player_tag>());
        CONJURE_REQUIRE_OK(outsider);
        CONJURE_REQUIRE_OK(a);
        CONJURE_REQUIRE_OK(b);
        CONJURE_REQUIRE_OK(world.set(a.value(), position{1.5f, -2.0f, 3.25f}));
        nested_component nested = {};
        nested.part.weight = 0.75f;
        nested.part.owner = b.value();
        nested.refs = {a.value(), outsider.value()};
        nested.direction = math::vec3(1.0f, 0.0f, 0.0f);
        nested.rotation = math::quat(0.0f, 0.0f, 1.0f, 0.0f);
        nested.label = core::string_id("test.label");
        CONJURE_REQUIRE_OK(world.set(a.value(), nested));
        CONJURE_REQUIRE_OK(world.set(a.value(), name_component{"alpha \"quoted\""}));
        CONJURE_REQUIRE_OK(world.set(b.value(), link{a.value(), b.value()}));
        CONJURE_REQUIRE_OK(world.set(b.value(), health{42, 99}));

        const std::array<entity, 2> set = {a.value(), b.value()};
        core::result<entity_archive> captured = entity_archive::capture(world, set);
        CONJURE_REQUIRE_OK(captured);
        const std::string text = archive_text(captured.value());
        const core::json::document document = parse(text);
        const core::json::value entities = document.root()["entities"];
        REQUIRE(entities.size() == 2);
        const core::json::value first = entities[0]["components"];
        CHECK(first["test.position"]["x"].as_f64() == 1.5);
        CHECK(first["test.nested"]["part"]["owner"].as_i64() == 1);
        CHECK(first["test.nested"]["refs"][0].as_i64() == 0);
        CHECK(first["test.nested"]["refs"][1].is_null());
        CHECK(first["test.nested"]["direction"][0].as_f64() == 1.0);
        CHECK(first["test.nested"]["rotation"][1].as_f64() == 1.0);
        CHECK(first["test.nested"]["rotation"][3].as_f64() == 0.0);
        CHECK(first["test.nested"]["label"].as_string() == "test.label");
        CHECK(first["test.name"]["value"].as_string() == "alpha \"quoted\"");
        const core::json::value second = entities[1]["components"];
        CHECK(second["test.link"]["target"].as_i64() == 0);
        CHECK(second["test.link"]["other"].as_i64() == 1);
        CHECK(second["test.health"]["value"].as_i64() == 42);
        CHECK(second["test.player_tag"].is_object());
        CHECK(second["test.player_tag"].size() == 0);

        // Read back and instantiate: values and references survive.
        core::result<entity_archive> loaded = read_archive_json(document.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(archive_text(loaded.value()) == text);
        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(loaded).value());
        const core::result<entity> root = world.instantiate(archive);
        CONJURE_REQUIRE_OK(root);
        const entity newA = root.value();
        const nested_component *newNested = world.get<nested_component>(newA);
        REQUIRE(newNested != nullptr);
        const entity newB = newNested->part.owner;
        REQUIRE(world.is_alive(newB));
        CHECK(newB != b.value());
        CHECK(newNested->refs[0] == newA);
        CHECK(newNested->refs[1] == NULL_ENTITY);
        CHECK(newNested->part.weight == 0.75f);
        CHECK(newNested->direction == math::vec3(1.0f, 0.0f, 0.0f));
        CHECK(newNested->rotation.y == 1.0f);
        CHECK(newNested->rotation.w == 0.0f);
        CHECK(newNested->label == core::string_id("test.label"));
        CHECK(*world.get<position>(newA) == position{1.5f, -2.0f, 3.25f});
        CHECK(world.get<name_component>(newA)->value == "alpha \"quoted\"");
        CHECK(world.get<link>(newB)->target == newA);
        CHECK(world.get<link>(newB)->other == newB);
        CHECK(world.get<health>(newB)->value == 42);
        CHECK(world.has<player_tag>(newB));
    }

    TEST_CASE("serialization - TRANSIENT skipped") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position, transient_component>());
        CONJURE_REQUIRE_OK(created);
        CONJURE_REQUIRE_OK(world.set(created.value(), transient_component{9}));
        core::result<entity_archive> captured =
          entity_archive::capture(world, std::span<const entity>(&created.value(), 1));
        CONJURE_REQUIRE_OK(captured);
        const std::string text = archive_text(captured.value());
        CHECK(text.find("test.transient") == std::string::npos);
        CHECK(text.find("test.position") != std::string::npos);

        // Transient data present in a file is ignored on load, and written as an empty object live.
        const core::json::document withTransient =
          parse(R"({"entities":[{"components":{"test.transient":{"frames":5},"test.position":{}}}]})");
        const core::result<entity_archive> loaded = read_archive_json(withTransient.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().entries()[0].mask == fixture.mask<position>());
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(component_to_json(world, created.value(), fixture.id<transient_component>(), out));
        CHECK(out.finish() == "{}");
    }

    TEST_CASE("serialization - unknown name preserved and rewritten verbatim") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const std::string input =
          R"({"entities":[{"components":{"test.position":{"x":1.0,"y":2.0,"z":3.0},)"
          R"("com.plugin.spinner":{"speed":2.5,"axis":[0,1,0],"name":"wheel"}}},)"
          R"({"components":{"com.plugin.other":true,"test.player_tag":{}}}]})";
        testing::log_capture logs = {};
        const core::json::document document = parse(input);
        core::result<entity_archive> loaded = read_archive_json(document.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(logs.count(core::log_level::INFO) == 2);
        const entity_archive &archive = loaded.value();
        REQUIRE(archive.entity_count() == 2);
        CHECK(archive.entries()[0].mask.test(component_id{0}));
        CHECK(archive.entries()[0].mask.test(fixture.id<position>()));
        CHECK(archive.entries()[1].mask.test(component_id{0}));
        CHECK(archive.entries()[1].mask.test(fixture.id<player_tag>()));
        const unresolved_components &preserved =
          *reinterpret_cast<const unresolved_components *>(archive.component_bytes(0, component_id{0}).data());
        REQUIRE(preserved.entries.size() == 1);
        CHECK(preserved.entries[0].name == "com.plugin.spinner");
        CHECK(canonical(preserved.entries[0].json) == canonical(R"({"speed":2.5,"axis":[0,1,0],"name":"wheel"})"));

        // Writing emits the unknown members under their original names with their original JSON.
        const std::string written = archive_text(archive);
        CHECK(canonical(written) == canonical(input));

        // Instantiating carries the hidden component along; capturing and writing again keeps it.
        const std::shared_ptr<const entity_archive> shared =
          std::make_shared<const entity_archive>(std::move(loaded).value());
        std::vector<entity> roots(1);
        CONJURE_REQUIRE_OK(world.instantiate_batch(shared, roots));
        CHECK(world.has_raw(roots[0], component_id{0}));
        const entity second = roots[0].index == 0 ? entity{1, 1} : entity{0, 1};
        const std::array<entity, 2> both = {roots[0], second};
        const core::result<entity_archive> recaptured = entity_archive::capture(world, both);
        CONJURE_REQUIRE_OK(recaptured);
        CHECK(canonical(archive_text(recaptured.value())) == canonical(input));

        // The live inspector path serialises the preserved data as an object of raw members.
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(component_to_json(world, roots[0], component_id{0}, out));
        const std::string expectedLive = R"({"com.plugin.spinner":{"speed":2.5,"axis":[0,1,0],"name":"wheel"}})";
        CHECK(canonical(out.finish()) == canonical(expectedLive));
    }

    TEST_CASE("serialization - a preserved component shadowed by a registered one round trips") {
        // Data read while 'test.late' was unknown is preserved. Once the type is registered and the entity
        // holds it, writing must emit the name once (the live value): a second key would make the file
        // fail to load with "component listed twice".
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::json::document input = parse(R"({"entities":[{"components":{"test.late":{"value":1}}}]})");
        core::result<entity_archive> loaded = read_archive_json(input.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        const std::shared_ptr<const entity_archive> shared =
          std::make_shared<const entity_archive>(std::move(loaded).value());
        const core::result<entity> instantiated = world.instantiate(shared);
        CONJURE_REQUIRE_OK(instantiated);
        const entity id = instantiated.value();
        REQUIRE(world.has_raw(id, component_id{0}));

        const std::array<dynamic_field_desc, 1> fields = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<i32>(), 0},
        };
        dynamic_component_desc late = {};
        late.name = core::string_id("test.late");
        late.fields = fields;
        late.size = sizeof(i32);
        late.alignment = alignof(i32);
        const core::result<component_id> lateId = fixture.registry.register_dynamic(late);
        CONJURE_REQUIRE_OK(lateId);
        CONJURE_REQUIRE_OK(world.add_raw(id, lateId.value()));
        const core::json::document seven = parse(R"({"value":7})");
        CONJURE_REQUIRE_OK(component_from_json(world, id, lateId.value(), seven.root()));

        const core::result<entity_archive> captured = entity_archive::capture(world, std::span<const entity>(&id, 1));
        CONJURE_REQUIRE_OK(captured);
        const std::string text = archive_text(captured.value());
        const usize first = text.find("\"test.late\"");
        REQUIRE(first != std::string::npos);
        CHECK(text.find("\"test.late\"", first + 1) == std::string::npos);
        const core::json::document written = parse(text);
        CHECK(written.root()["entities"][0]["components"]["test.late"]["value"].as_i64() == 7);

        core::result<entity_archive> reloaded = read_archive_json(written.root(), fixture.registry);
        CONJURE_REQUIRE_OK(reloaded);
        const std::shared_ptr<const entity_archive> reshared =
          std::make_shared<const entity_archive>(std::move(reloaded).value());
        const core::result<entity> copy = world.instantiate(reshared);
        CONJURE_REQUIRE_OK(copy);
        const i32 *value = static_cast<const i32 *>(world.get_raw(copy.value(), lateId.value()));
        REQUIRE(value != nullptr);
        CHECK(*value == 7);
        CHECK_FALSE(world.has_raw(copy.value(), component_id{0}));
    }

    TEST_CASE("regression - code sweep F606 component keys are written in name order whatever the registration order") {
        // Component ids follow registration order, which differs between builds (a feature compiled out,
        // another module load order). write_archive_json used to write an entity's keys in id order, so
        // two builds saved the same scene as different text. The keys are sorted by name now.
        const auto capture_text = [](const component_registry &registry) -> std::string {
            world source(registry);
            component_mask mask = {};
            mask.set(registry.id_of<position>());
            mask.set(registry.id_of<health>());
            mask.set(registry.id_of<velocity>());
            mask.set(registry.id_of<player_tag>());
            const core::result<entity> created = source.create_entity(mask);
            CONJURE_REQUIRE_OK(created);
            CONJURE_REQUIRE_OK(source.set(created.value(), position{1.0f, 2.0f, 3.0f}));
            CONJURE_REQUIRE_OK(source.set(created.value(), health{7, 9}));
            CONJURE_REQUIRE_OK(source.set(created.value(), velocity{0.5f, 0.0f, -0.5f}));
            const core::result<entity_archive> captured =
              entity_archive::capture(source, std::span<const entity>(&created.value(), 1));
            CONJURE_REQUIRE_OK(captured);
            return archive_text(captured.value());
        };

        component_registry forward = {};
        CONJURE_REQUIRE_OK(forward.register_component<position>());
        CONJURE_REQUIRE_OK(forward.register_component<health>());
        CONJURE_REQUIRE_OK(forward.register_component<velocity>());
        CONJURE_REQUIRE_OK(forward.register_component<player_tag>());
        component_registry backward = {};
        CONJURE_REQUIRE_OK(backward.register_component<player_tag>());
        CONJURE_REQUIRE_OK(backward.register_component<velocity>());
        CONJURE_REQUIRE_OK(backward.register_component<health>());
        CONJURE_REQUIRE_OK(backward.register_component<position>());
        CHECK(forward.id_of<position>().value < forward.id_of<health>().value);
        CHECK(backward.id_of<position>().value > backward.id_of<health>().value);

        const std::string forwardText = capture_text(forward);
        const std::string backwardText = capture_text(backward);
        CHECK(forwardText == backwardText);
        const std::string expected =
          R"({"entities":[{"components":{"test.health":{"value":7,"maximum":9},"test.player_tag":{},)"
          R"("test.position":{"x":1.0,"y":2.0,"z":3.0},"test.velocity":{"x":0.5,"y":0.0,"z":-0.5}}}]})";
        CHECK(forwardText == expected);

        // The text one registry wrote loads into the other and is written back unchanged.
        const core::json::document document = parse(forwardText);
        const core::result<entity_archive> reloaded = read_archive_json(document.root(), backward);
        CONJURE_REQUIRE_OK(reloaded);
        CHECK(archive_text(reloaded.value()) == forwardText);
    }

    TEST_CASE("serialization - preserved unknown components are written in the same name order") {
        // Keys compare as UTF-8 bytes, the order of core::json's canonical form: upper case before lower
        // case, a name before the longer names it prefixes, and bytes above 0x7F last (not first, as a
        // signed comparison would have it).
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const std::string accented = "\xC3\xA9.accent";
        const std::string input =
          R"({"entities":[{"components":{"zeta.unknown":{"b":1,"a":2},"test.position":{"x":1.0,"y":2.0,"z":3.0},)"
          R"("test.health.extra":true,")" + accented + R"(":[1,2],"com.plugin.spinner":{"speed":2.5},)"
          R"("test.health":{"value":3,"maximum":4},"Zed.upper":null}},{"components":{}},)"
          R"({"components":{"test.player_tag":{},"only.unknown":7}}]})";
        const std::string expected =
          R"({"entities":[{"components":{"Zed.upper":null,"com.plugin.spinner":{"speed":2.5},)"
          R"("test.health":{"value":3,"maximum":4},"test.health.extra":true,)"
          R"("test.position":{"x":1.0,"y":2.0,"z":3.0},"zeta.unknown":{"b":1,"a":2},")" + accented + R"(":[1,2]}},)"
          R"({"components":{}},{"components":{"only.unknown":7,"test.player_tag":{}}}]})";

        testing::log_capture logs = {};
        const core::json::document document = parse(input);
        core::result<entity_archive> loaded = read_archive_json(document.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(logs.count(core::log_level::INFO) == 6);
        const std::string written = archive_text(loaded.value());
        CHECK(written == expected);

        // The sorted text is a fixed point of read and write.
        const core::json::document sorted = parse(written);
        const core::result<entity_archive> again = read_archive_json(sorted.root(), fixture.registry);
        CONJURE_REQUIRE_OK(again);
        CHECK(archive_text(again.value()) == expected);

        // A capture lists the hidden unresolved_components component (id 0) first; its entries are still
        // merged into the name order.
        const std::shared_ptr<const entity_archive> shared =
          std::make_shared<const entity_archive>(std::move(loaded).value());
        std::vector<entity> roots(1);
        std::vector<entity> created = {};
        CONJURE_REQUIRE_OK(world.instantiate_batch(shared, roots, nullptr, &created));
        REQUIRE(created.size() == 3);
        const core::result<entity_archive> recaptured = entity_archive::capture(world, created);
        CONJURE_REQUIRE_OK(recaptured);
        CHECK(archive_text(recaptured.value()) == expected);
    }

    TEST_CASE("serialization - an unknown component listed twice keeps its file order") {
        // A name no type resolves is not checked for duplicates on load, so both values are preserved.
        // They sort to the same place and keep the order of the file, which makes the rewrite stable.
        ecs_fixture fixture = {};
        const std::string input =
          R"({"entities":[{"components":{"dup.unknown":2,"test.position":{"x":0.0,"y":0.0,"z":0.0},)"
          R"("dup.unknown":1,"a.first":{}}}]})";
        const std::string expected =
          R"({"entities":[{"components":{"a.first":{},"dup.unknown":2,"dup.unknown":1,)"
          R"("test.position":{"x":0.0,"y":0.0,"z":0.0}}}]})";
        const core::json::document document = parse(input);
        const core::result<entity_archive> loaded = read_archive_json(document.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        const std::string written = archive_text(loaded.value());
        CHECK(written == expected);
        const core::json::document rewritten = parse(written);
        const core::result<entity_archive> again = read_archive_json(rewritten.root(), fixture.registry);
        CONJURE_REQUIRE_OK(again);
        CHECK(archive_text(again.value()) == expected);
    }

    TEST_CASE("serialization - a failed archive write leaves partial output until reset") {
        // Neither failure path closes the scopes it opened (a caller nesting the archive in its own object
        // could not be balanced anyway): the writer holds partial output until it is reset.
        struct failing_component {
            u32 value = 0;
        };
        static constexpr auto writeHook = [](const void *, core::json::writer &) -> core::result<void> {
            return make_error(ecs_error::SERIALIZATION_FAILED, "this component refuses to serialise");
        };
        static constexpr auto readHook = [](void *, const core::json::value &) -> core::result<void> {
            return core::result<void>::success();
        };
        ecs_fixture fixture = {};
        world &world = fixture.world;
        component_type_desc desc = {};
        desc.name = "test.failing";
        desc.size = sizeof(failing_component);
        desc.alignment = alignof(failing_component);
        desc.flags = component_flags::TRIVIAL;
        desc.serializer.writeJson = writeHook;
        desc.serializer.readJson = readHook;
        const core::result<component_id> failing = fixture.registry.register_described(desc);
        CONJURE_REQUIRE_OK(failing);
        component_mask mask = fixture.mask<position>();
        mask.set(failing.value());
        const core::result<entity> refusing = world.create_entity(mask);
        CONJURE_REQUIRE_OK(refusing);
        const core::result<entity_archive> unwritable =
          entity_archive::capture(world, std::span<const entity>(&refusing.value(), 1));
        CONJURE_REQUIRE_OK(unwritable);

        core::json::writer out = {};
        CONJURE_CHECK_ERROR(write_archive_json(unwritable.value(), out), ecs_error::SERIALIZATION_FAILED);
        CHECK_FALSE(out.is_complete());
        CHECK(out.depth() > 0);

        // The unregistered-id path leaves partial output the same way.
        entity_archive unregistered(fixture.registry);
        archive_entry entry = {};
        entry.mask.set(component_id{900});
        archive_component component = {};
        component.id = component_id{900};
        entry.components.push_back(std::move(component));
        unregistered.push_entry(std::move(entry));
        out.reset();
        CONJURE_CHECK_ERROR(write_archive_json(unregistered, out), ecs_error::ARCHIVE_CORRUPT);
        CHECK_FALSE(out.is_complete());
        CHECK(out.depth() == 4);

        // After reset() the writer serves a valid archive.
        const core::result<entity> plain = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(plain);
        const core::result<entity_archive> writable =
          entity_archive::capture(world, std::span<const entity>(&plain.value(), 1));
        CONJURE_REQUIRE_OK(writable);
        out.reset();
        CONJURE_REQUIRE_OK(write_archive_json(writable.value(), out));
        CHECK(out.is_complete());
        const std::string text = out.finish();
        const core::json::document parsed = parse(text);
        CHECK(parsed.root()["entities"].size() == 1);
    }

    TEST_CASE("serialization - malformed json fails with field path") {
        ecs_fixture fixture = {};
        const core::json::document badScalar =
          parse(R"({"entities":[{"components":{}},{"components":{"test.position":{"x":"not a number"}}}]})");
        const core::result<entity_archive> scalar = read_archive_json(badScalar.root(), fixture.registry);
        CONJURE_CHECK_ERROR(scalar, ecs_error::SERIALIZATION_FAILED);
        CHECK(scalar.get_error().message().starts_with("1/test.position/x: "));

        const core::json::document badNested =
          parse(R"({"entities":[{"components":{"test.nested":{"part":{"owner":"nope"}}}}]})");
        const core::result<entity_archive> nested = read_archive_json(badNested.root(), fixture.registry);
        CONJURE_CHECK_ERROR(nested, ecs_error::SERIALIZATION_FAILED);
        CHECK(nested.get_error().message().starts_with("0/test.nested/part.owner: "));

        const core::json::document badArray =
          parse(R"({"entities":[{"components":{"test.nested":{"refs":[0,1,2]}}}]})");
        const core::result<entity_archive> array = read_archive_json(badArray.root(), fixture.registry);
        CONJURE_CHECK_ERROR(array, ecs_error::SERIALIZATION_FAILED);
        CHECK(array.get_error().message().starts_with("0/test.nested/refs: "));

        const core::json::document badElement =
          parse(R"({"entities":[{"components":{"test.nested":{"refs":[0,{"external":"x"}]}}}]})");
        const core::result<entity_archive> element = read_archive_json(badElement.root(), fixture.registry);
        CONJURE_CHECK_ERROR(element, ecs_error::SERIALIZATION_FAILED);
        CHECK(element.get_error().message().starts_with("0/test.nested/refs[1]: "));

        const core::json::document notObject = parse(R"({"entities":[{"components":{"test.position":5}}]})");
        CONJURE_CHECK_ERROR(read_archive_json(notObject.root(), fixture.registry), ecs_error::SERIALIZATION_FAILED);
        const core::json::document duplicate =
          parse(R"({"entities":[{"components":{"test.position":{},"test.position":{}}}]})");
        CONJURE_CHECK_ERROR(read_archive_json(duplicate.root(), fixture.registry), ecs_error::SERIALIZATION_FAILED);
        const core::json::document noEntities = parse(R"({"things":[]})");
        CONJURE_CHECK_ERROR(read_archive_json(noEntities.root(), fixture.registry), ecs_error::SERIALIZATION_FAILED);
        const core::json::document notRoot = parse("[1,2]");
        CONJURE_CHECK_ERROR(read_archive_json(notRoot.root(), fixture.registry), ecs_error::SERIALIZATION_FAILED);
        const core::json::document badEntity = parse(R"({"entities":[7]})");
        CONJURE_CHECK_ERROR(read_archive_json(badEntity.root(), fixture.registry), ecs_error::SERIALIZATION_FAILED);
        const core::json::document badComponents = parse(R"({"entities":[{"components":[]}]})");
        CONJURE_CHECK_ERROR(read_archive_json(badComponents.root(), fixture.registry), ecs_error::SERIALIZATION_FAILED);

        // A local reference past the end of the archive is a corrupt archive, not a parse error.
        const core::json::document outOfRange = parse(R"({"entities":[{"components":{"test.link":{"target":4}}}]})");
        CONJURE_CHECK_ERROR(read_archive_json(outOfRange.root(), fixture.registry), ecs_error::ARCHIVE_CORRUPT);

        // Missing members keep defaults; an entity without components is fine.
        const core::json::document lenient = parse(R"({"entities":[{"components":{"test.health":{"value":3}}},{}]})");
        const core::result<entity_archive> loaded = read_archive_json(lenient.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(loaded.value().entity_count() == 2);
        health loadedHealth = {};
        std::memcpy(&loadedHealth, loaded.value().component_bytes(0, fixture.id<health>()).data(), sizeof(health));
        CHECK(loadedHealth.value == 3);
        CHECK(loadedHealth.maximum == 100);
        CHECK(loaded.value().entries()[1].mask.none());
    }

    TEST_CASE("serialization - external references round trip") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> outsider = world.create_entity();
        const core::result<entity> linked = world.create_entity(fixture.mask<link>());
        CONJURE_REQUIRE_OK(outsider);
        CONJURE_REQUIRE_OK(linked);
        CONJURE_REQUIRE_OK(world.set(linked.value(), link{outsider.value(), NULL_ENTITY}));
        archive_options options = {};
        options.keepExternalReferences = true;
        const core::result<entity_archive> captured =
          entity_archive::capture(world, std::span<const entity>(&linked.value(), 1), options);
        CONJURE_REQUIRE_OK(captured);
        const std::string text = archive_text(captured.value());
        const core::json::document document = parse(text);
        const core::json::value target = document.root()["entities"][0]["components"]["test.link"]["target"];
        CHECK(target.is_object());
        const std::string expectedExternal = std::format("{}:{}", outsider.value().index, outsider.value().generation);
        CHECK(target["external"].as_string() == expectedExternal);

        const core::result<entity_archive> loaded = read_archive_json(document.root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);
        link loadedLink = {};
        std::memcpy(&loadedLink, loaded.value().component_bytes(0, fixture.id<link>()).data(), sizeof(link));
        CHECK(loadedLink.target == outsider.value());
        CHECK(loadedLink.other == NULL_ENTITY);
    }

    TEST_CASE("serialization - component_to_json and component_from_json on live entities") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position, link, player_tag>());
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();
        CONJURE_REQUIRE_OK(world.set(id, position{1.0f, 2.0f, 3.0f}));
        CONJURE_REQUIRE_OK(world.set(id, link{id, NULL_ENTITY}));

        core::json::writer out = {};
        CONJURE_REQUIRE_OK(component_to_json(world, id, fixture.id<position>(), out));
        CHECK(out.finish() == R"({"x":1.0,"y":2.0,"z":3.0})");
        CONJURE_REQUIRE_OK(component_to_json(world, id, fixture.id<link>(), out));
        const std::string expectedLink =
          std::format(R"({{"target":{{"external":"{}:{}"}},"other":null}})", id.index, id.generation);
        CHECK(out.finish() == expectedLink);
        CONJURE_REQUIRE_OK(component_to_json(world, id, fixture.id<player_tag>(), out));
        CHECK(out.finish() == "{}");

        // Partial updates keep the other members and stamp the column.
        world.bump_version();
        const u64 since = world.version();
        world.bump_version();
        const core::json::document patch = parse(R"({"y":20.0})");
        CONJURE_REQUIRE_OK(component_from_json(world, id, fixture.id<position>(), patch.root()));
        CHECK(*world.get<position>(id) == position{1.0f, 20.0f, 3.0f});
        CHECK(world.view<const position>().changed<position>(since).entity_count() == 1);
        const core::json::document relink =
          parse(std::format(R"({{"other":{{"external":"{}:{}"}}}})", id.index, id.generation));
        CONJURE_REQUIRE_OK(component_from_json(world, id, fixture.id<link>(), relink.root()));
        CHECK(world.get<link>(id)->other == id);
        CHECK(world.get<link>(id)->target == id);
        CONJURE_REQUIRE_OK(component_from_json(world, id, fixture.id<player_tag>(), patch.root()));

        // Failure paths.
        const core::json::document bad = parse(R"({"x":"text"})");
        const core::result<void> failed = component_from_json(world, id, fixture.id<position>(), bad.root());
        CONJURE_CHECK_ERROR(failed, ecs_error::SERIALIZATION_FAILED);
        CHECK(failed.get_error().message().starts_with("x: "));
        CONJURE_CHECK_ERROR(component_to_json(world, id, fixture.id<health>(), out), ecs_error::COMPONENT_NOT_PRESENT);
        out.reset();
        CONJURE_CHECK_ERROR(component_to_json(world, id, component_id{900}, out), ecs_error::UNKNOWN_COMPONENT);
        const entity dead{50, 50};
        CONJURE_CHECK_ERROR(component_to_json(world, dead, fixture.id<position>(), out), ecs_error::ENTITY_NOT_ALIVE);
        const component_id healthId = fixture.id<health>();
        CONJURE_CHECK_ERROR(component_from_json(world, id, healthId, patch.root()), ecs_error::COMPONENT_NOT_PRESENT);
        const component_id unknownId{900};
        CONJURE_CHECK_ERROR(component_from_json(world, id, unknownId, patch.root()), ecs_error::UNKNOWN_COMPONENT);
        const component_id positionId = fixture.id<position>();
        CONJURE_CHECK_ERROR(component_from_json(world, dead, positionId, patch.root()), ecs_error::ENTITY_NOT_ALIVE);
    }

    TEST_CASE("serialization - make_default_serializer and unsupported types") {
        ecs_fixture fixture = {};
        const component_serializer serializer = make_default_serializer<health>();
        REQUIRE(serializer.writeJson != nullptr);
        REQUIRE(serializer.readJson != nullptr);
        REQUIRE(serializer.remapEntities != nullptr);
        health sample{5, 6};
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(serializer.writeJson(&sample, out));
        CHECK(out.finish() == R"({"value":5,"maximum":6})");
        const core::json::document patch = parse(R"({"maximum":60})");
        CONJURE_REQUIRE_OK(serializer.readJson(&sample, patch.root()));
        CHECK(sample.maximum == 60);
        CHECK(sample.value == 5);

        // The remap hook rewrites entity fields of a reflected type.
        const component_serializer linkSerializer = make_default_serializer<link>();
        link remapped{entity::archive_local(1), entity{3, 3}};
        entity_remap map = {};
        map.set(1, entity{10, 1});
        linkSerializer.remapEntities(&remapped, map);
        CHECK(remapped.target == entity{10, 1});
        CHECK(remapped.other == entity{3, 3});

        // A type without reflection fails cleanly through the hooks.
        const component_serializer unreflected = make_default_serializer<unreflected_component>();
        unreflected_component value = {};
        CHECK_FALSE(unreflected.writeJson(&value, out).has_value());
        out.reset();
        CHECK_FALSE(unreflected.readJson(&value, patch.root()).has_value());
        unreflected.remapEntities(&value, map);

        // write/read_component_json on an info without hooks or reflection fail.
        component_info bare = {};
        bare.name = core::string_id("bare");
        bare.size = 4;
        CONJURE_CHECK_ERROR(write_component_json(bare, &value, out), ecs_error::SERIALIZATION_FAILED);
        out.reset();
        CONJURE_CHECK_ERROR(read_component_json(bare, &value, patch.root()), ecs_error::SERIALIZATION_FAILED);
    }

    TEST_CASE("serialization - asset references serialise as uuid strings") {
        // The ASSET_REF kind is owned by the assets module; its handle starts with a uuid. A dynamic
        // description cannot express it, so this exercises the reflection walker through a fake type.
        struct fake_asset_ref {
            core::uuid id = {};
            void *slot = nullptr;
        };
        core::reflect::field_info field = {};
        field.name = "clip";
        field.kind = core::reflect::field_kind::ASSET_REF;
        field.type = core::reflect::type_id::from_name("test.asset_ref");
        field.offset = 0;
        field.size = sizeof(fake_asset_ref);
        core::result<core::reflect::type_info> built =
          core::reflect::build_type_info(
            "test.with_asset",
            sizeof(fake_asset_ref),
            alignof(fake_asset_ref),
            std::span<const core::reflect::field_info>(&field, 1)
          );
        CONJURE_REQUIRE_OK(built);
        const core::reflect::type_info type = std::move(built).value();

        fake_asset_ref value = {};
        value.id = core::uuid(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
        value.slot = &value;
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(detail::write_reflected_component(type, &value, out));
        CHECK(out.finish() == R"({"clip":"01234567-89ab-cdef-fedc-ba9876543210"})");

        fake_asset_ref loaded = {};
        loaded.slot = &loaded;
        const core::json::document parsed = parse(R"({"clip":"01234567-89ab-cdef-fedc-ba9876543210"})");
        CONJURE_REQUIRE_OK(detail::read_reflected_component(type, &loaded, parsed.root()));
        CHECK(loaded.id == value.id);
        CHECK(loaded.slot == nullptr);

        const core::json::document nullClip = parse(R"({"clip":null})");
        CONJURE_REQUIRE_OK(detail::read_reflected_component(type, &loaded, nullClip.root()));
        CHECK(loaded.id.is_null());
        out.reset();
        CONJURE_REQUIRE_OK(detail::write_reflected_component(type, &loaded, out));
        CHECK(out.finish() == R"({"clip":null})");
        const core::json::document badClip = parse(R"({"clip":"not-a-uuid"})");
        CONJURE_CHECK_ERROR(
          detail::read_reflected_component(type, &loaded, badClip.root()),
          ecs_error::SERIALIZATION_FAILED
        );
        const core::json::document wrongType = parse(R"({"clip":7})");
        CONJURE_CHECK_ERROR(
          detail::read_reflected_component(type, &loaded, wrongType.root()),
          ecs_error::SERIALIZATION_FAILED
        );
    }
}
