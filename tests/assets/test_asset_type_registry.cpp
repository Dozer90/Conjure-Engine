#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_type_registry.h"

#include <format>
#include <memory>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    asset_type_desc named_desc(std::string name, u32 version = 1) {
        asset_type_desc desc = {};
        desc.id = make_asset_type_id(name);
        desc.name = std::move(name);
        desc.version = version;
        return desc;
    }
}

TEST_SUITE("assets/asset_type_registry") {

    TEST_CASE("asset_type_registry - register, find by id and name, and find the loader") {
        asset_type_registry registry = {};
        CHECK(registry.size() == 0);
        CHECK(registry.find(asset_type_traits<test_asset>::id()) == nullptr);
        CHECK(registry.find_loader(asset_type_traits<test_asset>::id()) == nullptr);

        CONJURE_REQUIRE_OK(
          registry.register_type(make_asset_type_desc<test_asset>(std::make_unique<test_loader>(), 3))
        );
        const asset_type_desc *found = registry.find(asset_type_traits<test_asset>::id());
        REQUIRE(found != nullptr);
        CHECK(found->name == "conjure.test.asset");
        CHECK(found->version == 3);
        CHECK(found->flags == asset_type_flags::NONE);
        CHECK(registry.find("conjure.test.asset") == found);
        CHECK(registry.find_loader(found->id) == found->loader.get());
        CHECK(registry.find_loader(found->id)->type() == found->id);
        CHECK(registry.types().size() == 1);
        CHECK(registry.size() == 1);
    }

    TEST_CASE("asset_type_registry - a duplicate id fails with TYPE_DUPLICATE") {
        asset_type_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.dup")));
        CONJURE_CHECK_ERROR(
          registry.register_type(named_desc("conjure.test.dup", 2)),
          assets_error::TYPE_DUPLICATE
        );
        CHECK(registry.size() == 1);
    }

    TEST_CASE("asset_type_registry - invalid descriptions are rejected") {
        asset_type_registry registry = {};
        asset_type_desc invalid = {};
        CONJURE_CHECK_ERROR(registry.register_type(std::move(invalid)), core::error_code::INVALID_ARGUMENT);

        asset_type_desc mismatched = named_desc("conjure.test.a");
        mismatched.id = make_asset_type_id("conjure.test.b");
        CONJURE_CHECK_ERROR(registry.register_type(std::move(mismatched)), core::error_code::INVALID_ARGUMENT);

        asset_type_desc wrongLoader = named_desc("conjure.test.c");
        wrongLoader.loader = std::make_unique<test_loader>();
        CONJURE_CHECK_ERROR(registry.register_type(std::move(wrongLoader)), core::error_code::INVALID_ARGUMENT);
        CHECK(registry.size() == 0);
    }

    TEST_CASE("asset_type_registry - MAX_ASSET_TYPES overflow fails and pointers stay valid") {
        asset_type_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.first")));
        const asset_type_desc *first = registry.find("conjure.test.first");
        for (usize index = 1; index < MAX_ASSET_TYPES; ++index) {
            CONJURE_REQUIRE_OK(registry.register_type(named_desc(std::format("conjure.test.type{}", index))));
        }
        CHECK(registry.size() == MAX_ASSET_TYPES);
        CONJURE_CHECK_ERROR(
          registry.register_type(named_desc("conjure.test.overflow")),
          core::error_code::OUT_OF_RANGE
        );
        CHECK(registry.find("conjure.test.first") == first);
        CHECK(first->name == "conjure.test.first");
    }

    TEST_CASE("asset_type_registry - unregister removes the type and reports unknown ids") {
        asset_type_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.a")));
        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.b")));
        CONJURE_REQUIRE_OK(registry.unregister_type(make_asset_type_id("conjure.test.a")));
        CHECK(registry.find("conjure.test.a") == nullptr);
        REQUIRE(registry.find("conjure.test.b") != nullptr);
        CHECK(registry.size() == 1);
        CONJURE_CHECK_ERROR(
          registry.unregister_type(make_asset_type_id("conjure.test.a")),
          assets_error::TYPE_UNKNOWN
        );
    }

    TEST_CASE("regression - unregistering a type leaves every other description in place") {
        asset_type_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.a")));
        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.b")));
        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.c")));
        const asset_type_desc *b = registry.find("conjure.test.b");
        const asset_type_desc *c = registry.find("conjure.test.c");
        REQUIRE(b != nullptr);
        REQUIRE(c != nullptr);

        CONJURE_REQUIRE_OK(registry.unregister_type(make_asset_type_id("conjure.test.a")));
        CHECK(registry.find("conjure.test.b") == b);
        CHECK(registry.find("conjure.test.c") == c);
        CHECK(b->name == "conjure.test.b");
        REQUIRE(registry.types().size() == 2);
        CHECK(registry.types()[0] == b);
        CHECK(registry.types()[1] == c);

        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.d")));
        CHECK(registry.find("conjure.test.b") == b);
        CHECK(registry.find("conjure.test.c") == c);
        CHECK(registry.types().size() == 3);
    }

    TEST_CASE("asset_type_registry - unregister is refused while a slot of the type is loaded") {
        pipeline_fixture fixture = {};
        auto source = std::make_unique<memory_asset_source>();
        const asset_id id = test_id(1);
        source->add(make_test_blob(id, "loaded"));
        fixture.manager->swap_source(std::move(source));

        asset_handle handle = fixture.manager->load(id);
        settle(*fixture.manager);
        REQUIRE(handle.state() == asset_state::LOADED);
        CONJURE_CHECK_ERROR(
          fixture.manager->types().unregister_type(asset_type_traits<test_asset>::id()),
          core::error_code::INVALID_STATE
        );

        handle.reset();
        CHECK(fixture.manager->collect() == 1);
        CONJURE_CHECK_OK(fixture.manager->types().unregister_type(asset_type_traits<test_asset>::id()));
        CHECK(fixture.manager->types().find(asset_type_traits<test_asset>::id()) == nullptr);
    }

    TEST_CASE("asset_type_registry - hooks observe registration and removal") {
        asset_type_registry registry = {};
        std::string registered = {};
        asset_type_id removed = asset_type_id::INVALID;
        bool inUse = true;
        asset_type_registry_hooks hooks = {};
        hooks.onRegistered = [&registered](const asset_type_desc &desc) {
            registered = desc.name;
        };
        hooks.onUnregistered = [&removed](asset_type_id id) {
            removed = id;
        };
        hooks.isInUse = [&inUse](asset_type_id) {
            return inUse;
        };
        registry.set_hooks(std::move(hooks));

        CONJURE_REQUIRE_OK(registry.register_type(named_desc("conjure.test.hooked")));
        CHECK(registered == "conjure.test.hooked");
        CONJURE_CHECK_ERROR(
          registry.unregister_type(make_asset_type_id("conjure.test.hooked")),
          core::error_code::INVALID_STATE
        );
        inUse = false;
        CONJURE_REQUIRE_OK(registry.unregister_type(make_asset_type_id("conjure.test.hooked")));
        CHECK(removed == make_asset_type_id("conjure.test.hooked"));
    }

    TEST_CASE("CONJURE_DECLARE_ASSET_TYPE - the traits id matches the registered description") {
        asset_type_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_type(make_asset_type_desc<other_asset>(std::make_unique<other_loader>())));
        const asset_type_desc *desc = registry.find(asset_type_traits<other_asset>::id());
        REQUIRE(desc != nullptr);
        CHECK(desc->name == asset_type_traits<other_asset>::NAME);
        CHECK(make_asset_type_id(desc->name) == asset_type_traits<other_asset>::id());
    }
}
