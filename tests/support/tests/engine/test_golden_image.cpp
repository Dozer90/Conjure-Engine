#include "conjure/testing/doctest.h"

#include "conjure/testing/golden_image.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"
#include "support_engine_child_runner.h"

#include "conjure/core/error.h"
#include "conjure/core/image.h"
#include "conjure/core/process.h"
#include "conjure/core/result.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

namespace {
    constexpr u32 WIDTH = 64;
    constexpr u32 HEIGHT = 32;

    [[nodiscard]] image solid(u32 width, u32 height, rgba8 colour) {
        image result(width, height);
        result.fill(colour);
        return result;
    }

    /// <summary>
    /// A copy of 'base' with a side x side square of 'colour' in its top-left corner.
    /// </summary>
    [[nodiscard]] image with_block(const image &base, u32 side, rgba8 colour) {
        image result = base;
        for (u32 y = 0; y < side; ++y) {
            for (u32 x = 0; x < side; ++x) {
                result.set_pixel(x, y, colour);
            }
        }
        return result;
    }

    [[nodiscard]] bool file_exists(const std::filesystem::path &path) {
        std::error_code ignored = {};
        return std::filesystem::is_regular_file(path, ignored);
    }

    /// <summary>
    /// Options pointing both roots into a scratch directory so nothing touches the repository.
    /// </summary>
    [[nodiscard]] golden_options scratch_options(const temp_directory &scratch) {
        golden_options options = {};
        options.referenceRoot = scratch.file("reference");
        options.outputRoot = scratch.file("output");
        return options;
    }

    /// <summary>
    /// Runs this executable again with --conjure-update-golden and a -tc filter (a comma-separated list of
    /// case names): updateGoldens is a start-up setting, so update mode is only observable from a second
    /// process (support_engine_child_runner.h clears the CONJURE_TEST_* variables of the shell).
    /// </summary>
    [[nodiscard]] process_result run_in_update_mode(std::string_view caseFilter) {
        return run_support_engine_program(
          CONJURE_SUPPORT_ENGINE_TESTS_PATH,
          {std::string("-tc=") + std::string(caseFilter), "--conjure-isolated", "--conjure-update-golden"}
        );
    }
}

TEST_SUITE("testing/golden_image") {

    /*-----------------------------------------------------------------------*/
    // Pure comparison

    TEST_CASE("golden_image - identical images pass") {
        const image reference = solid(WIDTH, HEIGHT, rgba8{10, 20, 30, 255});
        const golden_result verdict = golden_image::compare_images(reference, reference, golden_options{});

        CHECK(verdict.passed);
        CHECK_FALSE(verdict.dimensionMismatch);
        CHECK(verdict.mismatchPixelCount == 0);
        CHECK(verdict.mismatchFraction == doctest::Approx(0.0f));
        CHECK(verdict.meanAbsoluteError == doctest::Approx(0.0f));
        CHECK(verdict.message.starts_with("pass"));
    }

    TEST_CASE("golden_image - one pixel off by five passes") {
        const image reference = solid(WIDTH, HEIGHT, rgba8{100, 100, 100, 255});
        image actual = reference;
        actual.set_pixel(3, 4, rgba8{105, 100, 100, 255});

        const golden_result verdict = golden_image::compare_images(reference, actual, golden_options{});

        CHECK(verdict.passed);
        CHECK(verdict.mismatchPixelCount == 1);
        CHECK(verdict.mismatchFraction == doctest::Approx(1.0f / static_cast<f32>(WIDTH * HEIGHT)));
        CHECK(verdict.meanAbsoluteError == doctest::Approx(5.0f / static_cast<f32>(WIDTH * HEIGHT * 4)));
    }

    TEST_CASE("golden_image - a delta within the channel tolerance is not a mismatch") {
        const image reference = solid(WIDTH, HEIGHT, rgba8{100, 100, 100, 255});
        image actual = reference;
        actual.set_pixel(0, 0, rgba8{103, 97, 100, 255});

        const golden_result verdict = golden_image::compare_images(reference, actual, golden_options{});

        CHECK(verdict.passed);
        CHECK(verdict.mismatchPixelCount == 0);
        CHECK(verdict.meanAbsoluteError > 0.0f);
    }

    TEST_CASE("golden_image - one percent of pixels off fails") {
        const image reference = solid(WIDTH, HEIGHT, rgba8{0, 0, 0, 255});
        image actual = reference;
        const u32 offPixels = WIDTH * HEIGHT / 100;   // 1 % of 2048 pixels
        for (u32 index = 0; index < offPixels; ++index) {
            actual.set_pixel(index % WIDTH, index / WIDTH, rgba8{50, 0, 0, 255});
        }

        const golden_result verdict = golden_image::compare_images(reference, actual, golden_options{});

        CHECK_FALSE(verdict.passed);
        CHECK(verdict.mismatchPixelCount == offPixels);
        CHECK(verdict.mismatchFraction > golden_options{}.maxMismatchFraction);
        CHECK(verdict.meanAbsoluteError <= golden_options{}.maxMeanAbsoluteError);
        CHECK(verdict.message.starts_with("fail"));
    }

    TEST_CASE("golden_image - a wholly wrong 33x33 block passes the default limits") {
        // The blind spot D11 documents: at 640x360 up to 0.5 % of the pixels (1,152) may be wholly wrong while the
        // mean absolute error stays under 1.0, so a small missing object passes and the reference review of 6.2 is
        // the guard. Limits tight enough to reject this block (a mean absolute error limit below 0.904 or a mismatch
        // fraction below 0.47 %) flip the first verdict.
        const golden_options defaults = {};
        const rgba8 white = rgba8{255, 255, 255, 255};
        const u32 blockSide = 33;
        const u32 blockPixels = blockSide * blockSide;
        const f32 pixelCount = static_cast<f32>(defaults.width * defaults.height);
        const image reference = solid(defaults.width, defaults.height, rgba8{0, 0, 0, 255});
        const image block = with_block(reference, blockSide, white);
        const image largerBlock = with_block(reference, blockSide + 1, white);
        const f32 expectedError = static_cast<f32>(blockPixels) * 3.0f * 255.0f / (pixelCount * 4.0f);   // 0.904

        const golden_result verdict = golden_image::compare_images(reference, block, defaults);

        CHECK(verdict.passed);
        CHECK(verdict.mismatchPixelCount == blockPixels);
        CHECK(verdict.mismatchFraction == doctest::Approx(static_cast<f32>(blockPixels) / pixelCount));
        CHECK(verdict.meanAbsoluteError == doctest::Approx(expectedError));
        CHECK(verdict.meanAbsoluteError < defaults.maxMeanAbsoluteError);
        CHECK(verdict.message.starts_with("pass"));

        // One row and one column more cross the 0.5 % line; the mismatch fraction fails it, not the mean error.
        const golden_result larger = golden_image::compare_images(reference, largerBlock, defaults);

        CHECK_FALSE(larger.passed);
        CHECK(larger.mismatchPixelCount == (blockSide + 1) * (blockSide + 1));
        CHECK(larger.mismatchFraction > defaults.maxMismatchFraction);
        CHECK(larger.meanAbsoluteError <= defaults.maxMeanAbsoluteError);
    }

    TEST_CASE("golden_image - a mean absolute error above the limit fails without any pixel mismatch") {
        const image reference = solid(WIDTH, HEIGHT, rgba8{100, 100, 100, 255});
        const image actual = solid(WIDTH, HEIGHT, rgba8{102, 102, 102, 255});

        const golden_result verdict = golden_image::compare_images(reference, actual, golden_options{});

        CHECK_FALSE(verdict.passed);
        CHECK(verdict.mismatchPixelCount == 0);
        CHECK(verdict.meanAbsoluteError == doctest::Approx(1.5f));   // three of four channels off by two
    }

    TEST_CASE("golden_image - looser options accept what the defaults reject") {
        const image reference = solid(WIDTH, HEIGHT, rgba8{0, 0, 0, 255});
        const image actual = solid(WIDTH, HEIGHT, rgba8{6, 0, 0, 255});
        golden_options loose = {};
        loose.channelTolerance = 8;
        loose.maxMeanAbsoluteError = 2.0f;

        CHECK_FALSE(golden_image::compare_images(reference, actual, golden_options{}).passed);
        CHECK(golden_image::compare_images(reference, actual, loose).passed);
    }

    TEST_CASE("golden_image - a size mismatch fails immediately") {
        const image reference = solid(WIDTH, HEIGHT, rgba8{});
        const image actual = solid(WIDTH, HEIGHT + 1, rgba8{});

        const golden_result verdict = golden_image::compare_images(reference, actual, golden_options{});

        CHECK_FALSE(verdict.passed);
        CHECK(verdict.dimensionMismatch);
        CHECK(verdict.mismatchPixelCount == 0);
        CHECK(verdict.message.find("dimension mismatch") != std::string::npos);
        CHECK(golden_image::make_diff(reference, actual).is_empty());
    }

    TEST_CASE("golden_image - two empty images compare as a pass") {
        const golden_result verdict = golden_image::compare_images(image(), image(), golden_options{});

        CHECK(verdict.passed);
        CHECK(verdict.mismatchPixelCount == 0);
    }

    TEST_CASE("golden_image - make_diff amplifies the channel delta eight times and clamps") {
        const image reference = solid(4, 2, rgba8{100, 100, 100, 255});
        image actual = reference;
        actual.set_pixel(1, 1, rgba8{101, 90, 160, 255});

        const image diff = golden_image::make_diff(reference, actual);

        REQUIRE(diff.width() == 4);
        REQUIRE(diff.height() == 2);
        CHECK(diff.pixel(0, 0) == rgba8{0, 0, 0, 255});
        CHECK(diff.pixel(1, 1) == rgba8{8, 80, 255, 255});
    }

    TEST_CASE("golden_image - make_diff draws an alpha-only difference as grey") {
        const image reference = solid(4, 2, rgba8{100, 100, 100, 255});
        image actual = reference;
        actual.set_pixel(1, 1, rgba8{100, 100, 100, 250});

        const image diff = golden_image::make_diff(reference, actual);

        REQUIRE(diff.width() == 4);
        REQUIRE(diff.height() == 2);
        CHECK(diff.pixel(0, 0) == rgba8{0, 0, 0, 255});
        CHECK(diff.pixel(1, 1) == rgba8{40, 40, 40, 255});

        // A fully transparent copy fails the comparison on alpha alone, and the diff shows every pixel of it.
        const image transparent = solid(4, 2, rgba8{100, 100, 100, 0});
        CHECK_FALSE(golden_image::compare_images(reference, transparent, golden_options{}).passed);
        const image transparentDiff = golden_image::make_diff(reference, transparent);
        for (u32 y = 0; y < transparentDiff.height(); ++y) {
            for (u32 x = 0; x < transparentDiff.width(); ++x) {
                CAPTURE(x);
                CAPTURE(y);
                CHECK(transparentDiff.pixel(x, y) == rgba8{255, 255, 255, 255});
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    // References on disk

    TEST_CASE("golden_image - a missing reference is reported and the actual image is written") {
        temp_directory scratch("golden_missing");
        const golden_options options = scratch_options(scratch);
        const image actual = solid(WIDTH, HEIGHT, rgba8{1, 2, 3, 255});

        if (environment().updateGoldens) {
            // The update-mode half of this case runs in the child process spawned below.
            const golden_result verdict = golden_image::compare("missing_scene", actual, options);
            CHECK(verdict.passed);
            CHECK(verdict.referenceMissing);
            CHECK(verdict.referenceWritten);
            CHECK(file_exists(verdict.referencePath));
            const result<image> written = image::load_file(verdict.referencePath);
            CONJURE_REQUIRE_OK(written);
            CHECK(written.value() == actual);
            return;
        }

        const golden_result verdict = golden_image::compare("missing_scene", actual, options);

        CHECK_FALSE(verdict.passed);
        CHECK(verdict.referenceMissing);
        CHECK_FALSE(verdict.referenceWritten);
        CHECK(verdict.referencePath == scratch.file("reference") / "missing_scene" / "null.png");
        CHECK_FALSE(file_exists(verdict.referencePath));
        CHECK(verdict.actualPath == scratch.file("output") / "missing_scene" / "null" / "actual.png");
        CHECK(file_exists(verdict.actualPath));
        CHECK(verdict.diffPath.empty());
        CHECK(verdict.message.find("--conjure-update-golden") != std::string::npos);
    }

    TEST_CASE("golden_image - update mode writes the missing reference and passes") {
        log_capture logs = {};

        const process_result run =
          run_in_update_mode("golden_image - a missing reference is reported and the actual image is written");

        CHECK_MESSAGE(run.exitCode == 0, run.standardOutput);
        CHECK(selected_case_count(run.standardOutput) == std::optional<u32>(1));
    }

    TEST_CASE("golden_image - update mode rewrites an out-of-date, resized or unreadable reference and passes") {
        // The update-mode halves of the three failing cases below; the passing comparison rides along to show
        // that update mode never rewrites a reference that passes. No case name holds a comma (-tc splits on it).
        log_capture logs = {};

        const process_result run = run_in_update_mode(
          "golden_image - an existing reference is compared and a failure writes actual and diff,"
          "golden_image - a reference of another size fails without a diff image,"
          "golden_image - an unreadable reference is reported as a failure,"
          "golden_image - a passing comparison writes nothing"
        );

        CHECK_MESSAGE(run.exitCode == 0, run.standardOutput);
        CHECK(selected_case_count(run.standardOutput) == std::optional<u32>(4));
    }

    TEST_CASE("golden_image - an existing reference is compared and a failure writes actual and diff") {
        temp_directory scratch("golden_failure");
        const golden_options options = scratch_options(scratch);
        const image reference = solid(WIDTH, HEIGHT, rgba8{40, 40, 40, 255});
        const std::filesystem::path referencePath = golden_image::reference_path("shadows", options);
        std::error_code failure = {};
        std::filesystem::create_directories(referencePath.parent_path(), failure);
        REQUIRE_FALSE(failure);
        CONJURE_REQUIRE_OK(reference.save_png(referencePath));

        const image actual = solid(WIDTH, HEIGHT, rgba8{90, 40, 40, 255});
        const golden_result verdict = golden_image::compare("shadows", actual, options);
        const result<image> stored = image::load_file(referencePath);
        CONJURE_REQUIRE_OK(stored);

        CHECK_FALSE(verdict.referenceMissing);
        CHECK(verdict.mismatchPixelCount == WIDTH * HEIGHT);
        if (environment().updateGoldens) {
            // The update-mode half of this case runs in the child process of the update-mode case above.
            CHECK(verdict.passed);
            CHECK(verdict.referenceWritten);
            CHECK(verdict.actualPath.empty());
            CHECK(verdict.diffPath.empty());
            CHECK(verdict.message.find("was out of date") != std::string::npos);
            CHECK(stored.value() == actual);
            CHECK_FALSE(std::filesystem::exists(scratch.file("output"), failure));
            return;
        }

        CHECK_FALSE(verdict.passed);
        CHECK_FALSE(verdict.referenceWritten);
        CHECK(stored.value() == reference);
        CHECK(verdict.actualPath == golden_image::output_directory("shadows", options) / "actual.png");
        CHECK(verdict.diffPath == golden_image::output_directory("shadows", options) / "diff.png");
        CHECK(file_exists(verdict.actualPath));
        CHECK(file_exists(verdict.diffPath));
        CHECK(verdict.message.starts_with("golden 'shadows': fail"));

        const result<image> diff = image::load_file(verdict.diffPath);
        CONJURE_REQUIRE_OK(diff);
        CHECK(diff->pixel(0, 0) == rgba8{255, 0, 0, 255});
    }

    TEST_CASE("golden_image - a passing comparison writes nothing") {
        temp_directory scratch("golden_pass");
        const golden_options options = scratch_options(scratch);
        const image reference = solid(WIDTH, HEIGHT, rgba8{40, 40, 40, 255});
        const std::filesystem::path referencePath = golden_image::reference_path("ibl_only", options);
        std::error_code failure = {};
        std::filesystem::create_directories(referencePath.parent_path(), failure);
        REQUIRE_FALSE(failure);
        CONJURE_REQUIRE_OK(reference.save_png(referencePath));

        const golden_result verdict = golden_image::compare("ibl_only", reference, options);

        CHECK(verdict.passed);
        CHECK_FALSE(verdict.referenceWritten);   // in update mode too: a pass is never rewritten
        CHECK(verdict.actualPath.empty());
        CHECK(verdict.diffPath.empty());
        CHECK_FALSE(std::filesystem::exists(scratch.file("output"), failure));
    }

    TEST_CASE("golden_image - a reference of another size fails without a diff image") {
        temp_directory scratch("golden_size");
        const golden_options options = scratch_options(scratch);
        const std::filesystem::path referencePath = golden_image::reference_path("sprites", options);
        std::error_code failure = {};
        std::filesystem::create_directories(referencePath.parent_path(), failure);
        REQUIRE_FALSE(failure);
        CONJURE_REQUIRE_OK(solid(8, 8, rgba8{}).save_png(referencePath));

        const golden_result verdict = golden_image::compare("sprites", solid(WIDTH, HEIGHT, rgba8{}), options);
        const result<image> stored = image::load_file(referencePath);
        CONJURE_REQUIRE_OK(stored);

        CHECK(verdict.dimensionMismatch);
        CHECK(verdict.diffPath.empty());
        if (environment().updateGoldens) {
            // Regenerated, never resized (3.5): the new reference has the actual image's size.
            CHECK(verdict.passed);
            CHECK(verdict.referenceWritten);
            CHECK(verdict.actualPath.empty());
            CHECK(stored->width() == WIDTH);
            CHECK(stored->height() == HEIGHT);
            return;
        }

        CHECK_FALSE(verdict.passed);
        CHECK_FALSE(verdict.referenceWritten);
        CHECK(stored->width() == 8);
        CHECK(file_exists(verdict.actualPath));
    }

    TEST_CASE("golden_image - an unreadable reference is reported as a failure") {
        temp_directory scratch("golden_unreadable");
        const golden_options options = scratch_options(scratch);
        const std::filesystem::path referencePath = golden_image::reference_path("corrupt", options);
        std::error_code failure = {};
        std::filesystem::create_directories(referencePath.parent_path(), failure);
        REQUIRE_FALSE(failure);
        CONJURE_REQUIRE_OK(scratch.write_text("reference/corrupt/null.png", "this is not a png"));

        const image actual = solid(WIDTH, HEIGHT, rgba8{});
        const golden_result verdict = golden_image::compare("corrupt", actual, options);

        CHECK_FALSE(verdict.referenceMissing);
        CHECK(verdict.message.find("unreadable") != std::string::npos);
        if (environment().updateGoldens) {
            CHECK(verdict.passed);
            CHECK(verdict.referenceWritten);
            CHECK(verdict.message.find("was unreadable") != std::string::npos);
            CHECK(verdict.actualPath.empty());
            const result<image> rewritten = image::load_file(referencePath);
            CONJURE_REQUIRE_OK(rewritten);
            CHECK(rewritten.value() == actual);
            return;
        }

        CHECK_FALSE(verdict.passed);
        CHECK_FALSE(verdict.referenceWritten);
        CHECK(file_exists(verdict.actualPath));
    }

    TEST_CASE("golden_image - default paths point at the repository references and the test output root") {
        golden_options options = {};
        options.backendName = "d3d12";

        const std::filesystem::path reference = golden_image::reference_path("pbr_spheres", options);
        const std::filesystem::path output = golden_image::output_directory("pbr_spheres", options);

        const std::filesystem::path repositoryGolden = environment().repoAssetRoot.parent_path() / "golden";
        CHECK(reference == repositoryGolden / "reference" / "pbr_spheres" / "d3d12.png");
        CHECK(output == environment().outputRoot / "golden" / "pbr_spheres" / "d3d12");
    }

    /*-----------------------------------------------------------------------*/
    // Rendering

    TEST_CASE("golden_image - render_offscreen is refused while golden images are disabled") {
        if (environment().goldenEnabled) {
            MESSAGE("golden images are enabled in this build; the gate cannot be observed here");
            return;
        }
        const result<image> rendered = golden_image::render_offscreen("never_read.scene.json", golden_options{});

        CONJURE_CHECK_ERROR(rendered, error_code::UNSUPPORTED);
    }

    TEST_CASE("golden_image - render_headless rejects a zero size") {
        golden_options options = {};
        options.width = 0;

        CONJURE_CHECK_ERROR(golden_image::render_headless("unused.scene.json", options), error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("regression - render_headless refuses instead of returning an image nothing rendered into") {
        // A headless application has no swapchain and the engine submits camera views only to one, so until
        // the engine offers a headless view target there is no image to return (golden_image.h).
        temp_directory scratch("golden_render");
        const std::filesystem::path scenePath = scratch.file("small.scene.json");
        golden_options options = {};
        options.width = 48;
        options.height = 24;

        const result<image> rendered = golden_image::render_headless(scenePath, options);

        CONJURE_CHECK_ERROR(rendered, error_code::UNSUPPORTED);
        REQUIRE_FALSE(rendered.has_value());
        CHECK(rendered.get_error().message().find("off-screen") != std::string_view::npos);
        CHECK(rendered.get_error().message().find("small.scene.json") != std::string_view::npos);
        CHECK_FALSE(file_exists(scenePath));   // the scene was never read, let alone written
    }
}
