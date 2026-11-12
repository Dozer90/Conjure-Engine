#include "engine/subsystems/scripting_subsystem.h"

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING

#include "engine/module_consumers/scripting_module_consumer.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/log_category.h"
#include "conjure/engine/project.h"

#include "conjure/core/log.h"
#include "conjure/platform/input.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_system.h"
#include "conjure/scripting/scripting_error.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

scripting_subsystem::scripting_subsystem(engine_services &services) :
    mServices(&services) {}

scripting_subsystem::~scripting_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view scripting_subsystem::name() const noexcept {
    return "scripting";
}

core::result<void> scripting_subsystem::startup(engine_context &context) {
    const engine_config &config = *context.config;
    mInitialised = false;
    mFrameBlocksWritten = 0;

    if (!config.scripting.enabled) {
        CONJURE_LOG_INFO(LOG_ENGINE, "scripting: disabled by configuration");
        return core::result<void>::success();
    }

    const bool useNullHost = config.scripting.useNullHost;
    if (!useNullHost && !scripting::is_runtime_available()) {
        if (config.scripting.required) {
            return make_error(engine_error::SCRIPT_RUNTIME_NOT_FOUND, "no .NET runtime was found (hostfxr)");
        }
        CONJURE_LOG_WARN(LOG_ENGINE, "scripting: no .NET runtime found; scripting disabled");
        return core::result<void>::success();
    }

    const scripting::script_backend backend =
      useNullHost ? scripting::script_backend::NULL_HOST : scripting::script_backend::DOTNET;
    mServices->scripting = std::make_unique<scripting::script_runtime>(scripting::create_script_host(backend));
    CONJURE_TRY(scripting::register_script_component(*mServices->components));
    mServices->scripting->attach_registry(mServices->components.get());
    mServices->scripting->attach_world(mServices->world.get());

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    if (mServices->physics != nullptr) {
        physics::physics_world *physicsWorld = mServices->physics.get();
        std::vector<scripting::collision_event> *buffer = &mCollisions;
        using collision_span = std::span<const scripting::collision_event>;
        mServices->scripting->set_collision_source([physicsWorld, buffer]() -> collision_span {
            convert_contacts(physicsWorld->contact_events(), *buffer);
            return *buffer;
        });
    }
#endif

    // OnDestroy must see live entities: destroy every instance before the scene tears its entities down (R29).
    scripting::script_runtime *runtime = mServices->scripting.get();
    mUnloadSubscription = mServices->scenes->on_before_unload().subscribe(
      [runtime](scene::scene_id, std::span<const ecs::entity>) {
          if (runtime->status() == scripting::script_host_status::READY) {
              runtime->destroy_all(true);
          }
      }
    );

    context.scripting = mServices->scripting.get();
    CONJURE_LOG_INFO(LOG_ENGINE, "scripting: runtime constructed ({} host)", useNullHost ? "null" : ".NET");
    return core::result<void>::success();
}

core::result<void> scripting_subsystem::post_startup(engine_context &context) {
    if (mServices->scripting == nullptr) {
        return core::result<void>::success();
    }
    const engine_config &config = *context.config;

    scripting::script_runtime_config runtimeConfig = {};
    runtimeConfig.host.backend =
      config.scripting.useNullHost ? scripting::script_backend::NULL_HOST : scripting::script_backend::DOTNET;
    runtimeConfig.host.managedDirectory = config.scripting.managedDirectory.empty()
      ? mServices->engineDirectory / "managed"
      : config.scripting.managedDirectory;
    runtimeConfig.initialMode = config.editor ? scripting::script_mode::EDIT : scripting::script_mode::PLAY;

    std::filesystem::path assemblyPath = config.scripting.gameAssembly;
    if (!assemblyPath.empty() && !assemblyPath.is_absolute() && context.project != nullptr) {
        assemblyPath = context.project->root_directory() / assemblyPath;
    }

    const core::result<void> initialised = mServices->scripting->initialize(runtimeConfig);
    if (!initialised.has_value()) {
        const core::error &error = initialised.get_error();
        if (config.scripting.required) {
            // A missing .NET runtime is SCRIPT_RUNTIME_NOT_FOUND (R27); a runtime that exists but failed its
            // handshake or version check is reported as what it is, with the original message.
            const engine_error code = error.is(scripting::script_error::RUNTIME_NOT_FOUND)
              ? engine_error::SCRIPT_RUNTIME_NOT_FOUND
              : engine_error::SCRIPT_RUNTIME_INIT_FAILED;
            return make_error(code, std::format("script runtime initialisation failed: {}", error.to_string()));
        }
        CONJURE_LOG_WARN(LOG_ENGINE, "scripting: runtime unavailable, scripting disabled: {}", error.to_string());
        disable(context);
        return core::result<void>::success();
    }
    mInitialised = true;

    if (!assemblyPath.empty()) {
        const core::result<void> loaded = mServices->scripting->load_game_assembly(assemblyPath);
        if (!loaded.has_value()) {
            if (config.scripting.required) {
                return make_error(
                  engine_error::SCRIPT_ASSEMBLY_LOAD_FAILED,
                  std::format(
                    "game assembly '{}' did not load: {}",
                    assemblyPath.generic_string(),
                    loaded.get_error().to_string()
                  )
                );
            }
            CONJURE_LOG_WARN(
              LOG_ENGINE,
              "scripting: game assembly '{}' did not load: {}",
              assemblyPath.generic_string(),
              loaded.get_error().to_string()
            );
        } else {
#if !CONJURE_DISTRIBUTION
            if (config.scripting.hotReload) {
                mServices->scriptReload =
                  std::make_unique<scripting::hot_reload_controller>(*mServices->scripting, assemblyPath);
            }
#endif
        }
    }
    return core::result<void>::success();
}

void scripting_subsystem::register_systems(ecs::scheduler &scheduler, engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->scripting == nullptr || !mInitialised) {
        return;
    }
    const core::result<void> registered =
      scripting::register_script_systems(scheduler, *mServices->scripting, mServices->scriptReload.get());
    if (!registered.has_value()) {
        CONJURE_LOG_ERROR(
          LOG_ENGINE,
          "scripting: systems could not be registered: {}",
          registered.get_error().to_string()
        );
    }
}

void scripting_subsystem::begin_frame(engine_context &context) {
    if (mServices->scripting == nullptr || context.time == nullptr) {
        return;
    }
    static const platform::input_state EMPTY_INPUT = {};
    const platform::input_state &input = mServices->input != nullptr ? mServices->input->snapshot() : EMPTY_INPUT;
    write_frame_block(mServices->scripting->frame_block(), *context.time, input);
    ++mFrameBlocksWritten;
}

void scripting_subsystem::shutdown(engine_context &context) {
    context.scripting = nullptr;
    mUnloadSubscription = {};
    mServices->scriptReload.reset();
    if (mServices->scripting != nullptr) {
        mServices->scripting->shutdown();
    }
    // The modules subsystem shuts down first and has usually destroyed the consumer by now.
    if (mServices->scriptingConsumer != nullptr) {
        mServices->scriptingConsumer->set_runtime(nullptr);
    }
    mServices->scripting.reset();
    mCollisions.clear();
    mInitialised = false;
}

void scripting_subsystem::write_frame_block(
  conjure_frame_block &block,
  const engine_time &time,
  const platform::input_state &input
) noexcept {
    block.time_seconds = time.time;
    block.unscaled_time_seconds = time.unscaledTime;
    block.delta_time = time.deltaTime;
    block.fixed_delta_time = time.fixedDeltaTime;
    block.time_scale = time.timeScale;
    block.frame_index = time.frameIndex;
    block.fixed_step_index = static_cast<uint32_t>(time.fixedStepIndex);

    conjure_input_block &target = block.input;
    static_assert(sizeof(target.keys_down) == sizeof(input.keysDown.words), "key bitset layout is pinned (09 13e)");
    std::memcpy(target.keys_down, input.keysDown.words.data(), sizeof(target.keys_down));
    std::memcpy(target.keys_pressed, input.keysPressed.words.data(), sizeof(target.keys_pressed));
    std::memcpy(target.keys_released, input.keysReleased.words.data(), sizeof(target.keys_released));
    target.mouse_x = input.mousePosition.x;
    target.mouse_y = input.mousePosition.y;
    target.mouse_dx = input.mouseDelta.x;
    target.mouse_dy = input.mouseDelta.y;
    target.scroll_x = input.scrollDelta.x;
    target.scroll_y = input.scrollDelta.y;
    target.mouse_buttons_down = input.mouseButtonsDown;
    target.mouse_buttons_pressed = input.mouseButtonsPressed;
    target.mouse_buttons_released = input.mouseButtonsReleased;
    for (usize pad = 0; pad < platform::MAX_GAMEPADS; ++pad) {
        const platform::gamepad_state &source = input.gamepads[pad];
        conjure_gamepad_state &destination = target.gamepads[pad];
        destination.buttons_down = source.buttonsDown;
        destination.buttons_pressed = source.buttonsPressed;
        destination.buttons_released = source.buttonsReleased;
        for (usize axis = 0; axis < platform::GAMEPAD_AXIS_COUNT; ++axis) {
            destination.axes[axis] = source.axes[axis];
        }
        destination.connected = source.connected ? 1 : 0;
    }
}

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
void scripting_subsystem::convert_contacts(
  std::span<const physics::contact_event> contacts,
  std::vector<scripting::collision_event> &out
) {
    out.clear();
    for (const physics::contact_event &contact : contacts) {
        scripting::collision_event event = {};
        switch (contact.kind) {
            case physics::contact_kind::ENTER:
                event.kind = scripting::collision_kind::COLLISION_ENTER;
                break;
            case physics::contact_kind::EXIT:
                event.kind = scripting::collision_kind::COLLISION_EXIT;
                break;
            case physics::contact_kind::TRIGGER_ENTER:
                event.kind = scripting::collision_kind::TRIGGER_ENTER;
                break;
            case physics::contact_kind::TRIGGER_EXIT:
                event.kind = scripting::collision_kind::TRIGGER_EXIT;
                break;
            case physics::contact_kind::STAY:
                continue;
        }
        event.a = contact.a;
        event.b = contact.b;
        event.point = contact.point;
        event.normal = contact.normal;
        event.impulse = contact.impulse;
        out.push_back(event);
    }
}
#endif


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void scripting_subsystem::disable(engine_context &context) {
    context.scripting = nullptr;
    mUnloadSubscription = {};
    if (mServices->scripting != nullptr) {
        mServices->scripting->shutdown();
    }
    // The module consumer was built over this runtime at step 15; a later managed reload must not reach it.
    if (mServices->scriptingConsumer != nullptr) {
        mServices->scriptingConsumer->set_runtime(nullptr);
    }
    mServices->scripting.reset();
    mInitialised = false;
}

#endif
