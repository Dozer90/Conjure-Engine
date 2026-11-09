#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/log.h"
#include "conjure/ecs/archetype.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"

#include <algorithm>
#include <array>
#include <cstddef>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace layout_test {

    struct b1 {
        u8 value = 0;
    };

    struct b2 {
        u16 value = 0;
    };

    struct b4 {
        u32 value = 0;
    };

    struct b8 {
        u64 value = 0;
    };

    struct alignas(16) b16 {
        std::array<u32, 4> values = {};
    };

    struct alignas(32) b32 {
        std::array<u32, 8> values = {};
    };

    struct transform_like {
        std::array<f32, 16> matrix = {};
    };

    struct sprite_like {
        std::array<f32, 8> values = {};
    };

    struct velocity_like {
        std::array<f32, 3> values = {};
    };

    struct health_like {
        std::array<i32, 2> values = {};
    };
}

CONJURE_REFLECT_STRUCT(layout_test::b1, "layout.b1", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(layout_test::b2, "layout.b2", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(layout_test::b4, "layout.b4", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(layout_test::b8, "layout.b8", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(layout_test::b16, "layout.b16", CONJURE_FIELD(values));
CONJURE_REFLECT_STRUCT(layout_test::b32, "layout.b32", CONJURE_FIELD(values));
CONJURE_REFLECT_STRUCT(layout_test::transform_like, "layout.transform", CONJURE_FIELD(matrix));
CONJURE_REFLECT_STRUCT(layout_test::sprite_like, "layout.sprite", CONJURE_FIELD(values));
CONJURE_REFLECT_STRUCT(layout_test::velocity_like, "layout.velocity", CONJURE_FIELD(values));
CONJURE_REFLECT_STRUCT(layout_test::health_like, "layout.health", CONJURE_FIELD(values));

CONJURE_COMPONENT(layout_test::b1, "layout.b1");
CONJURE_COMPONENT(layout_test::b2, "layout.b2");
CONJURE_COMPONENT(layout_test::b4, "layout.b4");
CONJURE_COMPONENT(layout_test::b8, "layout.b8");
CONJURE_COMPONENT(layout_test::b16, "layout.b16");
CONJURE_COMPONENT(layout_test::b32, "layout.b32");
CONJURE_COMPONENT(layout_test::transform_like, "layout.transform");
CONJURE_COMPONENT(layout_test::sprite_like, "layout.sprite");
CONJURE_COMPONENT(layout_test::velocity_like, "layout.velocity");
CONJURE_COMPONENT(layout_test::health_like, "layout.health");

namespace {
    /// <summary>
    /// Recomputes the block end for an arbitrary capacity with the same alignment rules; true when
    /// every column fits.
    /// </summary>
    bool fits(const chunk_layout &layout, const component_registry &registry, u32 capacity) {
        u64 cursor = static_cast<u64>(sizeof(entity)) * capacity;
        for (u32 column = 0; column < layout.column_count(); ++column) {
            const component_info &info = registry.info(layout.components[column]);
            const u64 alignment = std::max(info.alignment, COLUMN_ALIGNMENT);
            cursor = (cursor + alignment - 1) & ~(alignment - 1);
            cursor += static_cast<u64>(info.size) * capacity;
        }
        return cursor <= CHUNK_SIZE;
    }
}

TEST_SUITE("ecs/chunk_layout") {

    TEST_CASE("chunk_layout - empty archetype has 2048 rows") {
        component_registry registry = {};
        const core::result<chunk_layout> layout = chunk_layout::compute(component_mask{}, registry);
        CONJURE_REQUIRE_OK(layout);
        CHECK(layout.value().capacity == 2048);
        CHECK(layout.value().column_count() == 0);
        CHECK(layout.value().row_bytes() == sizeof(entity));
        CHECK(layout.value().block_end() == CHUNK_SIZE);
    }

    TEST_CASE("chunk_layout - design examples") {
        component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::transform_like>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::sprite_like>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::velocity_like>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::health_like>());

        component_mask transformOnly = {};
        transformOnly.set(registry.id_of<layout_test::transform_like>());
        const core::result<chunk_layout> single = chunk_layout::compute(transformOnly, registry);
        CONJURE_REQUIRE_OK(single);
        CHECK(single.value().capacity == 227);

        component_mask four = transformOnly;
        four.set(registry.id_of<layout_test::sprite_like>());
        four.set(registry.id_of<layout_test::velocity_like>());
        four.set(registry.id_of<layout_test::health_like>());
        const core::result<chunk_layout> combined = chunk_layout::compute(four, registry);
        CONJURE_REQUIRE_OK(combined);
        CHECK(combined.value().capacity == 132);
        CHECK(combined.value().row_bytes() == 124);
    }

    TEST_CASE("chunk_layout - offsets respect column alignment") {
        component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b1>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b4>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b32>());
        component_mask mask = {};
        mask.set(registry.id_of<layout_test::b1>());
        mask.set(registry.id_of<layout_test::b4>());
        mask.set(registry.id_of<layout_test::b32>());

        const core::result<chunk_layout> computed = chunk_layout::compute(mask, registry);
        CONJURE_REQUIRE_OK(computed);
        const chunk_layout &layout = computed.value();
        REQUIRE(layout.column_count() == 3);
        // Descending alignment: 32, then 4, then 1.
        CHECK(layout.components[0] == registry.id_of<layout_test::b32>());
        CHECK(layout.components[1] == registry.id_of<layout_test::b4>());
        CHECK(layout.components[2] == registry.id_of<layout_test::b1>());
        CHECK(layout.columnOffsets[0] % 32 == 0);
        CHECK(layout.columnOffsets[1] % COLUMN_ALIGNMENT == 0);
        CHECK(layout.columnOffsets[2] % COLUMN_ALIGNMENT == 0);
        CHECK(layout.columnOffsets[0] >= sizeof(entity) * layout.capacity);
        CHECK(layout.columnOffsets[1] >= layout.columnOffsets[0] + 32 * layout.capacity);
        CHECK(layout.columnOffsets[2] >= layout.columnOffsets[1] + 4 * layout.capacity);
        CHECK(layout.columnSizes[0] == 32);
        CHECK(layout.columnSizes[2] == 1);
        CHECK(layout.column_of(registry.id_of<layout_test::b4>()) == 1);
        CHECK(layout.column_of(component_id{}) == chunk_layout::NO_COLUMN);
        CHECK(layout.column_of(registry.id_of<layout_test::b1>()) == 2);
    }

    TEST_CASE("chunk_layout - block never exceeds CHUNK_SIZE") {
        ecs_fixture fixture = {};
        const component_mask masks[] = {
            fixture.mask<position>(),
            fixture.mask<position, velocity, health>(),
            fixture.mask<wide_component, odd_component>(),
            fixture.mask<name_component, nested_component, link, odd_component>(),
        };
        for (const component_mask &mask : masks) {
            const core::result<chunk_layout> layout = chunk_layout::compute(mask, fixture.registry);
            CONJURE_REQUIRE_OK(layout);
            CHECK(layout.value().block_end() <= CHUNK_SIZE);
            CHECK(layout.value().capacity > 0);
        }
    }

    TEST_CASE("chunk_layout - capacity is maximal") {
        component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b1>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b2>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b4>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b8>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b16>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b32>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::transform_like>());

        const component_id all[] = {
            registry.id_of<layout_test::b1>(),
            registry.id_of<layout_test::b2>(),
            registry.id_of<layout_test::b4>(),
            registry.id_of<layout_test::b8>(),
            registry.id_of<layout_test::b16>(),
            registry.id_of<layout_test::b32>(),
            registry.id_of<layout_test::transform_like>(),
        };
        // Every subset of the seven types.
        for (u32 bits = 0; bits < (1u << 7); ++bits) {
            CAPTURE(bits);
            component_mask mask = {};
            for (u32 bit = 0; bit < 7; ++bit) {
                if ((bits & (1u << bit)) != 0) {
                    mask.set(all[bit]);
                }
            }
            const core::result<chunk_layout> computed = chunk_layout::compute(mask, registry);
            CONJURE_REQUIRE_OK(computed);
            const chunk_layout &layout = computed.value();
            CHECK(fits(layout, registry, layout.capacity));
            CHECK_FALSE(fits(layout, registry, layout.capacity + 1));
            CHECK(layout.block_end() <= CHUNK_SIZE);
        }
    }

    TEST_CASE("chunk_layout - tags have no column") {
        ecs_fixture fixture = {};
        const component_mask mask = fixture.mask<player_tag, position>();
        const core::result<chunk_layout> layout = chunk_layout::compute(mask, fixture.registry);
        CONJURE_REQUIRE_OK(layout);
        CHECK(layout.value().column_count() == 1);
        CHECK(layout.value().column_of(fixture.id<player_tag>()) == chunk_layout::NO_COLUMN);
        CHECK(layout.value().column_of(fixture.id<position>()) == 0);
        CHECK(layout.value().row_bytes() == sizeof(entity) + sizeof(position));

        const core::result<chunk_layout> tagOnly = chunk_layout::compute(fixture.mask<player_tag>(), fixture.registry);
        CONJURE_REQUIRE_OK(tagOnly);
        CHECK(tagOnly.value().capacity == 2048);
    }

    TEST_CASE("chunk_layout - capacity zero is ARCHETYPE_TOO_LARGE") {
        component_registry registry = {};
        const std::array<dynamic_field_desc, 1> fields = {
            dynamic_field_desc{core::string_id("v"), core::reflect::type_id_of<u32>(), 0},
        };
        // Five 4096-byte components: 20480 bytes per row cannot fit one row in a chunk.
        component_mask mask = {};
        for (u32 index = 0; index < 5; ++index) {
            dynamic_component_desc desc = {};
            desc.name = core::string_id("layout.big" + std::to_string(index));
            desc.fields = fields;
            desc.size = MAX_COMPONENT_SIZE;
            desc.alignment = 4;
            const core::result<component_id> id = registry.register_dynamic(desc);
            CONJURE_REQUIRE_OK(id);
            mask.set(id.value());
        }
        CONJURE_CHECK_ERROR(chunk_layout::compute(mask, registry), ecs_error::ARCHETYPE_TOO_LARGE);

        // Four of them fit a single row (16384 + 8 bytes do not; three do, with a warning).
        component_mask three = {};
        three.set(registry.find(core::string_id("layout.big0")));
        three.set(registry.find(core::string_id("layout.big1")));
        three.set(registry.find(core::string_id("layout.big2")));
        testing::log_capture logs = {};
        const core::result<chunk_layout> small = chunk_layout::compute(three, registry);
        CONJURE_REQUIRE_OK(small);
        CHECK(small.value().capacity == 1);
        CHECK(logs.count(core::log_level::WARN) == 1);
    }

    TEST_CASE("chunk_layout - unknown component is UNKNOWN_COMPONENT") {
        component_registry registry = {};
        component_mask mask = {};
        mask.set(component_id{5});
        CONJURE_CHECK_ERROR(chunk_layout::compute(mask, registry), ecs_error::UNKNOWN_COMPONENT);
    }

    TEST_CASE("chunk_layout - mixed alignments 1 to 64") {
        ecs_fixture fixture = {};
        component_registry &registry = fixture.registry;
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b1>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b2>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b4>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b8>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b16>());
        CONJURE_REQUIRE_OK(registry.register_component<layout_test::b32>());

        component_mask mask = {};
        mask.set(registry.id_of<layout_test::b1>());
        mask.set(registry.id_of<layout_test::b2>());
        mask.set(registry.id_of<layout_test::b4>());
        mask.set(registry.id_of<layout_test::b8>());
        mask.set(registry.id_of<layout_test::b16>());
        mask.set(registry.id_of<layout_test::b32>());
        mask.set(fixture.id<wide_component>());

        const core::result<chunk_layout> computed = chunk_layout::compute(mask, registry);
        CONJURE_REQUIRE_OK(computed);
        const chunk_layout &layout = computed.value();
        REQUIRE(layout.column_count() == 7);
        u32 previousAlignment = 1u << 30;
        for (u32 column = 0; column < layout.column_count(); ++column) {
            const component_info &info = registry.info(layout.components[column]);
            CHECK(info.alignment <= previousAlignment);
            previousAlignment = info.alignment;
            CHECK(layout.columnOffsets[column] % std::max(info.alignment, COLUMN_ALIGNMENT) == 0);
        }
        CHECK(registry.info(layout.components[0]).alignment == 64);
        CHECK(layout.block_end() <= CHUNK_SIZE);
        CHECK(layout.capacity <= CHUNK_SIZE / layout.row_bytes());
        CHECK_FALSE(fits(layout, registry, layout.capacity + 1));
    }
}
