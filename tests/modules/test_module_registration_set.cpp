#include "conjure/testing/doctest.h"

#include "fixture_descriptors.h"

#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_registration_set.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/string_id.h"

#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

TEST_SUITE("modules/module_registration_set") {

    TEST_CASE("module_registration_set - a default set is empty and rejects nothing") {
        const module_registration_set set = {};
        CHECK(set.managedAssemblies.empty());
        CHECK(set.extensions.empty());
        CHECK(set.extensionStorage.empty());
        CHECK(set.errors.empty());
        CHECK(set.lifecycle.onActivated == nullptr);
        CHECK(set.lifecycle.onDeactivating == nullptr);
        CHECK(extensions_of<fixtures::component_desc>(set).empty());
    }

    TEST_CASE("extension_record - defaults to a null point at offset zero") {
        const extension_record record = {};
        CHECK(record.point.is_null());
        CHECK(record.offset == 0);
        CHECK(record.size == 0);
        CHECK(record.alignment == 0);
    }

    TEST_CASE("extensions_of - returns the records of one point in registration order and ignores other points") {
        module_registration_set set = {};
        module_registrar registrar(set);
        registrar.register_extension(fixtures::component_desc{"first", 4, 4});
        registrar.register_extension(fixtures::system_desc{"between", 9});
        registrar.register_extension(fixtures::component_desc{"second", 8, 8});

        const small_vector<const fixtures::component_desc *, 8> components =
          extensions_of<fixtures::component_desc>(set);
        REQUIRE(components.size() == 2);
        CHECK(std::string_view(components[0]->name) == "first");
        CHECK(components[0]->size == 4);
        CHECK(std::string_view(components[1]->name) == "second");
        CHECK(components[1]->alignment == 8);

        const small_vector<const fixtures::system_desc *, 8> systems = extensions_of<fixtures::system_desc>(set);
        REQUIRE(systems.size() == 1);
        CHECK(systems[0]->phase == 9);
        CHECK(extensions_of<fixtures::panel_desc>(set).empty());
    }

    TEST_CASE("extensions_of - skips records whose size does not match the descriptor type") {
        module_registration_set set = {};
        module_registrar registrar(set);
        registrar.register_extension(fixtures::component_desc{"ok", 1, 1});

        // A record under the component point written by another descriptor layout.
        extension_record foreign = {};
        foreign.point = fixtures::component_desc::EXTENSION_POINT;
        foreign.offset = 0;
        foreign.size = 1;
        foreign.alignment = 1;
        set.extensions.push_back(foreign);

        // A record pointing past the storage.
        extension_record truncated = {};
        truncated.point = fixtures::component_desc::EXTENSION_POINT;
        truncated.offset = static_cast<u32>(set.extensionStorage.size());
        truncated.size = sizeof(fixtures::component_desc);
        truncated.alignment = alignof(fixtures::component_desc);
        set.extensions.push_back(truncated);

        const small_vector<const fixtures::component_desc *, 8> components =
          extensions_of<fixtures::component_desc>(set);
        REQUIRE(components.size() == 1);
        CHECK(std::string_view(components[0]->name) == "ok");
    }

    TEST_CASE("module_lifecycle - callbacks are plain function pointers that default to null") {
        module_lifecycle lifecycle = {};
        CHECK(lifecycle.onActivated == nullptr);
        lifecycle.onActivated = [](module_context &) {};
        CHECK(lifecycle.onActivated != nullptr);
        CHECK(lifecycle.onDeactivating == nullptr);
    }
}
