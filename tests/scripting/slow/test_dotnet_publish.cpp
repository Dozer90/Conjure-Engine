#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "conjure/scripting/dotnet_script_host.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"

#include <filesystem>
#include <string>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    /// <summary>
    /// The runtime identifier of this machine for dotnet publish.
    /// </summary>
    [[nodiscard]] std::string host_rid() {
#if CONJURE_PLATFORM_WINDOWS
        const char *os = "win";
#elif CONJURE_PLATFORM_MACOS
        const char *os = "osx";
#else
        const char *os = "linux";
#endif
#if CONJURE_ARCH_ARM64
        const char *arch = "arm64";
#else
        const char *arch = "x64";
#endif
        return std::string(os) + "-" + arch;
    }
}

TEST_SUITE("scripting/dotnet_publish") {

    TEST_CASE(
      "dotnet_script_host - a self-contained publish hosts the runtime [dotnet][slow]" * doctest::timeout(600)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        temp_directory directory = {};
        core::process_desc publish = {};
        publish.executable = "dotnet";
        // -o redirects the publish layout only; the restore and build intermediates follow --artifacts-path,
        // so nothing lands beside the project in the source tree (CONVENTIONS 6: file I/O stays inside
        // the temp_directory).
        publish.arguments = {
          "publish", bootstrap_project().string(), "-c", "Release", "-r", host_rid(), "--self-contained", "true",
          "-o", directory.file("managed").string(), "--artifacts-path", directory.file("artifacts").string(),
          "--nologo", "-p:ContinuousIntegrationBuild=true"
        };
        // dotnet looks for global.json from its working directory: the project's own directory makes the SDK the
        // repository pins build the anchor wherever the test runs from (code sweep F778).
        publish.workingDirectory = bootstrap_project().parent_path();
        publish.mergeStderr = true;
        publish.timeoutSeconds = 540.0;
        publish.environment = {{"DOTNET_CLI_TELEMETRY_OPTOUT", "1"}, {"DOTNET_NOLOGO", "1"}, {"MSBUILDNODEREUSE", "0"}};
        const core::result<core::process_result> published = core::process::run(publish);
        CONJURE_REQUIRE_OK(published);
        REQUIRE_MESSAGE(published.value().exitCode == 0, published.value().standardOutput);

        const std::filesystem::path layout = directory.file("managed");
        CONJURE_REQUIRE_OK(validate_managed_directory(layout));
        const core::result<host_mode> mode = detect_host_mode(layout);
        CONJURE_REQUIRE_OK(mode);
        CHECK(mode.value() == host_mode::SELF_CONTAINED);
        std::error_code probe = {};
        bool hasHostfxr = false;
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(layout, probe)) {
            if (entry.path().filename().string().find("hostfxr") != std::string::npos) {
                hasHostfxr = true;
            }
        }
        CHECK(hasHostfxr);

        // In an isolated process this starts the self-contained runtime; in a shared process the host
        // attaches to the runtime already running (D1) with a warning.
        dotnet_script_host host = {};
        conjure_native_api api = make_native_api(nullptr);
        fill_core_native_entries(api);
        script_host_config config = {};
        config.managedDirectory = layout;
        CONJURE_REQUIRE_OK(host.initialise(config, api));
        CHECK(host.status() == script_host_status::READY);
        if (!logs.contains(core::log_level::WARN, "attaching the host")) {
            CHECK(host.mode() == host_mode::SELF_CONTAINED);
            CHECK(host.managed_directory() == layout);
        }
        CHECK(host.managed_api().run_phase(static_cast<uint8_t>(script_phase::UPDATE), 0.0f) == 0);
        host.shutdown();
    }
}
