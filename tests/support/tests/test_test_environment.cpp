#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"
#include "support_child_runner.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"

#include <filesystem>
#include <string>
#include <system_error>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

namespace {
    bool probe_true() noexcept {
        return true;
    }

    bool probe_false() noexcept {
        return false;
    }

    struct probe_reset {
        precondition value = precondition::LOOPBACK_TCP;

        ~probe_reset() {
            set_precondition_probe(value, nullptr);
        }
    };

    // Runs the child's DOWNLOADED_ASSETS case against <scratch>/.downloaded, with or without the
    // <scratch>/.assets_ready stamp next to it.
    process_result run_downloaded_assets_child(const temp_directory &scratch, bool withStamp) {
        const std::filesystem::path root = scratch.file(".downloaded");
        std::error_code failure = {};
        std::filesystem::create_directories(root, failure);
        REQUIRE_MESSAGE(!failure, failure.message());
        if (withStamp) {
            CONJURE_REQUIRE_OK(scratch.write_text(".assets_ready", "ready"));
        }

        return run_support_child(
          {"-tc=support_child - downloaded assets precondition", "--conjure-isolated"},
          {{"CONJURE_TEST_DOWNLOADED_ASSET_ROOT", root.string()}}
        );
    }
}

TEST_SUITE("testing/test_environment") {

    /*-----------------------------------------------------------------------*/
    // In-process

    TEST_CASE("test_environment - defaults come from the compile definitions") {
        const test_environment &env = environment();

        CHECK(env.executableName == "conjure_support_tests");
        CHECK(env.repoAssetRoot.filename() == "assets");
        CHECK(env.repoAssetRoot.parent_path().filename() == "tests");
        CHECK(env.outputRoot.filename() == "conjure_support_tests");
        CHECK(env.temporaryRoot.filename() == "conjure_support_tests");
        CHECK(env.temporaryRoot.parent_path().filename() == "test_tmp");
        CHECK_FALSE(env.updateGoldens);
    }

    TEST_CASE("precondition - every value has a name") {
        CHECK(to_string(precondition::DOTNET_RUNTIME) == "DOTNET_RUNTIME");
        CHECK(to_string(precondition::DOWNLOADED_ASSETS) == "DOWNLOADED_ASSETS");
        CHECK(to_string(precondition::SOFTWARE_GPU_ADAPTER) == "SOFTWARE_GPU_ADAPTER");
        CHECK(to_string(precondition::LOOPBACK_TCP) == "LOOPBACK_TCP");
        CHECK(to_string(precondition::REAL_WINDOW) == "REAL_WINDOW");
        CHECK(to_string(precondition::COUNT) == "UNKNOWN");
    }

    TEST_CASE("precondition_holds - module preconditions are decided by their probe") {
        const probe_reset reset = {precondition::LOOPBACK_TCP};

        CHECK_FALSE(precondition_holds(precondition::LOOPBACK_TCP));

        set_precondition_probe(precondition::LOOPBACK_TCP, &probe_true);
        CHECK(precondition_holds(precondition::LOOPBACK_TCP));

        set_precondition_probe(precondition::LOOPBACK_TCP, &probe_false);
        CHECK_FALSE(precondition_holds(precondition::LOOPBACK_TCP));

        set_precondition_probe(precondition::LOOPBACK_TCP, nullptr);
        CHECK_FALSE(precondition_holds(precondition::LOOPBACK_TCP));
        CHECK_FALSE(precondition_holds(precondition::COUNT));
    }

    TEST_CASE("require_precondition - a met precondition returns true and continues") {
        const probe_reset reset = {precondition::SOFTWARE_GPU_ADAPTER};
        set_precondition_probe(precondition::SOFTWARE_GPU_ADAPTER, &probe_true);

        CHECK(require_precondition(precondition::SOFTWARE_GPU_ADAPTER, __FILE__, 1));
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        CHECK(true);
    }

    TEST_CASE("detail - the test thread is the one that installed the environment") {
        CHECK(testing::detail::is_test_thread());
    }

    /*-----------------------------------------------------------------------*/
    // Through the child executable

    TEST_CASE("require_precondition - unmet exits 77 in isolated mode") {
        const process_result run = run_support_child({"-tc=support_child - unmet precondition", "--conjure-isolated"});

        CHECK(run.exitCode == SKIP_EXIT_CODE);
        CHECK(output_contains(run.standardOutput, "skipped: precondition REAL_WINDOW not met"));
        CHECK_FALSE(output_contains(run.standardOutput, "REAL_WINDOW must not hold"));
    }

    TEST_CASE("require_precondition - unmet returns false in direct mode") {
        const process_result run = run_support_child({"-tc=support_child - unmet precondition"});

        CHECK(run.exitCode == 0);
        CHECK(output_contains(run.standardOutput, "skipped: precondition REAL_WINDOW not met"));
        CHECK_FALSE(output_contains(run.standardOutput, "REAL_WINDOW must not hold"));
    }

    TEST_CASE("precondition_holds - REAL_WINDOW holds only with CONJURE_TEST_WINDOW=1") {
        const process_result enabled = run_support_child(
          {"-tc=support_child - met precondition runs the case", "--conjure-isolated"},
          {{"CONJURE_TEST_WINDOW", "1"}}
        );
        CHECK(enabled.exitCode == 0);
        CHECK_FALSE(output_contains(enabled.standardOutput, "skipped: precondition"));

        const process_result disabled = run_support_child(
          {"-tc=support_child - met precondition runs the case", "--conjure-isolated"},
          {{"CONJURE_TEST_WINDOW", "0"}}
        );
        CHECK(disabled.exitCode == SKIP_EXIT_CODE);

        const process_result unset = run_support_child(
          {"-tc=support_child - met precondition runs the case", "--conjure-isolated"}
        );
        CHECK(unset.exitCode == SKIP_EXIT_CODE);
    }

    TEST_CASE("precondition_holds - DOWNLOADED_ASSETS needs the directory and the .assets_ready stamp") {
        const temp_directory scratch = {};

        const process_result withoutStamp = run_downloaded_assets_child(scratch, false);
        CHECK(withoutStamp.exitCode == SKIP_EXIT_CODE);
        CHECK(output_contains(withoutStamp.standardOutput, "skipped: precondition DOWNLOADED_ASSETS not met"));

        const process_result withStamp = run_downloaded_assets_child(scratch, true);
        CHECK(withStamp.exitCode == 0);
        CHECK_FALSE(output_contains(withStamp.standardOutput, "skipped: precondition"));

        // A root that does not exist is treated as absent, stamp or not.
        const process_result missingRoot = run_support_child(
          {"-tc=support_child - downloaded assets precondition", "--conjure-isolated"},
          {{"CONJURE_TEST_DOWNLOADED_ASSET_ROOT", scratch.file("does_not_exist").string()}}
        );
        CHECK(missingRoot.exitCode == SKIP_EXIT_CODE);
    }

    TEST_CASE("test_main - seed and threads come from the arguments") {
        const process_result run = run_support_child({
          "-tc=support_child - environment reflects the arguments",
          "--conjure-seed=0x1234",
          "--conjure-threads=3",
        });

        CHECK(run.exitCode == 0);
        CHECK(output_contains(run.standardError, "seed=4660 threads=3"));
        CHECK_FALSE(output_contains(run.standardOutput, "seed="));   // stdout belongs to the reporter
    }

    TEST_CASE("test_main - environment variables are the fallback for the arguments") {
        const process_result run = run_support_child(
          {"-tc=support_child - passes"},
          {{"CONJURE_TEST_SEED", "77"}, {"CONJURE_TEST_THREADS", "5"}}
        );

        CHECK(run.exitCode == 0);
        CHECK(output_contains(run.standardError, "seed=77 threads=5"));
    }

    TEST_CASE("test_main - arguments win over environment variables") {
        const process_result run = run_support_child(
          {"-tc=support_child - passes", "--conjure-seed=9"},
          {{"CONJURE_TEST_SEED", "77"}}
        );

        CHECK(run.exitCode == 0);
        CHECK(output_contains(run.standardError, "seed=9 "));
    }

    TEST_CASE("test_main - an unknown --conjure option exits 3") {
        const process_result run = run_support_child({"-tc=support_child - passes", "--conjure-bogus"});

        CHECK(run.exitCode == 3);
        CHECK(output_contains(run.standardError, "unknown option '--conjure-bogus'"));
    }

    TEST_CASE("test_main - an invalid seed exits 3") {
        const process_result run = run_support_child({"-tc=support_child - passes", "--conjure-seed=abc"});

        CHECK(run.exitCode == 3);
    }

    TEST_CASE("test_main - listing test cases prints nothing but the list") {
        const process_result plain = run_support_child({"--list-test-cases"});
        CHECK(plain.exitCode == 0);
        CHECK_FALSE(output_contains(plain.standardOutput, "seed="));
        CHECK_FALSE(output_contains(plain.standardError, "seed="));
        CHECK(output_contains(plain.standardOutput, "support_child - passes"));

        // doctest also accepts every option with its dt- prefix.
        const process_result prefixed = run_support_child({"--dt-list-test-cases"});
        CHECK(prefixed.exitCode == 0);
        CHECK_FALSE(output_contains(prefixed.standardError, "seed="));
        CHECK(output_contains(prefixed.standardOutput, "support_child - passes"));

        const process_result shortForm = run_support_child({"-dt-ltc"});
        CHECK(shortForm.exitCode == 0);
        CHECK_FALSE(output_contains(shortForm.standardError, "seed="));
    }

    TEST_CASE("test_main - the xml reporter's document is not corrupted by the banner") {
        const process_result run = run_support_child({"-tc=support_child - passes", "--reporters=xml"});

        CHECK(run.exitCode == 0);
        CHECK(run.standardOutput.starts_with("<?xml"));
        CHECK(output_contains(run.standardError, "seed="));
    }

    TEST_CASE("test_main - an unexpected assert fails the case and aborts in direct and isolated mode") {
        const process_result direct = run_support_child({"-tc=support_child - unexpected assert fails the case"});
        const process_result isolated = run_support_child(
          {"-tc=support_child - unexpected assert fails the case", "--conjure-isolated"}
        );

#if CONJURE_ASSERTS_ENABLED
        CHECK(direct.exitCode != 0);
        CHECK(direct.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(direct.standardOutput, "Unexpected assertion failure"));
        CHECK_FALSE(output_contains(direct.standardOutput, "reached after the assert"));
        CHECK(output_contains(direct.standardError, "log captured before the assertion"));

        CHECK(isolated.exitCode != 0);
        CHECK(isolated.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(isolated.standardOutput, "Unexpected assertion failure"));
        CHECK_FALSE(output_contains(isolated.standardOutput, "reached after the assert"));
#else
        // The assert is compiled out, so the case simply passes.
        CHECK(direct.exitCode == 0);
        CHECK(output_contains(direct.standardOutput, "reached after the assert"));
        CHECK(isolated.exitCode == 0);
#endif
    }

    TEST_CASE("test_main - continue-on-assert keeps a direct run going after the failure") {
        const process_result argument = run_support_child(
          {"-tc=support_child - unexpected assert fails the case", "--conjure-continue-on-assert"}
        );
        const process_result variable = run_support_child(
          {"-tc=support_child - unexpected assert fails the case"},
          {{"CONJURE_TEST_CONTINUE_ON_ASSERT", "1"}}
        );

#if CONJURE_ASSERTS_ENABLED
        CHECK(argument.exitCode != 0);
        CHECK(argument.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(argument.standardOutput, "Unexpected assertion failure"));
        CHECK(output_contains(argument.standardOutput, "reached after the assert"));

        CHECK(variable.exitCode != 0);
        CHECK(output_contains(variable.standardOutput, "reached after the assert"));
#else
        CHECK(argument.exitCode == 0);
        CHECK(variable.exitCode == 0);
#endif
    }
}
