#pragma once

// Shared fixture of the ecs tests (docs/design/01_ecs.md 7, 10): the test component types, a registry
// + world pair whose destructor validates the world, and for_each_job_mode(), which runs a body once
// inline (jobs == nullptr) and once on a worker pool as doctest SUBCASEs.

#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace ecs_test {

    /// <summary>
    /// Assertions recorded per contract violation: one in Debug and Development, none in Distribution
    /// where CONJURE_ASSERT compiles out (the error return still happens).
    /// </summary>
    inline constexpr conjure::u32 ASSERTS_PER_VIOLATION = CONJURE_ASSERTS_ENABLED ? 1u : 0u;

    struct position {
        conjure::f32 x = 0.0f;
        conjure::f32 y = 0.0f;
        conjure::f32 z = 0.0f;

        [[nodiscard]] bool operator==(const position &) const = default;
    };

    struct velocity {
        conjure::f32 x = 0.0f;
        conjure::f32 y = 0.0f;
        conjure::f32 z = 0.0f;

        [[nodiscard]] bool operator==(const velocity &) const = default;
    };

    /// <summary>
    /// Trivially copyable with a non-trivial default constructor: exercises the default-bytes path.
    /// </summary>
    struct health {
        conjure::i32 value = 100;
        conjure::i32 maximum = 100;

        [[nodiscard]] bool operator==(const health &) const = default;
    };

    struct player_tag {};

    struct frozen_tag {};

    /// <summary>
    /// Non-trivial component: exercises the component_ops path.
    /// </summary>
    struct name_component {
        std::string value = {};

        [[nodiscard]] bool operator==(const name_component &) const = default;
    };

    /// <summary>
    /// Non-trivial component that counts its special members so lifecycle balance can be asserted.
    /// </summary>
    struct tracked {
        static inline conjure::i64 sConstructed = 0;
        static inline conjure::i64 sDestroyed = 0;
        static inline conjure::i64 sMoved = 0;
        static inline conjure::i64 sCopied = 0;

        conjure::i32 value = 0;

        tracked() {
            ++sConstructed;
        }

        explicit tracked(conjure::i32 initial) :
            value(initial) {
            ++sConstructed;
        }

        tracked(const tracked &other) :
            value(other.value) {
            ++sConstructed;
            ++sCopied;
        }

        tracked(tracked &&other) noexcept :
            value(other.value) {
            ++sConstructed;
            ++sMoved;
        }

        tracked &operator=(const tracked &other) = default;
        tracked &operator=(tracked &&other) noexcept = default;

        ~tracked() {
            ++sDestroyed;
        }

        [[nodiscard]] static conjure::i64 live() {
            return sConstructed - sDestroyed;
        }

        static void reset_counters() {
            sConstructed = 0;
            sDestroyed = 0;
            sMoved = 0;
            sCopied = 0;
        }

        [[nodiscard]] bool operator==(const tracked &other) const {
            return value == other.value;
        }
    };

    /// <summary>
    /// Entity references: exercises archive remapping and the entity JSON encoding. Reflected entity
    /// fields sit at 8-byte offsets of an 8-byte aligned type (core::reflect's handle rule), so the
    /// struct is aligned explicitly; its size is a multiple of 8 so no padding is added.
    /// </summary>
    struct alignas(8) link {
        conjure::ecs::entity target = {};
        conjure::ecs::entity other = {};

        [[nodiscard]] bool operator==(const link &) const = default;
    };

    struct alignas(8) inner_part {
        conjure::ecs::entity owner = {};
        conjure::f32 weight = 1.0f;
        conjure::f32 scale = 1.0f;

        [[nodiscard]] bool operator==(const inner_part &) const = default;
    };

    /// <summary>
    /// Nested struct, array of entities and math fields: the serializer's harder cases.
    /// </summary>
    struct nested_component {
        inner_part part = {};
        std::array<conjure::ecs::entity, 2> refs = {};
        conjure::math::vec3 direction = conjure::math::vec3(0.0f, 1.0f, 0.0f);
        conjure::math::quat rotation = conjure::math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        conjure::core::string_id label = {};

        [[nodiscard]] bool operator==(const nested_component &) const = default;
    };

    struct transient_component {
        conjure::u32 frames = 0;

        [[nodiscard]] bool operator==(const transient_component &) const = default;
    };

    /// <summary>
    /// 64-byte aligned, 64 bytes: exercises column alignment.
    /// </summary>
    struct alignas(64) wide_component {
        std::array<conjure::f32, 16> values = {};

        [[nodiscard]] bool operator==(const wide_component &) const = default;
    };

    /// <summary>
    /// 4096 bytes: the largest component allowed.
    /// </summary>
    struct huge_component {
        std::array<conjure::u8, 4096> bytes = {};
    };

    /// <summary>
    /// Three odd sizes to make layout arithmetic interesting.
    /// </summary>
    struct odd_component {
        conjure::u8 a = 0;
        conjure::u8 b = 0;
        conjure::u8 c = 0;

        [[nodiscard]] bool operator==(const odd_component &) const = default;
    };

    struct unreflected_component {
        conjure::u32 value = 0;
    };
}

CONJURE_REFLECT_STRUCT(ecs_test::position, "test.position", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(ecs_test::velocity, "test.velocity", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(ecs_test::health, "test.health", CONJURE_FIELD(value), CONJURE_FIELD(maximum));
CONJURE_REFLECT_STRUCT(ecs_test::name_component, "test.name", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(ecs_test::tracked, "test.tracked", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(ecs_test::link, "test.link", CONJURE_FIELD(target), CONJURE_FIELD(other));
CONJURE_REFLECT_STRUCT(ecs_test::inner_part, "test.inner_part",
    CONJURE_FIELD(owner),
    CONJURE_FIELD(weight),
    CONJURE_FIELD(scale));
CONJURE_REFLECT_STRUCT(ecs_test::nested_component, "test.nested",
    CONJURE_FIELD(part),
    CONJURE_FIELD(refs),
    CONJURE_FIELD(direction),
    CONJURE_FIELD(rotation),
    CONJURE_FIELD(label));
CONJURE_REFLECT_STRUCT(ecs_test::transient_component, "test.transient", CONJURE_FIELD(frames));
CONJURE_REFLECT_STRUCT(ecs_test::wide_component, "test.wide", CONJURE_FIELD(values));
CONJURE_REFLECT_STRUCT(ecs_test::huge_component, "test.huge", CONJURE_FIELD(bytes));
CONJURE_REFLECT_STRUCT(ecs_test::odd_component, "test.odd", CONJURE_FIELD(a), CONJURE_FIELD(b), CONJURE_FIELD(c));

CONJURE_COMPONENT(ecs_test::position, "test.position");
CONJURE_COMPONENT(ecs_test::velocity, "test.velocity");
CONJURE_COMPONENT(ecs_test::health, "test.health");
CONJURE_COMPONENT(ecs_test::player_tag, "test.player_tag");
CONJURE_COMPONENT(ecs_test::frozen_tag, "test.frozen_tag");
CONJURE_COMPONENT(ecs_test::name_component, "test.name");
CONJURE_COMPONENT(ecs_test::tracked, "test.tracked");
CONJURE_COMPONENT(ecs_test::link, "test.link");
CONJURE_COMPONENT(ecs_test::nested_component, "test.nested");
CONJURE_COMPONENT(ecs_test::transient_component, "test.transient");
CONJURE_COMPONENT(ecs_test::wide_component, "test.wide");
CONJURE_COMPONENT(ecs_test::huge_component, "test.huge");
CONJURE_COMPONENT(ecs_test::odd_component, "test.odd");
CONJURE_COMPONENT(ecs_test::unreflected_component, "test.unreflected");

namespace ecs_test {

    /// <summary>
    /// Registers every test component type on a registry. Called by the fixture and by tests that
    /// build their own registry.
    /// </summary>
    inline void register_test_components(conjure::ecs::component_registry &registry) {
        using conjure::ecs::component_flags;
        CONJURE_REQUIRE_OK(registry.register_component<position>());
        CONJURE_REQUIRE_OK(registry.register_component<velocity>());
        CONJURE_REQUIRE_OK(registry.register_component<health>());
        CONJURE_REQUIRE_OK(registry.register_component<player_tag>());
        CONJURE_REQUIRE_OK(registry.register_component<frozen_tag>());
        CONJURE_REQUIRE_OK(registry.register_component<name_component>());
        CONJURE_REQUIRE_OK(registry.register_component<tracked>());
        CONJURE_REQUIRE_OK(registry.register_component<link>());
        CONJURE_REQUIRE_OK(registry.register_component<nested_component>());
        CONJURE_REQUIRE_OK(registry.register_component<transient_component>(component_flags::TRANSIENT));
        CONJURE_REQUIRE_OK(registry.register_component<wide_component>());
        CONJURE_REQUIRE_OK(registry.register_component<odd_component>());
    }

    /// <summary>
    /// A registry with every test type and a world on it. The destructor validates the world, so
    /// every test body ends with a full invariant check (01_ecs.md 10).
    /// </summary>
    struct ecs_fixture {
        conjure::ecs::component_registry registry = {};
        conjure::ecs::world world;      // no default: built on the registry in the constructor

        ecs_fixture() :
            world((register_test_components(registry), registry)) {}

        ~ecs_fixture() {
            CONJURE_CHECK_OK(world.validate());
        }

        ecs_fixture(const ecs_fixture &) = delete;
        ecs_fixture &operator=(const ecs_fixture &) = delete;

        template <typename ComponentType>
        [[nodiscard]] conjure::ecs::component_id id() const noexcept {
            return registry.id_of<ComponentType>();
        }

        template <typename... ComponentTypes>
        [[nodiscard]] conjure::ecs::component_mask mask() const noexcept {
            conjure::ecs::component_mask result = {};
            (result.set(registry.id_of<ComponentTypes>()), ...);
            return result;
        }
    };

    /// <summary>
    /// An archetype whose rows no longer fit a chunk (world::relayout_archetypes after redefine_dynamic):
    /// the component ids and the mask of the archetype, which already exists in the fixture's world with
    /// capacity 0, so every placement into it fails with ARCHETYPE_TOO_LARGE.
    /// </summary>
    struct zero_capacity_archetype {
        conjure::ecs::component_mask mask = {};
        std::array<conjure::ecs::component_id, 4> ids = {};
    };

    /// <summary>
    /// Builds a zero_capacity_archetype: three 4096-byte dynamic types and a 4-byte one fit one row per
    /// chunk; an entity creates the archetype and is destroyed; the small type is then redefined to
    /// 4096 bytes, after which the row no longer fits and the empty archetype is laid out with capacity 0.
    /// </summary>
    [[nodiscard]] inline zero_capacity_archetype make_zero_capacity_archetype(ecs_fixture &fixture) {
        using conjure::ecs::dynamic_component_desc;
        using conjure::ecs::dynamic_field_desc;
        using conjure::core::reflect::type_id_of;
        const conjure::core::string_id fieldName("value");
        const std::array<dynamic_field_desc, 1> byteField = {
            dynamic_field_desc{fieldName, type_id_of<conjure::u8>(), 0},
        };
        const std::array<dynamic_field_desc, 1> floatField = {
            dynamic_field_desc{fieldName, type_id_of<conjure::f32>(), 0},
        };
        const std::array<std::string_view, 3> hugeNames = {"test.zero.a", "test.zero.b", "test.zero.c"};

        zero_capacity_archetype result = {};
        for (conjure::usize index = 0; index < hugeNames.size(); ++index) {
            dynamic_component_desc huge = {};
            huge.name = conjure::core::string_id(hugeNames[index]);
            huge.fields = byteField;
            huge.size = conjure::ecs::MAX_COMPONENT_SIZE;
            huge.alignment = 4;
            const conjure::core::result<conjure::ecs::component_id> registered =
              fixture.registry.register_dynamic(huge);
            REQUIRE(registered.has_value());
            result.ids[index] = registered.value();
            result.mask.set(registered.value());
        }
        dynamic_component_desc small = {};
        small.name = conjure::core::string_id("test.zero.small");
        small.fields = floatField;
        small.size = sizeof(conjure::f32);
        small.alignment = 4;
        const conjure::core::result<conjure::ecs::component_id> smallId = fixture.registry.register_dynamic(small);
        REQUIRE(smallId.has_value());
        result.ids[3] = smallId.value();
        result.mask.set(smallId.value());

        const conjure::core::result<conjure::ecs::entity> probe = fixture.world.create_entity(result.mask);
        REQUIRE(probe.has_value());
        REQUIRE(fixture.world.destroy_entity(probe.value()).has_value());

        dynamic_component_desc grown = small;
        grown.size = conjure::ecs::MAX_COMPONENT_SIZE;
        REQUIRE(fixture.registry.redefine_dynamic(smallId.value(), grown).has_value());
        return result;
    }

    /// <summary>
    /// Worker count of the parallel job mode: at least 4, more when the environment allows it.
    /// </summary>
    [[nodiscard]] inline conjure::u32 parallel_worker_count() {
        return std::max<conjure::u32>(4, conjure::testing::environment().jobThreadCount);
    }

    /// <summary>
    /// Runs the body twice as SUBCASEs: "inline" with a null job system and "parallel" with a started
    /// worker pool. The body receives core::jobs::job_system * (nullptr inline).
    /// </summary>
    template <typename Callable>
    void for_each_job_mode(Callable &&body) {
        SUBCASE("inline") {
            body(static_cast<conjure::core::jobs::job_system *>(nullptr));
        }
        SUBCASE("parallel") {
            conjure::core::jobs::job_system jobs = {};
            conjure::core::jobs::job_system_desc desc = {};
            desc.workerCount = parallel_worker_count();
            REQUIRE(jobs.start(desc).has_value());
            body(&jobs);
            jobs.stop();
        }
    }
}
