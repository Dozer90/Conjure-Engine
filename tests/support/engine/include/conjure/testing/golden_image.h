#pragma once

#include "conjure/core/image.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <string>
#include <string_view>

// Golden-image comparison (docs/design/08_testing.md 2.3, 3.5, 4.6, D11). compare() loads the reference
// PNG of a scene, measures the actual image against it with per-channel tolerance, mismatching-pixel
// fraction and mean absolute error, and writes actual.png / diff.png into the test output root on a
// failure. --conjure-update-golden (environment().updateGoldens) writes the actual image as the reference
// whenever the reference is missing, unreadable or fails the comparison (a size mismatch included), and
// the result then passes with a WARN; a passing comparison never rewrites anything. render_offscreen() is
// meant to produce the actual image through a headless_engine and is refused unless the build enabled
// CONJURE_TEST_GOLDEN_IMAGES; the pure comparison runs everywhere.
//
// Rendering is not functional yet: a headless engine::application has no swapchain, and the engine's
// renderer_subsystem drops every camera view in that case instead of pointing it at an off-screen target,
// so nothing can be drawn into the image render_offscreen would read back. Until the engine offers a
// headless view target (a renderer_subsystem hook or an engine_config.rendering setting), render_headless
// fails with UNSUPPORTED rather than returning an image nothing was rendered into.
//
//   golden_options options = {};
//   golden_result verdict = golden_image::compare("pbr_spheres", actual, options);
//   CHECK_MESSAGE(verdict.passed, verdict.message);

namespace conjure::testing {

    struct golden_options {
        u8 channelTolerance = 3;                        // per channel, out of 255
        f32 maxMismatchFraction = 0.005f;               // pixels with any channel beyond the tolerance
        f32 maxMeanAbsoluteError = 1.0f;                // over every channel of every pixel
        u32 width = 640;
        u32 height = 360;
        u32 settleFrames = 3;
        std::string backendName = "null";               // reference/<scene>/<backend>.png
        std::filesystem::path referenceRoot = {};       // empty = <repo>/tests/golden/reference
        std::filesystem::path outputRoot = {};          // empty = environment().outputRoot / "golden"
    };

    struct golden_result {
        bool passed = false;
        bool referenceMissing = false;                  // no reference (written when updateGoldens)
        bool referenceWritten = false;                  // update mode stored the actual image as reference
                                                        // (it was missing, unreadable or failed the comparison)
        bool dimensionMismatch = false;
        u32 mismatchPixelCount = 0;
        f32 mismatchFraction = 0.0f;
        f32 meanAbsoluteError = 0.0f;
        std::filesystem::path referencePath = {};
        std::filesystem::path actualPath = {};          // written on failure; empty when update mode wrote
                                                        // the reference instead
        std::filesystem::path diffPath = {};            // written on a same-size mismatch
        std::string message = {};                       // one line a CHECK_MESSAGE can print
    };

    class golden_image {
        public:
            /// <summary>
            /// Will render a scene file for settleFrames frames at width x height through a headless_engine
            /// and read the colour target back. Fails with UNSUPPORTED when golden images are disabled in
            /// this build (environment().goldenEnabled) and, today, with UNSUPPORTED from render_headless
            /// because the engine cannot yet target an off-screen image headlessly (see the file comment).
            /// </summary>
            [[nodiscard]] static core::result<core::image> render_offscreen(
              const std::filesystem::path &scenePath,
              const golden_options &options
            );

            /// <summary>
            /// The rendering half of render_offscreen without the golden gate. Fails with INVALID_ARGUMENT
            /// for a zero render size and otherwise with UNSUPPORTED until the engine provides a headless
            /// view target; it never returns an image nothing was rendered into.
            /// </summary>
            [[nodiscard]] static core::result<core::image> render_headless(
              const std::filesystem::path &scenePath,
              const golden_options &options
            );

            /// <summary>
            /// Compares 'actual' with reference/<sceneName>/<backend>.png (3.5). A missing or unreadable
            /// reference fails, and a dimension mismatch fails without a diff. On failure actual.png (and
            /// diff.png when sizes agree) land in outputRoot/<scene>/<backend>/. With
            /// environment().updateGoldens, a reference that is missing, unreadable or fails the comparison
            /// (a size mismatch included) is written from the actual image instead, nothing lands in the
            /// output directory and the result passes with a WARN; a passing comparison never rewrites.
            /// </summary>
            [[nodiscard]] static golden_result compare(
              std::string_view sceneName,
              const core::image &actual,
              const golden_options &options
            );

            /// <summary>
            /// The pure comparison: no files are read or written. Passes iff the dimensions agree,
            /// mismatchFraction &lt;= maxMismatchFraction and meanAbsoluteError &lt;= maxMeanAbsoluteError.
            /// </summary>
            [[nodiscard]] static golden_result compare_images(
              const core::image &reference,
              const core::image &actual,
              const golden_options &options
            );

            /// <summary>
            /// Per channel max(|reference - actual|, |alpha delta|) x 8, clamped to 255, alpha opaque: an
            /// alpha-only difference shows as grey. Empty when the dimensions differ.
            /// </summary>
            [[nodiscard]] static core::image make_diff(const core::image &reference, const core::image &actual);

            /// <summary>
            /// reference/<sceneName>/<backend>.png under options.referenceRoot (or the repository default).
            /// </summary>
            [[nodiscard]] static std::filesystem::path reference_path(
              std::string_view sceneName,
              const golden_options &options
            );

            /// <summary>
            /// outputRoot/<sceneName>/<backend>/ (or the environment default).
            /// </summary>
            [[nodiscard]] static std::filesystem::path output_directory(
              std::string_view sceneName,
              const golden_options &options
            );
    };
}
