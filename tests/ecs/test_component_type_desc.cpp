#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/system.h"

#include <array>
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

        const component_type_desc tagDesc = describe_component<player_tag>(component_flags::TRANSIENT);
        CHECK(tagDesc.size == 0);
        CHECK(has_flag(tagDesc.flags, component_flags::TAG));
        CHECK(has_flag(tagDesc.flags, component_flags::TRANSIENT));
        CHECK(tagDesc.reflection == nullptr);

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
        CHECK(b.serializer.writeJson != nullptr);
        // Registering the C++ type afterwards on the same registry returns the described id.
        const core::result<component_id> again = fromDesc.register_component<health>();
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == viaDesc.value());
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
