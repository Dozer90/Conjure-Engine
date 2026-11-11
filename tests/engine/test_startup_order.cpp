#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_error.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// Startup and shutdown order (docs/design/06_engine_loop.md 3.4, 3.5, D7): the built-in adapters start in
// the documented order, user subsystems follow in push order, post_startup and register_systems run after
// every startup, a failure unwinds in reverse, shutdown is idempotent.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] usize index_of(const std::vector<std::string> &log, std::string_view entry) {
        const auto found = std::find(log.begin(), log.end(), entry);
        REQUIRE(found != log.end());
        return static_cast<usize>(found - log.begin());
    }

    [[nodiscard]] std::vector<std::string> subsystem_names(const application &app) {
        std::vector<std::string> names = {};
        for (const std::unique_ptr<engine_subsystem> &subsystem : app.subsystems()) {
            names.emplace_back(subsystem->name());
        }
        return names;
    }
}

TEST_SUITE("engine/startup_order") {

    TEST_CASE("startup_order - the built-in adapters start in the documented order") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        const std::vector<std::string> names = subsystem_names(app);
        REQUIRE(names.size() >= 6);
        CHECK(names[0] == "platform");
        CHECK(names[1] == "rhi");
        CHECK(names[2] == "assets");
        CHECK(names[3] == "ecs");
        CHECK(names[4] == "scene");
        CHECK(names.back() == "modules");
        const auto position = [&names](std::string_view name) {
            return static_cast<usize>(std::find(names.begin(), names.end(), name) - names.begin());
        };
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        CHECK(position("audio") == 5);
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        CHECK(position("physics") > position("scene"));
        CHECK(position("physics") < position("modules"));
#endif
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        CHECK(position("renderer") > position("scene"));
        CHECK(position("renderer") < position("modules"));
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        CHECK(position("scripting") < position("modules"));
        CHECK(position("scripting") > position("scene"));
#endif
        app.shutdown();
    }

    TEST_CASE("startup_order - probe subsystems record the documented order") {
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("one", log));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("two", log));
        CONJURE_REQUIRE_OK(app.initialise());

        const std::vector<std::string> names = subsystem_names(app);
        REQUIRE(names.size() >= 2);
        CHECK(names[names.size() - 2] == "one");
        CHECK(names[names.size() - 1] == "two");

        // Startups in push order, then every post_startup, then every register_systems.
        CHECK(index_of(log, "one.startup") < index_of(log, "two.startup"));
        CHECK(index_of(log, "two.startup") < index_of(log, "one.post_startup"));
        CHECK(index_of(log, "one.post_startup") < index_of(log, "two.post_startup"));
        CHECK(index_of(log, "two.post_startup") < index_of(log, "one.register_systems"));
        CHECK(index_of(log, "one.register_systems") < index_of(log, "two.register_systems"));

        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(index_of(log, "one.begin_frame") < index_of(log, "two.begin_frame"));
        CHECK(index_of(log, "two.end_frame") < index_of(log, "one.end_frame"));

        app.shutdown();
        CHECK(index_of(log, "two.shutdown") < index_of(log, "one.shutdown"));
        CHECK(std::count(log.begin(), log.end(), "one.shutdown") == 1);
    }

    TEST_CASE("startup_order - the third probe failing unwinds the two before it in reverse") {
        log_capture logs = {};
        logs.allow_errors();
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("one", log));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("two", log));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("three", log, true));
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::LAYER_ATTACH_FAILED);
        CHECK(app.state() == application_state::CREATED);
        CHECK(app.subsystems().empty());
        CHECK(app.context().world == nullptr);
        CHECK(app.context().jobs == nullptr);

        const std::vector<std::string> expected = {
            "one.startup", "two.startup", "three.startup", "three.shutdown", "two.shutdown", "one.shutdown"
        };
        CHECK(log == expected);
        CHECK(engine_test::filter_log(log, "one.post_startup").empty());
        CHECK(logs.contains(core::log_level::ERR, "startup failed"));
    }

    TEST_CASE("startup_order - a post_startup failure unwinds every started subsystem") {
        log_capture logs = {};
        logs.allow_errors();
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("one", log));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("two", log, false, true));
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::SCRIPT_RUNTIME_NOT_FOUND);
        CHECK(app.state() == application_state::CREATED);
        CHECK(index_of(log, "two.post_startup") < index_of(log, "two.shutdown"));
        CHECK(index_of(log, "two.shutdown") < index_of(log, "one.shutdown"));
        CHECK(engine_test::filter_log(log, "one.register_systems").empty());
    }

    TEST_CASE("startup_order - user subsystems survive a failed initialise and start on the next attempt") {
        log_capture logs = {};
        logs.allow_errors();
        std::vector<std::string> log = {};
        engine_config broken = engine_test::inline_config();
        broken.rendering.enabled = true;
        broken.rendering.backend = rhi_backend_choice::NULL_DEVICE;
        broken.rendering.shaderDirectory = "nowhere/shaders";
        application_desc desc = engine_test::make_desc(broken);
        application app(std::move(desc));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("user", log));
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::RENDERER_INIT_FAILED);
        CHECK(app.state() == application_state::CREATED);
        CHECK(engine_test::filter_log(log, "user.startup").empty());
        CHECK(engine_test::filter_log(log, "user.destroyed").empty());
#else
        CONJURE_REQUIRE_OK(app.initialise());
        app.shutdown();
#endif
        log.clear();
        // The failing piece is a configuration matter; the object itself is reusable.
        engine_config fixed = engine_test::inline_config();
        application again(engine_test::make_desc(fixed));
        again.add_subsystem(std::make_unique<engine_test::probe_subsystem>("user2", log));
        CONJURE_REQUIRE_OK(again.initialise());
        CHECK_FALSE(engine_test::filter_log(log, "user2.startup").empty());
        again.shutdown();
    }

    TEST_CASE("startup_order - shutdown is idempotent and the destructor finishes a running application") {
        std::vector<std::string> log = {};
        {
            application app(engine_test::make_desc(engine_test::inline_config()));
            app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("probe", log));
            CONJURE_REQUIRE_OK(app.initialise());
            app.shutdown();
            app.shutdown();
            CHECK(std::count(log.begin(), log.end(), "probe.shutdown") == 1);
            CONJURE_REQUIRE_OK(app.initialise());
            CHECK(std::count(log.begin(), log.end(), "probe.startup") == 2);
        }
        CHECK(std::count(log.begin(), log.end(), "probe.shutdown") == 2);
        CHECK(std::count(log.begin(), log.end(), "probe.destroyed") == 1);
    }

    TEST_CASE("regression - review finding 1: a step-20 failure detaches the layers attached at step 19") {
        log_capture logs = {};
        logs.allow_errors();
        std::vector<std::string> log = {};
        temp_directory directory("engine_step20_detach");
        project loaded = engine_test::write_project(directory.path(), "noscene", "scenes/missing.scene.json");
        engine_config config = engine_test::inline_config();
        config.projectPath = loaded.root_directory();
        loaded.apply_to(config);
        application_desc desc = engine_test::make_desc(config);
        desc.project = std::move(loaded);
        application app(std::move(desc));
        app.layers().push_layer(std::make_unique<engine_test::probe_layer>("L", log));

        CONJURE_CHECK_ERROR(app.initialise(), engine_error::STARTUP_SCENE_FAILED);
        CHECK(app.state() == application_state::CREATED);
        CHECK_FALSE(app.layers().is_attached());
        const std::vector<std::string> expected = {"L.attach", "L.detach"};
        CHECK(log == expected);

        // The object is reusable: the next attempt attaches (and, failing the same way, detaches) again instead
        // of hitting the attached-twice guard with dangling context pointers.
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::STARTUP_SCENE_FAILED);
        CHECK_FALSE(app.layers().is_attached());
        CHECK(std::count(log.begin(), log.end(), "L.attach") == 2);
        CHECK(std::count(log.begin(), log.end(), "L.detach") == 2);
    }

    TEST_CASE("startup_order - initialise twice is ALREADY_INITIALISED without disturbing the running instance") {
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("probe", log));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::ALREADY_INITIALISED);
        CHECK(app.state() == application_state::RUNNING);
        CHECK(std::count(log.begin(), log.end(), "probe.startup") == 1);
        CONJURE_REQUIRE_OK(app.step_frame());
        app.shutdown();
    }
}
