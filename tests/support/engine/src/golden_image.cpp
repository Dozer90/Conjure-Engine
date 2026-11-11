#include "conjure/testing/golden_image.h"

#include "conjure/testing/log_category.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <span>
#include <system_error>

using namespace conjure;
using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    constexpr u32 DIFF_AMPLIFICATION = 8;

    /// <summary>
    /// Saves an image, creating the directory; a failure is reported in the result message, never thrown.
    /// </summary>
    void save_image(const core::image &source, const std::filesystem::path &path, std::string &message) {
        std::error_code failure = {};
        std::filesystem::create_directories(path.parent_path(), failure);
        if (failure) {
            message += std::format(" (could not create {}: {})", path.parent_path().string(), failure.message());
            return;
        }
        const core::result<void> saved = source.save_png(path);
        if (!saved.has_value()) {
            message += std::format(" (could not write {}: {})", path.string(), saved.get_error().to_string());
        }
    }

    [[nodiscard]] u8 channel_delta(std::byte a, std::byte b) noexcept {
        const i32 difference = static_cast<i32>(std::to_integer<u8>(a)) - static_cast<i32>(std::to_integer<u8>(b));
        return static_cast<u8>(std::abs(difference));
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Rendering

core::result<core::image> golden_image::render_offscreen(
  const std::filesystem::path &scenePath,
  const golden_options &options
) {
    if (!environment().goldenEnabled) {
        return core::make_error(
          core::error_code::UNSUPPORTED,
          "golden images are disabled in this build (configure with CONJURE_TEST_GOLDEN_IMAGES=ON)"
        );
    }
    return render_headless(scenePath, options);
}

core::result<core::image> golden_image::render_headless(
  const std::filesystem::path &scenePath,
  const golden_options &options
) {
    if (options.width == 0 || options.height == 0) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "golden_options: a zero render size");
    }

    // A headless application has no swapchain and the engine's renderer_subsystem submits camera views only
    // to the swapchain (engine/src/engine/subsystems/renderer_subsystem.cpp), so a render target created
    // here would be read back untouched. Refusing is the honest answer until the engine grows a headless
    // view target; the scene path is part of the message so the caller can see what it asked for.
    return core::make_error(
      core::error_code::UNSUPPORTED,
      "golden_image::render_headless('{}'): no view can target an off-screen image yet (the engine has no "
      "headless render target hook); off-screen rendering is not functional in this build",
      scenePath.generic_string()
    );
}


/*===========================================================================*/
/*===========================================================================*/
// Comparison

golden_result golden_image::compare(
  std::string_view sceneName,
  const core::image &actual,
  const golden_options &options
) {
    golden_result verdict = {};
    verdict.referencePath = reference_path(sceneName, options);
    const std::filesystem::path outputDirectory = output_directory(sceneName, options);

    core::result<core::image> reference = core::image::load_file(verdict.referencePath);
    if (!reference.has_value()) {
        if (!reference.get_error().is(core::error_code::NOT_FOUND)) {
            verdict.message = std::format(
              "golden '{}': reference {} is unreadable: {}",
              sceneName,
              verdict.referencePath.string(),
              reference.get_error().to_string()
            );
            verdict.actualPath = outputDirectory / "actual.png";
            save_image(actual, verdict.actualPath, verdict.message);
            return verdict;
        }

        verdict.referenceMissing = true;
        if (environment().updateGoldens) {
            verdict.message = std::format(
              "golden '{}': reference {} was missing and has been written from the actual image",
              sceneName,
              verdict.referencePath.string()
            );
            save_image(actual, verdict.referencePath, verdict.message);
            std::error_code ignored = {};
            verdict.referenceWritten = std::filesystem::is_regular_file(verdict.referencePath, ignored);
            verdict.passed = verdict.referenceWritten;
            CONJURE_LOG_WARN(LOG_TESTING, "{}", verdict.message);
            return verdict;
        }

        verdict.message = std::format(
          "golden '{}': reference {} is missing (run with --conjure-update-golden to create it)",
          sceneName,
          verdict.referencePath.string()
        );
        verdict.actualPath = outputDirectory / "actual.png";
        save_image(actual, verdict.actualPath, verdict.message);
        return verdict;
    }

    verdict = compare_images(reference.value(), actual, options);
    verdict.referencePath = reference_path(sceneName, options);
    verdict.message = std::format("golden '{}': {}", sceneName, verdict.message);
    if (verdict.passed) {
        return verdict;
    }

    verdict.actualPath = outputDirectory / "actual.png";
    save_image(actual, verdict.actualPath, verdict.message);
    if (!verdict.dimensionMismatch) {
        verdict.diffPath = outputDirectory / "diff.png";
        save_image(make_diff(reference.value(), actual), verdict.diffPath, verdict.message);
    }
    return verdict;
}

golden_result golden_image::compare_images(
  const core::image &reference,
  const core::image &actual,
  const golden_options &options
) {
    golden_result verdict = {};
    if (reference.width() != actual.width() || reference.height() != actual.height()) {
        verdict.dimensionMismatch = true;
        verdict.message = std::format(
          "dimension mismatch: reference {}x{}, actual {}x{}",
          reference.width(),
          reference.height(),
          actual.width(),
          actual.height()
        );
        return verdict;
    }

    const std::span<const std::byte> expected = reference.pixels();
    const std::span<const std::byte> observed = actual.pixels();
    const usize pixelCount = expected.size() / core::image::BYTES_PER_PIXEL;
    u64 absoluteSum = 0;
    u32 mismatches = 0;
    for (usize pixel = 0; pixel < pixelCount; ++pixel) {
        const usize offset = pixel * core::image::BYTES_PER_PIXEL;
        u8 maxDelta = 0;
        for (usize channel = 0; channel < core::image::BYTES_PER_PIXEL; ++channel) {
            const u8 delta = channel_delta(expected[offset + channel], observed[offset + channel]);
            maxDelta = std::max(maxDelta, delta);
            absoluteSum += delta;
        }
        if (maxDelta > options.channelTolerance) {
            ++mismatches;
        }
    }

    verdict.mismatchPixelCount = mismatches;
    if (pixelCount > 0) {
        verdict.mismatchFraction = static_cast<f32>(mismatches) / static_cast<f32>(pixelCount);
        verdict.meanAbsoluteError =
          static_cast<f32>(absoluteSum) / static_cast<f32>(pixelCount * core::image::BYTES_PER_PIXEL);
    }
    verdict.passed = verdict.mismatchFraction <= options.maxMismatchFraction &&
      verdict.meanAbsoluteError <= options.maxMeanAbsoluteError;
    verdict.message = std::format(
      "{}: {} of {} pixels differ by more than {}/255 ({:.4f} %, limit {:.4f} %), "
      "mean absolute error {:.3f} (limit {:.3f})",
      verdict.passed ? "pass" : "fail",
      mismatches,
      pixelCount,
      options.channelTolerance,
      verdict.mismatchFraction * 100.0f,
      options.maxMismatchFraction * 100.0f,
      verdict.meanAbsoluteError,
      options.maxMeanAbsoluteError
    );
    return verdict;
}

core::image golden_image::make_diff(const core::image &reference, const core::image &actual) {
    if (reference.width() != actual.width() || reference.height() != actual.height()) {
        return core::image();
    }

    core::image diff(reference.width(), reference.height());
    const std::span<const std::byte> expected = reference.pixels();
    const std::span<const std::byte> observed = actual.pixels();
    const std::span<std::byte> output = diff.pixels();
    for (usize offset = 0; offset < output.size(); offset += core::image::BYTES_PER_PIXEL) {
        for (usize channel = 0; channel < 3; ++channel) {
            const u8 delta = channel_delta(expected[offset + channel], observed[offset + channel]);
            const u32 amplified = static_cast<u32>(delta) * DIFF_AMPLIFICATION;
            output[offset + channel] = static_cast<std::byte>(std::min<u32>(amplified, 255));
        }
        output[offset + 3] = static_cast<std::byte>(255);
    }
    return diff;
}


/*===========================================================================*/
/*===========================================================================*/
// Paths

std::filesystem::path golden_image::reference_path(std::string_view sceneName, const golden_options &options) {
    std::filesystem::path root = options.referenceRoot;
    if (root.empty()) {
        root = environment().repoAssetRoot.parent_path() / "golden" / "reference";
    }
    return root / std::filesystem::path(sceneName) / std::filesystem::path(options.backendName + ".png");
}

std::filesystem::path golden_image::output_directory(std::string_view sceneName, const golden_options &options) {
    std::filesystem::path root = options.outputRoot;
    if (root.empty()) {
        root = environment().outputRoot / "golden";
    }
    return root / std::filesystem::path(sceneName) / std::filesystem::path(options.backendName);
}
