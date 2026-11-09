#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/modules/module_service.h"

#include "conjure/core/hash.h"

#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    class world_service {
        public:
            CONJURE_MODULE_SERVICE(conjure::test::world_service);

            int value = 1;
    };

    class asset_service {
        public:
            CONJURE_MODULE_SERVICE(conjure::test::asset_service);

            int value = 2;
    };

    class unprovided_service {
        public:
            CONJURE_MODULE_SERVICE(conjure::test::unprovided_service);
    };
}

TEST_SUITE("modules/module_service") {

    TEST_CASE("service_id - make_service_id hashes the qualified name and the macro declares both members") {
        CHECK(make_service_id("conjure::test::world_service").hash == hash::fnv1a64("conjure::test::world_service"));
        CHECK(world_service::SERVICE_ID == make_service_id("conjure::test::world_service"));
        CHECK(world_service::SERVICE_NAME == "conjure::test::world_service");
        CHECK(world_service::SERVICE_ID != asset_service::SERVICE_ID);
        CHECK(service_id{}.hash == 0);
        CHECK(service_id{1} < service_id{2});
    }

    TEST_CASE("service_table - provide then find returns the same object and misses are null") {
        service_table table = {};
        world_service world = {};
        asset_service assets = {};
        CHECK(table.size() == 0);
        CHECK(table.find<world_service>() == nullptr);

        table.provide(world);
        table.provide(assets);
        CHECK(table.size() == 2);
        CHECK(table.find<world_service>() == &world);
        CHECK(table.find<asset_service>() == &assets);
        CHECK(table.find<unprovided_service>() == nullptr);
        CHECK(table.contains(world_service::SERVICE_ID));
        CHECK_FALSE(table.contains(unprovided_service::SERVICE_ID));
        CHECK(table.find<world_service>()->value == 1);
    }

    TEST_CASE("service_table - providing the same service twice asserts and keeps the first") {
        service_table table = {};
        world_service first = {};
        world_service second = {};
        table.provide(first);
        CONJURE_CHECK_ASSERTS(table.provide(second));
        CHECK(table.size() == 1);
        CHECK(table.find<world_service>() == &first);
    }

    TEST_CASE("service_table - clear forgets everything") {
        service_table table = {};
        world_service world = {};
        table.provide(world);
        table.clear();
        CHECK(table.size() == 0);
        CHECK(table.find<world_service>() == nullptr);
        table.provide(world);
        CHECK(table.find<world_service>() == &world);
    }
}
