#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/core/json/writer.h"
#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/project.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/key.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "engine/subsystems/scripting_subsystem.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
    #include "conjure/physics/contact_events.h"
    #include "conjure/physics/physics_world.h"
#endif

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// scripting_subsystem (docs/design/06_engine_loop.md 3.4 steps 14 and 17, D12, R27, R29): the runtime over the
// null host, the frame block written every frame from engine_time and the input snapshot, the contact to
// collision conversion and its STAY gate, the error paths of a missing runtime, and the headless opt-in: a headless
// run of a scripted project starts the runtime only with --scripting and says so when it does not. Inactive
// scripts sleep (code sweep W3-CS-L4): the subsystem hands the scene's two activity tags to the runtime, so
// scene::set_enabled reaches the scripts of a whole subtree.

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

    constexpr std::string_view HEADLESS_HINT = "unless --scripting is given";
    constexpr std::string_view PLAIN_DISABLED = "scripting: disabled by configuration";

    constexpr std::string_view SLEEPER = "Game.Sleeper";
    constexpr std::string_view BUMPER = "Game.Bumper";

    /// <summary>
    /// The null host behind the application's runtime (null_host_config).
    /// </summary>
    [[nodiscard]] scripting::null_script_host &null_host_of(scripting::script_runtime &runtime) {
        REQUIRE(runtime.host().backend() == scripting::script_backend::NULL_HOST);
        return static_cast<scripting::null_script_host &>(runtime.host());
    }

    /// <summary>
    /// A fake class for the null host with the given callbacks and no fields.
    /// </summary>
    [[nodiscard]] scripting::script_class_info fake_class(
      std::string_view fullName,
      std::initializer_list<scripting::script_callback> callbacks
    ) {
        scripting::script_class_info info = {};
        info.fullName = std::string(fullName);
        info.hash = scripting::script_name_hash(fullName);
        for (const scripting::script_callback callback : callbacks) {
            info.callbackMask = static_cast<u16>(info.callbackMask | scripting::callback_bit(callback));
        }
        return info;
    }

    /// <summary>
    /// Game.Sleeper: UPDATE, ENABLE and DISABLE.
    /// </summary>
    [[nodiscard]] scripting::script_class_info sleeper_class() {
        return fake_class(
          SLEEPER,
          {scripting::script_callback::UPDATE, scripting::script_callback::ENABLE, scripting::script_callback::DISABLE}
        );
    }

    /// <summary>
    /// Adds one script of the class to the entity (default fields); the instance exists after the next flush.
    /// </summary>
    [[nodiscard]] scripting::script_entry_id add_fake_script(
      scripting::script_runtime &runtime,
      ecs::entity entity,
      std::string_view fullName
    ) {
        const scripting::script_entry_id id = runtime.add_script(entity, scripting::script_name_hash(fullName), {});
        REQUIRE(id.is_valid());
        return id;
    }

    /// <summary>
    /// The scene as scene_serializer::save_scene writes it.
    /// </summary>
    [[nodiscard]] std::string saved_scene(const scene::scene &source) {
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene(source, out));
        return out.finish();
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    /// <summary>
    /// A root entity with a box collider and a rigid body at 'position': static ground 20 x 1 x 20, or a dynamic
    /// unit box that never sleeps. The physics subsystem creates the body at the next frame.
    /// </summary>
    [[nodiscard]] ecs::entity spawn_box(
      scene::scene &target,
      std::string_view name,
      const math::vec3 &position,
      bool staticGround
    ) {
        const core::result<ecs::entity> created = target.create_entity(core::string_id(name));
        CONJURE_REQUIRE_OK(created);
        scene::local_transform pose = {};
        pose.position = position;
        CONJURE_REQUIRE_OK(target.set_local_transform(created.value(), pose));
        physics::rigid_body body = {};
        physics::box_collider shape = {};
        if (staticGround) {
            body.motionType = physics::motion_type::STATIC;
            shape.halfExtents = math::vec3(10.0f, 0.5f, 10.0f);
        } else {
            body.flags = physics::rigid_body_flags::NONE;
        }
        CONJURE_REQUIRE_OK(target.world().add<physics::rigid_body>(created.value(), body));
        CONJURE_REQUIRE_OK(target.world().add<physics::box_collider>(created.value(), shape));
        return created.value();
    }

    /// <summary>
    /// The COLLISION_STAY events among 'events'.
    /// </summary>
    [[nodiscard]] usize count_stays(std::span<const scripting::collision_event> events) {
        usize stays = 0;
        for (const scripting::collision_event &event : events) {
            if (event.kind == scripting::collision_kind::COLLISION_STAY) {
                ++stays;
            }
        }
        return stays;
    }
#endif

    /// <summary>
    /// The descriptor conjure_runtime would build for the arguments, over a base that picks the null script
    /// host (the merge never touches useNullHost), so the cases run where no .NET runtime is installed.
    /// </summary>
    [[nodiscard]] application_desc null_host_desc(const std::vector<std::string_view> &arguments) {
        engine_config base = engine_config::defaults();
        base.scripting.useNullHost = true;
        core::result<application_desc> desc =
          application_desc::from_command_line(std::span<const std::string_view>(arguments), base);
        CONJURE_REQUIRE_OK(desc);
        return std::move(desc).value();
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

    TEST_CASE("scripting_subsystem - headless_holds_scripts_back names only a headless run of a scripted project") {
        // A scripted project enables scripting and names the game assembly its scripts come from.
        const auto parsed = [](std::string_view scriptingMember) {
            std::string text = R"({"schema_version": 1, "name": "probe")";
            text += scriptingMember;
            text += "}";
            core::result<project> loaded = project::parse(text, std::filesystem::path("."));
            CONJURE_REQUIRE_OK(loaded);
            return std::move(loaded).value();
        };
        const project scripted = parsed(R"(, "scripting": {"enabled": true, "assembly": "Scripts/bin/Game.dll"})");
        const project scriptedByDefault = parsed(R"(, "scripting": {"assembly": "Scripts/bin/Game.dll"})");
        const project unscripted = parsed(R"(, "scripting": {"enabled": false, "assembly": "Scripts/bin/Game.dll"})");
        // Neighbours with no script to hold back: scripting enabled without an assembly, and a project.json
        // without a "scripting" section, which enables scripting by default.
        const project enabledOnly = parsed(R"(, "scripting": {"enabled": true})");
        const project bare = parsed("");
        REQUIRE(bare.description().scripting.enabled);
        REQUIRE(bare.description().scripting.gameAssembly.empty());

        const engine_config headless = engine_config::headless_defaults();
        REQUIRE_FALSE(headless.scripting.enabled);
        CHECK(scripting_subsystem::headless_holds_scripts_back(headless, &scripted));
        CHECK(scripting_subsystem::headless_holds_scripts_back(headless, &scriptedByDefault));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, &unscripted));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, &enabledOnly));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, &bare));
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(headless, nullptr));

        engine_config optedIn = headless;
        optedIn.scripting.enabled = true;
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(optedIn, &scripted));

        engine_config windowed = engine_config::defaults();
        windowed.scripting.enabled = false;
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(windowed, &scripted));

        // The editor's headless mode picks its own script host, so a disabled one there was asked for.
        engine_config editorHeadless = headless;
        editorHeadless.editor = true;
        CHECK_FALSE(scripting_subsystem::headless_holds_scripts_back(editorHeadless, &scripted));
    }

    TEST_CASE("regression - code sweep F075 a headless run of a scripted project starts the runtime with --scripting") {
        // The whole path of `conjure_runtime --project <dir> --headless [--scripting]` short of the process: the
        // merge, then the application. Without the flag in the merge no headless run ever had a script runtime.
        log_capture logs = {};
        temp_directory directory("engine_headless_scripting");
        // The scripted project names a game assembly; the null host loads any file that exists under that name.
        const project scripted = engine_test::write_project(
          directory.path() / "scripted",
          "scripted",
          {},
          R"("scripting": {"enabled": true, "assembly": "Scripts/bin/Game.dll", "hot_reload": false}, )"
          R"("modules": {"engine_plugins": false})"
        );
        CONJURE_REQUIRE_OK(directory.write_text("scripted/Scripts/bin/Game.dll", "stand-in assembly"));
        REQUIRE(scripted.description().scripting.enabled);
        const project unscripted = engine_test::write_project(
          directory.path() / "unscripted",
          "unscripted",
          {},
          R"("scripting": {"enabled": false}, "modules": {"engine_plugins": false})"
        );
        REQUIRE_FALSE(unscripted.description().scripting.enabled);
        // No "scripting" section at all: scripting is enabled by default and there is no script to run.
        const project bare =
          engine_test::write_project(directory.path() / "bare", "bare", {}, R"("modules": {"engine_plugins": false})");
        REQUIRE(bare.description().scripting.enabled);
        const std::string scriptedArgument = (directory.path() / "scripted").string();
        const std::string unscriptedArgument = (directory.path() / "unscripted").string();
        const std::string bareArgument = (directory.path() / "bare").string();

        SUBCASE("--headless --scripting constructs the runtime and writes its frame block") {
            application app(
              null_host_desc({"--project", scriptedArgument, "--headless", "--scripting", "--jobs", "0"})
            );
            CONJURE_REQUIRE_OK(app.initialise());
            REQUIRE(app.context().scripting != nullptr);
            CHECK(app.context().scripting->status() == scripting::script_host_status::READY);
            CHECK(app.context().scripting->mode() == scripting::script_mode::PLAY);
            scripting_subsystem *subsystem = find_scripting_subsystem(app);
            REQUIRE(subsystem != nullptr);
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(subsystem->frame_blocks_written() == 1);
            app.shutdown();
            CHECK_FALSE(logs.contains(HEADLESS_HINT));
            CHECK_FALSE(logs.contains(PLAIN_DISABLED));
        }
        SUBCASE("plain --headless starts without it and the INFO line names the flag") {
            application app(null_host_desc({"--project", scriptedArgument, "--headless", "--jobs", "0"}));
            CONJURE_REQUIRE_OK(app.initialise());
            CHECK(app.context().scripting == nullptr);
            CONJURE_REQUIRE_OK(app.step_frame());
            app.shutdown();
            CHECK(logs.contains(core::log_level::INFO, HEADLESS_HINT));
            CHECK_FALSE(logs.contains(PLAIN_DISABLED));
        }
        SUBCASE("a project that disables scripting gets the plain line, and --scripting still wins over it") {
            application off(null_host_desc({"--project", unscriptedArgument, "--headless", "--jobs", "0"}));
            CONJURE_REQUIRE_OK(off.initialise());
            CHECK(off.context().scripting == nullptr);
            off.shutdown();
            CHECK(logs.contains(core::log_level::INFO, PLAIN_DISABLED));
            CHECK_FALSE(logs.contains(HEADLESS_HINT));

            application on(
              null_host_desc({"--project", unscriptedArgument, "--headless", "--scripting", "--jobs", "0"})
            );
            CONJURE_REQUIRE_OK(on.initialise());
            CHECK(on.context().scripting != nullptr);
            on.shutdown();
        }
        SUBCASE("a project without a scripting section has no script to hold back and gets the plain line") {
            application app(null_host_desc({"--project", bareArgument, "--headless", "--jobs", "0"}));
            CONJURE_REQUIRE_OK(app.initialise());
            CHECK(app.context().scripting == nullptr);
            app.shutdown();
            CHECK(logs.contains(core::log_level::INFO, PLAIN_DISABLED));
            CHECK_FALSE(logs.contains(HEADLESS_HINT));
        }
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

        scripting_subsystem::write_frame_block(block, time, input, nullptr);
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

    TEST_CASE("regression - code sweep W3-CS-L4 the subsystem gives the runtime scene.disabled and scene.inactive") {
        application app(engine_test::make_desc(null_host_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        scripting::script_runtime *runtime = app.context().scripting;
        REQUIRE(runtime != nullptr);
        const ecs::component_id disabledTag = app.context().components->id_of<scene::disabled>();
        const ecs::component_id inactiveTag = app.context().components->id_of<scene::inactive>();
        REQUIRE(disabledTag.is_valid());
        REQUIRE(inactiveTag.is_valid());
        const std::span<const ecs::component_id> tags = runtime->activity_tags();
        REQUIRE(tags.size() == 2);
        CHECK(tags[0] == disabledTag);
        CHECK(tags[1] == inactiveTag);
        app.shutdown();
    }

    TEST_CASE("scripting_subsystem - scene::set_enabled in PLAY tells a scripted child once") {
        application app(engine_test::make_desc(null_host_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        scripting::script_runtime *runtime = app.context().scripting;
        REQUIRE(runtime != nullptr);
        REQUIRE(runtime->mode() == scripting::script_mode::PLAY);
        scripting::null_script_host &host = null_host_of(*runtime);
        host.add_fake_class(sleeper_class());
        CONJURE_REQUIRE_OK(runtime->load_game_assembly({}));

        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> parent = active.create_entity(core::string_id("parent"));
        CONJURE_REQUIRE_OK(parent);
        const core::result<ecs::entity> child = active.create_entity(core::string_id("child"), parent.value());
        CONJURE_REQUIRE_OK(child);
        const scripting::script_entry_id id = add_fake_script(*runtime, child.value(), SLEEPER);
        CONJURE_REQUIRE_OK(app.step_frame());
        const scripting::script_entry *entry = runtime->entry(id);
        REQUIRE(entry != nullptr);
        REQUIRE(entry->is_live());
        const i32 slot = entry->managedSlot;
        CHECK(host.is_slot_active(slot));
        host.reset_recorder();

        // The parent carries no script: the scene's inactive tag on the child is what reaches the runtime.
        CONJURE_REQUIRE_OK(active.set_enabled(parent.value(), false));
        CONJURE_REQUIRE_OK(app.step_frame());
        REQUIRE(host.recorder().activities().size() == 1);
        CHECK(host.recorder().activities()[0].slot == slot);
        CHECK_FALSE(host.recorder().activities()[0].active);
        CHECK(host.recorder().activities()[0].changed);
        CHECK(runtime->entry(id)->has_flag(scripting::script_entry_flags::INACTIVE));
        CHECK_FALSE(host.is_slot_active(slot));

        CONJURE_REQUIRE_OK(active.set_enabled(parent.value(), true));
        CONJURE_REQUIRE_OK(app.step_frame());
        REQUIRE(host.recorder().activities().size() == 2);
        CHECK(host.recorder().activities()[1].slot == slot);
        CHECK(host.recorder().activities()[1].active);
        CHECK(host.recorder().activities()[1].changed);
        CHECK_FALSE(runtime->entry(id)->has_flag(scripting::script_entry_flags::INACTIVE));
        CHECK(host.is_slot_active(slot));
        app.shutdown();
    }

    TEST_CASE("scripting_subsystem - the saved scene of a disabled scripted entity is unchanged by a Play session") {
        application app(engine_test::make_desc(null_host_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        scripting::script_runtime *runtime = app.context().scripting;
        REQUIRE(runtime != nullptr);
        null_host_of(*runtime).add_fake_class(sleeper_class());
        CONJURE_REQUIRE_OK(runtime->load_game_assembly({}));
        runtime->set_mode(scripting::script_mode::EDIT);

        scene::scene &active = app.context().scenes->active();
        const core::result<ecs::entity> entity = active.create_entity(core::string_id("sleeper"));
        CONJURE_REQUIRE_OK(entity);
        const scripting::script_entry_id id = add_fake_script(*runtime, entity.value(), SLEEPER);
        const std::string before = saved_scene(active);
        REQUIRE(before.find("Game.Sleeper") != std::string::npos);

        // Disabled and enabled again during Play: the scene saved after Play is byte-identical.
        runtime->set_mode(scripting::script_mode::PLAY);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(active.set_enabled(entity.value(), false));
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        REQUIRE(runtime->entry(id)->has_flag(scripting::script_entry_flags::INACTIVE));
        CONJURE_REQUIRE_OK(active.set_enabled(entity.value(), true));
        CONJURE_REQUIRE_OK(app.step_frame());
        runtime->set_mode(scripting::script_mode::EDIT);
        CHECK(saved_scene(active) == before);

        // Play stopped while the entity is disabled, then enabled again in EDIT mode: the same bytes.
        runtime->set_mode(scripting::script_mode::PLAY);
        CONJURE_REQUIRE_OK(active.set_enabled(entity.value(), false));
        CONJURE_REQUIRE_OK(app.step_frame());
        REQUIRE(runtime->entry(id)->has_flag(scripting::script_entry_flags::INACTIVE));
        runtime->set_mode(scripting::script_mode::EDIT);
        CONJURE_REQUIRE_OK(active.set_enabled(entity.value(), true));
        CHECK(saved_scene(active) == before);
        app.shutdown();
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("scripting_subsystem - convert_contacts maps the four kinds and drops STAY without includeStay") {
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
        std::vector<scripting::collision_event> events(2);      // replaced, not appended to
        scripting_subsystem::convert_contacts(contacts, events, false);
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

    TEST_CASE("scripting_subsystem - convert_contacts maps STAY to COLLISION_STAY with impulse 0 when asked") {
        std::vector<physics::contact_event> contacts(2);
        contacts[0].kind = physics::contact_kind::STAY;
        contacts[0].a.index = 4;
        contacts[0].b.index = 7;
        contacts[0].impulse = 9.0f;
        contacts[0].point = math::vec3(1.0f, 2.0f, 3.0f);
        contacts[0].normal = math::vec3(0.0f, 1.0f, 0.0f);
        contacts[1].kind = physics::contact_kind::ENTER;
        contacts[1].impulse = 5.0f;
        std::vector<scripting::collision_event> events = {};
        scripting_subsystem::convert_contacts(contacts, events, true);
        REQUIRE(events.size() == 2);
        CHECK(events[0].kind == scripting::collision_kind::COLLISION_STAY);
        CHECK(events[0].impulse == 0.0f);
        CHECK(events[0].a.index == 4);
        CHECK(events[0].b.index == 7);
        CHECK(events[0].point == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(events[0].normal == math::vec3(0.0f, 1.0f, 0.0f));
        CHECK(events[1].kind == scripting::collision_kind::COLLISION_ENTER);
        CHECK(events[1].impulse == 5.0f);
    }

    TEST_CASE(
      "regression - code sweep W3-CS-L4 the collision source converts STAY only while a class overrides OnCollisionStay"
    ) {
        application app(engine_test::make_desc(null_host_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        scripting::script_runtime *runtime = app.context().scripting;
        REQUIRE(runtime != nullptr);
        REQUIRE(app.context().physics != nullptr);
        const scripting_subsystem *subsystem = find_scripting_subsystem(app);
        REQUIRE(subsystem != nullptr);
        scripting::null_script_host &host = null_host_of(*runtime);
        host.add_fake_class(fake_class(BUMPER, {scripting::script_callback::COLLISION_ENTER}));
        CONJURE_REQUIRE_OK(runtime->load_game_assembly({}));
        REQUIRE_FALSE(runtime->any_class_has(scripting::script_callback::COLLISION_STAY));

        // A box resting on static ground, kept awake so the pair reports a STAY every fixed step; both are scripted.
        scene::scene &active = app.context().scenes->active();
        const ecs::entity ground = spawn_box(active, "ground", math::vec3(0.0f, -0.5f, 0.0f), true);
        const ecs::entity box = spawn_box(active, "box", math::vec3(0.0f, 0.5f, 0.0f), false);
        static_cast<void>(add_fake_script(*runtime, ground, BUMPER));
        static_cast<void>(add_fake_script(*runtime, box, BUMPER));

        constexpr u32 FRAMES = 30;
        usize physicsStays = 0;
        usize convertedStays = 0;
        for (u32 frame = 0; frame < FRAMES; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
            for (const physics::contact_event &contact : app.context().physics->contact_events()) {
                if (contact.kind == physics::contact_kind::STAY) {
                    ++physicsStays;
                }
            }
            convertedStays += count_stays(subsystem->last_collisions());
        }
        REQUIRE(physicsStays > 0);          // the gate below had something to drop
        CHECK(convertedStays == 0);

        // A class that overrides OnCollisionStay arrives with a reload: from the next fixed step one STAY per pair
        // per participant per step reaches the scripts, with impulse 0.
        REQUIRE(host.remove_fake_class(scripting::script_name_hash(BUMPER)));
        host.add_fake_class(fake_class(
          BUMPER,
          {scripting::script_callback::COLLISION_ENTER, scripting::script_callback::COLLISION_STAY}
        ));
        CONJURE_REQUIRE_OK(runtime->reload_game_assembly({}));
        REQUIRE(runtime->any_class_has(scripting::script_callback::COLLISION_STAY));
        host.reset_recorder();
        const u64 firstStep = app.time().fixedStepIndex;
        for (u32 frame = 0; frame < FRAMES; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(count_stays(subsystem->last_collisions()) <= 1);
            for (const scripting::collision_event &event : subsystem->last_collisions()) {
                CHECK(event.impulse == 0.0f);
            }
        }
        const u64 fixedSteps = app.time().fixedStepIndex - firstStep;
        REQUIRE(fixedSteps > 0);
        u64 stayRecords = 0;
        for (const scripting::null_host_recorder::collision_record &record : host.recorder().collisions()) {
            if (record.kind == scripting::collision_kind::COLLISION_STAY) {
                ++stayRecords;
                CHECK(record.impulse == 0.0f);
            }
        }
        CHECK(stayRecords == 2 * fixedSteps);
        app.shutdown();
    }
#endif
}
