#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/result.h"

// Startup step 10 (docs/design/06_engine_loop.md 3.4, 09 section 3 "scene_subsystem"): the scene components
// (scene::register_components), the scene manager over the world and the asset manager, the scene and
// prefab asset types with their importers (before the step 17 scan), time.interpolatePhysics ->
// transform_system::set_interpolation_enabled and the layer table from project.json. Step 18 adds the four
// scene systems; shutdown step 2 (unload_all) is driven by the application because it must run before any
// subsystem stops.

namespace conjure::engine {

    class scene_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;

        public:
            explicit scene_subsystem(engine_services &services);
            ~scene_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void register_systems(ecs::scheduler &scheduler, engine_context &context) override;
            void shutdown(engine_context &context) override;
    };
}
