#include "engine/subsystems/physics_subsystem.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/log_category.h"
#include "conjure/engine/project.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/physics/register.h"
#include "conjure/scene/scene_manager.h"

#include <format>
#include <span>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] core::error physics_error(std::string_view what, const core::error &cause) {
        return core::make_error(engine_error::PHYSICS_INIT_FAILED, std::format("{}: {}", what, cause.to_string()));
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

physics_subsystem::physics_subsystem(engine_services &services) :
    mServices(&services) {}

physics_subsystem::~physics_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view physics_subsystem::name() const noexcept {
    return "physics";
}

core::result<void> physics_subsystem::startup(engine_context &context) {
    const engine_config &config = *context.config;
    mReloadsForwarded = 0;

    CONJURE_TRY(physics::register_components(*mServices->components));
    CONJURE_TRY(physics::register_asset_types(mServices->assetManager->types(), mServices->importers));

    if (!config.physics.enabled) {
        CONJURE_LOG_INFO(LOG_ENGINE, "physics: disabled by configuration");
        return core::result<void>::success();
    }

    CONJURE_TRY_ASSIGN(const physics::physics_settings settings, make_physics_settings(config, context.project));
    mServices->physics = std::make_unique<physics::physics_world>(settings, mServices->jobs);
    const core::result<void> attached = mServices->physics->attach(*mServices->world);
    if (!attached.has_value()) {
        const core::error error = attached.get_error();
        mServices->physics.reset();
        return physics_error("attaching the physics world", error);
    }

    physics::physics_world *world = mServices->physics.get();
    u32 *counter = &mReloadsForwarded;
    mSubscription = mServices->assetManager->subscribe([world, counter](const assets::asset_event &event) {
        if (event.kind == assets::asset_event_kind::RELOADED) {
            world->on_asset_reloaded(event.id);
            ++(*counter);
        }
    });

    // The unloaded bodies are destroyed inside unload; their shapes are collected at the next end_frame.
    mCollectShapes = false;
    if (mServices->scenes != nullptr) {
        bool *pending = &mCollectShapes;
        mUnloadSubscription = mServices->scenes->on_before_unload().subscribe(
          [pending](scene::scene_id, std::span<const ecs::entity>) {
              *pending = true;
          }
        );
    }

    context.physics = world;
    CONJURE_LOG_INFO(LOG_ENGINE, "physics: world ready ({} layer(s))", settings.layers.count());
    return core::result<void>::success();
}

void physics_subsystem::register_systems(ecs::scheduler &scheduler, engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->physics == nullptr) {
        return;
    }
    const core::result<void> registered = physics::register_systems(scheduler, *mServices->physics);
    if (!registered.has_value()) {
        CONJURE_LOG_ERROR(
          LOG_ENGINE,
          "physics: systems could not be registered: {}",
          registered.get_error().to_string()
        );
    }
}

void physics_subsystem::end_frame(engine_context &context) {
    CONJURE_UNUSED(context);
    if (!mCollectShapes || mServices->physics == nullptr) {
        return;
    }
    mCollectShapes = false;
    mServices->physics->collect_shapes();
}

void physics_subsystem::shutdown(engine_context &context) {
    context.physics = nullptr;
    mUnloadSubscription = {};
    mCollectShapes = false;
    if (mSubscription.is_valid() && mServices->assetManager != nullptr) {
        mServices->assetManager->unsubscribe(mSubscription);
    }
    mSubscription = {};
    if (mServices->physics != nullptr) {
        mServices->physics->detach();
    }
    mServices->physics.reset();
}

core::result<physics::physics_settings> physics_subsystem::make_physics_settings(
  const engine_config &config,
  const project *loaded
) {
    const core::json::document empty = core::json::document::object();
    core::json::value physicsSection = empty.root();
    core::json::value layersSection = {};
    if (loaded != nullptr) {
        physicsSection = loaded->description().physicsSection.root();
        layersSection = loaded->description().extensions.root()["physics_layers"];
    }

    core::result<physics::physics_settings> parsed =
      physics::physics_settings::from_json(physicsSection, layersSection);
    if (!parsed.has_value()) {
        return physics_error("reading the physics settings", parsed.get_error());
    }
    physics::physics_settings settings = std::move(parsed).value();
    if (!physicsSection.contains("gravity")) {
        settings.gravity = config.physics.gravity;
    }
    if (!physicsSection.contains("max_bodies")) {
        settings.maxBodies = config.physics.maxBodies;
    }

    const core::result<void> valid = settings.validate();
    if (!valid.has_value()) {
        return physics_error("validating the physics settings", valid.get_error());
    }
    return settings;
}

#endif
