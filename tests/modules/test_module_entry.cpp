#include "conjure/testing/doctest.h"

#include "conjure/modules/module_abi.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_entry.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_service.h"
#include "conjure/modules/module_types.h"

#include <cstdint>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

// This translation unit plays the module: CONJURE_MODULE_NAME is what conjure_add_game_module()
// would define, and CONJURE_DECLARE_MODULE exports describe/entry from the test executable itself.
#define CONJURE_MODULE_NAME "test_host_module"

namespace {
    u32 sEntryRuns = 0;
    u32 sNullEntryRuns = 0;

    module_entry_status counting_entry(module_context &context) {
        ++sEntryRuns;
        context.registrar().register_managed_assembly("managed/Counting.dll");
        return module_entry_status::OK;
    }

    module_entry_status failing_entry(module_context &) {
        return module_entry_status::FAILED;
    }

    CONJURE_DECLARE_STATIC_MODULE(
      test_static_counting,
      R"({"schema": 1, "name": "test.static.counting", "version": "1.0.0", "engine": "*"})",
      &counting_entry
    )

    CONJURE_DECLARE_STATIC_MODULE(
      test_static_manifest_only,
      R"({"schema": 1, "name": "test.static.manifest_only", "version": "1.0.0", "engine": "*"})",
      nullptr
    )

    struct context_fixture {
        module_manifest manifest = {};
        service_table services = {};
        module_registration_set set = {};
        module_registrar registrar;         // no default: binds to the set above
        module_context context;             // no default: built from the fields above

        context_fixture() :
            registrar(set),
            context(module_id{0, 0}, module_host_kind::TEST, host_engine_version(), manifest, {}, services) {
            manifest.name = "test_host_module";
            context.begin_entry(registrar);
        }
    };
}

CONJURE_DECLARE_MODULE(counting_entry)

TEST_SUITE("modules/module_entry") {

    TEST_CASE("static_module_desc - the macro yields a static descriptor with the identifier and manifest text") {
        const static_module_desc &desc = conjure_static_module_test_static_counting();
        CHECK(std::string_view(desc.identifier) == "test_static_counting");
        CHECK(std::string_view(desc.manifestJson).find("test.static.counting") != std::string::npos);
        REQUIRE(desc.entry != nullptr);
        CHECK(&conjure_static_module_test_static_counting() == &desc);

        const static_module_desc defaulted = {};
        CHECK(std::string_view(defaulted.identifier).empty());
        CHECK(std::string_view(defaulted.manifestJson).empty());
        CHECK(defaulted.entry == nullptr);
    }

    TEST_CASE("static_module_desc - a null entry makes a manifest-only module") {
        const static_module_desc &desc = conjure_static_module_test_static_manifest_only();
        CHECK(desc.entry == nullptr);
        CHECK(std::string_view(desc.identifier) == "test_static_manifest_only");
        CHECK(modules::detail::static_entry<nullptr>() == nullptr);
        CHECK(sNullEntryRuns == 0);
    }

    TEST_CASE("entry_thunk - adapts a C++ entry to the C signature and forwards its status") {
        context_fixture fixture = {};
        const static_module_desc &desc = conjure_static_module_test_static_counting();
        sEntryRuns = 0;

        const std::uint32_t status = desc.entry(&fixture.context);
        CHECK(status == static_cast<std::uint32_t>(module_entry_status::OK));
        CHECK(sEntryRuns == 1);
        CHECK(fixture.set.managedAssemblies == std::vector<std::string>{"managed/Counting.dll"});

        const conjure_module_entry_fn failing = modules::detail::static_entry<&failing_entry>();
        REQUIRE(failing != nullptr);
        CHECK(failing(&fixture.context) == static_cast<std::uint32_t>(module_entry_status::FAILED));
        CHECK(modules::detail::static_entry<&counting_entry>() == desc.entry);
    }

    TEST_CASE("CONJURE_DECLARE_MODULE - describe returns the host signature and the module name") {
        const conjure_module_info *info = conjure_module_describe();
        REQUIRE(info != nullptr);
        CHECK(std::string_view(info->name) == "test_host_module");
        CHECK(compare_abi(info->abi, host_abi_signature()).has_value());
        CHECK(conjure_module_describe() == info);
    }

    TEST_CASE("CONJURE_DECLARE_MODULE - entry forwards to the declared function") {
        context_fixture fixture = {};
        sEntryRuns = 0;
        CHECK(conjure_module_entry(&fixture.context) == static_cast<std::uint32_t>(module_entry_status::OK));
        CHECK(sEntryRuns == 1);
        CHECK(fixture.set.managedAssemblies.size() == 1);
    }
}
