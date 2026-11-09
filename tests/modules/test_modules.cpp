#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "fixture_descriptors.h"
#include "static_fixtures.h"

#include "conjure/modules/modules.h"

#include "conjure/core/string_id.h"

#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::testing;

// The umbrella header is what a module author includes; this file is written the way a module
// would be and then loaded as a static module, proving the umbrella brings everything an entry needs.

namespace {
    class host_clock_service {
        public:
            CONJURE_MODULE_SERVICE(conjure::test::host_clock_service);

            u32 ticks = 42;
    };

    u32 sObservedTicks = 0;

    module_entry_status umbrella_entry(module_context &context) {
        module_registrar &registrar = context.registrar();
        registrar.register_extension(fixtures::component_desc{"umbrella_component", 8, 8});
        registrar.register_managed_assembly("managed/Umbrella.dll");

        const host_clock_service *clock = context.try_service<host_clock_service>();
        if (clock == nullptr) {
            registrar.fail("the host provides no clock");
            return module_entry_status::FAILED;
        }
        sObservedTicks = clock->ticks;
        return module_entry_status::OK;
    }

    CONJURE_DECLARE_STATIC_MODULE(
      umbrella_module,
      R"({"schema": 1, "name": "umbrella", "version": "1.0.0", "engine": "*"})",
      &umbrella_entry
    )
}

TEST_SUITE("modules/modules") {

    TEST_CASE("modules - the umbrella header is enough to write and load a module") {
        host_clock_service clock = {};
        static_host host = {};
        host.manager.provide_service(clock);
        sObservedTicks = 0;

        const result<module_id> added = host.manager.add_static_module(conjure_static_module_umbrella_module());
        CONJURE_REQUIRE_OK(added);
        CONJURE_REQUIRE_OK(host.manager.load(added.value()));
        CHECK(sObservedTicks == 42);

        const module_registration_set &set = host.manager.registry().registrations(added.value());
        REQUIRE(extensions_of<fixtures::component_desc>(set).size() == 1);
        CHECK(std::string_view(extensions_of<fixtures::component_desc>(set)[0]->name) == "umbrella_component");
        CHECK(set.managedAssemblies.size() == 1);
    }

    TEST_CASE("modules - a module that needs a missing service fails its load cleanly") {
        static_host host = {};
        conjure::testing::log_capture logs = {};
        logs.allow_errors();
        sObservedTicks = 0;
        const module_id id = host.manager.add_static_module(conjure_static_module_umbrella_module()).value();
        const result<void> loaded = host.manager.load(id);
        CONJURE_CHECK_ERROR(loaded, module_error::ENTRY_FAILED);
        CHECK(loaded.get_error().message().find("no clock") != std::string::npos);
        CHECK(sObservedTicks == 0);
        CHECK(host.manager.registry().state(id) == module_state::FAILED);
    }
}
