#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/world.h"

#include <array>
#include <string>
#include <string_view>
#include <type_traits>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    void run_nothing(system_context &) {}
}

TEST_SUITE("ecs/component_type_desc") {

    TEST_CASE("component_type_desc - trivially copyable with the extension point") {
        static_assert(std::is_trivially_copyable_v<component_type_desc>);
        static_assert(std::is_trivially_copyable_v<system_extension_desc>);
        CHECK(component_type_desc::EXTENSION_POINT == core::string_id("ecs.component"));
        CHECK(system_extension_desc::EXTENSION_POINT == core::string_id("ecs.system"));
        const component_type_desc defaults = {};
        CHECK(defaults.name == nullptr);
        CHECK(defaults.size == 0);
        CHECK(defaults.alignment == 1);
        CHECK(defaults.reflection == nullptr);
        CHECK(defaults.serializer.writeJson == nullptr);
        CHECK(defaults.serializer.readJson == nullptr);
        CHECK(defaults.serializer.remapEntities == nullptr);
    }

    TEST_CASE("describe_component - trivial tag and non trivial types") {
        const component_type_desc positionDesc = describe_component<position>();
        CHECK(std::string_view(positionDesc.name) == "test.position");
        CHECK(positionDesc.size == sizeof(position));
        CHECK(positionDesc.alignment == alignof(position));
        CHECK(has_flag(positionDesc.flags, component_flags::TRIVIAL));
        CHECK_FALSE(has_flag(positionDesc.flags, component_flags::TAG));
        CHECK(positionDesc.ops.constructDefault == nullptr);
        REQUIRE(positionDesc.reflection != nullptr);
        CHECK(positionDesc.reflection->fields.size() == 3);

        // The described path carries no C++ serializer hooks: the registry's reflection walker over its
        // deep copy serves the type, so nothing in the record points into the module's code.
        CHECK(positionDesc.serializer.writeJson == nullptr);
        CHECK(positionDesc.serializer.readJson == nullptr);
        CHECK(positionDesc.serializer.remapEntities == nullptr);

        const component_type_desc tagDesc = describe_component<player_tag>(component_flags::TRANSIENT);
        CHECK(tagDesc.size == 0);
        CHECK(has_flag(tagDesc.flags, component_flags::TAG));
        CHECK(has_flag(tagDesc.flags, component_flags::TRANSIENT));
        CHECK(tagDesc.reflection == nullptr);
        CHECK(tagDesc.serializer.writeJson == nullptr);

        const component_type_desc nameDesc = describe_component<name_component>();
        CHECK_FALSE(has_flag(nameDesc.flags, component_flags::TRIVIAL));
        CHECK(nameDesc.ops.constructDefault != nullptr);
        CHECK(nameDesc.ops.destruct != nullptr);
        CHECK(nameDesc.ops.relocate != nullptr);
        CHECK(nameDesc.ops.copy != nullptr);
        CHECK(nameDesc.reflection != nullptr);

        const component_type_desc unreflected = describe_component<unreflected_component>(component_flags::TRANSIENT);
        CHECK(unreflected.reflection == nullptr);
    }

    TEST_CASE("component_type_desc - registers like the C++ type") {
        component_registry fromDesc = {};
        component_registry fromType = {};
        const core::result<component_id> viaDesc = fromDesc.register_described(describe_component<health>());
        const core::result<component_id> viaType = fromType.register_component<health>();
        CONJURE_REQUIRE_OK(viaDesc);
        CONJURE_REQUIRE_OK(viaType);
        const component_info &a = fromDesc.info(viaDesc.value());
        const component_info &b = fromType.info(viaType.value());
        CHECK(a.name == b.name);
        CHECK(a.size == b.size);
        CHECK(a.alignment == b.alignment);
        CHECK(a.flags == b.flags);
        CHECK(a.reflection->layout_hash() == b.reflection->layout_hash());
        // The described path carries no C++ serializer hooks; reflection drives it instead.
        CHECK(a.serializer.writeJson == nullptr);
        CHECK(a.serializer.readJson == nullptr);
        CHECK(a.serializer.remapEntities == nullptr);
        CHECK(b.serializer.writeJson != nullptr);
        // Registering the C++ type afterwards on the same registry returns the described id.
        const core::result<component_id> again = fromDesc.register_component<health>();
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == viaDesc.value());
    }

    TEST_CASE("regression - review 3 described reflected type serialises through the registry copy") {
        // A module that unloads leaves nothing behind but the registry's deep copy: with the hooks null
        // the JSON path has to walk that copy and produce the same text as the C++ registration.
        component_registry fromDesc = {};
        component_registry fromType = {};
        const core::result<component_id> viaDesc = fromDesc.register_described(describe_component<health>());
        const core::result<component_id> viaType = fromType.register_component<health>();
        CONJURE_REQUIRE_OK(viaDesc);
        CONJURE_REQUIRE_OK(viaType);
        CHECK(fromDesc.info(viaDesc.value()).serializer.writeJson == nullptr);

        const auto round_trip = [](world &target, component_id id) -> std::string {
            const core::result<entity> owner = target.create_entity();
            CONJURE_REQUIRE_OK(owner);
            const core::result<void *> added = target.add_raw(owner.value(), id);
            CONJURE_REQUIRE_OK(added);
            *static_cast<health *>(added.value()) = health{42, 100};
            core::json::writer out = {};
            CONJURE_REQUIRE_OK(component_to_json(target, owner.value(), id, out));
            const std::string text = out.finish();
            const core::result<core::json::document> parsed =
              core::json::document::parse(R"({"value":7,"maximum":9})");
            CONJURE_REQUIRE_OK(parsed);
            CONJURE_REQUIRE_OK(component_from_json(target, owner.value(), id, parsed.value().root()));
            const health *restored = static_cast<const health *>(target.get_raw(owner.value(), id));
            REQUIRE(restored != nullptr);
            CHECK(restored->value == 7);
            CHECK(restored->maximum == 9);
            return text;
        };
        world describedWorld(fromDesc);
        world typedWorld(fromType);
        const std::string fromDescText = round_trip(describedWorld, viaDesc.value());
        CHECK(fromDescText == round_trip(typedWorld, viaType.value()));
        CHECK(fromDescText == R"({"value":42,"maximum":100})");
    }

    TEST_CASE("component_type_desc - custom serializer hooks register without reflection") {
        // What scripting's script_component does (R29): a handle type with its own JSON form.
        struct handle_component {
            u32 value = 0;
        };
        static constexpr auto writeHook = [](const void *component, core::json::writer &out) -> core::result<void> {
            out.begin_object();
            out.key("handle");
            out.value(static_cast<i64>(static_cast<const handle_component *>(component)->value));
            out.end_object();
            return core::result<void>::success();
        };
        static constexpr auto readHook = [](void *component, const core::json::value &in) -> core::result<void> {
            static_cast<handle_component *>(component)->value = static_cast<u32>(in["handle"].as_i64());
            return core::result<void>::success();
        };
        component_type_desc desc = {};
        desc.name = "test.handle";
        desc.size = sizeof(handle_component);
        desc.alignment = alignof(handle_component);
        desc.flags = component_flags::TRIVIAL;
        desc.serializer.writeJson = writeHook;
        desc.serializer.readJson = readHook;

        component_registry registry = {};
        const core::result<component_id> id = registry.register_described(desc);
        CONJURE_REQUIRE_OK(id);
        const component_info &info = registry.info(id.value());
        CHECK(info.reflection == nullptr);
        CHECK(info.serializer.writeJson == writeHook);
        CHECK(info.serializer.readJson == readHook);
        CHECK(info.serializer.remapEntities == nullptr);

        // The hooks drive the live component JSON path both ways.
        world world(registry);
        const core::result<entity> owner = world.create_entity();
        CONJURE_REQUIRE_OK(owner);
        const core::result<void *> added = world.add_raw(owner.value(), id.value());
        CONJURE_REQUIRE_OK(added);
        static_cast<handle_component *>(added.value())->value = 42;
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(component_to_json(world, owner.value(), id.value(), out));
        CHECK(out.finish() == "{\"handle\":42}");
        const core::result<core::json::document> parsed = core::json::document::parse("{\"handle\":7}");
        CONJURE_REQUIRE_OK(parsed);
        CONJURE_REQUIRE_OK(component_from_json(world, owner.value(), id.value(), parsed.value().root()));
        CHECK(static_cast<const handle_component *>(world.get_raw(owner.value(), id.value()))->value == 7);

        // Without reflection the hooks are mandatory.
        component_type_desc bare = desc;
        bare.name = "test.bare_handle";
        bare.serializer = {};
        CONJURE_CHECK_ERROR(registry.register_described(bare), ecs_error::UNSUPPORTED_FIELD);
    }

    TEST_CASE("system_extension_desc - carries name lists by pointer") {
        const std::array<core::string_id, 2> reads = {
            core::string_id("test.position"),
            core::string_id("test.velocity"),
        };
        const std::array<core::string_id, 1> writes = {core::string_id("test.health")};
        const std::array<core::string_id, 1> after = {core::string_id("physics.step")};
        system_extension_desc desc = {};
        desc.name = "game.spin";
        desc.phase = system_phase::LATE_UPDATE;
        desc.flags = system_flags::MAIN_THREAD;
        desc.reads = reads.data();
        desc.readCount = static_cast<u32>(reads.size());
        desc.writes = writes.data();
        desc.writeCount = static_cast<u32>(writes.size());
        desc.after = after.data();
        desc.afterCount = 1;
        desc.run = &run_nothing;

        const system_extension_desc copy = desc;
        CHECK(std::string_view(copy.name) == "game.spin");
        CHECK(copy.phase == system_phase::LATE_UPDATE);
        CHECK(copy.flags == system_flags::MAIN_THREAD);
        CHECK(copy.readCount == 2);
        CHECK(copy.reads[1] == core::string_id("test.velocity"));
        CHECK(copy.writes[0] == core::string_id("test.health"));
        CHECK(copy.after[0] == core::string_id("physics.step"));
        CHECK(copy.beforeCount == 0);
        CHECK(copy.before == nullptr);
        CHECK(copy.run == &run_nothing);
    }
}
