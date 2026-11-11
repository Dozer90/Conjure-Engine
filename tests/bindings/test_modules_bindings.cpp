#include "conjure/testing/doctest.h"

#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/core/event_bus.h"
#include "conjure/core/hash.h"
#include "conjure/core/string_id.h"
#include "conjure/modules/module_entry.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"

#include <array>
#include <string>
#include <string_view>

// modules_* (docs/design/15_bindings.md 10, B12, R18): the read-only, index-based view over a registry holding
// manifest-only static modules (the engine's built-in libraries) and a plugin.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr const char *ENGINE_MANIFEST =
      R"({"schema": 1, "name": "conjure.ecs", "version": "0.1.0", "kind": "engine", "engine": "*"})";
    constexpr const char *PLUGIN_MANIFEST =
      R"({"schema": 1, "name": "test.sample_plugin", "version": "1.2.3", "kind": "plugin", "engine": "*", )"
      R"("requires": ["physics", "audio"]})";

    /// <summary>
    /// A bare fixture plus a module manager holding one manifest-only engine library and one plugin, both
    /// LOADED, with the registry installed into the binding context.
    /// </summary>
    struct modules_fixture {
        core::event_bus events = {};
        modules::module_manager_desc desc = {};
        modules::module_manager manager;
        modules::static_module_desc engineDesc = {};
        modules::static_module_desc pluginDesc = {};
        bare_fixture base = {};

        modules_fixture() :
            desc(make_desc()),
            manager(desc, events) {
            engineDesc.identifier = "conjure_ecs";
            engineDesc.manifestJson = ENGINE_MANIFEST;
            pluginDesc.identifier = "sample_plugin";
            pluginDesc.manifestJson = PLUGIN_MANIFEST;
            CONJURE_REQUIRE_OK(manager.add_static_module(engineDesc));
            CONJURE_REQUIRE_OK(manager.add_static_module(pluginDesc));
            CONJURE_REQUIRE_OK(manager.load_all());
            base.context.modules = &manager.registry();
            base.fill();
        }

        [[nodiscard]] static modules::module_manager_desc make_desc() noexcept {
            modules::module_manager_desc value = {};
            value.host = modules::module_host_kind::TEST;
            value.allowSharedLibraries = false;
            value.engineFeatures = modules::module_feature_flags::PHYSICS | modules::module_feature_flags::AUDIO;
            return value;
        }
    };
}

TEST_SUITE("bindings/modules_bindings") {

    TEST_CASE("regression - modules_find hashes the queried name without interning it") {
        modules_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        constexpr std::string_view NEVER_INTERNED = "never.interned.module.9c1e";
        const utf8_argument query = utf8(NEVER_INTERNED);
        CHECK(api.modules_find(query.data, query.length) == to_status(bindings_error::NOT_FOUND));
        CHECK(core::string_id::from_hash(core::hash::fnv1a64(NEVER_INTERNED)).view().empty());
        const utf8_argument known = utf8("test.sample_plugin");
        CHECK(api.modules_find(known.data, known.length) == 1);
    }

    TEST_CASE("modules - count and registration order") {
        modules_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        CHECK(api.modules_count() == 2);
        const std::string first = read_string([&](char *buffer, int32_t capacity) {
            return api.modules_name(0, buffer, capacity);
        });
        const std::string second = read_string([&](char *buffer, int32_t capacity) {
            return api.modules_name(1, buffer, capacity);
        });
        CHECK(first == "conjure.ecs");
        CHECK(second == "test.sample_plugin");
    }

    TEST_CASE("modules - find known and unknown names") {
        modules_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument plugin = utf8("test.sample_plugin");
        const utf8_argument ecs = utf8("conjure.ecs");
        const utf8_argument unknown = utf8("nope.plugin");
        CHECK(api.modules_find(plugin.data, plugin.length) == 1);
        CHECK(api.modules_find(ecs.data, ecs.length) == 0);
        CHECK(api.modules_find(unknown.data, unknown.length) == to_status(bindings_error::NOT_FOUND));
        CHECK(last_error_message() == "modules_find: nope.plugin");
        CHECK(api.modules_find(nullptr, 4) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.modules_find(nullptr, 0) == to_status(bindings_error::NOT_FOUND));
    }

    TEST_CASE("modules - info record fields") {
        modules_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        conjure_module_record record = {};
        CHECK(api.modules_info(1, &record) == 0);
        CHECK(record.version_major == 1);
        CHECK(record.version_minor == 2);
        CHECK(record.version_patch == 3);
        CHECK(record.state == static_cast<uint8_t>(modules::module_state::LOADED));
        CHECK(record.kind == static_cast<uint8_t>(modules::module_kind::PLUGIN));
        CHECK(
          record.feature_flags
          == static_cast<uint32_t>(modules::module_feature_flags::PHYSICS | modules::module_feature_flags::AUDIO)
        );
        CHECK(api.modules_info(0, &record) == 0);
        CHECK(record.kind == static_cast<uint8_t>(modules::module_kind::ENGINE));
        CHECK(record.feature_flags == 0);
        CHECK(record.version_minor == 1);
        CHECK(api.modules_info(2, &record) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.modules_info(-1, &record) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.modules_info(0, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("modules - name follows the needed-length protocol") {
        modules_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        std::array<char, 4> small = {};
        CHECK(api.modules_name(1, small.data(), 4) == 18);
        CHECK(std::string_view(small.data(), 4) == "test");
        CHECK(api.modules_name(1, nullptr, 0) == 18);
        CHECK(api.modules_name(5, small.data(), 4) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.modules_name(0, small.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("modules - manifest-only static modules are listed (R18)") {
        modules_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        conjure_module_record record = {};
        REQUIRE(api.modules_info(0, &record) == 0);
        CHECK(record.state == static_cast<uint8_t>(modules::module_state::LOADED));
        CHECK(record.kind == static_cast<uint8_t>(modules::module_kind::ENGINE));
    }

    TEST_CASE("modules - without a registry the group is stubbed") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        CHECK(api.modules_count() == to_status(bindings_error::NOT_SUPPORTED));
        conjure_module_record record = {};
        CHECK(api.modules_info(0, &record) == to_status(bindings_error::NOT_SUPPORTED));
    }
}
