#include "engine/subsystems/scene_subsystem.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/log_category.h"
#include "conjure/engine/project.h"

#include "conjure/core/log.h"
#include "conjure/core/string_id.h"
#include "conjure/scene/register.h"
#include "conjure/scene/transform_system.h"

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

scene_subsystem::scene_subsystem(engine_services &services) :
    mServices(&services) {}

scene_subsystem::~scene_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view scene_subsystem::name() const noexcept {
    return "scene";
}

core::result<void> scene_subsystem::startup(engine_context &context) {
    CONJURE_TRY(scene::register_components(*mServices->components));
    mServices->scenes = std::make_unique<scene::scene_manager>(*mServices->world, *mServices->assetManager);
    CONJURE_TRY(scene::register_asset_types(
      mServices->assetManager->types(),
      mServices->importers,
      *mServices->components
    ));

    mServices->scenes->transforms().set_interpolation_enabled(context.config->time.interpolatePhysics);

    if (context.project != nullptr) {
        const std::vector<std::string> &layers = context.project->description().layers;
        for (usize index = 0; index < layers.size() && index < scene::layer_table::LAYER_COUNT; ++index) {
            if (!layers[index].empty()) {
                mServices->scenes->layers().set_name(static_cast<u8>(index), core::string_id::intern(layers[index]));
            }
        }
    }

    context.scenes = mServices->scenes.get();
    return core::result<void>::success();
}

void scene_subsystem::register_systems(ecs::scheduler &scheduler, engine_context &context) {
    CONJURE_UNUSED(context);
    const core::result<void> registered = scene::register_systems(scheduler, *mServices->scenes);
    if (!registered.has_value()) {
        CONJURE_LOG_ERROR(LOG_ENGINE, "scene: systems could not be registered: {}", registered.get_error().to_string());
    }
}

void scene_subsystem::shutdown(engine_context &context) {
    context.scenes = nullptr;
    mServices->scenes.reset();
}
