#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_time.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/key.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "engine/subsystems/scripting_subsystem.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/contact_events.h"
#endif

#include <memory>
#include <vector>

// scripting_subsystem (docs/design/06_engine_loop.md 3.4 steps 14 and 17, R27, R29): the runtime over the
// null host, the frame block written every frame from engine_time and the input snapshot, the contact to
// collision conversion, and the error paths of a missing runtime.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] scripting_subsystem *find_scripting_subsystem(application &app) {
        for (const std::unique_ptr<engine_subsystem> &subsystem : app.subsystems()) {
            if (subsystem->name() == "scripting") {
                return dynamic_cast<scripting_subsystem *>(subsystem.get());
            }
        }
        return nullptr;
    }

    [[nodiscard]] engine_config null_host_config() {
        engine_config config = engine_test::inline_config();
        config.scripting.enabled = true;
        config.scripting.required = true;
        config.scripting.useNullHost = true;
        config.scripting.hotReload = false;
        return config;
    }
}

TEST_SUITE("engine/scripting_subsystem") {

    TEST_CASE("scripting_subsystem - the null host runtime initialises and writes the frame block every frame") {
        log_capture logs = {};
        application app(engine_test::make_desc(null_host_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().scripting != nullptr);
        CHECK(app.context().scripting->status() == scripting::script_host_status::READY);
        CHECK(app.context().scripting->mode() == scripting::script_mode::PLAY);
        CHECK(app.context().components->find(core::string_id("conjure.scripting.script_component")).is_valid());
        scripting_subsystem *subsystem = find_scripting_subsystem(app);
        REQUIRE(subsystem != nullptr);
        CHECK(subsystem->is_initialised());

        const u32 systemsBefore = app.context().scheduler->system_count();
        CHECK(systemsBefore >= 6);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(subsystem->frame_blocks_written() == 2);
        const conjure_frame_block &block = app.context().scripting->frame_block();
        CHECK(block.frame_index == 2);
        CHECK(block.delta_time == app.time().deltaTime);
        CHECK(block.fixed_delta_time == app.time().fixedDeltaTime);
        CHECK(block.time_scale == 1.0f);
        CHECK(block.in_play_mode == 1);
        app.shutdown();
        CHECK(app.context().scripting == nullptr);
    }

    TEST_CASE("scripting_subsystem - the editor starts the runtime in EDIT mode") {
        engine_config config = null_host_config();
        config.editor = true;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().scripting != nullptr);
        CHECK(app.context().scripting->mode() == scripting::script_mode::EDIT);
        app.shutdown();
    }

    TEST_CASE("scripting_subsystem - a missing game assembly is fatal when required and a warning otherwise") {
        log_capture logs = {};
        logs.allow_errors();
        engine_config config = null_host_config();
        config.scripting.gameAssembly = "Scripts/bin/Missing.dll";
        application strict(engine_test::make_desc(config));
        CONJURE_CHECK_ERROR(strict.initialise(), engine_error::SCRIPT_ASSEMBLY_LOAD_FAILED);
        CHECK(strict.state() == application_state::CREATED);

        config.scripting.required = false;
        application lenient(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(lenient.initialise());
        CHECK(lenient.context().scripting != nullptr);
        CHECK(logs.contains(core::log_level::WARN, "Missing.dll"));
        lenient.shutdown();
    }

    TEST_CASE("scripting_subsystem - write_frame_block copies time and input") {
        conjure_frame_block block = {};
        engine_time time = {};
        time.time = 12.5;
        time.unscaledTime = 13.0;
        time.deltaTime = 0.02f;
        time.fixedDeltaTime = 0.01f;
        time.timeScale = 0.5f;
        time.frameIndex = 77;
        time.fixedStepIndex = 154;
        platform::input_state input = {};
        input.keysDown.words[1] = 0x5;
        input.keysPressed.words[0] = 0x8;
        input.mousePosition = math::vec2(3.0f, 4.0f);
        input.mouseDelta = math::vec2(-1.0f, 2.0f);
        input.scrollDelta = math::vec2(0.0f, 1.0f);
        input.mouseButtonsDown = 1;
        input.mouseButtonsPressed = 1;
        input.gamepads[2].connected = true;
        input.gamepads[2].buttonsDown = 0x10;
        input.gamepads[2].axes[3] = 0.75f;

        scripting_subsystem::write_frame_block(block, time, input);
        CHECK(block.time_seconds == 12.5);
        CHECK(block.unscaled_time_seconds == 13.0);
        CHECK(block.delta_time == 0.02f);
        CHECK(block.fixed_delta_time == 0.01f);
        CHECK(block.time_scale == 0.5f);
        CHECK(block.frame_index == 77);
        CHECK(block.fixed_step_index == 154);
        CHECK(block.input.keys_down[1] == 0x5);
        CHECK(block.input.keys_pressed[0] == 0x8);
        CHECK(block.input.keys_released[0] == 0);
        CHECK(block.input.mouse_x == 3.0f);
        CHECK(block.input.mouse_y == 4.0f);
        CHECK(block.input.mouse_dx == -1.0f);
        CHECK(block.input.mouse_dy == 2.0f);
        CHECK(block.input.scroll_y == 1.0f);
        CHECK(block.input.mouse_buttons_down == 1);
        CHECK(block.input.gamepads[2].connected == 1);
        CHECK(block.input.gamepads[2].buttons_down == 0x10);
        CHECK(block.input.gamepads[2].axes[3] == 0.75f);
        CHECK(block.input.gamepads[0].connected == 0);
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("scripting_subsystem - convert_contacts maps the four kinds and drops STAY") {
        std::vector<physics::contact_event> contacts(5);
        contacts[0].kind = physics::contact_kind::ENTER;
        contacts[0].a.index = 1;
        contacts[0].b.index = 2;
        contacts[0].impulse = 3.0f;
        contacts[0].point = math::vec3(1.0f, 2.0f, 3.0f);
        contacts[1].kind = physics::contact_kind::STAY;
        contacts[2].kind = physics::contact_kind::EXIT;
        contacts[3].kind = physics::contact_kind::TRIGGER_ENTER;
        contacts[4].kind = physics::contact_kind::TRIGGER_EXIT;
        std::vector<scripting::collision_event> events = {};
        scripting_subsystem::convert_contacts(contacts, events);
        REQUIRE(events.size() == 4);
        CHECK(events[0].kind == scripting::collision_kind::COLLISION_ENTER);
        CHECK(events[0].a.index == 1);
        CHECK(events[0].b.index == 2);
        CHECK(events[0].impulse == 3.0f);
        CHECK(events[0].point == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(events[1].kind == scripting::collision_kind::COLLISION_EXIT);
        CHECK(events[2].kind == scripting::collision_kind::TRIGGER_ENTER);
        CHECK(events[3].kind == scripting::collision_kind::TRIGGER_EXIT);
    }
#endif
}
