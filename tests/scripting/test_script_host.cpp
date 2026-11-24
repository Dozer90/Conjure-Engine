#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "scripting_test_support.h"

#include "conjure/scripting/dotnet_script_host.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/scripting_error.h"

#include <memory>
#include <string>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;

namespace {

    constexpr std::string_view FRAMEWORK_CONFIG =
      R"({"runtimeOptions":{"tfm":"net10.0","framework":{"name":"Microsoft.NETCore.App","version":"10.0.0"}}})";
    constexpr std::string_view SELF_CONTAINED_CONFIG =
      R"({"runtimeOptions":{"includedFrameworks":[{"name":"Microsoft.NETCore.App","version":"10.0.0"}]}})";

    void write_anchor_files(const temp_directory &directory, std::string_view config) {
        CONJURE_REQUIRE_OK(directory.write_text("Conjure.Bootstrap.dll", "not a real assembly"));
        CONJURE_REQUIRE_OK(directory.write_text("Conjure.Managed.dll", "not a real assembly"));
        CONJURE_REQUIRE_OK(directory.write_text("Conjure.Bootstrap.runtimeconfig.json", config));
    }
}

TEST_SUITE("scripting/script_host") {

    TEST_CASE("create_script_host - NULL_HOST creates a null host that initialises without a runtime") {
        std::unique_ptr<script_host> host = create_script_host(script_backend::NULL_HOST);
        REQUIRE(host != nullptr);
        CHECK(host->backend() == script_backend::NULL_HOST);
        CHECK(host->status() == script_host_status::UNINITIALISED);
        CHECK(host->managed_api().run_phase == nullptr);

        conjure_native_api api = make_native_api(nullptr);
        CONJURE_REQUIRE_OK(host->initialise({}, api));
        CHECK(host->status() == script_host_status::READY);
        CHECK(host->managed_api().run_phase != nullptr);
        CONJURE_CHECK_OK(validate_managed_api(host->managed_api()));
        host->shutdown();
        CHECK(host->status() == script_host_status::UNINITIALISED);
    }

    TEST_CASE("create_script_host - DOTNET creates the real host without starting anything") {
        std::unique_ptr<script_host> host = create_script_host(script_backend::DOTNET);
        REQUIRE(host != nullptr);
        CHECK(host->backend() == script_backend::DOTNET);
        CHECK(host->status() == script_host_status::UNINITIALISED);
        CHECK(dynamic_cast<dotnet_script_host *>(host.get()) != nullptr);
    }

    TEST_CASE("is_runtime_available - answers the same every time and never logs") {
        log_capture logs = {};
        const bool first = is_runtime_available();
        const bool second = is_runtime_available();
        CHECK(first == second);
        CHECK(logs.count() == 0);
    }

    TEST_CASE("query_host_capabilities - both hosting paths are reported") {
        const host_capabilities capabilities = query_host_capabilities();
        CHECK(capabilities.frameworkDependent);
        CHECK(capabilities.selfContained);
    }

    TEST_CASE("detect_host_mode - reads the runtime config of the managed directory") {
        temp_directory directory = {};
        SUBCASE("framework-dependent") {
            write_anchor_files(directory, FRAMEWORK_CONFIG);
            const core::result<host_mode> mode = detect_host_mode(directory.path());
            CONJURE_REQUIRE_OK(mode);
            CHECK(mode.value() == host_mode::FRAMEWORK_DEPENDENT);
        }
        SUBCASE("self-contained") {
            write_anchor_files(directory, SELF_CONTAINED_CONFIG);
            const core::result<host_mode> mode = detect_host_mode(directory.path());
            CONJURE_REQUIRE_OK(mode);
            CHECK(mode.value() == host_mode::SELF_CONTAINED);
        }
        SUBCASE("missing file") {
            CONJURE_CHECK_ERROR(detect_host_mode(directory.path()), scripting_error::RUNTIME_CONFIG_INVALID);
        }
        SUBCASE("malformed JSON") {
            write_anchor_files(directory, "{not json");
            CONJURE_CHECK_ERROR(detect_host_mode(directory.path()), scripting_error::RUNTIME_CONFIG_INVALID);
        }
        SUBCASE("no runtimeOptions") {
            write_anchor_files(directory, "{}");
            CONJURE_CHECK_ERROR(detect_host_mode(directory.path()), scripting_error::RUNTIME_CONFIG_INVALID);
        }
        SUBCASE("no framework at all") {
            write_anchor_files(directory, R"({"runtimeOptions":{"tfm":"net10.0"}})");
            CONJURE_CHECK_ERROR(detect_host_mode(directory.path()), scripting_error::RUNTIME_CONFIG_INVALID);
        }
    }

    TEST_CASE("validate_managed_directory - names the first missing anchor file") {
        temp_directory directory = {};
        CONJURE_CHECK_ERROR(validate_managed_directory({}), scripting_error::MANAGED_SDK_MISSING);
        const core::result<void> empty = validate_managed_directory(directory.path());
        CONJURE_CHECK_ERROR(empty, scripting_error::MANAGED_SDK_MISSING);
        CHECK(empty.get_error().message().find("Conjure.Bootstrap.dll") != std::string::npos);

        CONJURE_REQUIRE_OK(directory.write_text("Conjure.Bootstrap.dll", "x"));
        const core::result<void> noConfig = validate_managed_directory(directory.path());
        CONJURE_CHECK_ERROR(noConfig, scripting_error::MANAGED_SDK_MISSING);
        CHECK(noConfig.get_error().message().find("runtimeconfig.json") != std::string::npos);

        write_anchor_files(directory, FRAMEWORK_CONFIG);
        CONJURE_CHECK_OK(validate_managed_directory(directory.path()));
    }

    TEST_CASE("dotnet_script_host - a wrong managed directory fails without touching the runtime") {
        temp_directory directory = {};
        dotnet_script_host host = {};
        conjure_native_api api = make_native_api(nullptr);
        script_host_config config = {};
        config.managedDirectory = directory.path();
        CONJURE_CHECK_ERROR(host.initialise(config, api), scripting_error::MANAGED_SDK_MISSING);
        CHECK(host.status() == script_host_status::DISABLED);
        CHECK(host.managed_api().run_phase == nullptr);
        host.shutdown();
        CHECK(host.status() == script_host_status::UNINITIALISED);
    }

    TEST_CASE("to_string - host enumerations have stable names") {
        CHECK(to_string(script_host_status::UNINITIALISED) == "UNINITIALISED");
        CHECK(to_string(script_host_status::READY) == "READY");
        CHECK(to_string(script_host_status::DISABLED) == "DISABLED");
        CHECK(to_string(script_backend::DOTNET) == "DOTNET");
        CHECK(to_string(script_backend::NULL_HOST) == "NULL_HOST");
        CHECK(to_string(host_mode::FRAMEWORK_DEPENDENT) == "FRAMEWORK_DEPENDENT");
        CHECK(to_string(host_mode::SELF_CONTAINED) == "SELF_CONTAINED");
    }
}
