#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    /// <summary>
    /// The C# struct a dynamic description stands for: { float speed; int hits; entity target; }.
    /// </summary>
    struct mover_image {
        f32 speed = 0.0f;
        i32 hits = 0;
        entity target = {};
    };

    // Namespace-scope ids are constant-initialised and never interned; intern() keeps the names visible.
    const std::array<dynamic_field_desc, 3> MOVER_FIELDS = {
        dynamic_field_desc{core::string_id::intern("speed"), core::reflect::type_id_of<f32>(), 0},
        dynamic_field_desc{core::string_id::intern("hits"), core::reflect::type_id_of<i32>(), 4},
        dynamic_field_desc{core::string_id::intern("target"), core::reflect::type_id_of<entity>(), 8},
    };

    dynamic_component_desc mover_desc(std::span<const std::byte> defaults = {}) {
        dynamic_component_desc desc = {};
        desc.name = core::string_id("script.mover");
        desc.fields = MOVER_FIELDS;
        desc.size = sizeof(mover_image);
        desc.alignment = 8;
        desc.defaultValue = defaults;
        return desc;
    }

    std::span<const std::byte> bytes_of(const mover_image &image) {
        return std::span<const std::byte>(reinterpret_cast<const std::byte *>(&image), sizeof(image));
    }

    mover_image read_image(const void *data) {
        mover_image image = {};
        std::memcpy(&image, data, sizeof(image));
        return image;
    }
}

TEST_SUITE("ecs/dynamic_components") {

    TEST_CASE("dynamic_components - register add by id bytes visible via column_data") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<component_id> registered = fixture.registry.register_dynamic(mover_desc());
        CONJURE_REQUIRE_OK(registered);
        const component_id mover = registered.value();

        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        mover_image image = {};
        image.speed = 2.5f;
        image.hits = 3;
        const core::result<void *> added = world.add_raw(created.value(), mover, bytes_of(image));
        CONJURE_REQUIRE_OK(added);
        CHECK(world.has_raw(created.value(), mover));
        CHECK(read_image(added.value()).speed == 2.5f);

        query_desc desc = {};
        desc.with.set(mover);
        query movers = world.create_query(desc);
        u32 seen = 0;
        movers.for_each_chunk([&](chunk_view &chunk) {
            CHECK(chunk.column_stride(mover) == sizeof(mover_image));
            void *column = chunk.column_data(mover, true);
            REQUIRE(column != nullptr);
            mover_image stored = read_image(column);
            CHECK(stored.hits == 3);
            stored.hits = 4;
            std::memcpy(column, &stored, sizeof(stored));
            ++seen;
        });
        CHECK(seen == 1);
        CHECK(read_image(world.get_raw(created.value(), mover, false)).hits == 4);
        CONJURE_REQUIRE_OK(world.remove_raw(created.value(), mover));
        CHECK_FALSE(world.has_raw(created.value(), mover));
    }

    TEST_CASE("dynamic_components - default value applied") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        mover_image defaults = {};
        defaults.speed = 1.0f;
        defaults.hits = 10;
        const core::result<component_id> registered = fixture.registry.register_dynamic(mover_desc(bytes_of(defaults)));
        CONJURE_REQUIRE_OK(registered);
        const component_id mover = registered.value();

        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        const core::result<void *> added = world.add_raw(created.value(), mover);
        CONJURE_REQUIRE_OK(added);
        CHECK(read_image(added.value()).speed == 1.0f);
        CHECK(read_image(added.value()).hits == 10);

        component_mask mask = {};
        mask.set(mover);
        std::vector<entity> batch(300);
        CONJURE_REQUIRE_OK(world.create_entities(mask, batch));
        for (const entity id : batch) {
            CHECK(read_image(world.get_raw(id, mover, false)).hits == 10);
        }

        command_buffer commands(world);
        const entity deferred = commands.create_entity();
        commands.add_raw(deferred, mover);
        CHECK(world.flush(commands).applied == 2);
        CHECK(read_image(world.get_raw(deferred, mover, false)).speed == 1.0f);

        // A default of the wrong size is refused.
        const std::array<std::byte, 3> wrong = {};
        dynamic_component_desc bad = mover_desc(wrong);
        bad.name = core::string_id("script.bad_default");
        CHECK_FALSE(fixture.registry.register_dynamic(bad).has_value());
    }

    TEST_CASE("dynamic_components - reflection fields enumerable") {
        ecs_fixture fixture = {};
        const core::result<component_id> registered = fixture.registry.register_dynamic(mover_desc());
        CONJURE_REQUIRE_OK(registered);
        const component_info &info = fixture.registry.info(registered.value());
        REQUIRE(info.reflection != nullptr);
        const core::reflect::type_info &type = *info.reflection;
        CHECK(type.name == "script.mover");
        CHECK(type.size == sizeof(mover_image));
        CHECK(type.alignment == 8);
        CHECK(type.isTriviallyCopyable);
        REQUIRE(type.fields.size() == 3);
        CHECK(type.fields[0].name == "speed");
        CHECK(type.fields[0].kind == core::reflect::field_kind::F32);
        CHECK(type.fields[0].offset == 0);
        CHECK(type.fields[1].name == "hits");
        CHECK(type.fields[1].kind == core::reflect::field_kind::I32);
        CHECK(type.fields[2].name == "target");
        CHECK(type.fields[2].kind == core::reflect::field_kind::ENTITY);
        CHECK(type.fields[2].size == sizeof(entity));
        CHECK(type.fields[2].offset == 8);
        CHECK(type.find_field("hits") == &type.fields[1]);
        CHECK(info.entityFieldOffsets == std::vector<u32>{8});
        CHECK(info.assetRefFieldOffsets.empty());

        // on_registered delivers the dynamic description too.
        bool fired = false;
        core::event<const component_info &> &registrations = fixture.registry.on_registered();
        const core::event_subscription token = registrations.subscribe([&](const component_info &other) {
            fired = other.name == core::string_id("script.second");
        });
        dynamic_component_desc second = mover_desc();
        second.name = core::string_id("script.second");
        CONJURE_REQUIRE_OK(fixture.registry.register_dynamic(second));
        CHECK(fired);
    }

    TEST_CASE("dynamic_components - hot reload sequence keeps id and data") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<component_id> registered = fixture.registry.register_dynamic(mover_desc());
        CONJURE_REQUIRE_OK(registered);
        const component_id mover = registered.value();

        std::vector<entity> ids(5);
        component_mask mask = {};
        mask.set(mover);
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        for (u32 index = 0; index < ids.size(); ++index) {
            mover_image image = {};
            image.speed = static_cast<f32>(index);
            image.hits = static_cast<i32>(index * 10);
            std::memcpy(world.get_raw(ids[index], mover, true), &image, sizeof(image));
        }

        // 1. serialise every instance
        std::vector<std::string> saved = {};
        for (const entity id : ids) {
            core::json::writer out = {};
            CONJURE_REQUIRE_OK(component_to_json(world, id, mover, out));
            saved.push_back(out.finish());
        }
        CHECK(saved[2] == R"({"speed":2.0,"hits":20,"target":null})");

        // 2. remove_from_all, 3. redefine with a wider layout
        CONJURE_REQUIRE_OK(world.remove_from_all(mover));
        struct wider_image {
            f64 speed = 0.0;
            i32 hits = 0;
            i32 armour = 0;
            entity target = {};
        };
        const std::array<dynamic_field_desc, 4> widerFields = {
            dynamic_field_desc{core::string_id("speed"), core::reflect::type_id_of<f64>(), 0},
            dynamic_field_desc{core::string_id("hits"), core::reflect::type_id_of<i32>(), 8},
            dynamic_field_desc{core::string_id("armour"), core::reflect::type_id_of<i32>(), 12},
            dynamic_field_desc{core::string_id("target"), core::reflect::type_id_of<entity>(), 16},
        };
        dynamic_component_desc wider = {};
        wider.name = core::string_id("script.mover");
        wider.fields = widerFields;
        wider.size = sizeof(wider_image);
        wider.alignment = 8;
        CONJURE_REQUIRE_OK(fixture.registry.redefine_dynamic(mover, wider));
        CHECK(fixture.registry.find(core::string_id("script.mover")) == mover);
        CHECK(fixture.registry.info(mover).size == sizeof(wider_image));

        // 4. re-add, 5. deserialise
        for (u32 index = 0; index < ids.size(); ++index) {
            CONJURE_REQUIRE_OK(world.add_raw(ids[index], mover));
            const core::result<core::json::document> document = core::json::document::parse(saved[index]);
            CONJURE_REQUIRE_OK(document);
            CONJURE_REQUIRE_OK(component_from_json(world, ids[index], mover, document.value().root()));
            wider_image image = {};
            std::memcpy(&image, world.get_raw(ids[index], mover, false), sizeof(image));
            CHECK(image.speed == static_cast<f64>(index));
            CHECK(image.hits == static_cast<i32>(index * 10));
            CHECK(image.armour == 0);
            CHECK(image.target == NULL_ENTITY);
        }
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("dynamic_components - entity field remap for dynamic type") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<component_id> registered = fixture.registry.register_dynamic(mover_desc());
        CONJURE_REQUIRE_OK(registered);
        const component_id mover = registered.value();

        const core::result<entity> a = world.create_entity();
        const core::result<entity> b = world.create_entity();
        CONJURE_REQUIRE_OK(a);
        CONJURE_REQUIRE_OK(b);
        mover_image imageA = {};
        imageA.target = b.value();
        mover_image imageB = {};
        imageB.target = a.value();
        imageB.hits = 1;
        CONJURE_REQUIRE_OK(world.add_raw(a.value(), mover, bytes_of(imageA)));
        CONJURE_REQUIRE_OK(world.add_raw(b.value(), mover, bytes_of(imageB)));

        const std::array<entity, 2> set = {a.value(), b.value()};
        core::result<entity_archive> captured = entity_archive::capture(world, set);
        CONJURE_REQUIRE_OK(captured);
        CHECK(read_image(captured.value().component_bytes(0, mover).data()).target == entity::archive_local(1));
        CHECK(read_image(captured.value().component_bytes(1, mover).data()).target == entity::archive_local(0));

        // Through JSON and back the references are local indices.
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(write_archive_json(captured.value(), out));
        const std::string text = out.finish();
        CHECK(text.find(R"("target":1)") != std::string::npos);
        const core::result<core::json::document> document = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(document);
        core::result<entity_archive> loaded = read_archive_json(document.value().root(), fixture.registry);
        CONJURE_REQUIRE_OK(loaded);

        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(loaded).value());
        const core::result<entity> root = world.instantiate(archive);
        CONJURE_REQUIRE_OK(root);
        const mover_image newA = read_image(world.get_raw(root.value(), mover, false));
        REQUIRE(world.is_alive(newA.target));
        CHECK(newA.target != b.value());
        const mover_image newB = read_image(world.get_raw(newA.target, mover, false));
        CHECK(newB.target == root.value());
        CHECK(newB.hits == 1);
        CHECK(world.entity_count() == 4);
    }
}
