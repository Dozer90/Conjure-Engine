#pragma once

// Shared fixture of the scene tests (docs/design/13_scene.md 7, 10): the scene components plus two test
// components (an asset reference holder and an entity reference holder), a world, an asset manager over a
// memory source, a scene_manager with its scene, a scheduler carrying the four scene systems, step() over
// the frame phases the scene owns, for_each_job_mode() and a destructor that validates scene and world.

#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scene/transform_system.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scene_test {

    /// <summary>
    /// Assertions recorded per contract violation: one in Debug and Development, none in Distribution.
    /// </summary>
    inline constexpr conjure::u32 ASSERTS_PER_VIOLATION = CONJURE_ASSERTS_ENABLED ? 1u : 0u;

    /// <summary>
    /// A component with an asset reference: exercises the bind pass and the importer's dependencies.
    /// </summary>
    struct test_asset_holder {
        conjure::assets::asset_ref<conjure::assets::binary_asset> asset = {};
    };

    /// <summary>
    /// A component with an entity reference: exercises archive remapping inside subtrees.
    /// </summary>
    struct test_entity_holder {
        conjure::ecs::entity other = {};
        conjure::u32 weight = 1;

        [[nodiscard]] bool operator==(const test_entity_holder &) const = default;
    };

    /// <summary>
    /// A plain gameplay value.
    /// </summary>
    struct test_speed {
        conjure::f32 value = 1.0f;

        [[nodiscard]] bool operator==(const test_speed &) const = default;
    };
}

CONJURE_REFLECT_STRUCT(scene_test::test_asset_holder, "test.asset_holder", CONJURE_FIELD(asset));
CONJURE_REFLECT_STRUCT(scene_test::test_entity_holder, "test.entity_holder",
    CONJURE_FIELD(other),
    CONJURE_FIELD(weight));
CONJURE_REFLECT_STRUCT(scene_test::test_speed, "test.speed", CONJURE_FIELD(value));

CONJURE_COMPONENT(scene_test::test_asset_holder, "test.asset_holder");
CONJURE_COMPONENT(scene_test::test_entity_holder, "test.entity_holder");
CONJURE_COMPONENT(scene_test::test_speed, "test.speed");

namespace scene_test {

    /// <summary>
    /// Registers the scene components and the test components on a registry.
    /// </summary>
    inline void register_scene_test_components(conjure::ecs::component_registry &registry) {
        CONJURE_REQUIRE_OK(conjure::scene::register_components(registry));
        CONJURE_REQUIRE_OK(registry.register_component<test_asset_holder>());
        CONJURE_REQUIRE_OK(registry.register_component<test_entity_holder>());
        CONJURE_REQUIRE_OK(registry.register_component<test_speed>());
    }

    /// <summary>
    /// A deterministic uuid for tests (never null).
    /// </summary>
    [[nodiscard]] inline conjure::core::uuid test_uuid(conjure::u64 index) noexcept {
        return conjure::core::uuid(0x5CE4E000000000ull + index, 0x1000 + index);
    }

    /// <summary>
    /// Worker count of the parallel job mode: at least 4, more when the environment allows it.
    /// </summary>
    [[nodiscard]] inline conjure::u32 parallel_worker_count() {
        return std::max<conjure::u32>(4, conjure::testing::environment().jobThreadCount);
    }

    /// <summary>
    /// Runs the body twice as SUBCASEs: "inline" with a null job system and "parallel" with a started
    /// worker pool. The body receives conjure::core::jobs::job_system * (nullptr inline).
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

    /// <summary>
    /// The scene fixture: everything the module needs, headless. The scheduler runs stage systems
    /// inline (parallelSystems = false) and fans parallel_for_chunks out over 'jobs' when given.
    /// </summary>
    class scene_fixture {
        public:
            conjure::ecs::component_registry registry = {};
            conjure::ecs::world world;                                   // no default: built on the registry
            conjure::core::jobs::job_system assetJobs = {};              // never started: asset loads run inline
            conjure::assets::memory_asset_source *source = nullptr;      // owned by the manager
            conjure::assets::asset_manager assets;                       // no default: built from a desc
            conjure::scene::scene_manager manager;                       // no default: bound to world and assets
            conjure::ecs::scheduler scheduler;                           // no default: bound to the world
            conjure::ecs::frame_info frame = {};

        public:
            scene_fixture() :
                scene_fixture(nullptr) {}

            explicit scene_fixture(conjure::core::jobs::job_system *jobs) :
                world((register_scene_test_components(registry), registry)),
                assets(make_asset_desc()),
                manager(world, assets),
                scheduler(world, jobs, make_scheduler_desc()) {
                CONJURE_REQUIRE_OK(conjure::assets::register_binary_asset_type(assets.types()));
                CONJURE_REQUIRE_OK(conjure::scene::register_systems(scheduler, manager));
                CONJURE_REQUIRE_OK(scheduler.finalise());
                frame.deltaTime = 1.0f / 60.0f;
                frame.fixedDeltaTime = 1.0f / 60.0f;
            }

            ~scene_fixture() {
                CONJURE_CHECK_OK(manager.active().validate());
                CONJURE_CHECK_OK(world.validate());
            }

            scene_fixture(const scene_fixture &) = delete;
            scene_fixture &operator=(const scene_fixture &) = delete;

        public:
            [[nodiscard]] conjure::scene::scene &active() noexcept {
                return manager.active();
            }

            /// <summary>
            /// Creates an entity (REQUIRE on failure) and returns it.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity create(
              std::string_view name,
              conjure::ecs::entity parent = conjure::ecs::NULL_ENTITY
            ) {
                const conjure::core::string_id entityName(name);
                const conjure::core::result<conjure::ecs::entity> created =
                  manager.active().create_entity(entityName, parent);
                REQUIRE(created.has_value());
                return created.value();
            }

            /// <summary>
            /// Runs FRAME_BEGIN, TRANSFORM and FRAME_END 'frames' times.
            /// </summary>
            void step(conjure::u32 frames = 1) {
                for (conjure::u32 index = 0; index < frames; ++index) {
                    ++frame.frameIndex;
                    scheduler.run_phase(conjure::ecs::system_phase::FRAME_BEGIN, frame);
                    scheduler.run_phase(conjure::ecs::system_phase::TRANSFORM, frame);
                    scheduler.run_phase(conjure::ecs::system_phase::FRAME_END, frame);
                }
            }

            /// <summary>
            /// Runs only the TRANSFORM phase once.
            /// </summary>
            void step_transform() {
                ++frame.frameIndex;
                scheduler.run_phase(conjure::ecs::system_phase::TRANSFORM, frame);
            }

            /// <summary>
            /// The world matrix the pass wrote, or the identity.
            /// </summary>
            [[nodiscard]] conjure::math::mat4 world_matrix(conjure::ecs::entity target) const {
                const conjure::scene::world_transform *value = world.get<conjure::scene::world_transform>(target);
                return value == nullptr ? conjure::math::mat4(1.0f) : value->matrix;
            }

            [[nodiscard]] conjure::math::mat4 render_matrix(conjure::ecs::entity target) const {
                const conjure::scene::render_world_transform *value =
                  world.get<conjure::scene::render_world_transform>(target);
                return value == nullptr ? conjure::math::mat4(1.0f) : value->matrix;
            }

            /// <summary>
            /// Adds a binary blob to the memory source.
            /// </summary>
            void add_binary_blob(const conjure::assets::asset_id &id, std::string_view content) {
                conjure::assets::cooked_blob_header header = {};
                header.type = conjure::assets::asset_type_traits<conjure::assets::binary_asset>::id();
                header.typeVersion = 1;
                header.importerVersion = 1;
                header.id = id;
                const std::byte *begin = reinterpret_cast<const std::byte *>(content.data());
                std::vector<std::byte> payload(begin, begin + content.size());
                source->add(conjure::assets::make_cooked_blob(header, {}, std::move(payload)));
            }

        private:
            [[nodiscard]] conjure::assets::asset_manager_desc make_asset_desc() {
                conjure::assets::asset_manager_desc desc = {};
                desc.jobs = &assetJobs;
                std::unique_ptr<conjure::assets::memory_asset_source> owned =
                  std::make_unique<conjure::assets::memory_asset_source>();
                source = owned.get();
                desc.source = std::move(owned);
                return desc;
            }

            [[nodiscard]] static conjure::ecs::scheduler_desc make_scheduler_desc() noexcept {
                conjure::ecs::scheduler_desc desc = {};
                desc.parallelSystems = false;
                desc.parallelChunks = true;
                return desc;
            }
    };

    /// <summary>
    /// A local transform with the given position.
    /// </summary>
    [[nodiscard]] inline conjure::scene::local_transform at(conjure::f32 x, conjure::f32 y, conjure::f32 z) noexcept {
        conjure::scene::local_transform value = {};
        value.position = conjure::math::vec3(x, y, z);
        return value;
    }

    /// <summary>
    /// A world-space position read out of a matrix.
    /// </summary>
    [[nodiscard]] inline conjure::math::vec3 translation_of(const conjure::math::mat4 &matrix) noexcept {
        return conjure::math::vec3(matrix[3]);
    }
}
