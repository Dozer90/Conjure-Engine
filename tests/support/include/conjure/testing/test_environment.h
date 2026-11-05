#pragma once

#include "conjure/core/types.h"

#include <filesystem>
#include <string>
#include <string_view>

// The process-wide description of one test run (docs/design/08_testing.md 2.2 and 3.2).
//
// test_main.cpp fills the environment from the --conjure-* arguments, the CONJURE_TEST_* environment
// variables and the compile definitions conjure_add_test_module() sets, then every test reads it
// through environment(). Tests never read environment variables themselves.
//
// Precedence: a --conjure-* argument wins over its CONJURE_TEST_* variable, which wins over the
// compiled-in default. CONJURE_TEST_DOWNLOADED_ASSET_ROOT is the one variable that overrides a
// compile definition; it exists so the downloaded-assets precondition can be tested against a
// scratch directory.

namespace conjure::testing {

    /// <summary>
    /// Exit code understood by CTest's SKIP_RETURN_CODE (D12). Same convention as Autotools and Meson.
    /// </summary>
    inline constexpr int SKIP_EXIT_CODE = 77;

    /// <summary>
    /// Something a test needs from the machine before it can run. Checked with CONJURE_TEST_REQUIRE.
    /// </summary>
    enum class precondition : u8 {
        DOTNET_RUNTIME,         // scripting::host::is_runtime_available(); probe installed by the engine library
        DOWNLOADED_ASSETS,      // tests/.assets_ready stamp present next to tests/.downloaded
        SOFTWARE_GPU_ADAPTER,   // rhi can create a device with preferSoftwareAdapter; probe installed by rhi tests
        LOOPBACK_TCP,           // a 127.0.0.1 socket can be bound; probe installed by the editor tests
        REAL_WINDOW,            // CONJURE_TEST_WINDOW=1 set by a developer; never in CI
        COUNT
    };

    /// <summary>
    /// The enumerator name of a precondition ("DOTNET_RUNTIME"), used in skip messages.
    /// </summary>
    [[nodiscard]] std::string_view to_string(precondition value) noexcept;

    /// <summary>
    /// Settings of the running test executable.
    /// </summary>
    struct test_environment {
        u64 seed = 0x5EED;                              // CONJURE_TEST_SEED / --conjure-seed
        u32 jobThreadCount = 2;                         // CONJURE_TEST_THREADS or clamp(hw, 2, 8)
        bool isolatedProcess = false;                   // --conjure-isolated (set by CTest discovery)
        bool goldenEnabled = false;                     // CONJURE_TEST_GOLDEN_IMAGES compile option
        bool updateGoldens = false;                     // --conjure-update-golden
        bool realWindowAllowed = false;                 // CONJURE_TEST_WINDOW=1
        bool continueOnAssert = false;                  // --conjure-continue-on-assert, CONJURE_TEST_CONTINUE_ON_ASSERT
        std::filesystem::path repoAssetRoot = {};       // <repo>/tests/assets (compile definition)
        std::filesystem::path downloadedAssetRoot = {}; // <repo>/tests/.downloaded, overridable through
                                                        // CONJURE_TEST_DOWNLOADED_ASSET_ROOT; empty when absent
        std::filesystem::path outputRoot = {};          // <build>/test_output/<executable>
        std::filesystem::path temporaryRoot = {};       // <build>/test_tmp/<executable>; temp_directory lives here
        std::string executableName = {};                // the CMake target, e.g. conjure_core_math_tests
    };

    /// <summary>
    /// The environment test_main installed. Before test_main runs (helper programs) it holds the
    /// defaults above.
    /// </summary>
    [[nodiscard]] const test_environment &environment() noexcept;

    /// <summary>
    /// True when the precondition is met right now. Never skips or exits; CONJURE_TEST_REQUIRE
    /// is what cases use.
    /// </summary>
    [[nodiscard]] bool precondition_holds(precondition value) noexcept;

    /// <summary>
    /// True when the precondition holds. Otherwise, in an isolated process, prints the reason and
    /// exits with SKIP_EXIT_CODE so CTest reports the case as Not Run; in a direct run it logs a
    /// doctest MESSAGE and returns false so the case can return early.
    /// </summary>
    [[nodiscard]] bool require_precondition(precondition value, const char *file, u32 line);

    /// <summary>
    /// A probe answering whether a precondition that depends on another module holds. The core
    /// support library cannot ask the scripting host or the rhi itself; the libraries that can
    /// install a probe at start-up. Preconditions without a probe never hold.
    /// </summary>
    using precondition_probe = bool (*)() noexcept;

    /// <summary>
    /// Installs (or, with nullptr, removes) the probe for a module-dependent precondition.
    /// </summary>
    void set_precondition_probe(precondition value, precondition_probe probe) noexcept;

    namespace detail {
        /// <summary>
        /// Publishes the environment and remembers the calling thread as the test thread. Called
        /// once by test_main before doctest runs.
        /// </summary>
        void set_environment(const test_environment &value);

        /// <summary>
        /// True on the thread that called set_environment (doctest's thread); false on helpers and
        /// job workers, and everywhere before set_environment ran.
        /// </summary>
        [[nodiscard]] bool is_test_thread() noexcept;
    }
}
