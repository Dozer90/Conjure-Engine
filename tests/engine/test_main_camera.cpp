#include "conjure/testing/doctest.h"

#include "conjure/engine/main_camera.h"

#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "engine_test_support.h"

    #include "conjure/assets/asset_manager.h"
    #include "conjure/assets/asset_source.h"
    #include "conjure/core/math.h"
    #include "conjure/ecs/scheduler.h"
    #include "conjure/ecs/system_phase.h"
    #include "conjure/engine/engine_config.h"
    #include "conjure/engine/engine_context.h"
    #include "conjure/renderer/components.h"
    #include "conjure/renderer/render_scene.h"
    #include "conjure/rhi/device.h"
    #include "conjure/rhi/native_window.h"
    #include "conjure/rhi/swapchain.h"
    #include "engine/engine_services.h"
    #include "engine/subsystems/renderer_components.h"
    #include "engine/subsystems/renderer_subsystem.h"

    #include <array>
    #include <memory>
    #include <string>
    #include <string_view>
    #include <utility>
    #include <vector>
#endif

// main_camera.h: find_main_camera names the camera extraction shows first (an enabled camera with a render world
// transform; a main one first, then the highest priority, then world order), and the null entity when there is
// none or the world never registered the components. A matrix pins it to the engine's extraction: the first view
// renderer_subsystem submits is always the camera find_main_camera names.

using namespace conjure;
using namespace conjure::engine;

namespace {

    /// <summary>
    /// A world over the scene components (and, with the renderer, the camera component when 'withCamera').
    /// </summary>
    struct camera_world {
        ecs::component_registry registry = {};
        ecs::world world;           // no default: built on the registry in the constructor

        explicit camera_world(bool withCamera) :
            world((register_all(registry, withCamera), registry)) {}

        static void register_all(ecs::component_registry &registry, bool withCamera) {
            CONJURE_REQUIRE_OK(scene::register_components(registry));
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
            if (withCamera) {
                CONJURE_REQUIRE_OK(renderer::register_components(core::reflect::registry()));
                CONJURE_REQUIRE_OK(registry.register_component<renderer::camera_component>());
            }
#else
            static_cast<void>(withCamera);
#endif
        }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        /// <summary>
        /// An entity with a camera_component and a render_world_transform.
        /// </summary>
        [[nodiscard]] ecs::entity add_camera(bool isMain, i32 priority) {
            const core::result<ecs::entity> created = world.create_entity();
            CONJURE_REQUIRE_OK(created);
            renderer::camera_component camera = {};
            camera.isMain = isMain;
            camera.priority = priority;
            CONJURE_REQUIRE_OK(world.add<renderer::camera_component>(created.value(), camera));
            CONJURE_REQUIRE_OK(world.add<scene::render_world_transform>(created.value()));
            return created.value();
        }
#endif
    };

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    /// <summary>
    /// The engine's renderer subsystem started as the application starts it, over the null device with a null
    /// swapchain so that its extraction submits views (without a swapchain it only counts them). Each world it
    /// extracts is built on its component registry, which the startup filled with the renderer components.
    /// </summary>
    class extraction_harness {
        public:
            engine_services services = {};
            engine_config config = engine_test::inline_config();
            engine_context context = {};
            std::unique_ptr<renderer_subsystem> subsystem = nullptr;

        public:
            extraction_harness() {
                engine_test::enable_null_rendering(config);
                services.components = std::make_unique<ecs::component_registry>();
                CONJURE_REQUIRE_OK(scene::register_components(*services.components));
                assets::asset_manager_desc assetDesc = {};
                assetDesc.jobs = &services.jobs;
                assetDesc.source = std::make_unique<assets::memory_asset_source>();
                services.assetManager = std::make_unique<assets::asset_manager>(std::move(assetDesc));

                rhi::device_desc deviceDesc = {};
                deviceDesc.backend = rhi::graphics_backend::NULL_DEVICE;
                core::result<std::unique_ptr<rhi::device>> device = rhi::device::create(deviceDesc);
                CONJURE_REQUIRE_OK(device);
                services.device = std::move(device).value();
                rhi::swapchain_desc chainDesc = {};
                chainDesc.width = 1280;
                chainDesc.height = 720;
                chainDesc.vsync = false;
                core::result<std::unique_ptr<rhi::swapchain>> chain =
                  rhi::swapchain::create(*services.device, rhi::native_window{}, chainDesc);
                CONJURE_REQUIRE_OK(chain);
                services.swapchain = std::move(chain).value();

                context.config = &config;
                context.components = services.components.get();
                context.assets = services.assetManager.get();
                context.device = services.device.get();
                subsystem = std::make_unique<renderer_subsystem>(services);
                CONJURE_REQUIRE_OK(subsystem->startup(context));
                REQUIRE(services.renderer != nullptr);
            }

            ~extraction_harness() {
                if (subsystem != nullptr) {
                    subsystem->shutdown(context);
                }
            }

            extraction_harness(const extraction_harness &) = delete;
            extraction_harness &operator=(const extraction_harness &) = delete;

        public:
            /// <summary>
            /// The camera position of every view one run of engine.render_extract submits for 'world', in order.
            /// </summary>
            [[nodiscard]] std::vector<math::vec3> extracted_view_positions(ecs::world &world) {
                ecs::scheduler_desc schedulerDesc = {};
                schedulerDesc.parallelSystems = false;
                schedulerDesc.parallelChunks = false;
                ecs::scheduler scheduler(world, nullptr, schedulerDesc);
                subsystem->register_systems(scheduler, context);
                CONJURE_REQUIRE_OK(scheduler.finalise());
                renderer::render_scene target = {};
                services.extractTarget = &target;
                scheduler.run_phase(ecs::system_phase::EXTRACT, ecs::frame_info{});
                services.extractTarget = nullptr;
                std::vector<math::vec3> positions = {};
                for (const renderer::render_view &view : target.views) {
                    positions.push_back(view.cameraPosition);
                }
                return positions;
            }
    };

    /// <summary>
    /// What a camera of the matrix is: neither tag, disabled itself or under a disabled ancestor.
    /// </summary>
    enum class camera_state : u8 {
        ACTIVE,
        DISABLED,
        INACTIVE
    };

    /// <summary>
    /// One camera of the matrix.
    /// </summary>
    struct camera_case {
        bool isMain = false;
        i32 priority = 0;
        camera_state state = camera_state::ACTIVE;
    };

    constexpr u32 MATRIX_CAMERAS = 3;
    constexpr u32 STATES_PER_CAMERA = 12;     // isMain x priority {0, 1} x the three states

    /// <summary>
    /// The camera of one digit (0..11) of a matrix combination: bit 0 isMain, bit 1 the priority, the rest the
    /// state.
    /// </summary>
    [[nodiscard]] camera_case camera_case_of(u32 digit) noexcept {
        camera_case value = {};
        value.isMain = (digit & 1u) != 0;
        value.priority = static_cast<i32>((digit >> 1) & 1u);
        value.state = static_cast<camera_state>(digit / 4u);
        return value;
    }

    [[nodiscard]] std::string describe(const std::array<camera_case, MATRIX_CAMERAS> &cameras) {
        constexpr std::array<std::string_view, 3> STATE_NAMES = {"active", "disabled", "inactive"};
        std::string text = {};
        for (const camera_case &camera : cameras) {
            text += camera.isMain ? "main" : "plain";
            text += "/p" + std::to_string(camera.priority) + "/";
            text += STATE_NAMES[static_cast<usize>(camera.state)];
            text += " ";
        }
        return text;
    }
#endif
}

TEST_SUITE("engine/main_camera") {

    TEST_CASE("find_main_camera - a world without cameras has none") {
        SUBCASE("the camera component is not registered") {
            camera_world fixture(false);
            const core::result<ecs::entity> plain = fixture.world.create_entity();
            CONJURE_REQUIRE_OK(plain);
            CHECK(find_main_camera(fixture.world).is_null());
        }
        SUBCASE("the camera component is registered but unused") {
            camera_world fixture(true);
            CHECK(find_main_camera(fixture.world).is_null());
        }
    }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    TEST_CASE("find_main_camera - a main camera comes before any other, then priority, then world order") {
        camera_world fixture(true);
        SUBCASE("a main camera beats a higher priority one that is not main") {
            const ecs::entity other = fixture.add_camera(false, 100);
            const ecs::entity main = fixture.add_camera(true, 0);
            CHECK(find_main_camera(fixture.world) == main);
            CHECK(find_main_camera(fixture.world) != other);
        }
        SUBCASE("among main cameras the highest priority wins") {
            const ecs::entity low = fixture.add_camera(true, 1);
            const ecs::entity high = fixture.add_camera(true, 5);
            CHECK(find_main_camera(fixture.world) == high);
            CHECK(find_main_camera(fixture.world) != low);
        }
        SUBCASE("without a main camera the highest priority wins") {
            const ecs::entity low = fixture.add_camera(false, -3);
            const ecs::entity high = fixture.add_camera(false, 2);
            CHECK(find_main_camera(fixture.world) == high);
            CHECK(find_main_camera(fixture.world) != low);
        }
        SUBCASE("equal cameras: the first in world order") {
            const ecs::entity first = fixture.add_camera(true, 0);
            const ecs::entity second = fixture.add_camera(true, 0);
            CHECK(find_main_camera(fixture.world) == first);
            CHECK(find_main_camera(fixture.world) != second);
        }
    }

    TEST_CASE("find_main_camera - disabled, inactive and untransformed cameras are skipped") {
        camera_world fixture(true);
        const ecs::entity fallback = fixture.add_camera(false, 0);
        SUBCASE("disabled") {
            const ecs::entity skipped = fixture.add_camera(true, 10);
            CONJURE_REQUIRE_OK(fixture.world.add<scene::disabled>(skipped));
            CHECK(find_main_camera(fixture.world) == fallback);
        }
        SUBCASE("inactive (a disabled ancestor)") {
            const ecs::entity skipped = fixture.add_camera(true, 10);
            CONJURE_REQUIRE_OK(fixture.world.add<scene::inactive>(skipped));
            CHECK(find_main_camera(fixture.world) == fallback);
        }
        SUBCASE("no render world transform") {
            const core::result<ecs::entity> skipped = fixture.world.create_entity();
            CONJURE_REQUIRE_OK(skipped);
            renderer::camera_component camera = {};
            camera.priority = 10;
            CONJURE_REQUIRE_OK(fixture.world.add<renderer::camera_component>(skipped.value(), camera));
            CHECK(find_main_camera(fixture.world) == fallback);
        }
        SUBCASE("every camera skipped") {
            CONJURE_REQUIRE_OK(fixture.world.add<scene::disabled>(fallback));
            CHECK(find_main_camera(fixture.world).is_null());
        }
    }

    TEST_CASE("find_main_camera - equals the first view extraction submits over isMain, priority and activity") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        // The second variant moves the first camera to a later archetype: a scene::local_transform added after the
        // camera and its render world transform (nothing in the extraction reads it). With every camera active,
        // world order is then cameras 1, 2, 0, unlike creation order, and ties compare across archetypes.
        bool firstInLaterArchetype = false;
        SUBCASE("every active camera in one archetype") {
            firstInLaterArchetype = false;
        }
        SUBCASE("the first camera in a later archetype than the others") {
            firstInLaterArchetype = true;
        }
        CAPTURE(firstInLaterArchetype);
        extraction_harness harness = {};
        u32 matrixSize = 1;
        for (u32 camera = 0; camera < MATRIX_CAMERAS; ++camera) {
            matrixSize *= STATES_PER_CAMERA;
        }
        u32 withoutView = 0;
        for (u32 combination = 0; combination < matrixSize; ++combination) {
            std::array<camera_case, MATRIX_CAMERAS> cameras = {};
            u32 remaining = combination;
            for (camera_case &camera : cameras) {
                camera = camera_case_of(remaining % STATES_PER_CAMERA);
                remaining /= STATES_PER_CAMERA;
            }
            CAPTURE(combination);
            CAPTURE(describe(cameras));

            // Camera i sits at x = i + 1, so a view's camera position names the camera it came from.
            ecs::world world(*harness.services.components);
            for (u32 index = 0; index < MATRIX_CAMERAS; ++index) {
                const camera_case &camera = cameras[index];
                const core::result<ecs::entity> created = world.create_entity();
                CONJURE_REQUIRE_OK(created);
                renderer::camera_component component = {};
                component.isMain = camera.isMain;
                component.priority = camera.priority;
                CONJURE_REQUIRE_OK(world.add<renderer::camera_component>(created.value(), component));
                scene::render_world_transform transform = {};
                const math::vec3 position = math::vec3(static_cast<f32>(index + 1), 0.0f, 0.0f);
                transform.matrix = glm::translate(math::mat4(1.0f), position);
                CONJURE_REQUIRE_OK(world.add<scene::render_world_transform>(created.value(), transform));
                if (firstInLaterArchetype && index == 0) {
                    CONJURE_REQUIRE_OK(world.add<scene::local_transform>(created.value()));
                }
                if (camera.state == camera_state::DISABLED) {
                    CONJURE_REQUIRE_OK(world.add<scene::disabled>(created.value()));
                }
                if (camera.state == camera_state::INACTIVE) {
                    CONJURE_REQUIRE_OK(world.add<scene::inactive>(created.value()));
                }
            }

            const std::vector<math::vec3> views = harness.extracted_view_positions(world);
            const ecs::entity found = find_main_camera(world);
            if (views.empty()) {
                ++withoutView;
                CHECK(found.is_null());
                continue;
            }
            REQUIRE_FALSE(found.is_null());
            const scene::render_world_transform *foundTransform = world.get<scene::render_world_transform>(found);
            REQUIRE(foundTransform != nullptr);
            CHECK(math::vec3(foundTransform->matrix[3]) == views.front());
        }
        // No view at all when every camera is disabled or inactive: 8 of the 12 states per camera.
        CHECK(withoutView == 8u * 8u * 8u);
    }
#endif
}
