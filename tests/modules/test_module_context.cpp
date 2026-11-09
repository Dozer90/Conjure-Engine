#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/modules/module_context.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_service.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/event.h"
#include "conjure/core/event_bus.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    class world_service {
        public:
            CONJURE_MODULE_SERVICE(conjure::test::world_service);
    };

    class missing_service {
        public:
            CONJURE_MODULE_SERVICE(conjure::test::missing_service);
    };

    struct context_fixture {
        module_manifest manifest = {};
        service_table services = {};
        world_service world = {};
        std::filesystem::path root = std::filesystem::path("project") / "modules" / "demo";
        std::unique_ptr<module_context> context = nullptr;

        context_fixture() {
            manifest.name = "demo";
            manifest.version = {1, 2, 3};
            services.provide(world);
            context = std::make_unique<module_context>(
              module_id{4, 2},
              module_host_kind::EDITOR,
              semantic_version{0, 9, 1},
              manifest,
              root,
              services
            );
        }
    };
}

TEST_SUITE("modules/module_context") {

    TEST_CASE("module_context - exposes what the manager constructed it with") {
        const context_fixture fixture = {};
        const module_context &context = *fixture.context;
        CHECK(context.id() == module_id{4, 2});
        CHECK(context.host() == module_host_kind::EDITOR);
        CHECK(context.engine_version() == semantic_version{0, 9, 1});
        CHECK(&context.manifest() == &fixture.manifest);
        CHECK(context.manifest().name == "demo");
        CHECK(context.root_directory() == fixture.root);
        CHECK(context.retained_subscription_count() == 0);
        CHECK_FALSE(context.is_entry_running());
    }

    TEST_CASE("module_context - the registrar is bound only while the entry runs") {
        context_fixture fixture = {};
        module_registration_set set = {};
        module_registrar registrar(set);

        fixture.context->begin_entry(registrar);
        CHECK(fixture.context->is_entry_running());
        CHECK(&fixture.context->registrar() == &registrar);
        fixture.context->registrar().register_managed_assembly("managed/Demo.dll");
        CHECK(set.managedAssemblies.size() == 1);

        fixture.context->end_entry();
        CHECK_FALSE(fixture.context->is_entry_running());
    }

    TEST_CASE("module_context - registrar outside the entry asserts and hands out a scratch registrar") {
        context_fixture fixture = {};
        module_registration_set set = {};
        module_registrar bound(set);
        fixture.context->begin_entry(bound);
        fixture.context->end_entry();

        conjure::testing::expect_assert guard = {};
        module_registrar &scratch = fixture.context->registrar();
        CHECK(guard.count() == 1);
        CHECK(&scratch != &bound);
        scratch.register_managed_assembly("managed/Lost.dll");
        CHECK(set.managedAssemblies.empty());
    }

    TEST_CASE("module_context - try_service and require_service find provided services") {
        const context_fixture fixture = {};
        CHECK(fixture.context->try_service<world_service>() == &fixture.world);
        const result<world_service *> required = fixture.context->require_service<world_service>();
        CONJURE_REQUIRE_OK(required);
        CHECK(required.value() == &fixture.world);
    }

    TEST_CASE("module_context - a missing service is nullptr or SERVICE_MISSING naming the interface and module") {
        const context_fixture fixture = {};
        CHECK(fixture.context->try_service<missing_service>() == nullptr);
        const result<missing_service *> required = fixture.context->require_service<missing_service>();
        CONJURE_CHECK_ERROR(required, module_error::SERVICE_MISSING);
        const std::string &message = required.get_error().message();
        CHECK(message.find("conjure::test::missing_service") != std::string::npos);
        CHECK(message.find("'demo'") != std::string::npos);
    }

    TEST_CASE("module_context - a retained subscription fires while the context lives and never after") {
        event<int> signal = {};
        int received = 0;
        {
            context_fixture fixture = {};
            fixture.context->retain(signal.subscribe([&received](int value) { received += value; }));
            fixture.context->retain(event_subscription());
            CHECK(fixture.context->retained_subscription_count() == 2);
            CHECK(signal.subscriber_count() == 1);

            signal.emit(5);
            CHECK(received == 5);
            fixture.context.reset();
        }
        CHECK(signal.subscriber_count() == 0);
        signal.emit(7);
        CHECK(received == 5);
    }

    TEST_CASE("module_context - a retained event_bus subscription dies with the context") {
        event_bus bus = {};
        struct ping {
            int value = 0;
        };
        int received = 0;
        {
            context_fixture fixture = {};
            fixture.context->retain(bus.subscribe<ping>([&received](const ping &event) { received += event.value; }));
            bus.publish(ping{3});
            CHECK(received == 3);
            CHECK(bus.subscriber_count<ping>() == 1);
        }
        CHECK(bus.subscriber_count<ping>() == 0);
        bus.publish(ping{4});
        CHECK(received == 3);
    }
}
