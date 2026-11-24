#include "conjure/testing/doctest.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_events.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/string_id.h"

#include <type_traits>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

TEST_SUITE("modules/module_events") {

    TEST_CASE("module_events - every event defaults to an invalid id and no payload") {
        CHECK_FALSE(module_loaded_event{}.id.is_valid());
        CHECK_FALSE(module_unloading_event{}.id.is_valid());
        CHECK_FALSE(module_unloading_event{}.reloading);
        CHECK_FALSE(module_unloaded_event{}.id.is_valid());
        CHECK(module_unloaded_event{}.name.is_null());
        CHECK_FALSE(module_load_failed_event{}.id.is_valid());
        CHECK(module_load_failed_event{}.error == error{});
    }

    TEST_CASE("module_events - events are copyable values that travel through an event_bus") {
        CHECK(std::is_copy_constructible_v<module_loaded_event>);
        CHECK(std::is_copy_constructible_v<module_load_failed_event>);

        event_bus bus = {};
        std::vector<module_id> loaded = {};
        std::vector<bool> reloading = {};
        std::vector<string_id> unloadedNames = {};
        std::vector<error> failures = {};

        const event_subscription onLoaded =
          bus.subscribe<module_loaded_event>([&loaded](const module_loaded_event &event) {
              loaded.push_back(event.id);
          });
        const event_subscription onUnloading =
          bus.subscribe<module_unloading_event>([&reloading](const module_unloading_event &event) {
              reloading.push_back(event.reloading);
          });
        const event_subscription onUnloaded =
          bus.subscribe<module_unloaded_event>([&unloadedNames](const module_unloaded_event &event) {
              unloadedNames.push_back(event.name);
          });
        const event_subscription onFailed =
          bus.subscribe<module_load_failed_event>([&failures](const module_load_failed_event &event) {
              failures.push_back(event.error);
          });

        bus.publish(module_loaded_event{module_id{1, 0}});
        bus.publish(module_unloading_event{module_id{1, 0}, true});
        bus.publish(module_unloaded_event{module_id{1, 0}, string_id("alpha")});
        const error gone = core::make_error(module_error::BINARY_NOT_FOUND, "gone");
        bus.publish(module_load_failed_event{module_id{2, 0}, gone});

        REQUIRE(loaded.size() == 1);
        CHECK(loaded[0] == module_id{1, 0});
        REQUIRE(reloading.size() == 1);
        CHECK(reloading[0]);
        REQUIRE(unloadedNames.size() == 1);
        CHECK(unloadedNames[0] == string_id("alpha"));
        REQUIRE(failures.size() == 1);
        CHECK(failures[0].is(module_error::BINARY_NOT_FOUND));
        CHECK(failures[0].message() == "gone");
    }
}
