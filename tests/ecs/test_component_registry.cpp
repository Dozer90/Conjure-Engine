#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"

#include "conjure/core/event.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace registry_test {

    struct odd_handle {
        u8 bytes[3] = {};
    };

    struct with_odd_handle {
        odd_handle handle = {};
        u8 pad = 0;
    };

    struct same_name_other_layout {
        f32 x = 0.0f;
        f32 y = 0.0f;
    };

    struct same_layout_twin {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
    };

    struct oversized {
        std::array<u8, 4097> bytes = {};
    };

    struct alignas(128) over_aligned {
        std::array<f32, 32> values = {};
    };

    // Padded by hand to a multiple of the alignment so MSVC does not warn (C4324) about padding.
    struct alignas(32) non_trivial_aligned {
        std::string text = {};
        std::array<u8, 32 - sizeof(std::string) % 32> values = {};
    };
}

template <>
struct conjure::core::reflect::reflect_kind_override<registry_test::odd_handle> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ENTITY;
    static constexpr std::string_view NAME = "test.odd_handle";
};

CONJURE_REFLECT_STRUCT(registry_test::with_odd_handle, "test.with_odd_handle",
    CONJURE_FIELD(handle),
    CONJURE_FIELD(pad));
CONJURE_REFLECT_STRUCT(registry_test::same_name_other_layout, "test.same_name_other_layout",
    CONJURE_FIELD(x),
    CONJURE_FIELD(y));
CONJURE_REFLECT_STRUCT(registry_test::same_layout_twin, "test.same_layout_twin",
    CONJURE_FIELD(x),
    CONJURE_FIELD(y),
    CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(registry_test::oversized, "test.oversized", CONJURE_FIELD(bytes));
CONJURE_REFLECT_STRUCT(registry_test::over_aligned, "test.over_aligned", CONJURE_FIELD(values));
CONJURE_REFLECT_STRUCT(registry_test::non_trivial_aligned, "test.non_trivial_aligned",
    CONJURE_FIELD(values),
    CONJURE_FIELD(text));

CONJURE_COMPONENT(registry_test::with_odd_handle, "test.with_odd_handle");
CONJURE_COMPONENT(registry_test::same_name_other_layout, "test.position");
CONJURE_COMPONENT(registry_test::same_layout_twin, "test.position");
CONJURE_COMPONENT(registry_test::oversized, "test.oversized");
CONJURE_COMPONENT(registry_test::over_aligned, "test.over_aligned");
CONJURE_COMPONENT(registry_test::non_trivial_aligned, "test.non_trivial_aligned");

namespace {
    dynamic_component_desc make_dynamic(
      std::string_view name,
      std::span<const dynamic_field_desc> fields,
      u32 size,
      u32 alignment
    ) {
        dynamic_component_desc desc = {};
        desc.name = core::string_id(name);
        desc.fields = fields;
        desc.size = size;
        desc.alignment = alignment;
        return desc;
    }
}

TEST_SUITE("ecs/component_registry") {

    TEST_CASE("component_registry - register trivial non trivial and tag") {
        component_registry registry = {};
        CHECK(registry.count() == 1);

        const core::result<component_id> trivial = registry.register_component<position>();
        CONJURE_REQUIRE_OK(trivial);
        const component_info &positionInfo = registry.info(trivial.value());
        CHECK(positionInfo.size == sizeof(position));
        CHECK(positionInfo.alignment == alignof(position));
        CHECK(positionInfo.is_trivial());
        CHECK_FALSE(positionInfo.is_tag());
        CHECK(positionInfo.ops.constructDefault == nullptr);
        CHECK(positionInfo.reflection != nullptr);
        CHECK(positionInfo.serializer.writeJson != nullptr);
        CHECK(positionInfo.name == core::string_id("test.position"));
        CHECK(positionInfo.name.view() == "test.position");

        const core::result<component_id> nonTrivial = registry.register_component<name_component>();
        CONJURE_REQUIRE_OK(nonTrivial);
        const component_info &nameInfo = registry.info(nonTrivial.value());
        CHECK_FALSE(nameInfo.is_trivial());
        CHECK(nameInfo.ops.constructDefault != nullptr);
        CHECK(nameInfo.ops.destruct != nullptr);
        CHECK(nameInfo.ops.relocate != nullptr);
        CHECK(nameInfo.ops.copy != nullptr);

        const core::result<component_id> tag = registry.register_component<player_tag>();
        CONJURE_REQUIRE_OK(tag);
        const component_info &tagInfo = registry.info(tag.value());
        CHECK(tagInfo.is_tag());
        CHECK(tagInfo.size == 0);
        CHECK(tagInfo.reflection == nullptr);

        const core::result<component_id> defaults = registry.register_component<health>();
        CONJURE_REQUIRE_OK(defaults);
        CHECK(registry.info(defaults.value()).defaultValue.size() == sizeof(health));

        CHECK(registry.count() == 5);
        CHECK(trivial.value().value == 1);
        CHECK(nonTrivial.value().value == 2);
        CHECK(tag.value().value == 3);
        CHECK(registry.types().size() == 5);
        CHECK(registry.types()[3].is_tag());
    }

    TEST_CASE("component_registry - same name same layout returns same id") {
        component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<position>());
        const component_id first = registry.id_of<position>();
        const core::result<component_id> again = registry.register_component<position>();
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == first);

        // A different C++ type with the same name, size, alignment and flags shares the id.
        const core::result<component_id> twin = registry.register_component<registry_test::same_layout_twin>();
        CONJURE_REQUIRE_OK(twin);
        CHECK(twin.value() == first);
        CHECK(registry.count() == 2);
    }

    TEST_CASE("component_registry - same name different layout is NAME_CONFLICT") {
        component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<position>());
        CONJURE_CHECK_ERROR(
          registry.register_component<registry_test::same_name_other_layout>(),
          ecs_error::NAME_CONFLICT
        );
        CONJURE_CHECK_ERROR(
          registry.register_component<position>(component_flags::TRANSIENT),
          ecs_error::NAME_CONFLICT
        );
        CHECK(registry.count() == 2);
    }

    TEST_CASE("component_registry - find by name") {
        component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<position>());
        CONJURE_REQUIRE_OK(registry.register_component<velocity>());
        CHECK(registry.find(core::string_id("test.velocity")) == registry.id_of<velocity>());
        CHECK(registry.find(core::string_id("test.position")) == registry.id_of<position>());
        CHECK_FALSE(registry.find(core::string_id("test.missing")).is_valid());
        CHECK_FALSE(registry.find(core::string_id()).is_valid());
        CHECK(registry.find(core::string_id(UNRESOLVED_COMPONENTS_NAME)) == component_id{0});
    }

    TEST_CASE("component_registry - id_of cache follows registry serial") {
        component_registry first = {};
        component_registry second = {};
        CHECK(first.serial() != second.serial());
        CHECK(first.serial() != 0);

        CONJURE_REQUIRE_OK(first.register_component<velocity>());
        CONJURE_REQUIRE_OK(first.register_component<position>());
        CONJURE_REQUIRE_OK(second.register_component<position>());

        CHECK(first.id_of<position>().value == 2);
        CHECK(second.id_of<position>().value == 1);
        CHECK(first.id_of<position>().value == 2);
        CHECK_FALSE(second.id_of<velocity>().is_valid());

        // A miss is not cached: registering afterwards is seen.
        CONJURE_REQUIRE_OK(second.register_component<velocity>());
        CHECK(second.id_of<velocity>().value == 2);
    }

    TEST_CASE("component_registry - register_dynamic happy path") {
        component_registry registry = {};
        const std::array<dynamic_field_desc, 3> fields = {
            dynamic_field_desc{core::string_id("speed"), core::reflect::type_id_of<f32>(), 0},
            dynamic_field_desc{core::string_id("hits"), core::reflect::type_id_of<i32>(), 4},
            dynamic_field_desc{core::string_id("target"), core::reflect::type_id_of<entity>(), 8},
        };
        const std::array<std::byte, 16> defaults = {
            std::byte{0}, std::byte{0}, std::byte{0x80}, std::byte{0x3F},     // 1.0f
            std::byte{3}, std::byte{0}, std::byte{0}, std::byte{0},           // 3
            std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
            std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        };
        dynamic_component_desc desc = make_dynamic("script.mover", fields, 16, 8);
        desc.defaultValue = defaults;

        const core::result<component_id> registered = registry.register_dynamic(desc);
        CONJURE_REQUIRE_OK(registered);
        const component_info &info = registry.info(registered.value());
        CHECK(has_flag(info.flags, component_flags::DYNAMIC));
        CHECK(has_flag(info.flags, component_flags::TRIVIAL));
        CHECK(info.size == 16);
        CHECK(info.alignment == 8);
        REQUIRE(info.reflection != nullptr);
        CHECK(info.reflection->fields.size() == 3);
        CHECK(info.reflection->fields[2].kind == core::reflect::field_kind::ENTITY);
        CHECK(info.entityFieldOffsets == std::vector<u32>{8});
        CHECK(info.defaultValue.size() == 16);
        CHECK(info.serializer.writeJson == nullptr);
        CHECK(registry.find(core::string_id("script.mover")) == registered.value());
    }

    TEST_CASE("component_registry - rejects bad alignment oversize and limit") {
        component_registry registry = {};
        CONJURE_CHECK_ERROR(registry.register_component<registry_test::oversized>(), ecs_error::COMPONENT_TOO_LARGE);
        CONJURE_CHECK_ERROR(registry.register_component<registry_test::over_aligned>(), ecs_error::BAD_ALIGNMENT);
        CONJURE_CHECK_ERROR(
          registry.register_component<registry_test::non_trivial_aligned>(),
          ecs_error::BAD_ALIGNMENT
        );

        const std::array<dynamic_field_desc, 1> fields = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<f32>(), 0},
        };
        CONJURE_CHECK_ERROR(registry.register_dynamic(make_dynamic("dyn.a", fields, 4, 3)), ecs_error::BAD_ALIGNMENT);
        CONJURE_CHECK_ERROR(registry.register_dynamic(make_dynamic("dyn.b", fields, 4, 32)), ecs_error::BAD_ALIGNMENT);
        CONJURE_CHECK_ERROR(
          registry.register_dynamic(make_dynamic("dyn.c", fields, 8192, 4)),
          ecs_error::COMPONENT_TOO_LARGE
        );

        // Fill the registry: 1023 more names bring it to MAX_COMPONENTS.
        for (u32 index = registry.count(); index < MAX_COMPONENTS; ++index) {
            const std::string name = "dyn.fill." + std::to_string(index);
            CONJURE_REQUIRE_OK(registry.register_dynamic(make_dynamic(name, fields, 4, 4)));
        }
        CHECK(registry.count() == MAX_COMPONENTS);
        CONJURE_CHECK_ERROR(
          registry.register_dynamic(make_dynamic("dyn.one_too_many", fields, 4, 4)),
          ecs_error::COMPONENT_LIMIT_REACHED
        );
        CONJURE_CHECK_ERROR(registry.register_component<position>(), ecs_error::COMPONENT_LIMIT_REACHED);
    }

    TEST_CASE("component_registry - redefine_dynamic refused while instances exist") {
        component_registry registry = {};
        const std::array<dynamic_field_desc, 1> fields = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<f32>(), 0},
        };
        const core::result<component_id> id = registry.register_dynamic(make_dynamic("dyn.live", fields, 4, 4));
        CONJURE_REQUIRE_OK(id);
        CHECK_FALSE(registry.has_instances(id.value()));
        {
            world world(registry);
            const core::result<entity> created = world.create_entity();
            CONJURE_REQUIRE_OK(created);
            CONJURE_REQUIRE_OK(world.add_raw(created.value(), id.value()));
            CHECK(registry.has_instances(id.value()));

            const std::array<dynamic_field_desc, 1> wider = {
                dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<f64>(), 0},
            };
            CONJURE_CHECK_ERROR(
              registry.redefine_dynamic(id.value(), make_dynamic("dyn.live", wider, 8, 8)),
              ecs_error::COMPONENT_IN_USE
            );
            CHECK(registry.info(id.value()).size == 4);

            CONJURE_REQUIRE_OK(world.remove_from_all(id.value()));
            CHECK_FALSE(registry.has_instances(id.value()));
            CONJURE_REQUIRE_OK(registry.redefine_dynamic(id.value(), make_dynamic("dyn.live", wider, 8, 8)));
            CHECK(registry.info(id.value()).size == 8);

            // The archetype that held the old layout is reusable with the new one.
            CONJURE_REQUIRE_OK(world.add_raw(created.value(), id.value()));
            CHECK(world.has_raw(created.value(), id.value()));
            CONJURE_CHECK_OK(world.validate());
        }
        CHECK_FALSE(registry.has_instances(id.value()));
    }

    TEST_CASE("component_registry - redefine_dynamic keeps id") {
        component_registry registry = {};
        const std::array<dynamic_field_desc, 1> fields = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<f32>(), 0},
        };
        const core::result<component_id> id = registry.register_dynamic(make_dynamic("dyn.keep", fields, 4, 4));
        CONJURE_REQUIRE_OK(id);

        const std::array<dynamic_field_desc, 2> more = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<f32>(), 0},
            dynamic_field_desc{core::string_id("other"), core::reflect::type_id_of<u32>(), 4},
        };
        CONJURE_REQUIRE_OK(registry.redefine_dynamic(id.value(), make_dynamic("dyn.keep", more, 8, 4)));
        CHECK(registry.find(core::string_id("dyn.keep")) == id.value());
        CHECK(registry.info(id.value()).size == 8);
        CHECK(registry.info(id.value()).reflection->fields.size() == 2);
        CHECK(registry.count() == 2);

        CONJURE_CHECK_ERROR(
          registry.redefine_dynamic(id.value(), make_dynamic("dyn.renamed", more, 8, 4)),
          ecs_error::NAME_CONFLICT
        );
        CONJURE_CHECK_ERROR(
          registry.redefine_dynamic(component_id{0}, make_dynamic("", more, 8, 4)),
          ecs_error::UNKNOWN_COMPONENT
        );
        CONJURE_CHECK_ERROR(
          registry.redefine_dynamic(component_id{77}, make_dynamic("", more, 8, 4)),
          ecs_error::UNKNOWN_COMPONENT
        );
        CONJURE_CHECK_ERROR(
          registry.redefine_dynamic(id.value(), make_dynamic("dyn.keep", more, 8, 3)),
          ecs_error::BAD_ALIGNMENT
        );
    }

    TEST_CASE("component_registry - unsupported field fails registration") {
        component_registry registry = {};
        CONJURE_CHECK_ERROR(
          registry.register_component<registry_test::with_odd_handle>(),
          ecs_error::UNSUPPORTED_FIELD
        );
        CONJURE_CHECK_ERROR(registry.register_component<unreflected_component>(), ecs_error::UNSUPPORTED_FIELD);
        // Without serialisation the missing reflection is fine.
        CONJURE_REQUIRE_OK(registry.register_component<unreflected_component>(component_flags::TRANSIENT));

        const std::array<dynamic_field_desc, 1> bad = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id::from_name("no.such.type"), 0},
        };
        CONJURE_CHECK_ERROR(
          registry.register_dynamic(make_dynamic("dyn.bad", bad, 4, 4)),
          ecs_error::UNSUPPORTED_FIELD
        );
        const std::array<dynamic_field_desc, 1> misaligned = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<f32>(), 1},
        };
        CONJURE_CHECK_ERROR(
          registry.register_dynamic(make_dynamic("dyn.misaligned", misaligned, 8, 4)),
          ecs_error::UNSUPPORTED_FIELD
        );
        CHECK(registry.count() == 2);
    }

    TEST_CASE("component_registry - on_registered fires once") {
        component_registry registry = {};
        std::vector<std::string> names = {};
        const core::event_subscription token = registry.on_registered().subscribe([&](const component_info &info) {
            names.push_back(std::string(info.name.view()));
        });
        CONJURE_REQUIRE_OK(registry.register_component<position>());
        CONJURE_REQUIRE_OK(registry.register_component<position>());
        CONJURE_REQUIRE_OK(registry.register_component<player_tag>());
        CHECK(names == std::vector<std::string>{"test.position", "test.player_tag"});
    }

    TEST_CASE("component_registry - unresolved_components is id zero") {
        component_registry registry = {};
        const component_info &info = registry.info(component_id{0});
        CHECK(info.id == component_id{0});
        CHECK(info.name == core::string_id(UNRESOLVED_COMPONENTS_NAME));
        CHECK(has_flag(info.flags, component_flags::EDITOR_HIDDEN));
        CHECK_FALSE(info.is_trivial());
        CHECK(info.ops.destruct != nullptr);
        CHECK(info.serializer.writeJson != nullptr);
        CHECK(info.serializer.readJson != nullptr);
        CHECK(info.size == sizeof(unresolved_components));
    }

    TEST_CASE("component_registry - info of an unregistered id asserts") {
        component_registry registry = {};
        CHECK_FALSE(registry.is_registered(component_id{5}));
        CHECK_FALSE(registry.is_registered(component_id{}));
        CONJURE_CHECK_ASSERTS(static_cast<void>(registry.info(component_id{5})));
    }

    TEST_CASE("component_registry - register_described copies the reflection") {
        component_registry registry = {};
        const component_type_desc desc = describe_component<velocity>();
        const core::result<component_id> registered = registry.register_described(desc);
        CONJURE_REQUIRE_OK(registered);
        const component_info &info = registry.info(registered.value());
        CHECK(info.size == sizeof(velocity));
        CHECK(info.reflection != desc.reflection);
        REQUIRE(info.reflection != nullptr);
        CHECK(info.reflection->fields.size() == 3);
        CHECK(registry.id_of<velocity>() == registered.value());

        component_type_desc nameless = desc;
        nameless.name = nullptr;
        CHECK_FALSE(registry.register_described(nameless).has_value());
    }

    TEST_CASE("regression - review 11 register_described deep-copies nested descriptions") {
        component_registry registry = {};
        const component_type_desc desc = describe_component<nested_component>();
        REQUIRE(desc.reflection != nullptr);
        const core::result<component_id> registered = registry.register_described(desc);
        CONJURE_REQUIRE_OK(registered);
        const component_info &info = registry.info(registered.value());
        REQUIRE(info.reflection != nullptr);
        CHECK(info.reflection != desc.reflection);
        REQUIRE(info.reflection->fields.size() == desc.reflection->fields.size());

        // The nested struct description is a private copy too, with the same content.
        const core::reflect::field_info &part = info.reflection->fields[0];
        const core::reflect::field_info &sourcePart = desc.reflection->fields[0];
        REQUIRE(part.kind == core::reflect::field_kind::STRUCT);
        REQUIRE(part.structType != nullptr);
        CHECK(part.structType != sourcePart.structType);
        CHECK(part.structType->name == sourcePart.structType->name);
        CHECK(part.structType->fields.size() == 3);
        CHECK(part.structType->has_same_layout(*sourcePart.structType));
        CHECK(info.entityFieldOffsets.size() == 3);

        // A refused registration keeps no copy: the registry is back at its baseline afterwards.
        CONJURE_REQUIRE_OK(registry.register_component<position>());
        component_type_desc conflicting = describe_component<health>();
        conflicting.name = "test.position";
        testing::memory_guard guard = {};
        CONJURE_CHECK_ERROR(registry.register_described(conflicting), ecs_error::NAME_CONFLICT);
        guard.check();
        CHECK(registry.count() == 3);
    }

    TEST_CASE("component_registry - has_instances sums over live worlds") {
        ecs_fixture fixture = {};
        world second(fixture.registry);
        const component_id id = fixture.id<position>();
        CHECK_FALSE(fixture.registry.has_instances(id));

        const core::result<entity> created = second.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        CHECK(fixture.registry.has_instances(id));
        CONJURE_REQUIRE_OK(second.destroy_entity(created.value()));
        CHECK_FALSE(fixture.registry.has_instances(id));
        CHECK_FALSE(fixture.registry.has_instances(component_id{}));
    }
}
