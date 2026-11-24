#include "conjure/testing/doctest.h"

#include "engine_test_support.h"

#include "conjure/engine/application.h"
#include "conjure/engine/engine_context.h"
#include "conjure/modules/module_service.h"

#include <cstddef>
#include <string_view>
#include <type_traits>

// engine_context.h (docs/design/06_engine_loop.md 2.5, R22, R51): the service table every pointer of which is
// null until an application fills it, and its module service identity.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/engine_context") {

    TEST_CASE("engine_context - every pointer starts null") {
        const engine_context context = {};
        CHECK(context.app == nullptr);
        CHECK(context.config == nullptr);
        CHECK(context.project == nullptr);
        CHECK(context.time == nullptr);
        CHECK(context.frameMemory == nullptr);
        CHECK(context.jobs == nullptr);
        CHECK(context.filesystem == nullptr);
        CHECK(context.window == nullptr);
        CHECK(context.input == nullptr);
        CHECK(context.device == nullptr);
        CHECK(context.assets == nullptr);
        CHECK(context.assetDatabase == nullptr);
        CHECK(context.components == nullptr);
        CHECK(context.world == nullptr);
        CHECK(context.scheduler == nullptr);
        CHECK(context.scenes == nullptr);
        CHECK(context.renderer == nullptr);
        CHECK(context.physics == nullptr);
        CHECK(context.audio == nullptr);
        CHECK(context.scripting == nullptr);
        CHECK(context.modules == nullptr);
        CHECK(context.checks == nullptr);
        CHECK(context.stats == nullptr);
        CHECK(context.renderDefaults == nullptr);
    }

    TEST_CASE("engine_context - renderDefaults is appended after stats, the former last member") {
        // Plugins built against the table before renderDefaults existed read every earlier member at its old
        // offset, so the new pointer comes after all of them and nothing else moved.
        static_assert(std::is_standard_layout_v<engine_context>);
        CHECK(offsetof(engine_context, renderDefaults) == offsetof(engine_context, stats) + sizeof(void *));
        CHECK(sizeof(engine_context) == offsetof(engine_context, renderDefaults) + sizeof(void *));
    }

    TEST_CASE("engine_context - is a module service with a stable id") {
        CHECK(std::string_view(engine_context::SERVICE_NAME) == "conjure::engine::engine_context");
        CHECK(engine_context::SERVICE_ID == modules::make_service_id("conjure::engine::engine_context"));
        CHECK(engine_context::SERVICE_ID != application::SERVICE_ID);
        modules::service_table table = {};
        engine_context context = {};
        table.provide(context);
        CHECK(table.find<engine_context>() == &context);
        CHECK(table.find<application>() == nullptr);
    }

    TEST_CASE("engine_context - an application publishes its own pointers before initialise") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        const engine_context &context = app.context();
        CHECK(context.app == &app);
        CHECK(context.config == &app.desc().config);
        CHECK(context.time == &app.time());
        CHECK(context.stats == &app.stats());
        CHECK(context.project == nullptr);
        CHECK(context.world == nullptr);
        // The renderer subsystem publishes the rendering defaults during initialise, not before.
        CHECK(context.renderDefaults == nullptr);
    }
}
