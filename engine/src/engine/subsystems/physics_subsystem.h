#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS

#include "conjure/assets/asset_manager.h"
#include "conjure/physics/physics_settings.h"

// Startup step 12 (docs/design/06_engine_loop.md 3.4, 09 R44/R53): physics::physics_settings::from_json over
// the raw project.json "physics" section and "extensions.physics_layers", validate() (PHYSICS_INIT_FAILED),
// the Jolt world over the job system attached to the ECS world, the physics components and the collision
// mesh asset type, and asset RELOADED events forwarded to physics_world::on_asset_reloaded. The components
// are registered even when physics.enabled == false so scenes with bodies still load; the world is not
// created then and the context pointer stays null.

namespace conjure::engine {

    class physics_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;
            assets::asset_subscription mSubscription = {};
            u32 mReloadsForwarded = 0;

        public:
            explicit physics_subsystem(engine_services &services);
            ~physics_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void register_systems(ecs::scheduler &scheduler, engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// RELOADED asset events handed to the physics world so far.
            /// </summary>
            [[nodiscard]] u32 reloads_forwarded() const noexcept {
                return mReloadsForwarded;
            }

            /// <summary>
            /// The settings a configuration and an optional project produce: from_json over the project's raw
            /// section (or an empty object), then the engine summary's gravity when the section lacks one.
            /// </summary>
            [[nodiscard]] static core::result<physics::physics_settings> make_physics_settings(
              const engine_config &config,
              const project *loaded
            );
    };
}

#endif
