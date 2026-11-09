#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "fake_consumer.h"
#include "static_fixtures.h"

#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_events.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/string_id.h"

#include <format>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

namespace {
    /// <summary>
    /// Records the lifecycle counters at the moment of each call, so the order of onActivated and
    /// onDeactivating relative to attach and detach is visible in the log.
    /// </summary>
    class probe_consumer final : public module_consumer {
        private:
            std::vector<std::string> *mLog = nullptr;
            const bool *mUnloadingSeen = nullptr;

        public:
            probe_consumer(std::vector<std::string> &log, const bool &unloadingSeen) :
                mLog(&log),
                mUnloadingSeen(&unloadingSeen) {}

            ~probe_consumer() override = default;

        public:
            [[nodiscard]] string_id name() const noexcept override {
                return string_id("probe");
            }

            [[nodiscard]] result<void> validate(
              module_id,
              const module_registration_set &,
              const module_registry &
            ) override {
                mLog->push_back(std::format("validate:probe:activated={}", counters().activated));
                return {};
            }

            void attach(module_id, module_context &, const module_registration_set &) override {
                mLog->push_back(std::format("attach:probe:activated={}", counters().activated));
            }

            [[nodiscard]] result<void> can_detach(module_id, unload_policy) override {
                mLog->push_back(std::format("can_detach:probe:deactivating={}", counters().deactivating));
                return {};
            }

            void detach(module_id, reload_context *) override {
                mLog->push_back(std::format(
                  "detach:probe:deactivating={}:unloading_event={}", counters().deactivating, *mUnloadingSeen));
            }

            void restore(module_id, const reload_context &) override {}

            [[nodiscard]] u32 live_use_count(module_id) const override {
                return 0;
            }
    };
}

TEST_SUITE("modules/module_lifecycle_order") {

    TEST_CASE("module_manager - consumers attach in add order and detach in reverse") {
        std::vector<std::string> log = {};
        fake_consumer ecs("ecs", &log);
        fake_consumer assets("assets", &log);
        fake_consumer scripting("scripting", &log);
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_host host = {};
        host.manager.add_consumer(ecs);
        host.manager.add_consumer(assets);
        host.manager.add_consumer(scripting);
        for (fake_consumer *consumer : {&ecs, &assets, &scripting}) {
            consumer->bind_registry(host.manager.registry());
        }
        const module_id id = host.manager.add_static_module(alpha.desc()).value();

        CONJURE_REQUIRE_OK(host.manager.load(id));
        CHECK(log == std::vector<std::string>{
            "validate:ecs:alpha", "validate:assets:alpha", "validate:scripting:alpha",
            "attach:ecs:alpha", "attach:assets:alpha", "attach:scripting:alpha",
        });

        log.clear();
        CONJURE_REQUIRE_OK(host.manager.unload(id));
        CHECK(log == std::vector<std::string>{
            "can_detach(refuse):ecs:alpha", "can_detach(refuse):assets:alpha", "can_detach(refuse):scripting:alpha",
            "detach:scripting:alpha", "detach:assets:alpha", "detach:ecs:alpha",
        });
    }

    TEST_CASE("module_manager - onActivated runs after the last attach and onDeactivating before the first detach") {
        std::vector<std::string> log = {};
        bool unloadingSeen = false;
        probe_consumer first(log, unloadingSeen);
        probe_consumer second(log, unloadingSeen);
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_host host = {};
        host.manager.add_consumer(first);
        host.manager.add_consumer(second);
        const module_id id = host.manager.add_static_module(alpha.desc()).value();

        bool activatedAtLoadedEvent = false;
        const event_subscription onLoaded =
          host.events.subscribe<module_loaded_event>([&activatedAtLoadedEvent](const module_loaded_event &) {
              activatedAtLoadedEvent = counters().activated == 1;
          });
        const event_subscription onUnloading =
          host.events.subscribe<module_unloading_event>([&unloadingSeen](const module_unloading_event &event) {
              unloadingSeen = !event.reloading && counters().deactivating == 1;
          });

        CONJURE_REQUIRE_OK(host.manager.load(id));
        CHECK(log == std::vector<std::string>{
            "validate:probe:activated=0", "validate:probe:activated=0",
            "attach:probe:activated=0", "attach:probe:activated=0",
        });
        CHECK(counters().activated == 1);
        CHECK(activatedAtLoadedEvent);

        log.clear();
        CONJURE_REQUIRE_OK(host.manager.unload(id));
        CHECK(log == std::vector<std::string>{
            "can_detach:probe:deactivating=0", "can_detach:probe:deactivating=0",
            "detach:probe:deactivating=1:unloading_event=true", "detach:probe:deactivating=1:unloading_event=true",
        });
        CHECK(counters().deactivating == 1);
    }

    TEST_CASE("module_manager - events are published in order with the ids and names of the module") {
        std::vector<std::string> events = {};
        static_module_source alpha("alpha", manifest_json("alpha"), REGISTERING_ENTRY);
        static_module_source broken("broken", manifest_json("broken"), FAILING_ENTRY);
        static_host host = {};
        const module_id alphaId = host.manager.add_static_module(alpha.desc()).value();
        const module_id brokenId = host.manager.add_static_module(broken.desc()).value();

        const event_subscription onLoaded =
          host.events.subscribe<module_loaded_event>([&](const module_loaded_event &event) {
              events.push_back(std::format("loaded:{}:{}", event.id, host.manager.registry().state(event.id)));
          });
        const event_subscription onUnloading =
          host.events.subscribe<module_unloading_event>([&](const module_unloading_event &event) {
              events.push_back(std::format("unloading:{}:{}", event.id, event.reloading));
          });
        const event_subscription onUnloaded =
          host.events.subscribe<module_unloaded_event>([&](const module_unloaded_event &event) {
              const bool stillValid = host.manager.registry().is_valid(event.id);
              events.push_back(std::format("unloaded:{}:{}:{}", event.id, event.name.view(), stillValid));
          });
        const event_subscription onFailed =
          host.events.subscribe<module_load_failed_event>([&](const module_load_failed_event &event) {
              events.push_back(std::format("failed:{}:{}", event.id, event.error.code_name()));
          });

        CONJURE_REQUIRE_OK(host.manager.load(alphaId));
        CONJURE_REQUIRE_OK(host.manager.unload(alphaId));
        {
            conjure::testing::log_capture logs = {};
            logs.allow_errors();
            CHECK_FALSE(host.manager.load(brokenId).has_value());
        }

        CHECK(events == std::vector<std::string>{
            std::format("loaded:{}:LOADED", alphaId),
            std::format("unloading:{}:false", alphaId),
            std::format("unloaded:{}:alpha:false", alphaId),
            std::format("failed:{}:ENTRY_FAILED", brokenId),
        });
    }
}
