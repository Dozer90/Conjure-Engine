#include "conjure/testing/doctest.h"

#include "engine_test_support.h"

#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_subsystem.h"

#include <string>
#include <vector>

// engine_subsystem.h (docs/design/06_engine_loop.md 2.9, D7): the adapter interface and its optional hooks.

using namespace conjure;
using namespace conjure::engine;

namespace {

    /// <summary>
    /// A subsystem overriding only the pure virtuals: the optional hooks come from the base.
    /// </summary>
    class minimal_subsystem final : public engine_subsystem {
        public:
            minimal_subsystem() = default;
            ~minimal_subsystem() override = default;

        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "minimal";
            }

            [[nodiscard]] core::result<void> startup(engine_context &) override {
                return core::result<void>::success();
            }

            void shutdown(engine_context &) override {}
    };
}

TEST_SUITE("engine/engine_subsystem") {

    TEST_CASE("engine_subsystem - the optional hooks default to no-ops that succeed") {
        minimal_subsystem subsystem = {};
        engine_context context = {};
        ecs::component_registry registry = {};
        ecs::world world(registry);
        ecs::scheduler scheduler(world, nullptr);
        CHECK(subsystem.name() == "minimal");
        CONJURE_CHECK_OK(subsystem.startup(context));
        CONJURE_CHECK_OK(subsystem.post_startup(context));
        subsystem.register_systems(scheduler, context);
        subsystem.begin_frame(context);
        subsystem.end_frame(context);
        subsystem.shutdown(context);
        CONJURE_CHECK_OK(scheduler.finalise());
        CHECK(scheduler.system_count() == 0);
    }

    TEST_CASE("engine_subsystem - a probe records the calls it receives") {
        std::vector<std::string> log = {};
        {
            engine_test::probe_subsystem probe("p", log);
            engine_context context = {};
            CONJURE_CHECK_OK(probe.startup(context));
            CHECK(probe.is_started());
            CONJURE_CHECK_OK(probe.post_startup(context));
            probe.begin_frame(context);
            probe.end_frame(context);
            probe.shutdown(context);
            CHECK_FALSE(probe.is_started());
        }
        const std::vector<std::string> expected = {
            "p.startup", "p.post_startup", "p.begin_frame", "p.end_frame", "p.shutdown", "p.destroyed"
        };
        CHECK(log == expected);
    }
}
